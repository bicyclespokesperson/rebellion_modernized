#pragma once
#include <cstdint>

#include "ai/ai_class_records.h"
#include "ai/ai_ranked_list.h"
#include "ai/ai_records.h"
#include "core/containers.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The collections an AiSideController (ai_side_controller.h) is built from (0x42dc10-0x431270). Each holds the records of
// one kind for one side and knows how to build them from the game objects, save and load them. They are sets keyed by
// object key (ObjectKeySet) or by id (IdAvlTree). Every set stores the view it is for (1 Alliance, 2 Empire) and back
// pointers to the sets it needs; AiSideController's constructor wires them up.
//
//   AiClassTable        VT 0x658c40 (0x40): the AiClassRecordBase records of the unit classes and the best classes found
//   AiTaskRecordSet     VT 0x658c50 (0x14): AiTaskRecords
//   AiAgentSet          VT 0x658c60 (0x18): the character agents (planner helpers), created by CreateAgent()
//   AiSystemRecordSet   VT 0x658c70 (0x14): AiSystemRecords
//   AiPlannerSet        VT 0x658c80 (0x18): the mission planners, created by CreatePlanner()
//   AiFleetEvalSet      VT 0x658c90 (0x14): AiFleetEvals
//   AiShipRecordSet     VT 0x658cb8 (0x20): AiShipRecords
//   AiSectorEvalSet     VT 0x658ce0 (0x14): AiSectorEvals
//   AiSystemEvalSet     VT 0x658d08 (0x18): AiSystemEvals
//   AiCharacterRecordSet (ai_character_records.h): AiCharacterRecords

class AiSideController;
class AiCharacterRecordSet;
class AiMissionPlanner;
class AiAgentBase;
class GameObjNotifier;
class Sector;
class System;

// ---- AiClassTable -----------------------------------------------------------------------------------------------------

class AiClassTable : public AvlTree
{
public:
  AiClassTable(); // 0x42dc10
  ~AiClassTable() override; // 0: 0x42dc50 (deleting), 0x42dc70
  int IsLess(AvlNode* a, AvlNode* b) override; // 2: 0x42ebc0

  void LoadValues(istream* in); // 0x42dc80: the three selected tiers
  void SaveValues(ostream* out); // 0x42dcc0
  // Creates the class records of every unit family from the data records and finds the highest tier the side can
  // build (m_maxCapitalTier ...), then selects the best classes for the tiers (SetCapitalShipTier ...).
  void Build(int side); // 0x42dd00
  AiClassRecordBase* Find(const ObjectKey* key); // 0x42e190
  // Set the tier and pick the best class of each role the side can build with it (the picks are the m_x28.. fields).
  void SetCapitalShipTier(int tier, int side); // 0x42e210
  void SetTroopTier(int tier, int side); // 0x42e330
  void SetFacilityTier(int tier, int unusedSide); // 0x42e3b0
  // Picks a class of the side's research tree for the data record type `recordType`: the one with the best value of the
  // statistic `mode` selects (the first of the side's matching classes at random when mode is 0). Returns the class's
  // m_valueB and stores its key in *out.
  int ChooseFromSide(ObjectKey* out,
                     GameObjNotifier* side,
                     int recordType,
                     uint32_t requiredFlags,
                     int mode,
                     int descending); // 0x42e3c0
  AiRankedList* RankClasses(GameObjNotifier* side,
                            int recordType,
                            uint32_t requiredFlags,
                            uint32_t valueMask,
                            int descending); // 0x42e720
  // The best class (key in *out) among the classes of `view` that have a flag of flagMask and a tier not above maxTier, by
  // the statistic valueMask; returns that statistic.
  int Select(ObjectKey* out, int view, int maxTier, uint32_t flagMask, uint32_t valueMask, int descending); // 0x42e960

