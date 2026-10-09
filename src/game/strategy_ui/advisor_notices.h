#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

class GameObj;
class SoundRequest;

// A message that the game raises for a side's UI (VT 0x65c538, 0x44 bytes; the concrete player-notice classes of the
// 0x48aaxx range derive from the two text variants below). The nodes carry their creation tick; Virtual12 tells
// whether the notice is older than 300 ticks.
class UiNotice : public IdNode {
public:
    UiNotice();                // 0x4c4ce0
    explicit UiNotice(int owner);  // 0x4c4d90
    virtual ~UiNotice();       // 0x4c4e30 (scalar deleting destructor 0x4c4d70)
    virtual void Read(istream* in);    // slot 1, 0x4c4e80
    virtual void Write(ostream* out);  // slot 2, 0x4c4f30
    virtual int GetKind() = 0;   // slot 3: the index of the notice's handler in g_uiHandlers (events/player_notices.h)
    virtual int GetClass() = 0;  // slot 4: the sort of notice, 3..5 are the ones with a picture/sound page
    // Slots 5-9: what a notification payload tells the notice (NotifPayload::DispatchToSink calls the one that
    // fits its shape with the payload's kind and data); the concrete notices override the ones they handle.
    virtual void Virtual05(int kind, intptr_t payload) {}                     // 0x403ef0 (shared no-op)
    virtual void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) = 0;
    // Message 0x302 passes `a` on to the owning side's UI.
    virtual void OnMessage(int message, intptr_t a, intptr_t b);  // slot 7, 0x4c5020
    virtual void Virtual08(int kind, intptr_t a, intptr_t b) = 0;
    virtual void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) {}             // 0x48acb0 (shared no-op)
    virtual UiNotice* Virtual10() = 0;                // slot 10: a new empty notice of the same class (UiHandler::CreateRecord)
    virtual int Virtual11() { return 0; }             // 0x6158b0 (shared)
    virtual int IsStale();                            // slot 12, 0x4c4fe0: created more than 300 ticks ago
    virtual void Virtual13() {}                       // 0x619730 (shared no-op)

    int m_unk1c;             // +0x1c
    int m_owner;             // +0x20 the side the notice is for
    PersistentInt m_flags;   // +0x24 bit 0x10 is set by the constructor
    int m_unk28;             // +0x28
    uint16_t m_picture[2];   // +0x2c the two picture ids of the message index's page
    uint16_t m_sound[2];     // +0x30 the two sound ids
    uint32_t m_categories;   // +0x34 the message index tabs the notice appears in (bit mask)
    uint32_t m_created;      // +0x38 GameHookTickCount() at construction
    uint16_t m_unk3c;        // +0x3c
    int m_unk40;             // +0x40
};

// Tells `sink` about every payload of the list (slot 7 of each payload); 0 for a null list, else 1. 0x4974f0 is its
// body in the UiTextNotice / UiKeyNotice slots.
class LinkNode;
int ForwardPayloads(UiNotice* sink, const LinkNode* payloads);

// VT 0x65c4f0, 0x64 bytes: a notice with two texts and two object references; slots 13-16 are new.
class UiTextNotice : public UiNotice {
public:
    UiTextNotice();                 // 0x4c4ab0
    explicit UiTextNotice(int owner);  // 0x4c4b50
    virtual ~UiTextNotice();        // 0x4c4bd0 (scalar deleting destructor 0x4c4b30)
    virtual void Read(istream* in) override;    // 0x4c4c50
    virtual void Write(ostream* out) override;  // 0x4c4c90
    void Virtual13() override {}  // 0x619730 (shared no-op)
    virtual void Virtual14() {}   // 0x619730
    virtual void Virtual15();     // 0x4c4cd0: Virtual13 through the vtable
    virtual int Virtual16(const LinkNode* payloads);  // 0x4974f0: hands every payload of the list to Virtual0x via DispatchToSink

    String m_text0;       // +0x44
    String m_text1;       // +0x50
    ObjectKey m_key;      // +0x5c
    ObjectKey m_target;   // +0x60 a key that is 0 when empty
};

// VT 0x65c570, 0x64 bytes.
class UiKeyNotice : public UiNotice {
public:
    UiKeyNotice();                 // 0x4c5040
    explicit UiKeyNotice(int owner);  // 0x4c50d0
    virtual ~UiKeyNotice();        // 0x4c5140 (scalar deleting destructor 0x4c50b0)
    virtual void Read(istream* in) override;    // 0x4c51b0
    virtual void Write(ostream* out) override;  // 0x4c51f0
    void Virtual13() override {}  // 0x619730 (shared no-op)
    virtual int Virtual14(const LinkNode* payloads);  // 0x4974f0 (shared)

    String m_text0;       // +0x44
    String m_text1;       // +0x50
    int m_value;          // +0x5c
    ObjectKey m_key;      // +0x60
};

struct DroidScriptEntry {
    uint16_t m_id;       // +0x00 resource id of the droid script
    uint16_t m_pad;      // +0x02
    uint32_t m_flags;    // +0x04 bit 0: only while the advisor chats (UI flag 0x1000, not 0x4000); bit 1: second set
    uint32_t m_x;        // +0x08
    uint32_t m_y;        // +0x0c
};

// A cue's script: the droid animations to play (VT 0x65c5b0, 0x2c bytes). The tree of cue scripts lives in the
// advisor at +0x138.
class DroidScript : public IdNode {
public:
    DroidScript();                      // 0x4c5230
    virtual ~DroidScript();             // 0x4c5270 (scalar deleting destructor 0x4c5250)
    virtual void Read(istream* in);     // slot 1, 0x4c52d0
    virtual void Write(ostream* out);   // slot 2, 0x4c5380
    // The next entry, or null (and the cursor is reset) at the end.
    DroidScriptEntry* NextEntry();      // 0x4c5420

    int m_unk1c;                  // +0x1c
    uint32_t m_entryCount;        // +0x20
    uint32_t m_cursor;            // +0x24
    DroidScriptEntry* m_entries;  // +0x28
};

// The id of the advice line the advisor says for `adviceId`: one random pick of the RCDATA table (resource module 9).
uint16_t AdvisorPickAdviceLine(uint32_t adviceId);  // 0x4c4990
// Plays `adviceId` as a sound effect of the advisor module and returns a copy of the request in `result`.
SoundRequest* REB_CDECL AdvisorPlayAdvice(SoundRequest* result, uint32_t adviceId);  // 0x4c4a20
uint16_t REB_STDCALL AdvisorRecordTypeResource(GameObj* object);  // 0x4c5000

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(UiNotice, 0x44);
REB_ASSERT_OFFSET(UiNotice, m_flags, 0x24);
REB_ASSERT_OFFSET(UiNotice, m_created, 0x38);
REB_ASSERT_SIZE(UiTextNotice, 0x64);
REB_ASSERT_OFFSET(UiTextNotice, m_text1, 0x50);
REB_ASSERT_SIZE(UiKeyNotice, 0x64);
REB_ASSERT_OFFSET(UiKeyNotice, m_key, 0x60);
REB_ASSERT_SIZE(DroidScript, 0x2c);
#pragma clang diagnostic pop
