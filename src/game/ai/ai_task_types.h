#pragma once
#include <cstdint>

#include "ai/ai_task_base.h"
#include "ai/ai_task_family.h"
#include "app/player_controller_decl.h"
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
    aitask::FamilyFields Fields();
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

// ---- type 10: fighters ----------------------------------------------------------------------------------------------------

// A task of the facility family (ai_task_family.h) that also sends the side's fleets to where fighters are needed and scraps
// the weakest fighter squadron of a fleet (VT 0x65c6f8, 0x84 bytes, 0x4cc690-0x4cf080). Stage 0x3f2 handles the fleets, 0x3ec the
// systems.
class AiTask10 : public AiStagedTask {
public:
    explicit AiTask10(int view);                    // 0x4cc6b0
    virtual ~AiTask10();                            // 0: 0x4cc690 (deleting), 0x4cc790
    virtual void Read(istream* in) override;        // 1: 0x4cc830
    virtual void Write(ostream* out) override;      // 2: 0x4cc8e0
    virtual OrderBase* Step() override;             // 5: 0x4cc9c0

    OrderBase* StepFleets();                        // 0x4cca20
    OrderBase* StepTargets();                       // 0x4ccb50
    int ChooseManager();                            // 0x4cccc0
    int NextRecord();                               // 0x4cd040
    int ComputeSquadronCount();                     // 0x4cd230
    int IsOverQuota();                              // 0x4cd2f0
    OrderBase* BuildSquadronScrapOrder();           // 0x4cd310
    int SelectTarget();                             // 0x4cd580
    int PickSupplySystem();                         // 0x4cd860
    OrderBase* BuildScrapOrder();                   // 0x4cda90
    int ComputeFacilityCount();                     // 0x4cdfd0
    OrderBase* BuildCapitalOrder();                 // 0x4ce070
    OrderBase* BuildFleetDestinationOrder(ObjectKey* target);  // 0x4ce350
    OrderBase* BuildManufactureOrder211();          // 0x4ce490
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4ce5b0
    OrderBase* BuildManufactureOrder210();          // 0x4ce6f0
    void AddTargetSystem();                         // 0x4ce810
    int Need();                                     // 0x4cec20
    int NeedForTargets();                           // 0x4cec80
    void RefreshTargets();                          // 0x4ced60
    aitask::FamilyFields Fields();

    int m_recordId;       // +0x40 the AiSystemRecord being handled
    int m_recordCursor;   // +0x44
    AiRankedSelector m_selector;  // +0x48
    ObjectKey m_manager;  // +0x58
    ObjectKey m_unit;     // +0x5c the fleet or ship being handled
    ObjectKey m_system;   // +0x60
    ObjectKey m_classKey; // +0x64 starts as 0
    int m_count;          // +0x68
    int m_cap;            // +0x6c
    int m_limit;          // +0x70
    ObjectKeySet m_targets;  // +0x74
    ObjectKeySet m_dropped;  // +0x7c
};

// ---- types 3 and 7: facility tasks with a short list of target systems ---------------------------------------------------------

// The fields and the serialization the original shares between task types 3 and 7 (0x7c bytes; Read is 0x4d3070, Write 0x4d6ca0).
class AiFacilityTaskBase : public AiStagedTask {
public:
    AiFacilityTaskBase(int id, int view);
    virtual void Read(istream* in) override;        // 1: 0x4d3070
    virtual void Write(ostream* out) override;      // 2: 0x4d6ca0
    aitask::FamilyFields Fields();

    AiRankedSelector m_selector;  // +0x40
    ObjectKey m_manager;          // +0x50
    ObjectKey m_unit;             // +0x54
    ObjectKey m_system;           // +0x58
    ObjectKey m_classKey;         // +0x5c starts as 0
    int m_count;                  // +0x60
    int m_cap;                    // +0x64
    int m_limit;                  // +0x68
    ObjectKeySet m_targets;       // +0x6c
    ObjectKeySet m_dropped;       // +0x74
};

