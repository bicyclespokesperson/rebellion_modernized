#include "ai/ai_system_record.h"

#include "ai/ai_agent_bases.h"
#include "ai/ai_common.h"
#include "ai/ai_planner_decl.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/game_obj.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace {

constexpr int kAssignOrder = 0x270;

constexpr uint32_t kSectorFamilyFirst = 0x80;
constexpr uint32_t kSectorFamilyLast = 0x90;
constexpr uint32_t kSystemFamilyFirst = 0x90;
constexpr uint32_t kSystemFamilyLast = 0x98;
constexpr uint32_t kFleetFamilyFirst = 8;
constexpr uint32_t kFleetFamilyLast = 0x10;
constexpr uint32_t kShipFamilyFirst = 0x14;
constexpr uint32_t kShipFamilyLast = 0x1c;

// AiShipRecord::m_flags
constexpr uint32_t kShipFree = 0x20000;
constexpr uint32_t kShipInFleet = 0x4000;
constexpr uint32_t kShipReserve = 0x1000000;
constexpr uint32_t kShipKeepAsReserve = 0x2000000;
constexpr uint32_t kShipReservedMask = 0x13000000;
// AiFleetEval::m_flags
constexpr uint32_t kFleetOurs = 1;
constexpr uint32_t kFleetFree = 4;
// AiSystemEval::m_flags30
constexpr uint32_t kSystemMarkedMask = 0xfff8ffff;
// AiSystemRecord::m_flagsA
constexpr uint32_t kHasReserve = 0x2000000;

bool IsSectorKey(const ObjectKey& key) { return KeyInFamilies(key, kSectorFamilyFirst, kSectorFamilyLast); }
bool IsSystemKey(const ObjectKey& key) { return KeyInFamilies(key, kSystemFamilyFirst, kSystemFamilyLast); }
bool IsFleetKey(const ObjectKey& key) { return KeyInFamilies(key, kFleetFamilyFirst, kFleetFamilyLast); }
bool IsShipKey(const ObjectKey& key) { return KeyInFamilies(key, kShipFamilyFirst, kShipFamilyLast); }

ObjectKey KeyOfNode(ObjectKeyNode* node) {
    ObjectKey key;
    node->GetKey(&key);
    return key;
}

ObjectKeyNode* NextNode(ObjectKeyNode* node) { return static_cast<ObjectKeyNode*>(node->m_successor); }

void AddToTeam(ObjectKeyValueSet* team, const ObjectKey& key) {
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
    if (node) team->Add(node);
}

OrderBase* NewAssignOrder(int side, ObjectKeyValueSet* team, const ObjectKey* target) {
    OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(kAssignOrder));
    if (!order) return nullptr;
    order->m_side = side;
    order->SetTeam(team);
    order->SetTarget(target);
    return order;
}

void ClearFields(AiSystemRecord* record) {
    record->m_state = 0;
    record->m_tier = 0;
    record->m_x9c = 0;
    for (int& value : record->m_sum) value = 0;
    record->m_x94 = 0;
    record->m_x98 = 0;
    for (int& value : record->m_plannerCount) value = 0;
    record->m_xbc = 0;
    record->m_xc0 = 0;
    record->m_xc4 = 0;
    for (int& value : record->m_agentCount) value = 0;
    record->m_xe0 = 0;
    record->m_xe4 = 0;
    record->m_dirty = 0;
}

}  // namespace

AiSystemRecord::AiSystemRecord()
    : IdNode(), m_ships(), m_fleets(), m_sectorKey(), m_reserveShip(), m_reserveSystem(), m_systemKey(), m_selector(),
      m_flagsA(0), m_flagsB(0), m_planners(), m_agents() {
    m_view = 0;
    m_owner = nullptr;
    m_posture = nullptr;
    ClearFields(this);
}

AiSystemRecord::AiSystemRecord(int view, AiSideController* owner)
    : IdNode(), m_ships(), m_fleets(), m_sectorKey(), m_reserveShip(), m_reserveSystem(), m_systemKey(), m_selector(),
      m_flagsA(0), m_flagsB(0), m_planners(), m_agents() {
    m_view = view;
    m_owner = owner;
    m_posture = nullptr;
    ClearFields(this);
}

AiSystemRecord::~AiSystemRecord() {
    m_owner = nullptr;
    delete m_posture;
    m_posture = nullptr;
    m_flagsA.m_value = 0;
    m_flagsB.m_value = 0;
    REB_KEEP_VPTR_STORE();
}

void AiSystemRecord::Read(istream* in) {
    ReadId(in);
    ReadValue32(in, &m_state);
    ReadValue32(in, &m_view);
    ReadValue32(in, &m_tier);
    m_flagsA.Read(in);
    m_flagsB.Read(in);
    m_systemKey.Read(in);
    ReadValue32(in, &m_x9c);
    for (int& value : m_sum) ReadValue32(in, &value);
    ReadValue32(in, &m_x94);
    ReadValue32(in, &m_x98);
    for (int& value : m_plannerCount) ReadValue32(in, &value);
    ReadValue32(in, &m_xbc);
    ReadValue32(in, &m_xc0);
    ReadValue32(in, &m_xc4);
    m_planners.Read(in);
    for (int& value : m_agentCount) ReadValue32(in, &value);
    ReadValue32(in, &m_xe0);
    ReadValue32(in, &m_xe4);
    m_agents.Read(in);
    ReadValue32(in, &m_dirty);
    m_ships.Load(in);
    m_fleets.Load(in);
    m_selector.Read(in);
    m_reserveShip.Read(in);
    m_reserveSystem.Read(in);
    m_sectorKey.Read(in);
    int postureKind;
    ReadValue32(in, &postureKind);
    if (postureKind != 0) {
        m_posture = CreatePosture(postureKind);
        if (m_posture) m_posture->Read(in);
    }
}

