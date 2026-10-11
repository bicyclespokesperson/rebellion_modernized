#include "mission/mission_autoroute.h"


extern int g_missionAutorouteEventKindsReady;      // 0x6bbae0
extern EventKindSet g_missionAutorouteEventKinds;  // 0x6bbae8

namespace {
constexpr uint32_t kFamilyMissionAutoroute = 0x43;
}

MissionAutoroute::MissionAutoroute() = default;

MissionAutoroute::~MissionAutoroute() = default;

uint32_t MissionAutoroute::GetFamily() { return kFamilyMissionAutoroute; }

int MissionAutoroute::Read(istream* in) { return this->Mission::ReadImpl(in); }

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
EventKindSet g_missionAutorouteEventKinds{};
int g_missionAutorouteEventKindsReady{};
