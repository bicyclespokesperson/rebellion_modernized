#include "galaxy/system_type92.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"

// GNPRTB rules that roll the starting values of a system of this family.
int FUN_00559970();                       // starting energy
int FUN_005599c0(int energy);             // starting raw material
int FUN_00559a10();                       // is the system populated
int FUN_00559a20(int side, int dataFlag);  // starting loyalty
int FUN_00559a60(int rawMaterial, int a, int* created, ObjectKey* key);

namespace {

constexpr uint32_t kFamilySystemType92 = 0x92;
constexpr int kSpecialSystemId = 0x121;
constexpr int kAllianceSide = 1;
constexpr int kEmpireSide = 2;
constexpr int kStartingLoyalty = 100;

}  // namespace

SystemType92::SystemType92() {}

SystemType92::~SystemType92() { REB_KEEP_VPTR_STORE(); }

uint32_t SystemType92::GetFamily() { return kFamilySystemType92; }

int SystemType92::Read(istream* in) { return FUN_0050e410(this, in); }

int SystemType92::Write(ostream* out) { return FUN_0050e4e0(this, out); }

int SystemType92::Checksum(Crc32* crc) { return FUN_0050e730(this, crc); }

int SystemType92::Validate() { return FUN_0050e820(this); }

int SystemType92::ApplyAll(NotifyContext* ctx) {
    if (IsSideView()) return ApplyAllImpl(ctx);
    return 0;
}

int SystemType92::ApplyAllImpl(NotifyContext* ctx) { return FUN_0050e5b0(this, ctx); }

int SystemType92::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int SystemType92::CreateViewsImpl(int controlKind, NotifyContext* ctx) { return FUN_0050e200(this, controlKind, ctx); }

GameObj* SystemType92::CreateViewObject() { return NewOrNull<SystemType92>(); }

GameObjNotifier* SystemType92::Create() { return NewOrNull<SystemType92>(); }

int SystemType92::Virtual47(int a) {
    NotifyContext* ctx = reinterpret_cast<NotifyContext*>(a);
    const int result = System::Virtual47(a);
    if (!result) return 0;

    if (m_id == kSpecialSystemId) {
        SetPopulated(1, ctx);
        SetLoyalty(kStartingLoyalty, ctx);
        NotifyContext empireOnly(*ctx);
        empireOnly.m_flag04 = 1;
        empireOnly.m_flag08 = kEmpireSide;
        SystemView(this, 2)->ApplyControlKind(&empireOnly);
        SystemView(this, 2)->ApplyLoyalty(&empireOnly);
        SystemView(this, 2)->ApplyPopulated(&empireOnly);
        SystemView(this, 2)->ApplyExplored(&empireOnly);
        SetEnergy(FUN_00559970(), ctx);
    } else if (ControlSide() == kAllianceSide) {
        SetPopulated(1, ctx);
        SetLoyalty(kStartingLoyalty, ctx);
        SetEnergy(FUN_00559970(), ctx);
    } else {
        SetPopulated(FUN_00559a10(), ctx);
        if (m_flags & system_flag::kPopulated) {
            const uint32_t side = ControlSide();
            SetLoyalty(FUN_00559a20(side, FUN_005092f0(this)), ctx);
        }
        SetEnergy(FUN_00559970(), ctx);
    }
    SetRawMaterial(FUN_005599c0(m_energy), ctx);
    return result;
}

int SystemType92::Virtual115(NotifyContext* ctx) {
    int result = 1;
    if (m_flags & system_flag::kPopulated) {
        const int count = m_energy;
        ObjectKey key;
        const int rawMaterial = m_rawMaterial;
        for (int i = 0; i < count; ++i) {
            int created;
            result = FUN_00559a60(rawMaterial, FUN_0052d370(this, ControlSide(), 1), &created, &key);
            if (result && created) result = CreateChild(&key, ControlSide(), ctx, 1, 1) != 0;
            if (!result) break;
        }
    }
    return result;
}
