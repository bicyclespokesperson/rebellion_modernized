#include <new>

#include "character/character_data.h"
#include "character/role.h"
#include "data/probability_table.h"
#include "gameobj/object_family.h"

extern EventKindSet g_roleEventKinds; // 0x6b9028
extern int g_roleEventKindsBuilt; // 0x6b9030, set by the first BuildEventKinds

namespace
{

constexpr FamilyRange kRoleFamilies(0x30, 0x40);
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kStatusOnMission = 0x30;
constexpr int kStatusBusy = 1;
constexpr int kEventKindsCount = 6;
constexpr int kRoleEventKinds[kEventKindsCount] = {0x201, 0x202, 0x242, 0x240, 0x204, 0x241};
constexpr int kDefaultEventKindValue = 1;
constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kLockedState = 0x4000;

void setStatus(StatusCode* out, int code)
{
  *out = StatusCode(kStatusCategory, code);
}

} // namespace

// ---- construction -----------------------------------------------------------------------------------------

Role::Role()
: m_baseDiplomacy(0),
  m_baseEspionage(0),
  m_baseShipyardRD(0),
  m_baseTrainingFacilRD(0),
  m_baseConstructionYardRD(0),
  m_baseCombat(0),
  m_baseLeadership(0),
  m_baseLoyalty(0),
  m_flags78(0)
{
  m_missionSeedKey.m_value = 0;
}

Role::~Role() = default;

GameObjData* Role::CreateData()
{
  void* memory = operator new(sizeof(RoleData));
  if (!memory)
    return nullptr;
  return new (memory) RoleData();
}

// ---- event kinds ---------------------------------------------------------------------------------------------

int Role::BuildEventKinds()
{
  if (g_roleEventKindsBuilt)
    return 1;
  g_roleEventKindsBuilt = 1;
  int result = UnitObjBase::InitEventKinds(0, &g_roleEventKinds);
  for (int index = 0; result && index < kEventKindsCount; ++index)
  {
    result = g_roleEventKinds.Add(kRoleEventKinds[index], kDefaultEventKindValue) != nullptr;
  }
  return result;
}

int Role::InitEventKinds(int unused, EventKindSet* out)
{
  const int result = BuildEventKinds();
  *out = g_roleEventKinds;
  return result;
}

int Role::GetEventKinds(int unused, EventKindSet* out)
{
  const int result = BuildEventKinds();
  if (result)
    *out = g_roleEventKinds;
  return result;
}

// ---- queries and checks ------------------------------------------------------------------------------------

int Role::GetTransitSpeed(int arg)
{
  return GameObj::GetTransitSpeed(1);
}

int Role::GetEffectiveRdForKind(int kind, int* out)
{
  *out = 0;
  switch (kind)
  {
    case 0:
      *out = GetShipyardRD();
      return 1;
    case 1:
      *out = GetConstructionYardRD();
      return 1;
    case 2:
      *out = GetTrainingFacilRD();
      return 1;
    default:
      return 0;
  }
}

int Role::GetBaseRdForKind(int kind, int* out)
{
  *out = 0;
  switch (kind)
  {
    case 0:
      *out = m_baseShipyardRD;
      return 1;
    case 1:
      *out = m_baseConstructionYardRD;
      return 1;
    case 2:
      *out = m_baseTrainingFacilRD;
      return 1;
    default:
      return 0;
  }
}

int Role::SetBaseRdForKind(int kind, int value, NotifyContext* ctx)
{
  switch (kind)
  {
    case 0:
      return SetBaseShipyardRD(value, ctx);
    case 1:
      return SetBaseConstructionYardRD(value, ctx);
    case 2:
      return SetBaseTrainingFacilRD(value, ctx);
    default:
      return 0;
  }
}

int Role::CheckOrder(int a, int orderKind, StatusCode* out)
{
  setStatus(out, kStatusNotSupported);
  const int result = BuildEventKinds();
  if (result && g_roleEventKinds.Contains(orderKind))
    setStatus(out, kStatusOk);
  return result;
}

int Role::CheckRequestImpl(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusNotSupported);
  return 1;
}

int Role::CheckRenameImpl(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusNotSupported);
  return 1;
}

int Role::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusOk);
  return 1;
}

int Role::CheckUnknown(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusOk);
  int result = CheckUnknownImpl(request, out);
  if (result)
    result = CheckActivityRequest(request, out);
  if (!result)
    setStatus(out, kStatusNoSuchOrder);
  return result;
}

