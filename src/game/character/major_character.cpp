#include "character/major_character.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_00594780();
extern "C" void __cdecl LAB_0056f000();
extern "C" void __cdecl LAB_00573c70();
extern "C" void __cdecl LAB_00575690();

// The story characters' shared CreateViews stub (0x56f950, in the Luke code).
int __thiscall FUN_0056f950(Character* self, int controlKind, NotifyContext* ctx);

extern int g_majorCharacterEventKindsReady;      // 0x6bc070
extern EventKindSet g_majorCharacterEventKinds;  // 0x6bc068
extern int g_majorChar30EventKindsReady;         // 0x6bb9d0
extern EventKindSet g_majorChar30EventKinds;     // 0x6bb9c8
extern int g_majorChar34EventKindsReady;         // 0x6bba78
extern EventKindSet g_majorChar34EventKinds;     // 0x6bba70
extern int g_majorChar35EventKindsReady;         // 0x6bbacc
extern EventKindSet g_majorChar35EventKinds;     // 0x6bbac0

namespace {

constexpr uint32_t kFamilyMajorChar30 = 0x30;
constexpr uint32_t kFamilyMajorChar34 = 0x34;
constexpr uint32_t kFamilyMajorChar35 = 0x35;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusBusyCategory = 0x30;

template <typename Init>
int buildKindsOnce(int* ready, Init init) {
    int result = 1;
    if (!*ready) {
        *ready = 1;
        result = init();
    }
    return result;
}

int copyKinds(int ready, EventKindSet& kinds, EventKindSet* out) {
    if (ready) *out = kinds;
    return ready;
}

int checkOrder(int ready, EventKindSet& kinds, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    if (ready && kinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return ready;
}

}  // namespace

// ---- MajorCharacter ------------------------------------------------------------------------------------------

MajorCharacter::MajorCharacter() {}

MajorCharacter::~MajorCharacter() { REB_KEEP_VPTR_STORE(); }

int MajorCharacter::GetEventKinds(int unused, EventKindSet* out) {
    return copyKinds(BuildEventKinds(), g_majorCharacterEventKinds, out);
}

int MajorCharacter::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_majorCharacterEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int MajorCharacter::CheckActivity(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusBusyCategory, 1);
    return 1;
}

int MajorCharacter::Virtual130(NotifyContext* ctx) { return Character::Virtual130(ctx); }

int MajorCharacter::ValidateBody() {
    int result = Character::Validate();
    if (result) result = (m_stateFlags & kDestroyedFlag) ? 0 : 1;
    return result;
}

int MajorCharacter::BuildEventKinds() {
    return buildKindsOnce(&g_majorCharacterEventKindsReady,
                          [] { return Character::InitEventKinds(0, &g_majorCharacterEventKinds); });
}

int MajorCharacter::InitEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    *out = g_majorCharacterEventKinds;
    return result;
}

void ConstructMajorCharacterEventKinds() { new (&g_majorCharacterEventKinds) EventKindSet(); }

void RegisterMajorCharacterEventKindsAtExit() { atexit(LAB_00594780); }

void MajorCharacter::InitStatics() {
    ConstructMajorCharacterEventKinds();
    RegisterMajorCharacterEventKindsAtExit();
}

// ---- forwarders -----------------------------------------------------------------------------------------------

int __thiscall CharacterLeafRead(Character* self, istream* in) { return self->Character::Read(in); }

int __thiscall CharacterLeafWrite(Character* self, ostream* out) { return self->Character::Write(out); }

int __thiscall CharacterLeafApplyAll(Character* self, NotifyContext* ctx) { return self->Character::ApplyAll(ctx); }

int __thiscall CharacterLeafChecksum(Character* self, Crc32* crc) { return self->Character::Checksum(crc); }

int __thiscall CharacterLeafCreateViews(Character* self, int controlKind, NotifyContext* ctx) {
    return self->CreateViewsImpl(controlKind, ctx);
}

int __thiscall MajorLeafRead(Character* self, istream* in) { return CharacterLeafRead(self, in); }

int __thiscall MajorLeafWrite(Character* self, ostream* out) { return CharacterLeafWrite(self, out); }

int __thiscall MajorLeafApplyAll(Character* self, NotifyContext* ctx) { return CharacterLeafApplyAll(self, ctx); }

int __thiscall MajorLeafChecksum(Character* self, Crc32* crc) { return CharacterLeafChecksum(self, crc); }

// ---- MajorChar30 ---------------------------------------------------------------------------------------------

MajorChar30::MajorChar30() {}

MajorChar30::~MajorChar30() { REB_KEEP_VPTR_STORE(); }

uint32_t MajorChar30::GetFamily() { return kFamilyMajorChar30; }

int MajorChar30::Read(istream* in) { return MajorLeafRead(this, in); }

int MajorChar30::Write(ostream* out) { return MajorLeafWrite(this, out); }

int MajorChar30::Checksum(Crc32* crc) { return MajorLeafChecksum(this, crc); }

int MajorChar30::Validate() { return ValidateBody(); }

