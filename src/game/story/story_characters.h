#pragma once
#include "character/major_character.h"
#include "gameobj/child_iterator.h"

// The named story characters Leia (family 0x31, ctor 0x56fd40, VT 0x6663e0), Luke (0x32, ctor 0x56f4e0, VT 0x665fd8) and
// Han Solo (0x33, ctor 0x572530, VT 0x666f90). All three are MajorCharacters with one more field, a word of story flags
// at +0xb0 (0xb4 bytes in all) that is read, written and checksummed as one 32-bit value. Each flag is a property of the
// object with the pattern of GameObj (see game_obj.h): SetX on the master, ApplyX on a side view, a virtual OnXChanged
// on the master and a virtual OnXNotified on a side view. The new virtual slots start at 251: first the OnXChanged events
// of the flags in the order of the bits, then the OnXNotified events.
//
//   Leia  flag 0x1 KnowsHeritage                        slots 251 changed (shared stub), 252 notified
//   Luke  flag 0x1 KnowsHeritage, 0x2 FinalBattleReady, 0x8 DagobahCompleted       251-253 changed, 254-256 notified
//   Han   flag 0x1 BountyAttack, 0x2 CapturedByBountyHunters                        251-252 changed, 253-254 notified
//
// StoryCharacter exists only in this reconstruction: it groups the field and the persistence bodies the three classes
// share (the original code reached them through three identical first-level forwarders).
class StoryCharacter : public MajorCharacter {
public:
    StoryCharacter();
    ~StoryCharacter() override;

    int ReadBody(istream* in);       // 0x572880
    int WriteBody(ostream* out);     // 0x5728b0
    int ChecksumBody(Crc32* crc);    // 0x572920

    uint32_t m_storyFlags;  // +0xb0
};

class Luke : public StoryCharacter {
public:
    static constexpr uint32_t kKnowsHeritage = 0x1;
    static constexpr uint32_t kFinalBattleReady = 0x2;
    static constexpr uint32_t kDagobahCompleted = 0x8;

    Luke();           // 0x56f4e0
    ~Luke() override;  // 0x56f510 (deleting), 0x56f530

    uint32_t GetFamily() override;                                      // 1, 0x56f8a0: 0x32
    int Read(istream* in) override;                                     // 5, 0x56f840
    int Write(ostream* out) override;                                   // 6, 0x56f850
    int Checksum(Crc32* crc) override;                                  // 7, 0x56f880
    int Validate() override;                                            // 8, 0x56f890 (jump thunk to 0x56f9b0)
    int ApplyAll(NotifyContext* ctx) override;                          // 9, 0x56f860
    uint32_t GetRecordTypeBits() override;                              // 12, 0x56f550
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56f570
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56f590
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x56f7d0
    int Virtual130(NotifyContext* ctx) override;                        // 130, 0x56f8b0
    int OnForceChanged(int oldValue, int newValue, NotifyContext* ctx) override;  // 199, 0x56fb80
    virtual int OnKnowsHeritageChanged(int on, NotifyContext* ctx);             // 251, 0x56fbb0
    virtual int OnFinalBattleReadyChanged(int on, NotifyContext* ctx);          // 252, 0x56fbc0
    virtual int OnDagobahCompletedChanged(int on, NotifyContext* ctx) { return 1; }  // 253, shared stub 0x524fb0
    virtual int OnKnowsHeritageNotified(int on, NotifyContext* ctx);            // 254, 0x56fbe0
    virtual int OnFinalBattleReadyNotified(int on, NotifyContext* ctx);         // 255, 0x56fc30
    virtual int OnDagobahCompletedNotified(int on, NotifyContext* ctx);         // 256, 0x56fc70

    int SetKnowsHeritage(int on, NotifyContext* ctx);      // 0x56f650
    int SetFinalBattleReady(int on, NotifyContext* ctx);   // 0x56f6c0
    int SetDagobahCompleted(int on, NotifyContext* ctx);   // 0x56f730
    int ApplyKnowsHeritage(NotifyContext* ctx);            // 0x56f9d0
    int ApplyFinalBattleReady(NotifyContext* ctx);         // 0x56fa60
    int ApplyDagobahCompleted(NotifyContext* ctx);         // 0x56faf0
    int ApplyAllBody(NotifyContext* ctx);                  // 0x56f970: slot 9 behind its side-view guard
    // FinalBattleReady follows KnowsHeritage and the Force reaching the threshold.
    int UpdateFinalBattleReady(NotifyContext* ctx);        // 0x56f7a0
    // The story-character validity check: MajorCharacter's and a Force value.
    int ValidateWithForce();                              // 0x56f9b0

    static int BuildEventKinds();      // 0x56fcc0, g_lukeEventKinds at 0x6bb9f0
    static GameObjNotifier* Create();  // 0x56f8e0, the object factory of family 0x32
};

