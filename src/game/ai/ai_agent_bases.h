#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/containers.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The base classes of the AI's character agents (0x4bb9f0-0x4bce50). An agent drives one character (a record of
// AiCharacterRecordSet) through a mission request: it picks the character, sends it where it is needed and releases it when
// done. AiAgentSet (ai_members.h) creates the concrete agents (kinds 1, 3, 4, 5, 6; part 2 of the AI module).
//
//   AiAgentBase       VT 0x65c358 (0x48 bytes): the state every agent has; slots 3-9 are pure
//   AiAgentSimple     VT 0x65c318 (0x4c): adds the state machine hooks (slots 10-14 pure); kinds 1 and 6 derive from it
//   AiAgentTargeted   VT 0x65c380 (0x5c): agents that send their character to a target system (kinds 3, 4, 5)
//
// m_flags (PersistentInt): 0x1 a character is assigned, 0x2 the character is at the target, 0x4 on its way home, 0x8 at its
// target, 0x10 the character is already where the agent's target is, 0x80000000 the agent has work to do.

class AiSideController;
class AiCharacterRecordSet;
class OrderBase;

class AiAgentBase : public IdNode {
public:
    AiAgentBase(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x4bbd60
    virtual ~AiAgentBase();                       // 0: 0x4bbd40 (deleting), 0x4bbe00
    virtual void Read(istream* in);               // 1: 0x4bbe90
    virtual void Write(ostream* out);             // 2: 0x4bbef0
    virtual int GetAgentClass() = 0;              // 3: 1 for the targeted agents, 2 for the simple ones
    virtual int IsAvailable() = 0;                // 4: no assigned character, or the character is neither captured nor busy
    virtual OrderBase* Step(int* done) = 0;       // 5: one step of the agent's state machine; the order to submit, if any
    virtual int GetKind() = 0;                    // 6
    virtual int GetOrderKind() = 0;               // 7
    virtual void SetTier(int tier) = 0;           // 8
    virtual int GetTier() = 0;                    // 9

    // Orders the character of the agent to do something: creates the message `kind` for the character (null when the
    // key is no character), the caller fills it in and sends it.
    OrderBase* CreateCharacterMessage(int kind);                // 0x4bbf50
    OrderBase* CreateMoveMessage(const ObjectKey* target);      // 0x4bc040: character order 0x201 to `target`
    int CanReach(const ObjectKey* characterKey);                // 0x4bc180: 0 if the character is where a system is closed to it

    int m_state;                   // +0x1c
    PersistentInt m_flags;         // +0x20
    int m_view;                    // +0x24
    ObjectKey m_targetKey;         // +0x28 the system the agent works at
    ObjectKey m_characterKey;      // +0x2c
    AiRankedSelector m_selector;   // +0x30
    AiCharacterRecordSet* m_characters;  // +0x40
    AiSideController* m_owner;     // +0x44
};

class AiAgentSimple : public AiAgentBase {
public:
    AiAgentSimple(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x4bba10
    virtual ~AiAgentSimple();                     // 0: 0x4bb9f0 (deleting), 0x4bba40
    virtual void Read(istream* in) override;      // 1: 0x4bba50
    virtual void Write(ostream* out) override;    // 2: 0x4bba70
    virtual int GetAgentClass() override;         // 3: 0x43c1f0 returns 2
    virtual int IsAvailable() override;           // 4: 0x4bba90
    virtual OrderBase* Step(int* done) override;  // 5: 0x4bbb00
    virtual void SetTier(int tier) override;      // 8: 0x5ae1e0 (empty)
    virtual int GetTier() override;               // 9: 0x6158b0 returns 0
    virtual int GetCountSlot() = 0;               // 10: 0 or 1, the index AiSideController::CountAgent counts the agent under
    virtual void ReleaseHook() = 0;               // 11: called when the agent releases its character
    virtual ObjectKey* ChooseTarget(ObjectKey* out) = 0;  // 12: the system the agent should work at, into *out
    virtual void Prepare() = 0;                   // 13: state 3
    virtual int Evaluate() = 0;                   // 14: state 2, non-zero when ready

    OrderBase* ReleaseCharacter();   // 0x4bbba0
    OrderBase* Execute();            // 0x4bbc50

    int m_x48;                // +0x48
};

class AiAgentTargeted : public AiAgentBase {
public:
    AiAgentTargeted(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x4bc290
    virtual ~AiAgentTargeted();                   // 0: 0x4bc270 (deleting), 0x4bc310
    virtual void Read(istream* in) override;      // 1: 0x4bc370
    virtual void Write(ostream* out) override;    // 2: 0x4bc3c0
    virtual int GetAgentClass() override;         // 3: 0x40f340 returns 1
    virtual int IsAvailable() override;           // 4: 0x4bba90
    virtual OrderBase* Step(int* done) override;  // 5: 0x4bc480
    virtual int GetKind() override = 0;           // 6
    virtual int GetOrderKind() override = 0;      // 7
    virtual void SetTier(int tier) override;      // 8: 0x4bc410 (m_assignment = tier)
    virtual int GetTier() override;               // 9: 0x53b870 (returns m_assignment)
    virtual int Evaluate();                       // 10: 0x4bc6d0: 1 when the agent has its character

    OrderBase* ReleaseCharacter();    // 0x4bc520
    void FindTarget();         // 0x4bc5d0
    OrderBase* Execute();      // 0x4bca20

    ObjectKey m_homeKey;       // +0x48
    int m_assignment;          // +0x4c
    int m_x50;                 // +0x50
    int m_systemRecordId;      // +0x54 id of the AiSystemRecord this agent serves
    ObjectKey m_sectorKey;     // +0x58
};
