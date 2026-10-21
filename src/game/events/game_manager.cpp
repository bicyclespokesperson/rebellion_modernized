#include "events/game_manager.h"
#include "events/game_step_driver.h"

#include <new>

#include "core/checked_new.h"
#include "data/probability_table.h"
#include "data/table_registration.h"
#include "economy/side.h"
#include "events/event_base.h"
#include "events/game_manager_state.h"
#include "events/notif_event_payloads.h"
#include "gameobj/factory_registries.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "orders/order_base.h"

extern int g_gameFactoriesRegistered; // 0x6b2fd0

namespace
{

constexpr uint32_t kBattlePendingFlag = 0x10000000;
constexpr uint32_t kUnitFamilyFirst = 0x90;
constexpr uint32_t kUnitFamilyEnd = 0x98;
constexpr int kOrderKindSingleTeam = 0x100;
constexpr int kOrderKindNeedsNoUnit = 0x103;
constexpr uint32_t kAllianceTickKey = 0xf300000f;
constexpr uint32_t kEmpireTickKey = 0xf3000010;

} // namespace

Crc32* GameStateSnapshot::CopyStateCrc(Crc32* out)
{
  new (out) Crc32(m_stateCrc);
  return out;
}

Crc32* GameStateSnapshot::CopyTableCrc(Crc32* out)
{
  new (out) Crc32(m_tableCrc);
  return out;
}

int GameStateSnapshot::Read(istream* in)
{
  ChecksumRecord record;
  const int result = record.Read(in);
  m_tick = record.m_value00;
  m_stamp = record.m_value04;
  m_unk08 = record.m_value08;
  m_stateCrc = record.m_crcA;
  m_tableCrc = record.m_crcB;
  return result;
}

int GameStateSnapshot::Write(ostream* out)
{
  ChecksumRecord record;
  record.m_value00 = m_tick;
  record.m_value04 = m_stamp;
  record.m_value08 = m_unk08;
  record.m_crcA = m_stateCrc;
  record.m_crcB = m_tableCrc;
  return record.Write(out);
}

GameManager::GameManager()
{
  InitState();
}

GameManager::~GameManager()
{
  ReleaseMembers();
}

GameManager* GetGameManager()
{
  GameStepDriver* driver = GetGameStepDriver();
  return driver ? driver->Manager() : nullptr;
}

void CreateGameManager()
{
  if (GameStepDriver* driver = GetGameStepDriver()) {
    driver->CreateManager();
  }
}

void DestroyGameManager()
{
  if (GameStepDriver* driver = GetGameStepDriver()) {
    driver->DestroyManager();
  }
}

int GameManager::InitState()
{
  m_ok = 1;
  m_unk08 = 1;
  m_tickCount = 1;
  m_day = 1;
  m_serialA = 0;
  m_serialB = 0;
  m_ticksPerDay = 1;
  m_tickOfDay = 0;
  m_dayEnded = 0;
  m_lastBattleTick = 0;
  m_setupDone = 0;
  m_stateDiffers = 0;
  m_unk34 = 0;
  m_unk38 = 0;
  m_unk3c = 0;
  m_unk40 = 0;
  m_unk44 = 0;
  m_firstSnapshotTick = 0;
  m_haveSideSnapshots = 0;
  m_notices = nullptr;
  m_timers = nullptr;
  m_commands = nullptr;
  m_events = nullptr;
  m_firstQueue = nullptr;
  m_secondQueue = nullptr;
  m_tickTable = nullptr;
  m_root = nullptr;

  m_notices = NewOrNull<MessageTree>();
  if (!m_notices || m_notices->m_unk0c == 0) {
    m_ok = 0;
  }

  m_timers = NewOrNull<TimerQueue>();
  if (!m_timers || m_timers->m_flag0c == 0) {
    m_ok = 0;
  }

  m_commands = NewOrNull<CommandQueue>();
  if (!m_commands || m_commands->m_flag0c == 0) {
    m_ok = 0;
  }

  m_events = NewOrNull<TypeQueue>();
  if (!m_events || m_events->m_flag0c == 0) {
    m_ok = 0;
  }

  m_firstQueue = NewOrNull<PostedEventQueue>();
  if (!m_firstQueue || m_firstQueue->m_flag == 0) {
    m_ok = 0;
  }

  m_secondQueue = NewOrNull<PostedEventQueue>();
  if (!m_secondQueue || m_secondQueue->m_flag == 0) {
    m_ok = 0;
  }

  m_tickTable = NewOrNull<TickTable>();
  if (!m_tickTable || m_tickTable->m_ready == 0) {
    m_ok = 0;
  }
  return m_ok;
}

