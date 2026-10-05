#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// Declaration of the AI's posture base class (0x4ad8c0-0x4adb70, VT 0x65c0b0, 0x5c bytes), needed by AiSystemRecord. A
// posture is the stance an AiSystemRecord takes towards its system (the six subclasses are created by
// AiSystemRecord::CreatePosture, kinds 1-6); the class belongs to part 2 of the AI module (postures, 0x4ad770-0x4bb9f0). This is
// a stand-in meant to be replaced by the full header: slots keep their order, unknown ones are VirtualNN.

class AiSideController;
class AiSystemRecord;

class AiPosture {
public:
    AiPosture(int view, AiSideController* owner, AiSystemRecord* record);  // 0x4ad8e0
    virtual ~AiPosture();                      // 0: 0x4ad8c0 (deleting), 0x4ad980
    virtual void Read(istream* in);            // 1: 0x4ada10
    virtual void Write(ostream* out);          // 2: 0x4ada80
    virtual int Virtual03();                   // 3: 0x6158b0 returns 0
    virtual void Virtual04(const ObjectKey* key);  // 4: 0x4adde0
    virtual int Virtual05() = 0;               // 5: the posture's kind id
    virtual int Virtual06(int* a, int* b, ObjectKey* c, ObjectKey* d) = 0;  // 6
    virtual int Virtual07(int* out) = 0;       // 7
    virtual int Virtual08(void* a, void* b) = 0;  // 8
    virtual int Virtual09(ObjectKey* a, ObjectKey* b) = 0;  // 9
    virtual int Virtual10(ObjectKey* a, ObjectKey* b) = 0;  // 10
    virtual int Virtual11(ObjectKey* a, ObjectKey* b) = 0;  // 11
    virtual int Virtual12(ObjectKey* a, ObjectKey* b) = 0;  // 12
    virtual int Virtual13();                   // 13
    virtual int Virtual14();                   // 14
    virtual int Virtual15();                   // 15
    virtual int Virtual16();                   // 16
    virtual int Virtual17();                   // 17
    virtual int Virtual18();                   // 18
    virtual int Virtual19();                   // 19
    virtual int Virtual20() = 0;               // 20
    virtual int Virtual21() = 0;               // 21

    int m_view;                    // +0x04
    int m_x08;                     // +0x08
    AiSideController* m_owner;     // +0x0c
    AiSystemRecord* m_record;      // +0x10
    PersistentInt m_flags;         // +0x14
    AiRankedSelector m_selector;   // +0x18
    ObjectKey m_x28;               // +0x28
    ObjectKey m_x2c;               // +0x2c
    ObjectKey m_x30;               // +0x30
    int m_budget[10];              // +0x34
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiPosture, 0x5c);
#pragma clang diagnostic pop
