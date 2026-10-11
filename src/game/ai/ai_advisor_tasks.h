#pragma once
#include <cstdint>

#include "ai/ai_galaxy_analysis.h"
#include "ai/ai_selector.h"
#include "ai/ai_task_base.h"
#include "core/containers.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"
#include "strategy_ui/advisor_notices.h"

// The tasks of the player's advisors (0x49c6a0-0x49ecb0, 0x4c6ab0-0x4c7e10): small cousins of the AI's task types that work
// from an AiGalaxyAnalysis and hand the orders they build to the advisor droid (the human player's helper that proposes and
// starts building, shipbuilding and troop orders).

class AdvisorBase;
class AiSideController;
class OrderBase;
class ManufactureOrderBase;
class System;

// ---- AdvisorTaskBase --------------------------------------------------------------------------------------------------

// What the advisor tasks are built on (VT 0x65c670, 0x34 bytes, ctor 0x4c7d20): like AiTaskBase but working from an analysis.
class AdvisorTaskBase : public AiTaskNode {
public:
    AdvisorTaskBase(int id, int view);                // 0x4c7d20
    virtual ~AdvisorTaskBase();                       // 0: 0x4c7d00 (deleting), 0x4c7d50
    virtual void Read(istream* in) override;          // 1: 0x4c7d70
    virtual void Write(ostream* out) override;        // 2: 0x4c7dc0
    virtual void SetAnalysis(AiGalaxyAnalysis* analysis) { m_analysis = analysis; }  // 3: 0x49eca0
    virtual void Evaluate();                          // 4: 0x4cc990 (shared with AiStagedTask)
    virtual OrderBase* Step() = 0;                    // 5
    virtual void Refused(int* status, OrderBase* order) = 0;  // 6

    int m_finished;                 // +0x20
    int m_x24;                      // +0x24
    AiGalaxyAnalysis* m_analysis;   // +0x28
    int m_view;                     // +0x2c
    int m_count;                    // +0x30 orders made; AdvisorTaskManager runs the task with the most
};

// ---- the tasks --------------------------------------------------------------------------------------------------------

// Type 0x14 (VT 0x65c630, 0x58 bytes): finds a manufacturing manager that builds facilities and a system in need of them and
// orders one built (orders 0x214 and 0x210).
class AdvisorTask20 : public AdvisorTaskBase {
public:
    explicit AdvisorTask20(int view);                 // 0x4c6ad0
    virtual ~AdvisorTask20();                         // 0: 0x4c6ab0 (deleting), 0x4c6b60
    virtual void Read(istream* in) override;          // 1: 0x4c6be0
    virtual void Write(ostream* out) override;        // 2: 0x4c6c40
    virtual OrderBase* Step() override;               // 5: 0x4c6ca0
    virtual void Refused(int* status, OrderBase* order) override;  // 6: 0x4c7550 (shared with AdvisorTask21)

    OrderBase* Advance();                             // 0x4c6ce0
    int PickManager();                                // 0x4c6d90
    int PickSupplySystem();                           // 0x4c6f00
    OrderBase* BuildDestinationOrder();               // 0x4c70e0
    OrderBase* BuildManufactureOrder();               // 0x4c7220

    int m_x34;                    // +0x34
    int m_state;                  // +0x38
    ObjectKey m_supply;           // +0x3c
    ObjectKey m_manager;          // +0x40
    ObjectKey m_item;             // +0x44 (starts as 0)
    AiRankedSelector m_selector;  // +0x48
};

// Type 0x15 (VT 0x65c650, 0x5c bytes): the same for ships (orders 0x214 and 0x212).
class AdvisorTask21 : public AdvisorTaskBase {
public:
    explicit AdvisorTask21(int view);                 // 0x4c7340
    virtual ~AdvisorTask21();                         // 0: 0x4c7320 (deleting), 0x4c73d0
    virtual void Read(istream* in) override;          // 1: 0x4c7450
    virtual void Write(ostream* out) override;        // 2: 0x4c74b0
    virtual OrderBase* Step() override;               // 5: 0x4c7510
    virtual void Refused(int* status, OrderBase* order) override;  // 6: 0x4c7550 (shared with AdvisorTask20)

