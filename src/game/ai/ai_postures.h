#pragma once
#include <cstdint>

#include "ai/ai_posture.h"

// The six postures an AiSystemRecord can take (AiSystemRecord::CreatePosture kinds 1-6, 0x4b1720-0x4bb9f0). Each overrides the
// choosers of AiPosture (Virtual09-12: which system and object to send characters and forces to), the fleet assignment steps
// of the Step state machine (Virtual13-18) and the strength it asks for (Virtual20). The kind is the value of the sector's
// AiSectorEval::m_x30.
//
// Slots 1 and 2 of kinds 1, 3, 5 and 6 are forwarders to AiPosture::Read/Write (0x4ba5c0/0x4ba5d0).

// Kind 5: VT 0x65c108, 0x5c bytes.
class AiPostureKind5 : public AiPosture {
public:
    AiPostureKind5(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4b1720
    virtual ~AiPostureKind5();                                                  // 0: 0x4b1700 (deleting), 0x4b1750
    virtual int Virtual03() override;                                           // 3: 0x4b17b0
    virtual int GetKind() override { return 5; }                                // 5: 0x499cd0
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                       // 7: 0x4b1800
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4b1760
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4b1880
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4b1bf0
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4b1db0
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4b1f80
    virtual int Virtual13() override;                                           // 13: 0x4b24b0
    virtual OrderBase* Virtual14() override;                                    // 14: 0x4b28c0
    virtual int Virtual15() override;                                           // 15: 0x4b2d70
    virtual int Virtual17() override;                                           // 17: 0x4b3110
    virtual int Virtual18() override;                                           // 18: 0x4b2f40
    virtual void Virtual20() override;                                           // 20: 0x4b3300
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4b3400
};

// Kind 3: VT 0x65c160, 0x5c bytes.
class AiPostureKind3 : public AiPosture {
public:
    AiPostureKind3(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4b37f0
    virtual ~AiPostureKind3();                                                  // 0: 0x4b37d0 (deleting), 0x4b3820
    virtual int GetKind() override { return 3; }                                // 5: 0x5a6730
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                       // 7: 0x4b1800
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4b3830
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4b3880
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4b3bf0
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4b3db0
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4b3f80
    virtual int Virtual15() override;                                           // 15: 0x4b4560
    virtual int Virtual17() override;                                           // 17: 0x4b4730
    virtual void Virtual20() override;                                           // 20: 0x4b4920
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4b4a20
};

// Kind 6: VT 0x65c1b8, 0x5c bytes.
class AiPostureKind6 : public AiPosture {
public:
    AiPostureKind6(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4b4d50
    virtual ~AiPostureKind6();                                                  // 0: 0x4b4d30 (deleting), 0x4b4d80
    virtual int Virtual03() override;                                           // 3: 0x4b4df0
    virtual int GetKind() override { return 6; }                                // 5: 0x48d640
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                      // 7: 0x4b72d0
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4b4d90
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4b4e40
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4b51b0
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4b5370
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4b5540
    virtual int Virtual13() override;                                           // 13: 0x4b5b20
    virtual OrderBase* Virtual14() override;                                    // 14: 0x4b5f30
    virtual int Virtual15() override;                                           // 15: 0x4b6340
    virtual int Virtual16() override;                                           // 16: 0x4b6510
    virtual int Virtual17() override;                                           // 17: 0x4b6870
    virtual int Virtual19() override { return HasSeveralAgentsOfClass1(); }     // 19: 0x4b8b30
    virtual void Virtual20() override;                                          // 20: 0x4b6a60
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4b6b70
};

// Kind 4: VT 0x65c210, 0x60 bytes. Adds the system it has chosen to work at (m_target), which ChooseTargetSystem picks
// from the sector's systems that have strength of the kind the sector asks for.
class AiPostureKind4 : public AiPosture {
public:
    AiPostureKind4(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4b6fb0
    virtual ~AiPostureKind4();                                                  // 0: 0x4b6f90 (deleting), 0x4b7010
    virtual void Read(istream* in) override;                                    // 1: 0x4b7060
    virtual void Write(ostream* out) override;                                  // 2: 0x4b7080
    virtual int Virtual03() override;                                           // 3: 0x4b7100
    virtual int GetKind() override { return 4; }                                // 5: 0x5a6700
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                      // 7: 0x4b72d0
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4b70a0
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4b7370
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4b7780
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4b7940
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4b7b10
    virtual int Virtual13() override;                                           // 13: 0x4b8070
    virtual OrderBase* Virtual14() override;                                    // 14: 0x4b8470
    virtual int Virtual15() override;                                           // 15: 0x4b8880
    virtual int Virtual16() override;                                           // 16: 0x4b8ab0
    virtual int Virtual19() override { return HasSeveralAgentsOfClass1(); }     // 19: 0x4b8b30
    virtual void Virtual20() override;                                          // 20: 0x4b8b70
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4b8ca0

    void ChooseTargetSystem();  // 0x4b8f80

    ObjectKey m_target;  // +0x5c
};

// Kind 2: VT 0x65c268, 0x64 bytes. Remembers in m_chosen the systems it has already used for what Virtual08/21 look for.
class AiPostureKind2 : public AiPosture {
public:
    AiPostureKind2(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4b91a0
    virtual ~AiPostureKind2();                                                  // 0: 0x4b9180 (deleting), 0x4b9200
    virtual void Read(istream* in) override;                                    // 1: 0x4b9250
    virtual void Write(ostream* out) override;                                  // 2: 0x4b9270
    virtual int GetKind() override { return 2; }                                // 5: 0x4e8740
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                      // 7: 0x4b9290
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4b9310
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4b94b0
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4b96b0
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4b97e0
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4b99b0
    virtual int Virtual15() override;                                           // 15: 0x4b9c90
    virtual void Virtual20() override;                                          // 20: 0x4b9fe0
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4ba0d0

    ObjectKeySet m_chosen;  // +0x5c
};

// Kind 1: VT 0x65c2c0, 0x5c bytes.
class AiPostureKind1 : public AiPosture {
public:
    AiPostureKind1(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4ba580
    virtual ~AiPostureKind1();                                                  // 0: 0x4ba560 (deleting), 0x4ba5b0
    virtual int GetKind() override { return 1; }                                // 5: 0x5a66f0
    virtual void GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) override;  // 6: 0x4b7150
    virtual void GetAgentRequest(int* agentKind) override;                      // 7: 0x4b9290
    virtual void Virtual08(PersistentInt* flagsA, PersistentInt* flagsB) override;  // 8: 0x4ba5e0
    virtual void Virtual09(ObjectKey* a, ObjectKey* b) override;                // 9: 0x4ba630
    virtual void Virtual10(ObjectKey* a, ObjectKey* b) override;                // 10: 0x4ba830
    virtual void Virtual11(ObjectKey* a, ObjectKey* b) override;                // 11: 0x4ba960
    virtual void Virtual12(ObjectKey* a, ObjectKey* b) override;                // 12: 0x4bab30
    virtual int Virtual15() override;                                           // 15: 0x4bafe0
    virtual int Virtual17() override;                                           // 17: 0x4bb300
    virtual void Virtual20() override;                                          // 20: 0x4bb510
    virtual ObjectKey* Virtual21(ObjectKey* out) override;                      // 21: 0x4bb630
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiPostureKind2, 0x64);
REB_ASSERT_SIZE(AiPostureKind1, 0x5c);
REB_ASSERT_SIZE(AiPostureKind4, 0x60);
REB_ASSERT_SIZE(AiPostureKind5, 0x5c);
REB_ASSERT_SIZE(AiPostureKind3, 0x5c);
REB_ASSERT_SIZE(AiPostureKind6, 0x5c);
#pragma clang diagnostic pop