int GameManager::ReleaseMembers()
{
  delete m_notices;
  m_notices = nullptr;
  delete m_timers;
  m_timers = nullptr;
  delete m_commands;
  m_commands = nullptr;
  delete m_events;
  m_events = nullptr;
  delete m_firstQueue;
  m_firstQueue = nullptr;
  delete m_secondQueue;
  m_secondQueue = nullptr;
  delete m_tickTable;
  m_tickTable = nullptr;
  delete m_root;
  m_root = nullptr;
  return 1;
}

namespace
{

int readFlagsAndSnapshots(GameManager* self, istream* in)
{
  int* fields = &self->m_ok;
  for (int i = 0; i < 19; ++i) {
    ReadValue32(in, fields + i);
  }
  int ok = 1;
  for (int i = 0; i < 3 && ok; ++i) {
    ok = self->m_snapshot[i].Read(in);
  }
  return ok;
}

} // namespace

int GameManager::Read(istream* in)
{
  int ok = ReleaseMembers();
  if (ok) {
    ok = InitState();
  }
  if (ok) {
    ok = CheckPositionMarker(in);
  }
  if (ok) {
    ReadLegacyTexts(in);
    ok = readFlagsAndSnapshots(this, in);
    if (ok) {
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      m_notices->Read(in);
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      m_timers->Read(in);
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      m_commands->Read(in);
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      m_events->Read(in);
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      ok = m_firstQueue->Read(in);
      if (ok) {
        ok = CheckPositionMarker(in);
      }
    }
    if (ok) {
      ok = m_secondQueue->Read(in);
      if (ok) {
        ok = CheckPositionMarker(in);
      }
    }
    if (ok) {
      m_tickTable->Read(in);
      ok = CheckPositionMarker(in);
    }
    if (ok) {
      ok = GameRoot::LoadGame(in, &m_root);
    }
  }

  int hasCurrentFormat = 1;
  if (ok) {
    ReadValue32(in, &hasCurrentFormat);
    if (hasCurrentFormat == 0) {
      LegacyTextTable legacy;
      for (int i = 0; i < 6; ++i) {
        legacy.Read(in);
      }
      ok = CheckPositionMarker(in);
    }
    ok = ok ? ReadLegacyBlocks(in) : 0;
  }

  m_stateDiffers = 0;
  m_unk34 = 0;
  m_unk38 = 0;
  m_unk3c = 0;
  m_unk40 = 0;
  m_unk44 = 0;
  m_firstSnapshotTick = 0;
  m_haveSideSnapshots = 0;
  m_snapshot[0] = GameStateSnapshot();
  m_snapshot[1] = GameStateSnapshot();
  m_snapshot[2] = GameStateSnapshot();
  return ok;
}

int GameManager::Write(ostream* out)
{
  WritePositionMarker(out);
  WriteLegacyTexts(out);
  const int* fields = &m_ok;
  for (int i = 0; i < 19; ++i) {
    WriteValue32(out, fields + i);
  }

  int ok = m_snapshot[0].Write(out);
  if (ok) {
    ok = m_snapshot[1].Write(out);
  }
  if (ok) {
    ok = m_snapshot[2].Write(out);
  }
  if (ok) {
    WritePositionMarker(out);
  }
  if (ok) {
    ok = m_notices != nullptr;
  }
  if (ok) {
    m_notices->Write(out);
    WritePositionMarker(out);
  }
  if (ok) {
    ok = m_timers ? m_timers->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_commands ? m_commands->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_events ? m_events->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_firstQueue ? m_firstQueue->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_secondQueue ? m_secondQueue->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_tickTable ? m_tickTable->Write(out) : 0;
    if (ok) {
      WritePositionMarker(out);
    }
  }
  if (ok) {
    ok = m_root ? GameRoot::SaveGame(out, m_root) : 0;
  }

  int currentFormat = 1;
  if (ok) {
    WriteValue32(out, &currentFormat);
    if (ok) {
      ok = WriteLegacyBlocks(out);
    }
  }
  return ok;
}

