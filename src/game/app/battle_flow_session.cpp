#include <new>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <locale>
#include <span>
#include <utility>

#include "app/battle_flow.h"
#include "app/battle_engine_space.h"
#include "app/battle_live.h"
#include "space_battle/battle.h"
#include "space_battle/battle_save.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "events/game_clock_hooks.h"
#include "events/game_manager.h"
#include "port/native/game_context.h"
#include "ui/cool_app.h"

// Session lifetime and saves. Live battles retain the strategic game and append a versioned tactical checkpoint.


namespace
{

constexpr int kModernTacticalSaveState = 0x114;
constexpr uint32_t kTacticalSaveMagic = 0x564c4252;
constexpr uint32_t kTacticalSaveVersion = 1;
constexpr uint32_t kMaxTacticalBlobBytes = 64 * 1024 * 1024;

constexpr int kMinGalaxySize = 1;
constexpr int kMaxGalaxySize = 3;

bool WriteBlob(ostream* out, std::span<const uint8_t> bytes)
{
  if (bytes.size() > kMaxTacticalBlobBytes || !WriteCount32(out, bytes.size())) {
    return false;
  }
  ostream_write(out, bytes.data(), static_cast<int>(bytes.size()));
  return iostream_access::State(out) == 0;
}

bool WriteText(ostream* out, const std::string& text)
{
  return WriteBlob(out, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
}

bool ReadBlob(istream* in, std::vector<uint8_t>& bytes)
{
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::State(in) != 0 || count > kMaxTacticalBlobBytes) {
    return false;
  }
  bytes.resize(count);
  istream_read(in, bytes.data(), static_cast<int>(bytes.size()));
  return iostream_access::State(in) == 0;
}

std::string AsText(const std::vector<uint8_t>& bytes)
{
  return {bytes.begin(), bytes.end()};
}

std::string SaveOptions(const reb::battle::BattleOptions& options, reb::battle::Side playerSide)
{
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  out << static_cast<int>(playerSide) << ' ' << options.aiControlled[0] << ' ' << options.aiControlled[1] << ' '
      << options.humanSideAiWithdrawThreshold << ' ' << options.tickLimit << ' ' << options.attackerBearing;
  for (const auto& commander : options.commanders) {
    out << ' ' << static_cast<int>(commander.temperament) << ' ' << static_cast<int>(commander.difficulty);
  }
  return out.str();
}

bool LoadOptions(const std::string& text, reb::battle::BattleOptions& options, reb::battle::Side& playerSide)
{
  std::istringstream in(text);
  in.imbue(std::locale::classic());
  int side = 0;
  if (!(in >> side >> options.aiControlled[0] >> options.aiControlled[1] >> options.humanSideAiWithdrawThreshold
           >> options.tickLimit >> options.attackerBearing)
      || side < 0 || side > 1 || !std::isfinite(options.humanSideAiWithdrawThreshold)
      || options.humanSideAiWithdrawThreshold < 0 || options.tickLimit <= 0 || !std::isfinite(options.attackerBearing)) {
    return false;
  }
  playerSide = static_cast<reb::battle::Side>(side);
  for (auto& commander : options.commanders) {
    int temperament = 0;
    int difficulty = 0;
    if (!(in >> temperament >> difficulty) || temperament < 0 || temperament > 2 || difficulty < 0 || difficulty > 2) {
      return false;
    }
    commander.temperament = static_cast<reb::battle::Temperament>(temperament);
    commander.difficulty = static_cast<reb::battle::CommanderDifficulty>(difficulty);
  }
  in >> std::ws;
  return in.eof();
}

reb::native::GameContext* currentGameContext()
{
  auto* application = static_cast<RebellionApplication*>(g_app);
  return application ? &application->m_context : nullptr;
}

} // namespace

// ---- hook callbacks ---------------------------------------------------------------------------------------------

void RouteNoticeToSession(HookArg notice)
{
  GameSession* session = GetGameSession();
  if (session) {
    session->RouteNotice(reinterpret_cast<NotifPayload*>(notice));
  }
}

const char* GetStepDriverDataDirectory()
{
  GameStepDriver* driver = GetGameStepDriver();
  return driver ? driver->m_dataLocation : nullptr;
}

// ---- lifetime ---------------------------------------------------------------------------------------------------

BattleFlow::BattleFlow()
    : m_ok(1)
    , m_state(1)
{
  m_voteTimeA.m_value = 0;
  m_voteTimeB.m_value = 0;
}

BattleFlow::~BattleFlow()
{
  m_state = 0;
  m_ok = 1;
  m_liveBattle.reset();
  m_battleEngine.reset();
  DestroyStepDriver();
}

BattleFlow* GetBattleFlow()
{
  reb::native::GameContext* context = currentGameContext();
  return context ? context->GetBattleFlow() : nullptr;
}

GameStepDriver* GetGameStepDriver()
{
  BattleFlow* flow = GetBattleFlow();
  return flow ? flow->StepDriver() : nullptr;
}

void CreateBattleFlow()
{
  reb::native::GameContext* context = currentGameContext();
  if (!context) {
    return;
  }
  context->SetBattleFlow(nullptr);
  std::unique_ptr<BattleFlow> flow(NewOrNull<BattleFlow>());
  if (flow) {
    g_gameHook2AE0 = RouteNoticeToSession;
    g_gameHook2AE8 = GetStepDriverDataDirectory;
    InstallGameManagerHooks();
  }
  if (flow && flow->m_ok == 0) {
    flow.reset();
  }
  context->SetBattleFlow(std::move(flow));
}

void DestroyBattleFlow()
{
  if (reb::native::GameContext* context = currentGameContext()) {
    context->SetBattleFlow(nullptr);
  }
}

