#include "mission/mission_autoroute.h"

void DestroyMissionAutorouteEventKinds();

extern int g_missionAutorouteEventKindsReady;      // 0x6bbae0
extern EventKindSet g_missionAutorouteEventKinds;  // 0x6bbae8

namespace {
constexpr uint32_t kFamilyMissionAutoroute = 0x43;
}

MissionAutoroute::MissionAutoroute() {}

MissionAutoroute::~MissionAutoroute() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionAutoroute::GetFamily() { return kFamilyMissionAutoroute; }

int MissionAutoroute::Read(istream* in) { return reinterpret_cast<MissionRuleBase*>(this)->MissionRuleBase::ReadBody(in); }

int MissionAutoroute::Write(ostream* out) { return MissionTypeWriteForward(this, out); }

int MissionAutoroute::Checksum(Crc32* crc) { return this->MissionTypeBase::ChecksumBody(crc); }

int MissionAutoroute::Validate() { return static_cast<Mission*>(this)->Mission::ValidateImpl(); }

int MissionAutoroute::ApplyAll(NotifyContext* ctx) { return IsSideView() ? MissionTypeApplyAllForward(this, ctx) : 0; }

int MissionAutoroute::CreateViews(int controlKind, NotifyContext* ctx) { return this->Mission::CreateViewsImpl(controlKind, ctx); }

int MissionAutoroute::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionAutorouteEventKinds, out);
}

int MissionAutoroute::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionAutorouteEventKinds, orderKind, out);
}

GameObj* MissionAutoroute::CreateViewObject() { return NewOrNull<MissionAutoroute>(); }

GameObjNotifier* MissionAutoroute::Create() { return NewOrNull<MissionAutoroute>(); }

int MissionAutoroute::BuildEventKinds() {
    int result = 1;
    if (!g_missionAutorouteEventKindsReady) {
        g_missionAutorouteEventKindsReady = 1;
        result = MissionTypeBase::InitEventKinds(0, &g_missionAutorouteEventKinds);
    }
    return result;
}

void ConstructMissionAutorouteEventKinds() { new (&g_missionAutorouteEventKinds) EventKindSet(); }

void RegisterMissionAutorouteEventKindsAtExit() { atexit(DestroyMissionAutorouteEventKinds); }

void MissionAutoroute::InitStatics() {
    ConstructMissionAutorouteEventKinds();
    RegisterMissionAutorouteEventKindsAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_missionAutorouteEventKindsDestroyedGuard;  // 0x6bbaf0

void DestroyMissionAutorouteEventKinds() {
    if (!(g_missionAutorouteEventKindsDestroyedGuard & 0x1)) {
        g_missionAutorouteEventKindsDestroyedGuard |= 0x1;
        g_missionAutorouteEventKinds.~EventKindSet();
    }
}
