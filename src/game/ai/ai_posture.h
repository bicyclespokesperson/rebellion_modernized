#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The posture of an AiSystemRecord (0x4ad8c0-0x4bb9f0, VT 0x65c0b0 and six subclasses): the stance the AI takes towards
// the system of the record (hold it, attack it, ...), chosen from the sector's flags by AiSystemRecord::UpdatePosture.
// The posture says which planner and agent the record should have (GetPlannerRequest, GetAgentRequest), which of the
// record's fleets and characters to send where (the Step state machine and its helpers), and how much strength it lacks
// (UpdateStrengthBudget). Subclasses (AiSystemRecord::CreatePosture kinds): 1 ... 6, see ai_postures.h.
//
// m_state (+0x08) is the Step() state: 1 clear the marks of the previous pass, 2 .. 0xe the individual checks.

class AiSideController;
class AiSystemRecord;
class AiFleetEval;
class AiSystemEval;
class OrderBase;

class AiPosture
{
  public:
  static constexpr int kBudgetCount = 10;

  AiPosture(int view, AiSideController* owner, AiSystemRecord* record); // 0x4ad8e0
  virtual ~AiPosture();                                                 // 0: 0x4ad8c0 (deleting), 0x4ad980
  virtual void Read(istream* in);                                       // 1: 0x4ada10
  virtual void Write(ostream* out);                                     // 2: 0x4ada80
  virtual int GetPostureScore();                                        // 3: 0x6158b0 returns 0
  virtual void ChooseReserveSystem(ObjectKey* system);                  // 4: 0x4adde0 (the record's reserve system)
  virtual int GetKind() = 0;                                            // 5: the posture's kind id (1-6)
  // The planner the record should start (kind createKind with createSubtype, 0 for none) and the kind it can do without
  // (releaseKind, releaseSubtype).
  virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) = 0; // 6
  virtual void GetAgentRequest(int* agentKind) = 0; // 7: the agent kind (3, 4, 5) the record is missing
  virtual void UpdateSectorFlags(PersistentInt* flagsA, PersistentInt* flagsB) = 0; // 8
  virtual void SelectCharacterSystem(ObjectKey* a, ObjectKey* b) = 0;               // 9
  virtual void SelectAgePreferredSystem(ObjectKey* a, ObjectKey* b) = 0;            // 10
  virtual void SelectSystemByAge(ObjectKey* a, ObjectKey* b) = 0;                   // 11
  virtual void SelectObjectSystem(ObjectKey* a, ObjectKey* b) = 0;                  // 12
  virtual int SelectTargetSystem();                                                 // 13: 0x4af8a0 (state 2 of Step)
  virtual OrderBase* OrderTroopsForSystem();                                        // 14: 0x4afce0 (state 3)
  virtual int AssignTakeFleet();                                                    // 15: state 7
  virtual int AssignFleet();                                                        // 16: state 8
  virtual int AssignOrderFleet();                                                   // 17: state 9
  virtual int AssignSupportFleet();                                                 // 18: state 10
  virtual int HasEnoughClass1Agents();                                              // 19: 0x4b1690
  virtual void UpdateStrengthBudget() = 0;                                          // 20
  virtual ObjectKey* ChooseFleetDestination(ObjectKey* out) = 0; // 21: a system fleets can be sent to

  // Subtracts the strength the posture asks for (m_budget, filled by UpdateStrengthBudget) from the ten committed
  // statistics.
  void SubtractBudget(int* stats);     // 0x4adaf0
  OrderBase* Step(int* done);          // 0x4adb70: the state machine, returns the order to submit
  int AgentIsNeeded(uint32_t agentId); // 0x4add40
  // Picks a random object of the system (its facilities, troops, squadrons, ... as selected by `kinds`, a mask of
  // 0x20000-0x2000000) into *out.
  void PickRandomObject(const ObjectKey* system, ObjectKey* out, uint32_t kinds); // 0x4ae480
  // Picks a random character at the system whose flags match `characterFlags` into *out.
  void PickRandomCharacter(const ObjectKey* system, ObjectKey* out, uint32_t kinds,
                           uint32_t characterFlags);            // 0x4af1c0
  void MarkFleetTargets();                                      // 0x4af6d0: state 1
  OrderBase* OrderFleetToTarget();                              // 0x4b0120
  int FindIdleFleet();                                          // 0x4b04c0
  OrderBase* OrderCharacterFleet(int* found);                   // 0x4b06b0
  OrderBase* OrderFleetsAway();                                 // 0x4b0c90
  OrderBase* OrderFleetHome();                                  // 0x4b0f90
  OrderBase* OrderAssignedFleets(int* done);                    // 0x4b1070
  OrderBase* MakeFleetOrder(AiFleetEval* fleet, int orderKind); // 0x4b1330
  OrderBase* MakeTroopOrder(AiFleetEval* fleet);                // 0x4b1440
  // The planner and agent requests the postures share (their GetPlannerRequest / GetAgentRequest overrides forward to
  // these).
  void DefaultPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype); // 0x4b7150
  void AgentRequestA(int* agentKind);                                          // 0x4b1800 (kinds 5 and 3)
  void AgentRequestB(int* agentKind);                                          // 0x4b72d0 (kinds 6 and 4)
  void AgentRequestC(int* agentKind);                                          // 0x4b9290 (kinds 2 and 1)
  int HasSeveralAgentsOfClass1();                                              // 0x4b8b30 (slot 19 of kinds 6 and 4)
  int HasMultipleAgents();                                                     // 0x4b1650
  int HasEscort();                                                             // 0x4b16d0
  int CanUseFleet(AiFleetEval* fleet, AiSystemEval* system);                   // 0x4ae240
  int CanOrderFleet(AiFleetEval* fleet, AiSystemEval* system, int* orderKind); // 0x4ae320
  static int CanTakeSystem(AiFleetEval* fleet, AiSystemEval* system);          // 0x4ae150
  static int CanSupportSystem(AiFleetEval* fleet, AiSystemEval* system);       // 0x4ae440

  int m_view;                  // +0x04
  int m_state;                 // +0x08
  AiSideController* m_owner;   // +0x0c
  AiSystemRecord* m_record;    // +0x10
  PersistentInt m_flags;       // +0x14
  AiRankedSelector m_selector; // +0x18
  ObjectKey m_targetFleet;     // +0x28
  ObjectKey m_cursorA;         // +0x2c
  ObjectKey m_cursorB;         // +0x30
  int m_budget[kBudgetCount];  // +0x34
};
