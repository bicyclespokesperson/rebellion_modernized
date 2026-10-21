#include "port/native/lan_game.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iomanip>
#include <random>
#include <sstream>

#include "app/battle_flow.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/iostream_lib.h"
#include "core/memory_stream.h"
#include "events/game_clock_hooks.h"
#include "events/game_step_driver.h"
#include "events/game_manager.h"
#include "gameobj/factory_registries.h"
#include "gameobj/notif_payload.h"
#include "multiplayer/lan_player.h"
#include "multiplayer/world_replica.h"
#include "orders/order_base.h"
#include "port/minwin_impl/config/config.h"
#include "port/native/game_context.h"
#include "port/native/lan_wire.h"
#include "port/native/platform.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"

#ifndef REB_LAN_BUILD_ID
#define REB_LAN_BUILD_ID "development"
#endif

namespace reb::native
{
namespace
{
using Clock = std::chrono::steady_clock;
enum Kind : uint32_t { Hello = 1, Welcome, Settings, Ready, Begin, BeginAck, World, Order, Notice,
                       Pause, SaveRequest, Checkpoint, Heartbeat, ErrorMessage, Leave, BattleVote, BattleResponse,
                       EndGame };
constexpr uint32_t kSnapshotVersion = 1;
constexpr size_t kMaximumOrder = 256 * 1024;

std::chrono::seconds AutosaveInterval()
{
  try {
    const std::string configured = mw::config::Global().Get("multiplayer_autosave_seconds", "300");
    size_t end = 0;
    const int seconds = std::stoi(configured, &end);
    if (end == configured.size() && seconds >= 1 && seconds <= 3600) {
      return std::chrono::seconds(seconds);
    }
  } catch (const std::exception&) {
  }
  return std::chrono::minutes(5);
}

std::string NewId()
{
  std::random_device random;
  std::ostringstream text;
  text << std::hex << std::setfill('0');
  for (int i = 0; i < 4; ++i) {
    text << std::setw(8) << static_cast<uint32_t>(random());
  }
  return text.str();
}

std::filesystem::path CheckpointDirectory()
{
  const std::string configured = mw::config::Global().Get("multiplayer_save_dir");
  if (!configured.empty()) {
    return configured;
  }
  return std::filesystem::path(reb::platform::UserConfigDir()) / "Multiplayer";
}

uint64_t DataFingerprint()
{
  struct DataFile {
    std::string key;
    std::string relativePath;
    std::filesystem::path path;
  };
  std::vector<DataFile> files;
  const std::filesystem::path install = mw::config::Global().InstallDir();
  for (const char* folder : {"GData", "EData"}) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(install / folder, error)) {
      if (entry.is_regular_file()) {
        std::string relative = entry.path().lexically_relative(install).generic_string();
        std::string key = relative;
        std::ranges::transform(key, key.begin(), [](unsigned char character) {
          return static_cast<char>(std::tolower(character));
        });
        files.push_back({std::move(key), std::move(relative), entry.path()});
      }
    }
    if (error) throw std::runtime_error("Cannot list game data: " + (install / folder).string());
  }
  std::ranges::sort(files, [](const DataFile& left, const DataFile& right) {
    if (left.key != right.key) return left.key < right.key;
    return left.relativePath < right.relativePath;
  });
  uint64_t hash = 14695981039346656037ULL;
  std::array<char, 16384> buffer;
  const auto fold = [&](uint8_t byte) { hash = (hash ^ byte) * 1099511628211ULL; };
  const auto foldLength = [&](uint64_t length) {
    for (int i = 0; i < 8; ++i) fold(static_cast<uint8_t>(length >> (i * 8)));
  };
  for (const auto& file : files) {
    foldLength(file.key.size());
    for (unsigned char character : file.key) fold(character);
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(file.path, error);
    if (error) throw std::runtime_error("Cannot read game data: " + file.path.string());
    foldLength(size);
    std::ifstream input(file.path, std::ios::binary);
    if (!input) {
      throw std::runtime_error("Cannot read game data: " + file.path.string());
    }
    while (input) {
      input.read(buffer.data(), buffer.size());
      for (std::streamsize i = 0; i < input.gcount(); ++i) {
        fold(static_cast<uint8_t>(buffer[i]));
      }
    }
    if (!input.eof()) {
      throw std::runtime_error("Cannot read game data: " + file.path.string());
    }
  }
  return hash;
}

std::vector<uint8_t> Serialize(const std::function<bool(ostream*)>& write)
{
  reb::io::MemoryWriter stream;
  if (!write(stream.Out()) || !stream.Good()) {
    return {};
  }
  return stream.TakeBytes();
}

using Input = reb::io::MemoryReader;

void WriteSettings(lan::Writer& out, const LanSettings& settings)
{
  out.I32(settings.hostSide);
  out.I32(settings.galaxySize);
  out.I32(settings.victoryMode);
  out.I32(settings.seed);
}

bool ReadSettings(lan::Reader& in, LanSettings& settings)
{
  settings.hostSide = in.I32();
  settings.galaxySize = in.I32();
  settings.victoryMode = in.I32();
  settings.seed = in.I32();
  return in.Good() && settings.hostSide >= 1 && settings.hostSide <= 2 && settings.galaxySize >= 1
         && settings.galaxySize <= 3 && settings.victoryMode >= 1 && settings.victoryMode <= 2;
}

bool SaveDriver(std::vector<uint8_t>& bytes)
{
  GameStepDriver* driver = GetGameStepDriver();
  bytes = Serialize([&](ostream* out) { return driver && driver->Save(out); });
  return !bytes.empty();
}