void AiSystemRecord::Write(ostream* out) {
    WriteId(out);
    WriteValue32(out, &m_state);
    WriteValue32(out, &m_view);
    WriteValue32(out, &m_tier);
    m_flagsA.Write(out);
    m_flagsB.Write(out);
    m_systemKey.Write(out);
    WriteValue32(out, &m_x9c);
    for (int& value : m_sum) WriteValue32(out, &value);
    WriteValue32(out, &m_x94);
    WriteValue32(out, &m_x98);
    for (int& value : m_plannerCount) WriteValue32(out, &value);
    WriteValue32(out, &m_xbc);
    WriteValue32(out, &m_xc0);
    WriteValue32(out, &m_xc4);
    m_planners.Write(out);
    for (int& value : m_agentCount) WriteValue32(out, &value);
    WriteValue32(out, &m_xe0);
    WriteValue32(out, &m_xe4);
    m_agents.Write(out);
    WriteValue32(out, &m_dirty);
    m_ships.Save(out);
    m_fleets.Save(out);
    m_selector.Write(out);
    m_reserveShip.Write(out);
    m_reserveSystem.Write(out);
    m_sectorKey.Write(out);
    if (m_posture) {
        int postureKind = m_posture->Virtual05();
        WriteValue32(out, &postureKind);
        m_posture->Write(out);
        return;
    }
    const int noPosture = 0;
    WriteValue32(out, &noPosture);
}

void AiSystemRecord::SetSector(const ObjectKey* sector) {
    if (IsSectorKey(m_sectorKey)) {
        AiSectorEval* current = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&m_sectorKey));
        if (current && current->m_x38 == static_cast<int>(m_id)) current->SetPeer(0);
        m_sectorKey.Reset();
    }
    if (IsSectorKey(*sector)) {
        AiSectorEval* next = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(sector));
        if (next && next->m_x38 == 0) {
            next->SetPeer(m_id);
            m_sectorKey = *sector;
            SetTier(next->m_x3c);
        }
    }
    if (!IsSectorKey(m_sectorKey)) SetTier(0);
}

void AiSystemRecord::SetTier(uint32_t tier) {
    if (m_tier == tier) return;
    m_tier = tier;
    for (ObjectKeyNode* node = m_ships.First(); node; node = NextNode(node)) {
        ObjectKey key = KeyOfNode(node);
        AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
        if (ship && ship->m_x30 == static_cast<int>(m_id)) ship->m_x34 = tier;
    }
    for (ObjectKeyNode* node = m_fleets.First(); node; node = NextNode(node)) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(KeyOfNode(node));
        if (fleet && fleet->m_x30 == static_cast<int>(m_id)) fleet->m_x34 = tier;
    }
    for (AvlNode* node = m_planners.First(); node; node = node->m_successor) {
        AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(static_cast<IdNode*>(node)->m_id));
        if (planner) planner->SetTier(tier);
    }
    for (AvlNode* node = m_agents.First(); node; node = node->m_successor) {
        AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(static_cast<IdNode*>(node)->m_id));
        if (agent) agent->SetTier(tier);
    }
}

int AiSystemRecord::ForwardToPosture() { return m_posture->Virtual03(); }

int AiSystemRecord::HasWarships() { return m_xbc != 0 || m_xc4 != 0; }

int __stdcall PlannerClassOf(AiMissionPlanner* planner) {
    if (!planner) return 0;
    switch (planner->m_missionKey.Family()) {
    case 0x52:
        return 1;
    case 0x54:
        return 2;
    case 0x62:
    case 0x63:
        return 0;
    case 0x69:
        return 3;
    }
    return 0;
}

int __stdcall AgentClassOf(AiAgentTargeted* agent) {
    if (agent) {
        const int kind = agent->GetKind();
        if (kind != 3) {
            if (kind == 4) return 1;
            if (kind == 5) return 2;
        }
    }
    return 0;
}

void AiSystemRecord::AddPlanner(AiMissionPlanner* planner) {
    if (!planner) return;
    const uint32_t id = planner->m_id;
    m_xbc = 0;
    m_xc0 = 0;
    if (m_planners.FindById(id)) return;
    IdNode* node = NewOrNull<IdNode>(id);
    if (!node) return;
    m_planners.AddWithId(node);
    ++m_plannerCount[static_cast<uint16_t>(PlannerClassOf(planner))];
    planner->m_systemRecordId = m_id;
    planner->m_sectorKey = m_sectorKey;
    planner->SetTier(m_tier);
}

void AiSystemRecord::RemovePlanner(AiMissionPlanner* planner) {
    if (!planner) return;
    IdNode* node = m_planners.DetachById(planner->m_id);
    if (!node) return;
    delete node;
    --m_plannerCount[static_cast<uint16_t>(PlannerClassOf(planner))];
    planner->SetTier(0);
    planner->m_systemRecordId = 0;
    planner->m_sectorKey.Reset();
    planner->SetTier(0);
}