int GameManager::Checksum(Crc32* crc)
{
  const int* fields = &m_ok;
  for (int i = 0; i < 11; ++i) {
    crc->AddInt(fields[i]);
  }
  if (m_timers) {
    m_timers->Checksum(crc);
  }
  if (m_commands) {
    m_commands->Checksum(crc);
  }
  if (m_events) {
    m_events->Checksum(crc);
  }
  int result = 1;
  if (m_root) {
    result = GameRoot::ChecksumGame(crc, m_root);
  }
  return result;
}

void GameManager::BeginNewGame(GameSettings* settings)
{
  ResetLegacyTexts();
  m_setupDone = 0;
  delete m_root;
  m_root = nullptr;
  m_root = NewOrNull<GameRoot>();
  if (m_root) {
    m_root->SetSettings(*settings);
    m_root->BeginSetup();
  }
}

int GameManager::GetSettings(GameSettings* out)
{
  if (!m_root) {
    GameSettings defaults;
    *out = defaults;
  } else {
    *out = m_root->m_settings;
  }
  return 1;
}

int GameManager::StepSetup(int* status)
{
  *status = 1;
  const int done = m_root->StepSetup(status);
  if (done) {
    m_setupDone = 1;
  }
  return done;
}

int GameManager::SetSnapshot(GameStateSnapshot* snapshot, int which)
{
  const int ok = which >= 1 && which <= 2;
  if (ok) {
    if (which == 1) {
      m_snapshot[1] = *snapshot;
      return ok;
    }
    if (which == 2) {
      m_snapshot[2] = *snapshot;
    }
  }
  return ok;
}

void GameManager::AddNotice(MessageBase* notice)
{
  if (notice && m_notices) {
    m_notices->Adopt(notice);
  }
}

void GameManager::ScheduleTimer(TimedEvent* timer, int delay)
{
  if (timer) {
    timer->m_dueDay = m_day + delay;
    if (m_timers) {
      m_timers->Add(timer);
    }
  }
}

void GameManager::ScheduleCommand(EventBase* event)
{
  if (event && m_commands) {
    m_commands->Add(event);
  }
}

void GameManager::ScheduleEvent(EventBase* event)
{
  if (event && m_events) {
    m_events->Add(event);
  }
}

int GameManager::DispatchPostedEvent(NotifPayload* event)
{
  if (event) {
    switch (event->GetDeliveryMode()) {
      case 1:
        return HandleKind1(event);
      case 2:
        return HandleKind2(event);
      case 3:
        return HandleKind3(event);
      case 4:
        return HandleKind4(event);
    }
  }
  return 0;
}