int Role::CheckActivity(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusOk);
  const int result = CheckActivityRequest(request, out);
  if (result && out->m_second == static_cast<uint32_t>(kStatusOk) && HasMissionFlag(role_flag::kOnMission))
  {
    *out = StatusCode(kStatusOnMission, kStatusBusy);
  }
  return result;
}

int Role::CheckActivityStatus(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusOk);
  return CheckActivity(request, out);
}

int Role::CheckActivity2(const ActivityRequest* request, StatusCode* out)
{
  setStatus(out, kStatusOk);
  const int result = CheckActivityRequest(request, out);
  if (result && out->m_second == static_cast<uint32_t>(kStatusOk) && HasMissionFlag(role_flag::kOnMission))
  {
    *out = StatusCode(kStatusOnMission, kStatusBusy);
  }
  return result;
}

int Role::CheckActivityStatus2(const ActivityRequest* request, int unused, StatusCode* out)
{
  setStatus(out, kStatusOk);
  return CheckActivity2(request, out);
}

// ---- mission handling ------------------------------------------------------------------------------------------

int Role::Virtual47(intptr_t a)
{
  return UnitObjBase::Virtual47(a);
}

int Role::Virtual48(intptr_t a)
{
  return UnitObjBase::Virtual47(a);
}

int Role::Virtual55(intptr_t a)
{
  NotifyContext* ctx = reinterpret_cast<NotifyContext*>(a);
  int result = UnitObjBase::Virtual55(a) != 0;
  Role* master = ViewAsRole(0);
  ObjectKey missionKey;
  if (master->GetMissionKey(&missionKey)->IsSet())
  {
    ObjectKey cleared;
    result = master->SetMissionKey(&cleared, ctx);
  }
  NotifyContext applyContext(*ctx);
  applyContext.SetViewFlag(1, 1);
  applyContext.SetViewFlag(2, 1);
  applyContext.m_flag04 = 1;
  applyContext.m_flag08 = 3;
  ApplyMissionKey(&applyContext);
  return result;
}

int Role::UpdateOnMission(NotifyContext* ctx)
{
  const int onMission = m_missionKey.IsSet() || HasMissionFlag(role_flag::kMovingBetweenMissions);
  return SetOnMission(onMission ? 1 : 0, ctx);
}

int Role::RollCombatCheck(const ObjectKey* opponent, int difficulty, NotifyContext* ctx)
{
  int succeeded = 0;
  const int combat = GetCombat();
  if (!RollRlevadChance(static_cast<int>(ControlSide()), combat, difficulty, &succeeded))
    return 0;
  if (succeeded)
    return Virtual130(ctx);
  return Virtual131(opponent, ctx);
}

int Role::Virtual128(const ActivityRequest* request, NotifyContext* ctx)
{
  StatusCode status(kStatusCategory, kStatusOk);
  int result = CheckActivityStatus(request, &status);
  if (result && status.m_second == static_cast<uint32_t>(kStatusOk))
    result = Virtual129(ctx);
  return result;
}

int Role::Virtual129(NotifyContext* ctx)
{
  return GameObj::Destroy(ctx);
}

int Role::Virtual130(NotifyContext* ctx)
{
  int result = 1;
  if (HasMissionFlag(role_flag::kCanResignFromMission) && m_missionKey.IsSet())
  {
    result = SetMissionResignRequest(1, ctx);
  }
  return result;
}

ObjectKey* Role::GetParentAtMissionCompletionKey(ObjectKey* out)
{
  out->m_value = m_parentAtMissionCompletionKey.m_value;
  return out;
}

// ---- persistence and validation -----------------------------------------------------------------------------

int Role::Read(istream* in)
{
  const int result = GameObj::Read(in);
  if (result)
  {
    ReadValue16(in, &m_baseDiplomacy);
    ReadValue16(in, &m_baseEspionage);
    ReadValue16(in, &m_baseShipyardRD);
    ReadValue16(in, &m_baseTrainingFacilRD);
    ReadValue16(in, &m_baseConstructionYardRD);
    ReadValue16(in, &m_baseCombat);
    ReadValue16(in, &m_baseLeadership);
    ReadValue16(in, &m_baseLoyalty);
    m_missionKey.Read(in);
    MissionSeedKeyAsObjectKey()->Read(in);
    m_parentAtMissionCompletionKey.Read(in);
    m_locationAtMissionCompletionKey.Read(in);
    ReadValue32(in, &m_flags78);
  }
  return result;
}

