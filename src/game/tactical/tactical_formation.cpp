#include "tactical/tactical_formation.h"
#include "core/reb_x87.h"

#include <new>

#include "tactical/tactical_facade.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_ship.h"

namespace {

constexpr unsigned kSecondSideFirstHandle = 1000000;
constexpr float kQuarterTurn = 0.5f;
constexpr float kPi = 3.1415927f;
constexpr float kRadiusScale = 1.2f;
constexpr float kBaseRadius = 3.0f * 4.0f;
constexpr float kSlotStepScale = -1.5f;
constexpr float kTurnA = 0.6666667f;
constexpr float kTurnC = 0.4f;
constexpr float kDefaultDepth = 4.0f;
}  // namespace

// ---- slots and the trees that hold them ------------------------------------------------------------------------------

FormationSlot::FormationSlot(TacticalShipBase* unit) : UnitRefNode(unit) {}

FormationSlot::~FormationSlot() {}

FormationSlotTree::~FormationSlotTree() {}

FormationRankTree::~FormationRankTree() {}

int FormationRankTree::IsLess(AvlNode* a, AvlNode* b) {
    struct RankKey {
        uint8_t m_node[0x18];
        float m_primary;    // +0x18
        float m_secondary;  // +0x1c
    };
    const RankKey* left = reinterpret_cast<const RankKey*>(a);
    const RankKey* right = reinterpret_cast<const RankKey*>(b);
    if (left->m_primary == right->m_primary) {
        if (left->m_secondary < right->m_secondary) return 1;
    } else if (left->m_primary < right->m_primary) {
        return 1;
    }
    return 0;
}

// ---- the formation -----------------------------------------------------------------------------------------------------

FormationBase::FormationBase(int groupHandle, TacticalVector position) {
    m_position.CopyFrom(&position);
    m_unk124 = 0;
    m_unk128 = kDefaultDepth;
    m_count = 0;
    m_maxCount = 0;
    m_unk134 = 0;
    m_radius = kBaseRadius;
    m_leaderHandle = groupHandle;
    m_forward.m_x = 0;
    m_forward.m_y = 0;
    if (static_cast<unsigned>(groupHandle) < kSecondSideFirstHandle) {
        m_forward.m_z = 1.0f;
        m_forward.m_cachedLength = -1.0f;
        m_angle = kPi * kQuarterTurn;
    } else {
        m_forward.m_z = -1.0f;
        m_forward.m_cachedLength = -1.0f;
        m_angle = kPi * -kQuarterTurn;
    }
}

FormationBase::~FormationBase() {}

int FormationBase::AddMember(int handle) {
    TacticalShipBase* ship = GetTacticalRegistry()->FindShip(handle);
    if (!ship) return 0;
    AddUnit(ship);
    const unsigned count = m_count + 1;
    m_count = count;
    if (!(m_maxCount > count)) m_maxCount = count;
    const double scaledRadius = static_cast<double>(ship->m_radius) * static_cast<double>(kRadiusScale);
    const float radius = m_radius;
    m_radius = static_cast<float>(radius > scaledRadius ? static_cast<double>(radius) : scaledRadius);

    if (count == 1) {
        m_position = ship->m_position;
        m_leaderHandle = handle;
    }
    void* memory = ::operator new(sizeof(FormationSlot));
    FormationSlot* slot = memory ? new (memory) FormationSlot(ship) : nullptr;
    TacticalVector offset;
    slot->m_position = *SlotOffset(&offset, ship);
    m_slotsByUnit.AddUniqueLinear(slot);
    return 1;
}

int FormationBase::RemoveMember(int handle) {
    TacticalShipBase* ship = GetTacticalRegistry()->FindShip(handle);
    if (!ship) return 0;
    const int shipHandle = static_cast<int>(ship->m_id);
    if (m_leaderHandle == shipHandle) {
        Find(shipHandle);
        UnitRefNode* node = static_cast<UnitRefNode*>(FirstNode());
        bool found = false;
        if (node) {
            do {
                if (node->Key() != m_leaderHandle) break;
                node = static_cast<UnitRefNode*>(node->Next());
            } while (node);
            if (node) {
                m_leaderHandle = node->Key();
                found = true;
            }
        }
        if (!found) m_leaderHandle = 0;
    }
    if (RemoveUnit(ship) == 1) --m_count;
    IdNode* slot = m_slotsByUnit.FindLinear(static_cast<int>(ship->m_id));
    if (slot) m_slotsByUnit.RemoveLinear(static_cast<UnitRefNode*>(slot)->Key());
    if (m_slotsByUnit2.FindLinear(static_cast<int>(ship->m_id))) m_slotsByUnit2.RemoveLinear(static_cast<int>(ship->m_id));
    if (m_count == 1) m_leaderHandle = static_cast<int>(FirstUnit()->m_id);
    return 1;
}