bool RestoreDriver(RebellionApplication& app, std::span<const uint8_t> bytes, int humanSide,
                   reb::multiplayer::WorldReplicaState& replica)
{
  app.ReleaseOverlayWindow();
  app.DetachActiveWindow();
  app.DestroyStrategicWindow();
  replica.Reset();
  CreateBattleFlow();
  BattleFlow* flow = GetBattleFlow();
  if (!flow || !flow->CreateStepDriver()) {
    return false;
  }
  Input input(bytes);
  GameStepDriver* driver = flow->StepDriver();
  if (!driver->Load(input.In()) || !input.Good() || !input.AtEnd()
      || !driver->Session()->UseLanControllers(humanSide)) {
    return false;
  }
  flow->m_state = kFlowStrategic;
  flow->m_ok = 1;
  app.m_side = humanSide;
  app.m_gameType = 1;
  app.m_networkMode = 0;
  app.m_unknown130 = 1;
  app.m_unknown134 = 0;
  driver->m_setupDone = 1;
  driver->m_hold = 0;
  app.SetState(kStateStrategic);
  return true;
}
} // namespace

struct LanGame::Impl
{
  lan::Transport connection;
  lan::Transport discovery;
  LanSettings settings;
  CampaignCheckpoints checkpoints{CheckpointDirectory()};
  CampaignCheckpointCadence cadence{AutosaveInterval()};
  reb::multiplayer::WorldReplicaState replica;
  bool active = false;
  bool host = false;
  bool ready = false;
  bool peerReady = false;
  bool verified = false;
  bool playing = false;
  bool starting = false;
  bool waitingForBeginAck = false;
  bool beginPending = false;
  bool paused = false;
  bool handlingRemote = false;
  bool checkpointPending = false;
  bool checkpointAutomatic = false;
  bool helloSent = false;
  bool peerResponding = false;
  bool reconnecting = false;
  bool reconnectBlocked = false;
  bool awaitingSnapshot = false;
  bool battleVotePending = false;
  bool battleVoteRequestedOnConnection = false;
  bool battleVoteDone = false;
  int battleVoteId = 0;
  uint32_t battleVoteKey = 0;
  int lastBattleState = 0;
  int pendingMovieState = 0;
  uint32_t lastBattleTimeBits = 0;
  uint16_t port = lan::kDefaultPort;
  uint64_t fingerprint = 0;
  uint64_t acceptedOrders = 0;
  uint64_t sentOrder = 0;
  uint64_t receivedOrder = 0;
  uint32_t replicaTick = 0;
  uint32_t expectedSnapshotTick = 0;
  uint64_t expectedSnapshotFingerprint = 0;
  std::string address;
  std::string name = "Rebellion campaign";
  std::string campaignId;
  std::string token;
  std::string status = "Host a campaign or join a nearby game.";
  std::string lastSave;
  std::string windowTitle;
  Clock::time_point lastPoll = Clock::now();
  Clock::time_point lastReceive = Clock::now();
  Clock::time_point lastHeartbeat = Clock::now();
  Clock::time_point lastWorld = Clock::now();
  Clock::time_point nextRetry = Clock::now();
  Clock::time_point lastSaveAttempt = Clock::now();
  lan::Status previousStatus = lan::Status::Idle;
  std::vector<std::vector<uint8_t>> notices;

  bool Send(uint32_t kind, const lan::Writer& out)
  {
    return out.Good() && connection.Send(kind, out.Bytes());
  }

  void SendSettings()
  {
    lan::Writer out;
    WriteSettings(out, settings);
    out.U32(ready);
    Send(Kind::Settings, out);
  }

  void Fail(const std::string& message, bool tellPeer = true)
  {
    status = message;
    ready = false;
    peerReady = false;
    verified = false;
    peerResponding = false;
    waitingForBeginAck = false;
    beginPending = false;
    if (tellPeer) {
      lan::Writer out;
      out.String(message);
      Send(ErrorMessage, out);
      connection.Poll();
    }
    connection.DisconnectPeer();
    std::fprintf(stderr, "LAN: %s\n", message.c_str());
  }

  void SendBegin()
  {
    if (!GetBattleFlow() || !GetGameStepDriver() || GetBattleFlow()->m_state != kFlowStrategic
        || GetGameStepDriver()->m_phase != 0) {
      beginPending = true;
      status = "Waiting for the current campaign step before synchronizing...";
      return;
    }
    beginPending = false;
    battleVotePending = false;
    std::vector<uint8_t> state;
    if (!SaveDriver(state)) {
      Fail("Could not prepare the campaign for the other player.");
      return;
    }
    lan::Writer out;
    out.U32(kSnapshotVersion);
    WriteSettings(out, settings);
    out.String(campaignId);
    out.String(name);
    out.U32(paused);
    out.Blob(state);
    const uint64_t snapshotFingerprint = lan::Fingerprint(state);
    out.U64(snapshotFingerprint);
    if (Send(Begin, out)) {
      expectedSnapshotTick = static_cast<uint32_t>(GetGameTickCount());
      expectedSnapshotFingerprint = snapshotFingerprint;
      waitingForBeginAck = true;
      status = "Sending campaign to the other player...";
    } else {
      Fail("Could not send the campaign snapshot.");
    }
  }

  bool SendPendingBattleVote()
  {
    if (!battleVotePending || !playing || !verified || connection.GetStatus() != lan::Status::Connected)
      return false;
    if (!GetGameManager()) {
      Fail("Could not prepare the battle decision.");
      return false;
    }
    std::vector<uint8_t> world;
    std::string error;
    if (!replica.Capture(*GetGameManager(), world, error) || !connection.Send(World, world)) {
      Fail("Could not send battle state: " + error);
      return false;
    }
    for (const auto& notice : notices) {
      if (!connection.Send(Notice, notice)) {
        Fail("Could not send battle notices.");
        return false;
      }
    }
    notices.clear();
    lan::Writer out;
    out.I32(battleVoteId);
    out.U32(battleVoteKey);
    out.U32(battleVoteDone);
    if (!Send(BattleVote, out)) {
      Fail("Could not send battle decision request.");
      return false;
    }
    return true;
  }

