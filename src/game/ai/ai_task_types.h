#pragma once
#include <cstdint>

#include "ai/ai_task_base.h"
#include "app/player_controller_decl.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The 14 task types of the AI player (ids 1-14, created by AiTaskManager::MakeTask). Each is a state machine that AiPlayer steps:
// Evaluate() says whether the task wants to run, Step() returns the next order (null if there is none), Refused() is told when
// the order was not accepted. The ranges per type are listed in docs/code_map.md.
//
// Every task keeps its stage in +0x38 and its step in +0x3c; the default Evaluate and Refused are the same function in the
// original (the compiler's identical-code folding), so they are declared once, in AiStagedTask.

class AiSideController;
class AiShipRecord;
class AiTaskEvaluator;
class OrderBase;

class AiStagedTask : public AiTaskBase {
public:
    AiStagedTask(int id, int view) : AiTaskBase(id, view), m_stage(0), m_step(0) {}
    virtual void Evaluate() override;                                 // 4: 0x4cc990 (shared by 13 task types)
    virtual void Refused(int* result, OrderBase* order) override;     // 6: 0x4dd810 (shared by 13 task types)

    int m_stage;  // +0x38
    int m_step;   // +0x3c
};

// ---- type 5: facilities ------------------------------------------------------------------------------------------------------

// Keeps a short list of systems (m_targets) the side wants to build manufacturing facilities in: it picks the systems, orders
// the manufacturing managers of the systems to produce the facilities the side is short of (orders 0x210, 0x212, 0x214), and
// scraps the old ones (order 0x200). VT 0x65c758, 0x88 bytes, 0x4cfb00-0x4d2200. The two stages (0x3f4 and 0x3ec) alternate;
// m_kind runs through the four special force types (stat indices 0x5a-0x5d).
class AiTask5 : public AiStagedTask {
public:
    explicit AiTask5(int view);                     // 0x4cfb20
    virtual ~AiTask5();                             // 0: 0x4cfb00 (deleting), 0x4cfc00
    virtual void Read(istream* in) override;        // 1: 0x4cfcb0
    virtual void Write(ostream* out) override;      // 2: 0x4cfdc0
    virtual OrderBase* Step() override;             // 5: 0x4cfed0

    OrderBase* StepProduce();                       // 0x4cff20 (stage 0x3f4)
    OrderBase* StepTargets();                       // 0x4d0010 (stage 0x3ec)
    int ChooseManager();                            // 0x4d01a0: 1 if a manufacturing manager was picked into m_manager
    int NextKind();                                 // 0x4d0480: 1 if a kind of special force the side lacks was found
    int IsOverQuota();                              // 0x4d0570
    int ComputeSpecialForceCount();                 // 0x4d0590: 0xb, 2 or 0
    OrderBase* BuildSpecialForceOrder();            // 0x4d0610: order 0x242 for the best special force
    int SelectTarget();                             // 0x4d0860: 1 if m_system was picked
    int PickSupplySystem();                         // 0x4d0b40: 1 if m_system and m_manager were picked
    int IsOverQuotaForFacilities();                 // 0x4d0d10
    OrderBase* BuildScrapOrder();                   // 0x4d0d30: order 0x200
    int ComputeFacilityCount();                     // 0x4d1270: 10, 8 or 0
    OrderBase* BuildCapitalOrder();                 // 0x4d1310: order 0x200 for the first facility of a system of the list
    OrderBase* BuildManufactureOrder212();          // 0x4d15f0
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4d1710: order 0x214
    OrderBase* BuildManufactureOrder210();          // 0x4d1850
    void AddTargetSystem();                         // 0x4d1970
    int Need();                                     // 0x4d1d90
    int NeedForTargets();                           // 0x4d1df0
    void RefreshTargets();                          // 0x4d1ed0

    int16_t m_kind;                  // +0x40
    int m_x44;                       // +0x44
    int m_x48;                       // +0x48
    int m_x4c;                       // +0x4c
    int m_x50;                       // +0x50
    int m_x54;                       // +0x54
    ObjectKey m_key58;               // +0x58
    ObjectKey m_system;              // +0x5c
    ObjectKey m_manager;             // +0x60
    ObjectKey m_classKey;            // +0x64 starts as 0, not the unset key
    AiRankedSelector m_selector;     // +0x68
    ObjectKeySet m_targets;          // +0x78
    ObjectKeySet m_dropped;          // +0x80
};

// ---- type 8: system records ----------------------------------------------------------------------------------------------

// Runs the AiSystemRecords of the side one at a time: moves each record to a sector that suits it and lets it step through its
// posture (VT 0x65c738, 0x58 bytes, 0x4cf3f0-0x4cf7c0 and 0x4cfac0).
class AiTask8 : public AiStagedTask {
public:
    explicit AiTask8(int view);                     // 0x4cf410
    virtual ~AiTask8();                             // 0: 0x4cf3f0 (deleting), 0x4cf470
    virtual void Read(istream* in) override;        // 1: 0x4cf4c0
    virtual void Write(ostream* out) override;      // 2: 0x4cf560
    virtual OrderBase* Step() override;             // 5: 0x4cf600

