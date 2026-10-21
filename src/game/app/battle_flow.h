#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "app/battle_engine.h"
#include "app/player_controller_decl.h"
#include "core/game_string.h"
#include "events/game_step_driver.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"

class CapShip;
class Character;
class FightSquad;
class GameObj;
class System;
class LiveBattleSession;

// The battle flow: the state machine the application steps once per frame (0x40a050, state in the battle-flow object
// DAT_006b1214 at +8). It drives the game step driver (events/game_step_driver.h) through the strategic game,
// and, when a battle is pending, hands the battle over to the tactical engine and takes the result back:
//
//    1 INIT   2 STRAT   3 ALL_BATTLE_START   4 BATTLE_START   5 BATTLE_CHOOSE_TYPE   6 BATTLE_WAIT_FOR_TYPE_CHOICE
//    7 BATTLE_CONVERT_TO_TACT (live)   8 BATTLE_CONVERT_TO_TACT_ABS (headless)   9 BATTLE_FINISH
//    10 BATTLE_CHOOSE_DONE   11 BATTLE_WAIT_FOR_DONE_CHOICE   12 ALL_BATTLE_FINISH
//    0x14 TACT   0x15 TACT_ABSTRACT   0x16 TACT_CONVERT_TO_STRAT   0x17 TACT_ABSTRACT_CONVERT_TO_STRAT
enum BattleFlowState {
  kFlowInit = 1,
  kFlowStrategic = 2,
  kFlowAllBattleStart = 3,
  kFlowBattleStart = 4,
  kFlowBattleChooseType = 5,
  kFlowBattleWaitForType = 6,
  kFlowConvertToTactical = 7,
  kFlowConvertToAbstract = 8,
  kFlowBattleFinish = 9,
  kFlowBattleChooseDone = 10,
  kFlowBattleWaitForDone = 11,
  kFlowAllBattleFinish = 12,
  kFlowTactical = 0x14,
  kFlowTacticalAbstract = 0x15,
  kFlowTacticalToStrategic = 0x16,
  kFlowAbstractToStrategic = 0x17,
};

// The battle-flow object (VT 0x658644, created by CreateBattleFlow). It owns the game-step driver and the game's
// session lifecycle: frame stepping, new games and saves. Live battles retain the held strategic game;
// their saves append a versioned simulator checkpoint and screen state to the strategic save.
class BattleFlow
{
  public:
  // Inlined into CreateBattleFlow: starts in state 1 (STRAT_INIT) with no pending vote.
  BattleFlow();
  virtual ~BattleFlow(); // 0x4093f0 (scalar deleting destructor 0x4093d0); drops the strategic game and battle session

  // One frame. *stepped is set by the game step driver's frame phase; the state advances on its own conditions.
  void Step(int* stepped); // 0x40a050

  // Creates the game step driver for a new game (galaxy size 1-3 in the settings). Returns its m_ok, or 0.
  int StartGame(GameSettings* settings, int a, int b); // 0x409fe0

  // Reads the strategic game and, for a modern tactical save, resumes its checkpoint. m_ok reports success.
  void Restore(istream* in); // 0x409480
  // Writes the saved form that Restore reads; returns m_ok.
  int Save(ostream* out); // 0x4095b0

  // The strategic part of a save: the game step driver and the application's strategic window state.
  int SaveStrategic(ostream* out); // 0x409690
  // The step driver's +0x1c flag while the strategic game runs (state 2); 0 / ignored otherwise.
  int GetAutosaveFlag();           // 0x409390
  void SetAutosaveFlag(int value); // 0x4093b0
  GameStepDriver* StepDriver() const noexcept;
  int CreateStepDriver();
  int CreateStepDriverFor(GameSettings* settings, int a, int b);
  int DestroyStepDriver();
  LiveBattleSession* LiveSession() const { return m_liveBattle.get(); }

  int m_ok;             // +0x04
  int m_state;          // +0x08
  GameTime m_voteTimeA; // +0x0c, filled by the battle-type vote
  GameTime m_voteTimeB; // +0x10

  private:
  // Creates the battle's engine and hands the battle to it (states 7 and 8).
  void ConvertBattle();
  // Applies the finished battle's results and drops the engine (states 0x16 and 0x17).
  void ApplyBattle();

  std::unique_ptr<LiveBattleSession> m_liveBattle;
  std::unique_ptr<GameStepDriver> m_stepDriver;
  std::unique_ptr<BattleEngine> m_battleEngine;
};

