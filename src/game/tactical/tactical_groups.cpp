#include "tactical/tactical_groups.h"

#include <new>

#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_unit_access.h"

namespace {

constexpr int kBehaviorSize = 0x3c;
constexpr float kDefaultBehaviorRange = 5.0f;
constexpr int kPositionOffset = 0x30;
constexpr int kChannelAOffset = 0x11c;
constexpr int kChannelBOffset = 0x124;
constexpr int kUnitGroupIdBackReference = 0x178;
constexpr int kFighterGroupKind = 3;
constexpr int kCapitalGroupKind = 4;
constexpr int kMemberStateActive = 2;
constexpr int kMemberStateReturning = 6;
constexpr int kMemberStateEngaging = 4;
constexpr int kBeginPlayerSideOffset = 0x18;

SimUnit* shipByHandle(int handle) { return GetTacticalRegistry()->FindShip(handle); }

SimUnitNode* firstUnitNode(UnitRefTree& tree) { return reinterpret_cast<SimUnitList*>(&tree)->First(); }

TacticalVec4 positionOf(SimUnit* unit) {
    return *reinterpret_cast<TacticalVec4*>(reinterpret_cast<char*>(unit) + kPositionOffset);
}

int firstMemberOtherThan(UnitRefTree* members, int handle) {
    UnitRefNode* node = static_cast<UnitRefNode*>(members->FirstNode());
    if (node) {
        do {
            if (node->Key() != handle) break;
            node = static_cast<UnitRefNode*>(node->Next());
        } while (node);
        if (node) return node->Key();
    }
    return 0;
}

// The order's own second virtual function must be called through its real vtable (the stubs of the classes only
// complete the vtable that the original one replaces).
__attribute__((noinline)) void startOrder(GroupOrder* order) { order->Virtual02(); }

template <class Order>
Order* newOrder(ShipGroup* group) {
    void* memory = ::operator new(sizeof(Order));
    return memory ? new (memory) Order(group) : nullptr;
}

}  // namespace

// ---- behavior -------------------------------------------------------------------------------------------------------

BehaviorLink::~BehaviorLink() {}

GroupBehavior::GroupBehavior(ShipGroup* group) {
    m_unk04 = 0;
    m_unk14 = 0;
    m_unk10 = 0;
    m_range = kDefaultBehaviorRange;
    m_group = group;
    m_unk30 = 0;
    m_rangeNow = m_range;
    m_unk38 = 0;
}

GroupBehavior::~GroupBehavior() {}

ShipGroupBehavior::ShipGroupBehavior(ShipGroup* group) : GroupBehavior(group) { m_unk04 = 2; }

ShipGroupBehavior::~ShipGroupBehavior() {}

// ---- orders ---------------------------------------------------------------------------------------------------------

GroupOrder::GroupOrder(ShipGroup* group) {
    m_unk14 = 0;
    m_unk0c = 0;
    m_unk10 = 0;
    m_unk04 = 0;
    m_group = group;
}

GroupOrder::~GroupOrder() {}

#define REB_DEFINE_GROUP_ORDER(Name) \
    Name::Name(ShipGroup* group) : GroupOrder(group) {}

REB_DEFINE_GROUP_ORDER(GroupOrderMode1)
REB_DEFINE_GROUP_ORDER(GroupOrderMode2)
REB_DEFINE_GROUP_ORDER(GroupOrderMode3)
REB_DEFINE_GROUP_ORDER(GroupOrderMode4)
REB_DEFINE_GROUP_ORDER(GroupOrderMode5)
REB_DEFINE_GROUP_ORDER(GroupOrderMode7)
REB_DEFINE_GROUP_ORDER(GroupOrderMode8)
REB_DEFINE_GROUP_ORDER(GroupOrderMode9)
REB_DEFINE_GROUP_ORDER(GroupOrderMode10)
REB_DEFINE_GROUP_ORDER(GroupOrderMode11)
#undef REB_DEFINE_GROUP_ORDER

// ---- ship groups ----------------------------------------------------------------------------------------------------

ShipGroup::ShipGroup(int seedHandle) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_unkb0 = 0;
    m_unkb4 = 0;
    m_unk38 = 2;
    m_unk3c = 2;
    void* memory = ::operator new(kBehaviorSize);
    m_behavior = memory ? new (memory) ShipGroupBehavior(this) : nullptr;
    m_order = nullptr;
    m_seedHandle = seedHandle;
    m_firstMember = 0;
    m_mode = 0;
    m_unk68 = 0;
    m_unk58 = 0;
    m_unk5c = 0;
    m_formation = nullptr;
    SimUnit* seed = shipByHandle(seedHandle);
    if (seed) {
        m_founders.AddUnit(seed);
        m_side = unitSide(seed);
        m_channelA.m_data = unitField<void*>(seed, kChannelAOffset);
        m_channelB.m_data = unitField<void*>(seed, kChannelBOffset);
    }
}

ShipGroup::ShipGroup(int seedHandle, int id) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_unkb0 = 0;
    m_unkb4 = 0;
    m_unk38 = 2;
    m_unk3c = 2;
    void* memory = ::operator new(kBehaviorSize);
    m_behavior = memory ? new (memory) ShipGroupBehavior(this) : nullptr;
    m_order = nullptr;
    m_seedHandle = seedHandle;
    m_firstMember = 0;
    m_mode = 0;
    m_unk68 = 0;
    m_unk58 = 0;
    m_unk5c = 0;
    m_formation = nullptr;
    SetId(id);
    SimUnit* seed = shipByHandle(seedHandle);
    if (seed) {
        m_founders.AddUnit(seed);
        unitField<int>(seed, kUnitGroupIdBackReference) = m_id;
        m_side = unitSide(seed);
        m_channelA.m_data = unitField<void*>(seed, kChannelAOffset);
        m_channelB.m_data = unitField<void*>(seed, kChannelBOffset);
    }
}