  void Receive(RebellionApplication& app, const lan::Message& message)
  {
    lan::Reader in(message.bytes);
    if (message.kind == Hello && host) {
      const auto protocol = in.U32();
      const auto build = in.String();
      const auto data = in.U64();
      const auto reconnectToken = in.String();
      if (!in.Done() || protocol != lan::kProtocol || build != REB_LAN_BUILD_ID) {
        Fail("Game versions differ. Install the same build on both computers.");
        return;
      }
      if (data != fingerprint) {
        Fail("Game data differs. Use matching game data and rules on both computers.");
        return;
      }
      if (playing && reconnectToken != token) {
        Fail("This campaign is in progress. Rejoin from the original game window.");
        return;
      }
      verified = true;
      peerResponding = true;
      lan::Writer out;
      out.String(token);
      out.U32(playing);
      out.U64(receivedOrder);
      Send(Welcome, out);
      SendSettings();
      if (playing) {
        if (battleVotePending && GetBattleFlow()
            && ((GetBattleFlow()->m_state == kFlowBattleWaitForType && !battleVoteDone)
                || (GetBattleFlow()->m_state == kFlowBattleWaitForDone && battleVoteDone))
            && !SendPendingBattleVote()) return;
        SendBegin();
      } else {
        status = "Player connected. Choose settings, then both select Ready.";
      }
      return;
    }
    if (message.kind == Welcome && !host) {
      const auto receivedToken = in.String();
      const uint32_t resumedValue = in.U32();
      const uint64_t receivedSequence = in.U64();
      if (!in.Done() || receivedToken.empty() || resumedValue > 1
          || (playing && (receivedToken != token || resumedValue == 0 || receivedSequence > sentOrder))) {
        Fail("Invalid session reply.");
        return;
      }
      token = receivedToken;
      sentOrder = receivedSequence;
      const bool resumed = resumedValue != 0;
      awaitingSnapshot = resumed;
      verified = true;
      peerResponding = true;
      reconnecting = false;
      status = resumed ? "Receiving the current campaign..." : "Connected. Choose Ready when you are ready to play.";
      return;
    }
    if (message.kind == ErrorMessage) {
      const auto reason = in.String();
      if (!host) reconnectBlocked = true;
      Fail(!in.Done() || reason.empty() ? "Connection rejected." : reason, false);
      return;
    }
    if (!verified) {
      Fail("The other player did not complete the connection check.");
      return;
    }
    switch (message.kind) {
      case Kind::Settings:
        if (host || !ReadSettings(in, settings)) {
          Fail("Invalid campaign settings.");
          return;
        }
        {
          const uint32_t readyValue = in.U32();
          if (readyValue > 1) {
            Fail("Invalid campaign settings.");
            return;
          }
          peerReady = readyValue != 0;
        }
        ready = false;
        {
          lan::Writer out;
          out.U32(0);
          Send(Kind::Ready, out);
        }
        break;
      case Kind::Ready:
        {
          const uint32_t readyValue = in.U32();
          if (readyValue > 1) {
            Fail("Invalid ready state.");
            return;
          }
          peerReady = readyValue != 0;
        }
        break;
      case Begin:
        {
          if (host || in.U32() != kSnapshotVersion || !ReadSettings(in, settings)) {
            Fail("Unsupported campaign snapshot.");
            return;
          }
          const auto receivedCampaignId = in.String();
          const auto receivedName = in.String();
          const uint32_t receivedPaused = in.U32();
          const auto state = in.Blob();
          const auto snapshotFingerprint = in.U64();
          if (!in.Done() || state.empty() || receivedCampaignId.empty() || receivedPaused > 1
              || (playing && receivedCampaignId != campaignId)
              || snapshotFingerprint != lan::Fingerprint(state)) {
            Fail("The campaign snapshot is incomplete.");
            return;
          }
          campaignId = receivedCampaignId;
          name = receivedName;
          paused = receivedPaused != 0;
          handlingRemote = true;
          const bool restored = RestoreDriver(app, state, 3 - settings.hostSide, replica);
          handlingRemote = false;
          if (!restored) {
            Fail("Could not load the host's campaign.");
            return;
          }
          playing = true;
          awaitingSnapshot = false;
          battleVotePending = false;
          battleVoteRequestedOnConnection = false;
          replicaTick = GetGameTickCount();
          cadence.Reset(replicaTick);
          lan::Writer ack;
          ack.U32(replicaTick);
          ack.U64(snapshotFingerprint);
          Send(BeginAck, ack);
          status = paused ? "Campaign paused." : "Connected to host.";
          return;
        }
      case BeginAck:
        if (!host || !waitingForBeginAck || in.U32() != expectedSnapshotTick
            || in.U64() != expectedSnapshotFingerprint || GetGameTickCount() != static_cast<int>(expectedSnapshotTick)) {
          Fail("The other player loaded a different campaign state.");
          return;
        }
        waitingForBeginAck = false;
        status = paused ? "Campaign paused." : "Both players connected.";
        break;
      case World:
        if (host || !playing || !GetGameManager()) {
          Fail("Unexpected campaign update.");
          return;
        }
        {
          std::string error;
          if (!replica.Apply(*GetGameManager(), message.bytes, error)) {
            Fail("Could not apply campaign update: " + error);
            return;
          }
          replicaTick = GetGameTickCount();
          const GameRoot* root = GetGameManager()->Root();
          if (battleVotePending && battleVoteDone && root && !root->m_key8c.IsSet() && root->m_unk70 == 0) {
            battleVotePending = false;
            battleVoteRequestedOnConnection = false;
            status = paused ? "Campaign paused." : "Connected to host.";
          }
          UiSetDateAndSpeed(GameHookDay(), GetGameSession()->m_human->m_speed);
          UiRefreshChildWindows();
          if (app.m_strategicWindow) {
            reb::compat::InvalidateRect(app.m_strategicWindow->m_hwnd, nullptr, FALSE);
          }
          return;
        }
      case Order:
        {
          const uint64_t sequence = in.U64();
          const auto bytes = in.Blob(kMaximumOrder);
          if (!host || !playing || !in.Done() || bytes.size() < 4 || sequence != receivedOrder + 1) {
            Fail("Invalid player order.");
            return;
          }
          receivedOrder = sequence;
          Input input(bytes);
          int kind = 0;
          ReadValue32(input.In(), &kind);
          if (kind < 0x200 || kind > 0x280) {
            Fail("Unsupported player order.");
            return;
          }
          std::unique_ptr<MessageBase> order(static_cast<MessageBase*>(CreateMessageByKind(kind)));
          if (!order || !order->Read(input.In()) || !input.Good() || !input.AtEnd()) {
            Fail("Could not read player order.");
            return;
          }
          auto* sided = dynamic_cast<SidedMessage*>(order.get());
          if (!sided || sided->m_side != 3 - settings.hostSide) {
            Fail("Player order belongs to the wrong faction.");
            return;
          }
          StatusCode validation;
          if (!sided->CheckValid(&validation)) {
            lan::Writer out;
            out.U32(0);
            out.String("An order could not be applied because the game state changed. Try it again.");
            Send(Notice, out);
            return;
          }
          order->SetId(GetGameSession()->m_controllers[sided->m_side - 1]->NextMessageId());
          handlingRemote = true;
          GetGameSession()->RouteMessage(order.release(), 3 - settings.hostSide);
          handlingRemote = false;
          ++acceptedOrders;
          return;
        }
      case Notice:
        {
          if (host) {
            Fail("Unexpected player notification.");
            return;
          }
          const auto noticeKind = in.U32();
          if (noticeKind == 0) {
            status = in.String();
          } else {
            const auto bytes = in.Blob(kMaximumOrder);
            auto notice = std::unique_ptr<NotifPayload>(static_cast<NotifPayload*>(CreateKeyedObject(noticeKind)));
            if (!notice || !in.Done()) {
              Fail("Invalid campaign notification.");
              return;
            }
            Input input(bytes);
            notice->Read(input.In());
            if (!input.Good() || !input.AtEnd() || notice->m_value18 != 3 - settings.hostSide) {
              Fail("Invalid campaign notification data.");
              return;
            }
            GetGameSession()->m_human->HandleNotification(notice.release());
          }
          break;
        }
      case BattleVote:
        {
          const int voteId = in.I32();
          const uint32_t keyValue = in.U32();
          const uint32_t done = in.U32();
          GameSession* session = GetGameSession();
          if (host || !playing || !in.Done() || voteId < 0 || keyValue == 0 || done > 1
              || !session || !session->m_human) {
            Fail("Invalid battle decision request.");
            return;
          }
          const bool sameVote = battleVotePending && battleVoteId == voteId
                                && battleVoteKey == keyValue && battleVoteDone == (done != 0);
          battleVotePending = true;
          battleVoteRequestedOnConnection = true;
          battleVoteId = voteId;
          battleVoteKey = keyValue;
          battleVoteDone = done != 0;
          lastBattleState = 0;
          if (!sameVote) {
            ObjectKey key;
            key.m_value = keyValue;
            if (battleVoteDone) session->m_human->ResetVote(voteId, &key);
            else session->m_human->BeginVote(voteId, &key);
          }
          status = battleVoteDone ? "Confirm the battle result." : "Choose how to fight this battle.";
          return;
        }
      case BattleResponse:
        {
          const int voteId = in.I32();
          const uint32_t keyValue = in.U32();
          const uint32_t done = in.U32();
          const int voteState = in.I32();
          const uint32_t timeBits = in.U32();
          const float voteTime = std::bit_cast<float>(timeBits);
          GameSession* session = GetGameSession();
          const int guestSide = 3 - settings.hostSide;
          auto* passive = session ? dynamic_cast<LanPlayer*>(session->m_controllers[guestSide - 1]) : nullptr;
          if (!host || !playing || !in.Done() || !battleVotePending || voteId != battleVoteId
              || keyValue != battleVoteKey || done != static_cast<uint32_t>(battleVoteDone)
              || (battleVoteDone ? voteState != 3 : (voteState != 1 && voteState != 2))
              || !std::isfinite(voteTime) || voteTime < 0 || voteTime > 10000
              || !passive || passive->m_voteId != voteId || passive->m_voteKey.m_value != keyValue
              || (passive->m_voteState != 0 && (passive->m_voteState != voteState
                                                || passive->m_voteTime.m_value != voteTime))) {
            Fail("Invalid battle decision response.");
            return;
          }
          passive->m_voteState = voteState;
          passive->m_voteTime.m_value = voteTime;
          return;
        }
      case Pause:
        if (host) {
          paused = !paused;
          lan::Writer out;
          out.U32(paused);
          Send(Pause, out);
        } else {
          const uint32_t pauseValue = in.U32();
          if (pauseValue > 1) {
            Fail("Invalid pause state.");
            return;
          }
          paused = pauseValue != 0;
        }
        status = paused ? "Campaign paused. Press Pause to resume." : "Both players connected.";
        break;
      case SaveRequest:
        if (!host || !playing) {
          Fail("Unexpected save request.");
          return;
        }
        checkpointPending = true;
        checkpointAutomatic = false;
        break;
      case Checkpoint:
        {
          if (host) {
            Fail("Unexpected checkpoint transfer.");
            return;
          }
          CampaignCheckpointMetadata metadata;
          metadata.campaignId = in.String();
          metadata.sessionId = in.String();
          metadata.displayName = in.String();
          metadata.gameTick = in.U32();
          const uint32_t hostSide = in.U32();
          metadata.hostSide = static_cast<uint8_t>(hostSide);
          metadata.unixTimestamp = static_cast<int64_t>(in.U64());
          const uint32_t automaticValue = in.U32();
          const bool automatic = automaticValue != 0;
          const auto bytes = in.Blob();
          if (!in.Done() || bytes.empty() || hostSide < 1 || hostSide > 2 || automaticValue > 1
              || metadata.campaignId != campaignId || metadata.sessionId != token
              || metadata.hostSide != settings.hostSide) {
            Fail("Invalid checkpoint transfer.");
            return;
          }
          const auto result = checkpoints.Save(metadata, bytes, automatic);
          lastSave = result ? "Checkpoint copied to this computer." : "Backup save failed: " + result.error;
          return;
        }
      case Heartbeat:
        break;
      case EndGame:
        {
          const uint32_t hostMovieState = in.U32();
          if (host || !playing || !in.Done() || hostMovieState < kStateVictoryMovieEmpire
              || hostMovieState > kStateDefeatMovieAlliance) {
            Fail("Invalid campaign outcome.");
            return;
          }
          switch (hostMovieState) {
            case kStateVictoryMovieEmpire: pendingMovieState = kStateDefeatMovieAlliance; break;
            case kStateVictoryMovieAlliance: pendingMovieState = kStateDefeatMovieEmpire; break;
            case kStateDefeatMovieEmpire: pendingMovieState = kStateVictoryMovieAlliance; break;
            case kStateDefeatMovieAlliance: pendingMovieState = kStateVictoryMovieEmpire; break;
          }
          return;
        }
      case Leave:
        reconnectBlocked = true;
        Fail("The other player left. Reconnect or resume a saved checkpoint.", false);
        return;
      default:
        Fail("Unsupported network message.");
        return;
    }
    if (!in.Done()) {
      Fail("Invalid network message.");
    }
  }
};