int GameManager::BuildAvailableOrders(int side, ObjectKeyValueSet* team, EventKindSet* orders)
{
  orders->Clear();
  EventKindSet perObject;
  EventKindSet common;

  int ok = 1;
  for (auto nodePosition = team->Entries().begin(); nodePosition != team->Entries().end();) {
    auto* node = (*(nodePosition++)).get();
    if (!(true && ok)) {
      break;
    }

    ObjectKey key;
    static_cast<ObjectKeyNode*>(node)->GetKey(&key);
    auto* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&key));
    ok = object != nullptr;
    if (ok) {
      perObject.Clear();
      ok = object->GetEventKinds(static_cast<ObjectKeyValueNode*>(node)->m_value, &perObject);
      if (node == team->First()) {
        common = perObject;
      } else {
        common.Intersect(perObject);
      }
    }
  }

  for (const auto& [kind, wanted] : common.Entries()) {
    if (!ok) {
      return 0;
    }
    if (wanted.m_value == 0) {
      break;
    }
    auto* order = static_cast<OrderBase*>(CreateMessageByKind(wanted.m_key));
    ok = order != nullptr;
    if (ok) {
      order->m_side = side;
      order->SetTeam(team);
      ObjectKey noTarget;
      order->SetTarget(&noTarget);
      if (order->IsListed()) {
        if (EventKindNode* entry = orders->Add(wanted.m_key, 1)) {
          StatusCode status;
          const int enabled = !HasPendingBattle() && order->CheckEnabled(&status);
          entry->m_value = enabled ? 1 : 0;
          entry->m_unknown = order->IsChecked();
        }
      }
    }
    delete order;
  }

  if (!ok) {
    return 0;
  }
  EventKindNode* single = orders->Add(kOrderKindSingleTeam, 1);
  ok = single != nullptr;
  if (!ok) {
    return 0;
  }
  single->m_value = team->Count() == 1;
  EventKindNode* noUnit = orders->Add(kOrderKindNeedsNoUnit, 1);
  ok = noUnit != nullptr;
  if (ok) {
    noUnit->m_value = 0;
    if (team->Count() == 1) {
      ObjectKey key;
      static_cast<ObjectKeyNode*>(team->First())->GetKey(&key);
      const FamilyRange units(kUnitFamilyFirst, kUnitFamilyEnd);
      const uint32_t family = key.m_value >> 24;
      const bool isUnit = family >= units.m_first && family < units.m_second;
      if (!isUnit) {
        noUnit->m_value = 1;
      }
    }
  }
  return ok;
}

int GameManager::GetRootField70()
{
  return m_root ? m_root->m_unk70 : 0;
}

int GameManager::GetRootKey(ObjectKey* out)
{
  return m_root ? m_root->GetRootKey(out) : 0;
}

int GameManager::QueryRootBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d)
{
  return m_root ? m_root->QueryRetreat(side, a, b, c, d) : 0;
}

int GameManager::ExecuteNotice(SidedMessage* notice)
{
  if (notice) {
    StatusCode status(1, 0xffffffffu);
    notice->Execute(&status);
    NoteDispatch(notice);
    delete notice;
  }
  return notice != nullptr;
}

int GameManager::StartPendingBattle()
{
  int result = 0;
  if (m_root) {
    result = m_root->StartPendingBattle();
  }
  m_lastBattleTick = m_tickCount;
  return result;
}

int GameManager::RunFollowUps()
{
  if (!m_root) {
    return 0;
  }
  int ok = m_root->PrepareBattle() != 0;
  ok = RunUntilIdle() && ok;
  ok = m_root->ResolveBattle() && ok;
  ok = RunUntilIdle() && ok;
  return EndOfStep() && ok;
}

int GameManager::HasPendingBattle()
{
  if (!m_root) {
    return 0;
  }
  return static_cast<uint32_t>(m_root->m_unk74) & kBattlePendingFlag;
}

int GameManager::AdvanceTick()
{
  if (!HasPendingBattle()) {
    ++m_tickCount;
    if (m_dayEnded == 0) {
      ++m_tickOfDay;
    } else {
      m_dayEnded = 0;
      m_tickOfDay = 0;
      ++m_day;
    }
    Side* alliance = static_cast<Side*>(GetGameRootSideMemberThunk(1));
    Side* empire = static_cast<Side*>(GetGameRootSideMemberThunk(2));
    if (alliance && empire) {
      if (static_cast<uint32_t>(empire->m_unkC4) < static_cast<uint32_t>(alliance->m_unkC4)) {
        m_ticksPerDay = alliance->m_unkC4;
      } else {
        m_ticksPerDay = empire->m_unkC4;
      }
    }
    m_dayEnded = m_ticksPerDay <= m_tickOfDay;
  }
  return 1;
}

