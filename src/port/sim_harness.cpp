// Headless simulation harness: loads a saved game the way RebellionApplication::LoadPendingGame does (minus the strategic
// window state, which belongs to the UI), then steps the battle flow (AppStepGame, the same call as the main loop) with
// the 0.25 s wait timer forced on, so no wall clock, message pump or input is involved, and saves the game.
// A "tick" here is one advance of the game manager's tick counter plus the rest of that tick's frame phases.
#if defined(REB_NATIVE)
#include "port/native/win_compat.h"
#else
#include <windows.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "events/game_step_driver.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_queries.h"
#include "strategy_ui/human_player.h"

#include "port/native/save_transaction.h"
#include "port/native/ui_smoke.h"
#include "port/sim_harness.h"

namespace
{

struct SimOptions
{
  char load[MAX_PATH] = "";
  char save[MAX_PATH] = "";
  char log[MAX_PATH] = "";
  uint32_t ticks = 0;
  uint32_t every = 0;
};

HANDLE g_log = INVALID_HANDLE_VALUE;

void Log(const char* format, ...)
{
  char line[512];
  va_list args;
  va_start(args, format);
  const int n = vsnprintf(line, sizeof line - 1, format, args);
  va_end(args);
  if (g_log == INVALID_HANDLE_VALUE || n <= 0)
    return;
  const DWORD length = static_cast<DWORD>(n) < sizeof line - 1 ? static_cast<DWORD>(n) : sizeof line - 2;
  line[length] = '\n';
  DWORD written;
  WriteFile(g_log, line, length + 1, &written, nullptr);
}

void CopyField(char* destination, size_t capacity, const char* value, size_t length)
{
  if (length >= capacity)
    length = capacity - 1;
  memcpy(destination, value, length);
  destination[length] = 0;
}

bool KeyIs(const char* text, size_t length, const char* key)
{
  return strlen(key) == length && !strncmp(text, key, length);
}

bool ParseOptions(const char* text, SimOptions* options)
{
  while (*text)
  {
    const char* end = strchr(text, ',');
    const size_t length = end ? static_cast<size_t>(end - text) : strlen(text);
    const char* colon = static_cast<const char*>(memchr(text, ':', length));
    if (!colon)
      return false;
    const size_t keyLength = static_cast<size_t>(colon - text);
    const char* value = colon + 1;
    const size_t valueLength = length - keyLength - 1;
    if (KeyIs(text, keyLength, "load"))
      CopyField(options->load, sizeof options->load, value, valueLength);
    else if (KeyIs(text, keyLength, "save"))
      CopyField(options->save, sizeof options->save, value, valueLength);
    else if (KeyIs(text, keyLength, "log"))
      CopyField(options->log, sizeof options->log, value, valueLength);
    else if (KeyIs(text, keyLength, "ticks"))
      options->ticks = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    else if (KeyIs(text, keyLength, "every"))
      options->every = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    else
      return false;
    text = end ? end + 1 : text + length;
  }
  return options->load[0] && options->save[0];
}

bool ReadOptions(SimOptions* options)
{
  char buffer[1024];
  const DWORD n = GetEnvironmentVariableA("REB_SIM", buffer, sizeof buffer);
  return n != 0 && n < sizeof buffer && ParseOptions(buffer, options);
}

int Finish(int code, const char* message)
{
  Log("%s", message);
  if (g_log != INVALID_HANDLE_VALUE)
  {
    CloseHandle(g_log);
    g_log = INVALID_HANDLE_VALUE;
  }
  return code;
}

// Mirrors RebellionApplication::LoadPendingGame and BattleFlow::Restore for a running strategic game; the strategic
// window state that follows the game in the file is left unread (no window exists).
bool LoadGame(RebellionApplication* app, const char* path, SavedGameInfo* info)
{
  ScopedIfstream file;
  OpenInputFile(file.get(), path);
  if (GetFileError() != 0)
    return false;
  istream* in = file.get()->In();
  info->Read(in);
  app->m_unknown130 = info->m_unknown10;
  app->m_side = info->m_side;
  app->m_unknown134 = info->m_unknown14;
  app->m_gameType = info->m_gameType;
  app->m_autosaveTick = 0;
  CreateBattleFlow();
  BattleFlow* flow = GetBattleFlow();
  if (!flow)
    return false;
  flow->m_ok = 1;
  flow->m_state = 0;
  CheckPositionMarker(in);
  ReadValue32(in, &flow->m_ok);
  ReadValue32(in, &flow->m_state);
  ReadGameTime(&flow->m_voteTimeA, in);
  ReadGameTime(&flow->m_voteTimeB, in);
  if (flow->m_ok == 0 || flow->m_state != 2)
    return false;
  if (CreateGameStepDriver() == 0)
    return false;
  CheckPositionMarker(in);
  const int loaded = GetGameStepDriver()->Load(in);
  const int readState = iostream_access::State(file.get());
  flow->m_ok = 1;
  CloseFileStream(file.get());
  return loaded != 0 && readState == 0 && iostream_access::State(file.get()) == 0;
}

bool SaveGame(RebellionApplication* app, const SavedGameInfo& source, const char* path)
{
  SavedGameInfo info;
  info.m_gameType = app->m_gameType;
  info.m_unknown10 = 1;
  info.m_unknown14 = 0;
  info.m_slot = 1;
  info.SetName(source.GetName());
  info.m_side = app->m_side;
  info.m_unknown24 = app->m_autosaveTick;
  return reb::native::WriteSaveAtomically(path,
                                          [&](ostream* out)
                                          {
                                            info.Write(out);
                                            return AppSaveGame(out);
                                          });
}

const char* ValidateReferences()
{
  BattleFlow* flow = GetBattleFlow();
  if (!flow || flow->m_state != kFlowStrategic)
    return "battle flow";
  if (!GetGameStepDriver() || !GetGameStepDriver()->m_ok)
    return "game step driver";
  if (!GetGameManager())
    return "game manager";
  GameSession* session = GetGameSession();
  if (!session || !session->m_valid)
    return "game session";
  if (!session->m_controllers[0] || !session->m_controllers[1])
    return "player controllers";
  if (!session->m_human ||
      (session->m_human != session->m_controllers[0] && session->m_human != session->m_controllers[1]))
    return "human controller";
  GameObjNotifier* root = GetGameRootObjectCached();
  if (!root || !static_cast<GameObj*>(root)->Validate())
    return "game root";
  if (!GetGameRootSideMember(1) || !GetGameRootSideMember(2))
    return "side roots";
  return nullptr;
}

struct StepStats
{
  uint64_t frames = 0;
  uint32_t maxFramesPerTick = 0;
  uint32_t stateEntries[0x18] = {};
  int lastState = -1;
};

// The human side's answers to the battle votes, in place of the vote and battle windows: "resolve" (auto-resolve,
// HumanPlayer::SetVote(1, 1)) to the battle-type vote, then "confirmed" (HumanPlayer::ConfirmVote) to the done vote.
// The UI notices those calls would queue for the windows are not created.
void AnswerHumanVotes(const BattleFlow* flow)
{
  GameSession* session = GetGameSession();
  HumanPlayer* human = session ? session->m_human : nullptr;
  if (!human)
    return;
  if (flow->m_state == kFlowBattleWaitForType && human->m_voteState == 0)
  {
    ClearGameTime(&human->m_voteTime);
    human->m_voteState = 1;
    human->m_voteTime.m_value = 10000.0f;
  }
  else if (flow->m_state == kFlowBattleWaitForDone && human->m_voteState != 3)
  {
    human->m_voteState = 3;
  }
}

bool FrameSettled(const BattleFlow* flow)
{
  return flow->m_state == kFlowStrategic && GetGameStepDriver() && GetGameStepDriver()->m_phase == 0;
}

// Runs frames until the game's tick counter reaches `target` and the frame phases of that tick are done.
bool RunToTick(RebellionApplication* app, uint32_t target, StepStats* stats)
{
  constexpr uint32_t kFrameLimit = 2000000;
  uint32_t frames = 0;
  for (;;)
  {
    BattleFlow* flow = GetBattleFlow();
    if (!flow || !GetGameStepDriver())
      return false;
    const uint32_t tick = static_cast<uint32_t>(GetGameTickCount());
    if (tick >= target && FrameSettled(flow))
      break;
    if (++frames > kFrameLimit)
    {
      Log("stall: tick %u state %d phase %d hold %d pause %d", tick, flow->m_state, GetGameStepDriver()->m_phase,
          GetGameStepDriver()->m_hold, GetGameStepDriver()->m_pauseTick);
      return false;
    }
    app->m_waitElapsed = 1;
    AnswerHumanVotes(flow);
    int stepped = 1;
    AppStepGame(&stepped);
    ++stats->frames;
    const int state = flow->m_state;
    if (state != stats->lastState && state >= 0 && state < 0x18)
      ++stats->stateEntries[state];
    stats->lastState = state;
  }
  if (frames > stats->maxFramesPerTick)
    stats->maxFramesPerTick = frames;
  return true;
}

} // namespace

