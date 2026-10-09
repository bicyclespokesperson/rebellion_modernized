#include "core/reb_abi.h"
#include "events/ui_notifs.h"
#include "core/atexit_keep.h"

#include <new>

#include "app/player_controller_decl.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "strategy_ui/game_ui_facade.h"

extern "C" int REB_CDECL atexit(void(REB_CDECL* function)(void));
extern uint8_t g_uiNotif2KeyReady;
extern ObjectKey g_uiNotif2Key;
void DestroyUiNotif2Key();

// ---- UiNotif340 (kind 0x340) ----
UiNotif340::UiNotif340() : KeyPairPayload() {}

UiNotif340::~UiNotif340() { REB_KEEP_VPTR_STORE(); }

void UiNotif340::Read(istream* in) { reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in); }

void UiNotif340::Write(ostream* out) { reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out); }

int UiNotif340::GetKind() { return 0x340; }

void UiNotif340::DispatchToSink(UiNotice* sink) {
    if (sink) sink->OnMessage(GetKind(), asArg(&m_key), asArg(&m_keyA));
}

void* CreateUiNotif340() { return NewOrNull<UiNotif340>(); }

// ---- UiNotif14f (kind 0x14f) ----
UiNotif14f::UiNotif14f() : IntPairPayload() {}

UiNotif14f::~UiNotif14f() { REB_KEEP_VPTR_STORE(); }

void UiNotif14f::Read(istream* in) { ReadPayload(in); }

void UiNotif14f::Write(ostream* out) { WritePayload(out); }

int UiNotif14f::GetKind() { return 0x14f; }

void UiNotif14f::Dispatch(PlayerController* controller) { controller->Virtual43(&m_key, m_valueB); }

void UiNotif14f::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif14f() { return NewOrNull<UiNotif14f>(); }

// ---- UiNotif150 (kind 0x150) ----
UiNotif150::UiNotif150() : IntPairPayload() {}

UiNotif150::~UiNotif150() { REB_KEEP_VPTR_STORE(); }

void UiNotif150::Dispatch(PlayerController* controller) { controller->Virtual44(&m_key, m_valueB); }

void UiNotif150::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif150() { return NewOrNull<UiNotif150>(); }

// ---- UiNotif14d (kind 0x14d) ----
UiNotif14d::UiNotif14d() : IntPairPayload() {}

UiNotif14d::~UiNotif14d() { REB_KEEP_VPTR_STORE(); }

int UiNotif14d::GetKind() { return 0x14d; }

void UiNotif14d::Dispatch(PlayerController* controller) { controller->Virtual41(&m_key, m_valueB); }

void UiNotif14d::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif14d() { return NewOrNull<UiNotif14d>(); }

// ---- UiNotif14e (kind 0x14e) ----
UiNotif14e::UiNotif14e() : IntPairPayload() {}

UiNotif14e::~UiNotif14e() { REB_KEEP_VPTR_STORE(); }

int UiNotif14e::GetKind() { return 0x14e; }

void UiNotif14e::Dispatch(PlayerController* controller) { controller->Virtual42(&m_key, m_valueB); }

void UiNotif14e::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif14e() { return NewOrNull<UiNotif14e>(); }

// ---- UiNotif142 (kind 0x142) ----
UiNotif142::UiNotif142() : IntPairPayload() {}

UiNotif142::~UiNotif142() { REB_KEEP_VPTR_STORE(); }

int UiNotif142::GetKind() { return 0x142; }

void UiNotif142::Dispatch(PlayerController* controller) { controller->Virtual32(&m_key, m_valueB); }

void* CreateUiNotif142() { return NewOrNull<UiNotif142>(); }

// ---- UiNotif141 (kind 0x141) ----
UiNotif141::UiNotif141() : IntPairPayload() {}

UiNotif141::~UiNotif141() { REB_KEEP_VPTR_STORE(); }

int UiNotif141::GetKind() { return 0x141; }

void UiNotif141::Dispatch(PlayerController* controller) { controller->Virtual31(&m_key, m_valueB); }

void UiNotif141::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif141() { return NewOrNull<UiNotif141>(); }

// ---- UiNotif14a (kind 0x14a) ----
UiNotif14a::UiNotif14a() : IntPairPayload() {}

UiNotif14a::~UiNotif14a() { REB_KEEP_VPTR_STORE(); }

int UiNotif14a::GetKind() { return 0x14a; }

void UiNotif14a::Dispatch(PlayerController* controller) { controller->Virtual38(&m_key, m_valueB); }

void* CreateUiNotif14a() { return NewOrNull<UiNotif14a>(); }

// ---- UiNotif140 (kind 0x140) ----
UiNotif140::UiNotif140() : IntPairPayload() {}

UiNotif140::~UiNotif140() { REB_KEEP_VPTR_STORE(); }

int UiNotif140::GetKind() { return 0x140; }

void UiNotif140::Dispatch(PlayerController* controller) { controller->Virtual30(&m_key, m_valueB); }

void* CreateUiNotif140() { return NewOrNull<UiNotif140>(); }

// ---- UiNotif14c (kind 0x14c) ----
UiNotif14c::UiNotif14c() : IntPairPayload() {}

UiNotif14c::~UiNotif14c() { REB_KEEP_VPTR_STORE(); }

int UiNotif14c::GetKind() { return 0x14c; }

void UiNotif14c::Dispatch(PlayerController* controller) { controller->Virtual40(&m_key, m_valueB); }

void* CreateUiNotif14c() { return NewOrNull<UiNotif14c>(); }

// ---- UiNotif149 (kind 0x149) ----
UiNotif149::UiNotif149() : IntPairPayload() {}

