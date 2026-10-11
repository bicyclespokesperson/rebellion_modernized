#pragma once
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"

class System;

// The planetary bombardment (command 0x224, see SystemCommand224). The bombarding fleets (Bombard flag set) and the
// system's defences shoot at each other once; what the request kind (0x220-0x223) lets the bombardment reach is kept
// in the walker: the defence facilities and the garrison, the production facilities, the energy and raw material of the
// system, and (0x223, with a Death Star) the planet itself.
//
//   BombardResolver (VT 0x66a950, 0x48 bytes)   runs the four phases
//   BombardWalker   (VT 0x66a910, 0x40 bytes)   the state; walks the bombarding ships or the things that can be hit
//   ShipVisitor / DefenderVisitor               one pass over the ships / over the defenders; subclasses below
class BombardWalker;

// What a pass over the defenders found: exactly one of the four is set.
struct DefenderRef {
    GameObjNotifier* m_facility;  // +0x00 a defence, production or other facility
    GameObjNotifier* m_troop;     // +0x04 a garrison troop
    int m_energy;                 // +0x08 the system's energy
    int m_rawMaterial;            // +0x0c the system's raw material
};

class ShipVisitor {
public:
    ShipVisitor(BombardWalker* walker);  // 0x58c960
    virtual ~ShipVisitor();              // 0x58c9a0 (scalar deleting destructor 0x58c980)
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) = 0;  // 1

    BombardWalker* m_walker;  // +0x04
};

class DefenderVisitor {
public:
    DefenderVisitor(BombardWalker* walker);  // 0x58c9b0
    virtual ~DefenderVisitor();              // 0x58c9f0 (scalar deleting destructor 0x58c9d0)
    // Exactly one of facility, troop, energy, rawMaterial is set.
    virtual int Visit(GameObj* facility, GameObj* troop, int energy, int rawMaterial, uint32_t* stop,
                      NotifyContext* ctx) = 0;  // 1

    BombardWalker* m_walker;  // +0x04
};

class BombardWalker {
public:
    BombardWalker();           // 0x58ca00
    virtual ~BombardWalker();  // 0x58ca60 (scalar deleting destructor 0x58ca40)

    int ForEachBombardingShip(ShipVisitor* visitor, NotifyContext* ctx);   // 0x58ca70
    int ForEachDefender(DefenderVisitor* visitor, NotifyContext* ctx);     // 0x58cba0
    // Destroys `object` (an attacker's own facility counts as friendly fire); a facility marks the system as damaged.
    int DestroyDefender(GameObj* object, NotifyContext* ctx);              // 0x58ce70
    int DestroyEnergy(NotifyContext* ctx);                                 // 0x58cf20, one point of energy
    int DestroyRawMaterial(NotifyContext* ctx);                            // 0x58cf50, one point of raw material
    // How much the defender is worth against the bombardment (the `this` is not used).
    int GetDefenderStrength(const DefenderRef* ref, int* out);             // 0x58cf80
    int DestroyDefenderRef(const DefenderRef* ref, NotifyContext* ctx);    // 0x58d020
    // A random production facility of the system. BUG(original): see the definition.
    int PickRandomProcFacility(int* found, DefenderRef* ref);              // 0x58d080

    int m_unused4;             // +0x04, 1
    System* m_system;          // +0x08
    uint32_t m_attackerSide;   // +0x0c
    uint32_t m_defenderSide;   // +0x10, the side that controlled the system
    int m_shipCount;           // +0x14 bombarding ships
    int m_defenderCount;       // +0x18 things that can be hit
    int m_collateral;          // +0x1c copy of m_collateralAllowed taken at the start
    int m_damaged;             // +0x20 something of the system was destroyed
    int m_planetDestroyed;     // +0x24
    int m_shipsGone;           // +0x28 no bombarding ship or no defender is left
    int m_hitDefenses;         // +0x2c request kinds 0x220, 0x222, 0x223
    int m_hitProduction;       // +0x30 request kinds 0x221, 0x222, 0x223
    int m_hitResources;        // +0x34 request kinds 0x222, 0x223
    int m_collateralAllowed;   // +0x38 request kind 0x220: a production facility may be hit too
    int m_deathStar;           // +0x3c request kind 0x223
};

