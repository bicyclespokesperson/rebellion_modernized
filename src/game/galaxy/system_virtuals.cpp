#include "character/role.h"
#include "core/vptr_store.h"
#include "fleet/fleet.h"
#include "galaxy/system.h"
#include "gameobj/object_family.h"
#include "orders/order_checks.h"

namespace {

constexpr uint32_t kBothSidesBit = 0xfffffffe;
constexpr uint32_t kAlwaysState = 3;
constexpr FamilyRange kFleetRange(0x08, 0x10);
constexpr FamilyRange kTroopRange(0x10, 0x14);
constexpr FamilyRange kFighterRange(0x1c, 0x20);
constexpr FamilyRange kFacilityKindARange(0x22, 0x28);
constexpr FamilyRange kFacilityKindBRange(0x28, 0x30);
constexpr FamilyRange kManagerRange(0xa0, 0xb0);
constexpr FamilyRange kRoleRange(0x30, 0x40);
constexpr uint32_t kDefaultFleetKey = 0x08000004;
constexpr uint32_t kOnHiddenMission = role_flag::kOnHiddenMission;

int addFrom(ChildIterator& iterator, ObjectKeyValueSet* out, int side) { return AddChildKeys(&iterator, out, 1, static_cast<uint32_t>(side)); }

}  // namespace

GameObjData* System::CreateData() { return NewSystemData(); }

// What a request (`context`, an ActivityRequest) needs in order to be carried out here, keyed by the type bits of
// `value`: the system's own key, or the keys of the children of the matching families.
int System::CollectDependencies(void* context, int value, ObjectKeyValueSet* out) {
    const ActivityRequest* request = static_cast<const ActivityRequest*>(context);
    out->Clear();
    const int side = request->m_side == 1 ? 1 : request->m_side == 2 ? 2 : 0;
    int result = side != 0;
    const uint32_t type = static_cast<uint32_t>(value) & kBothSidesBit;
    if (type == 0) {
        ObjectKey key;
        GetKey(&key);
        return out->AddKeyValue(&key, 0);
    }
    switch (type) {
        case 4: {
            const int kind = request->m_kind;
            if (kind == 0x200) {
                if (result) {
                    ChildIterator it(this, &kFacilityKindBRange, kAlwaysState);
                    result = addFrom(it, out, side);
                }
            } else if (kind == 0x214 || kind == 0x216) {
                if (result) {
                    ChildIterator it(this, &kManagerRange, kAlwaysState);
                    result = addFrom(it, out, side);
                }
            }
            break;
        }
        case 8: {
            const int kind = request->m_kind;
            if (kind == 0x200) {
                if (result) {
                    ChildIterator troops(this, &kTroopRange, kAlwaysState);
                    int added = addFrom(troops, out, side);
                    result = 0;
                    if (added) {
                        ChildIterator fighters(this, static_cast<uint32_t>(side), &kFighterRange, kAlwaysState);
                        added = addFrom(fighters, out, side);
                        result = 0;
                        if (added) {
                            ChildIterator facilities(this, static_cast<uint32_t>(side), &kFacilityKindARange, kAlwaysState);
                            result = addFrom(facilities, out, side);
                        }
                    }
                }
            } else if (kind == 0x201 || kind == 0x202) {
                if (result) {
                    ChildIterator troops(this, &kTroopRange, kAlwaysState);
                    int added = addFrom(troops, out, side);
                    result = 0;
                    if (added) {
                        ChildIterator fighters(this, &kFighterRange, kAlwaysState);
                        added = addFrom(fighters, out, side);
                        result = 0;
                        if (added) {
                            RoleIterator roles(this, kAlwaysState);
                            result = addFrom(roles, out, side);
                        }
                    }
                }
            } else if (kind == 0x240 || kind == 0x242) {
                if (result) {
                    RoleIterator roles(this, kAlwaysState);
                    result = addFrom(roles, out, side);
                }
            }
            break;
        }
        case 0x10:
            if (result) {
                ChildIterator fleets(this, &kFleetRange, kAlwaysState);
                result = addFrom(fleets, out, side);
            }
            break;
        case 0x40:
            if (result) {
                RoleIterator roles(this, kAlwaysState, static_cast<uint32_t>(side));
                for (roles.Begin(); roles.Current(); roles.Advance()) {
                    Role* role = static_cast<Role*>(roles.Current());
                    const ObjectKey key = role->m_missionKey;
                    if (key.IsSet() && !(role->m_flags78 & kOnHiddenMission) && !out->Find(&key)) out->AddKeyValue(&key, 0);
                }
            }
            break;
        default:
            result = 0;
            break;
    }
    return result;
}

// Creates the fleet with the fixed key 0x08000004 under this system and marks it as deployed or as arriving.
int System::CreateDefaultFleet(int initArg, int arrived, NotifyContext* ctx) {
    ObjectKey key;
    key.m_value = kDefaultFleetKey;
    GameObj* fleet = CreateChild(&key, initArg, ctx, 0, 0);
    int result = 0;
    if (fleet && static_cast<Fleet*>(fleet)->SetFlag01(0, ctx)) result = arrived ? static_cast<Fleet*>(fleet)->SetFlag02(1, ctx) : static_cast<Fleet*>(fleet)->SetFlag04(1, ctx);
    return result;
}

SystemIterator::SystemIterator(GameObjNotifier* parent) : ChildIterator(parent, &family::kSystems, 1) {}

SystemIterator::SystemIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kSystems, stateFilter) {}

SystemIterator::~SystemIterator() { REB_KEEP_VPTR_STORE(); }