UiNotif149::~UiNotif149() { REB_KEEP_VPTR_STORE(); }

int UiNotif149::GetKind() { return 0x149; }

void UiNotif149::Dispatch(PlayerController* controller) { controller->Virtual37(&m_key, m_valueB); }

void* CreateUiNotif149() { return NewOrNull<UiNotif149>(); }

// ---- UiNotif146 (kind 0x146) ----
UiNotif146::UiNotif146() : IntPairPayload() {}

UiNotif146::~UiNotif146() { REB_KEEP_VPTR_STORE(); }

int UiNotif146::GetKind() { return 0x146; }

void UiNotif146::Dispatch(PlayerController* controller) { controller->Virtual36(&m_key, m_valueB); }

void* CreateUiNotif146() { return NewOrNull<UiNotif146>(); }

// ---- UiNotif144 (kind 0x144) ----
UiNotif144::UiNotif144() : IntPairPayload() {}

UiNotif144::~UiNotif144() { REB_KEEP_VPTR_STORE(); }

int UiNotif144::GetKind() { return 0x144; }

void UiNotif144::Dispatch(PlayerController* controller) { controller->Virtual34(&m_key, m_valueB); }

void* CreateUiNotif144() { return NewOrNull<UiNotif144>(); }

// ---- UiNotif143 (kind 0x143) ----
UiNotif143::UiNotif143() : IntPairPayload() {}

UiNotif143::~UiNotif143() { REB_KEEP_VPTR_STORE(); }

int UiNotif143::GetKind() { return 0x143; }

void UiNotif143::Dispatch(PlayerController* controller) { controller->Virtual33(&m_key, m_valueB); }

void UiNotif143::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif143() { return NewOrNull<UiNotif143>(); }

// ---- UiNotif145 (kind 0x145) ----
UiNotif145::UiNotif145() : IntPairPayload() {}

UiNotif145::~UiNotif145() { REB_KEEP_VPTR_STORE(); }

int UiNotif145::GetKind() { return 0x145; }

void UiNotif145::Dispatch(PlayerController* controller) { controller->Virtual35(&m_key, m_valueB); }

void UiNotif145::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif145() { return NewOrNull<UiNotif145>(); }

// ---- UiNotif14b (kind 0x14b) ----
UiNotif14b::UiNotif14b() : IntPairPayload() {}

UiNotif14b::~UiNotif14b() { REB_KEEP_VPTR_STORE(); }

int UiNotif14b::GetKind() { return 0x14b; }

void UiNotif14b::Dispatch(PlayerController* controller) { controller->Virtual39(&m_key, m_valueB); }

void UiNotif14b::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif14b() { return NewOrNull<UiNotif14b>(); }

// ---- UiNotif147 (kind 0x147) ----
UiNotif147::UiNotif147() : IntPairPayload() {}

UiNotif147::~UiNotif147() { REB_KEEP_VPTR_STORE(); }

int UiNotif147::GetKind() { return 0x147; }

void* CreateUiNotif147() { return NewOrNull<UiNotif147>(); }

// ---- UiNotif148 (kind 0x148) ----
UiNotif148::UiNotif148() : IntPairPayload() {}

UiNotif148::~UiNotif148() { REB_KEEP_VPTR_STORE(); }

int UiNotif148::GetKind() { return 0x148; }

void UiNotif148::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif148() { return NewOrNull<UiNotif148>(); }

// ---- UiNotif156 (kind 0x156) ----
UiNotif156::UiNotif156() : IntPairPayload() {}

UiNotif156::~UiNotif156() { REB_KEEP_VPTR_STORE(); }

void UiNotif156::Dispatch(PlayerController* controller) { controller->Virtual50(&m_key, m_valueB); }

void UiNotif156::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif156() { return NewOrNull<UiNotif156>(); }

// ---- UiNotif151 (kind 0x151) ----
UiNotif151::UiNotif151() : IntPairPayload() {}

UiNotif151::~UiNotif151() { REB_KEEP_VPTR_STORE(); }

int UiNotif151::GetKind() { return 0x151; }

void UiNotif151::Dispatch(PlayerController* controller) { controller->Virtual49(&m_key, m_valueB); }

void* CreateUiNotif151() { return NewOrNull<UiNotif151>(); }

// ---- UiNotif152 (kind 0x152) ----
UiNotif152::UiNotif152() : IntPairPayload() {}

UiNotif152::~UiNotif152() { REB_KEEP_VPTR_STORE(); }

void UiNotif152::Dispatch(PlayerController* controller) { controller->Virtual48(&m_key, m_valueB); }

void* CreateUiNotif152() { return NewOrNull<UiNotif152>(); }

// ---- UiNotif153 (kind 0x153) ----
UiNotif153::UiNotif153() : IntPairPayload() {}

UiNotif153::~UiNotif153() { REB_KEEP_VPTR_STORE(); }

int UiNotif153::GetKind() { return 0x153; }

void UiNotif153::Dispatch(PlayerController* controller) { controller->Virtual47(&m_key, m_valueB); }

void UiNotif153::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif153() { return NewOrNull<UiNotif153>(); }

// ---- UiNotif154 (kind 0x154) ----
UiNotif154::UiNotif154() : IntPairPayload() {}

UiNotif154::~UiNotif154() { REB_KEEP_VPTR_STORE(); }

int UiNotif154::GetKind() { return 0x154; }

void UiNotif154::Dispatch(PlayerController* controller) { controller->Virtual46(&m_key, m_valueB); }

void UiNotif154::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif154() { return NewOrNull<UiNotif154>(); }

// ---- UiNotif155 (kind 0x155) ----
UiNotif155::UiNotif155() : IntPairPayload() {}

