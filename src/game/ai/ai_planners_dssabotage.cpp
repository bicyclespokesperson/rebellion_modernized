#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"
#include "core/vptr_store.h"
#include "fleet/fleet_iterator.h"
#include "gameobj/object_lookup.h"

using namespace ai_planner;
using ai::IsFleetKey;
using ai::IsSystemKey;
using ai::KeyOfNode;
using ai::KeyOfObject;
using ai::NextNode;

AiDsSabotagePlanner::AiDsSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x6a, 0x41);
}

AiDsSabotagePlanner::~AiDsSabotagePlanner() { REB_KEEP_VPTR_STORE(); }

int AiDsSabotagePlanner::Virtual06() {
    int sum = 0;
    if ((m_flags.m_value & 0x20) == 0) {
        for (ObjectKeyNode* node = m_leaders.First(); node; node = NextNode(node)) {
            const ObjectKey key = KeyOfNode(node);
            AiCharacterRecord* character = static_cast<AiCharacterRecord*>(m_characters->Find(&key));
            if (character) sum += (character->m_values[5] + character->m_values[1]) / 2;
        }
    }
    return sum;
}

void AiDsSabotagePlanner::Prepare() { ChooseLeader(this, 0x80, 5, 0x33, 0x47400051, 0, 2, 1); }

void AiDsSabotagePlanner::Virtual19() { ChooseMembers(this, Virtual17(), 0x80); }

void AiDsSabotagePlanner::FindTargetShip() {
    bool found = false;
    if (!IsSystemKey(m_x34)) return;
    GameObjNotifier* systemObject = GetObjectInFamilies_90_98(m_view, &m_x34);
    AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
    if (!systemObject || !system || (system->m_flags2c.m_value & 0x1000000) == 0) return;
    FleetIterator fleets(systemObject, 1);
    fleets.Begin();
    while (fleets.m_current && !found) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(KeyOfObject(fleets.m_current));
        if (fleet && (fleet->m_flags.m_value & 1) == 0 && (fleet->m_flags.m_value & 0x10) != 0) {
            for (ObjectKeyNode* node = fleet->m_shipKeys.First(); node && !found; node = NextNode(node)) {
                const ObjectKey shipKey = KeyOfNode(node);
                AiShipRecord* ship = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&shipKey));
                if (ship && (ship->m_flags.m_value & 0x80) != 0) {
                    ship->GetKey(&m_target);
                    found = true;
                }
            }
        }
        fleets.Advance();
    }
}

int AiDsSabotagePlanner::ChooseTarget() {
    int result = 0;
    ObjectKeySet fleetSystems;
    ObjectKey fleetKey;
    ObjectKey scratch;
    m_x34.Reset();
    m_target.Reset();
    m_flags.m_value &= ~0x10u;
    const int membersAtDestination = CountMembersAtDestination();
    // BUG(original): the target was reset above, so the branch that would keep a ship target never runs.
    m_selector.Add(m_owner->RankFleets(0x10, 1, 0x15, 2));
    fleetKey = *m_selector.PickBest(&scratch);
    while (IsFleetKey(fleetKey)) {
        AiFleetEval* fleet = m_owner->m_fleets.FindEval(fleetKey);
        if (fleet) {
            ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(fleet->m_systemKey);
            if (node) fleetSystems.Add(node);
        }
        fleetKey = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    if (static_cast<uint16_t>(fleetSystems.Count()) != 0) {
        bool found = false;
        for (ObjectKeyNode* node = fleetSystems.First(); node && !found; node = NextNode(node)) {
            found = false;
            AiSystemEval* system = m_owner->m_systems.FindEval(KeyOfNode(node));
            if (system && (membersAtDestination == Virtual17() || system->Stat(0xbc) / 2 <= membersAtDestination)) {
                m_x34 = KeyOfNode(node);
                found = true;
            }
        }
        if (IsSystemKey(m_x34)) FindTargetShip();
    }
    if (ai::KeyInFamilies(m_target, 1, 0xff)) {
        result = 1;
        m_flags.m_value |= 0x10;
    }
    return result;
}