    OrderBase* StepRun();                           // 0x4cf630
    void PrepareRecords();                          // 0x4cf700: adds or removes a record when the side's demand changed
    int NextRecord();                               // 0x4cf760: 1 if there is a record to handle
    int ChooseSector();                             // 0x4cf7c0: 1 when the record has the sector and posture it should
    OrderBase* RunRecord(int* done);                // 0x4cfac0

    int m_recordId;       // +0x40 the AiSystemRecord being handled
    int m_recordCursor;   // +0x44
    AiRankedSelector m_selector;  // +0x48
};

// ---- type 9: task records ------------------------------------------------------------------------------------------------

// Runs the AiTaskRecords of the side one at a time (VT 0x65c718, 0x58 bytes, 0x4cf080-0x4cf3f0).
class AiTask9 : public AiStagedTask {
public:
    explicit AiTask9(int view);                     // 0x4cf0a0
    virtual ~AiTask9();                             // 0: 0x4cf080 (deleting), 0x4cf100
    virtual void Read(istream* in) override;        // 1: 0x4cf150
    virtual void Write(ostream* out) override;      // 2: 0x4cf1f0
    virtual OrderBase* Step() override;             // 5: 0x4cf290

    OrderBase* StepRun();                           // 0x4cf2c0
    int NextRecord();                               // 0x4cf350: 1 if there is a record to handle
    OrderBase* RunRecord(int* done);                // 0x4cf3b0

    int m_recordId;       // +0x40 the AiTaskRecord being handled
    int m_recordCursor;   // +0x44 where the search for the next record continues
    AiRankedSelector m_selector;  // +0x48
};

// ---- type 4: planners of the side ----------------------------------------------------------------------------------------

// Carries out the planner requests of the evaluators (AiSideController::m_missionTarget/Kind/Subtype): releases the planner the
// side no longer wants, creates the one it asks for, and then does the same for the planner every AiSystemRecord wants, one
// record per step; finally it runs the planners in turn (VT 0x65c778, 0x60 bytes, 0x4d2200-0x4d27f0).
class AiTask4 : public AiStagedTask {
public:
    explicit AiTask4(int view);                     // 0x4d2220
    virtual ~AiTask4();                             // 0: 0x4d2200 (deleting), 0x4d2290
    virtual void Read(istream* in) override;        // 1: 0x4d22e0
    virtual void Write(ostream* out) override;      // 2: 0x4d23a0
    virtual OrderBase* Step() override;             // 5: 0x4d2460

    OrderBase* StepRun();                           // 0x4d2490
    int PrepareSidePlanner();                       // 0x4d2580: 1 if a planner of the side was created
    int NextRecordWithPlanners();                   // 0x4d2610: 1 if a record with a planner request was found
    int PrepareRecordPlanner();                     // 0x4d2690
    int NextPlanner();                              // 0x4d2730: 1 if a planner that wants to run was found
    OrderBase* RunPlanner(int* done);               // 0x4d27b0

    int m_recordId;       // +0x40 the AiSystemRecord being handled
    int m_recordCursor;   // +0x44 where the search for the next record continues
    int m_plannerId;      // +0x48 the planner being run
    int m_plannerCursor;  // +0x4c where the search for the next planner continues
    AiRankedSelector m_selector;  // +0x50
};

// ---- type 12: agents of the side ----------------------------------------------------------------------------------------

// The same for the agents (characters on missions): AiSideController::m_agentSystem/m_agentTarget are the side's own request,
// AiSystemRecord::m_xe0/m_xe4 those of the records (VT 0x65c6b8, 0x60 bytes, 0x4c8240-0x4c8800).
class AiTask12 : public AiStagedTask {
public:
    explicit AiTask12(int view);                    // 0x4c8260
    virtual ~AiTask12();                            // 0: 0x4c8240 (deleting), 0x4c82d0
    virtual void Read(istream* in) override;        // 1: 0x4c8320
    virtual void Write(ostream* out) override;      // 2: 0x4c83e0
    virtual OrderBase* Step() override;             // 5: 0x4c84a0

    OrderBase* StepRun();                           // 0x4c84d0
    int PrepareSideAgent();                         // 0x4c85b0: 1 if an agent of the side was created
    int NextRecordWithAgents();                     // 0x4c8630
    int PrepareRecordAgent();                       // 0x4c86b0
    int NextAgent();                                // 0x4c8740
    OrderBase* RunAgent(int* done);                 // 0x4c87c0

    int m_recordId;       // +0x40
    int m_recordCursor;   // +0x44
    int m_agentId;        // +0x48
    int m_agentCursor;    // +0x4c
    AiRankedSelector m_selector;  // +0x50
};

// ---- type 13: the vote -------------------------------------------------------------------------------------------------

