#include "character/character38.h"

#include "character/major_character.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "data/gnprtb.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
void DestroyCharacter38EventKinds();

extern int g_character38EventKindsReady;      // 0x6bb9e0
extern EventKindSet g_character38EventKinds;  // 0x6bb9d8

namespace {

constexpr uint32_t kFamilyCharacter38 = 0x38;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;

}  // namespace

Character38::Character38() {}

Character38::~Character38() { REB_KEEP_VPTR_STORE(); }

uint32_t Character38::GetFamily() { return kFamilyCharacter38; }

int Character38::Read(istream* in) { return MajorLeafRead(this, in); }

int Character38::Write(ostream* out) { return MajorLeafWrite(this, out); }

int Character38::Checksum(Crc32* crc) { return MajorLeafChecksum(this, crc); }

int Character38::Validate() { return Character::Validate(); }

int Character38::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return CharacterLeafApplyAll(this, ctx);
    return isSideView;
}

int Character38::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_character38EventKinds;
    return result;
}

int Character38::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_character38EventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int Character38::CreateViews(int controlKind, NotifyContext* ctx) { return CharacterLeafCreateViews(this, controlKind, ctx); }

GameObj* Character38::CreateViewObject() { return NewOrNull<Character38>(); }

GameObjNotifier* Character38::Create() { return NewOrNull<Character38>(); }

int Character38::Virtual184() {
    int value = 0;
    RollChanceA11(&value);
    return value;
}

int Character38::OnInjuryChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return Character::OnInjuryChanged(oldValue, newValue, ctx);
}

int Character38::BuildEventKinds() {
    int result = 1;
    if (!g_character38EventKindsReady) {
        g_character38EventKindsReady = 1;
        result = Character::InitEventKinds(0, &g_character38EventKinds);
    }
    return result;
}

void ConstructCharacter38EventKinds() { new (&g_character38EventKinds) EventKindSet(); }

void RegisterCharacter38EventKindsAtExit() { atexit(DestroyCharacter38EventKinds); }

void Character38::InitStatics() {
    ConstructCharacter38EventKinds();
    RegisterCharacter38EventKindsAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_character38EventKindsDestroyedGuard;  // 0x6bb9e4

void DestroyCharacter38EventKinds() {
    if (!(g_character38EventKindsDestroyedGuard & 0x1)) {
        g_character38EventKindsDestroyedGuard |= 0x1;
        g_character38EventKinds.~EventKindSet();
    }
}