CapitalShipFormation::CapitalShipFormation(int groupHandle, TacticalVector position)
    : FormationBase(groupHandle, position) {
    m_unk140 = 0;
}

CapitalShipFormation::~CapitalShipFormation() {}

void CapitalShipFormation::Virtual06() {}

// The offset of the Nth unit of a capital-ship formation: units alternate to either side of the leader in a column of
// rotated slots. The original evaluates the trigonometry on the x87 stack (cosines from the extended product, sines from
// the argument rounded to float); doubles stand in for the extended values here.
TacticalVector* CapitalShipFormation::SlotOffset(TacticalVector* out, TacticalShipBase* unit) {
    if (m_count == 1) {
        *out = g_formationOrigin;
        return out;
    }
    const int side = (m_count & 1) ? 1 : -1;
    const int step = reb_ftol(side * 3.0f * kSlotStepScale);
    const int slot = 1 - step * (static_cast<int>(m_count) >> 1);
    const double turn = static_cast<double>(slot) * static_cast<double>(kPi);
    const double angleA = turn * static_cast<double>(kTurnA);
    const double angleB = turn * static_cast<double>(kQuarterTurn);
    const double angleC = turn * static_cast<double>(kTurnC);
    const double cosA = reb_cos(angleA);
    const double sinA = reb_sin(static_cast<double>(static_cast<float>(angleA)));
    const double cosB = reb_cos(angleB);
    const double sinB = reb_sin(static_cast<double>(static_cast<float>(angleB)));
    const double cosC = reb_cos(angleC);
    const double sinC = reb_sin(static_cast<double>(static_cast<float>(angleC)));
    const float headingX = m_forward.m_x;
    const float headingY = m_forward.m_y;
    const float headingZ = m_forward.m_z;

    const float rotatedZ = static_cast<float>(static_cast<double>(static_cast<float>(sinA)) * headingX + cosA * headingZ);
    const float rotatedX = static_cast<float>(cosA) * headingX - static_cast<float>(sinA) * headingZ;
    const float rotatedY = static_cast<float>(sinB * static_cast<double>(rotatedZ) + cosB * headingY);
    out->m_z = static_cast<float>(cosB) * rotatedZ - static_cast<float>(sinB) * headingY;
    out->m_cachedLength = -1.0f;
    out->m_y = static_cast<float>(cosC) * rotatedX - static_cast<float>(sinC) * rotatedY;
    out->m_x = static_cast<float>(sinC) * rotatedX + static_cast<float>(cosC) * rotatedY;
    return out;
}

FighterFormation::FighterFormation(int groupHandle, TacticalVector position) : FormationBase(groupHandle, position) {
    m_turnAxis = TacticalVector(2 * kPi, 2 * kPi, 2 * kPi);
    m_position = position;
}

FighterFormation::~FighterFormation() {}

void FighterFormation::Virtual06() {}

TacticalVector* FighterFormation::SlotOffset(TacticalVector* out, TacticalShipBase* unit) {
    if (m_count == 1) {
        *out = g_formationOrigin;
        return out;
    }
    const int side = (m_count & 1) ? 1 : -1;
    unsigned widest = m_maxCount;
    if (!(widest > m_count)) widest = m_count;
    const double scaled = static_cast<double>(unit->m_radius) * static_cast<double>(kRadiusScale);
    const float radius = m_radius;
    const double reach = radius > scaled ? static_cast<double>(radius) : scaled;
    int steps = reb_ftol(side * reach);
    steps = steps * (static_cast<int>(widest) >> 1);
    out->m_x = -(m_forward.m_z * static_cast<float>(steps));
    out->m_y = 0.0f;
    out->m_z = m_forward.m_x * static_cast<float>(steps);
    out->m_cachedLength = -1.0f;
    return out;
}
