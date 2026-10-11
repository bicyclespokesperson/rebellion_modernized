#pragma once
#include "character/character.h"

// MajorCharacter (0x594520, VT 0x66b608, secondary 0x66b600, 0xb0 bytes) is the base of the story's major characters:
// the leaf classes MajorChar30 (type 0x30, probably Mon Mothma), MajorChar34 (0x34, Vader?) and MajorChar35 (0x35, the
// Emperor?). It adds no fields. Over Character it makes the character never available for orders (CheckActivity
// reports status {0x30, 1}), and keeps its own copy of the event-kind set.
//
// The leaf classes have the same 251-slot vtable; each overrides the destructor, GetFamily, the persistence slots 5-9,
// GetEventKinds, CheckOrder, CreateViews (37, a shared stub that calls the Luke code at 0x56f950) and CreateViewObject.
// The bodies of slots 5-7 and 9 are shared between the three leaves (and Character38), so only one of each has an
// address: MajorChar30 owns Read/Write/Checksum (0x56ee90/0x56eea0/0x56eeb0), MajorChar35 owns ApplyAll (0x575590) and
// MajorChar34 owns CreateViews (0x573b70); the other leaves' copies are never hooked.
class MajorCharacter : public Character {
public:
    MajorCharacter();           // 0x594520
    ~MajorCharacter() override;  // 0x594540 (deleting), 0x594560

    int GetEventKinds(int unused, EventKindSet* out) override;                      // 15, 0x594580
    int CheckOrder(int a, int orderKind, StatusCode* out) override;                 // 22, 0x5945a0
    int CheckActivity(const ActivityRequest* request, StatusCode* out) override;    // 112, 0x594660: {0x30, 1}
    int Virtual116() override { return 0; }                                         // 116
    int Virtual127() override { return 3; }                                         // 127, 0x526f00
    int Virtual130(NotifyContext* ctx) override;                                    // 130, 0x5946d0

    // Validate body of the leaf classes (their slot 8 jumps here): Character::Validate and not destroyed.
    int ValidateBody();  // 0x5946e0

    static int BuildEventKinds();                              // 0x594700, g_majorCharacterEventKinds at 0x6bc068
    static int InitEventKinds(int unused, EventKindSet* out);  // 0x594730
};

class MajorChar30 : public MajorCharacter {
public:
    MajorChar30();           // 0x56ece0
    ~MajorChar30() override;  // 0x56ed00 (deleting), 0x56ed20

    uint32_t GetFamily() override;                                      // 1, 0x56eed0: 0x30
    int Read(istream* in) override;                                     // 5, 0x56ee90
    int Write(ostream* out) override;                                   // 6, 0x56eea0
    int Checksum(Crc32* crc) override;                                  // 7, 0x56eeb0
    int Validate() override;                                            // 8, 0x56eec0 (jump thunk)
    int ApplyAll(NotifyContext* ctx) override;                          // 9, shared 0x575590
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56ed40
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56ed60
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x56ee20

    static int BuildEventKinds();   // 0x56efa0, g_majorChar30EventKinds at 0x6bb9c8
    static GameObjNotifier* Create();  // 0x56eee0, the object factory of family 0x30
};

class MajorChar34 : public MajorCharacter {
public:
    MajorChar34();           // 0x5739c0
    ~MajorChar34() override;  // 0x5739e0 (deleting), 0x573a00

    uint32_t GetFamily() override;                                      // 1, 0x573b90: 0x34
    int Read(istream* in) override;                                     // 5, shared 0x56ee90
    int Write(ostream* out) override;                                   // 6, shared 0x56eea0
    int Checksum(Crc32* crc) override;                                  // 7, shared 0x56eeb0
    int Validate() override;                                            // 8, 0x56f890 (jump thunk to the story-character check at 0x56f9b0)
    int ApplyAll(NotifyContext* ctx) override;                          // 9, shared 0x575590
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x573a20
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x573a40
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x573b00

    static int BuildEventKinds();   // 0x573c10, g_majorChar34EventKinds at 0x6bba70
    static GameObjNotifier* Create();  // 0x573ba0, the object factory of family 0x34
};

class MajorChar35 : public MajorCharacter {
public:
    MajorChar35();           // 0x5753e0
    ~MajorChar35() override;  // 0x575400 (deleting), 0x575420

    uint32_t GetFamily() override;                                      // 1, 0x5755b0: 0x35
    int Read(istream* in) override;                                     // 5, shared 0x56ee90
    int Write(ostream* out) override;                                   // 6, shared 0x56eea0
    int Checksum(Crc32* crc) override;                                  // 7, shared 0x56eeb0
    int Validate() override;                                            // 8, shared 0x56f890
    int ApplyAll(NotifyContext* ctx) override;                          // 9, 0x575590
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x575440
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x575460
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x575520

    static int BuildEventKinds();   // 0x575630, g_majorChar35EventKinds at 0x6bbac0
    static GameObjNotifier* Create();  // 0x5755c0, the object factory of family 0x35
};

// First-level forwarders shared by Character38 and the major characters: they call the Character bodies of the
// persistence slots and of CreateViews (all thiscall).
int CharacterLeafRead(Character* self, istream* in);            // 0x56f2c0
int CharacterLeafWrite(Character* self, ostream* out);          // 0x56f2d0
int CharacterLeafApplyAll(Character* self, NotifyContext* ctx);  // 0x56f2e0
int CharacterLeafChecksum(Character* self, Crc32* crc);         // 0x56f2f0
int CharacterLeafCreateViews(Character* self, int controlKind, NotifyContext* ctx);  // 0x56f2a0
// Second level, owned by MajorChar30's range and used by Character38's slots as well: forward to the above.
int MajorLeafRead(Character* self, istream* in);            // 0x56ef50
int MajorLeafWrite(Character* self, ostream* out);          // 0x56ef60
int MajorLeafApplyAll(Character* self, NotifyContext* ctx);  // 0x56ef70
int MajorLeafChecksum(Character* self, Crc32* crc);         // 0x56ef80