int Role::Write(ostream* out)
{
  const int result = GameObj::Write(out);
  if (result)
  {
    WriteValue16(out, &m_baseDiplomacy);
    WriteValue16(out, &m_baseEspionage);
    WriteValue16(out, &m_baseShipyardRD);
    WriteValue16(out, &m_baseTrainingFacilRD);
    WriteValue16(out, &m_baseConstructionYardRD);
    WriteValue16(out, &m_baseCombat);
    WriteValue16(out, &m_baseLeadership);
    WriteValue16(out, &m_baseLoyalty);
    m_missionKey.Write(out);
    MissionSeedKeyAsObjectKey()->Write(out);
    m_parentAtMissionCompletionKey.Write(out);
    m_locationAtMissionCompletionKey.Write(out);
    WriteValue32(out, &m_flags78);
  }
  return result;
}

int Role::ApplyAll(NotifyContext* ctx)
{
  const int result = ApplyAllToSideView(ctx);
  if (result && ctx->m_flag04 == 1)
  {
    ApplyBaseDiplomacy(ctx);
    ApplyBaseEspionage(ctx);
    ApplyBaseShipyardRD(ctx);
    ApplyBaseTrainingFacilRD(ctx);
    ApplyBaseConstructionYardRD(ctx);
    ApplyBaseCombat(ctx);
    ApplyBaseLeadership(ctx);
    ApplyBaseLoyalty(ctx);
    ApplyMissionKey(ctx);
    ApplyMissionSeedKey(ctx);
    ApplyParentAtMissionCompletionKey(ctx);
    ApplyLocationAtMissionCompletionKey(ctx);
    ApplyDecoy(ctx);
    ApplyMovingBetweenMissions(ctx);
    ApplyMissionRemoveRequest(ctx);
    ApplyMissionResignRequest(ctx);
    ApplyCanResignFromMission(ctx);
    ApplyIsDecoying(ctx);
    ApplyAdrift(ctx);
    ApplyOnMission(ctx);
    ApplyOnHiddenMission(ctx);
    ApplyOnMandatoryMission(ctx);
  }
  return result;
}

int Role::Checksum(Crc32* crc)
{
  int result = GameObj::Checksum(crc);
  if (result)
  {
    crc->AddShort(m_baseDiplomacy);
    crc->AddShort(m_baseEspionage);
    crc->AddShort(m_baseShipyardRD);
    crc->AddShort(m_baseTrainingFacilRD);
    crc->AddShort(m_baseConstructionYardRD);
    crc->AddShort(m_baseCombat);
    crc->AddShort(m_baseLeadership);
    crc->AddShort(m_baseLoyalty);
    result = m_missionKey.Checksum(crc);
    if (result)
      result = MissionSeedKeyAsObjectKey()->Checksum(crc);
    if (result)
      result = m_parentAtMissionCompletionKey.Checksum(crc);
    if (result)
      result = m_locationAtMissionCompletionKey.Checksum(crc);
    if (result)
      crc->AddInt(static_cast<int>(m_flags78));
  }
  return result;
}

int Role::Validate()
{
  int notAdrift = 0;
  if (GameObj::Validate())
    notAdrift = !HasMissionFlag(role_flag::kAdrift);
  GameObjNotifier* parent = nullptr;
  int result = 0;
  if (notAdrift)
  {
    result = GetParent(&parent);
    const bool parentStateApplies =
      !HasMissionFlag(role_flag::kOnMission) && !((m_stateFlags & kDestroyedState) && (m_stateFlags & kLockedState));
    if (result && parentStateApplies)
      result = !(static_cast<GameObj*>(parent)->m_stateFlags & kDestroyedState);
  }
  return result;
}

// ---- RoleIterator ----------------------------------------------------------------------------------------------

RoleIterator::RoleIterator(GameObjNotifier* parent, uint32_t stateFilter)
: ChildIterator(parent, &kRoleFamilies, stateFilter)
{
}

RoleIterator::RoleIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
: ChildIterator(parent, side, &kRoleFamilies, stateFilter)
{
}

RoleIterator::~RoleIterator() = default;
EventKindSet g_roleEventKinds{};
int g_roleEventKindsBuilt{};