LanGame::LanGame() : impl_(std::make_unique<Impl>()) {}
LanGame::~LanGame() = default;

LanGame* CurrentLanGame()
{
  auto* app = dynamic_cast<RebellionApplication*>(g_app);
  return app ? &app->m_context.GetLanGame() : nullptr;
}

bool LanGame::Host(const std::string& name, uint16_t port)
{
  Close();
  auto& state = *impl_;
  state.active = true;
  state.host = true;
  state.name = name.empty() ? "Rebellion campaign" : name.substr(0, 80);
  state.port = port;
  state.token = NewId();
  state.campaignId = NewId();
  state.settings.seed = static_cast<int>(std::random_device{}() & 0x7fffffffU);
  try {
    state.fingerprint = DataFingerprint();
  } catch (const std::exception& error) {
    state.Fail(error.what(), false);
    return false;
  }
  if (!state.connection.Host(state.name, port)) {
    state.Fail(state.connection.Error(), false);
    return false;
  }
  state.port = state.connection.LocalPort();
  state.status = "Hosting on port " + std::to_string(state.port) + ". Waiting for the other player.";
  return true;
}

bool LanGame::Join(const std::string& address, uint16_t port)
{
  Close();
  auto& state = *impl_;
  state.active = true;
  state.host = false;
  state.address = address;
  state.port = port;
  try {
    state.fingerprint = DataFingerprint();
  } catch (const std::exception& error) {
    state.Fail(error.what(), false);
    return false;
  }
  if (!state.connection.Join(address, port)) {
    state.Fail(state.connection.Error(), false);
    return false;
  }
  state.status = "Connecting to " + address + "...";
  return true;
}