class ShipCounter : public ShipVisitor {
public:
    ShipCounter(BombardWalker* walker);  // 0x58d150
    virtual ~ShipCounter();              // 0x58d190 (scalar deleting destructor 0x58d170)
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override;  // 1: 0x58d1a0

    int m_count;  // +0x08
};

class ShipPicker : public ShipVisitor {
public:
    ShipPicker(BombardWalker* walker);  // 0x58d230
    virtual ~ShipPicker();              // 0x58d280 (scalar deleting destructor 0x58d260)
    // Shared (ICF) with UnitPicker::Visit.
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override;  // 1: 0x587470

    int m_index;          // +0x08
    int m_target;         // +0x0c
    GameObj* m_ship;      // +0x10
    GameObj* m_fleet;     // +0x14
};

// Stops at the first Death Star.
class DeathStarFinder : public ShipVisitor {
public:
    DeathStarFinder(BombardWalker* walker);  // 0x58d3f0
    virtual ~DeathStarFinder();              // 0x58d430 (scalar deleting destructor 0x58d410)
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override;  // 1: 0x58d440

    int m_found;  // +0x08
};

// Moves the damage the defences dealt to a ship into its hull, shields and critical hits.
class ShipDamager : public ShipVisitor {
public:
    ShipDamager(BombardWalker* walker);  // 0x58d4f0
    virtual ~ShipDamager();              // 0x58d530 (scalar deleting destructor 0x58d510)
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override;  // 1: 0x58d540
};

// Forgets the damage the defences have dealt.
class ShipCleaner : public ShipVisitor {
public:
    ShipCleaner(BombardWalker* walker);  // 0x58d680
    virtual ~ShipCleaner();              // 0x58d6c0 (scalar deleting destructor 0x58d6a0)
    virtual int Visit(GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override;  // 1: 0x58d6d0
};

class DefenderCounter : public DefenderVisitor {
public:
    DefenderCounter(BombardWalker* walker);  // 0x58d1c0
    virtual ~DefenderCounter();              // 0x58d200 (scalar deleting destructor 0x58d1e0)
    virtual int Visit(GameObj* facility, GameObj* troop, int energy, int rawMaterial, uint32_t* stop,
                      NotifyContext* ctx) override;  // 1: 0x58d210

    int m_count;  // +0x08
};

class DefenderPicker : public DefenderVisitor {
public:
    DefenderPicker(BombardWalker* walker);  // 0x58d290
    virtual ~DefenderPicker();              // 0x58d2e0 (scalar deleting destructor 0x58d2c0)
    virtual int Visit(GameObj* facility, GameObj* troop, int energy, int rawMaterial, uint32_t* stop,
                      NotifyContext* ctx) override;  // 1: 0x58d2f0
    // *found = whether anything was picked; *ref = what.
    void GetPicked(int* found, DefenderRef* ref);  // 0x58d370

    int m_index;             // +0x08
    int m_target;            // +0x0c
    DefenderRef m_picked;    // +0x10, +0x20
};

class BombardResolver {
public:
    BombardResolver();           // 0x58d720
    virtual ~BombardResolver();  // 0x58d760 (scalar deleting destructor 0x58d740)

    // Works out what the request kind lets the bombardment hit and which side bombards, then runs the phases.
    int Run(GameObjNotifier* system, ActivityRequest* request, NotifyContext* ctx);  // 0x58d800
    int RunPhases(NotifyContext* ctx);                                               // 0x58d770

    int StartBombard(NotifyContext* ctx);         // 0x58d990, flags the system, counts ships and defenders
    int DefensesFireAtShips(NotifyContext* ctx);  // 0x58dc10, the defences hurt the bombarding ships
    int ShipsFireAtDefenders(NotifyContext* ctx); // 0x58df60, the surviving ships hurt the defenders
    int FinishBombard(NotifyContext* ctx);        // 0x58e320, loyalty falls out, flags are cleared

    int m_unused4;             // +0x04, 1
    BombardWalker m_walker;    // +0x08
};
