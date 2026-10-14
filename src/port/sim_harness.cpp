// Headless simulation harness: loads a saved game the way RebellionApplication::LoadPendingGame does (minus the strategic
// window state, which belongs to the UI), then steps the battle flow (AppStepGame, the same call as the main loop) with
// the 0.25 s wait timer forced on, so no wall clock, message pump or input is involved, and saves the game.
// A "tick" here is one advance of the game manager's tick counter plus the rest of that tick's frame phases.
#if defined(REB_NATIVE)
#include "port/native/win_compat.h"
#else
#include <windows.h>
#endif
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "ai/ai_controller_factory.h"
#include "core/file_util.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "events/game_step_driver.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_root.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_queries.h"
#include "galaxy/game_setup_seed.h"
#include "galaxy/system.h"
#include "strategy_ui/human_player.h"

#include "port/native/save_transaction.h"
#include "port/native/ui_smoke.h"
#include "port/sim_harness.h"

namespace
{

constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kCoruscantKey = 0x90000109;

struct SimOptions
{
  char load[MAX_PATH] = "";
  char save[MAX_PATH] = "";
  char log[MAX_PATH] = "";
  uint32_t ticks = 0;
  uint32_t every = 0;
  uint32_t seed = 1;
  int galaxySize = 0;
  int victoryMode = 0;
  int difficulty = 7;
  int aiKinds[2] = {};
  bool aiMatch = false;
  bool untilVictory = false;
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
    const auto keyLength = static_cast<size_t>(colon - text);
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
    else if (KeyIs(text, keyLength, "new"))
    {
      options->galaxySize = static_cast<int>(strtoul(value, nullptr, 10));
      options->aiMatch = true;
    }
    else if (KeyIs(text, keyLength, "seed"))
      options->seed = static_cast<uint32_t>(strtoul(value, nullptr, 10));
    else if (KeyIs(text, keyLength, "victory"))
      options->victoryMode = static_cast<int>(strtoul(value, nullptr, 10));
    else if (KeyIs(text, keyLength, "difficulty"))
      options->difficulty = static_cast<int>(strtoul(value, nullptr, 10));
    else if (KeyIs(text, keyLength, "ai1") || KeyIs(text, keyLength, "ai2"))
    {
      char name[32];
      CopyField(name, sizeof name, value, valueLength);
      const int kind = AiControllerKindFromName(name);
      if (!kind)
        return false;
      options->aiKinds[KeyIs(text, keyLength, "ai1") ? 0 : 1] = kind;
      options->aiMatch = true;
    }
    else if (KeyIs(text, keyLength, "until"))
    {
      if (valueLength != 3 || strncmp(value, "win", 3) != 0)
        return false;
      options->untilVictory = true;
      options->aiMatch = true;
    }
    else
      return false;
    text = end ? end + 1 : text + length;
  }
  return options->save[0] && (options->load[0] != 0 || options->galaxySize >= 1) &&
         !(options->load[0] && options->galaxySize) &&
         (!options->galaxySize || options->galaxySize <= 3) && options->victoryMode >= 0 &&
         options->victoryMode <= 1 && options->difficulty >= 0 && options->difficulty <= 7 &&
         (!options->untilVictory || options->ticks != 0);
}

bool ReadOptions(SimOptions* options)
{
  char buffer[1024];
  const DWORD n = GetEnvironmentVariableA("REB_SIM", buffer, sizeof buffer);
  return n != 0 && n < sizeof buffer && ParseOptions(buffer, options);
}