UiNotif155::~UiNotif155() { REB_KEEP_VPTR_STORE(); }

int UiNotif155::GetKind() { return 0x155; }

void UiNotif155::Dispatch(PlayerController* controller) { controller->Virtual45(&m_key, m_valueB); }

void UiNotif155::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif155() { return NewOrNull<UiNotif155>(); }

// ---- UiNotif121 (kind 0x121) ----
UiNotif121::UiNotif121() : IntPairPayload() {}

UiNotif121::~UiNotif121() { REB_KEEP_VPTR_STORE(); }

int UiNotif121::GetKind() { return 0x121; }

void UiNotif121::Dispatch(PlayerController* controller) { controller->Virtual23(m_valueB); }

void* CreateUiNotif121() { return NewOrNull<UiNotif121>(); }

// ---- UiNotif123 (kind 0x123) ----
UiNotif123::UiNotif123() : IntPairPayload() {}

UiNotif123::~UiNotif123() { REB_KEEP_VPTR_STORE(); }

int UiNotif123::GetKind() { return 0x123; }

void UiNotif123::Dispatch(PlayerController* controller) { controller->Virtual27(m_valueB); }

void* CreateUiNotif123() { return NewOrNull<UiNotif123>(); }

// ---- UiNotif122 (kind 0x122) ----
UiNotif122::UiNotif122() : IntPairPayload() {}

UiNotif122::~UiNotif122() { REB_KEEP_VPTR_STORE(); }

int UiNotif122::GetKind() { return 0x122; }

void UiNotif122::Dispatch(PlayerController* controller) { controller->Virtual24(m_valueB); }

void* CreateUiNotif122() { return NewOrNull<UiNotif122>(); }

// ---- UiNotif124 (kind 0x124) ----
UiNotif124::UiNotif124() : IntPairPayload() {}

UiNotif124::~UiNotif124() { REB_KEEP_VPTR_STORE(); }

int UiNotif124::GetKind() { return 0x124; }

void UiNotif124::Dispatch(PlayerController* controller) { controller->Virtual28(m_valueB); }

void* CreateUiNotif124() { return NewOrNull<UiNotif124>(); }

// ---- UiNotif600 (kind 0x600) ----
UiNotif600::UiNotif600() : IntQuadPayload() {}

UiNotif600::~UiNotif600() { REB_KEEP_VPTR_STORE(); }

void UiNotif600::Read(istream* in) { ReadPayload(in); }

void UiNotif600::Write(ostream* out) { WritePayload(out); }

int UiNotif600::GetKind() { return 0x600; }

void UiNotif600::Dispatch(PlayerController* controller) {
    IntPair range;
    controller->Virtual25(reinterpret_cast<int>(GetRange(&range)));
}

IntPair* UiNotif600::GetRange(IntPair* out) {
    out->m_first = m_valueC;
    out->m_second = m_valueD;
    return out;
}

void* CreateUiNotif600() { return NewOrNull<UiNotif600>(); }

// ---- UiNotif120 (kind 0x120) ----
UiNotif120::UiNotif120() : IntPairPayload() {}

UiNotif120::~UiNotif120() { REB_KEEP_VPTR_STORE(); }

int UiNotif120::GetKind() { return 0x120; }

void UiNotif120::Dispatch(PlayerController* controller) { controller->Virtual26(m_valueB); }

void* CreateUiNotif120() { return NewOrNull<UiNotif120>(); }

// ---- UiNotif125 (kind 0x125) ----
UiNotif125::UiNotif125() : IntPairPayload() {}

UiNotif125::~UiNotif125() { REB_KEEP_VPTR_STORE(); }

int UiNotif125::GetKind() { return 0x125; }

void UiNotif125::Dispatch(PlayerController* controller) { controller->Virtual61(m_valueA, m_valueB); }

void UiNotif125::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif125() { return NewOrNull<UiNotif125>(); }

// ---- UiNotif126 (kind 0x126) ----
UiNotif126::UiNotif126() : IntPairPayload() {}

UiNotif126::~UiNotif126() { REB_KEEP_VPTR_STORE(); }

int UiNotif126::GetKind() { return 0x126; }

void UiNotif126::Dispatch(PlayerController* controller) { controller->Virtual60(m_valueA, m_valueB); }

void UiNotif126::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif126() { return NewOrNull<UiNotif126>(); }

// ---- UiNotif127 (kind 0x127) ----
UiNotif127::UiNotif127() : IntPairPayload() {}

UiNotif127::~UiNotif127() { REB_KEEP_VPTR_STORE(); }

int UiNotif127::GetKind() { return 0x127; }

void UiNotif127::Dispatch(PlayerController* controller) { controller->Virtual59(m_valueA, m_valueB); }

void UiNotif127::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif127() { return NewOrNull<UiNotif127>(); }

// ---- UiNotif129 (kind 0x129) ----
UiNotif129::UiNotif129() : IntPairPayload() {}

UiNotif129::~UiNotif129() { REB_KEEP_VPTR_STORE(); }

int UiNotif129::GetKind() { return 0x129; }

void UiNotif129::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif129() { return NewOrNull<UiNotif129>(); }

// ---- UiNotif12a (kind 0x12a) ----
UiNotif12a::UiNotif12a() : IntPairPayload() {}

UiNotif12a::~UiNotif12a() { REB_KEEP_VPTR_STORE(); }

int UiNotif12a::GetKind() { return 0x12a; }

void UiNotif12a::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif12a() { return NewOrNull<UiNotif12a>(); }

// ---- UiNotif12b (kind 0x12b) ----
UiNotif12b::UiNotif12b() : IntPairPayload() {}

UiNotif12b::~UiNotif12b() { REB_KEEP_VPTR_STORE(); }

