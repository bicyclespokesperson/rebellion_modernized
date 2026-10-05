#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// Declaration of the AI's mission planner base class (VT 0x65c3b0, base ctor 0x47aae0), needed by AiPlannerSet,
// AiSystemRecord and the side controller. The planners (one per mission kind 0x51-0x6a, created by AiPlannerSet::CreatePlanner)
// belong to part 2 of the AI module; this is a stand-in meant to be replaced by the full header. Slots keep their order,
// unknown ones are VirtualNN.

class AiSideController;
class AiCharacterRecordSet;

class AiMissionPlanner : public IdNode {
public:
    virtual ~AiMissionPlanner();                  // 0
    virtual void Read(istream* in);               // 1: 0x4bceb0
    virtual void Write(ostream* out);             // 2: 0x4bcec0
    virtual int Virtual03(int argument);       // 3
    virtual int Virtual04();                      // 4: 0x4e8740: the planner's role (1 = works for an AiSystemRecord)
    virtual int GetSubtype();                     // 5: the mission subtype
    virtual int Virtual06();                      // 6
    virtual int Virtual07();                      // 7
    virtual int Virtual08();                      // 8
    virtual int Virtual09();                      // 9
    virtual int Virtual10();                      // 10
    virtual int Virtual11();                      // 11
    virtual int Virtual12(int argument);      // 12
    virtual int Virtual13();                      // 13
    virtual int Virtual14();                      // 14
    virtual int Virtual15();                      // 15
    virtual int GetKindIndex();                   // 16: index of the planner's kind (CountPlanner)
    virtual int Virtual17();                      // 17
    virtual int Virtual18();                      // 18
    virtual int Virtual19();                      // 19
    virtual int Virtual20();                      // 20

    int m_state;                  // +0x1c
    PersistentInt m_flags;        // +0x20
    int m_x24;                    // +0x24
    ObjectKey m_target;           // +0x28
    ObjectKey m_missionKey;       // +0x2c the mission record's key: the family byte is the mission kind (0x51-0x6a)
    uint8_t m_unk30[0x64 - 0x30]; // +0x30
    AiCharacterRecordSet* m_characters;  // +0x64
    AiSideController* m_owner;    // +0x68
    uint8_t m_unk6c[4];           // +0x6c
    uint32_t m_systemRecordId;    // +0x70 id of the AiSystemRecord the planner works for
    ObjectKey m_sectorKey;        // +0x74
};

// The planner and agent constructors of part 2 (called with the memory from operator new): (view, owner, characters).
AiMissionPlanner* __thiscall FUN_00481460(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x51 diplomacy
AiMissionPlanner* __thiscall FUN_00480d10(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x52 subtype 6
AiMissionPlanner* __thiscall FUN_00480730(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x52
AiMissionPlanner* __thiscall FUN_0047bba0(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x53 subtype 1
AiMissionPlanner* __thiscall FUN_0047c170(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x53 subtype 2
AiMissionPlanner* __thiscall FUN_0047c750(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x53 subtype 3
AiMissionPlanner* __thiscall FUN_0047f550(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x54 subtype 5
AiMissionPlanner* __thiscall FUN_0047f190(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x54
AiMissionPlanner* __thiscall FUN_0047d9a0(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x55
AiMissionPlanner* __thiscall FUN_0047fe50(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x56
AiMissionPlanner* __thiscall FUN_0047cda0(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x57
AiMissionPlanner* __thiscall FUN_0047e130(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x61
AiMissionPlanner* __thiscall FUN_00483140(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x62
AiMissionPlanner* __thiscall FUN_00482b30(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x62 subtype 4, 0x63
AiMissionPlanner* __thiscall FUN_0047d380(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x69
AiMissionPlanner* __thiscall FUN_00482020(void* memory, int view, AiSideController* owner, AiCharacterRecordSet* characters);  // 0x6a
void __thiscall FUN_0047adf0(AiMissionPlanner* planner, const ObjectKey* missionObject);  // 0x47adf0
void __thiscall FUN_0047ae50(AiMissionPlanner* planner);                                  // 0x47ae50
