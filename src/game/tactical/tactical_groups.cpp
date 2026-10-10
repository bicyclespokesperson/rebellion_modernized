#include "core/reb_abi.h"
#include "tactical/tactical_groups.h"

#include <new>

#include "tactical/tactical_externals.h"
#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_ship.h"

namespace {

constexpr float kDefaultBehaviorRange = 5.0f;
constexpr int kFighterGroupKind = 3;
constexpr int kCapitalGroupKind = 4;
constexpr int kMemberStateActive = 2;
constexpr int kMemberStateReturning = 6;
constexpr int kMemberStateEngaging = 4;
constexpr int kBeginPlayerSideOffset = 0x18;

TacticalShipBase* shipByHandle(int handle) { return GetTacticalRegistry()->FindShip(handle); }

UnitRefNode* firstUnitNode(UnitRefTree& tree) { return tree.FirstNode(); }

int firstMemberOtherThan(UnitRefTree* members, int handle) {
    UnitRefNode* node = members->FirstNode();
    if (node) {
        do {
            if (node->Key() != handle) break;
            node = node->Next();
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
    void* memory = ::operator new(sizeof(ShipGroupBehavior));
    m_behavior = memory ? new (memory) ShipGroupBehavior(this) : nullptr;
    m_order = nullptr;
    m_seedHandle = seedHandle;
    m_firstMember = 0;
    m_mode = 0;
    m_unk68 = 0;
    m_unk58 = 0;
    m_unk5c = 0;
    m_formation = nullptr;
    TacticalShipBase* seed = shipByHandle(seedHandle);
    if (seed) {
        m_founders.AddUnit(seed);
        m_side = seed->m_side;
        m_channelA.m_data = seed->m_channelA.m_data;
        m_channelB.m_data = seed->m_channelB.m_data;
    }
}

ShipGroup::ShipGroup(int seedHandle, int id) {
    m_channelA.m_data = nullptr;
    m_channelB.m_data = nullptr;
    m_unkb0 = 0;
    m_unkb4 = 0;
    m_unk38 = 2;
    m_unk3c = 2;
    void* memory = ::operator new(sizeof(ShipGroupBehavior));
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
    TacticalShipBase* seed = shipByHandle(seedHandle);
    if (seed) {
        m_founders.AddUnit(seed);
        seed->m_unk178 = m_id;
        m_side = seed->m_side;
        m_channelA.m_data = seed->m_channelA.m_data;
        m_channelB.m_data = seed->m_channelB.m_data;
    }
}

ShipGroup::~ShipGroup() {
    delete m_order;
    delete m_formation;
    delete m_behavior;
}

void ShipGroup::SetSeedHandle(int handle) {
    m_firstMember = handle;
    TacticalShipBase* unit = shipByHandle(handle);
    if (unit) {
        AddUnit(unit);
        unit->m_groupHandle = m_id;
    }
}

void ShipGroup::AddMember(int handle) {
    TacticalShipBase* unit = shipByHandle(handle);
    AddUnit(unit);
    if (m_firstMember == 0) m_firstMember = handle;
    m_formation->AddMember(handle);
    unit->m_groupHandle = m_id;
}

void ShipGroup::RemoveMember(int handle) {
    TacticalShipBase* unit = shipByHandle(handle);
    if (!unit) return;
    Find(handle);
    if (unit->m_groupHandle == static_cast<int>(m_id)) unit->m_groupHandle = 0;
    if (handle == m_firstMember) m_firstMember = firstMemberOtherThan(this, m_firstMember);
    RemoveUnit(unit);
    m_formation->RemoveMember(handle);
}

float ShipGroup::MemberStrengthAgainst(TacticalShipBase* target) {
    float total = 0.0f;
    for (UnitRefNode* node = firstUnitNode(*this); node; node = node->Next()) {
        if (node->Value()->m_state == kMemberStateActive || node->Value()->m_state == kMemberStateReturning ||
            node->Value()->m_state == kMemberStateEngaging) {
            total = node->Value()->TacticalShipBase::AttackRating(target, 0) + total;
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
    TacticalShipBase* seed = shipByHandle(seedHandle);
    if (seed) {
        void* memory = ::operator new(sizeof(CapitalShipFormation));
        m_formation = memory ? new (memory) CapitalShipFormation(seedHandle, seed->m_position) : nullptr;
        m_formation->AddMember(seedHandle);
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
    TacticalShipBase* seed = shipByHandle(seedHandle);
    void* memory = ::operator new(sizeof(FighterFormation));
    m_formation = memory ? new (memory) FighterFormation(seedHandle, seed->m_position) : nullptr;
    m_formation->AddMember(seedHandle);
    m_unk3c = 2;
    m_unk38 = 2;
    m_unk58 = 2;
    m_unk5c = 2;
}

FighterGroup::~FighterGroup() {}

void REB_STDCALL RemoveMemberFromGroup(ShipGroup* group, int handle) {
    if (group) group->RemoveMember(handle);
}

void REB_STDCALL RegisterFighterGroup(ShipGroup* group) {
    group->AssignHandle(group->m_side);
    GetTacticalRegistry()->AddFighterGroup(group);
}

void REB_STDCALL RegisterCapitalShipGroup(ShipGroup* group) {
    group->AssignHandle(group->m_side);
    GetTacticalRegistry()->AddCapitalShipGroup(group);
}

void DestroyTacticalHub() {
    TacticalHub* hub = g_tacticalHub;
    if (hub != nullptr) {
        FUN_0059ee10(hub);
        ::operator delete(static_cast<void*>(hub));
    }
    g_tacticalHub = nullptr;
}

int CapitalShipGroup::Virtual01() { return 4; }

int FighterGroup::Virtual01() { return 3; }