int UiNotif12b::GetKind() { return 0x12b; }

void UiNotif12b::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif12b() { return NewOrNull<UiNotif12b>(); }

// ---- UiNotif12c (kind 0x12c) ----
UiNotif12c::UiNotif12c() : IntPairPayload() {}

UiNotif12c::~UiNotif12c() { REB_KEEP_VPTR_STORE(); }

int UiNotif12c::GetKind() { return 0x12c; }

void UiNotif12c::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif12c() { return NewOrNull<UiNotif12c>(); }

// ---- UiNotif128 (kind 0x128) ----
UiNotif128::UiNotif128() : IntPairPayload() {}

UiNotif128::~UiNotif128() { REB_KEEP_VPTR_STORE(); }

int UiNotif128::GetKind() { return 0x128; }

void UiNotif128::Dispatch(PlayerController* controller) { controller->Virtual29(m_valueB); }

void UiNotif128::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif128() { return NewOrNull<UiNotif128>(); }

// ---- UiNotif380 (kind 0x380) ----
UiNotif380::UiNotif380() : KeyPairPayload() {}

UiNotif380::~UiNotif380() { REB_KEEP_VPTR_STORE(); }

void UiNotif380::Dispatch(PlayerController* controller) { controller->Virtual81(&m_key, &m_keyA, &m_keyB); }

void UiNotif380::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual06(GetKind(), asArg(&m_key), asArg(&m_keyA), asArg(&m_keyB));
}

void* CreateUiNotif380() { return NewOrNull<UiNotif380>(); }

// ---- UiNotif1f0 (kind 0x1f0) ----
UiNotif1f0::UiNotif1f0() : IntPairPayload() {}

UiNotif1f0::~UiNotif1f0() { REB_KEEP_VPTR_STORE(); }

int UiNotif1f0::GetKind() { return 0x1f0; }

void UiNotif1f0::Dispatch(PlayerController* controller) { controller->Virtual74(&m_key, m_valueB); }

void UiNotif1f0::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif1f0() { return NewOrNull<UiNotif1f0>(); }

// ---- UiNotif1f1 (kind 0x1f1) ----
UiNotif1f1::UiNotif1f1() : IntPairPayload() {}

UiNotif1f1::~UiNotif1f1() { REB_KEEP_VPTR_STORE(); }

int UiNotif1f1::GetKind() { return 0x1f1; }

void UiNotif1f1::Dispatch(PlayerController* controller) { controller->Virtual75(&m_key, m_valueB); }

void UiNotif1f1::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif1f1() { return NewOrNull<UiNotif1f1>(); }

// ---- UiNotif370 (kind 0x370) ----
UiNotif370::UiNotif370() : KeyPairPayload() {}

UiNotif370::~UiNotif370() { REB_KEEP_VPTR_STORE(); }

void UiNotif370::Dispatch(PlayerController* controller) { controller->Virtual76(&m_key, reinterpret_cast<int>(&m_keyA), &m_keyB); }

void* CreateUiNotif370() { return NewOrNull<UiNotif370>(); }

// ---- UiNotif221 (kind 0x221) ----
UiNotif221::UiNotif221() : IntPairPayload() {}

UiNotif221::~UiNotif221() { REB_KEEP_VPTR_STORE(); }

void UiNotif221::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif221() { return NewOrNull<UiNotif221>(); }

// ---- UiNotif220 (kind 0x220) ----
UiNotif220::UiNotif220() : IntPairPayload() {}

UiNotif220::~UiNotif220() { REB_KEEP_VPTR_STORE(); }

void UiNotif220::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif220() { return NewOrNull<UiNotif220>(); }

// ---- UiNotif161 (kind 0x161) ----
UiNotif161::UiNotif161() : IntPairPayload() {}

UiNotif161::~UiNotif161() { REB_KEEP_VPTR_STORE(); }

int UiNotif161::GetKind() { return 0x161; }

void UiNotif161::Dispatch(PlayerController* controller) { controller->Virtual52(&m_key, m_valueB); }

void* CreateUiNotif161() { return NewOrNull<UiNotif161>(); }

// ---- UiNotif321 (kind 0x321) ----
UiNotif321::UiNotif321() : KeyPairPayload() {}

UiNotif321::~UiNotif321() { REB_KEEP_VPTR_STORE(); }

void UiNotif321::Dispatch(PlayerController* controller) { controller->Virtual53(&m_key, &m_keyB); }

void* CreateUiNotif321() { return NewOrNull<UiNotif321>(); }

// ---- UiNotif320 (kind 0x320) ----
UiNotif320::UiNotif320() : KeyPairPayload() {}

UiNotif320::~UiNotif320() { REB_KEEP_VPTR_STORE(); }

void UiNotif320::Dispatch(PlayerController* controller) { controller->Virtual55(&m_key, &m_keyB, &m_keyB); }

void* CreateUiNotif320() { return NewOrNull<UiNotif320>(); }

// ---- UiNotif520 (kind 0x520) ----
UiNotif520::UiNotif520() : StringPairPayload() {}

UiNotif520::~UiNotif520() { REB_KEEP_VPTR_STORE(); }

void UiNotif520::Read(istream* in) { ReadPayload(in); }

void UiNotif520::Write(ostream* out) { WritePayload(out); }

int UiNotif520::GetKind() { return 0x520; }

void UiNotif520::Dispatch(PlayerController* controller) {
    String* text = NewOrNull<String>();
    if (text) {
        text->Assign(m_textB);
        controller->Virtual54(&m_key, text);
        delete text;
    }
}

void* CreateUiNotif520() { return NewOrNull<UiNotif520>(); }