// Type 3: defense facilities (VT 0x65c7d8, 0x7c bytes, 0x4d6b00-0x4da930). Stage 0x3ed picks defense targets, 0x3ee builds and
// scraps defenses, 0x3ec handles the target systems.
class AiTask3 : public AiFacilityTaskBase {
public:
    explicit AiTask3(int view);                     // 0x4d6b20
    virtual ~AiTask3();                             // 0: 0x4d6b00 (deleting), 0x4d6c00
    virtual OrderBase* Step() override;             // 5: 0x4d6d30

    OrderBase* StepA();                             // 0x4d6da0 (stage 0x3ed)
    OrderBase* StepB();                             // 0x4d6e70 (stage 0x3ee)
    OrderBase* StepTargets();                       // 0x4d7060 (stage 0x3ec)
    int PickDefenseTarget();                        // 0x4d71e0
    OrderBase* BuildDefenseScrapOrder();            // 0x4d7330
    int IsOverQuota();                              // 0x4d76a0
    int PickFleetSystem();                          // 0x4d76f0
    int ChooseManager();                            // 0x4d7ac0
    OrderBase* BuildSurplusScrapOrder();            // 0x4d7cf0
    int ComputeClassCount();                        // 0x4d8460
    OrderBase* BuildDefenseOrder();                 // 0x4d8520
    int SelectTarget();                             // 0x4d8ad0
    int PickSupplySystem();                         // 0x4d8db0
    OrderBase* BuildScrapOrder();                   // 0x4d8fe0
    int ComputeFacilityCount();                     // 0x4d9520
    OrderBase* BuildCapitalOrder();                 // 0x4d95c0
    int PickShieldTarget();                         // 0x4d98a0
    int PickIonTarget();                            // 0x4d99d0
    OrderBase* BuildAllScrapOrder();                // 0x4d9b00
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4d9e70
    OrderBase* BuildManufactureOrder210();          // 0x4d9fb0
    void AddTargetSystem();                         // 0x4da0d0
    int Need();                                     // 0x4da4d0
    int NeedForTargets();                           // 0x4da530
    void RefreshTargets();                          // 0x4da610
};

// Type 7: fighters again, for the ships of the side's systems (VT 0x65c7b8, 0x7c bytes, 0x4d2ed0-0x4d6b00). Three stages: 0x3f1 moves
// and scraps fighter squadrons between systems, 0x3f2 builds ships, 0x3ec handles the target systems.
class AiTask7 : public AiFacilityTaskBase {
public:
    explicit AiTask7(int view);                     // 0x4d2ef0
    virtual ~AiTask7();                             // 0: 0x4d2ed0 (deleting), 0x4d2fd0
    virtual OrderBase* Step() override;             // 5: 0x4d3100

    OrderBase* StepSquadrons();                     // 0x4d3170 (stage 0x3f1)
    OrderBase* StepShips();                         // 0x4d3220 (stage 0x3f2)
    OrderBase* StepTargets();                       // 0x4d3350 (stage 0x3ec)
    int PickFleetSystem();                          // 0x4d34c0
    int PickFleetTarget();                          // 0x4d3610
    OrderBase* BuildMoveOrder();                    // 0x4d3a90
    OrderBase* BuildSquadronScrapOrder();           // 0x4d3db0
    int ChooseManager();                            // 0x4d3ff0
    int PickShipSystem();                           // 0x4d4370
    int ComputeShipCount();                         // 0x4d46e0
    int IsShipQuotaExceeded();                      // 0x4d47b0
    OrderBase* BuildShipOrder();                    // 0x4d4800
    int SelectTarget();                             // 0x4d5000
    int PickSupplySystem();                         // 0x4d52e0
    OrderBase* BuildScrapOrder();                   // 0x4d5510
    int ComputeFacilityCount();                     // 0x4d5a50
    OrderBase* BuildCapitalOrder();                 // 0x4d5af0
    OrderBase* BuildFleetDestinationOrder(ObjectKey* target);  // 0x4d5dd0
    OrderBase* BuildManufactureOrder211();          // 0x4d5f10
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4d6030
    OrderBase* BuildManufactureOrder210();          // 0x4d6170
    void AddTargetSystem();                         // 0x4d6290
    int Need();                                     // 0x4d66a0
    int NeedForTargets();                           // 0x4d6700
    void RefreshTargets();                          // 0x4d67e0
};

