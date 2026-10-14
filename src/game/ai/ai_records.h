#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/entry_identity.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The per-object records the computer player keeps about the strategic situation of its side. All of them belong to one
// AiSideController (m_owner / the owner pointer) and live in the sets of ai_members.h.
//
//   AiSystemEval     VT 0x658d70 (0x138 bytes): what the AI knows about one system (forces, facilities, loyalty ...), keyed by
//                    the system's key; Refresh() (0x433e40) rebuilds its figures from the game objects when it is dirty
//   AiSectorEval     VT 0x658d30 (0x1d8): the same for one sector, summed up over the (up to ten) systems of the sector
//   AiFleetEval      VT 0x658d50 (0xcc): one fleet of the side and the ship records it holds
//   AiShipRecord     VT 0x65aa50 (0xbc): one capital ship
//   AiTaskRecord     VT 0x65a3a0 (0x50): an order the AI is carrying out with a group of ships (state machine 0x474390)
//   AiSystemRecord   VT 0x65a3b0 (0xe8): the plan for one system: the postures it can take, the ships and fleets sent
//
// The "dirty" flags: a record whose objects changed is marked dirty (AiSideController::MarkDirty / the record's own flag)
// and rebuilt by the next Analyze() pass.

class AiSideController;
class AiSystemEvalSet;
class AiShipRecordSet;
class AiClassTable;
class AiFleetEvalSet;
class AiPosture;
class GameObjNotifier;
class Sector;
class System;
class ChildIterator;
class OrderBase;

namespace ai_flag
{
// AiSystemEval::m_flagsC8 / AiSectorEval m_flags (selected bits used by the selectors; see the code for the rest)
constexpr uint32_t kHighBit = 0x80000000;
} // namespace ai_flag

// ---- AiSystemEval ------------------------------------------------------------------------------------------------------

class AiSystemEval : public ObjectKeyNode
{
public:
  static constexpr int kStatCount = 0x3c;

  AiSystemEval(); // 0x433a60
  AiSystemEval(int view, System* system); // 0x433b10
  ~AiSystemEval() override; // 0: 0x433af0 (deleting), 0x433be0
  // slot 1 Clone is ObjectKeyNode::Clone (0x4ecda0)
  void Load(istream* in) override; // 2: 0x433c60
  void Save(ostream* out) override; // 4: 0x433d10

  void MarkChanged(); // 0x433e30: m_dirty = 1 and the sector's eval is dirty too
  void MarkDirty(); // 0x433e00: m_dirty = 1 only
  void NoteCombat(); // 0x433e10: m_combatDay = today, then MarkChanged
  void NoteTouched(); // 0x433dc0: m_touchedDay = today, then MarkChanged
  int DaysSinceTouched(); // 0x433de0: 0 if the usable flag is set, else today - m_touchedDay
  void Refresh(); // 0x433e40: re-reads the system if dirty
  ObjectKey* GetSectorKey(ObjectKey* out); // 0x4bd7d0: the key of the sector's record

  int m_view; // +0x1c the view (side) this record is for
  int m_x20; // +0x20 control side of the system (0 neutral, 1/2 the sides)
  int m_dirty; // +0x24
  PersistentInt m_flags28; // +0x28
  PersistentInt m_flags2c; // +0x2c
  PersistentInt m_flags30; // +0x30
  int m_position; // +0x34 packed map position (x low, y high), the system data record's field 0x4c
  int m_combatDay; // +0x38
  class AiSectorEval* m_sector; // +0x3c the sector's record
  AiClassTable* m_classes; // +0x40
  int m_touchedDay; // +0x44
  int& Stat(int offset)
  {
    return m_stat[(offset - 0x48) / 4];
  }

  int m_stat[kStatCount]; // +0x48
};

// ---- AiSectorEval ------------------------------------------------------------------------------------------------------

class AiSectorEval : public ObjectKeyNode
{
public:
  static constexpr int kSystemCount = 10;
  static constexpr int kStatCount = 0x5a;

  AiSectorEval(); // 0x431270
  explicit AiSectorEval(int view); // 0x431330
  ~AiSectorEval() override; // 0: 0x431310 (deleting), 0x4313c0
  // slot 1 Clone is ObjectKeyNode::Clone (0x4ecda0)
  void Save(ostream* out) override; // 4: 0x431680

