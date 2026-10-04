#include "mission/mission_autoroute.h"

extern "C" void __cdecl LAB_005762d0();

extern int g_missionAutorouteEventKindsReady;      // 0x6bbae0
extern EventKindSet g_missionAutorouteEventKinds;  // 0x6bbae8

namespace {
constexpr uint32_t kFamilyMissionAutoroute = 0x43;
}

MissionAutoroute::MissionAutoroute() {}

MissionAutoroute::~MissionAutoroute() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionAutoroute::GetFamily() { return kFamilyMissionAutoroute; }

int MissionAutoroute::Read(istream* in) { return FUN_00592c50(this, in); }

int MissionAutoroute::Write(ostream* out) { return FUN_0056ec40(this, out); }

int MissionAutoroute::Checksum(Crc32* crc) { return FUN_00593ad0(this, crc); }

int MissionAutoroute::Validate() { return FUN_00523c60(this); }

int MissionAutoroute::ApplyAll(NotifyContext* ctx) { return IsSideView() ? FUN_0056ec50(this, ctx) : 0; }

int MissionAutoroute::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_005236e0(this, controlKind, ctx); }

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

void RegisterMissionAutorouteEventKindsAtExit() { atexit(LAB_005762d0); }

void MissionAutoroute::InitStatics() {
    ConstructMissionAutorouteEventKinds();
    RegisterMissionAutorouteEventKindsAtExit();
}
