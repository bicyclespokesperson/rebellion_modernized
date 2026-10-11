#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/containers.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The base classes of the AI player's task machinery (0x4e4d40-0x4e60c0, with the small node classes they build on at
// 0x4ec920-0x4ecbc0). Part 2 of the AI module derives the 14 task types (ids 1-14), created by AiTaskManager::MakeTask, from
// AiTaskBase and the strategy evaluators from AiTaskEvaluator.

class AiSideController;
class AiSystemRecord;
class OrderBase;
class PlayerController;

// A note the AI player has been sent (the objects CreateKeyedObject builds; a PlayerNotice of the player module). The AI keeps
// them in AiNoticeQueue, grouped by m_groupId.
class AiNotice : public LinkNode {
public:
    virtual int GetKind() = 0;       // 3
    virtual void Virtual04() = 0;    // 4
    virtual void Virtual05() = 0;    // 5
    virtual void Virtual06(PlayerController* player) = 0;    // 6: called with the AI player when it takes the note
    virtual void Virtual07(int value) = 0;  // 7: called with 0 when a note that belongs to a group is taken

    int m_groupId;  // +0x0c
};

// ---- AiTaskNode / AiTaskBase ------------------------------------------------------------------------------------------

// An id node with a flag word (VT 0x65ca30, 0x20 bytes): what every AI task is built on.
class AiTaskNode : public IdNode {
public:
    explicit AiTaskNode(int id);              // 0x4ec940
    virtual ~AiTaskNode();                    // 0: 0x4ec920 (deleting), 0x4ec9b0
    virtual void Read(istream* in);           // 1: 0x4eca00
    virtual void Write(ostream* out);         // 2: 0x4eca20

    PersistentInt m_flags;  // +0x1c bit 0: the task wants to run (the task manager only picks tasks with the value 1)
};

// The base of the AI's 14 task types (VT 0x65c868, ctor 0x4e4fb0). The id is the task type. AiPlayer asks the tasks in turn:
// Evaluate() decides whether the task wants to run, Step() produces the next order of the running task and Refused() is told
// when the order was not accepted.
class AiTaskBase : public AiTaskNode {
public:
    AiTaskBase(int id, int view);             // 0x4e4fb0
    virtual ~AiTaskBase();                    // 0: 0x4e4f90 (deleting), 0x4e5020
    virtual void Read(istream* in) override;  // 1: 0x4e5070
    virtual void Write(ostream* out) override;  // 2: 0x4e50c0
    virtual void SetOwner(AiSideController* owner) { m_owner = owner; }  // 3: 0x4c06e0 (shared)
    virtual void Evaluate() = 0;              // 4
    virtual OrderBase* Step() = 0;            // 5: the next order of the task, null if it has none
    virtual void Refused(int* result, OrderBase* order) = 0;  // 6

    int m_finished;               // +0x20 the task has finished its pass
    int m_x24;                    // +0x24
    PersistentInt m_flags28;      // +0x28
    AiSideController* m_owner;    // +0x2c
    int m_view;                   // +0x30 the side
    int m_priority;               // +0x34 AiTaskManager picks the highest
};

// ---- AiNoticeGroup -----------------------------------------------------------------------------------------------------

// The notes with one group id, in arrival order (0x2c bytes: a SortedLinkList whose IdNode part at +0xc is what AiNoticeQueue
// keeps in its tree; VT 0x65c858 / 0x65c860).
class AiNoticeGroup : public SortedLinkList, public IdNode {
public:
    AiNoticeGroup();                          // 0x4e4d40
    virtual ~AiNoticeGroup();                 // 0: 0x4e4da0 (deleting), 0x4e4dc0 (the IdNode part's slot is 0x4e4f70)
    virtual int IsLess(LinkNode* a, LinkNode* b) override;  // 1: 0x4e4f80 (0: notes stay in arrival order)

    void Load(istream* in);                   // 0x4e4e20
    void Save(ostream* out);                  // 0x4e4eb0
    void Add(AiNotice* notice);               // 0x4e4f20: takes the group's id from the first note; kind 2 marks it finished