// ---- UiNotif160 (kind 0x160) ----
UiNotif160::UiNotif160() : IntPairPayload() {}

UiNotif160::~UiNotif160() { REB_KEEP_VPTR_STORE(); }

int UiNotif160::GetKind() { return 0x160; }

void UiNotif160::Dispatch(PlayerController* controller) { controller->Virtual51(&m_key, m_valueB); }

void UiNotif160::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif160() { return NewOrNull<UiNotif160>(); }

// ---- UiNotif162 (kind 0x162) ----
UiNotif162::UiNotif162() : IntPairPayload() {}

UiNotif162::~UiNotif162() { REB_KEEP_VPTR_STORE(); }

int UiNotif162::GetKind() { return 0x162; }

void UiNotif162::Dispatch(PlayerController* controller) { controller->Virtual56(&m_key, m_valueB); }

void* CreateUiNotif162() { return NewOrNull<UiNotif162>(); }

// ---- UiNotif400 (kind 0x400) ----
UiNotif400::UiNotif400() : NullableKeyPairPayload() {}

UiNotif400::~UiNotif400() { REB_KEEP_VPTR_STORE(); }

void UiNotif400::Read(istream* in) { ReadPayload(in); }

void UiNotif400::Write(ostream* out) { WritePayload(out); }

void UiNotif400::Dispatch(PlayerController* controller) { controller->Virtual57(&m_key, reinterpret_cast<const ObjectKey*>(&m_keyB)); }

void* CreateUiNotif400() { return NewOrNull<UiNotif400>(); }

// ---- UiNotif163 (kind 0x163) ----
UiNotif163::UiNotif163() : IntPairPayload() {}

UiNotif163::~UiNotif163() { REB_KEEP_VPTR_STORE(); }

int UiNotif163::GetKind() { return 0x163; }

void UiNotif163::Dispatch(PlayerController* controller) { controller->Virtual58(&m_key, m_valueB); }

void* CreateUiNotif163() { return NewOrNull<UiNotif163>(); }

// ---- UiNotif360 (kind 0x360) ----
UiNotif360::UiNotif360() : KeyPairPayload() {}

UiNotif360::~UiNotif360() { REB_KEEP_VPTR_STORE(); }

int UiNotif360::GetKind() { return 0x360; }

void UiNotif360::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif360() { return NewOrNull<UiNotif360>(); }

// ---- UiNotif361 (kind 0x361) ----
UiNotif361::UiNotif361() : KeyPairPayload() {}

UiNotif361::~UiNotif361() { REB_KEEP_VPTR_STORE(); }

int UiNotif361::GetKind() { return 0x361; }

void UiNotif361::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual06(GetKind(), asArg(&m_key), asArg(&m_keyA), asArg(&m_keyB));
}

void* CreateUiNotif361() { return NewOrNull<UiNotif361>(); }

// ---- UiNotif362 (kind 0x362) ----
UiNotif362::UiNotif362() : KeyPairPayload() {}

UiNotif362::~UiNotif362() { REB_KEEP_VPTR_STORE(); }

int UiNotif362::GetKind() { return 0x362; }

void UiNotif362::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif362() { return NewOrNull<UiNotif362>(); }

// ---- UiNotif1e1 (kind 0x1e1) ----
UiNotif1e1::UiNotif1e1() : IntPairPayload() {}

UiNotif1e1::~UiNotif1e1() { REB_KEEP_VPTR_STORE(); }

int UiNotif1e1::GetKind() { return 0x1e1; }

void UiNotif1e1::Dispatch(PlayerController* controller) { controller->Virtual78(&m_key, m_valueB); }

void UiNotif1e1::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual05(GetKind(), asArg(this));
}

void* CreateUiNotif1e1() { return NewOrNull<UiNotif1e1>(); }

// ---- UiNotif1e5 (kind 0x1e5) ----
UiNotif1e5::UiNotif1e5() : IntPairPayload() {}

UiNotif1e5::~UiNotif1e5() { REB_KEEP_VPTR_STORE(); }

int UiNotif1e5::GetKind() { return 0x1e5; }

void* CreateUiNotif1e5() { return NewOrNull<UiNotif1e5>(); }

// ---- UiNotif1e4 (kind 0x1e4) ----
UiNotif1e4::UiNotif1e4() : IntPairPayload() {}

UiNotif1e4::~UiNotif1e4() { REB_KEEP_VPTR_STORE(); }

int UiNotif1e4::GetKind() { return 0x1e4; }

void* CreateUiNotif1e4() { return NewOrNull<UiNotif1e4>(); }

// ---- UiNotif1e2 (kind 0x1e2) ----
UiNotif1e2::UiNotif1e2() : IntPairPayload() {}

UiNotif1e2::~UiNotif1e2() { REB_KEEP_VPTR_STORE(); }

int UiNotif1e2::GetKind() { return 0x1e2; }

void UiNotif1e2::Dispatch(PlayerController* controller) { controller->Virtual80(&m_key, m_valueB); }

void* CreateUiNotif1e2() { return NewOrNull<UiNotif1e2>(); }

// ---- UiNotif1e3 (kind 0x1e3) ----
UiNotif1e3::UiNotif1e3() : IntPairPayload() {}

UiNotif1e3::~UiNotif1e3() { REB_KEEP_VPTR_STORE(); }

int UiNotif1e3::GetKind() { return 0x1e3; }

void* CreateUiNotif1e3() { return NewOrNull<UiNotif1e3>(); }

// ---- UiNotif210 (kind 0x210) ----
UiNotif210::UiNotif210() : IntPairPayload() {}

UiNotif210::~UiNotif210() { REB_KEEP_VPTR_STORE(); }

