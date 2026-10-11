#pragma once
#include <cstdint>

#include "gameobj/game_obj.h"

// RoleContainer (family 0x98, ctor 0x577050, VT 0x669260, secondary ObjectBase VT 0x669258, 0x58 bytes; the name is
// provisional, "sector_gameobj" in the code map): a GameObj without fields or new virtuals (111 slots) that holds
// characters and special forces as children. It owns its own key (GetOwnerKey), supports exactly the orders of its
// static event-kind set, and Validate requires all children to be in families 0x30-0x3f, none of them on a mission,
// all controlled by the same side as the container.
//
// Overrides: 0 destructor, 1 GetFamily, 3 GetOwnerKey, 5-7 and 9 (shared stubs forwarding to GameObj), 8 Validate
// (jump stub to ValidateImpl), 10 SetRecord (shared), 15 GetEventKinds, 16 (returns 1), 22 CheckOrder, 29, 30,
// 37 CreateViews (shared stub 0x5774f0), 57 CreateViewObject.
class RoleContainer : public GameObj {
public:
    RoleContainer();            // 0x577050
    ~RoleContainer() override;   // 0x577070 (scalar deleting), 0x577090

    uint32_t GetFamily() override;                                  // 1, 0x577520: 0x98
    int GetOwnerKey(ObjectKey* out) override;                       // 3, 0x577130: its own key
    int Read(istream* in) override;                                 // 5, shared stub 0x569840
    int Write(ostream* out) override;                               // 6, shared stub 0x569850
    int Checksum(Crc32* crc) override;                              // 7, shared stub 0x569860
    int Validate() override;                                        // 8, 0x577510 (jump stub to ValidateImpl)
    int ApplyAll(NotifyContext* ctx) override;                      // 9, shared with GameRoot (0x513f20)
    int SetRecord(DatRecord* record) override;                      // 10, 0x50e1c0 (shared with Character)
    int GetEventKinds(int unused, EventKindSet* out) override;      // 15, 0x577190
    int Virtual16() override { return 1; }                          // 16, shared 0x40f340
    int CheckOrder(int a, int orderKind, StatusCode* out) override;  // 22, 0x5771b0
    int Virtual29(int a, int b, int c, intptr_t d, StatusCode* out) override;  // 29, 0x577270
    int Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) override;  // 30, 0x5773b0
    int CreateViews(int controlKind, NotifyContext* ctx) override;  // 37, 0x5774f0
    GameObj* CreateViewObject() override;                           // 57, 0x577480

    int ValidateImpl();  // 0x5775a0
    // Walks the container's characters (usable or en route) and reports 0 of them in *out; always 1. The two
    // trailing stack arguments are unused.
    int CountCharacterChildren(int* out, int unused1, int unused2);  // 0x5770b0

    // The event-kind set of RoleContainer (g_roleContainerEventKinds, 0x6bbb40): an empty set, made on first use.
    static int BuildEventKinds();      // 0x577740
    static GameObjNotifier* Create();  // 0x577530, the object factory of family 0x98
};

// Static-initializer pieces of g_roleContainerEventKinds (0x577780 constructs it, 0x577790 registers its destructor).
