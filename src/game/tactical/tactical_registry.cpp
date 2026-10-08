#include "tactical/tactical_registry.h"

#include "tactical/tactical_externals.h"
#include "tactical/tactical_facade.h"
#include "tactical/tactical_game.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_simulator.h"

// TacticalUnit::Virtual01 returns the unit's kind; the declared signature in tactical_units.h is void, so the call goes
// through this view of the same vtable.
class UnitKindView {
public:
    virtual void Slot00();
    virtual int Kind();
};

namespace {

constexpr int kShipSubobjectOffset = 0xd4;
constexpr int kLaunchInterfaceOffset = 0x418;
constexpr int kUnitInterfaceOffset = 0x1c;
constexpr int kCharacterKind = 3;
constexpr int kSpecialForceKind = 4;
constexpr int kCapitalShipClass = 0;

int kindOf(TacticalUnit* unit) { return reinterpret_cast<UnitKindView*>(unit)->Kind(); }

template <class T>
T* atOffset(void* base, int offset) {
    return reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

IdNode* shipNode(SimUnit* ship) { return atOffset<IdNode>(ship, kShipSubobjectOffset); }

SimUnit* shipOfNode(IdNode* node) { return atOffset<SimUnit>(node, -kShipSubobjectOffset); }

}  // namespace

void* __thiscall FUN_005a9330(TacticalObjectManager* manager);  // the object manager's unit lookup for the launch interface

TacticalRegistry::TacticalRegistry() {
    m_deathStar = nullptr;
    for (uint32_t base = 0; base < 2000000; base += 1000000) {
        m_handleCounter[base / 1000000] = base;
        m_groupCounter[base / 1000000] = base;
    }
}

TacticalRegistry::~TacticalRegistry() {
    delete m_deathStar;
    m_deathStar = nullptr;
    g_tacticalRegistry = nullptr;
}

uint32_t TacticalRegistry::NextUnitHandle(unsigned side) {
    if (side < 2) return ++m_handleCounter[side];
    return 0;
}

uint32_t TacticalRegistry::NextGroupHandle(unsigned side) {
    if (side < 2) return ++m_groupCounter[side];
    return 0;
}

SimUnit* TacticalRegistry::FindShip(int handle) {
    IdNode* node = m_ships.FindById(handle);
    return node ? shipOfNode(node) : nullptr;
}

TacticalUnit* TacticalRegistry::FindLaunchInterface(int handle) {
    SimUnit* ship = FindShip(handle);
    if (!ship) {
        if (GetTacticalGame()->m_abstract == 0) {
            void* world = FUN_005a9330(GetObjectManager());
            return world ? atOffset<TacticalUnit>(world, kUnitInterfaceOffset) : nullptr;
        }
        GetHeadlessSimulator();
        TacticalUnit* deathStar = GetTacticalRegistry()->GetDeathStar();
        return deathStar ? atOffset<TacticalUnit>(deathStar, kUnitInterfaceOffset) : nullptr;
    }
    if (ship->GetUnitClass() == kCapitalShipClass) return atOffset<TacticalUnit>(ship, kLaunchInterfaceOffset);
    return nullptr;
}

IdNode* TacticalRegistry::FindShipOrGroup(int handle) {
    SimUnit* ship = FindShip(handle);
    if (ship) return shipNode(ship);
    return m_groups.FindById(handle);
}

IdNode* TacticalRegistry::FindGroup(int handle) { return m_groups.FindById(handle); }

IdNode* TacticalRegistry::FindCharacter(int handle) { return m_characters.FindById(handle); }

IdNode* TacticalRegistry::FindSpecialForce(int handle) { return m_specialForces.FindById(handle); }

IdNode* TacticalRegistry::FindCharacterOrSpecialForce(int handle) {
    IdNode* found = m_characters.FindById(handle);
    if (!found) found = m_specialForces.FindById(handle);
    return found;
}

IdNode* TacticalRegistry::RemoveCharacterOrSpecialForce(int handle) {
    IdNode* found = m_characters.DetachById(handle);
    if (!found) found = m_specialForces.DetachById(handle);
    return found;
}

uint32_t TacticalRegistry::AddShip(SimUnit* ship) { return m_ships.AddWithId(ship ? shipNode(ship) : nullptr); }

uint32_t TacticalRegistry::AddGroup(IdNode* group) { return m_groups.AddWithId(group); }

uint32_t TacticalRegistry::AddCharacter(IdNode* character) { return m_characters.AddWithId(character); }

uint32_t TacticalRegistry::AddSpecialForce(IdNode* force) { return m_specialForces.AddWithId(force); }

uint32_t TacticalRegistry::AddCharacterOrSpecialForce(TacticalUnit* unit) {
    if (kindOf(unit) == kCharacterKind) return m_characters.AddWithId(unit);
    if (kindOf(unit) == kSpecialForceKind) return m_specialForces.AddWithId(unit);
    return 0;
}

TacticalUnit* TacticalRegistry::GetDeathStar() { return m_deathStar; }

TacticalUnit::TacticalUnit() { SetId(0); }

TacticalUnit::~TacticalUnit() {}

void TacticalUnit::AssignHandle(int side) { SetId(GetTacticalRegistry()->NextUnitHandle(side)); }

void TacticalUnit::AssignGroupId(int side) { SetId(GetTacticalRegistry()->NextGroupHandle(side)); }