int Finish(int code, const char* message)
{
  if (g_log == INVALID_HANDLE_VALUE)
    fprintf(stderr, "%s\n", message);
  else
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

bool StartNewGame(RebellionApplication* app, const SimOptions& options, SavedGameInfo* info)
{
  app->m_side = 1;
  app->m_gameType = 1;
  app->m_unknown130 = 1;
  app->m_unknown134 = 0;
  app->m_autosaveTick = 0;
  CreateBattleFlow();
  BattleFlow* flow = GetBattleFlow();
  if (!flow)
    return false;
  GameSettings settings;
  settings.m_field04 = options.galaxySize;
  settings.m_field08 = options.victoryMode;
  settings.m_field0c = options.difficulty;
  settings.m_field10 = static_cast<int>(options.seed);
  if (!flow->StartGame(&settings, 1, 0))
    return false;
  constexpr int kSetupFrameLimit = 1000000;
  for (int frame = 0; frame < kSetupFrameLimit && flow->m_state != kFlowStrategic; ++frame)
  {
    GameStepDriver* driver = GetGameStepDriver();
    if (!driver || !driver->m_ok)
      return false;
    if (driver->m_setupStage == 3)
      driver->m_setupDone = 1;
    int stepped = 1;
    AppStepGame(&stepped);
  }
  GameStepDriver* driver = GetGameStepDriver();
  GameSession* session = GetGameSession();
  if (flow->m_state != kFlowStrategic || !driver || !session)
    return false;
  session->m_viewReady = 1;
  driver->m_hold = 0;
  info->m_gameType = 1;
  info->m_side = 1;
  info->SetName("AI match");
  return true;
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

struct TerritoryCounts
{
  int alliance = 0;
  int empire = 0;
  int neutral = 0;
  int coruscantControl = 0;
  bool coruscantDestroyed = false;
  bool allianceHqDestroyed = false;
};

TerritoryCounts CountControlledSystems()
{
  TerritoryCounts counts;
  for (GameObjNotifier* object = GetFirstMasterInFamilies_90_98(); object; object = NextSystemMaster(object))
  {
    const auto* system = static_cast<const System*>(object);
    if (system->m_stateFlags & kDestroyedFlag)
      continue;
    switch (system->ControlSide())
    {
      case 1: ++counts.alliance; break;
      case 2: ++counts.empire; break;
      default: ++counts.neutral; break;
    }
  }
  const ObjectKey coruscantKey(ObjectKey::Make(kCoruscantKey >> 24, kCoruscantKey & 0xffffff));
  const GameObjNotifier* coruscant = GetMasterInFamilies_90_98(&coruscantKey);
  if (coruscant)
  {
    counts.coruscantControl = static_cast<const System*>(coruscant)->ControlSide();
    counts.coruscantDestroyed = (static_cast<const GameObj*>(coruscant)->m_stateFlags & kDestroyedFlag) != 0;
  }
  const GameObjNotifier* allianceHq = GetFirstMasterInFamilies_20_22();
  counts.allianceHqDestroyed = !allianceHq ||
                               (static_cast<const GameObj*>(allianceHq)->m_stateFlags & kDestroyedFlag) != 0;
  return counts;
}

bool WriteMatchResult(const SimOptions& options, int winner, uint32_t tick, uint64_t frames)
{
  const auto* root = static_cast<const GameRoot*>(GetGameRootObjectCached());
  if (!root)
    return false;
  char path[MAX_PATH];
  snprintf(path, sizeof path, "%s.result.json", options.save);
  HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return false;
  const TerritoryCounts territory = CountControlledSystems();
  char result[512];
  const int length = snprintf(result, sizeof result,
                              "{\"ai1\":\"%s\",\"ai2\":\"%s\",\"seed\":%u,\"galaxy_size\":%d,"
                              "\"victory_mode\":%d,\"difficulty\":%d,\"winner\":%d,\"tick\":%u,\"day\":%d,"
                              "\"frames\":%llu,\"systems\":{\"alliance\":%d,\"empire\":%d,"
                              "\"neutral\":%d},\"coruscant_control\":%d,\"coruscant_destroyed\":%s,"
                              "\"alliance_hq_destroyed\":%s,"
                              "\"outcome\":\"%s\"}\n",
                              AiControllerName(options.aiKinds[0]), AiControllerName(options.aiKinds[1]),
                              static_cast<uint32_t>(root->m_settings.m_field10), root->m_settings.m_field04,
                              root->m_settings.m_field08, root->m_settings.m_field0c, winner, tick, GameHookDay(),
                              static_cast<unsigned long long>(frames), territory.alliance, territory.empire,
                              territory.neutral, territory.coruscantControl,
                              territory.coruscantDestroyed ? "true" : "false",
                              territory.allianceHqDestroyed ? "true" : "false",
                              winner ? "victory" : (options.untilVictory ? "tick_limit" : "tick_target"));
  DWORD written = 0;
  const bool ok = length > 0 && length < static_cast<int>(sizeof result) &&
                  WriteFile(file, result, static_cast<DWORD>(length), &written, nullptr) &&
                  written == static_cast<DWORD>(length);
  CloseHandle(file);
  return ok;
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
  if (session->m_human &&
      session->m_human != session->m_controllers[0] && session->m_human != session->m_controllers[1])
    return "human controller";
  if (session->m_controllers[0]->m_side != 1 || session->m_controllers[1]->m_side != 2)
    return "controller sides";
  GameObjNotifier* root = GetGameRootObjectCached();
  if (!root || !static_cast<GameObj*>(root)->Validate())
    return "game root";
  if (!GetGameRootSideMember(1) || !GetGameRootSideMember(2))
    return "side roots";
  return nullptr;
}

int VictoryWinner()
{
  const auto* root = static_cast<const GameRoot*>(GetGameRootObjectCached());
  if (!root)
    return 0;
  const auto flags = static_cast<uint32_t>(root->m_unk74);
  if ((flags & 0x1000000) != 0)
    return 1;
  return (flags & 0x100000) != 0 ? 2 : 0;
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
bool RunToTick(RebellionApplication* app, uint32_t target, bool untilVictory, bool reportProgress, StepStats* stats)
{
  constexpr uint32_t kFrameLimit = 2000000;
  uint32_t framesAtTick = 0;
  auto lastTick = static_cast<uint32_t>(GetGameTickCount());
  for (;;)
  {
    BattleFlow* flow = GetBattleFlow();
    if (!flow || !GetGameStepDriver())
      return false;
    const auto tick = static_cast<uint32_t>(GetGameTickCount());
    if (tick != lastTick)
    {
      if (framesAtTick > stats->maxFramesPerTick)
        stats->maxFramesPerTick = framesAtTick;
      framesAtTick = 0;
      lastTick = tick;
    }
    if (FrameSettled(flow) && (tick >= target || (untilVictory && VictoryWinner())))
      break;
    if (++framesAtTick > kFrameLimit)
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
    if (reportProgress && stats->frames % 100000 == 0)
      Log("progress: tick %u flow %d phase %d frames %llu", tick, flow->m_state,
          GetGameStepDriver()->m_phase, static_cast<unsigned long long>(stats->frames));
    const int state = flow->m_state;
    if (state != stats->lastState && state >= 0 && state < 0x18)
      ++stats->stateEntries[state];
    stats->lastState = state;
  }
  if (framesAtTick > stats->maxFramesPerTick)
    stats->maxFramesPerTick = framesAtTick;
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
    return Finish(2, "REB_SIM: expected load:<file> or new:<1-3>,ticks:<N>,save:<path>"
                     "[,ai1:classic,ai2:classic,until:win,every:<k>,seed:<N>,victory:<0-1>,"
                     "difficulty:<0-7>,log:<path>]");

  SavedGameInfo header;
  if (options.galaxySize)
  {
    if (!StartNewGame(app, options, &header))
      return Finish(3, "REB_SIM: new game setup failed");
  }
  else if (!LoadGame(app, options.load, &header))
    return Finish(3, "REB_SIM: load or close failed");
  GameSession* session = GetGameSession();
  if (options.aiMatch && session)
  {
    for (int index = 0; index < 2; ++index)
    {
      if (options.aiKinds[index])
        continue;
      if (!session->m_controllers[index])
        return Finish(3, "REB_SIM: missing player controller");
      const int savedKind = session->m_controllers[index]->GetKind();
      options.aiKinds[index] = AiControllerName(savedKind) ? savedKind : kPlayerAi;
    }
  }
  if (options.aiMatch && (!session || !session->UseAiControllers(options.aiKinds[0], options.aiKinds[1])))
    return Finish(3, "REB_SIM: cannot set requested AI controllers");
  if (options.aiMatch && session)
  {
    session->m_viewReady = 1;
    Log("AI match: side 1 %s, side 2 %s", AiControllerName(options.aiKinds[0]),
        AiControllerName(options.aiKinds[1]));
  }
  if (const char* invalid = ValidateReferences())
  {
    Log("REB_SIM: invalid reference after load: %s", invalid);
    return Finish(6, "REB_SIM: reference validation failed");
  }
  char uiSmoke[8];
  if (GetEnvironmentVariableA("REB_SWITCH_SIDES_SMOKE", uiSmoke, sizeof uiSmoke) != 0)
  {
    RunNativeSwitchSidesSmoke(*app);
    if (const char* invalid = ValidateReferences())
    {
      Log("REB_SIM: invalid reference after switching sides: %s", invalid);
      return Finish(6, "REB_SIM: switch sides reference validation failed");
    }
    Log("Switch sides checks passed");
  }
  if (GetEnvironmentVariableA("REB_UI_SMOKE", uiSmoke, sizeof uiSmoke) != 0)
  {
    RunNativeUiSmoke(*app);
    Log("UI visibility checks passed");
  }
  const auto startTick = static_cast<uint32_t>(GetGameTickCount());
  if (options.ticks > UINT32_MAX - startTick)
    return Finish(2, "REB_SIM: tick limit overflows game counter");
  Log("%s %s: tick %u day %d side %d type %d pause %d", options.galaxySize ? "started" : "loaded",
      options.galaxySize ? "new game" : options.load, startTick, GameHookDay(), app->m_side, app->m_gameType,
      GetGameStepDriver()->m_pauseTick);
  if (options.aiMatch)
  {
    const TerritoryCounts territory = CountControlledSystems();
    Log("systems tick %u: alliance %d empire %d neutral %d coruscant %d hq_destroyed %d", startTick,
        territory.alliance, territory.empire, territory.neutral, territory.coruscantControl,
        territory.allianceHqDestroyed);
  }
  GetGameStepDriver()->m_pauseTick = 0; // a loaded game starts paused; the player's un-pause click is not simulated

  StepStats stats;
  const uint32_t endTick = startTick + options.ticks;
  uint32_t next = options.every && options.every < options.ticks ? startTick + options.every : endTick;
  for (;;)
  {
    if (next > endTick)
      next = endTick;
    if (!RunToTick(app, next, options.untilVictory, options.aiMatch, &stats))
      return Finish(4, "REB_SIM: step stalled");
    const auto tick = static_cast<uint32_t>(GetGameTickCount());
    const int winner = options.untilVictory ? VictoryWinner() : 0;
    if (const char* invalid = ValidateReferences())
    {
      Log("REB_SIM: invalid reference at tick %u: %s", tick, invalid);
      return Finish(6, "REB_SIM: reference validation failed");
    }
    if (options.aiMatch)
    {
      const TerritoryCounts territory = CountControlledSystems();
      Log("systems tick %u: alliance %d empire %d neutral %d coruscant %d hq_destroyed %d", tick,
          territory.alliance, territory.empire, territory.neutral, territory.coruscantControl,
          territory.allianceHqDestroyed);
    }
    char path[MAX_PATH];
    if (tick >= endTick || winner)
      snprintf(path, sizeof path, "%s", options.save);
    else
      snprintf(path, sizeof path, "%s.t%u", options.save, tick);
    if (!SaveGame(app, header, path))
      return Finish(5, "REB_SIM: save write or close failed");
    session = GetGameSession();
    Log("saved tick %u day %d frames %llu next vote ids (side1 %d, side2 %d) -> %s", tick, GameHookDay(),
        static_cast<unsigned long long>(stats.frames), session ? session->m_controllers[0]->m_nextMessageId : -1,
        session ? session->m_controllers[1]->m_nextMessageId : -1, path);
    if (winner)
    {
      Log("winner: side %d at tick %u", winner, tick);
      break;
    }
    if (tick >= endTick)
      break;
    next = options.every < endTick - tick ? tick + options.every : endTick;
  }
  Log("done: %llu frames, max %u frames per tick", static_cast<unsigned long long>(stats.frames),
      stats.maxFramesPerTick);
  for (int state = 0; state < 0x18; ++state)
    if (state != 2 && stats.stateEntries[state])
      Log("flow state %d entered %u times", state, stats.stateEntries[state]);
  Log("references ok");
  if (options.aiMatch && !WriteMatchResult(options, VictoryWinner(), static_cast<uint32_t>(GetGameTickCount()),
                                          stats.frames))
    return Finish(8, "REB_SIM: result write failed");
  if (options.untilVictory && !VictoryWinner())
    return Finish(7, "REB_SIM: tick limit reached without victory");
  return Finish(0, "returning for normal teardown");
}