// ---- type 11: fleets ------------------------------------------------------------------------------------------------------------

// A facility-family task (ai_task_family.h) for the side's capital ships: it keeps fleets moving to the systems that need them,
// sends troops to systems with warships, and picks the class of ship the side lacks (VT 0x65c6d8, 0x88 bytes,
// 0x4c8800-0x4cc690). Stage 0x3f8 handles the fleets, 0x3f9 the ship classes, 0x3ec the systems.
class AiTask11 : public AiStagedTask {
public:
    explicit AiTask11(int view);                    // 0x4c8820
    virtual ~AiTask11();                            // 0: 0x4c8800 (deleting), 0x4c8900
    virtual void Read(istream* in) override;        // 1: 0x4c89b0
    virtual void Write(ostream* out) override;      // 2: 0x4c8a70
    virtual OrderBase* Step() override;             // 5: 0x4c8b30

    OrderBase* StepFleets();                        // 0x4c8ba0 (stage 0x3f8)
    OrderBase* StepClasses();                       // 0x4c8c20 (stage 0x3f9)
    OrderBase* StepTargets();                       // 0x4c8d20 (stage 0x3ec)
    OrderBase* BuildFleetMoveOrder();               // 0x4c8e90
    OrderBase* BuildFleetOrder();                   // 0x4c94c0
    OrderBase* BuildTroopOrder();                   // 0x4c9cb0
    int ChooseManager();                            // 0x4ca300
    void ChooseClassMask();                         // 0x4ca5e0
    int ChooseClass();                              // 0x4ca910
    int IsOverQuota();                              // 0x4caad0
    OrderBase* BuildShipScrapOrder();               // 0x4cab20
    int SelectTarget();                             // 0x4cacc0
    int PickSupplySystem();                         // 0x4cafa0
    OrderBase* BuildScrapOrder();                   // 0x4cb1d0
    int ComputeFacilityCount();                     // 0x4cb710
    OrderBase* BuildCapitalOrder();                 // 0x4cb7b0
    OrderBase* BuildManufactureOrder211();          // 0x4cba90
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4cbbb0
    OrderBase* BuildManufactureOrder210();          // 0x4cbcf0
    void AddTargetSystem();                         // 0x4cbe10
    int Need();                                     // 0x4cc220
    int NeedForTargets();                           // 0x4cc280
    void RefreshTargets();                          // 0x4cc360
    aitask::FamilyFields Fields();

    AiRankedSelector m_selector;  // +0x40
    ObjectKey m_manager;          // +0x50
    ObjectKey m_unit;             // +0x54
    ObjectKey m_system;           // +0x58
    ObjectKey m_classKey;         // +0x5c starts as 0
    int m_classFlags;             // +0x60 the flags of the class to build (ChooseClassMask)
    int m_classMask;              // +0x64
    int m_claimBit;               // +0x68 the m_flags28 bit that records the choice
    int m_count;                  // +0x6c
    int m_cap;                    // +0x70
    int m_limit;                  // +0x74
    ObjectKeySet m_targets;       // +0x78
    ObjectKeySet m_dropped;       // +0x80
};

// ---- type 6: troops ---------------------------------------------------------------------------------------------------------