int BattleFlow::StartGame(GameSettings* settings, int a, int b)
{
  m_ok = 1;
  m_state = 0;
  m_liveBattle.reset();
  m_battleEngine.reset();
  DestroyStepDriver();
  int result = 0;
  if (settings->m_field04 >= kMinGalaxySize && settings->m_field04 <= kMaxGalaxySize) {
    result = CreateStepDriverFor(settings, a, b);
    if (result != 0) {
      m_state = 1;
    }
  }
  return result;
}

int BattleFlow::GetAutosaveFlag()
{
  int flag = 0;
  if (m_state == 2 && m_stepDriver) {
    flag = m_stepDriver->m_unk1c;
  }
  return flag;
}

void BattleFlow::SetAutosaveFlag(int value)
{
  if (m_state == 2 && m_stepDriver) {
    m_stepDriver->m_unk1c = value;
  }
}

// ---- save and restore -------------------------------------------------------------------------------------------

void BattleFlow::Restore(istream* in)
{
  m_ok = 0;
  m_state = 0;
  m_liveBattle.reset();
  m_battleEngine.reset();
  DestroyStepDriver();
  if (!CheckPositionMarker(in)) {
    return;
  }
  ReadValue32(in, &m_ok);
  ReadValue32(in, &m_state);
  ReadGameTime(&m_voteTimeA, in);
  ReadGameTime(&m_voteTimeB, in);
  const bool tactical = m_state == kModernTacticalSaveState;
  if (!m_ok || (m_state != kFlowStrategic && !tactical) || iostream_access::State(in) != 0) {
    m_ok = 0;
    return;
  }
  m_ok = CreateStepDriver() && CheckPositionMarker(in) && m_stepDriver && m_stepDriver->Load(in)
      && CheckPositionMarker(in) && AppLoadStrategicState(in) && CheckPositionMarker(in)
      && iostream_access::State(in) == 0;
  if (!m_ok || !tactical) {
    return;
  }
  m_ok = 0;
  uint32_t magic = 0;
  uint32_t version = 0;
  ReadValue32(in, &magic);
  ReadValue32(in, &version);
  std::vector<uint8_t> snapshotText;
  std::vector<uint8_t> checkpoint;
  std::vector<uint8_t> commands;
  std::vector<uint8_t> uiText;
  std::vector<uint8_t> optionsText;
  if (magic != kTacticalSaveMagic || version != kTacticalSaveVersion || !ReadBlob(in, snapshotText)
      || !ReadBlob(in, checkpoint) || !ReadBlob(in, commands) || !ReadBlob(in, uiText)
      || !ReadBlob(in, optionsText) || !CheckPositionMarker(in)) {
    return;
  }
  const auto snapshot = reb::battle::ParseSnapshot(AsText(snapshotText));
  reb::battle::BattleOptions options;
  reb::battle::Side playerSide;
  if (!snapshot || !reb::battle::DecodeCommands(commands) || !LoadOptions(AsText(optionsText), options, playerSide)) {
    return;
  }
  m_battleEngine = CreateSpaceBattleEngine();
  if (!m_battleEngine || !m_battleEngine->RestoreLiveBattle(*snapshot, checkpoint, options)) {
    m_battleEngine.reset();
    return;
  }
  auto* battle = m_battleEngine->LiveBattle();
  if (!battle || reb::battle::EncodeCommands(battle->CommandLog()) != commands) {
    m_battleEngine.reset();
    return;
  }
  m_liveBattle = std::make_unique<LiveBattleSession>(*battle, playerSide, true);
  if (!m_liveBattle->Screen().RestoreUiState(AsText(uiText))) {
    m_liveBattle.reset();
    m_battleEngine.reset();
    return;
  }
  m_state = kFlowTactical;
  if (LiveBattlePresenterAvailable()) {
    AppSetState(kStateTactical);
    m_ok = m_liveBattle->Open() && iostream_access::State(in) == 0;
  } else {
    m_liveBattle->GetImmediateResult();
    m_ok = iostream_access::State(in) == 0;
  }
}

int BattleFlow::Save(ostream* out)
{
  const bool tactical = m_state == kFlowTactical && m_liveBattle && m_battleEngine
      && m_battleEngine->LiveBattle();
  if (!m_ok || (m_state != kFlowStrategic && !tactical)) {
    return 0;
  }
  WritePositionMarker(out);
  WriteValue32(out, &m_ok);
  const int state = tactical ? kModernTacticalSaveState : m_state;
  WriteValue32(out, &state);
  StreamWrite32(out, &m_voteTimeA);
  StreamWrite32(out, &m_voteTimeB);
  int ok = SaveStrategic(out);
  if (ok && tactical) {
    const auto& battle = *m_battleEngine->LiveBattle();
    WriteValue32(out, &kTacticalSaveMagic);
    WriteValue32(out, &kTacticalSaveVersion);
    ok = WriteText(out, reb::battle::SerializeSnapshot(battle.Snapshot()))
        && WriteBlob(out, battle.SaveState()) && WriteBlob(out, reb::battle::EncodeCommands(battle.CommandLog()))
        && WriteText(out, m_liveBattle->Screen().SaveUiState())
        && WriteText(out, SaveOptions(battle.Options(), m_liveBattle->Screen().PlayerSide()));
    WritePositionMarker(out);
  }
  return ok && iostream_access::State(out) == 0;
}

int BattleFlow::SaveStrategic(ostream* out)
{
  WritePositionMarker(out);
  const bool saved = m_stepDriver && m_stepDriver->Save(out);
  WritePositionMarker(out);
  const bool windowSaved = AppSaveStrategicState(out);
  WritePositionMarker(out);
  return saved && (windowSaved || !m_liveBattle) && iostream_access::State(out) == 0;
}

RandomGenerator g_sessionRandom{};