int AiSystemRecord::HasAgents() { return m_xe0 != 0 || m_xe4 != 0; }

void AiSystemRecord::AddAgent(AiAgentTargeted* agent) {
    if (!agent) return;
    m_xe0 = 0;
    const uint32_t id = agent->m_id;
    if (m_agents.FindById(id)) return;
    IdNode* node = NewOrNull<IdNode>(id);
    if (!node) return;
    m_agents.AddWithId(node);
    ++m_agentCount[static_cast<uint16_t>(AgentClassOf(agent))];
    agent->m_systemRecordId = m_id;
    agent->m_sectorKey = m_sectorKey;
    agent->SetTier(m_tier);
}

void AiSystemRecord::RemoveAgent(AiAgentTargeted* agent) {
    if (!agent) return;
    IdNode* node = m_agents.DetachById(agent->m_id);
    if (!node) return;
    delete node;
    --m_agentCount[static_cast<uint16_t>(AgentClassOf(agent))];
    agent->m_systemRecordId = 0;
    agent->m_sectorKey.Reset();
    agent->SetTier(0);
}

int AiSystemRecord::HasAgent(uint32_t id) { return m_agents.FindById(id) != nullptr; }

void AiSystemRecord::GetOrderTargets(int orderKind, ObjectKey* first, ObjectKey* second) {
    switch (orderKind) {
    case 0x12:
        m_posture->Virtual12(first, second);
        return;
    case 0x13:
        m_posture->Virtual10(first, second);
        return;
    case 0x15:
        m_posture->Virtual11(first, second);
        return;
    case 0x17:
    case 0x81:
        m_posture->Virtual09(first, second);
        return;
    default:
        first->Reset();
        second->Reset();
        return;
    }
}

ObjectKey* AiSystemRecord::ChooseReinforcementSource(ObjectKey* out) {
    ObjectKey picked;
    ObjectKey source;
    ObjectKey scratch;
    int committed = 0;
    m_x9c = 0;
    if (IsSystemKey(m_systemKey)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
        if (system && (system->m_flags28.m_value & 2) == 0) {
            m_selector.Add(m_owner->RankFleetsByTarget(m_id, 1, 0, 0x17, 1));
            picked = *m_selector.PickBest(&scratch);
            while (IsFleetKey(picked)) {
                AiFleetEval* fleet = m_owner->m_fleets.FindEval(picked);
                if (fleet) committed += fleet->Stat(0xac);
                picked = *m_selector.PickBest(&scratch);
            }
            m_selector.Reset();
            committed -= system->Stat(0xa4);
            m_x9c = committed;
            if (0 < committed) source = m_systemKey;
        }
    }
    *out = source;
    return out;
}

void AiSystemRecord::Reset() {
    m_flagsA.m_value = 0;
    m_flagsB.m_value = 0;
    if (m_posture) {
        delete m_posture;
        m_posture = nullptr;
    }
    m_state = 0;
    if (IsSectorKey(m_sectorKey)) {
        AiSectorEval* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&m_sectorKey));
        if (sector) sector->SetPeer(0);
    }
    m_sectorKey.Reset();
    m_dirty = 0;
    m_x9c = 0;
    for (ObjectKeyNode* node = m_ships.First(); node;) {
        ObjectKey key = KeyOfNode(node);
        node = NextNode(node);
        RemoveShip(&key);
    }
    for (ObjectKeyNode* node = m_fleets.First(); node;) {
        ObjectKey key = KeyOfNode(node);
        node = NextNode(node);
        RemoveFleet(&key);
    }
    for (AvlNode* node = m_planners.First(); node;) {
        const uint32_t id = static_cast<IdNode*>(node)->m_id;
        node = node->m_successor;
        m_owner->m_planners.ReleasePlanner(id);
    }
    for (AvlNode* node = m_agents.First(); node;) {
        AvlNode* next = node->m_successor;
        m_owner->m_agents.ReleaseAgent(static_cast<IdNode*>(node)->m_id);
        node = next;
    }
    for (int& value : m_sum) value = 0;
    if (IsSystemKey(m_systemKey)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
        if (system) {
            system->m_flags30.m_value &= kSystemMarkedMask;
            system->MarkChanged();
            m_systemKey.Reset();
        }
    }
    SetTier(0);
}

int AiSystemRecord::UpdatePosture() {
    int ok = 0;
    AiSectorEval* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&m_sectorKey));
    if (sector) {
        if (m_posture) {
            const int wanted = sector->m_x30;
            if (m_posture->Virtual05() == wanted) {
                ok = 1;
            } else {
                delete m_posture;
                m_posture = nullptr;
            }
        }
        if (!m_posture) {
            m_flagsA.m_value = 0;
            m_flagsB.m_value = 0;
            m_state = 0;
            m_dirty = 0;
            if (sector->m_x30 != 0) {
                m_posture = CreatePosture(sector->m_x30);
                if (m_posture) {
                    const int wanted = sector->m_x30;
                    if (m_posture->Virtual05() == wanted) ok = 1;
                }
            }
        }
        if (static_cast<uint32_t>(sector->m_x3c) != m_tier) SetTier(sector->m_x3c);
    }
    if (ok == 0) Reset();
    return ok;
}

