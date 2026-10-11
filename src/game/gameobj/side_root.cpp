#include "gameobj/side_root.h"

#include "core/checked_new.h"
#include "gameobj/object_family.h"

namespace {
constexpr uint32_t kMasterSideRootId = 3;
}

SideRoot::SideRoot() : GameObj() {
    SetId(kMasterSideRootId);
    SetFlag31(GetFlag31());
}

SideRoot::~SideRoot() {  }

uint32_t SideRoot::GetFamily() { return family::kSideRoot; }

int SideRoot::Read(istream* in) { return GameObj::Read(in); }

int SideRoot::Write(ostream* out) { return GameObj::Write(out); }

int SideRoot::Checksum(Crc32* crc) { return GameObj::Checksum(crc); }

int SideRoot::Validate() { return GameObj::Validate(); }

int SideRoot::ApplyAll(NotifyContext* ctx) {
    if (IsSideView()) return GameObj::ApplyAll(ctx);
    return 0;
}

int SideRoot::CreateViews(int controlKind, NotifyContext* ctx) { return GameObj::CreateViews(controlKind, ctx); }

GameObj* SideRoot::CreateViewObject() { return NewOrNull<SideRoot>(); }

GameObjNotifier* CreateSideRoot() { return NewOrNull<SideRoot>(); }