    int m_finished;  // +0x28 set when a note of kind 2 was added
};

// ---- AiTaskObject / AiTaskObjectSet ------------------------------------------------------------------------------------

// A key with two numbers (VT 0x65ca40, 0x28 bytes): the entries of an evaluator's list of systems or sectors.
class AiTaskObject : public IdNode {
public:
    AiTaskObject();                                                 // 0x4eca40
    AiTaskObject(const ObjectKey& key, int first, int second);      // 0x4ecac0
    virtual ~AiTaskObject();                                        // 0: 0x4ecaa0 (deleting), 0x4ecb30
    virtual void Read(istream* in);                                 // 1: 0x4ecb80
    virtual void Write(ostream* out);                               // 2: 0x4ecbc0

    ObjectKey m_key;  // +0x1c
    int m_first;      // +0x20
    int m_second;     // +0x24
};

class AiTaskObjectSet : public IdAvlTree {
public:
    AiTaskObjectSet();                        // 0x4e5110
    virtual ~AiTaskObjectSet();               // 0: 0x4e5130 (deleting), 0x4e5150
    void Load(istream* in);                   // 0x4e5160
    void Save(ostream* out);                  // 0x4e5210
    AiTaskObject* FindByKey(const ObjectKey* key);  // 0x4e5260
};

// ---- AiTaskEvaluator ---------------------------------------------------------------------------------------------------

// What an AI strategy evaluator does every pass (VT 0x65c898, 0x2c bytes + the part 2 subclasses): Run() steps through six
// phases, each of them asks the subclass for what it wants (slots 5-8) and turns the answer into figures in the AiSideController
// (mission kind and subtype, the planner and the agent to use).
class AiTaskEvaluator {
public:
    AiTaskEvaluator(int view, AiSideController* owner);  // 0x4e52c0
    virtual ~AiTaskEvaluator();                // 0: 0x4e52a0 (deleting), 0x4e5330
    virtual void Read(istream* in);            // 1: 0x4e5380
    virtual void Write(ostream* out);          // 2: 0x4e53e0
    virtual int GetKind() = 0;                 // 3: the plan kind (1-7) the evaluator belongs to
    // 4: the chance (0-0.9) that the side accepts the vote `key` of the other side, put into *chance
    virtual void ScoreVote(const ObjectKey* key, float* chance) = 0;
    virtual void Prepare() = 0;                // 5: when the side's capabilities changed (owner flag 2)
    // 6: the mission the evaluator wants started (kind and subtype) and the planner that does it (family and subtype)
    virtual void GetPlannerRequest(int* missionKind, int* missionSubtype, int* plannerKind, int* plannerSubtype) = 0;
    virtual void GetAgentRequest(int* agentTarget, int* wantAgent) = 0;  // 7
    virtual void Finish() = 0;                 // 8

    int Run();                                 // 0x4e5440: 1 when the pass is complete
    void ClearRecordFlags();                   // 0x4e54f0
    void PhasePrepare();                       // 0x4e5530
    void PhaseUpdateTiers();                   // 0x4e5560
    void PhasePickPlanner();                   // 0x4e5710
    void PhasePickAgent();                     // 0x4e57e0
    void PhaseFinish();                        // 0x4e5870
    // The sector (key in *sector) the evaluator should work on, picked by what the side lacks; returns which kind of
    // facility (1 manufacturing, 2 defense, 3 troop) the bits of the side's cursor point at, 0 for none.
    int ChooseSector(int useAge, ObjectKey* sector);  // 0x4e5b10

    int m_view;                      // +0x04
    int m_state;                     // +0x08
    AiSideController* m_owner;       // +0x0c
    PersistentInt m_flags;           // +0x10
    int m_x14;                       // +0x14
    int m_x18;                       // +0x18
    AiRankedSelector m_selector;     // +0x1c
    AiTaskObjectSet* m_objects;      // +0x2c the sectors the evaluator was asked to handle (set by the subclass)
};
