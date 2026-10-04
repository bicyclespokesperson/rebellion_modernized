#include "mission/mission_adrift.h"

extern "C" void __cdecl LAB_00576b20();

extern int g_missionAdriftEventKindsReady;      // 0x6bbb18
extern EventKindSet g_missionAdriftEventKinds;  // 0x6bbb10

namespace {
constexpr uint32_t kFamilyMissionAdrift = 0x44;
constexpr uint32_t kInitialPhaseLevels = 0x4103;
}

MissionAdrift::MissionAdrift() {}

MissionAdrift::~MissionAdrift() { REB_KEEP_VPTR_STORE(); }

uint32_t MissionAdrift::GetFamily() { return kFamilyMissionAdrift; }

int MissionAdrift::Read(istream* in) { return FUN_00592c50(this, in); }

int MissionAdrift::Write(ostream* out) { return FUN_0056ec40(this, out); }

int MissionAdrift::Checksum(Crc32* crc) { return FUN_00593ad0(this, crc); }

int MissionAdrift::Validate() { return FUN_00523c60(this); }

int MissionAdrift::ApplyAll(NotifyContext* ctx) { return IsSideView() ? FUN_0056ec50(this, ctx) : 0; }

int MissionAdrift::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int MissionAdrift::CreateViewsImpl(int controlKind, NotifyContext* ctx) {
    const int created = FUN_005236e0(this, controlKind, ctx);
    MissionPhaseLevels levels;
    levels.m_levels = kInitialPhaseLevels;
    AssignPhaseLevels(reinterpret_cast<IntPair*>(&levels));
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

void RegisterMissionAdriftEventKindsAtExit() { atexit(LAB_00576b20); }

void MissionAdrift::InitStatics() {
    ConstructMissionAdriftEventKinds();
    RegisterMissionAdriftEventKindsAtExit();
}