void LanGame::Discover(uint16_t port)
{
  impl_->discovery.Discover(port);
}

void LanGame::Close()
{
  if (impl_->connection.GetStatus() == lan::Status::Connected) {
    lan::Writer out;
    impl_->Send(Leave, out);
    impl_->connection.Drain(std::chrono::milliseconds(250));
  }
  impl_ = std::make_unique<Impl>();
}

void LanGame::Poll(RebellionApplication& app)
{
  auto& state = *impl_;
  state.discovery.Poll();
  if (!state.active) {
    return;
  }
  const auto now = Clock::now();
  const auto elapsed = now - state.lastPoll;
  state.lastPoll = now;
  state.connection.Poll();
  if (state.host && state.battleVotePending && GetBattleFlow()
      && GetBattleFlow()->m_state == kFlowStrategic) state.battleVotePending = false;
  const auto status = state.connection.GetStatus();
  if (status == lan::Status::Connected && state.previousStatus != status) {
    state.lastReceive = now;
    state.helloSent = false;
    state.verified = false;
    state.peerResponding = false;
    if (!state.host && state.playing) state.awaitingSnapshot = true;
    if (!state.host) state.battleVoteRequestedOnConnection = false;
  }
  if (status == lan::Status::Connected && !state.host && !state.helloSent) {
    lan::Writer hello;
    hello.U32(lan::kProtocol);
    hello.String(REB_LAN_BUILD_ID);
    hello.U64(state.fingerprint);
    hello.String(state.token);
    state.Send(Hello, hello);
    state.helloSent = true;
  }
  state.previousStatus = status;
  while (state.verified || state.connection.GetStatus() == lan::Status::Connected) {
    auto message = state.connection.TakeMessage();
    if (!message) break;
    state.lastReceive = now;
    state.peerResponding = true;
    state.Receive(app, *message);
    if (state.pendingMovieState != 0) {
      const int movieState = state.pendingMovieState;
      app.SetState(movieState);
      return;
    }
    if (!state.verified) break;
  }
  if (!state.host && state.battleVotePending) SubmitBattleVote();
  if (state.connection.GetStatus() == lan::Status::Connected &&
      ((!state.verified && now - state.lastReceive > std::chrono::seconds(5)) ||
       (state.verified && now - state.lastReceive > std::chrono::seconds(10)))) {
    state.Fail(state.verified ? "The other computer stopped responding." : "Connection check timed out.", false);
  }
  const auto currentStatus = state.connection.GetStatus();
  if (currentStatus == lan::Status::Disconnected || currentStatus == lan::Status::Error) {
    state.peerResponding = false;
    state.verified = false;
    state.waitingForBeginAck = false;
    state.beginPending = false;
    if (!state.playing) {
      state.ready = false;
      state.peerReady = false;
    }
    if (state.reconnectBlocked) {
      // Keep the peer's rejection reason visible until the player starts another session.
    } else if (state.playing) {
      state.status = "Connection lost. Campaign paused; waiting to reconnect.";
      if (!state.host && now >= state.nextRetry) {
        if (state.connection.Join(state.address, state.port)) {
          state.previousStatus = lan::Status::Connecting;
        }
        state.nextRetry = now + std::chrono::seconds(2);
        state.reconnecting = true;
      }
    } else if (currentStatus == lan::Status::Error) {
      if (!state.connection.Error().empty()) state.status = state.connection.Error();
    } else if (state.host) {
      state.status = "Waiting for the other player.";
    } else {
      state.status = "Connection closed. Join the game again.";
    }
  }
  if (currentStatus == lan::Status::Connected && now - state.lastHeartbeat >= std::chrono::seconds(1)) {
    lan::Writer heartbeat;
    if (state.verified) {
      state.Send(Heartbeat, heartbeat);
    }
    state.lastHeartbeat = now;
  }
  if (currentStatus == lan::Status::Connected && now - state.lastReceive > std::chrono::seconds(3)) {
    state.peerResponding = false;
    if (state.playing) {
      state.status = "Waiting for the other computer. Campaign paused.";
    }
  }
  if (state.starting && GetBattleFlow() && GetGameStepDriver()) {
    GameStepDriver* driver = GetGameStepDriver();
    if (driver->m_setupStage == 3) {
      driver->m_setupDone = 1;
    }
    if (GetBattleFlow()->m_state == kFlowStrategic) {
      state.starting = false;
      state.playing = true;
      driver->m_hold = 0;
      driver->Session()->m_viewReady = 1;
      app.SetState(kStateStrategic);
      state.cadence.Reset(GetGameTickCount());
      state.SendBegin();
      state.checkpointPending = true;
      state.checkpointAutomatic = true;
    }
  }
  if (state.beginPending && state.host && state.connection.GetStatus() == lan::Status::Connected
      && state.verified && GetBattleFlow() && GetBattleFlow()->m_state == kFlowStrategic
      && GetGameStepDriver() && GetGameStepDriver()->m_phase == 0) {
    state.SendBegin();
  }
  if (state.playing && state.host && GetGameStepDriver()) {
    const bool paused = state.paused || GetGameStepDriver()->m_hold != 0 || GetGameStepDriver()->m_pauseTick != 0
                        || app.m_state != kStateStrategic;
    if (state.cadence.Update(elapsed, Connected() && !state.waitingForBeginAck, paused, GetGameTickCount())) {
      if (!state.checkpointPending) {
        state.checkpointPending = true;
        state.checkpointAutomatic = true;
      }
    }
  }
  if (state.playing) {
    const std::string title = "Rebellion | " + state.status + (state.lastSave.empty() ? "" : " | " + state.lastSave);
    if (title != state.windowTitle) {
      state.windowTitle = title;
      app.SetWindowTitle(title.c_str());
    }
  }
  if (state.playing && !state.host && GetGameSession()) {
    GetGameSession()->Update();
  }
}

