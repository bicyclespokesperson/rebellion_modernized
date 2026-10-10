#include "mission/mission_adrift.h"

void DestroyMissionAdriftEventKinds();

extern int g_missionAdriftEventKindsReady;      // 0x6bbb18
extern EventKindSet g_missionAdriftEventKinds;  // 0x6bbb10

namespace {
constexpr uint32_t kFamilyMissionAdrift = 0x44;
constexpr uint32_t kInitialPhaseLevels = 0x4103;
}

MissionAdrift::MissionAdrift() {}

MissionAdrift::~MissionAdrift() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionAdrift::GetFamily() { return kFamilyMissionAdrift; }

int MissionAdrift::Read(istream* in) { return reinterpret_cast<MissionRuleBase*>(this)->MissionRuleBase::ReadBody(in); }

int MissionAdrift::Write(ostream* out) { return MissionTypeWriteForward(this, out); }

int MissionAdrift::Checksum(Crc32* crc) { return this->MissionTypeBase::ChecksumBody(crc); }

int MissionAdrift::Validate() { return static_cast<Mission*>(this)->Mission::ValidateImpl(); }

int MissionAdrift::ApplyAll(NotifyContext* ctx) { return IsSideView() ? MissionTypeApplyAllForward(this, ctx) : 0; }

int MissionAdrift::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int MissionAdrift::CreateViewsImpl(int controlKind, NotifyContext* ctx) {
    const int created = this->Mission::CreateViewsImpl(controlKind, ctx);
    MissionPhaseLevels levels;
    levels.m_levels = kInitialPhaseLevels;
    AssignPhaseLevels(&levels);
    return created != 0;
}

int MissionAdrift::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionAdriftEventKinds, out);
}

int MissionAdrift::CheckOrder(int a, int orderKind, StatusCode* out) {
    return CheckOrderAgainstKinds(BuildEventKinds, &g_missionAdriftEventKinds, orderKind, out);
}

GameObj* MissionAdrift::CreateViewObject() { return NewOrNull<MissionAdrift>(); }

GameObjNotifier* MissionAdrift::Create() { return NewOrNull<MissionAdrift>(); }

int MissionAdrift::BuildEventKinds() {
    int result = 1;
    if (!g_missionAdriftEventKindsReady) {
        g_missionAdriftEventKindsReady = 1;
        result = MissionTypeBase::InitEventKinds(0, &g_missionAdriftEventKinds);
    }
    return result;
}

void ConstructMissionAdriftEventKinds() { new (&g_missionAdriftEventKinds) EventKindSet(); }

void RegisterMissionAdriftEventKindsAtExit() { atexit(DestroyMissionAdriftEventKinds); }

void MissionAdrift::InitStatics() {
    ConstructMissionAdriftEventKinds();
    RegisterMissionAdriftEventKindsAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_missionAdriftEventKindsDestroyedGuard;  // 0x6bbb1c

void DestroyMissionAdriftEventKinds() {
    if (!(g_missionAdriftEventKindsDestroyedGuard & 0x1)) {
        g_missionAdriftEventKindsDestroyedGuard |= 0x1;
        g_missionAdriftEventKinds.~EventKindSet();
    }
}