OrderBase* AiSystemRecord::Step(int* done) {
    OrderBase* order = nullptr;
    int stepDone = 1;
    if (m_dirty != 0) {
        ReleaseLostShips();
        ReleaseLostFleets();
        m_dirty = 0;
    }
    *done = 0;
    if (m_posture == nullptr) {
        *done = 1;
    } else {
        switch (m_state) {
        case 1:
            m_flagsA.m_value = 0;
            m_flagsB.m_value = 0;
            order = RequestReinforcements();
            m_state = 2;
            break;
        case 2:
            SumCommittedStats();
            FUN_004adaf0(m_posture, m_sum);
            m_state = 3;
            break;
        case 3:
            order = CheckShortfall(&stepDone);
            if (stepDone != 0) m_state = 4;
            break;
        case 4:
            CountPlanners();
            CountAgents();
            m_state = 5;
            break;
        case 5:
            order = FUN_004adb70(m_posture, &stepDone);
            if (stepDone != 0) m_state = 6;
            break;
        case 6:
            UpdateReserve();
            UpdateReserveFleet();
            m_posture->Virtual08(&m_flagsA, &m_flagsB);
            UpdateShortage();
            m_state = 0;
            *done = 1;
            [[fallthrough]];  // BUG(original): the finished pass falls into the default case and restarts at state 1
        default:
            m_state = 1;
            m_dirty = 1;
        }
    }
    if (order) m_dirty = 1;
    return order;
}

AiPosture* AiSystemRecord::CreatePosture(int kind) {
    void* memory;
    switch (kind) {
    case 1:
        memory = ::operator new(0x5c);
        if (memory) return FUN_004ba580(memory, m_view, m_owner, this);
        break;
    case 2:
        memory = ::operator new(100);
        if (memory) return FUN_004b91a0(memory, m_view, m_owner, this);
        break;
    case 3:
        memory = ::operator new(0x5c);
        if (memory) return FUN_004b37f0(memory, m_view, m_owner, this);
        break;
    case 4:
        memory = ::operator new(0x60);
        if (memory) return FUN_004b6fb0(memory, m_view, m_owner, this);
        break;
    case 5:
        memory = ::operator new(0x5c);
        if (memory) return FUN_004b1720(memory, m_view, m_owner, this);
        break;
    case 6:
        memory = ::operator new(0x5c);
        if (memory) return FUN_004b4d50(memory, m_view, m_owner, this);
        break;
    }
    return nullptr;
}

void AiSystemRecord::ReleaseLostShips() {
    ObjectKey fleetKey;
    ObjectKeyNode* node = m_ships.First();
    while (node) {
        bool keep = false;
        ObjectKey key = KeyOfNode(node);
        AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
        if (ship && ship->m_x30 == static_cast<int>(m_id) && (ship->m_flags.m_value & kShipInFleet) != 0) {
            keep = true;
            fleetKey = ship->m_fleetKey;
            if (!m_fleets.Find(&fleetKey)) {
                AiFleetEval* fleet = m_owner->m_fleets.FindEval(fleetKey);
                if (fleet && (fleet->m_flags.m_value & kFleetFree) != 0) AddFleet(&fleetKey);
            }
        }
        if (keep) {
            node = NextNode(node);
        } else {
            ObjectKeyNode* next = NextNode(node);
            ObjectKey releasedKey = KeyOfNode(node);
            RemoveShip(&releasedKey);
            node = next;
        }
    }
}

void AiSystemRecord::ReleaseLostFleets() {
    ObjectKeyNode* node = m_fleets.First();
    while (node) {
        bool keep = false;
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(KeyOfNode(node));
        if (fleet && fleet->m_x30 == static_cast<int>(m_id) && (fleet->m_flags.m_value & kFleetOurs) != 0) {
            if (fleet->m_shipKeys.Count() != 0) keep = true;
        }
        if (keep) {
            node = NextNode(node);
        } else {
            ObjectKeyNode* next = NextNode(node);
            ObjectKey releasedKey = KeyOfNode(node);
            RemoveFleet(&releasedKey);
            node = next;
        }
    }
}

OrderBase* AiSystemRecord::RequestReinforcements() {
    OrderBase* order = nullptr;
    ObjectKeyValueSet team;
    ObjectKey systemKey;
    int moved = 0;
    for (ObjectKeyNode* node = m_fleets.First(); node && moved == 0; node = NextNode(node)) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(KeyOfNode(node));
        if (!fleet || (fleet->m_flags.m_value & 0x3000) == 0 || (static_cast<uint32_t>(fleet->m_flags.m_value) & 0xf0000802) != 0)
            continue;
        systemKey = fleet->m_systemKey;
        for (ObjectKeyNode* shipNode = fleet->m_shipKeys.First(); shipNode; shipNode = NextNode(shipNode)) {
            ObjectKey key = KeyOfNode(shipNode);
            AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
            if (ship && (ship->m_flags.m_value & 0x801000) != 0 && (ship->m_flags.m_value & 0x800) == 0) {
                moved = 1;
                ObjectKey shipKey = KeyOfNode(shipNode);
                RemoveShip(&shipKey);
                AddToTeam(&team, KeyOfNode(shipNode));
            }
        }
    }
    if (team.First()) order = NewAssignOrder(m_view, &team, &systemKey);
    return order;
}