void LanGame::AfterStep(RebellionApplication& app)
{
  auto& state = *impl_;
  if (!state.active || !state.playing || !state.host || !GetBattleFlow() || !GetGameStepDriver()) {
    return;
  }
  const auto now = Clock::now();
  if (state.checkpointPending && GetBattleFlow()->m_state == kFlowStrategic
      && GetGameStepDriver()->m_phase == 0
      && now - state.lastSaveAttempt >= std::chrono::seconds(2)) {
    state.lastSaveAttempt = now;
    SaveCheckpoint(app, state.checkpointAutomatic);
  }
  if (Connected() && state.connection.PendingSendBytes() == 0
      && !state.waitingForBeginAck && now - state.lastWorld >= std::chrono::milliseconds(200)
      && GetBattleFlow()->m_state == kFlowStrategic && GetGameStepDriver()->m_phase == 0) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!state.replica.Capture(*GetGameManager(), bytes, error) || !state.connection.Send(World, bytes)) {
      state.Fail("Could not send campaign update: " + error);
      return;
    }
    state.replicaTick = GetGameTickCount();
    state.lastWorld = now;
    for (const auto& notice : state.notices) {
      state.connection.Send(Notice, notice);
    }
    state.notices.clear();
  }
}

void LanGame::FinishGame(int movieState)
{
  auto& state = *impl_;
  if (!state.active || !state.host || !state.playing || !state.verified || !Connected()
      || movieState < kStateVictoryMovieEmpire || movieState > kStateDefeatMovieAlliance) return;
  lan::Writer out;
  out.U32(static_cast<uint32_t>(movieState));
  if (!state.Send(EndGame, out)) {
    std::fprintf(stderr, "LAN: could not queue campaign outcome.\n");
  } else if (!state.connection.Drain(std::chrono::seconds(2))) {
    std::fprintf(stderr, "LAN: campaign outcome did not drain before closing.\n");
  }
}

void LanGame::SetReady(bool ready)
{
  impl_->ready = ready;
  lan::Writer out;
  out.U32(ready);
  impl_->Send(Kind::Ready, out);
}

void LanGame::SetSettings(const LanSettings& settings)
{
  if (!impl_->host || impl_->playing || impl_->starting || settings.hostSide < 1 || settings.hostSide > 2 || settings.galaxySize < 1
      || settings.galaxySize > 3 || settings.victoryMode < 1 || settings.victoryMode > 2) {
    return;
  }
  impl_->settings = settings;
  if (impl_->settings.seed == 0) {
    impl_->settings.seed = static_cast<int>(std::random_device{}() & 0x7fffffffU);
  }
  impl_->ready = false;
  impl_->peerReady = false;
  impl_->SendSettings();
}