  int m_maxShipTier; // +0x08 highest tier of the side's capital ships and fighters
  int m_maxTroopTier; // +0x0c
  int m_maxFacilityTier; // +0x10
  int m_majorCharacters; // +0x14
  int m_capitalTier; // +0x18 the tier SetCapitalShipTier last used
  int m_troopTier; // +0x1c
  int m_facilityTier; // +0x20
  int m_bestDefense; // +0x24 best defensive troop value at m_troopTier
  int m_x28; // +0x28 picked by SetCapitalShipTier: flag 0x200000, stat 0x400
  int m_x2c; // +0x2c flag 1, stat 0x200
  int m_x30; // +0x30 flag 2, stat 0x400
  int m_x34; // +0x34 flag 0x100000, stat 0x80
  int m_x38; // +0x38 flag 0x100000, stat 0x100
  int m_x3c; // +0x3c flag 0x800, stat 0x800
};

// ---- AiTaskRecordSet --------------------------------------------------------------------------------------------------

class AiTaskRecordSet : public IdAvlTree
{
public:
  AiTaskRecordSet(); // 0x42ec30
  ~AiTaskRecordSet() override; // 0: 0x42ec50 (deleting), 0x42ec70
  void Load(istream* in); // 0x42ec90
  void Save(ostream* out); // (shares the code of AiSystemRecordSet::Save, 0x42f2b0)
  uint32_t CreateRecord(); // 0x42ed50: adds a new record, returns its id
  uint32_t DeleteFirstUnassigned(); // 0x42edc0

  int m_view; // +0x0c
  AiSideController* m_owner; // +0x10
};

// ---- AiAgentSet ------------------------------------------------------------------------------------------------------

class AiAgentSet : public IdAvlTree
{
public:
  AiAgentSet(); // 0x42ee00
  ~AiAgentSet() override; // 0: 0x42ee20 (deleting), 0x42ee40
  void Load(istream* in); // 0x42ee50
  void Save(ostream* out); // 0x42eee0
  uint32_t AddAgent(int kind); // 0x42ef50: creates the agent, adds it, returns its id (or `kind` if it cannot be made)
  void ReleaseAgent(uint32_t id); // 0x42ef90
  AiAgentBase* CreateAgent(int kind); // 0x42efe0: kinds 1, 3, 4, 5, 6

  int m_view; // +0x0c
  AiCharacterRecordSet* m_characters; // +0x10
  AiSideController* m_owner; // +0x14
};

// ---- AiSystemRecordSet ------------------------------------------------------------------------------------------------

class AiSystemRecord;

class AiSystemRecordSet : public IdAvlTree
{
public:
  AiSystemRecordSet(); // 0x42f190
  ~AiSystemRecordSet() override; // 0: 0x42f1b0 (deleting), 0x42f1d0
  void Load(istream* in); // 0x42f1f0
  void Save(ostream* out); // 0x42f2b0
  AiSystemRecord* FindByTarget(const ObjectKey* key); // 0x42f310 (the record whose system key is *key)
  uint32_t CreateRecord(); // 0x42f350
  uint32_t DeleteFirstIdle(); // 0x42f3d0

  int m_view; // +0x0c
  AiSideController* m_owner; // +0x10
};

// ---- AiPlannerSet -----------------------------------------------------------------------------------------------------

class AiPlannerSet : public IdAvlTree
{
public:
  AiPlannerSet(); // 0x42f4c0
  ~AiPlannerSet() override; // 0: 0x42f4e0 (deleting), 0x42f500
  void Load(istream* in); // 0x42f510
  void Save(ostream* out); // 0x42f5b0
  uint32_t AddPlanner(int kind, int subtype); // 0x42f640
  void ReleasePlanner(uint32_t id); // 0x42f690
  AiMissionPlanner* FindByTarget(const int* key); // 0x42f700 (the planner whose target key is *key)
  void NoteMissionObject(const ObjectKey* key); // 0x42f740
  void NoteMission(const ObjectKey* key); // 0x42f7d0
  void CallSlot12(const int* key, int argument); // 0x42f7f0 (planner virtual 12)
  void CallSlot3(const int* key, int argument); // 0x42f810 (planner virtual 3)
  // The planner of a mission kind (0x51-0x6a) and subtype, or null.
  AiMissionPlanner* CreatePlanner(int kind, int subtype); // 0x42f830

  int m_view; // +0x0c
  AiCharacterRecordSet* m_characters; // +0x10
  AiSideController* m_owner; // +0x14
};

// ---- AiFleetEvalSet ---------------------------------------------------------------------------------------------------