void AiSystemRecord::SumCommittedStats() {
    for (int& value : m_sum) value = 0;
    for (ObjectKeyNode* node = m_fleets.First(); node; node = NextNode(node)) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(KeyOfNode(node));
        if (!fleet) continue;
        m_sum[0] += fleet->Stat(0xb4);
        m_sum[1] += fleet->Stat(0xb8);
        m_sum[2] += fleet->Stat(0x84);
        m_sum[3] += fleet->Stat(0x78);
        m_sum[5] += fleet->Stat(0x68);
        m_sum[7] += fleet->Stat(0x7c);
        m_sum[8] += fleet->Stat(0x70);
        m_sum[9] += fleet->Stat(0x9c);
        if (fleet->m_flags.m_value & 0x10) ++m_sum[4];
        if (fleet->m_flags.m_value & 0x20) ++m_sum[6];
    }
    if (0 < m_sum[0]) m_flagsB.m_value |= 0x200000;
    if (0 < m_sum[1]) m_flagsB.m_value |= 0x400000;
    if (0 < m_sum[2]) m_flagsB.m_value |= 0x800000;
    if (0 < m_sum[3]) m_flagsB.m_value |= 0x1000000;
    if (0 < m_sum[5]) m_flagsB.m_value |= 0x2000000;
    if (0 < m_sum[7]) m_flagsB.m_value |= 0x4000000;
    if (0 < m_sum[8]) m_flagsB.m_value |= 0x8000000;
    if (0 < m_sum[9]) m_flagsB.m_value |= 0x10000000;
    if (0 < m_sum[6]) m_flagsB.m_value |= 0x100000;
    m_x94 = m_sum[2];
    m_x98 = m_sum[3];
}

OrderBase* AiSystemRecord::CheckShortfall(int* done) {
    OrderBase* order = nullptr;
    *done = 0;
    const auto lacking = [this] {
        return m_sum[0] < 0 || m_sum[1] < 0 || m_sum[2] < 0 || m_sum[3] < 0 || m_sum[5] < 0 || m_sum[4] < 0 || m_sum[6] < 0 ||
               m_sum[9] < 0;
    };
    const auto surplus = [this] {
        return 0 < m_sum[0] || 0 < m_sum[1] || 0 < m_sum[2] || 0 < m_sum[3] || 0 < m_sum[5] || 0 < m_sum[4] || 0 < m_sum[6] ||
               0 < m_sum[9];
    };
    if (lacking()) {
        order = FillShortfallByDemand();
        if (order == nullptr && lacking()) order = FillShortfallByClass();
        if (m_sum[0] < 0) m_flagsB.m_value |= 1;
        if (m_sum[1] < 0) m_flagsB.m_value |= 2;
        if (m_sum[2] < 0) m_flagsB.m_value |= 4;
        if (m_sum[3] < 0) m_flagsB.m_value |= 8;
        if (m_sum[5] < 0) m_flagsB.m_value |= 0x10;
        if (m_sum[7] < 0) m_flagsB.m_value |= 0x20;
        if (m_sum[8] < 0) m_flagsB.m_value |= 0x40;
        if (m_sum[9] < 0) m_flagsB.m_value |= 0x80;
        if (m_sum[6] < 0) m_flagsB.m_value |= 0x800;
    }
    if (order == nullptr && surplus()) ReleaseSurplus();  // BUG(original): the order it builds is dropped
    *done = 1;
    return order;
}

void AiSystemRecord::CountPlanners() {
    m_xbc = 0;
    m_xc0 = 0;
    m_xc4 = 0;
    int wantedKind;
    int wantedSubtype;
    m_posture->Virtual06(&m_xbc, &m_xc0, &wantedKind, &wantedSubtype);
    if (m_xbc != 0) m_flagsB.m_value |= 0x100;
    int bestPriority = 2000000000;
    for (AvlNode* node = m_planners.First(); node; node = node->m_successor) {
        AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(static_cast<IdNode*>(node)->m_id));
        if (!planner) continue;
        m_flagsB.m_value |= 0x20000000;
        if ((planner->m_flags.m_value & 3) != 3) m_flagsB.m_value |= 0x400;
        if (planner->m_flags.m_value & 0x100) m_flagsB.m_value |= 0x80000000;
        if (wantedKind != 0 && static_cast<int>(planner->m_missionKey.Family()) == wantedKind &&
            planner->GetSubtype() == wantedSubtype && (planner->m_flags.m_value & 0x10000000) == 0 &&
            planner->Virtual06() < bestPriority) {
            m_xc4 = planner->m_id;
            bestPriority = planner->Virtual06();
        }
    }
}

void AiSystemRecord::CountAgents() {
    m_xe0 = 0;
    m_xe4 = 0;
    m_flagsA.m_value &= 0xf8ffffff;
    for (AvlNode* node = m_agents.First(); node; node = node->m_successor) {
        AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(static_cast<IdNode*>(node)->m_id));
        if (!agent) continue;
        const uint32_t flags = agent->m_flags.m_value;
        m_flagsA.m_value |= 0x2000000;
        m_flagsB.m_value |= 0x40000000;
        if (flags & 0x10) m_flagsA.m_value |= 0x4000000;
        if (flags & 8) {
            if ((flags & 4) == 0)
                m_flagsA.m_value |= 0x1000000;
            else if (agent->GetKind() == 4)
                m_flagsA.m_value |= 0x200000;
        }
        if ((flags & 1) == 0)
            m_flagsB.m_value |= 0x400;
        else
            m_flagsB.m_value |= 0x80000000;
    }
    m_posture->Virtual07(&m_xe0);
    if (m_xe0 != 0) m_flagsB.m_value |= 0x200;
    AvlNode* node = m_agents.First();
    while (node && m_xe4 == 0) {
        const uint32_t id = static_cast<IdNode*>(node)->m_id;
        if (FUN_004add40(m_posture, id) != 0) m_xe4 = id;
        node = node->m_successor;
    }
}

