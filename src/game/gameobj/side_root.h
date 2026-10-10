#pragma once
#include "gameobj/game_obj.h"

// SideRoot (family 0xf2, ctor 0x5696b0, VT 0x6639b8, secondary ObjectBase VT 0x6639b0, 0x58 bytes like GameObj): the
// per-side root of the object tree. The master one has the key 0xf2000003 (SetId(3)); the side views of every other
// object hang below the side root of their side. It adds no data to GameObj and overrides only the abstract slots:
//
//   1 GetFamily (0xf2)   5-8 Read/Write/Checksum/Validate, 9 ApplyAll (side views only), 37 CreateViews: all
//   forward to GameObj's bodies (their thunks are ICF-shared with other classes, so they have no symbol here)
//   16 Virtual16 returns 1 (GameObj's returns 0)   57 CreateViewObject: a new SideRoot.
class SideRoot : public GameObj {
public:
    SideRoot();           // 0x5696b0
    virtual ~SideRoot();  // 0x569740 (scalar deleting 0x569720)

    virtual uint32_t GetFamily() override;  // 0x569880
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;
    virtual int Validate() override;
    // Side views only (shared with the game root class, 0x513f20): the master has nothing to apply.
    virtual int ApplyAll(NotifyContext* ctx) override;
    virtual int Virtual16() override { return 1; }
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;
    virtual GameObj* CreateViewObject() override;  // 0x5697d0
};

// The factory the object family registry holds for family 0xf2 (0x569760).
GameObjNotifier* CreateSideRoot();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SideRoot, 0x58);
#pragma clang diagnostic pop