void UiNotif210::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif210() { return NewOrNull<UiNotif210>(); }

// ---- UiNotif211 (kind 0x211) ----
UiNotif211::UiNotif211() : IntPairPayload() {}

UiNotif211::~UiNotif211() { REB_KEEP_VPTR_STORE(); }

void UiNotif211::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif211() { return NewOrNull<UiNotif211>(); }

// ---- UiNotif212 (kind 0x212) ----
UiNotif212::UiNotif212() : IntPairPayload() {}

UiNotif212::~UiNotif212() { REB_KEEP_VPTR_STORE(); }

void UiNotif212::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif212() { return NewOrNull<UiNotif212>(); }

// ---- UiNotif230 (kind 0x230) ----
UiNotif230::UiNotif230() : IntPairPayload() {}

UiNotif230::~UiNotif230() { REB_KEEP_VPTR_STORE(); }

void UiNotif230::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif230() { return NewOrNull<UiNotif230>(); }

// ---- UiNotif231 (kind 0x231) ----
UiNotif231::UiNotif231() : IntPairPayload() {}

UiNotif231::~UiNotif231() { REB_KEEP_VPTR_STORE(); }

int UiNotif231::GetKind() { return 0x231; }

void UiNotif231::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif231() { return NewOrNull<UiNotif231>(); }

// ---- UiNotif300 (kind 0x300) ----
UiNotif300::UiNotif300() : KeyPairPayload() {}

UiNotif300::~UiNotif300() { REB_KEEP_VPTR_STORE(); }

void UiNotif300::Dispatch(PlayerController* controller) { controller->Virtual83(&m_key, &m_keyA, &m_keyB); }

void UiNotif300::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual06(GetKind(), asArg(&m_key), asArg(&m_keyA), asArg(&m_keyB));
}

void* CreateUiNotif300() { return NewOrNull<UiNotif300>(); }

// ---- UiNotif301 (kind 0x301) ----
UiNotif301::UiNotif301() : KeyPairPayload() {}

UiNotif301::~UiNotif301() { REB_KEEP_VPTR_STORE(); }

void UiNotif301::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual06(GetKind(), asArg(&m_key), asArg(&m_keyA), asArg(&m_keyB));
}

void* CreateUiNotif301() { return NewOrNull<UiNotif301>(); }

// ---- UiNotif103 (kind 0x103) ----
UiNotif103::UiNotif103() : IntPairPayload() {}

UiNotif103::~UiNotif103() { REB_KEEP_VPTR_STORE(); }

int UiNotif103::GetKind() { return 0x103; }

void UiNotif103::Dispatch(PlayerController* controller) { controller->Virtual82(&m_key); }

void* CreateUiNotif103() { return NewOrNull<UiNotif103>(); }

// ---- UiNotif107 (kind 0x107) ----
UiNotif107::UiNotif107() : IntPairPayload() {}

UiNotif107::~UiNotif107() { REB_KEEP_VPTR_STORE(); }

int UiNotif107::GetKind() { return 0x107; }

void UiNotif107::Dispatch(PlayerController* controller) { controller->Virtual90(&m_key); }

void UiNotif107::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif107() { return NewOrNull<UiNotif107>(); }

// ---- UiNotif100 (kind 0x100) ----
UiNotif100::UiNotif100() : IntPairPayload() {}

UiNotif100::~UiNotif100() { REB_KEEP_VPTR_STORE(); }

void UiNotif100::Dispatch(PlayerController* controller) { controller->Virtual84(&m_key, m_valueB); }

void UiNotif100::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual09(GetKind(), asArg(&m_key), m_valueB, m_valueA);
}

void* CreateUiNotif100() { return NewOrNull<UiNotif100>(); }

// ---- UiNotif102 (kind 0x102) ----
UiNotif102::UiNotif102() : IntPairPayload() {}

UiNotif102::~UiNotif102() { REB_KEEP_VPTR_STORE(); }

int UiNotif102::GetKind() { return 0x102; }

void UiNotif102::Dispatch(PlayerController* controller) { controller->Virtual82(&m_key); }

void* CreateUiNotif102() { return NewOrNull<UiNotif102>(); }

// ---- UiNotif302 (kind 0x302) ----
UiNotif302::UiNotif302() : KeyPairPayload() {}

UiNotif302::~UiNotif302() { REB_KEEP_VPTR_STORE(); }

void UiNotif302::Dispatch(PlayerController* controller) {
    ObjectKey keyA;
    controller->Virtual91(&m_key, copyFirstKey(this, &keyA));
    if (m_value0c == 0) UiSendNotificationToSide(m_value18, reinterpret_cast<int>(&m_key));
}

void UiNotif302::DispatchToSink(UiNotice* sink) {
    if (!sink) {
        UiSendNotificationToSide(m_value18, asArg(&m_key));
        return;
    }
    ObjectKey keyA;
    sink->OnMessage(GetKind(), asArg(&m_key), asArg(copyFirstKey(this, &keyA)));
}

void* CreateUiNotif302() { return NewOrNull<UiNotif302>(); }

// ---- UiNotif303 (kind 0x303) ----
UiNotif303::UiNotif303() : KeyPairPayload() {}

UiNotif303::~UiNotif303() { REB_KEEP_VPTR_STORE(); }

void* CreateUiNotif303() { return NewOrNull<UiNotif303>(); }

// ---- UiNotif304 (kind 0x304) ----
UiNotif304::UiNotif304() : KeyPairPayload() {}

UiNotif304::~UiNotif304() { REB_KEEP_VPTR_STORE(); }

void UiNotif304::DispatchToSink(UiNotice* sink) {
    if (sink) {
        ObjectKey keyA;
        sink->OnMessage(GetKind(), asArg(&m_key), asArg(copyFirstKey(this, &keyA)));
    }
}