namespace {

// The per-ship test of the two shortfall fillers: whether the ship covers one of the missing kinds of strength.
struct ShortfallScan {
    AiSystemRecord* record;
    ObjectKeyValueSet* team;
    uint32_t forbiddenFlags;
    bool requireFree;
    bool keepsAsReserve;
    int taken = 0;
    bool settled = false;

    // Scans the ships of the fleets the selector yields (best first) until a ship is taken or the fleets run out.
    void Run(AiRankedSelector* selector) {
        ObjectKey scratch;
        ObjectKey picked = *selector->PickBest(&scratch);
        for (;;) {
            if (!IsFleetKey(picked) || taken != 0) break;
            AiFleetEval* fleet = record->m_owner->m_fleets.FindEval(picked);
            if (fleet) {
                for (ObjectKeyNode* node = fleet->m_shipKeys.First(); node && !settled; node = NextNode(node)) {
                    ObjectKey key = KeyOfNode(node);
                    AiShipRecord* ship = static_cast<AiShipRecord*>(record->m_owner->m_ships.Find(&key));
                    if (!ship) continue;
                    const uint32_t flags = ship->m_flags.m_value;
                    if (requireFree && (flags & kShipFree) == 0) continue;
                    if ((flags & forbiddenFlags) != 0) continue;
                    if (Covers(ship, flags)) TakeOrMark(node, ship, flags);
                }
            }
            if (taken != 0) continue;
            picked = *selector->PickBest(&scratch);
        }
        selector->Reset();
    }

private:
    bool Covers(AiShipRecord* ship, uint32_t flags) {
        const int* lack = record->m_sum;
        bool takes = false;
        settled = true;
        if (lack[0] < 0) {
            settled = false;
            if (flags & 1) takes = true;
        }
        if (lack[1] < 0 && !takes) {
            settled = false;
            if (flags & 0x102) takes = true;
        }
        if (lack[9] < 0 && !takes) {
            settled = false;
            if (flags & 2) takes = true;
        }
        if (lack[2] < 0 && !takes) {
            settled = false;
            if (flags & 0xc) takes = true;
        }
        if (lack[3] < 0 && !takes) {
            settled = false;
            if (0 < ship->Stat(0x60)) takes = true;
        }
        if (lack[5] < 0 && !takes) {
            settled = false;
            if (0 < ship->Stat(0x48)) takes = true;
        }
        if (lack[4] < 0 && !takes) {
            settled = false;
            if (flags & 0x80) takes = true;
        }
        if (lack[6] < 0 && !takes) {
            settled = false;
            if (flags & 0x400) takes = true;
        }
        return takes;
    }

    void TakeOrMark(ObjectKeyNode* node, AiShipRecord* ship, uint32_t flags) {
        if (keepsAsReserve && (flags & kShipReserve) != 0) {
            ship->m_flags.m_value |= kShipKeepAsReserve;
            return;
        }
        taken = 1;
        ObjectKeyValueNode* teamNode = NewOrNull<ObjectKeyValueNode>(KeyOfNode(node), 0);
        if (teamNode) {
            team->Add(teamNode);
            ObjectKey shipKey = KeyOfNode(node);
            record->AddShip(&shipKey);
        }
    }
};

}  // namespace

OrderBase* AiSystemRecord::FillShortfallByDemand() {
    OrderBase* order = nullptr;
    ObjectKeyValueSet team;
    m_selector.Add(m_owner->RankFleetsBySectorDistance(&m_sectorKey, 0x8005, 2, 2));
    ShortfallScan scan{this, &team, 0xc03800, true, false};
    scan.Run(&m_selector);
    if (team.First()) {
        ObjectKey picked;
        ObjectKey scratch;
        ObjectKey target = *ChooseSource(&scratch, &picked);
        order = NewAssignOrder(m_view, &team, &target);
    }
    return order;
}

OrderBase* AiSystemRecord::FillShortfallByClass() {
    OrderBase* order = nullptr;
    ObjectKeyValueSet team;
    m_selector.Add(m_owner->RankFleetsByX34(m_tier, 0x8001, 0xf0000006, 1));
    ShortfallScan scan{this, &team, 0xc23800, false, true};
    scan.Run(&m_selector);
    if (team.First()) {
        ObjectKey picked;
        ObjectKey scratch;
        ObjectKey target = *ChooseSource(&scratch, &picked);
        order = NewAssignOrder(m_view, &team, &target);
    }
    return order;
}