// A facility-family task for troop regiments: stage 0x3ef sends regiments to the systems that lack them, 0x3f0 picks the kind of
// regiment the side lacks (m_round walks through five ways to pick the system), 0x3ec handles the facilities (VT 0x65c818,
// 0x8c bytes, 0x4dd440-0x4e1e30).
class AiTask6 : public AiStagedTask {
public:
    explicit AiTask6(int view);                     // 0x4dd460
    virtual ~AiTask6();                             // 0: 0x4dd440 (deleting), 0x4dd550
    virtual void Read(istream* in) override;        // 1: 0x4dd600
    virtual void Write(ostream* out) override;      // 2: 0x4dd6d0
    virtual OrderBase* Step() override;             // 5: 0x4dd7a0

    OrderBase* StepMoves();                         // 0x4dd840 (stage 0x3ef)
    OrderBase* StepClasses();                       // 0x4dd8d0 (stage 0x3f0)
    OrderBase* StepTargets();                       // 0x4dda00 (stage 0x3ec)
    OrderBase* BuildTroopMoveOrderA();              // 0x4ddb70
    OrderBase* BuildTroopMoveOrderB();              // 0x4de130
    OrderBase* BuildTroopMoveOrderC();              // 0x4de6f0
    OrderBase* BuildTroopMoveOrderD();              // 0x4deba0
    int ChooseManager();                            // 0x4df160
    int ChooseKind();                               // 0x4df440
    int ComputeClassCount();                        // 0x4df9a0
    int ResetCount();                               // 0x4dfa70
    OrderBase* BuildTroopScrapOrder();              // 0x4dfa80
    int SelectTarget();                             // 0x4dfcf0
    int PickSupplySystem();                         // 0x4dffd0
    OrderBase* BuildScrapOrder();                   // 0x4e0170
    int ComputeFacilityCount();                     // 0x4e06b0
    OrderBase* BuildCapitalOrder();                 // 0x4e0750
    OrderBase* BuildFleetDestinationOrder(ObjectKey* target);  // 0x4e0a30
    OrderBase* BuildManufactureOrder212();          // 0x4e0b70
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4e0c90
    OrderBase* BuildManufactureOrder210();          // 0x4e0dd0
    void PickByFleetDistance();                     // 0x4e0ef0
    void PickBySectorDistanceA();                   // 0x4e1060
    void PickBySectorRank();                        // 0x4e11a0
    void PickByParentSystem();                      // 0x4e12e0
    void PickBySectorDistanceB();                   // 0x4e1430
    void AddTargetSystem();                         // 0x4e15b0
    int Need();                                     // 0x4e19c0
    int NeedForTargets();                           // 0x4e1a20
    void RefreshTargets();                          // 0x4e1b00
    aitask::FamilyFields Fields();

    AiRankedSelector m_selector;  // +0x40
    ObjectKey m_manager;          // +0x50
    ObjectKey m_unit;             // +0x54
    ObjectKey m_system;           // +0x58
    ObjectKey m_classKey;         // +0x5c starts as 0
    int m_count;                  // +0x60
    int m_cap;                    // +0x64
    int m_limit;                  // +0x68
    int m_classFlags;             // +0x6c ChooseClass arguments set by the Pick functions
    int m_classMode;              // +0x70
    int m_classOrder;             // +0x74
    int m_round;                  // +0x78 which of the four orders ChooseKind uses next
    ObjectKeySet m_targets;       // +0x7c
    ObjectKeySet m_dropped;       // +0x84
};

// ---- type 1: shields and ion cannons ------------------------------------------------------------------------------------------

// A facility-family task (ai_task_family.h) that picks one system at a time for the side's defense facilities (VT 0x65c7f8,
// 0x78 bytes, 0x4da930-0x4dd440). Stage 0 chooses the target, stage 0x3ec builds and scraps.
class AiTask1 : public AiStagedTask {
public:
    explicit AiTask1(int view);                     // 0x4da950
    virtual ~AiTask1();                             // 0: 0x4da930 (deleting), 0x4daa20
    virtual void Read(istream* in) override;        // 1: 0x4daac0
    virtual void Write(ostream* out) override;      // 2: 0x4dab50
    virtual OrderBase* Step() override;             // 5: 0x4dabe0

