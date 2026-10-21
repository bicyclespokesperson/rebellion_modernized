#include "port/native/lan_smoke.h"

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <thread>

#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "app/battle_flow.h"
#include "app/game_session.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"
#include "events/net_messages.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_database.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_family.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "strategy_ui/game_ui_facade.h"
#include "port/native/lan_game.h"
#include "port/native/lan_battle_smoke.h"
#include "orders/orders_210_216.h"
#include "orders/move_orders.h"

namespace
{
using Clock = std::chrono::steady_clock;
constexpr int kDefaultRunSeconds = 80;
constexpr uint32_t kRequiredTicks = 200;

const char* Environment(const char* name)
{
  const char* value = std::getenv(name);
  return value && *value ? value : nullptr;
}

std::string JsonString(const std::string& value)
{
  std::string escaped;
  escaped.reserve(value.size() + 2);
  for (char c : value) {
    if (c == '"' || c == '\\') escaped.push_back('\\');
    if (c == '\n') { escaped += "\\n"; continue; }
    if (c == '\r') { escaped += "\\r"; continue; }
    escaped.push_back(c);
  }
  return escaped;
}

bool ParsePort(const char* text, uint16_t& port)
{
  if (!text || !*text) return false;
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (!end || *end || value == 0 || value > 65535) return false;
  port = static_cast<uint16_t>(value);
  return true;
}

int RunTimeoutSeconds()
{
  const char* text = Environment("REB_LAN_SMOKE_TIMEOUT");
  if (!text) return kDefaultRunSeconds;
  char* end = nullptr;
  const long value = std::strtol(text, &end, 10);
  return end && *end == '\0' && value >= 1 && value <= 3600 ? static_cast<int>(value) : kDefaultRunSeconds;
}

void WriteReport(const std::filesystem::path& output, const std::string& role, bool success,
                 const std::string& error, uint32_t initialTick, uint32_t finalTick, uint64_t acceptedOrders,
                 bool orderSent, bool renameApplied, bool reconnectVerified,
                 size_t automaticCheckpoints, bool manualCheckpoint,
                 bool battleCompleted, bool guestVoted, bool endgameCompleted, int movieState,
                 bool lanClosed, const std::filesystem::path& checkpoint, bool resumed)
{
  const auto path = output / (role + ".json");
  std::ofstream report(path, std::ios::binary | std::ios::trunc);
  if (!report) {
    std::cerr << "LAN smoke: could not write report " << path << '\n';
    return;
  }
  report << "{\n"
         << "  \"role\": \"" << role << "\",\n"
         << "  \"success\": " << (success ? "true" : "false") << ",\n"
         << "  \"error\": \"" << JsonString(error) << "\",\n"
         << "  \"initial_tick\": " << initialTick << ",\n"
         << "  \"final_tick\": " << finalTick << ",\n"
         << "  \"ticks_advanced\": " << (finalTick - initialTick) << ",\n"
         << "  \"accepted_orders\": " << acceptedOrders << ",\n"
         << "  \"guest_order_sent\": " << (orderSent ? "true" : "false") << ",\n"
         << "  \"rename_applied\": " << (renameApplied ? "true" : "false") << ",\n"
         << "  \"reconnect_verified\": " << (reconnectVerified ? "true" : "false") << ",\n"
         << "  \"automatic_checkpoints\": " << automaticCheckpoints << ",\n"
         << "  \"manual_checkpoint\": " << (manualCheckpoint ? "true" : "false") << ",\n"
         << "  \"battle_completed\": " << (battleCompleted ? "true" : "false") << ",\n"
         << "  \"battle_vote_submitted\": " << (guestVoted ? "true" : "false") << ",\n"
         << "  \"endgame_completed\": " << (endgameCompleted ? "true" : "false") << ",\n"
         << "  \"movie_state\": " << movieState << ",\n"
         << "  \"lan_closed\": " << (lanClosed ? "true" : "false") << ",\n"
         << "  \"resumed\": " << (resumed ? "true" : "false") << ",\n"
         << "  \"checkpoint\": \"" << JsonString(checkpoint.string()) << "\"\n"
         << "}\n";
  report.flush();
}

bool SubmitRenameOrder(reb::native::LanGame& game, int side, ObjectKey& renamedKey, const std::string& name)
{
  if (!g_objectDatabase) return false;
  for (const auto& entry : g_objectDatabase->Entries()) {
    GameObjNotifier* notifier = entry->GetObject(side);
    auto* object = dynamic_cast<GameObj*>(notifier);
    if (!object) continue;
    ObjectKey key;
    entry->GetKey(&key);
    ObjectKeyValueSet team;
    if (!team.AddKeyValue(&key, 1)) continue;
    auto* order = new (std::nothrow) RenameOrder();
    if (!order) return false;
    order->m_side = side;
    order->SetTeam(&team);
    order->m_text = name;
    StatusCode status;
    if (!order->CheckValid(&status) || static_cast<int32_t>(status.m_second) != -1) {
      delete order;
      continue;
    }
    if (game.Submit(order, side)) {
      renamedKey = key;
      return true;
    }
    delete order;
  }
  return false;
}

bool HasRenamedObject(int view, const ObjectKey& key, const std::string& expected)
{
  GameObjNotifier* object = LookupObject(&key, view);
  if (!object) return false;
  std::string actual;
  static_cast<GameObj*>(object)->GetNameCopy(&actual);
  return actual == expected;
}

bool HasNamedObject(int view, const std::string& expected)
{
  if (!g_objectDatabase) return false;
  for (const auto& entry : g_objectDatabase->Entries()) {
    auto* object = dynamic_cast<GameObj*>(entry->GetObject(view));
    if (!object) continue;
    std::string actual;
    object->GetNameCopy(&actual);
    if (actual == expected) return true;
  }
  return false;
}

void LogHeartbeat(const std::string& role, const reb::native::LanGame& game)
{
  auto* flow = GetBattleFlow();
  auto* driver = flow ? flow->StepDriver() : nullptr;
  auto* session = driver ? driver->Session() : nullptr;
  std::cout << "LAN smoke " << role << " heartbeat: flow=" << (flow ? flow->m_state : -1)
            << " phase=" << (driver ? driver->m_phase : -1)
            << " tick=" << (driver ? GetGameTickCount() : 0)
            << " hold=" << (driver ? driver->m_hold : -1)
            << " pauseTick=" << (driver ? driver->m_pauseTick : -1)
            << " pausePhase=" << (driver ? driver->IsPauseTick() : -1)
            << " canSimulate=" << game.CanSimulate() << " connected=" << game.Connected()
            << " paused=" << game.Paused() << " replica=" << game.ReplicaTick()
            << " status=" << game.StatusText();
  if (session) {
    for (int side = 0; side < 2; ++side) {
      const auto* controller = session->m_controllers[side];
      if (!controller) continue;
      std::cout << " side" << side + 1 << "{sync=" << controller->m_syncId
                << ",target=" << controller->m_syncTarget << ",max=" << controller->m_maxSyncPoint
                << ",unk=" << controller->m_unk4c << ",pending=" << controller->m_pending.Count() << '}';
    }
  }
  if (driver && driver->Manager() && driver->Manager()->Root()) {
    auto* root = driver->Manager()->Root();
    std::cout << " root70=" << root->m_unk70 << " root74=0x" << std::hex
              << static_cast<uint32_t>(root->m_unk74) << std::dec
              << " battleKey=" << root->m_key8c.m_value;
    auto* battleSystem = root->m_key8c.IsSet()
        ? static_cast<System*>(GetMasterInFamilies_90_98(&root->m_key8c)) : nullptr;
    if (battleSystem) {
      std::cout << " battleSystemFlags=0x" << std::hex << battleSystem->m_flags << std::dec
                << " dataFlag3=" << battleSystem->GetDataFlagBit3();
    }
  }
  if (auto* manager = GetGameManager()) std::cout << " lastBattleTick=" << manager->m_lastBattleTick;
  std::cout << '\n' << std::flush;
}

} // namespace