bool LanGame::StartNew(RebellionApplication& app)
{
  auto& state = *impl_;
  if (!state.host || !Connected() || !state.ready || !state.peerReady || state.playing || state.starting) {
    state.status = "Connect both players and select Ready before starting.";
    return false;
  }
  state.replica.Reset();
  app.m_side = state.settings.hostSide;
  app.m_gameType = 1;
  app.m_networkMode = 0;
  app.m_unknown130 = 1;
  app.m_unknown134 = 0;
  CreateBattleFlow();
  GameSettings settings;
  settings.m_field04 = state.settings.galaxySize;
  settings.m_field08 = state.settings.victoryMode - 1;
  settings.m_field0c = 2;
  settings.m_field10 = state.settings.seed;
  if (!GetBattleFlow() || !GetBattleFlow()->StartGame(&settings, kPlayerLan, state.settings.hostSide)) {
    state.Fail("Could not create the campaign.");
    return false;
  }
  state.starting = true;
  state.status = "Creating the galaxy...";
  return true;
}

bool LanGame::Resume(RebellionApplication& app, const std::filesystem::path& checkpoint)
{
  auto& state = *impl_;
  if (!state.host || !Connected() || !state.ready || !state.peerReady || state.playing || state.starting) {
    state.status = "Connect both players and select Ready before resuming.";
    return false;
  }
  std::string error;
  auto saved = state.checkpoints.Load(checkpoint, &error);
  if (!saved || saved->metadata.protocolVersion != lan::kProtocol || saved->metadata.formatVersion != kSnapshotVersion
      || saved->metadata.hostSide < 1 || saved->metadata.hostSide > 2) {
    state.status = "Cannot load checkpoint: " + error;
    return false;
  }
  lan::Reader checkpointState(saved->gameState);
  const auto savedBuild = checkpointState.String();
  const auto savedData = checkpointState.U64();
  const auto driverState = checkpointState.Blob();
  if (!checkpointState.Done() || savedBuild.empty() || savedData != state.fingerprint || driverState.empty()) {
    state.status = "This checkpoint needs matching game data and a supported save format.";
    return false;
  }
  state.handlingRemote = true;
  const bool loaded = RestoreDriver(app, driverState, saved->metadata.hostSide, state.replica);
  state.handlingRemote = false;
  if (!loaded) {
    state.Fail("Could not restore this campaign.");
    return false;
  }
  state.settings.hostSide = saved->metadata.hostSide;
  GameSettings settings;
  GetGameManager()->GetSettings(&settings);
  state.settings.galaxySize = settings.m_field04;
  state.settings.victoryMode = settings.m_field08 + 1;
  state.settings.seed = settings.m_field10;
  state.campaignId = saved->metadata.campaignId;
  state.name = saved->metadata.displayName;
  state.playing = true;
  state.paused = false;
  state.cadence.Reset(GetGameTickCount());
  state.SendBegin();
  return true;
}

bool LanGame::SaveCheckpoint(RebellionApplication& app, bool automatic)
{
  return WriteCheckpoint(app, automatic, true);
}

bool LanGame::SaveBeforeExit(RebellionApplication& app)
{
  if (!IsHost()) return false;
  if (!GetBattleFlow() || GetBattleFlow()->m_state != kFlowStrategic) {
    impl_->lastSave = "A battle is in progress; the last completed checkpoint remains available.";
    return false;
  }
  return WriteCheckpoint(app, false, false);
}

bool LanGame::WriteCheckpoint(RebellionApplication&, bool automatic, bool requireCompleteStep)
{
  auto& state = *impl_;
  if (!state.playing) {
    return false;
  }
  if (!state.host) {
    lan::Writer out;
    state.Send(SaveRequest, out);
    state.lastSave = "Checkpoint requested from host...";
    return true;
  }
  if (!GetBattleFlow() || !GetGameStepDriver() || GetBattleFlow()->m_state != kFlowStrategic
      || (requireCompleteStep && GetGameStepDriver()->m_phase != 0)) {
    state.checkpointPending = true;
    state.checkpointAutomatic = automatic;
    state.lastSave = "Checkpoint queued until the current step or battle finishes.";
    return true;
  }
  std::vector<uint8_t> bytes;
  if (!SaveDriver(bytes)) {
    state.lastSave = "Checkpoint failed: could not serialize the campaign.";
    return false;
  }
  lan::Writer checkpointState;
  checkpointState.String(REB_LAN_BUILD_ID);
  checkpointState.U64(state.fingerprint);
  checkpointState.Blob(bytes);
  if (!checkpointState.Good()) {
    state.lastSave = "Checkpoint failed: campaign exceeds the supported size.";
    return false;
  }
  bytes = checkpointState.TakeBytes();
  CampaignCheckpointMetadata metadata;
  metadata.campaignId = state.campaignId;
  metadata.sessionId = state.token;
  metadata.gameTick = GetGameTickCount();
  metadata.hostSide = static_cast<uint8_t>(state.settings.hostSide);
  metadata.displayName = state.name;
  metadata.unixTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  auto result = state.checkpoints.Save(metadata, bytes, automatic);
  if (!result) {
    state.lastSave = "Checkpoint failed: " + result.error;
    return false;
  }
  state.checkpointPending = false;
  state.cadence.Saved(metadata.gameTick);
  state.lastSave = "Saved checkpoint at day " + std::to_string(GameHookDay()) + ".";
  lan::Writer out;
  out.String(metadata.campaignId);
  out.String(metadata.sessionId);
  out.String(metadata.displayName);
  out.U32(metadata.gameTick);
  out.U32(metadata.hostSide);
  out.U64(metadata.unixTimestamp);
  out.U32(automatic);
  out.Blob(bytes);
  if (state.connection.GetStatus() == lan::Status::Connected && !state.Send(Checkpoint, out)) {
    state.lastSave = "Saved locally, but could not copy the checkpoint to the other computer.";
  }
  return true;
}