    int ChooseTarget();                             // 0x4daca0
    int PickSystem();                               // 0x4daf10
    OrderBase* StepBuild();                         // 0x4db2f0 (stage 0x3ec)
    int SelectTarget();                             // 0x4db510
    OrderBase* BuildScrapOrder();                   // 0x4db820
    int ComputeFacilityCount();                     // 0x4dbd60
    OrderBase* BuildCapitalOrder();                 // 0x4dbe70
    int PickShieldTarget();                         // 0x4dc150
    int PickIonTarget();                            // 0x4dc3f0
    OrderBase* BuildAllScrapOrder();                // 0x4dc650
    OrderBase* BuildDestinationOrder();             // 0x4dc9f0
    OrderBase* BuildManufactureOrder210();          // 0x4dcb30
    void AddTargetSystem();                         // 0x4dcc40
    int NeedForTargets();                           // 0x4dd050
    void RefreshTargets();                          // 0x4dd120
    aitask::FamilyFields Fields();

    ObjectKey m_system;           // +0x40
    ObjectKey m_manager;          // +0x44
    ObjectKey m_classKey;         // +0x48 starts as 0
    int m_count;                  // +0x4c
    int m_limit;                  // +0x50
    int m_cap;                    // +0x54
    ObjectKeySet m_targets;       // +0x58
    ObjectKeySet m_dropped;       // +0x60
    AiRankedSelector m_selector;  // +0x68
};

// ---- type 2: shields, ion cannons and their facilities ------------------------------------------------------------------------

// The last task of the facility family (VT 0x65c838, 0x80 bytes, 0x4e1e30-0x4e4d40). Stage 0 decides which of the three stages to
// run (0x3ea: shields, 0x3eb: ion cannons, 0x3ec: the facilities).
class AiTask2 : public AiStagedTask {
public:
    explicit AiTask2(int view);                     // 0x4e1e50
    virtual ~AiTask2();                             // 0: 0x4e1e30 (deleting), 0x4e1f30
    virtual void Read(istream* in) override;        // 1: 0x4e1fe0
    virtual void Write(ostream* out) override;      // 2: 0x4e2080
    virtual OrderBase* Step() override;             // 5: 0x4e2120

    int ChooseManager();                            // 0x4e21d0
    OrderBase* StepFacilities();                    // 0x4e2400 (stage 0x3ec)
    OrderBase* StepDefenses();                      // 0x4e25c0 (stages 0x3ea, 0x3eb)
    int PickSystem();                               // 0x4e2760
    OrderBase* BuildFirstScrapOrder();              // 0x4e2940
    int PickShieldTarget();                         // 0x4e2c70
    int PickIonTarget();                            // 0x4e2f10
    OrderBase* BuildAllScrapOrder(ObjectKey* system);  // 0x4e31b0
    int SelectTarget();                             // 0x4e3530
    int PickSupplySystem();                         // 0x4e3810
    OrderBase* BuildScrapOrder();                   // 0x4e3a40
    int ComputeFacilityCount();                     // 0x4e3f80
    OrderBase* BuildCapitalOrder();                 // 0x4e4020
    OrderBase* BuildDestinationOrder(ObjectKey* target);  // 0x4e4300
    OrderBase* BuildManufactureOrder210();          // 0x4e4440
    void AddTargetSystem();                         // 0x4e4540
    int NeedForTargets();                           // 0x4e4950
    void RefreshTargets();                          // 0x4e4a20
    aitask::FamilyFields Fields();

    ObjectKey m_shieldSystem;     // +0x40
    ObjectKey m_ionSystem;        // +0x44
    ObjectKey m_system;           // +0x48
    ObjectKey m_manager;          // +0x4c
    ObjectKey m_classKey;         // +0x50 starts as 0
    int m_count;                  // +0x54
    int m_cap;                    // +0x58
    int m_limit;                  // +0x5c
    ObjectKeySet m_targets;       // +0x60
    ObjectKeySet m_dropped;       // +0x68
    AiRankedSelector m_selector;  // +0x70
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
