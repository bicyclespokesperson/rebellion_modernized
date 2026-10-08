#include "events/game_manager.h"

#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "economy/side.h"
#include "events/event_base.h"
#include "data/probability_table.h"
#include "data/table_registration.h"
#include "gameobj/factory_registries.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "orders/order_base.h"

extern int g_gameFactoriesRegistered;  // 0x6b2fd0

namespace {

constexpr uint32_t kBattlePendingFlag = 0x10000000;
constexpr uint32_t kUnitFamilyFirst = 0x90;
constexpr uint32_t kUnitFamilyEnd = 0x98;
constexpr int kOrderKindSingleTeam = 0x100;
constexpr int kOrderKindNeedsNoUnit = 0x103;
constexpr uint32_t kAllianceTickKey = 0xf300000f;
constexpr uint32_t kEmpireTickKey = 0xf3000010;

}  // namespace

Crc32* GameStateSnapshot::CopyStateCrc(Crc32* out) {
    new (out) Crc32(m_stateCrc);
    return out;
}

Crc32* GameStateSnapshot::CopyTableCrc(Crc32* out) {
    new (out) Crc32(m_tableCrc);
    return out;
}

GameManager::GameManager() { InitState(); }

GameManager::~GameManager() {
    REB_KEEP_VPTR_STORE();
    ReleaseMembers();
}

GameManager* GetGameManager() { return g_gameManager; }

void CreateGameManager() {
    DestroyGameManager();
    g_gameManager = NewOrNull<GameManager>();
    if (!g_gameManager || !g_gameManager->m_ok) DestroyGameManager();
}

void DestroyGameManager() {
    if (g_gameManager) {
        delete g_gameManager;
        g_gameManager = nullptr;
    }
}

int GameManager::InitState() {
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
    if (!m_notices || m_notices->m_unk0c == 0) m_ok = 0;

    m_timers = NewOrNull<TimerQueue>();
    if (!m_timers || m_timers->m_flag0c == 0) m_ok = 0;

    m_commands = NewOrNull<CommandQueue>();
    if (!m_commands || m_commands->m_flag0c == 0) m_ok = 0;

    m_events = NewOrNull<TypeQueue>();
    if (!m_events || m_events->m_flag0c == 0) m_ok = 0;

    void* memory = operator new(sizeof(PostedEventQueue));
    m_firstQueue = memory ? FUN_00568840(memory) : nullptr;
    if (!m_firstQueue || m_firstQueue->m_flag == 0) m_ok = 0;

    memory = operator new(sizeof(PostedEventQueue));
    m_secondQueue = memory ? FUN_00568840(memory) : nullptr;
    if (!m_secondQueue || m_secondQueue->m_flag == 0) m_ok = 0;

    memory = operator new(sizeof(TickTable));
    m_tickTable = memory ? FUN_00568b40(memory) : nullptr;
    if (!m_tickTable || *reinterpret_cast<int*>(reinterpret_cast<char*>(m_tickTable) + 8) == 0) m_ok = 0;
    return m_ok;
}