bool LanSmokeRequested()
{
  const char* mode = Environment("REB_LAN_SMOKE");
  return mode && (std::string(mode) == "host" || std::string(mode) == "guest");
}

int RunLanSmoke(RebellionApplication& app)
{
  const char* requestedMode = Environment("REB_LAN_SMOKE");
  const std::string role = requestedMode ? requestedMode : "";
  const char* outputValue = Environment("REB_LAN_SMOKE_OUTPUT");
  if ((role != "host" && role != "guest") || !outputValue) {
    std::cerr << "LAN smoke requires REB_LAN_SMOKE=host|guest and REB_LAN_SMOKE_OUTPUT.\n";
    return 2;
  }

  std::filesystem::path output;
  uint16_t port = reb::native::lan::kDefaultPort;
  if (!ParsePort(Environment("REB_LAN_SMOKE_PORT"), port) && Environment("REB_LAN_SMOKE_PORT")) {
    std::cerr << "LAN smoke port must be between 1 and 65535.\n";
    return 2;
  }
  try {
    output = std::filesystem::absolute(outputValue);
    std::filesystem::create_directories(output);
  } catch (const std::exception& exception) {
    std::cerr << "LAN smoke output directory error: " << exception.what() << '\n';
    return 2;
  }

  auto* game = reb::native::CurrentLanGame();
  if (!game) {
    std::cerr << "LAN smoke could not find the application's LAN runtime.\n";
    return 2;
  }

  bool success = false;
  bool orderSent = false;
  bool renameApplied = false;
  bool startRequested = false;
  bool pauseRequested = false;
  bool checkpointSaved = false;
  bool automaticCheckpointSeen = false;
  bool initialTickSet = false;
  bool completionDelayStarted = false;
  bool resumed = false;
  bool reconnectStarted = false;
  bool reconnectVerified = false;
  bool battleRequested = Environment("REB_LAN_SMOKE_BATTLE") != nullptr;
  bool endgameRequested = Environment("REB_LAN_SMOKE_ENDGAME") != nullptr;
  bool battlePrepared = false;
  bool tacticalBattleSeen = false;
  bool battleCompleted = false;
  bool guestVoted = false;
  bool endgameTriggered = false;
  bool endgameCompleted = false;
  bool basePhasesComplete = false;
  uint32_t reportTick = 0;
  uint64_t reportAcceptedOrders = 0;
  size_t reportAutomaticCheckpoints = 0;
  bool reportManualCheckpoint = false;
  bool campaignSideSet = false;
  uint32_t initialTick = 0;
  uint32_t targetTick = 0;
  ObjectKey renamedKey;
  std::string orderName;
  auto completionTime = Clock::time_point{};
  auto nextHeartbeat = Clock::now() + std::chrono::seconds(3);
  uint32_t reconnectTick = 0;
  int campaignHostSide = game->Settings().hostSide;
  std::string error;
  std::filesystem::path checkpointPath;
  const char* resumePath = role == "host" ? Environment("REB_LAN_SMOKE_RESUME") : nullptr;

  if (!app.Create(0)) {
    error = "could not create the application window";
  } else {
    app.SetState(kStateLobbyHost);
    if (role == "host") {
      if (!game->Host("LAN smoke campaign", port)) error = game->StatusText();
      else if (const char* hostSideValue = Environment("REB_LAN_SMOKE_HOST_SIDE")) {
        const int hostSide = std::atoi(hostSideValue);
        if (hostSide != 1 && hostSide != 2) error = "REB_LAN_SMOKE_HOST_SIDE must be 1 or 2";
        else {
          auto settings = game->Settings();
          settings.hostSide = hostSide;
          game->SetSettings(settings);
        }
      }
    } else if (!game->Join("127.0.0.1", port)) {
      error = game->StatusText();
    }
  }

  const auto deadline = Clock::now() + std::chrono::seconds(RunTimeoutSeconds());
  while (error.empty() && Clock::now() < deadline) {
    MSG message{};
    int quit = 0;
    app.PumpMessages(&message, &quit);
    if (quit) {
      error = "application quit while smoke test was running";
      break;
    }

    if (!campaignSideSet && game->Playing()) {
      campaignHostSide = game->Settings().hostSide;
      campaignSideSet = true;
    }
    game->Poll(app);
    const int victoryState = campaignHostSide == 1 ? kStateVictoryMovieAlliance : kStateVictoryMovieEmpire;
    const int defeatState = campaignHostSide == 1 ? kStateDefeatMovieEmpire : kStateDefeatMovieAlliance;
    if (endgameRequested && role == "host" && basePhasesComplete && !endgameTriggered) {
      endgameTriggered = true;
      reportTick = static_cast<uint32_t>(GetGameTickCount());
      reportAcceptedOrders = game->AcceptedOrders();
      const auto beforeEndgame = game->Checkpoints();
      reportAutomaticCheckpoints = static_cast<size_t>(std::count_if(
          beforeEndgame.begin(), beforeEndgame.end(), [](const auto& item) { return item.automatic; }));
      reportManualCheckpoint = std::any_of(beforeEndgame.begin(), beforeEndgame.end(),
                                           [](const auto& item) { return !item.automatic; });
      std::cout << "LAN smoke host triggering victory movie state " << victoryState << '\n' << std::flush;
      app.SetState(victoryState);
    }
    if (endgameRequested) {
      const int expectedState = role == "host" ? victoryState : defeatState;
      endgameCompleted = app.m_state == expectedState && !game->Active();
      if (endgameCompleted) {
        success = true;
        break;
      }
    }
    if (battleRequested) {
      reb::native::AnswerLanBattleSmokeVotes();
      if (role == "guest") {
        auto* session = GetGameSession();
        guestVoted = guestVoted || (session && session->m_human && session->m_human->m_voteState == 3);
      }
      auto* flow = GetBattleFlow();
      if (flow && flow->m_state == kFlowTacticalAbstract) tacticalBattleSeen = true;
      if (flow && flow->m_state == kFlowStrategic && tacticalBattleSeen && GetGameManager()
          && GetGameManager()->m_lastBattleTick > static_cast<int>(initialTick)) {
        battleCompleted = true;
      }
      if (role == "host" && !battlePrepared && game->AcceptedOrders() >= 1 && initialTickSet
          && static_cast<uint32_t>(GetGameTickCount()) >= initialTick + 70 && GetGameStepDriver()
          && GetGameStepDriver()->m_phase == 13) {
        std::string battleError;
        if (!reb::native::PrepareLanBattleSmoke(battleError)) error = battleError;
        else {
          battlePrepared = true;
          std::cout << "LAN smoke host prepared an opposing-fleet battle.\n" << std::flush;
        }
      }
    }
    if (Clock::now() >= nextHeartbeat) {
      LogHeartbeat(role, *game);
      nextHeartbeat = Clock::now() + std::chrono::seconds(3);
    }
    if (game->Connected() && !game->Ready()) game->SetReady(true);

    if (role == "host" && game->Connected() && game->Ready() && game->PeerReady() && !game->Playing()
        && !startRequested) {
      startRequested = true;
      if (resumePath) {
        std::error_code fileError;
        const std::filesystem::path saved = std::filesystem::absolute(resumePath);
        if (!std::filesystem::is_regular_file(saved, fileError) || fileError) {
          error = "requested resume checkpoint does not exist";
        } else {
          resumed = game->Resume(app, saved);
          if (!resumed) error = game->StatusText();
        }
      } else if (!game->StartNew(app)) {
        error = game->StatusText();
      }
    }

    if (role == "host" && game->CanSimulate()) {
      app.m_waitElapsed = 1;
      int stepped = 0;
      AppStepGame(&stepped);
    }

    if (game->Playing()) {
      if (!initialTickSet) {
        initialTick = static_cast<uint32_t>(GetGameTickCount());
        targetTick = initialTick + kRequiredTicks;
        orderName = "LAN smoke order " + std::to_string(initialTick);
        initialTickSet = true;
        std::cout << "LAN smoke " << role << " campaign started at tick " << initialTick << '\n';
      }
      if (role == "host") {
        game->AfterStep(app);
        const auto existingCheckpoints = game->Checkpoints();
        automaticCheckpointSeen = std::any_of(existingCheckpoints.begin(), existingCheckpoints.end(),
                                               [](const auto& item) { return item.automatic; });
        if (game->AcceptedOrders() >= 1 && automaticCheckpointSeen && (!battleRequested || battleCompleted)
            && static_cast<uint32_t>(GetGameTickCount()) >= targetTick && !pauseRequested) {
          game->TogglePause();
          pauseRequested = true;
          if (!game->SaveCheckpoint(app, false)) error = game->LastSaveText();
          else checkpointSaved = true;
        }
      } else {
        if (!orderSent && game->Connected()) {
          orderSent = SubmitRenameOrder(*game, 3 - game->Settings().hostSide, renamedKey, orderName);
        }
        if (orderSent && !reconnectStarted && game->ReplicaTick() >= initialTick + 50) {
          reconnectTick = game->ReplicaTick();
          game->Reconnect();
          reconnectStarted = true;
          std::cout << "LAN smoke guest requested reconnect at replica tick " << reconnectTick << '\n' << std::flush;
        }
        if (reconnectStarted && game->Connected() && game->ReplicaTick() > reconnectTick) {
          reconnectVerified = true;
        }
      }
    }

    if (game->Playing() && initialTickSet && checkpointSaved && role == "host" && pauseRequested) {
      if (!completionDelayStarted) {
        completionDelayStarted = true;
        completionTime = Clock::now() + std::chrono::seconds(3);
      }
      if (Clock::now() >= completionTime) {
        const auto checkpoints = game->Checkpoints();
        const auto manual = std::find_if(checkpoints.begin(), checkpoints.end(), [](const auto& item) {
          return !item.automatic;
        });
        if (manual != checkpoints.end()) checkpointPath = manual->path;
        const int guestSide = 3 - campaignHostSide;
        renameApplied = HasNamedObject(guestSide, orderName);
        basePhasesComplete = game->AcceptedOrders() >= 1 && game->Paused() && !checkpointPath.empty() && renameApplied;
        if (!basePhasesComplete) {
          error = "host did not accept the rename, pause, save a checkpoint, and expose the renamed system";
        } else if (!endgameRequested) {
          success = true;
          break;
        }
      }
    }

    if (role == "guest" && game->Playing() && initialTickSet && orderSent && reconnectVerified && game->Paused()
        && game->ReplicaTick() >= targetTick) {
      const auto checkpoints = game->Checkpoints();
      renameApplied = HasRenamedObject(3 - game->Settings().hostSide, renamedKey, orderName);
      const auto manual = std::find_if(checkpoints.begin(), checkpoints.end(), [](const auto& item) {
        return !item.automatic;
      });
      if (manual != checkpoints.end() && renameApplied && (!battleRequested || guestVoted)) {
        checkpointPath = manual->path;
        basePhasesComplete = true;
        reportTick = game->ReplicaTick();
        reportAcceptedOrders = game->AcceptedOrders();
        reportAutomaticCheckpoints = static_cast<size_t>(std::count_if(
            checkpoints.begin(), checkpoints.end(), [](const auto& item) { return item.automatic; }));
        reportManualCheckpoint = manual != checkpoints.end();
        if (!endgameRequested) {
          success = true;
          break;
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (error.empty() && !success) {
    error = "timed out waiting for the LAN campaign smoke phases";
  }
  const uint32_t finalTick = endgameRequested && reportTick != 0
      ? reportTick : (role == "guest" ? game->ReplicaTick() : static_cast<uint32_t>(GetGameTickCount()));
  const uint64_t finalAcceptedOrders = endgameRequested && reportTick != 0
      ? reportAcceptedOrders : game->AcceptedOrders();
  std::cout << "LAN smoke " << role << (success ? " succeeded" : " failed") << ": "
            << (error.empty() ? game->StatusText() : error) << ", tick " << finalTick
            << ", accepted orders " << game->AcceptedOrders() << '\n';
  const auto finalCheckpoints = game->Checkpoints();
  const size_t automaticCheckpointCount = endgameRequested && reportTick != 0
      ? reportAutomaticCheckpoints : static_cast<size_t>(std::count_if(
          finalCheckpoints.begin(), finalCheckpoints.end(), [](const auto& item) { return item.automatic; }));
  const bool manualCheckpointExists = endgameRequested && reportTick != 0
      ? reportManualCheckpoint : std::any_of(finalCheckpoints.begin(), finalCheckpoints.end(),
                                             [](const auto& item) { return !item.automatic; });
  const bool lanClosed = !game->Active();
  WriteReport(output, role, success, error, initialTick, finalTick, finalAcceptedOrders, orderSent, renameApplied,
              reconnectVerified, automaticCheckpointCount, manualCheckpointExists, battleCompleted, guestVoted,
              endgameCompleted, app.m_state, lanClosed, checkpointPath, resumed);
  game->Close();
  app.DestroyAllWindows();
  app.DestroyNativeWindow();
  return success ? 0 : 1;
}