OrderBase* AiSystemRecord::ReleaseSurplus() {
    OrderBase* order = nullptr;
    ObjectKey fleetKey;
    ObjectKeyValueSet team;
    bool found = false;
    ObjectKeyNode* node = m_ships.First();
    while (node && !found) {
        ObjectKey key = KeyOfNode(node);
        AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
        found = false;
        if (!ship || (ship->m_flags.m_value & 0x402800) != 0 || ship->IsUnusable() != 0) {
            node = NextNode(node);
            continue;
        }
        const uint32_t flags = ship->m_flags.m_value;
        found = ship->Stat(0x7c) <= m_sum[0] && ship->Stat(0x80) <= m_sum[1];
        if (found && (flags & 2) != 0 && m_sum[9] < 1) found = false;
        if (found && (flags & 0xc) != 0 && m_sum[2] < ship->Stat(0x6c)) found = false;
        if (found && 0 < ship->Stat(0x60) && m_sum[3] < ship->Stat(0x60)) found = false;
        if (found && 0 < ship->Stat(0x48) && m_sum[5] < ship->Stat(0x48)) found = false;
        if (found && m_sum[4] < 1 && (flags & 0x80) != 0) found = false;
        if (found && m_sum[6] < 1 && (flags & 0x400) != 0) found = false;
        if (found) {
            ObjectKey shipKey = KeyOfNode(ship);
            if (shipKey.m_value == m_reserveShip.m_value && 1 < static_cast<uint16_t>(m_ships.Count())) {
                found = false;
                ship->m_flags.m_value |= kShipKeepAsReserve;
            }
        }
        if (!found) {
            node = NextNode(node);
            continue;
        }
        ObjectKeyNode* taken = node;
        node = NextNode(node);
        fleetKey = ship->m_fleetKey;
        ObjectKeyValueNode* teamNode = NewOrNull<ObjectKeyValueNode>(KeyOfNode(taken), 0);
        if (teamNode) {
            team.Add(teamNode);
            ObjectKey takenKey = KeyOfNode(taken);
            RemoveShip(&takenKey);
        }
    }
    if (team.First()) {
        ObjectKey scratch;
        ObjectKey target = *ChooseSource(&scratch, &fleetKey);
        order = NewAssignOrder(m_view, &team, &target);
    }
    return order;
}

int AiSystemRecord::AddShip(const ObjectKey* key) {
    int added = 0;
    if (!IsShipKey(*key)) return added;
    ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*key);
    if (!node) return added;
    AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(key));
    if (!ship) {
        delete node;
        return added;
    }
    ship->m_flags.m_value &= ~kShipFree;
    ship->m_x30 = m_id;
    ship->m_x34 = m_tier;
    if (0 < ship->Stat(0x7c)) {
        m_sum[0] += ship->Stat(0x7c);
        m_flagsB.m_value |= 0x200000;
    }
    if (0 < ship->Stat(0x80)) {
        m_sum[1] += ship->Stat(0x80);
        m_flagsB.m_value |= 0x400000;
    }
    if (0 < ship->Stat(0x6c)) {
        m_sum[2] += ship->Stat(0x6c);
        m_flagsB.m_value |= 0x800000;
    }
    if (0 < ship->Stat(0x60)) {
        m_sum[3] += ship->Stat(0x60);
        m_flagsB.m_value |= 0x1000000;
    }
    if (0 < ship->Stat(0x48)) {
        m_sum[5] += ship->Stat(0x48);
        m_flagsB.m_value |= 0x2000000;
    }
    m_sum[7] += ship->Stat(0x64);
    m_sum[8] += ship->Stat(0x58);
    if (ship->m_flags.m_value & 0x80) ++m_sum[4];
    if (ship->m_flags.m_value & 0x400) ++m_sum[6];
    if (ship->m_flags.m_value & 2) ++m_sum[9];
    m_ships.Add(node);
    return 1;
}

int AiSystemRecord::AddFleet(const ObjectKey* key) {
    int added = 0;
    if (!IsFleetKey(*key)) return added;
    ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*key);
    if (!node) return added;
    AiFleetEval* fleet = m_owner->m_fleets.FindEval(*key);
    if (!fleet) {
        delete node;
        return added;
    }
    fleet->m_flags.m_value &= ~kFleetFree;
    fleet->m_x30 = m_id;
    fleet->m_x34 = m_tier;
    m_fleets.Add(node);
    return 1;
}

int AiSystemRecord::RemoveShip(const ObjectKey* key) {
    if (!m_ships.Find(key)) return 0;
    AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(key));
    if (ship) {
        if (ship->m_x30 == static_cast<int>(m_id)) {
            ship->m_flags.m_value = (ship->m_flags.m_value & 0xfcffffff) | kShipFree;
            ship->m_x30 = 0;
            ship->m_x34 = 0;
        }
        m_sum[0] -= ship->Stat(0x7c);
        m_sum[1] -= ship->Stat(0x80);
        m_sum[2] -= ship->Stat(0x6c);
        m_sum[3] -= ship->Stat(0x60);
        m_sum[5] -= ship->Stat(0x48);
        m_sum[7] -= ship->Stat(0x64);
        m_sum[8] -= ship->Stat(0x58);
        if (ship->m_flags.m_value & 0x80) --m_sum[4];
        if (ship->m_flags.m_value & 0x400) --m_sum[6];
        if (ship->m_flags.m_value & 2) --m_sum[9];
    }
    m_ships.RemoveKey(key);
    return 1;
}

int AiSystemRecord::RemoveFleet(const ObjectKey* key) {
    if (!m_fleets.Find(key)) return 0;
    AiFleetEval* fleet = m_owner->m_fleets.FindEval(*key);
    if (fleet) {
        fleet->m_flags.m_value |= kFleetFree;
        fleet->m_x30 = 0;
        fleet->m_x34 = 0;
    }
    m_fleets.RemoveKey(key);
    return 1;
}