int GameManager::RunScheduler()
{
  const bool ready = m_timers && m_commands && m_events && m_notices;
  if (!ready) {
    return 1;
  }

  int ran = 0;
  if (EventBase* event = m_events->GetNext()) {
    NoteDispatch(event);
    ran = m_events->RunNext();
  }
  if (ran == 0) {
    if (EventBase* timer = m_timers->GetNext()) {
      NoteDispatch(timer);
      ran = m_timers->RunNext();
    }
    if (ran == 0) {
      if (EventBase* command = m_commands->GetNext()) {
        NoteDispatch(command);
        ran = m_commands->RunNext();
      }
    }
  }

  int result = 0;
  if (ran == 0) {
    auto* notice = static_cast<SidedMessage*>(m_notices->First());
    if (!notice) {
      result = 1;
    } else {
      m_notices->Detach(notice->m_id);
      ExecuteNotice(notice);
    }
  }
  if (FlushFirstQueue() == 0) {
    return 1;
  }
  return result;
}

int GameManager::RunUntilIdle()
{
  while (RunScheduler() == 0) {
  }
  return 1;
}

int GameManager::EndOfStep()
{
  if (m_firstSnapshotTick != 0) {
    const int tickA = m_snapshot[1].m_tick;
    const int tickB = m_snapshot[2].m_tick;
    Crc32 stateA(m_snapshot[1].m_stateCrc);
    Crc32 stateB(m_snapshot[2].m_stateCrc);
    const uint32_t stateValueA = stateA.Value();
    const uint32_t stateValueB = stateB.Value();
    Crc32 tableA(m_snapshot[1].m_tableCrc);
    Crc32 tableB(m_snapshot[2].m_tableCrc);
    const uint32_t tableValueB = tableB.Value();
    const uint32_t tableValueA = tableA.Value();

    m_stateDiffers = !(stateValueA == stateValueB && tickA == tickB);
    if (m_haveSideSnapshots == 0) {
      m_unk38 = tableValueA != tableValueB;
    } else {
      m_unk40 = tableValueA != tableValueB;
    }
    m_haveSideSnapshots = 1;
  }
  if (PublishVictoryState() && FlushFirstQueue() && m_tickTable) {
    m_tickTable->Clear();
    FlushSecondQueue();
  }
  return 1;
}

int GameManager::CaptureSnapshot()
{
  const uint32_t tick = m_tickCount;
  m_snapshot[0].m_tick = tick;
  if (m_root->m_unk70 != 0 || tick == static_cast<uint32_t>(m_lastBattleTick) + 1 || tick % 0x28 != 0
      || m_firstSnapshotTick == 0) {
    m_snapshot[0].m_stamp = tick;
    m_snapshot[0].m_unk08 = m_unk08;
    Crc32 state;
    Checksum(&state);
    m_snapshot[0].m_stateCrc = state;
    Crc32 tables;
    ForEachDataFamily(&tables);
    ChecksumTables(&tables);
    m_snapshot[0].m_tableCrc = tables;
    if (m_firstSnapshotTick == 0) {
      m_firstSnapshotTick = m_tickCount;
    }
  }
  return 1;
}

int GameManager::NoteDispatch(void* item)
{
  return 1;
}

int GameManager::HandleKind1(NotifPayload* event)
{
  if (event->GetDeliveryMode() == 1) {
    event->m_value0c = 0;
    return PostToFirstQueue(event);
  }
  return 0;
}

namespace
{

SeedKey makeTickKey(NotifPayload* event, int kind, int second)
{
  SeedKey key;
  key.m_key = kind;
  key.m_second = second;
  key.m_third = event->m_value18;
  key.m_fourth = event->m_value10;
  key.m_fifth = event->m_value14;
  return key;
}

} // namespace

int GameManager::HandleKind2(NotifPayload* event)
{
  if (event->GetDeliveryMode() != 2) {
    return 0;
  }
  const int kind = event->GetDeliveryMode();
  SeedKey key = makeTickKey(event, kind, event->GetKind());
  int value;
  int ok = ResolveTickValue(&key, &value);
  if (ok) {
    event->m_value0c = value;
    ok = PostToFirstQueue(event);
  }
  return ok;
}