class AiFleetEvalSet : public ObjectKeySet
{
public:
  AiFleetEvalSet(); // 0x42fd50
  ~AiFleetEvalSet() override; // 0: 0x42fd70 (deleting), 0x42fd90
  void Load(istream* in) override; // 4: 0x42fdb0
  void Save(ostream* out) override; // 6: 0x42fe90

  void AddEval(AiFleetEval* eval); // 0x42ff30
  void NoteFleet(const ObjectKey* key); // 0x42ff70
  void NoteFleetCombat(const ObjectKey* key); // 0x42ffe0
  void RemoveShip(ObjectKey* fleetKey, const ObjectKey* shipKey); // 0x430050
  void AddShip(const ObjectKey* fleetKey, const ObjectKey* shipKey); // 0x430120
  void DeleteIfEmpty(ObjectKey* fleetKey); // 0x430220
  AiFleetEval* FindEval(ObjectKey key); // 0x419e40 (the copy used for this set)

  int m_view; // +0x08
  AiSideController* m_owner; // +0x0c
  AiShipRecordSet* m_ships; // +0x10
};

// ---- AiShipRecordSet --------------------------------------------------------------------------------------------------

class AiShipRecordSet : public ObjectKeySet
{
public:
  AiShipRecordSet(); // 0x430240
  ~AiShipRecordSet() override; // 0: 0x430270 (deleting), 0x430290
  void Load(istream* in) override; // 4: 0x4302b0
  void Save(ostream* out) override; // 6: 0x4303a0

  int AddRecord(AiShipRecord* record); // 0x430450
  int BuildStep(); // 0x4304d0
  void SetFleet(ObjectKey* shipKey, const ObjectKey* fleetKey); // 0x430670
  void AddOrRefresh(const ObjectKey* key); // 0x4306e0
  void MoveShip(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to); // 0x430830
  void RemoveShip(const ObjectKey* key, const ObjectKey* fleet); // 0x430860

  int m_view; // +0x08
  int m_done; // +0x0c
  GameObjNotifier* m_cursor; // +0x10
  AiFleetEvalSet* m_fleets; // +0x14
  AiClassTable* m_classes; // +0x18
  AiSideController* m_owner; // +0x1c
};

// ---- AiSectorEvalSet --------------------------------------------------------------------------------------------------

class AiSystemEvalSet;

class AiSectorEvalSet : public ObjectKeySet
{
public:
  AiSectorEvalSet(); // 0x430890
  ~AiSectorEvalSet() override; // 0: 0x4308b0 (deleting), 0x4308d0
  void Save(ostream* out) override; // 6: 0x430a10

  void LoadAll(istream* in, AiSystemEvalSet* systems, AiSideController* owner); // 0x4308f0
  int BuildStep(AiSystemEvalSet* systems, AiSideController* owner); // 0x430b80
  void NoteSector(const ObjectKey* key); // 0x430c90
  void ReadOne(istream* in, AiSystemEvalSet* systems, AiSideController* owner); // 0x430cc0
  AiSectorEval* FindEval(ObjectKey key); // 0x41bd90

  int m_view; // +0x08
  AiSectorEval* m_cursor; // +0x0c
  GameObjNotifier* m_cursorObject; // +0x10
};

// ---- AiSystemEvalSet --------------------------------------------------------------------------------------------------

class AiSystemEvalSet : public ObjectKeySet
{
public:
  AiSystemEvalSet(); // 0x430d50
  ~AiSystemEvalSet() override; // 0: 0x430d70 (deleting), 0x430d90
  void Load(istream* in) override; // 4: 0x430db0
  void Save(ostream* out) override; // 6: 0x430ed0

  int BuildStep(); // 0x431040
  void NoteSystem(const ObjectKey* key); // 0x431170
  void NoteSystemCombat(const ObjectKey* key); // 0x4311a0
  void MarkAllChanged(); // 0x4311d0
  void ReadOne(istream* in); // 0x4311f0
  AiSystemEval* FindEval(ObjectKey key)
  {
    return static_cast<AiSystemEval*>(Find(&key));
  }

  int m_view; // +0x08
  AiSystemEval* m_cursor; // +0x0c
  GameObjNotifier* m_cursorObject; // +0x10
  AiClassTable* m_classes; // +0x14
};