ObjectKey* AiSystemRecord::ChooseSource(ObjectKey* out, const ObjectKey* hint) {
    ObjectKey result;
    ObjectKey scratch;
    if (IsSystemKey(m_systemKey)) {
        AiSystemEval* system = m_owner->m_systems.FindEval(m_systemKey);
        if (system && (system->m_flags28.m_value & 2) == 0) result = m_systemKey;
    }
    if (!IsSystemKey(result)) {
        m_selector.Add(m_owner->RankSystemsOfSector(&m_sectorKey, 0x80000000, 0, 1, 2, 0, 0, 7, 2));
        result = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(result)) {
        m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&m_sectorKey, hint, 0, 0, 1, 2, 0, 0, 2));
        result = *m_selector.PickBest(&scratch);
        m_selector.Reset();
    }
    if (!IsSystemKey(result)) result = *m_owner->FindNearestSystemWithFleets(&scratch, &m_sectorKey, 0, 0, 1, 2, 0, 0);
    *out = result;
    return out;
}

void AiSystemRecord::UpdateReserve() {
    int keptReserve = 0;
    int bestPower = 0;
    ObjectKey scratch;
    ObjectKey candidate;
    if ((m_flagsA.m_value & kHasReserve) == 0) {
        if (m_ships.Find(&m_reserveShip)) {
            AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_reserveShip));
            if (ship && ship->m_x30 == static_cast<int>(m_id)) ship->m_flags.m_value &= ~kShipReserve;
        }
        m_reserveShip.Reset();
        return;
    }
    if (m_ships.Find(&m_reserveShip)) {
        AiShipRecord* reserve = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_reserveShip));
        if (reserve && reserve->m_x30 == static_cast<int>(m_id) && (reserve->m_flags.m_value & kShipReserve) != 0) {
            const uint32_t flags = reserve->m_flags.m_value;
            if ((flags & kShipKeepAsReserve) == 0) {
                bool isShip = true;
                keptReserve = 1;
                if ((flags & 0x800) == 0) {
                    candidate = reserve->m_fleetKey;
                    if (m_fleets.Find(&candidate)) {
                        const auto promote = [&](AiShipRecord* better) {
                            better->m_flags.m_value |= kShipReserve;
                            reserve->m_flags.m_value &= ~kShipReserve;
                            m_reserveShip = candidate;
                        };
                        m_selector.Add(m_owner->RankShipsOfFleet(&candidate, 0x4001, 0x2803800, 3, 1));
                        candidate = *m_selector.PickBest(&scratch);
                        m_selector.Reset();
                        if (!IsShipKey(candidate)) isShip = false;
                        if (isShip) {
                            AiShipRecord* better = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&candidate));
                            if (better && (reserve->Stat(0x50) < better->Stat(0x50) || (reserve->m_flags.m_value & 1) == 0))
                                promote(better);
                        } else {
                            m_selector.Add(m_owner->RankShipsOfFleet(&candidate, 0x4000, 0x2803800, 3, 1));
                            candidate = *m_selector.PickBest(&scratch);
                            m_selector.Reset();
                            if (IsShipKey(candidate)) {
                                AiShipRecord* better = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&candidate));
                                if (better && reserve->Stat(0x50) < better->Stat(0x50)) promote(better);
                            }
                        }
                    }
                }
            } else {
                reserve->m_flags.m_value &= ~kShipReserve;
            }
        }
    }
    if (keptReserve == 0) {
        m_reserveShip.Reset();
        for (ObjectKeyNode* node = m_fleets.First(); node; node = NextNode(node)) {
            ObjectKey fleetKey = KeyOfNode(node);
            m_selector.Add(m_owner->RankShipsOfFleet(&fleetKey, 0x4000, 0x2803800, 3, 1));
            candidate = *m_selector.PickBest(&scratch);
            m_selector.Reset();
            if (!IsShipKey(candidate)) continue;
            AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&candidate));
            if (ship && bestPower < ship->Stat(0x50)) {
                m_reserveShip = candidate;
                bestPower = ship->Stat(0x50);
            }
        }
        if (IsShipKey(m_reserveShip)) {
            AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_reserveShip));
            if (ship) ship->m_flags.m_value |= kShipReserve;
        }
    }
    for (ObjectKeyNode* node = m_ships.First(); node; node = NextNode(node)) {
        ObjectKey key = KeyOfNode(node);
        AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&key));
        if (ship) ship->m_flags.m_value &= ~kShipKeepAsReserve;
    }
}

void AiSystemRecord::UpdateReserveFleet() {
    if ((m_flagsA.m_value & kHasReserve) != 0) {
        if (m_posture) m_posture->Virtual04(&m_reserveSystem);
        return;
    }
    m_reserveSystem.Reset();
}

void AiSystemRecord::UpdateShortage() {
    if ((m_flagsB.m_value & 0x4ff) == 0) {
        m_flagsA.m_value |= 0x1c0000;
        return;
    }
    if ((static_cast<uint32_t>(m_flagsB.m_value) & 0x9fe00000) == 0) {
        m_flagsA.m_value |= 0x10000;
        return;
    }
    m_flagsA.m_value |= 0x100000;
    if ((m_flagsB.m_value & 0x1f) == 0) m_flagsA.m_value |= 0x80000;
}
