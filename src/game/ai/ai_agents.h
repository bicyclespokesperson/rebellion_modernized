#pragma once
#include "ai/ai_agent_bases.h"

// The concrete character agents (0x479a40-0x47aa90, kinds 1, 3, 4, 5, 6 of AiAgentSet::CreateAgent). Each looks for the character
// that does its job (Evaluate), the system the character should go to (Prepare/ChooseTarget for the simple agents, the
// AiSystemRecord the agent serves for the targeted ones) and the order to send it with (GetOrderKind):
//
//   kind 1  AiAgentKind1  0x4c  simple, order 0x263 (VT 0x65a490): recruits a character into a system that needs one
//   kind 3  AiAgentKind3  0x5c  targeted, order 0x262, character flag filter 0x1000000 (VT 0x65a460)
//   kind 4  AiAgentKind4  0x5c  targeted, order 0x263, character flag filter 0x4000000 (VT 0x65a430)
//   kind 5  AiAgentKind5  0x5c  targeted, order 0x261, character flag filter 0x2000000 (VT 0x65a400)
//   kind 6  AiAgentKind6  0x4c  simple, no order (VT 0x65a3c0)
//
// Slots 1 and 2 of every kind are forwarders to AiAgentSimple/AiAgentTargeted::Read/Write.

class AiAgentKind6 : public AiAgentSimple {
public:
    AiAgentKind6(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x479a60
    virtual ~AiAgentKind6();                                                            // 0: 0x479a40 (deleting), 0x479a90
    virtual int GetKind() override { return 6; }                                        // 6: 0x4953a0
    virtual int GetOrderKind() override { return 0; }                                   // 7: 0x6158b0
    virtual int GetCountSlot() override;                                                // 10: 0x479ac0
    virtual void ReleaseHook() override {}                                              // 11: 0x619730
    virtual ObjectKey* ChooseTarget(ObjectKey* out) override;                           // 12: 0x479e30
    virtual void Prepare() override;                                                    // 13: 0x479ad0
    virtual int Evaluate() override;                                                    // 14: 0x479c00
};

class AiAgentKind1 : public AiAgentSimple {
public:
    AiAgentKind1(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47a430
    virtual ~AiAgentKind1();                                                            // 0: 0x47a410 (deleting), 0x47a460
    virtual int GetKind() override { return 1; }                                        // 6: 0x40f340
    virtual int GetOrderKind() override { return 0x263; }                               // 7: 0x47a040
    virtual int GetCountSlot() override;                                                // 10: 0x47a470
    virtual void ReleaseHook() override;                                                // 11: 0x47a850
    virtual ObjectKey* ChooseTarget(ObjectKey* out) override;                           // 12: 0x47a870
    virtual void Prepare() override;                                                    // 13: 0x47a480
    virtual int Evaluate() override;                                                    // 14: 0x47a5b0
};

class AiAgentKind5 : public AiAgentTargeted {
public:
    AiAgentKind5(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x479f70
    virtual ~AiAgentKind5();                                                            // 0: 0x479f50 (deleting), 0x479fa0
    virtual int GetKind() override { return 5; }                                        // 6: 0x404c20
    virtual int GetOrderKind() override;                                                // 7: 0x479fd0
};

class AiAgentKind4 : public AiAgentTargeted {
public:
    AiAgentKind4(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47a000
    virtual ~AiAgentKind4();                                                            // 0: 0x479fe0 (deleting), 0x47a030
    virtual int GetKind() override { return 4; }                                        // 6: 0x4047d0
    virtual int GetOrderKind() override;                                                // 7: 0x47a040
    virtual int Evaluate() override;                                                    // 10: 0x47a050
};

class AiAgentKind3 : public AiAgentTargeted {
public:
    AiAgentKind3(int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x47a3c0
    virtual ~AiAgentKind3();                                                            // 0: 0x47a3a0 (deleting), 0x47a3f0
    virtual int GetKind() override { return 3; }                                        // 6: 0x526f00
    virtual int GetOrderKind() override;                                                // 7: 0x47a400
};