bool RebSimRequested()
{
  char buffer[8];
  return GetEnvironmentVariableA("REB_SIM", buffer, sizeof buffer) != 0;
}

int RebSimRun(RebellionApplication* app)
{
  SimOptions options;
  const bool valid = ReadOptions(&options);
  if (valid)
  {
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s.log", options.save);
    g_log = CreateFileA(options.log[0] ? options.log : path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);
  }
  if (!valid)
    return Finish(2, "REB_SIM: expected load:<file>,ticks:<N>,save:<path>[,every:<k>][,log:<path>]");

  SavedGameInfo header;
  if (!LoadGame(app, options.load, &header))
    return Finish(3, "REB_SIM: load or close failed");
  if (const char* invalid = ValidateReferences())
  {
    Log("REB_SIM: invalid reference after load: %s", invalid);
    return Finish(6, "REB_SIM: reference validation failed");
  }
  char uiSmoke[8];
  if (GetEnvironmentVariableA("REB_UI_SMOKE", uiSmoke, sizeof uiSmoke) != 0)
  {
    RunNativeUiSmoke(*app);
    Log("UI visibility checks passed");
  }
  const uint32_t startTick = static_cast<uint32_t>(GetGameTickCount());
  Log("loaded %s: tick %u side %d type %d pause %d", options.load, startTick, app->m_side, app->m_gameType,
      GetGameStepDriver()->m_pauseTick);
  GetGameStepDriver()->m_pauseTick = 0; // a loaded game starts paused; the player's un-pause click is not simulated

  StepStats stats;
  const uint32_t endTick = startTick + options.ticks;
  uint32_t next = options.every ? startTick + options.every : endTick;
  for (;;)
  {
    if (next > endTick)
      next = endTick;
    if (!RunToTick(app, next, &stats))
      return Finish(4, "REB_SIM: step stalled");
    const uint32_t tick = static_cast<uint32_t>(GetGameTickCount());
    if (const char* invalid = ValidateReferences())
    {
      Log("REB_SIM: invalid reference at tick %u: %s", tick, invalid);
      return Finish(6, "REB_SIM: reference validation failed");
    }
    char path[MAX_PATH];
    if (tick >= endTick)
      snprintf(path, sizeof path, "%s", options.save);
    else
      snprintf(path, sizeof path, "%s.t%u", options.save, tick);
    if (!SaveGame(app, header, path))
      return Finish(5, "REB_SIM: save write or close failed");
    GameSession* session = GetGameSession();
    Log("saved tick %u frames %llu messages sent (side1 %d, side2 %d) -> %s", tick,
        static_cast<unsigned long long>(stats.frames), session ? session->m_controllers[0]->m_nextVoteId : -1,
        session ? session->m_controllers[1]->m_nextVoteId : -1, path);
    if (tick >= endTick)
      break;
    next = tick + options.every;
  }
  Log("done: %llu frames, max %u frames per tick", static_cast<unsigned long long>(stats.frames),
      stats.maxFramesPerTick);
  for (int state = 0; state < 0x18; ++state)
    if (state != 2 && stats.stateEntries[state])
      Log("flow state %d entered %u times", state, stats.stateEntries[state]);
  Log("references ok");
  return Finish(0, "returning for normal teardown");
}
