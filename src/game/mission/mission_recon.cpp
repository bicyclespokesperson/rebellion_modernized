#include "mission/mission_recon.h"

extern int g_missionReconEventKindsReady; // 0x6bb970
extern EventKindSet g_missionReconEventKinds; // 0x6bb978

namespace
{
constexpr uint32_t kFamilyMissionRecon = 0x54;
constexpr int kReconSuccessChance = 100;
constexpr int kTaskStarted = 2;
constexpr int kTaskSucceeded = 3;
constexpr int kSystemViewEventMode = 8;
} // namespace

MissionRecon::MissionRecon() = default;

MissionRecon::~MissionRecon() = default;

REB_MISSION_SHARED_PERSISTENCE(MissionRecon)

uint32_t MissionRecon::GetFamily()
{
  return kFamilyMissionRecon;
}

int MissionRecon::GetEventKinds(int unused, EventKindSet* out)
{
  return CopyEventKinds(BuildEventKinds, &g_missionReconEventKinds, out);
}

int MissionRecon::CheckOrder(int a, int orderKind, StatusCode* out)
{
  return CheckOrderAgainstKinds(BuildEventKinds, &g_missionReconEventKinds, orderKind, out);
}

int MissionRecon::GetCheckFunctions(MissionCheckFunctions* out)
{
  out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
  out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
  return 1;
}

GameObj* MissionRecon::CreateViewObject()
{
  return NewOrNull<MissionRecon>();
}

GameObjNotifier* MissionRecon::Create()
{
  return NewOrNull<MissionRecon>();
}

int MissionRecon::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out)
{
  *status = MakeStatus(mission_status::kOk);
  *out = 0;
  return MissionSystemTargetBase::CheckTarget(check, status, out);
}

int MissionRecon::Virtual157(GameObj* person, int* out)
{
  *out = kReconSuccessChance;
  return 1;
}

int MissionRecon::Virtual158(NotifyContext* ctx)
{
  return 1;
}

int MissionRecon::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx)
{
  *out = 1;
  return SetTaskStatus(kTaskSucceeded, ctx);
}

int MissionRecon::Virtual160(NotifyContext* ctx)
{
  int ok = 1;
  if (m_taskStatus == 0)
    ok = SetTaskStatus(kTaskStarted, ctx) != 0;
  if (m_taskStatus == kTaskSucceeded)
  {
    GameObj* leader = nullptr;
    ok = GetTargetLocationSystem(&leader) && ok;
    System* system = nullptr;
    if (leader)
    {
      const int isSystem = FamilyInRange(leader->GetFamily(), family::kSystems);
      ok = isSystem && ok;
      if (isSystem)
        system = static_cast<System*>(leader);
    }
    if (system)
      ok = system->SyncViewForEvent(kSystemViewEventMode, ControlSide(), ctx) && ok;
  }
  return ok;
}

int MissionRecon::BuildEventKinds()
{
  if (!g_missionReconEventKindsReady)
  {
    g_missionReconEventKindsReady = 1;
    return MissionSystemTargetBase::InitEventKinds(0, &g_missionReconEventKinds);
  }
  return 1;
}
EventKindSet g_missionReconEventKinds{};
int g_missionReconEventKindsReady{};