    OrderBase* Advance();                             // 0x4c7580
    int ChooseTarget();                               // 0x4c7650
    int PickSite();                                   // 0x4c7860
    int ComputeCount();                               // 0x4c79e0
    OrderBase* BuildDestinationOrder();               // 0x4c7a90
    OrderBase* BuildManufactureOrder();               // 0x4c7bd0

    int m_x34;                    // +0x34
    int m_state;                  // +0x38
    int m_units;                  // +0x3c how many to build
    ObjectKey m_target;           // +0x40
    ObjectKey m_site;             // +0x44
    ObjectKey m_item;             // +0x48 (starts as 0)
    AiRankedSelector m_selector;  // +0x4c
};

// Type 0x17 (VT 0x65bc98, 0x58 bytes): builds what the player asked the advisor for (m_kind 1-3: facilities, ships, troops)
// at a target the player picked; makes the order and opens the manufacture window on it.
class AdvisorTask23 : public AdvisorTaskBase {
public:
    explicit AdvisorTask23(int view);                 // 0x49e1c0
    virtual ~AdvisorTask23();                         // 0: 0x49e1a0 (deleting), 0x49e240
    virtual void Read(istream* in) override;          // 1: 0x49e2c0
    virtual void Write(ostream* out) override;        // 2: 0x49e310
    virtual OrderBase* Step() override;               // 5: 0x49e600
    virtual void Refused(int* status, OrderBase* order) override;  // 6: 0x49e660

    void Start(int kind);                             // 0x49e360
    void SetTarget(const ObjectKey* target);          // 0x49e380: the object the player clicked
    OrderBase* Advance();                             // 0x49e6b0
    int PickSite();                                   // 0x49e7a0
    OrderBase* BuildDestinationOrder();               // 0x49e960
    OrderBase* BuildManufactureOrder(int kind);       // 0x49ea40
    int OrderKind();                                  // 0x49eb50
    void GetFacilityMask(uint32_t* out);              // 0x49eb80
    void GetSystemMask(uint32_t* out);                // 0x49ebc0
    GameObjNotifier* FindManager(GameObjNotifier* system);  // 0x49ec00
    void OpenWindow(ManufactureOrderBase* order);     // 0x49ec40

    int m_kind;                   // +0x34
    int m_phase;                  // +0x38
    ObjectKey m_target;           // +0x3c
    ObjectKey m_site;             // +0x40
    AiRankedSelector m_selector;  // +0x44
    AdvisorBase* m_advisor;       // +0x54
};

// ---- the collections --------------------------------------------------------------------------------------------------

// The advisor's tasks (VT 0x65bc88, 0x1c bytes): AiTaskManager's twin for the three advisor task types.
class AdvisorTaskManager : public IdAvlTree {
public:
    AdvisorTaskManager();                         // 0x49dcf0
    virtual ~AdvisorTaskManager();                // 0: 0x49dd10 (deleting), 0x49dd30
    void Load(istream* in, AiGalaxyAnalysis* analysis);  // 0x49dd50
    void Save(ostream* out);                      // 0x49de40 (the code AiTaskManager::Save shares)
    uint32_t CreateTask(int type, AiGalaxyAnalysis* analysis);  // 0x49df20
    AdvisorTaskBase* SelectNext();                // 0x49df70
    AdvisorTaskBase* MakeTask(int type);          // 0x49e010
    AdvisorTaskBase* PickBest();                  // 0x49e130

    int m_view;                   // +0x0c
    AdvisorTaskBase* m_current;   // +0x10
    AdvisorTaskBase* m_cursor;    // +0x14
    AdvisorTaskBase* m_last;      // +0x18
};

// The tree of DroidScript (VT 0x65bc78, 0xc bytes).
class AdvisorEntryTree : public IdAvlTree {
public:
    AdvisorEntryTree();                           // 0x49c6a0
    virtual ~AdvisorEntryTree();                  // 0: 0x49c6c0 (deleting), 0x49c6e0
    void Load(istream* in);                       // 0x49c6f0
    void Save(ostream* out);                      // 0x4e5210 (the code AiTaskObjectSet::Save shares)
};
