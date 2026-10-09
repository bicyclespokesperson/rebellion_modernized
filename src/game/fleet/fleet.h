#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_family.h"

// Fleet (family 0x08, ctor 0x4fd650, VT 0x65d438, secondary ObjectBase VT 0x65d430): a group of capital ships (and,
// through them, fighter squadrons and troops) that moves, fights and blockades together. It derives straight from
// GameObj, not from UnitObjBase, and adds one word of fleet state; objects are 0x5c bytes. Vtable: 132 slots, 0-110
// are GameObj's, 111-131 are new.
//
// m_fleetFlags (+0x58) follows the same SetX -> ApplyX -> OnXChanged / OnXNotified scheme as GameObj's state flags
// (see game_obj.h) with its own seven bits:
//
//   bit   Set / Apply        Changed / Notified slot
//   0x01  Flag01   (0x4fe230 / 0x4ff180)   118 / 125 (the notified default does nothing)
//   0x02  Flag02   (0x4fe2a0 / 0x4ff210)   119 / 126 (ditto)
//   0x04  Flag04   (0x4fe310 / 0x4ff2a0)   120 / 127 (ditto)
//   0x10  Battle   (0x4fe380 / 0x4ff330)   121 / 128 "FleetBattleNotif", event 0x180
//   0x20  Blockade (0x4fe3f0 / 0x4ff3c0)   122 / 129 "FleetBlockadeNotif", event 0x181
//   0x40  Bombard  (0x4fe460 / 0x4ff450)   123 / 130 "FleetBombardNotif", event 0x182
//   0x80  Assault  (0x4fe4d0 / 0x4ff4e0)   124 / 131 "FleetAssaultNotif", event 0x183
//
// Flag01 is set once the fleet has capital ships (RefreshFromShips). Flags 02 and 04 are set by the galaxy code that
// creates fleets (0x512d00); their meaning is not known. Several of the old virtual slots take a different argument
// type than GameObj declares: Virtual48..51 receive (NotifyContext*) or (GameObj* other, int, NotifyContext*) in
// the arguments typed int there.

class FleetIterator;

class Fleet : public GameObj {
public:
    Fleet();                                                         // 0x4fd650
    virtual ~Fleet();                                                // 0x4fd670 (deleting), 0x4fd690