  void Attach(Sector* sector, AiSystemEvalSet* systems); // 0x431450: key, position and the system evals of the sector
  void ReadRecord(istream* in, AiSystemEvalSet* systems); // 0x431550
  void SetTarget(int x, int y); // 0x431790
  void ClearTarget(); // 0x4317b0
  void SetPeer(int value); // 0x4317d0: m_x38 = value, dirty
  int ScoreByAge(); // 0x4317e0
  ObjectKey* GetSystemKey(ObjectKey* out, uint32_t index); // 0x432120
  void MarkDirty(); // 0x4321d0
  int Refresh(); // 0x4321e0: 1 if it was dirty
  void SetSystem(uint16_t index, const ObjectKey* key, AiSystemEvalSet* systems); // 0x432310
  // The systems of the sector whose flags contain all the require masks and none of the forbid masks (flags28, flags2c,
  // flags30 order), as a ranked list of their keys.
  AiRankedList* RankSystemsByStat(uint32_t requireA,
                                  uint32_t requireB,
                                  uint32_t requireC,
                                  uint32_t forbidA,
                                  uint32_t forbidB,
                                  uint32_t forbidC,
                                  uint32_t statIndex,
                                  int descending); // 0x431830
  AiRankedList* RankSystemsByAge(uint32_t requireA,
                                 uint32_t requireB,
                                 uint32_t requireC,
                                 uint32_t forbidA,
                                 uint32_t forbidB,
                                 uint32_t forbidC,
                                 int descending); // 0x431a20
  AiRankedList* RankSystemsByDistance(const ObjectKey* target,
                                      uint32_t requireA,
                                      uint32_t requireB,
                                      uint32_t requireC,
                                      uint32_t forbidA,
                                      uint32_t forbidB,
                                      uint32_t forbidC,
                                      int descending); // 0x431c00
  AiRankedList* RankSystemsRandomly(uint32_t requireA,
                                    uint32_t requireB,
                                    uint32_t requireC,
                                    uint32_t forbidA,
                                    uint32_t forbidB,
                                    uint32_t forbidC,
                                    int descending); // 0x431f40
  void Accumulate(const int* systemStats, const int* flags28, const int* flags30, const int* flags2c); // 0x432350

  int& Stat(int offset)
  {
    return m_stat[(offset - 0x70) / 4];
  }

  int m_view; // +0x1c
  int m_dirty; // +0x20
  PersistentInt m_flags24; // +0x24
  PersistentInt m_flags28; // +0x28
  PersistentInt m_flags2c; // +0x2c
  int m_x30; // +0x30
  int m_x34; // +0x34
  int m_x38; // +0x38
  int m_x3c; // +0x3c
  int16_t m_x; // +0x40 packed map position of the sector
  int16_t m_y; // +0x42
  AiSideController* m_owner; // +0x44
  AiSystemEval* m_systems[kSystemCount]; // +0x48
  int m_stat[kStatCount]; // +0x70
};

// ---- AiShipRecord -----------------------------------------------------------------------------------------------------

class AiShipRecord : public ObjectKeyNode
{
public:
  static constexpr int kStatCount = 0x1e;

  AiShipRecord(); // 0x484180
  AiShipRecord(GameObjNotifier* ship,
               int view,
               AiFleetEvalSet* fleets,
               AiClassTable* classes,
               AiSideController* owner); // 0x484230
  ~AiShipRecord() override; // 0: 0x484210 (deleting), 0x484370
  // slot 1 Clone is ObjectKeyNode::Clone (0x4ecda0)
  void Save(ostream* out) override; // 4: 0x484490

  void ReadRecord(istream* in, AiFleetEvalSet* fleets, AiClassTable* classes, AiSideController* owner); // 0x4843f0
  int IsUnusable(); // 0x484510: its fleet is busy or flagged
  void ClearFleet(); // 0x484570: dirty, and the fleet's eval is told
  void MoveToFleet(const ObjectKey* newFleet, const ObjectKey* oldFleet); // 0x484590
  int ChangeFleet(const ObjectKey* fleetKey); // 0x4845d0
  void Refresh(); // 0x484630

  int m_dirty; // +0x1c
  int m_view; // +0x20
  PersistentInt m_flags; // +0x24
  ObjectKey m_classKey; // +0x28 key of the ship's data record
  ObjectKey m_fleetKey; // +0x2c
  int m_x30; // +0x30 the fleet the AI assigned the ship to (id of its AiSystemRecord / task)
  int m_x34; // +0x34
  AiFleetEvalSet* m_fleets; // +0x38
  AiClassTable* m_classes; // +0x3c
  AiSideController* m_owner; // +0x40
  int& Stat(int offset)
  {
    return m_stat[(offset - 0x44) / 4];
  }

