#pragma once
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"

class System;

// The planetary assault (command 0x225, see SystemCommand225): the troops carried by the assaulting fleets (the
// fleets whose Assault flag is set) fight the troops that garrison the system, then the system's defence facilities
// shoot at the attackers and the survivors damage the system. The whole fight is resolved at once, round by round,
// with the AssaultWalker walking the troops and one TroopVisitor per kind of pass.
//
//   AssaultResolver (VT 0x66a8f8, 0x30 bytes)    owns the AssaultWalker and runs the phases in order
//   AssaultWalker   (VT 0x66a8d0, 0x28 bytes)    the state of the fight; walks the attacking or the defending troops
//   TroopVisitor    (VT 0x66a8c8)                one pass over the troops; subclasses below
class AssaultWalker;

class TroopVisitor
{
public:
  TroopVisitor(AssaultWalker* walker); // 0x58b960
  virtual ~TroopVisitor(); // 0x58b9a0 (scalar deleting destructor 0x58b980)
  // Called for every troop: `ship` and `fleet` are the troop's ship and fleet (null for the garrison). A visit that
  // sets *stop ends the walk; a zero result aborts it.
  virtual int Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) = 0; // 1

  AssaultWalker* m_walker; // +0x04
};

class AssaultWalker
{
public:
  AssaultWalker(); // 0x58b9b0
  virtual ~AssaultWalker(); // 0x58ba00 (scalar deleting destructor 0x58b9e0)

  // Visits the troops of the ships of the assaulting fleets of the attacker side.
  int ForEachAttackerTroop(TroopVisitor* visitor, NotifyContext* ctx); // 0x58ba10
  // Visits the garrison of the system (the troops of the side that controls it).
  int ForEachDefenderTroop(TroopVisitor* visitor, NotifyContext* ctx); // 0x58bbd0

  int m_unused4; // +0x04, 1
  System* m_system; // +0x08
  uint32_t m_attackerSide; // +0x0c
  uint32_t m_defenderSide; // +0x10, the side that controlled the system
  int m_attackersAtStart; // +0x14
  int m_defendersAtStart; // +0x18
  int m_attackersLeft; // +0x1c
  int m_defendersLeft; // +0x20
  int m_rounds; // +0x24, fights fought so far
};

// Counts the troops it visits.
class TroopCounter : public TroopVisitor
{
public:
  TroopCounter(AssaultWalker* walker); // 0x58bc90
  ~TroopCounter() override; // 0x58bcd0 (scalar deleting destructor 0x58bcb0)
  int Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override; // 1: 0x58bce0

  int m_count; // +0x08
};

// Remembers the troop whose position in the walk is m_target.
class TroopPicker : public TroopVisitor
{
public:
  TroopPicker(AssaultWalker* walker); // 0x58bd00
  ~TroopPicker() override; // 0x58bd50 (scalar deleting destructor 0x58bd30)
  int Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override; // 1: 0x58bd60

  int m_index; // +0x08
  int m_target; // +0x0c
  GameObj* m_picked; // +0x10
};

// Moves the first m_target troops it visits into the system.
class TroopReparenter : public TroopVisitor
{
public:
  TroopReparenter(AssaultWalker* walker); // 0x58bdb0
  ~TroopReparenter() override; // 0x58be00 (scalar deleting destructor 0x58bde0)
  int Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override; // 1: 0x58be10

  int m_index; // +0x08
  int m_target; // +0x0c
};

// Every attacking troop fights one random defender; the loser is destroyed.
class AssaultCombat : public TroopVisitor
{
public:
  AssaultCombat(AssaultWalker* walker); // 0x58be70
  ~AssaultCombat() override; // 0x58beb0 (scalar deleting destructor 0x58be90)
  int Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx) override; // 1: 0x58bec0
};

class AssaultResolver
{
public:
  AssaultResolver(); // 0x58c0d0
  virtual ~AssaultResolver(); // 0x58c110 (scalar deleting destructor 0x58c0f0)

  // Finds the side of the assaulting fleets of `system`, then runs the phases. The request is not used.
  int Run(GameObjNotifier* system, ActivityRequest* request, NotifyContext* ctx); // 0x58c1e0
  int RunPhases(NotifyContext* ctx); // 0x58c120

  int StartAssault(NotifyContext* ctx); // 0x58c2d0, flags the system and counts the two armies
  int DefensesFire(NotifyContext* ctx); // 0x58c3c0, every defence facility may kill one attacking troop
  int FightTroops(NotifyContext* ctx); // 0x58c500, one round of troop against troop
  int DamageSystem(NotifyContext* ctx); // 0x58c580, the fight's rounds destroy energy, raw material and facilities
  int Occupy(NotifyContext* ctx); // 0x58c810, surviving attackers move in when no defender is left
  int EndAssault(NotifyContext* ctx); // 0x58c8b0, clears the flags

  int m_unused4; // +0x04, 1
  AssaultWalker m_walker; // +0x08
};