ShipGroup::~ShipGroup() {
    delete m_order;
    delete m_formation;
    delete m_behavior;
}

void ShipGroup::SetSeedHandle(int handle) {
    m_firstMember = handle;
    SimUnit* unit = shipByHandle(handle);
    if (unit) {
        AddUnit(unit);
        unitGroupId(unit) = m_id;
    }
}

void ShipGroup::AddMember(int handle) {
    SimUnit* unit = shipByHandle(handle);
    AddUnit(unit);
    if (m_firstMember == 0) m_firstMember = handle;
    m_formation->AddUnit(handle);
    unitGroupId(unit) = m_id;
}

void ShipGroup::RemoveMember(int handle) {
    SimUnit* unit = shipByHandle(handle);
    if (!unit) return;
    Find(handle);
    if (unitGroupId(unit) == static_cast<int>(m_id)) unitGroupId(unit) = 0;
    if (handle == m_firstMember) m_firstMember = firstMemberOtherThan(this, m_firstMember);
    RemoveUnit(unit);
    m_formation->RemoveUnit(handle);
}

float ShipGroup::MemberStrengthAgainst(SimUnit* target) {
    float total = 0.0f;
    for (SimUnitNode* node = firstUnitNode(*this); node; node = node->Next()) {
        if (unitState(node->Value()) == kMemberStateActive || unitState(node->Value()) == kMemberStateReturning ||
            unitState(node->Value()) == kMemberStateEngaging) {
            total = FUN_005b7780(node->Value(), target, 0) + total;
        }
    }
    return total;
}

void ShipGroup::SetMode(int mode) {
    delete m_order;
    const int previousMode = m_mode;
    m_order = nullptr;
    if (previousMode == 1) m_formationA.Clear();
    m_mode = mode;
    switch (mode) {
        case 1:
            m_targetsA.Clear();
            if (TacticalUnitKind(this) == kFighterGroupKind) FUN_005cb1a0(this, 1);
            m_order = newOrder<GroupOrderMode1>(this);
            return;
        case 2:
            if (TacticalUnitKind(this) != kCapitalGroupKind) {
                m_mode = previousMode;
                return;
            }
            if (m_side == GetObjectManager()->m_playerSide) {
                TacticalHub* hub = FUN_0059ed70();
                FUN_005a0320(hub, m_id);
            }
            m_targetsA.Clear();
            m_formationA.Clear();
            m_order = newOrder<GroupOrderMode2>(this);
            return;
        case 3: {
            m_targetsA.Clear();
            m_formationA.Clear();
            GroupOrder* order = newOrder<GroupOrderMode3>(this);
            m_order = order;
            startOrder(order);
            return;
        }
        case 4:
            m_targetsA.Clear();
            m_formationA.Clear();
            m_order = newOrder<GroupOrderMode4>(this);
            return;
        case 5:
            m_targetsA.Clear();
            m_formationA.Clear();
            m_order = newOrder<GroupOrderMode5>(this);
            return;
        case 6: {
            if (TacticalUnitKind(this) == kCapitalGroupKind && m_side == 0) {
                m_targetsA.Clear();
                m_formationA.Clear();
                void* memory = ::operator new(0x84);
                m_order = memory ? FUN_005d0820(memory, this) : nullptr;
                TacticalSideData* side = GetObjectManager()->m_sides[0];
                *reinterpret_cast<int*>(reinterpret_cast<char*>(side) + kBeginPlayerSideOffset) = 0;
                return;
            }
            m_mode = previousMode;
            return;
        }
        case 7:
            m_targetsA.Clear();
            m_order = newOrder<GroupOrderMode7>(this);
            return;
        case 8:
            m_targetsA.Clear();
            m_order = newOrder<GroupOrderMode8>(this);
            return;
        case 9:
            m_targetsA.Clear();
            m_order = newOrder<GroupOrderMode9>(this);
            return;
        case 10:
            m_targetsA.Clear();
            m_order = newOrder<GroupOrderMode10>(this);
            return;
        case 11:
            m_targetsA.Clear();
            m_order = newOrder<GroupOrderMode11>(this);
            return;
        default:
            return;
    }
}

CapitalShipGroup::CapitalShipGroup(int seedHandle) : ShipGroup(seedHandle) {
    m_role = 4;
    m_unkc0 = 0;
    SimUnit* seed = shipByHandle(seedHandle);
    if (seed) {
        void* memory = ::operator new(0x144);
        m_formation = memory ? FUN_005c72b0(memory, seedHandle, positionOf(seed)) : nullptr;
        m_formation->AddUnit(seedHandle);
    }
    m_unk3c = 2;
    m_unk38 = 2;
    m_unk58 = 3;
    m_unk5c = 3;
    m_unk68 = 0;
    SetMode(0);
}

CapitalShipGroup::~CapitalShipGroup() {}

void CapitalShipGroup::SetRole(int role) { m_role = role; }

void CapitalShipGroup::SetMode(int mode) { ShipGroup::SetMode(mode); }

FighterGroup::FighterGroup(int seedHandle) : ShipGroup(seedHandle) {
    SimUnit* seed = shipByHandle(seedHandle);
    void* memory = ::operator new(0x140);
    m_formation = memory ? FUN_005ec310(memory, seedHandle, positionOf(seed)) : nullptr;
    m_formation->AddUnit(seedHandle);
    m_unk3c = 2;
    m_unk38 = 2;
    m_unk58 = 2;
    m_unk5c = 2;
}

FighterGroup::~FighterGroup() {}
