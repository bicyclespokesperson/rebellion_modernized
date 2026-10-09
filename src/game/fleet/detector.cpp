#include "fleet/detector.h"

#include <new>

#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "data/sd_record_bases.h"
#include "fleet/unit_data_decl.h"
#include "fleet/unit_view_flags.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
void DestroyDetectorEventKinds();

extern int g_detectorEventKindsReady;      // 0x6bb360
extern EventKindSet g_detectorEventKinds;  // 0x6bb368

namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kApplyEverything = 1;

SdRecordExt2* recordOf(Detector* detector) { return static_cast<SdRecordExt2*>(detector->m_record); }
}  // namespace

Detector::Detector() { m_detectorFlags = 0; }

Detector::~Detector() { REB_KEEP_VPTR_STORE(); }

int Detector::GetEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    if (ready) *out = g_detectorEventKinds;
    return ready;
}

int Detector::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int ready = EnsureEventKinds();
    if (ready && g_detectorEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return ready;
}

int Detector::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

GameObjData* Detector::CreateData() { return new DetectorData(); }

int Detector::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) {
    return UnitObjBase::OnParentChanged(oldParent, newParent, ctx);
}

int Detector::OnUsableChanged(int newValue, NotifyContext* ctx) { return UnitObjBase::OnUsableChanged(newValue, ctx); }

int Detector::OnDestroyedChanged(int newValue, NotifyContext* ctx) {
    return UnitObjBase::OnDestroyedChanged(newValue, ctx);
}

int Detector::OnExistingChanged(int newValue, NotifyContext* ctx) {
    return UnitObjBase::OnExistingChanged(newValue, ctx);
}

int Detector::OnDamagedChanged(int newValue, NotifyContext* ctx) { return GameObj::OnDamagedChanged(newValue, ctx); }

int Detector::Read(istream* in) {
    const int result = GameObj::Read(in);
    if (result) ReadValue32(in, &m_detectorFlags);
    return result;
}

int Detector::Write(ostream* out) {
    const int result = GameObj::Write(out);
    if (result) WriteValue32(out, &m_detectorFlags);
    return result;
}

int Detector::Checksum(Crc32* crc) {
    const int result = GameObj::Checksum(crc);
    if (result) crc->AddInt(m_detectorFlags);
    return result;
}

int Detector::Validate() {
    int result = GameObj::Validate();
    GameObjNotifier* parent = nullptr;
    if (result) result = GetParent(&parent);
    return result;
}

int Detector::ApplyAll(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) result = ApplyAllToSideView(ctx);
    if (result && ctx->m_flag04 == kApplyEverything) ApplyIsDecoyed(ctx);
    return result;
}

uint32_t Detector::GetRecordField58() { return m_record ? recordOf(this)->m_field58 : 0; }

uint32_t Detector::GetRecordField5c() { return m_record ? recordOf(this)->m_field5c : 0; }

int Detector::OnIsDecoyedNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(DetectorIsDecoyedNotif)", "IsDecoyed", value, 0, ctx);
    return result;
}

int Detector::SetIsDecoyed(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(kIsDecoyedFlag, on, &m_detectorFlags)) {
        static_cast<Detector*>(GetViewObject(1))->ApplyIsDecoyed(ctx);
        static_cast<Detector*>(GetViewObject(2))->ApplyIsDecoyed(ctx);
        OnIsDecoyedChanged(on, ctx);
    }
    return result;
}

int Detector::ApplyIsDecoyed(NotifyContext* ctx) {
    return fleet::applyFlagToView(this, &Detector::m_detectorFlags, kIsDecoyedFlag, ctx,
                                  [&](int value) { return OnIsDecoyedNotified(value, ctx); });
}

int Detector::EnsureEventKinds() {
    if (g_detectorEventKindsReady == 0) {
        g_detectorEventKindsReady = 1;
        UnitObjBase::InitEventKinds(0, &g_detectorEventKinds);
    }
    return 1;
}

int Detector::InitEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    *out = g_detectorEventKinds;
    return ready;
}

void DetectorStaticInit() {
    ConstructDetectorEventKinds();
    RegisterDetectorEventKindsAtExit();
}

void ConstructDetectorEventKinds() { new (&g_detectorEventKinds) EventKindSet(); }

void RegisterDetectorEventKindsAtExit() { atexit(DestroyDetectorEventKinds); }

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_detectorEventKindsDestroyedGuard;  // 0x6bb370

void DestroyDetectorEventKinds() {
    if (!(g_detectorEventKindsDestroyedGuard & 0x1)) {
        g_detectorEventKindsDestroyedGuard |= 0x1;
        g_detectorEventKinds.~EventKindSet();
    }
}