BattleFlow* GetBattleFlow(); // 0x409270
// Replaces the battle flow; an object whose m_ok is 0 is dropped again at once.
void CreateBattleFlow();  // 0x409280
void DestroyBattleFlow(); // 0x409370

// Callbacks CreateBattleFlow installs in the object model's hook table (game_hooks.h).
void RouteNoticeToSession(HookArg notice); // 0x41d100 (slot 0x6b2ae0)
const char* GetStepDriverDataDirectory();  // 0x41dc70 (slot 0x6b2ae8)

// Writes the name of battle-flow state `state` into *out ("STRAT_INIT"...); returns 0 ("Invalid ") for an unknown
// state.
int GetBattleStateName(int state, std::string* out); // 0x40a700

// ---- Game step driver lifetime ---------------------------------------------------------------------------------
int CreateGameStepDriver();                                        // 0x40ac40: m_ok, or 0
int CreateGameStepDriverFor(GameSettings* settings, int a, int b); // 0x40adb0: m_ok, or 0
int DestroyGameStepDriver();                                       // 0x40af40
void InitFlowRangeSpan();                                          // 0x40d780

// ---- Strategic -> tactical hand-over ---------------------------------------------------------------------------
// Walks the strategic battle at the root system and builds its BattleSnapshot (space_battle/snapshot.h): the capital
// ships, Death Stars, fighter squadrons, characters, special forces and troop regiments, in the order the original
// registered them with the tactical engine. It then loads the snapshot into the battle engine and stores each unit's
// handle in its data block (GameObj::SetDataField04).
class BattleSetup
{
  public:
  BattleSetup();          // 0x40afa0
  virtual ~BattleSetup(); // 0x40b050 (scalar deleting destructor 0x40b030)

  // Returns the result of the last conversion call (the system it found, if there was nothing to convert).
  unsigned Convert(); // 0x40b0e0

  // Each appends the object's snapshot entry and returns whether it did.
  int AddCapitalShip(CapShip* ship);      // 0x40b8a0
  int AddDeathStar(CapShip* deathStar);   // 0x40bcd0
  int AddFightSquad(FightSquad* squad);   // 0x40be90
  int AddCharacter(Character* character); // 0x40c200
  int AddSpecialForce(GameObj* force);    // 0x40c4b0
  int AddTroopRegiment(GameObj* troops);  // 0x40c5f0

  GameStepDriver* m_driver;  // +0x04
  System* m_system;          // +0x08 the battle's system, found by Convert
  GameTime m_timeA;          // +0x10 copies of the battle-flow object's vote times
  GameTime m_timeB;          // +0x14
  ObjectKey m_keyKind2Side1; // +0x18 the System::FindBestShipOf* results for side 1 / 2 (kinds 2 and 1)
  ObjectKey m_keyKind1Side1; // +0x1c
  ObjectKey m_keyKind2Side2; // +0x20
  ObjectKey m_keyKind1Side2; // +0x24
  BattleEngine* m_engine = nullptr;       // receives the snapshot
  reb::battle::BattleSnapshot m_snapshot; // what Convert built

  private:
  int AddPassenger(GameObj* passenger, bool troopRegiment);
  int AddUnit(GameObj* object, reb::battle::UnitEntry entry);
  void AddDefenses();
  reb::battle::UnitIndex UnitOf(GameObjNotifier* object) const;

  std::vector<GameObj*> m_unitObjects; // the strategic object of each snapshot unit
};

// ---- Tactical -> strategic result application ------------------------------------------------------------------
// Reads the tactical engine's results back and applies them to the strategic objects: the winner, ship damage,
// fighter losses, carrier changes and hangar overflow.
class BattleResults
{
  public:
  BattleResults();          // 0x40c730
  virtual ~BattleResults(); // 0x40c770 (scalar deleting destructor 0x40c750)

  // Applies the whole battle and releases the battle engine state.
  unsigned Apply(); // 0x40c780

  int ApplyCapitalShip(CapShip* ship, NotifyContext* context);       // 0x40cfc0
  int ApplyDeathStar(CapShip* deathStar, NotifyContext* context);    // 0x40d170
  int ApplyFightSquad(FightSquad* squad, NotifyContext* context);    // 0x40d260
  int ApplyCarrierChange(FightSquad* squad, NotifyContext* context); // 0x40d320
  int ApplyHangarOverflow(CapShip* ship, NotifyContext* context);    // 0x40d510

  GameStepDriver* m_driver; // +0x04
  System* m_system;         // +0x08
  BattleEngine* m_engine = nullptr; // the engine that ran the battle; Apply finishes it
};