void* CreateUiNotif304() { return NewOrNull<UiNotif304>(); }

// ---- UiNotif305 (kind 0x305) ----
UiNotif305::UiNotif305() : KeyPairPayload() {}

UiNotif305::~UiNotif305() { REB_KEEP_VPTR_STORE(); }

void UiNotif305::Dispatch(PlayerController* controller) {
    ObjectKey keyA;
    controller->Virtual92(reinterpret_cast<int>(&m_key), copyFirstKey(this, &keyA));
}

void UiNotif305::DispatchToSink(UiNotice* sink) {
    if (sink) {
        ObjectKey keyA;
        sink->OnMessage(GetKind(), asArg(&m_key), asArg(copyFirstKey(this, &keyA)));
    }
}

void* CreateUiNotif305() { return NewOrNull<UiNotif305>(); }

// ---- UiNotif306 (kind 0x306) ----
UiNotif306::UiNotif306() : KeyPairPayload() {}

UiNotif306::~UiNotif306() { REB_KEEP_VPTR_STORE(); }

void UiNotif306::DispatchToSink(UiNotice* sink) {
    if (sink) {
        ObjectKey keyA;
        sink->OnMessage(GetKind(), asArg(&m_key), asArg(copyFirstKey(this, &keyA)));
    }
}

void* CreateUiNotif306() { return NewOrNull<UiNotif306>(); }

// ---- UiNotif105 (kind 0x105) ----
UiNotif105::UiNotif105() : IntPairPayload() {}

UiNotif105::~UiNotif105() { REB_KEEP_VPTR_STORE(); }

int UiNotif105::GetKind() { return 0x105; }

void UiNotif105::Dispatch(PlayerController* controller) { controller->Virtual85(&m_key, m_valueB); }

void UiNotif105::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif105() { return NewOrNull<UiNotif105>(); }

// ---- UiNotif104 (kind 0x104) ----
UiNotif104::UiNotif104() : IntPairPayload() {}

UiNotif104::~UiNotif104() { REB_KEEP_VPTR_STORE(); }

int UiNotif104::GetKind() { return 0x104; }

void UiNotif104::Dispatch(PlayerController* controller) { controller->Virtual86(&m_key, m_valueB); }

void* CreateUiNotif104() { return NewOrNull<UiNotif104>(); }

// ---- UiNotif500 (kind 0x500) ----
UiNotif500::UiNotif500() : StringPairPayload() {}

UiNotif500::~UiNotif500() { REB_KEEP_VPTR_STORE(); }

int UiNotif500::GetKind() { return 0x500; }

void UiNotif500::Dispatch(PlayerController* controller) {
    alignas(String) char nameStorage[sizeof(String)];
    String* name = CopyNameForSink(reinterpret_cast<String*>(nameStorage));
    const int* words = reinterpret_cast<const int*>(name);
    controller->Virtual87(GetKeyPointer(), words[0], words[1], words[2]);
}

ObjectKey* UiNotif500::GetKeyPointer() { return &m_key; }

String* UiNotif500::CopyName(String* out) {
    new (out) String(m_textB);
    return out;
}

String* UiNotif500::CopyNameForSink(String* out) {
    CopyName(out);
    return out;
}

void* CreateUiNotif500() { return NewOrNull<UiNotif500>(); }

// ---- UiNotif101 (kind 0x101) ----
UiNotif101::UiNotif101() : IntPairPayload() {}

UiNotif101::~UiNotif101() { REB_KEEP_VPTR_STORE(); }

int UiNotif101::GetKind() { return 0x101; }

void UiNotif101::Dispatch(PlayerController* controller) { controller->Virtual88(&m_key, m_valueB); }

void* CreateUiNotif101() { return NewOrNull<UiNotif101>(); }

// ---- UiNotif106 (kind 0x106) ----
UiNotif106::UiNotif106() : IntPairPayload() {}

UiNotif106::~UiNotif106() { REB_KEEP_VPTR_STORE(); }

int UiNotif106::GetKind() { return 0x106; }

void UiNotif106::Dispatch(PlayerController* controller) { controller->Virtual89(&m_key, m_valueB); }

void UiNotif106::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif106() { return NewOrNull<UiNotif106>(); }

// ---- UiNotif183 (kind 0x183) ----
UiNotif183::UiNotif183() : IntPairPayload() {}

UiNotif183::~UiNotif183() { REB_KEEP_VPTR_STORE(); }

int UiNotif183::GetKind() { return 0x183; }

void UiNotif183::Dispatch(PlayerController* controller) { controller->Virtual62(&m_key, m_valueB); }

void UiNotif183::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif183() { return NewOrNull<UiNotif183>(); }

// ---- UiNotif180 (kind 0x180) ----
UiNotif180::UiNotif180() : IntPairPayload() {}

UiNotif180::~UiNotif180() { REB_KEEP_VPTR_STORE(); }

int UiNotif180::GetKind() { return 0x180; }

void UiNotif180::Dispatch(PlayerController* controller) { controller->Virtual63(&m_key, m_valueB); }

void UiNotif180::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif180() { return NewOrNull<UiNotif180>(); }

// ---- UiNotif181 (kind 0x181) ----
UiNotif181::UiNotif181() : IntPairPayload() {}

UiNotif181::~UiNotif181() { REB_KEEP_VPTR_STORE(); }

int UiNotif181::GetKind() { return 0x181; }

void UiNotif181::Dispatch(PlayerController* controller) { controller->Virtual64(&m_key, m_valueB); }

void* CreateUiNotif181() { return NewOrNull<UiNotif181>(); }