int GameManager::ReleaseMembers() {
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

namespace {

int readFlagsAndSnapshots(GameManager* self, istream* in) {
    int* fields = &self->m_ok;
    for (int i = 0; i < 19; ++i) ReadValue32(in, fields + i);
    int ok = 1;
    for (int i = 0; i < 3 && ok; ++i) ok = FUN_00539960(&self->m_snapshot[i], in);
    return ok;
}

}  // namespace

int GameManager::Read(istream* in) {
    int ok = ReleaseMembers();
    if (ok) ok = InitState();
    if (ok) ok = CheckPositionMarker(in);
    if (ok) {
        FUN_00568a30(in);
        ok = readFlagsAndSnapshots(this, in);
        if (ok) ok = CheckPositionMarker(in);
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
            ok = FUN_005688d0(m_firstQueue, in);
            if (ok) ok = CheckPositionMarker(in);
        }
        if (ok) {
            ok = FUN_005688d0(m_secondQueue, in);
            if (ok) ok = CheckPositionMarker(in);
        }
        if (ok) {
            m_tickTable->Read(in);
            ok = CheckPositionMarker(in);
        }
        if (ok) ok = GameRoot::LoadGame(in, &m_root);
    }

    int hasCurrentFormat = 1;
    if (ok) {
        ReadValue32(in, &hasCurrentFormat);
        if (hasCurrentFormat == 0) {
            LegacySaveBlock legacy;
            FUN_005686a0(&legacy);
            for (int i = 0; i < 6; ++i) FUN_00568770(&legacy, in);
            FUN_005686e0(&legacy);
            ok = CheckPositionMarker(in);
        }
        ok = ok ? FUN_00568560(in) : 0;
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

int GameManager::Write(ostream* out) {
    WritePositionMarker(out);
    FUN_00568a80(out);
    const int* fields = &m_ok;
    for (int i = 0; i < 19; ++i) WriteValue32(out, fields + i);

    int ok = FUN_00539970(&m_snapshot[0], out);
    if (ok) ok = FUN_00539970(&m_snapshot[1], out);
    if (ok) ok = FUN_00539970(&m_snapshot[2], out);
    if (ok) WritePositionMarker(out);
    if (ok) ok = m_notices != nullptr;
    if (ok) {
        m_notices->Write(out);
        WritePositionMarker(out);
    }
    if (ok) {
        ok = m_timers ? m_timers->Write(out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) {
        ok = m_commands ? m_commands->Write(out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) {
        ok = m_events ? m_events->Write(out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) {
        ok = m_firstQueue ? FUN_00568980(m_firstQueue, out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) {
        ok = m_secondQueue ? FUN_00568980(m_secondQueue, out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) {
        ok = m_tickTable ? m_tickTable->Write(out) : 0;
        if (ok) WritePositionMarker(out);
    }
    if (ok) ok = m_root ? GameRoot::SaveGame(out, m_root) : 0;

    int currentFormat = 1;
    if (ok) {
        WriteValue32(out, &currentFormat);
        if (ok) ok = FUN_005685a0(out);
    }
    return ok;
}

int GameManager::Checksum(Crc32* crc) {
    const int* fields = &m_ok;
    for (int i = 0; i < 11; ++i) crc->AddInt(fields[i]);
    if (m_timers) m_timers->Checksum(crc);
    if (m_commands) m_commands->Checksum(crc);
    if (m_events) m_events->Checksum(crc);
    int result = 1;
    if (m_root) result = GameRoot::ChecksumGame(crc, m_root);
    return result;
}

void GameManager::BeginNewGame(GameSettings* settings) {
    FUN_00568a00();
    m_setupDone = 0;
    delete m_root;
    m_root = nullptr;
    m_root = NewOrNull<GameRoot>();
    if (m_root) {
        m_root->SetSettings(*settings);
        m_root->BeginSetup();
    }
}

int GameManager::GetSettings(GameSettings* out) {
    if (!m_root) {
        GameSettings defaults;
        *out = defaults;
    } else {
        *out = m_root->m_settings;
    }
    return 1;
}

int GameManager::StepSetup(int* status) {
    *status = 1;
    const int done = m_root->StepSetup(status);
    if (done) m_setupDone = 1;
    return done;
}

int GameManager::SetSnapshot(GameStateSnapshot* snapshot, int which) {
    const int ok = which >= 1 && which <= 2;
    if (ok) {
        if (which == 1) {
            m_snapshot[1] = *snapshot;
            return ok;
        }
        if (which == 2) m_snapshot[2] = *snapshot;
    }
    return ok;
}

void GameManager::AddNotice(IdNode* notice) {
    if (notice && m_notices) m_notices->Adopt(notice);
}

void GameManager::ScheduleTimer(TimedEvent* timer, int delay) {
    if (timer) {
        timer->m_dueDay = m_day + delay;
        if (m_timers) m_timers->Add(timer);
    }
}

void GameManager::ScheduleCommand(EventBase* event) {
    if (event && m_commands) m_commands->Add(event);
}

void GameManager::ScheduleEvent(EventBase* event) {
    if (event && m_events) m_events->Add(event);
}

int GameManager::DispatchPostedEvent(PostedEvent* event) {
    if (event) {
        switch (event->GetKind()) {
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

int GameManager::BuildAvailableOrders(int side, ObjectKeyValueSet* team, EventKindSet* orders) {
    orders->AvlTree::Clear();
    EventKindSet perObject;
    EventKindSet common;

    int ok = 1;
    for (AvlNode* node = team->First(); node && ok; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        GameObj* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&key));
        ok = object != nullptr;
        if (ok) {
            perObject.AvlTree::Clear();
            ok = object->GetEventKinds(static_cast<ObjectKeyValueNode*>(node)->m_value, &perObject);
            if (node == team->First())
                common = perObject;
            else
                common.Intersect(perObject);
        }
    }

    for (AvlNode* kindNode = common.First(); kindNode; kindNode = kindNode->m_successor) {
        if (!ok) return 0;
        EventKindNode* wanted = static_cast<EventKindNode*>(kindNode);
        if (wanted->m_value == 0) break;
        OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(wanted->m_key));
        ok = order != nullptr;
        if (ok) {
            order->m_side = side;
            order->SetTeam(team);
            ObjectKey noTarget;
            order->SetTarget(&noTarget);
            if (order->IsListed()) {
                if (EventKindNode* entry = orders->Add(wanted->m_key, 1)) {
                    StatusCode status;
                    const int enabled = !HasPendingBattle() && order->CheckEnabled(&status);
                    entry->m_value = enabled ? 1 : 0;
                    entry->m_unknown = order->IsChecked();
                }
            }
        }
        delete order;
    }

    if (!ok) return 0;
    EventKindNode* single = orders->Add(kOrderKindSingleTeam, 1);
    ok = single != nullptr;
    if (!ok) return 0;
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
            if (!isUnit) noUnit->m_value = 1;
        }
    }
    return ok;
}

int GameManager::GetRootField70() { return m_root ? m_root->m_unk70 : 0; }

int GameManager::GetRootKey(ObjectKey* out) { return m_root ? FUN_00516710(m_root, out) : 0; }

int GameManager::QueryRootBattle(int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d) {
    return m_root ? FUN_00516730(m_root, side, a, b, c, d) : 0;
}

int GameManager::ExecuteNotice(SidedMessage* notice) {
    if (notice) {
        StatusCode status(1, 0xffffffffu);
        notice->Execute(&status);
        NoteDispatch(notice);
        delete notice;
    }
    return notice != nullptr;
}

int GameManager::StartPendingBattle() {
    int result = 0;
    if (m_root) result = FUN_00516d50(m_root);
    m_lastBattleTick = m_tickCount;
    return result;
}

int GameManager::RunFollowUps() {
    if (!m_root) return 0;
    int ok = FUN_00516ef0(m_root) != 0;
    ok = RunUntilIdle() && ok;
    ok = FUN_00517270(m_root) && ok;
    ok = RunUntilIdle() && ok;
    return EndOfStep() && ok;
}

int GameManager::HasPendingBattle() {
    if (!m_root) return 0;
    return static_cast<uint32_t>(m_root->m_unk74) & kBattlePendingFlag;
}

int GameManager::AdvanceTick() {
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
            if (static_cast<uint32_t>(empire->m_unkC4) < static_cast<uint32_t>(alliance->m_unkC4))
                m_ticksPerDay = alliance->m_unkC4;
            else
                m_ticksPerDay = empire->m_unkC4;
        }
        m_dayEnded = m_ticksPerDay <= m_tickOfDay;
    }
    return 1;
}

int GameManager::RunScheduler() {
    const bool ready = m_timers && m_commands && m_events && m_notices;
    if (!ready) return 1;

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
        SidedMessage* notice = static_cast<SidedMessage*>(m_notices->First());
        if (!notice) {
            result = 1;
        } else {
            m_notices->Detach(notice->m_id);
            ExecuteNotice(notice);
        }
    }
    if (FlushFirstQueue() == 0) return 1;
    return result;
}

int GameManager::RunUntilIdle() {
    while (RunScheduler() == 0) {
    }
    return 1;
}

int GameManager::EndOfStep() {
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
        if (m_haveSideSnapshots == 0)
            m_unk38 = tableValueA != tableValueB;
        else
            m_unk40 = tableValueA != tableValueB;
        m_haveSideSnapshots = 1;
    }
    if (FUN_00518460() && FlushFirstQueue() && m_tickTable) {
        m_tickTable->Clear();
        FlushSecondQueue();
    }
    return 1;
}

int GameManager::CaptureSnapshot() {
    const uint32_t tick = m_tickCount;
    m_snapshot[0].m_tick = tick;
    if (m_root->m_unk70 != 0 || tick == static_cast<uint32_t>(m_lastBattleTick) + 1 || tick % 0x28 != 0 ||
        m_firstSnapshotTick == 0) {
        m_snapshot[0].m_stamp = tick;
        m_snapshot[0].m_unk08 = m_unk08;
        Crc32 state;
        Checksum(&state);
        m_snapshot[0].m_stateCrc = state;
        Crc32 tables;
        FUN_0051cce0(&tables);
        ChecksumTables(&tables);
        m_snapshot[0].m_tableCrc = tables;
        if (m_firstSnapshotTick == 0) m_firstSnapshotTick = m_tickCount;
    }
    return 1;
}

int GameManager::NoteDispatch(void* item) { return 1; }

int GameManager::HandleKind1(PostedEvent* event) {
    if (event->GetKind() == 1) {
        event->m_result = 0;
        return PostToFirstQueue(event);
    }
    return 0;
}

namespace {

SeedKey makeTickKey(PostedEvent* event, int kind, int second) {
    SeedKey key;
    key.m_key = kind;
    key.m_second = second;
    key.m_third = event->m_field18;
    key.m_fourth = event->m_field10;
    key.m_fifth = event->m_field14;
    return key;
}

}  // namespace

int GameManager::HandleKind2(PostedEvent* event) {
    if (event->GetKind() != 2) return 0;
    const int kind = event->GetKind();
    SeedKey key = makeTickKey(event, kind, event->GetFactoryKey());
    int value;
    int ok = ResolveTickValue(&key, &value);
    if (ok) {
        event->m_result = value;
        ok = PostToFirstQueue(event);
    }
    return ok;
}

int GameManager::HandleKind3(PostedEvent* event) {
    if (event->GetKind() != 3) return 0;
    const int kind = event->GetKind();
    SeedKey key = makeTickKey(event, kind, event->GetFactoryKey());
    int value;
    int ok = ResolveTickValue(&key, &value);
    if (ok) {
        event->m_result = value;
        ok = PostToFirstQueue(event);
    }
    return ok;
}

int GameManager::HandleKind4(PostedEvent* event) {
    if (event->GetKind() != 4) return 0;
    SeedKey key = makeTickKey(event, event->GetKind(), 0);
    int value;
    int ok = ResolveTickValue(&key, &value);
    if (ok) {
        event->m_result = value;
        ok = PostToFirstQueue(event);
    }
    return ok;
}

int GameManager::ResolveTickValue(SeedKey* key, int* out) {
    *out = 0;
    int ok = m_tickTable != nullptr;
    if (ok) {
        TickNode* node = nullptr;
        if (key->m_key != 2) node = FUN_00568de0(m_tickTable, key);
        if (!node) {
            node = FUN_00568d70(m_tickTable, key);
            if (!node) return 0;
            const int value = CallGameHook2AF4();
            node->m_value = value;
            ok = PostTickValue(node->m_a, value, node->m_c);
        }
        if (ok) *out = node->m_value;
    }
    return ok;
}

int GameManager::PostTickValue(int a, int b, int c) {
    PostedEvent* event = static_cast<PostedEvent*>(CreateKeyedObject(2));
    int ok = 0;
    if (event) {
        event->m_field18 = a;
        event->m_result = b;
        event->m_field14 = c;
        const ObjectKey key = ObjectKey::Make(a == 1 ? kAllianceTickKey >> 24 : kEmpireTickKey >> 24,
                                              a == 1 ? kAllianceTickKey & 0xffffff : kEmpireTickKey & 0xffffff);
        new (&event->m_key20) ObjectKey(key);
        ok = m_secondQueue != nullptr;
        if (ok) {
            event->Prepare();
            m_secondQueue->Enqueue(event);
            NoteDispatch(event);
        }
    }
    return ok;
}

int GameManager::PostToFirstQueue(PostedEvent* event) {
    int ok = 0;
    if (event) {
        ok = m_firstQueue != nullptr;
        if (ok) {
            event->Prepare();
            m_firstQueue->Enqueue(event);
            NoteDispatch(event);
        }
    }
    return ok;
}

int GameManager::FlushFirstQueue() {
    const int ok = m_firstQueue != nullptr;
    if (ok) {
        while (m_firstQueue->m_head) CallGameHook2AE0(reinterpret_cast<int>(m_firstQueue->Dequeue()));
    }
    return ok;
}

int GameManager::FlushSecondQueue() {
    const int ok = m_secondQueue != nullptr;
    if (ok) {
        while (m_secondQueue->m_head) CallGameHook2AE0(reinterpret_cast<int>(m_secondQueue->Dequeue()));
    }
    return ok;
}

void RegisterAllFactoriesOnce() {
    if (g_gameFactoriesRegistered == 0) {
        EnsureRecordFactoriesRegistered();
        EnsureTableEntryClassesRegistered();
        RegisterGameObjFactories();
        RegisterEventFactories();
        RegisterOrderFactories();
        g_gameFactoriesRegistered = 1;
    }
}