class Leia : public StoryCharacter {
public:
    static constexpr uint32_t kKnowsHeritage = 0x1;

    Leia();           // 0x56fd40
    ~Leia() override;  // 0x56fd70 (deleting), 0x56fd90

    uint32_t GetFamily() override;                                      // 1, 0x56ff90: 0x31
    int Read(istream* in) override;                                     // 5, shared 0x56f840
    int Write(ostream* out) override;                                   // 6, shared 0x56f850
    int Checksum(Crc32* crc) override;                                  // 7, shared 0x56f880
    int Validate() override;                                            // 8, shared 0x56eec0
    int ApplyAll(NotifyContext* ctx) override;                          // 9, 0x56ff70
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x56fdb0
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x56fdd0
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x56ff00
    int Virtual130(NotifyContext* ctx) override;                        // 130, 0x56ffa0
    virtual int OnKnowsHeritageChanged(int on, NotifyContext* ctx) { return 1; }  // 251, shared stub 0x524fb0
    virtual int OnKnowsHeritageNotified(int on, NotifyContext* ctx);            // 252, 0x570110

    int SetKnowsHeritage(int on, NotifyContext* ctx);   // 0x56fe90
    int ApplyKnowsHeritage(NotifyContext* ctx);         // 0x570080
    int ApplyAllBody(NotifyContext* ctx);               // 0x570050

    static int BuildEventKinds();      // 0x570150, g_leiaEventKinds at 0x6bba00
    static GameObjNotifier* Create();  // 0x56ffe0, the object factory of family 0x31
};

class Han : public StoryCharacter {
public:
    static constexpr uint32_t kBountyAttack = 0x1;
    static constexpr uint32_t kCapturedByBountyHunters = 0x2;

    Han();           // 0x572530
    ~Han() override;  // 0x572560 (deleting), 0x572580

    uint32_t GetFamily() override;                                      // 1, 0x572800: 0x33
    int Read(istream* in) override;                                     // 5, shared 0x56f840
    int Write(ostream* out) override;                                   // 6, shared 0x56f850
    int Checksum(Crc32* crc) override;                                  // 7, shared 0x56f880
    int Validate() override;                                            // 8, shared 0x56eec0
    int ApplyAll(NotifyContext* ctx) override;                          // 9, 0x5727e0
    int GetEventKinds(int unused, EventKindSet* out) override;          // 15, 0x5725b0
    int CheckOrder(int a, int orderKind, StatusCode* out) override;     // 22, 0x5725d0
    int CreateViews(int controlKind, NotifyContext* ctx) override;      // 37, shared 0x573b70
    GameObj* CreateViewObject() override;                               // 57, 0x572770
    int OnCapturedChanged(int on, NotifyContext* ctx) override;         // 211, 0x572a70
    virtual int OnBountyAttackChanged(int on, NotifyContext* ctx);              // 251, 0x572aa0
    virtual int OnCapturedByBountyHuntersChanged(int on, NotifyContext* ctx) { return 1; }  // 252, shared stub 0x524fb0
    virtual int OnBountyAttackNotified(int on, NotifyContext* ctx);             // 253, 0x572b40
    virtual int OnCapturedByBountyHuntersNotified(int on, NotifyContext* ctx);  // 254, 0x572b90

    int SetBountyAttack(int on, NotifyContext* ctx);              // 0x572690
    int SetCapturedByBountyHunters(int on, NotifyContext* ctx);   // 0x572700
    int ApplyBountyAttack(NotifyContext* ctx);                    // 0x572950
    int ApplyCapturedByBountyHunters(NotifyContext* ctx);         // 0x5729e0
    int ApplyAllBody(NotifyContext* ctx);                         // 0x5728e0

    static int BuildEventKinds();      // 0x572bd0, g_hanEventKinds at 0x6bba50
    static GameObjNotifier* Create();  // 0x572810, the object factory of family 0x33
};

// Slot 37 of the major characters and of Character38's neighbours: forwards to the Character body (0x56f950).
int StoryCharacterCreateViews(Character* self, int controlKind, NotifyContext* ctx);  // 0x56f950

// Han's mission hyperdrive modifier while he is on a bounty hunter's errand (0x5725a0: a GNPRTB value).
int GetHanHyperdriveModifier();  // 0x5725a0

// Iterator over the minor characters among the children of an object (family 0x38-0x3b, VT 0x665fcc).
class MinorCharacterIterator : public ChildIterator {
public:
    MinorCharacterIterator(GameObjNotifier* parent, uint32_t stateFilter);                 // 0x56f3d0
    MinorCharacterIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side);  // 0x56f450
    ~MinorCharacterIterator() override;                                                     // 0x56f3b0 (deleting), 0x56f4d0
};