int GameManager::HandleKind3(NotifPayload* event)
{
  if (event->GetDeliveryMode() != 3) {
    return 0;
  }
  const int kind = event->GetDeliveryMode();
  SeedKey key = makeTickKey(event, kind, event->GetKind());
  int value;
  int ok = ResolveTickValue(&key, &value);
  if (ok) {
    event->m_value0c = value;
    ok = PostToFirstQueue(event);
  }
  return ok;
}

int GameManager::HandleKind4(NotifPayload* event)
{
  if (event->GetDeliveryMode() != 4) {
    return 0;
  }
  SeedKey key = makeTickKey(event, event->GetDeliveryMode(), 0);
  int value;
  int ok = ResolveTickValue(&key, &value);
  if (ok) {
    event->m_value0c = value;
    ok = PostToFirstQueue(event);
  }
  return ok;
}

int GameManager::ResolveTickValue(SeedKey* key, int* out)
{
  *out = 0;
  int ok = m_tickTable != nullptr;
  if (ok) {
    SeedKeyCountNode* node = nullptr;
    if (key->m_key != 2) {
      node = m_tickTable->Find(key);
    }
    if (!node) {
      node = m_tickTable->Create(key);
      if (!node) {
        return 0;
      }
      const int value = CallGameHook2AF4();
      node->m_count = value;
      ok = PostTickValue(node->m_key.m_third, value, node->m_key.m_fifth);
    }
    if (ok) {
      *out = node->m_count;
    }
  }
  return ok;
}

int GameManager::PostTickValue(int a, int b, int c)
{
  auto* event = static_cast<NotifPayload*>(CreateKeyedObject(2));
  int ok = 0;
  if (event) {
    event->m_value18 = a;
    event->m_value0c = b;
    event->m_value14 = c;
    const ObjectKey key = ObjectKey::Make(a == 1 ? kAllianceTickKey >> 24 : kEmpireTickKey >> 24,
                                          a == 1 ? kAllianceTickKey & 0xffffff : kEmpireTickKey & 0xffffff);
    static_cast<KeyedNotifPayload*>(event)->m_key = key;
    ok = m_secondQueue != nullptr;
    if (ok) {
      event->PrepareForDispatch();
      m_secondQueue->Enqueue(std::unique_ptr<NotifPayload>(event));
      NoteDispatch(event);
    }
  }
  return ok;
}

int GameManager::PostToFirstQueue(NotifPayload* event)
{
  int ok = 0;
  if (event) {
    ok = m_firstQueue != nullptr;
    if (ok) {
      event->PrepareForDispatch();
      m_firstQueue->Enqueue(std::unique_ptr<NotifPayload>(event));
      NoteDispatch(event);
    }
  }
  return ok;
}

int GameManager::FlushFirstQueue()
{
  const int ok = m_firstQueue != nullptr;
  if (ok) {
    while (auto payload = m_firstQueue->Dequeue()) {
      CallGameHook2AE0(reinterpret_cast<HookArg>(payload.release()));
    }
  }
  return ok;
}

int GameManager::FlushSecondQueue()
{
  const int ok = m_secondQueue != nullptr;
  if (ok) {
    while (auto payload = m_secondQueue->Dequeue()) {
      CallGameHook2AE0(reinterpret_cast<HookArg>(payload.release()));
    }
  }
  return ok;
}

void RegisterAllFactoriesOnce()
{
  if (g_gameFactoriesRegistered == 0) {
    EnsureRecordFactoriesRegistered();
    EnsureTableEntryClassesRegistered();
    RegisterGameObjFactories();
    RegisterEventFactories();
    RegisterOrderFactories();
    g_gameFactoriesRegistered = 1;
  }
}

LegacyBlockA g_legacyBlockA{};
LegacyBlockB g_legacyBlockB[3]{};
std::string g_legacyTexts[10]{};
int g_gameFactoriesRegistered{};
int g_legacyTextCount{};
int g_legacyTextExtra{};