  int m_stat[kStatCount]; // +0x44
};

// ---- AiFleetEval ------------------------------------------------------------------------------------------------------

class AiFleetEval : public ObjectKeyNode
{
public:
  static constexpr int kStatCount = 0x1e;

  AiFleetEval(); // 0x432d80
  AiFleetEval(const int* view, GameObjNotifier* fleet, AiShipRecordSet* ships, AiSideController* owner); // 0x432e40
  ~AiFleetEval() override; // 0: 0x432e20 (deleting), 0x432f40
  // slot 1 Clone is ObjectKeyNode::Clone (0x4ecda0)
  void Save(ostream* out) override; // 4: 0x433150

  void ReadRecord(istream* in, AiShipRecordSet* ships, AiSideController* owner); // 0x432fd0
  void RemoveShip(ObjectKey* key); // 0x4332b0
  void AddShip(const ObjectKey* key); // 0x433320
  int HasShip(const ObjectKey* key); // 0x4333f0
  AiRankedList* RankShips(uint32_t requireFlags, uint32_t forbidFlags, uint32_t statIndex, int descending); // 0x433450
  void NoteCombat(); // 0x4335e0
  void MarkDirty(); // 0x433600
  int Refresh(); // 0x433620
  void AddShipStats(const int* stats, const int* flags); // 0x433780

  int m_dirty; // +0x1c
  int m_view; // +0x20
  int m_control; // +0x24
  ObjectKey m_systemKey; // +0x28 the system the fleet is at
  ObjectKey m_x2c; // +0x2c
  int m_x30; // +0x30
  int m_x34; // +0x34
  PersistentInt m_flags; // +0x38
  int m_combatDay; // +0x3c
  AiShipRecordSet* m_ships; // +0x40
  ObjectKeyNode* m_cursor; // +0x44 the ship key being refreshed
  ObjectKeySet m_shipKeys; // +0x48
  int& Stat(int offset)
  {
    return m_stat[(offset - 0x50) / 4];
  }

  int m_stat[kStatCount]; // +0x50
  AiSideController* m_owner; // +0xc8
};

// ---- AiTaskRecord -----------------------------------------------------------------------------------------------------

class AiTaskRecord : public IdNode
{
public:
  AiTaskRecord(); // 0x473fd0
  AiTaskRecord(int view, AiSideController* owner); // 0x474070
  ~AiTaskRecord() override; // 0: 0x474050 (deleting), 0x474100
  virtual void Read(istream* in); // 1: 0x474180
  virtual void Write(ostream* out); // 2: 0x4741f0

  int IsFinished(); // 0x474260: a system is assigned
  void Release(); // 0x4742b0: gives back the ships, the fleet and the system
  // One step of the task's state machine (m_state 1-8). Returns the order to submit for this step (or null); *done is set
  // to 1 when the task has finished its pass.
  OrderBase* Step(int* done); // 0x474390
  int PickTarget(); // 0x4744b0: assigns a system of the side that needs forces; 1 if one is assigned
  void ReleaseShips(); // 0x4747b0
  OrderBase* SendIdleMessage(); // 0x4749b0: order 0x270: the fleet's idle ships
  void ComputeBudget(); // 0x474b90
  OrderBase* ChooseAction(int* done); // 0x474c00
  OrderBase* MoveToSystem(); // 0x474ce0: order 0x201
  OrderBase* MoveCharacters(); // 0x474ff0
  OrderBase* RecallFleet(); // 0x475330
  OrderBase* ReinforceFleet(); // 0x475490
  OrderBase* ReinforceBySize(); // 0x475890
  OrderBase* Reinforce(); // 0x475c70
  int AddShip(const ObjectKey* key); // 0x475e70
  int RemoveShip(const ObjectKey* key); // 0x475f60
  int SetFleet(const ObjectKey* key); // 0x475fe0
  int ClearFleet(); // 0x476080

  int m_state; // +0x1c
  int m_view; // +0x20
  ObjectKeySet m_ships; // +0x24
  ObjectKey m_fleetKey; // +0x2c
  ObjectKey m_systemKey; // +0x30
  int m_demand; // +0x34
  int m_restart; // +0x38
  AiRankedSelector m_selector; // +0x3c
  AiSideController* m_owner; // +0x4c
};
