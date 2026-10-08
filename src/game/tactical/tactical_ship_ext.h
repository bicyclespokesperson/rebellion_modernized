#pragma once
#include <cstdint>

#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_ship.h"

// Functions and objects outside the unit modules that the capital ship / fighter / Death Star code calls. They are
// declared by their original addresses; their owners (ship groups and AI 0x59cd60-0x5a7500 / 0x5c5b30-0x5d2360, object
// manager 0x5a7500-0x5af4e0, turn sync 0x5bbf10) drop the declarations when they name them.

class TacticalObjectManager;
class TacticalRegistry;
class TacticalHub;

// Weapon profile objects (0x14 bytes each) inside a weapon arc.
void __thiscall FUN_005d2530(void* profile);
void __thiscall FUN_005d24e0(void* profile);
void __thiscall FUN_005d2490(void* profile);
void __thiscall FUN_005d24d0(void* profile);
void __thiscall FUN_005d2520(void* profile);
void __thiscall FUN_005d2570(void* profile);

// Weapon profile streaming (visual-resources module): read / write / checksum of the four factors.
void __thiscall FUN_005d23b0(TacticalWeaponProfile* profile, istream* in);
void __thiscall FUN_005d2400(TacticalWeaponProfile* profile, ostream* out);
void __thiscall FUN_005d2450(TacticalWeaponProfile* profile, Crc32* crc);

// Commander skill of a side as the object manager (3D battle) reports it.
double __thiscall FUN_005ad750(TacticalObjectManager* manager, int side);
double __thiscall FUN_005ad7e0(TacticalObjectManager* manager, int side);

// The object manager's random number between two bounds (rand() % (high - low + 1) + low).
int __thiscall FUN_005a8a70(TacticalObjectManager* manager, int low, int high);

// A ship group (owned by the ship-group module): an object with a vtable; only the slots and fields the unit code
// uses are named.
class TacticalGroupView {
public:
    virtual void Slot00();
    virtual void Slot01();
    virtual void Slot02();
    virtual void Slot03();
    virtual void Slot04();
    virtual void Slot05();
    virtual void Slot06();
    virtual void Slot07();
    virtual void Slot08();
    virtual void Slot09();
    virtual void Slot10();
    virtual void Slot11();
    virtual void RemoveMember(int handle);   // slot 12 (+0x30)
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void ForgetMember(int handle);   // slot 21 (+0x54)

    uint8_t m_pad04[0x14];
    int m_id;               // +0x18
    uint8_t m_pad1c[0x34];
    int m_side;             // +0x50
    uint8_t m_pad54[0x18];
    int m_memberCount;      // +0x6c
};

// Removes a unit from its group.
void __stdcall FUN_005caed0(TacticalGroupView* group, int handle);

// Gives a unit a fresh handle for its side (0x5a6620 + IdNode::SetId); `unit` is the TacticalUnit subobject.
void __thiscall FUN_005a6660(TacticalUnit* unit, int side);

// Adds a battle unit (the object, not its TacticalUnit subobject) to the registry.
uint32_t __thiscall FUN_005a64e0(TacticalRegistry* registry, TacticalShipBase* ship);

// The registry object (DAT_006bc470): only the Death Star pointer.
struct TacticalRegistryView {
    uint8_t m_pad00[0x44];
    TacticalUnit* m_deathStar;  // +0x44
};

// The tactical sound manager (presentation): created on first use.
class TacticalSoundManager;
TacticalSoundManager* FUN_005ba8b0();
void __thiscall FUN_005ba980(TacticalSoundManager* manager, int sound, int flag);

// ---- object manager / registry / hub calls made by the event handlers and unit methods ----------------------------------
// Whether the unit is controlled on this machine (RET 4).
int __thiscall FUN_005ad870(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a90c0(TacticalObjectManager* manager, TacticalShipBase* ship);
void __thiscall FUN_005a9130(TacticalObjectManager* manager, TacticalShipBase* ship);
// The Death Star the 3D battle knows (the registry's); `manager` is only passed through.
TacticalUnit* __thiscall FUN_005a9330(TacticalObjectManager* manager);
TacticalGroupView* __thiscall FUN_005a6460(TacticalRegistry* registry, int handle);
TacticalGroupView* __thiscall FUN_005a6470(TacticalRegistry* registry, int handle);
int __thiscall FUN_005cb980(TacticalGroupView* group);
// Removes a member from its group and unregisters the group when it is empty (cdecl).
void FUN_005caef0(TacticalGroupView* group, int handle);
void __thiscall FUN_0059f4d0(TacticalHub* hub, TacticalShipBase* ship);
void __thiscall FUN_005a5cf0(TacticalHub* hub, int attacker, int target);

// 3D presentation hooks (effects, camera, sound): called as the original does.
void FUN_005c12d0(int target, int source, int kind, float volume, int level);
void FUN_005c13d0(int unit, int kind, int sound);
void FUN_005c1320(int unit, int on);
void FUN_005c1360(int unit, int on);
void FUN_005c1200(int unit);