    virtual uint32_t GetFamily() override { return family::kFleet; }  // 1, 0x48c2a0 (shared)
    virtual int Read(istream* in) override;                          // 5, 0x4fe7b0
    virtual int Write(ostream* out) override;                        // 6, 0x4fe7c0
    virtual int Checksum(Crc32* crc) override;                       // 7, 0x4fe7f0
    virtual int Validate() override;                                 // 8, 0x4fe800 (jump to 0x4ff040)
    virtual int ApplyAll(NotifyContext* ctx) override;               // 9, 0x4fe7d0
    virtual int CollectDependencies(void* context, int value, ObjectKeyValueSet* out) override;  // 11, 0x4ffc90
    virtual int GetTransitSpeed(int arg) override;                   // 13, 0x4fd900: slowest ready ship's speed
    virtual int IsConstructed() override;                            // 14, 0x4fda10: some ship is ready
    virtual int GetEventKinds(int unused, EventKindSet* out) override;  // 15, 0x4fdad0
    virtual int Virtual18() override { return 1; }                   // 18, 0x40f340
    virtual int CanBeControlledBy(int arg) override;                 // 20, 0x4fdaf0: true until the fleet is created
    virtual int CheckOrder(int a, int orderKind, StatusCode* out) override;  // 22, 0x4fdb00
    virtual int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override;  // 23, 0x4fdbc0
    virtual int CheckRenameImpl(const ActivityRequest* request, StatusCode* out) override;   // 25, 0x4fdd20
    virtual int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) override;  // 27, 0x4fdc70
    virtual int Virtual29(int a, int b, int c, int d, StatusCode* out) override;  // 29, 0x4fe0d0
    virtual int Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) override;  // 30, 0x4fe160
    virtual int UpdateUsable(NotifyContext* ctx) override;           // 33, 0x4fe540
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x4fe790
    virtual int Reparent(ObjectKey* key, NotifyContext* ctx) override;      // 42, 0x4feca0
    virtual int Virtual44(NotifyContext* ctx) override { return 1; }        // 44, 0x51ebb0 (shared)
    virtual int Virtual45(int a, NotifyContext* ctx) override { return 1; } // 45, 0x524fb0 (shared)
    virtual int Virtual48(int ctx) override;                         // 48, 0x4fe810 (ctx is a NotifyContext*)
    virtual int Virtual49(int other, int b, int ctx) override;       // 49, 0x4fe870 (other is a GameObj*)
    virtual int Virtual50(int other, int b, int ctx) override;       // 50, 0x4fe930
    virtual int Virtual51(int other, int b, int ctx) override;       // 51, 0x4fe9f0
    virtual GameObj* CreateViewObject() override;                    // 57, 0x4fe710
    virtual int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) override;  // 61, 0x4ff570
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;       // 68, 0x4ff5e0
    virtual int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;    // 71, 0x4ff600
    virtual int OnAutoroutingChanged(int newValue, NotifyContext* ctx) override;  // 79, 0x4ff690

    // ---- slots 111-131 ----
    // Assault request check on top of CheckFleetRequest: not allowed unless the fleet blockades ({8,7}).
    virtual int CheckAssaultImpl(const ActivityRequest* request, StatusCode* out);   // 111, 0x4fddd0
    virtual int CheckAssault(const ActivityRequest* request, StatusCode* out);       // 112, 0x4fdec0: out = {1,-1}, then 111
    virtual int CheckBombardImpl(const ActivityRequest* request, StatusCode* out);   // 113, 0x4fdf70: CheckFleetRequest
    virtual int CheckBombard(const ActivityRequest* request, StatusCode* out);       // 114, 0x4fe020: out = {1,-1}, then 113
    virtual int UpdateDamaged(NotifyContext* ctx);                   // 115, 0x4feab0: Damaged follows the first ship's bit
    virtual int StartAssault(int request, NotifyContext* ctx);       // 116, 0x4feb60: CheckAssault, then SetAssault(1)
    virtual int StartBombard(int request, NotifyContext* ctx);       // 117, 0x4fec00: CheckBombard, then SetBombard(1)
    virtual int OnFlag01Changed(int value, NotifyContext* ctx) { return 1; }    // 118, 0x524fb0
    virtual int OnFlag02Changed(int value, NotifyContext* ctx) { return 1; }    // 119
    virtual int OnFlag04Changed(int value, NotifyContext* ctx) { return 1; }    // 120
    virtual int OnBattleChanged(int value, NotifyContext* ctx) { return 1; }    // 121
    virtual int OnBlockadeChanged(int value, NotifyContext* ctx) { return 1; }  // 122
    virtual int OnBombardChanged(int value, NotifyContext* ctx) { return 1; }   // 123
    virtual int OnAssaultChanged(int value, NotifyContext* ctx) { return 1; }   // 124
    virtual int OnFlag01Notified(int value, NotifyContext* ctx);     // 125, 0x4ff790: returns IsSideView
    virtual int OnFlag02Notified(int value, NotifyContext* ctx);     // 126
    virtual int OnFlag04Notified(int value, NotifyContext* ctx);     // 127
    virtual int OnBattleNotified(int value, NotifyContext* ctx);     // 128, 0x4ff7a0
    virtual int OnBlockadeNotified(int value, NotifyContext* ctx);   // 129, 0x4ff7f0
    virtual int OnBombardNotified(int value, NotifyContext* ctx);    // 130, 0x4ff840
    virtual int OnAssaultNotified(int value, NotifyContext* ctx);    // 131, 0x4ff890

    // ---- serialisation bodies behind slots 5-7 and 9 ----
    int ReadImpl(istream* in);           // 0x4fef40
    int WriteImpl(ostream* out);         // 0x4fef70
    int ChecksumImpl(Crc32* crc);        // 0x4ff010
    int ValidateImpl();                  // 0x4ff040
    int ApplyAllImpl(NotifyContext* ctx);  // 0x4fefa0
    int CreateViewsImpl(int controlKind, NotifyContext* ctx);  // 0x4fef10

    // ---- queries over the ships ----
    // Sum of the capital ships' and their squadrons' value from capital ship slot 119.
    int SumUnitStat119();                                  // 0x4fd6b0
    // Finds the ship below a ready capital ship whose short at +0x96 equals `number` (see 0x5006f0); *out is null if none.
    int FindShipByNumber(int number, GameObjNotifier** out);  // 0x4fd790
    // Adds the capital ships' counts (0x5007a0, which always reports zero) into *out.
    int SumShipCounts(int* out, int unused1, int unused2);  // 0x4fd840 (callers pass two zeros)
    // Basic request check: GameObj's, then needs a ready, non-travelling ship; fills *out with the reason.
    int CheckFleetRequest(const ActivityRequest* request, StatusCode* out);  // 0x4ffa70
    // Sets Flag01, Created and Completed if the fleet has ships, else destroys it (reason 0x14) once Flag01 was set.
    int RefreshFromShips(NotifyContext* ctx);              // 0x4fe630

    // ---- the seven fleet flags ----
    int SetFlag01(int on, NotifyContext* ctx);   // 0x4fe230
    int SetFlag02(int on, NotifyContext* ctx);   // 0x4fe2a0
    int SetFlag04(int on, NotifyContext* ctx);   // 0x4fe310
    int SetBattle(int on, NotifyContext* ctx);   // 0x4fe380
    int SetBlockade(int on, NotifyContext* ctx);  // 0x4fe3f0
    int SetBombard(int on, NotifyContext* ctx);  // 0x4fe460
    int SetAssault(int on, NotifyContext* ctx);  // 0x4fe4d0
    int ApplyFlag01(NotifyContext* ctx);   // 0x4ff180
    int ApplyFlag02(NotifyContext* ctx);   // 0x4ff210
    int ApplyFlag04(NotifyContext* ctx);   // 0x4ff2a0
    int ApplyBattle(NotifyContext* ctx);   // 0x4ff330
    int ApplyBlockade(NotifyContext* ctx);  // 0x4ff3c0
    int ApplyBombard(NotifyContext* ctx);  // 0x4ff450
    int ApplyAssault(NotifyContext* ctx);  // 0x4ff4e0

    // Fills the class's static event kind set once; false if a kind could not be added.
    static int InitEventKinds();  // 0x4ff8e0

    uint32_t m_fleetFlags;  // +0x58, bits as in the table above
};

// The factory registered for family 0x08 (0x51ebf0): a new empty fleet, or null if allocation fails (0x4fee30).
Fleet* CreateFleet();
// Number of fleets below `parent` controlled by `side` that pass the state `filter` (0x4feea0, cdecl).
int CountFleets(GameObj* parent, uint32_t side, uint32_t filter);
// Static initialisation of the event kind set (0x4ffd80, called from the CRT initialisers).
void FleetStaticInit();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(Fleet, 0x5c);
REB_ASSERT_OFFSET(Fleet, m_fleetFlags, 0x58);
#pragma clang diagnostic pop