// Decides how the side answers a vote (kinds of plan 1-7, each with an evaluator) and, for the Alliance, scraps facilities of a
// chosen system and sends the unit 0x20000005 there (VT 0x65c480, 0x70 bytes, 0x4bf0b0-0x4c06e0).
class AiTask13 : public AiStagedTask {
public:
    explicit AiTask13(int view);                    // 0x4bf0d0
    virtual ~AiTask13();                            // 0: 0x4bf0b0 (deleting), 0x4bf160
    virtual void Read(istream* in) override;        // 1: 0x4bf1f0
    virtual void Write(ostream* out) override;      // 2: 0x4bf280
    virtual OrderBase* Step() override;             // 5: 0x4bf320

    // Passes the other side's vote to the plan's evaluator, which puts the chance of accepting it into *chance.
    void ForwardVote(const ObjectKey* key, GameTime* chance);  // 0x4bf300
    void StepPlan();                                // 0x4bf390
    OrderBase* StepAlliance();                      // 0x4bf3e0
    void StepEmpire();                              // 0x4bf480
    void ChoosePlan();                              // 0x4bf490
    void CreateEvaluator();                         // 0x4bf7c0
    int ChooseTargets();                            // 0x4bf9e0: 1 if a system was picked into m_target
    OrderBase* BuildScrapOrder();                   // 0x4bfe30
    OrderBase* BuildMoveOrder();                    // 0x4c05e0

    AiRankedSelector m_selector;        // +0x40
    ObjectKey m_target;                 // +0x50
    int m_nextTime;                     // +0x54 game day before which no new target is chosen (initially 200)
    int m_x58;                          // +0x58
    int m_plan;                         // +0x5c the kind of plan, 1-7 (0 for none)
    AiTaskEvaluator* m_evaluator;       // +0x60
    AiTaskObjectSet m_objects;          // +0x64
};

// ---- type 14: ship status messages -------------------------------------------------------------------------------------

// Walks the AI's ship records ten at a time and sends the player's advisor a text message (order 0x203) about ships that are
// enroute, damaged or in some other state; the texts come from a table of string resources per side (VT 0x65c798, 0x4c
// bytes, 0x4d2810-0x4d2ed0). The only task type that does not use AiStagedTask.
class AiTask14 : public AiTaskBase {
public:
    explicit AiTask14(int view);                    // 0x4d2810
    virtual ~AiTask14();                            // 0: 0x4d27f0 (deleting), 0x4d2880
    virtual void Read(istream* in) override;        // 1: 0x4d28d0
    virtual void Write(ostream* out) override;      // 2: 0x4d2920
    virtual void Evaluate() override;               // 4: 0x4d2970
    virtual OrderBase* Step() override;             // 5: 0x4d29b0
    virtual void Refused(int* result, OrderBase* order) override;  // 6: 0x4d2b10

    // The message about one ship: the next unused text of the kind that matches the ship's flags (or the ship's name).
    OrderBase* BuildMessage(AiShipRecord* record);  // 0x4d2b30

    int m_count;               // +0x38 messages sent in this pass
    AiShipRecord* m_cursor;    // +0x3c the ship record to look at next
    ObjectKey m_resume;        // +0x40 the ship the pass stopped at
    int16_t m_enrouteTexts;    // +0x44 index of the next text for each kind of ship state
    int16_t m_damagedTexts;    // +0x46
    int16_t m_otherTexts;      // +0x48
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiStagedTask, 0x40);
REB_ASSERT_SIZE(AiTask13, 0x70);
REB_ASSERT_OFFSET(AiTask13, m_plan, 0x5c);
REB_ASSERT_OFFSET(AiTask13, m_objects, 0x64);
REB_ASSERT_SIZE(AiTask5, 0x88);
REB_ASSERT_OFFSET(AiTask5, m_selector, 0x68);
REB_ASSERT_OFFSET(AiTask5, m_dropped, 0x80);
REB_ASSERT_SIZE(AiTask8, 0x58);
REB_ASSERT_SIZE(AiTask9, 0x58);
REB_ASSERT_SIZE(AiTask4, 0x60);
REB_ASSERT_SIZE(AiTask12, 0x60);
REB_ASSERT_SIZE(AiTask14, 0x4c);
REB_ASSERT_OFFSET(AiTask14, m_enrouteTexts, 0x44);
#pragma clang diagnostic pop

// The constructors of the evaluators (0x4e60c0-0x4ebae0): (view, owner) on memory from operator new.
AiTaskEvaluator* __thiscall FUN_004e60e0(void* memory, int view, AiSideController* owner);  // plan 1
AiTaskEvaluator* __thiscall FUN_004e77c0(void* memory, int view, AiSideController* owner);  // plan 2
AiTaskEvaluator* __thiscall FUN_004e8770(void* memory, int view, AiSideController* owner);  // plan 3
AiTaskEvaluator* __thiscall FUN_004e94a0(void* memory, int view, AiSideController* owner);  // plan 4
AiTaskEvaluator* __thiscall FUN_004e9e90(void* memory, int view, AiSideController* owner);  // plan 5
AiTaskEvaluator* __thiscall FUN_004eab80(void* memory, int view, AiSideController* owner);  // plan 6
AiTaskEvaluator* __thiscall FUN_004e6a30(void* memory, int view, AiSideController* owner);  // plan 7