bool LanGame::Submit(MessageBase* message, int senderSide)
{
  auto& state = *impl_;
  if (!state.active || state.host || state.handlingRemote || !message) {
    return false;
  }
  std::unique_ptr<MessageBase> owned(message);
  auto* sided = dynamic_cast<SidedMessage*>(message);
  if (!state.playing || !Connected()) {
    state.status = "Order not sent. Wait for the connection to return.";
    return true;
  }
  if (!sided || senderSide != 3 - state.settings.hostSide || sided->m_side != senderSide
      || message->GetKind() < 0x200 || message->GetKind() > 0x280) {
    return true;
  }
  auto bytes = Serialize([&](ostream* out) {
    const int kind = message->GetKind();
    WriteValue32(out, &kind);
    return message->Write(out) != 0;
  });
  lan::Writer out;
  out.U64(state.sentOrder + 1);
  out.Blob(bytes, kMaximumOrder);
  if (!bytes.empty() && state.Send(Order, out)) {
    ++state.sentOrder;
  } else {
    state.status = "Order could not be sent. Try again after reconnecting.";
  }
  return true;
}

bool LanGame::ForwardNotification(NotifPayload* notice)
{
  auto& state = *impl_;
  if (!state.active || !state.host || !notice) {
    return false;
  }
  std::unique_ptr<NotifPayload> owned(notice);
  const auto bytes = Serialize([&](ostream* out) { notice->Write(out); return true; });
  if (state.playing && !bytes.empty()) {
    lan::Writer out;
    out.U32(notice->GetKind());
    out.Blob(bytes, kMaximumOrder);
    if (out.Good() && state.notices.size() < 4096) {
      state.notices.push_back(out.Bytes());
    }
  }
  return true;
}

bool LanGame::RequestBattleVote(int voteId, uint32_t objectKey, bool done)
{
  auto& state = *impl_;
  if (!state.active || !state.host || !state.playing || voteId < 0 || objectKey == 0) return false;
  state.battleVotePending = true;
  state.battleVoteId = voteId;
  state.battleVoteKey = objectKey;
  state.battleVoteDone = done;
  if (state.connection.GetStatus() == lan::Status::Connected && state.verified)
    return state.SendPendingBattleVote();
  return true;
}

bool LanGame::SubmitBattleVote()
{
  auto& state = *impl_;
  if (!state.active || state.host || !state.playing || !state.battleVotePending
      || !state.battleVoteRequestedOnConnection || !state.verified
      || state.connection.GetStatus() != lan::Status::Connected) return false;
  GameSession* session = GetGameSession();
  if (!session || !session->m_human) return false;
  const HumanPlayer& human = *session->m_human;
  const int voteState = human.m_voteState;
  const float voteTime = human.m_voteTime.m_value;
  if (human.m_voteId != state.battleVoteId || human.m_voteKey.m_value != state.battleVoteKey
      || (state.battleVoteDone ? voteState != 3 : (voteState != 1 && voteState != 2))
      || !std::isfinite(voteTime) || voteTime < 0 || voteTime > 10000) return false;
  const uint32_t timeBits = std::bit_cast<uint32_t>(voteTime);
  if (state.lastBattleState == voteState && state.lastBattleTimeBits == timeBits) return true;
  lan::Writer out;
  out.I32(state.battleVoteId);
  out.U32(state.battleVoteKey);
  out.U32(state.battleVoteDone);
  out.I32(voteState);
  out.U32(timeBits);
  if (!state.Send(BattleResponse, out)) return false;
  state.lastBattleState = voteState;
  state.lastBattleTimeBits = timeBits;
  return true;
}

void LanGame::TogglePause()
{
  if (!Playing()) {
    return;
  }
  if (IsHost()) {
    impl_->paused = !impl_->paused;
    impl_->status = impl_->paused ? "Campaign paused. Press Pause to resume." : "Both players connected.";
    lan::Writer out;
    out.U32(impl_->paused);
    impl_->Send(Pause, out);
  } else {
    lan::Writer out;
    impl_->Send(Pause, out);
  }
}

void LanGame::Reconnect()
{
  if (!impl_->playing) {
    return;
  }
  impl_->connection.DisconnectPeer();
  impl_->verified = false;
  impl_->peerResponding = false;
  impl_->awaitingSnapshot = !impl_->host;
  impl_->reconnectBlocked = false;
  impl_->nextRetry = Clock::now();
  impl_->status = "Reconnecting. Campaign paused until both players are ready.";
}

bool LanGame::Active() const { return impl_->active; }
bool LanGame::IsHost() const { return impl_->active && impl_->host; }
bool LanGame::IsGuest() const { return impl_->active && !impl_->host; }
bool LanGame::Connected() const
{
  return impl_->verified && impl_->peerResponding && !impl_->awaitingSnapshot
         && impl_->connection.GetStatus() == lan::Status::Connected;
}
bool LanGame::Playing() const { return impl_->playing; }
bool LanGame::CanSimulate() const
{
  if (!impl_->active) return true;
  if (!impl_->host) return false;
  if (!impl_->playing) return true;
  if (impl_->waitingForBeginAck) return false;
  if (GetBattleFlow() && GetBattleFlow()->m_state == kFlowStrategic && GetGameStepDriver()
      && GetGameStepDriver()->m_phase != 0) return true;
  return (Connected() || impl_->beginPending) && !impl_->paused;
}
bool LanGame::Ready() const { return impl_->ready; }
bool LanGame::PeerReady() const { return impl_->peerReady; }
const LanSettings& LanGame::Settings() const { return impl_->settings; }
const std::string& LanGame::StatusText() const { return impl_->status; }
const std::string& LanGame::LastSaveText() const { return impl_->lastSave; }
std::vector<lan::Advertisement> LanGame::NearbyGames() const { return impl_->discovery.Peers(); }
std::vector<CampaignCheckpointInfo> LanGame::Checkpoints() const { return impl_->checkpoints.List(); }
uint16_t LanGame::Port() const { return impl_->port; }
bool LanGame::Paused() const { return impl_->paused; }
uint64_t LanGame::AcceptedOrders() const { return impl_->acceptedOrders; }
uint32_t LanGame::ReplicaTick() const { return impl_->replicaTick; }

} // namespace reb::native
