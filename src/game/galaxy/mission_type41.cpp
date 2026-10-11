#include "galaxy/mission_type41.h"

#include "core/checked_new.h"
#include "gameobj/game_hooks.h"
#include "mission/mission_bases.h"
#include "mission/mission_forwarders.h"


extern int g_mission41EventKindsReady;      // 0x6bb9b0
extern EventKindSet g_mission41EventKinds;  // 0x6bb9b8


namespace {

constexpr uint32_t kFamilyMission41 = 0x41;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;

}  // namespace

Mission41::Mission41() = default;

Mission41::~Mission41() = default;

uint32_t Mission41::GetFamily() { return kFamilyMission41; }

int Mission41::Read(istream* in) { return this->Mission::ReadImpl(in); }

int Mission41::Write(ostream* out) { return MissionTypeWriteForward(this, out); }

int Mission41::Checksum(Crc32* crc) { return this->MissionTypeBase::ChecksumBody(crc); }

int Mission41::Validate() { return static_cast<Mission*>(this)->Mission::ValidateImpl(); }

int Mission41::ApplyAll(NotifyContext* ctx) {
    if (IsSideView()) return MissionTypeApplyAllForward(this, ctx);
    return 0;
}

int Mission41::CreateViews(int controlKind, NotifyContext* ctx) { return MissionTypeCreateViewsForward(this, controlKind, ctx); }

int Mission41::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_mission41EventKinds;
    return result;
}

int Mission41::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_mission41EventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

GameObj* Mission41::CreateViewObject() { return NewOrNull<Mission41>(); }

GameObjNotifier* Mission41::Create() { return NewOrNull<Mission41>(); }

int Mission41::BuildEventKinds() {
    int result = 1;
    if (!g_mission41EventKindsReady) {
        g_mission41EventKindsReady = 1;
        result = MissionTypeBase::InitEventKinds(0, &g_mission41EventKinds);
    }
    return result;
}


EventKindSet g_mission41EventKinds{};
int g_mission41EventKindsReady{};