// ---- UiNotif182 (kind 0x182) ----
UiNotif182::UiNotif182() : IntPairPayload() {}

UiNotif182::~UiNotif182() { REB_KEEP_VPTR_STORE(); }

int UiNotif182::GetKind() { return 0x182; }

void UiNotif182::Dispatch(PlayerController* controller) { controller->Virtual65(&m_key, m_valueB); }

void UiNotif182::DispatchToSink(UiNotice* sink) {
    if (sink) sink->Virtual08(GetKind(), asArg(&m_key), m_valueB);
}

void* CreateUiNotif182() { return NewOrNull<UiNotif182>(); }

// ---- UiNotif1a0 (kind 0x1a0) ----
UiNotif1a0::UiNotif1a0() : IntPairPayload() {}

UiNotif1a0::~UiNotif1a0() { REB_KEEP_VPTR_STORE(); }

int UiNotif1a0::GetKind() { return 0x1a0; }

void UiNotif1a0::Dispatch(PlayerController* controller) { controller->Virtual73(&m_key, m_valueB); }

void* CreateUiNotif1a0() { return NewOrNull<UiNotif1a0>(); }

// ---- UiNotif2 (kind 0x2) ----
UiNotif2::UiNotif2() : IntQuadPayload() {}

UiNotif2::~UiNotif2() { REB_KEEP_VPTR_STORE(); }

int UiNotif2::GetKind() { return 0x2; }

void UiNotif2::DispatchToSink(UiNotice* sink) {
    if ((g_uiNotif2KeyReady & 1) == 0) {
        g_uiNotif2KeyReady |= 1;
        new (&g_uiNotif2Key) ObjectKey();
        atexit(DestroyUiNotif2Key);
    }
    if (sink) sink->Virtual08(GetKind(), asArg(&g_uiNotif2Key), 1);
}

void* CreateUiNotif2() { return NewOrNull<UiNotif2>(); }

// ---- UiNotif1c0 (kind 0x1c0) ----
UiNotif1c0::UiNotif1c0() : IntPairPayload() {}

UiNotif1c0::~UiNotif1c0() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c0::GetKind() { return 0x1c0; }

void UiNotif1c0::Dispatch(PlayerController* controller) { controller->Virtual66(&m_key, m_valueB); }

void* CreateUiNotif1c0() { return NewOrNull<UiNotif1c0>(); }

// ---- UiNotif1c1 (kind 0x1c1) ----
UiNotif1c1::UiNotif1c1() : IntPairPayload() {}

UiNotif1c1::~UiNotif1c1() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c1::GetKind() { return 0x1c1; }

void UiNotif1c1::Dispatch(PlayerController* controller) { controller->Virtual67(&m_key, m_valueB); }

void* CreateUiNotif1c1() { return NewOrNull<UiNotif1c1>(); }

// ---- UiNotif1c2 (kind 0x1c2) ----
UiNotif1c2::UiNotif1c2() : IntPairPayload() {}

UiNotif1c2::~UiNotif1c2() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c2::GetKind() { return 0x1c2; }

void UiNotif1c2::Dispatch(PlayerController* controller) { controller->Virtual68(&m_key, m_valueB); }

void* CreateUiNotif1c2() { return NewOrNull<UiNotif1c2>(); }

// ---- UiNotif1c3 (kind 0x1c3) ----
UiNotif1c3::UiNotif1c3() : IntPairPayload() {}

UiNotif1c3::~UiNotif1c3() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c3::GetKind() { return 0x1c3; }

void UiNotif1c3::Dispatch(PlayerController* controller) { controller->Virtual69(&m_key, m_valueB); }

void* CreateUiNotif1c3() { return NewOrNull<UiNotif1c3>(); }

// ---- UiNotif1c4 (kind 0x1c4) ----
UiNotif1c4::UiNotif1c4() : IntPairPayload() {}

UiNotif1c4::~UiNotif1c4() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c4::GetKind() { return 0x1c4; }

void UiNotif1c4::Dispatch(PlayerController* controller) { controller->Virtual70(&m_key, m_valueB); }

void* CreateUiNotif1c4() { return NewOrNull<UiNotif1c4>(); }

// ---- UiNotif1c5 (kind 0x1c5) ----
UiNotif1c5::UiNotif1c5() : IntPairPayload() {}

UiNotif1c5::~UiNotif1c5() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c5::GetKind() { return 0x1c5; }

void UiNotif1c5::Dispatch(PlayerController* controller) { controller->Virtual71(&m_key, m_valueB); }

void* CreateUiNotif1c5() { return NewOrNull<UiNotif1c5>(); }

// ---- UiNotif1c6 (kind 0x1c6) ----
UiNotif1c6::UiNotif1c6() : IntPairPayload() {}

UiNotif1c6::~UiNotif1c6() { REB_KEEP_VPTR_STORE(); }

int UiNotif1c6::GetKind() { return 0x1c6; }

void UiNotif1c6::Dispatch(PlayerController* controller) { controller->Virtual72(&m_key, m_valueB); }

void* CreateUiNotif1c6() { return NewOrNull<UiNotif1c6>(); }

// ---- UiNotif1 (kind 0x1) ----
UiNotif1::UiNotif1() : IntQuadPayload() {}

UiNotif1::~UiNotif1() { REB_KEEP_VPTR_STORE(); }

void UiNotif1::Dispatch(PlayerController* controller) { controller->Virtual22(m_valueC, m_valueD); }

void* CreateUiNotif1() { return NewOrNull<UiNotif1>(); }

void DestroyUiNotif2Key() {
    REB_KEEP_ATEXIT_TARGET();
    g_uiNotif2Key.~ObjectKey();
}