int MajorChar30::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return MajorLeafApplyAll(this, ctx);
    return isSideView;
}

int MajorChar30::GetEventKinds(int unused, EventKindSet* out) {
    return copyKinds(BuildEventKinds(), g_majorChar30EventKinds, out);
}

int MajorChar30::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrder(BuildEventKinds(), g_majorChar30EventKinds, orderKind, out);
}

int MajorChar30::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_0056f950(this, controlKind, ctx); }

GameObj* MajorChar30::CreateViewObject() { return NewOrNull<MajorChar30>(); }

GameObjNotifier* MajorChar30::Create() { return NewOrNull<MajorChar30>(); }

int MajorChar30::BuildEventKinds() {
    return buildKindsOnce(&g_majorChar30EventKindsReady,
                          [] { return MajorCharacter::InitEventKinds(0, &g_majorChar30EventKinds); });
}

void ConstructMajorChar30EventKinds() { new (&g_majorChar30EventKinds) EventKindSet(); }

void RegisterMajorChar30EventKindsAtExit() { atexit(LAB_0056f000); }

void MajorChar30::InitStatics() {
    ConstructMajorChar30EventKinds();
    RegisterMajorChar30EventKindsAtExit();
}

// ---- MajorChar34 ---------------------------------------------------------------------------------------------

MajorChar34::MajorChar34() {}

MajorChar34::~MajorChar34() { REB_KEEP_VPTR_STORE(); }

uint32_t MajorChar34::GetFamily() { return kFamilyMajorChar34; }

int MajorChar34::Read(istream* in) { return MajorLeafRead(this, in); }

int MajorChar34::Write(ostream* out) { return MajorLeafWrite(this, out); }

int MajorChar34::Checksum(Crc32* crc) { return MajorLeafChecksum(this, crc); }

int MajorChar34::Validate() {
    const int valid = ValidateBody();
    return valid && m_force != 0;
}

int MajorChar34::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return MajorLeafApplyAll(this, ctx);
    return isSideView;
}

int MajorChar34::GetEventKinds(int unused, EventKindSet* out) {
    return copyKinds(BuildEventKinds(), g_majorChar34EventKinds, out);
}

int MajorChar34::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrder(BuildEventKinds(), g_majorChar34EventKinds, orderKind, out);
}

int MajorChar34::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_0056f950(this, controlKind, ctx); }

GameObj* MajorChar34::CreateViewObject() { return NewOrNull<MajorChar34>(); }

GameObjNotifier* MajorChar34::Create() { return NewOrNull<MajorChar34>(); }

int MajorChar34::BuildEventKinds() {
    return buildKindsOnce(&g_majorChar34EventKindsReady,
                          [] { return MajorCharacter::InitEventKinds(0, &g_majorChar34EventKinds); });
}

void ConstructMajorChar34EventKinds() { new (&g_majorChar34EventKinds) EventKindSet(); }

void RegisterMajorChar34EventKindsAtExit() { atexit(LAB_00573c70); }

void MajorChar34::InitStatics() {
    ConstructMajorChar34EventKinds();
    RegisterMajorChar34EventKindsAtExit();
}

// ---- MajorChar35 ---------------------------------------------------------------------------------------------

MajorChar35::MajorChar35() {}

MajorChar35::~MajorChar35() { REB_KEEP_VPTR_STORE(); }

uint32_t MajorChar35::GetFamily() { return kFamilyMajorChar35; }

int MajorChar35::Read(istream* in) { return MajorLeafRead(this, in); }

int MajorChar35::Write(ostream* out) { return MajorLeafWrite(this, out); }

int MajorChar35::Checksum(Crc32* crc) { return MajorLeafChecksum(this, crc); }

int MajorChar35::Validate() {
    const int valid = ValidateBody();
    return valid && m_force != 0;
}

int MajorChar35::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return MajorLeafApplyAll(this, ctx);
    return isSideView;
}

int MajorChar35::GetEventKinds(int unused, EventKindSet* out) {
    return copyKinds(BuildEventKinds(), g_majorChar35EventKinds, out);
}

int MajorChar35::CheckOrder(int a, int orderKind, StatusCode* out) {
    return checkOrder(BuildEventKinds(), g_majorChar35EventKinds, orderKind, out);
}

int MajorChar35::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_0056f950(this, controlKind, ctx); }

GameObj* MajorChar35::CreateViewObject() { return NewOrNull<MajorChar35>(); }

GameObjNotifier* MajorChar35::Create() { return NewOrNull<MajorChar35>(); }

int MajorChar35::BuildEventKinds() {
    return buildKindsOnce(&g_majorChar35EventKindsReady,
                          [] { return MajorCharacter::InitEventKinds(0, &g_majorChar35EventKinds); });
}

void ConstructMajorChar35EventKinds() { new (&g_majorChar35EventKinds) EventKindSet(); }

void RegisterMajorChar35EventKindsAtExit() { atexit(LAB_00575690); }

void MajorChar35::InitStatics() {
    ConstructMajorChar35EventKinds();
    RegisterMajorChar35EventKindsAtExit();
}
