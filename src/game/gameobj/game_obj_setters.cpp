#include "core/game_string.h"
#include "events/event_post.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_database.h"

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr uint32_t kUsable = 0x1;
constexpr uint32_t kCreated = 0x2;
constexpr uint32_t kCompleted = 0x4;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kEnroute = 0x10;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kExisting = 0x40;
constexpr uint32_t kObservedByAlliance = 0x80;
constexpr uint32_t kObservedByEmpire = 0x100;
constexpr uint32_t kDamaged = 0x200;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr uint32_t kAutorouting = 0x1000;
constexpr uint32_t kAutoscrapRequest = 0x2000;
constexpr uint32_t kLocked = 0x4000;
constexpr uint32_t kReadyForDelete = 0x8000;
constexpr uint32_t kConstructed = 0x10000;
constexpr uint32_t kDeployed = 0x20000;
constexpr uint32_t kReasonAutoscrap = 0x15;

} // namespace

// ---- property setters: master view only -----------------------------------------------------------------------

int GameObj::SetName(std::string* value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result) {
    if (CompareStrings(*GetName(), *value) != 0) {
      std::string oldName(*GetName());
      result = InstallName();
      if (result) {
        *m_name = *value;
        ViewAs(kAllianceView)->ApplyName(ctx);
        ViewAs(kEmpireView)->ApplyName(ctx);
        OnNameChanged(&oldName, m_name, ctx);
      }
    }
  }
  return result;
}

int GameObj::SetControlKind(uint32_t value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = static_cast<int>(value) >= 1 && static_cast<int>(value) <= 3;
  }
  if (valid) {
    const uint32_t oldKind = ControlSide();
    if (oldKind != value) {
      SetControlSide(value);
      ViewAs(kAllianceView)->ApplyControlKind(ctx);
      ViewAs(kEmpireView)->ApplyControlKind(ctx);
      OnControlKindChanged(oldKind, ControlSide(), ctx);
    }
  }
  return valid;
}

int GameObj::SetParent(GameObjNotifier* value, NotifyContext* ctx)
{
  int result = 0;
  if (value && IsMasterView() && value->IsMasterView()) {
    result = value->CanAdopt(this);
    GameObjNotifier* const oldParent = m_parent;
    if (result && value != oldParent) {
      result = static_cast<GameObj*>(value)->AdoptChild(this, ctx);
      if (result) {
        ViewAs(kAllianceView)->ApplyParent(ctx);
        ViewAs(kEmpireView)->ApplyParent(ctx);
        OnParentChanged(oldParent, value, ctx);
      }
    }
  }
  return result;
}

int GameObj::SetBuilderKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_builderKey.m_value != value->m_value) {
    ObjectKey oldKey(m_builderKey);
    m_builderKey = *value;
    ViewAs(kAllianceView)->ApplyBuilderKey(ctx);
    ViewAs(kEmpireView)->ApplyBuilderKey(ctx);
    OnBuilderKeyChanged(&oldKey, &m_builderKey, ctx);
  }
  return result;
}

int GameObj::SetDepartureKey(ObjectKey* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_departureKey.m_value != value->m_value) {
    ObjectKey oldKey(m_departureKey);
    m_departureKey = *value;
    ViewAs(kAllianceView)->ApplyDepartureKey(ctx);
    ViewAs(kEmpireView)->ApplyDepartureKey(ctx);
    OnDepartureKeyChanged(&oldKey, &m_departureKey, ctx);
  }
  return result;
}

int GameObj::SetDestroyedReason(uint32_t value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result) {
    result = IsInRange(value, 0, value, "Invalid DestroyedReason value!");
  }
  if (result) {
    const uint32_t oldPacked = m_packed;
    if ((oldPacked & 0xff) != value) {
      m_packed = ((value ^ oldPacked) & 0xff) ^ oldPacked;
      ViewAs(kAllianceView)->ApplyDestroyedReason(ctx);
      ViewAs(kEmpireView)->ApplyDestroyedReason(ctx);
      OnDestroyedReasonChanged(oldPacked & 0xff, m_packed & 0xff, ctx);
    }
  }
  return result;
}

int GameObj::SetDeploymentCount(uint32_t value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result) {
    result = IsInRange(value, 0, value, "capshp.cpp: Invalid DeploymentCount value!");
  }
  if (result) {
    const uint32_t oldPacked = m_packed;
    const uint32_t oldCount = (oldPacked >> 8) & 0xff;
    if (oldCount != value) {
      m_packed = (oldPacked & 0xffff00ff) | ((value & 0xff) << 8);
      ViewAs(kAllianceView)->ApplyDeploymentCount(ctx);
      ViewAs(kEmpireView)->ApplyDeploymentCount(ctx);
      OnDeploymentCountChanged(oldCount, (m_packed >> 8) & 0xff, ctx);
    }
  }
  return result;
}

int GameObj::SetDestinationCount(uint32_t value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result) {
    result = IsInRange(value, 0, value, "capshp.cpp: Invalid DestinationCount value!");
  }
  if (result) {
    const uint32_t oldCount = (m_packed >> 16) & 0xff;
    if (oldCount != value) {
      m_packed = ((value & 0xff) << 16) | (m_packed & 0xff00ffff);
      ViewAs(kAllianceView)->ApplyDestinationCount(ctx);
      ViewAs(kEmpireView)->ApplyDestinationCount(ctx);
      OnDestinationCountChanged(oldCount, (m_packed >> 16) & 0xff, ctx);
    }
  }
  return result;
}

int GameObj::SetEta(int value, NotifyContext* ctx)
{
  int result = IsMasterView();
  if (result) {
    result = IsInRange(value, 0, value, "Invalid ETA value!");
  }
  if (result && m_eta != value) {
    const int oldEta = m_eta;
    m_eta = value;
    ViewAs(kAllianceView)->ApplyEta(ctx);
    ViewAs(kEmpireView)->ApplyEta(ctx);
    OnEtaChanged(oldEta, m_eta, ctx);
  }
  return result;
}

// ---- state flag setters: master view only ---------------------------------------------------------------------

int GameObj::SetUsable(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kUsable, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyUsable(ctx);
    ViewAs(kEmpireView)->ApplyUsable(ctx);
    OnUsableChanged(on, ctx);
  }
  return result;
}

int GameObj::SetCreated(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kCreated, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyCreated(ctx);
    ViewAs(kEmpireView)->ApplyCreated(ctx);
    OnCreatedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetCompleted(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kCompleted, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyCompleted(ctx);
    ViewAs(kEmpireView)->ApplyCompleted(ctx);
    OnCompletedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetDestroyed(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kDestroyed, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyDestroyed(ctx);
    ViewAs(kEmpireView)->ApplyDestroyed(ctx);
    OnDestroyedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetEnroute(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kEnroute, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyEnroute(ctx);
    ViewAs(kEmpireView)->ApplyEnroute(ctx);
    OnEnrouteChanged(on, ctx);
  }
  return result;
}

int GameObj::SetEnrouteActive(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kEnrouteActive, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyEnrouteActive(ctx);
    ViewAs(kEmpireView)->ApplyEnrouteActive(ctx);
    OnEnrouteActiveChanged(on, ctx);
  }
  return result;
}

int GameObj::SetExisting(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kExisting, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyExisting(ctx);
    ViewAs(kEmpireView)->ApplyExisting(ctx);
    OnExistingChanged(on, ctx);
  }
  return result;
}

int GameObj::SetObservedByAlliance(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kObservedByAlliance, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyObservedByAlliance(ctx);
    ViewAs(kEmpireView)->ApplyObservedByAlliance(ctx);
    OnObservedByAllianceChanged(on, ctx);
  }
  return result;
}

int GameObj::SetObservedByEmpire(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kObservedByEmpire, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyObservedByEmpire(ctx);
    ViewAs(kEmpireView)->ApplyObservedByEmpire(ctx);
    OnObservedByEmpireChanged(on, ctx);
  }
  return result;
}

int GameObj::SetDamaged(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kDamaged, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyDamaged(ctx);
    ViewAs(kEmpireView)->ApplyDamaged(ctx);
    OnDamagedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetHyperdriveActive(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kHyperdriveActive, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyHyperdriveActive(ctx);
    ViewAs(kEmpireView)->ApplyHyperdriveActive(ctx);
    OnHyperdriveActiveChanged(on, ctx);
  }
  return result;
}

int GameObj::SetAutorouting(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kAutorouting, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyAutorouting(ctx);
    ViewAs(kEmpireView)->ApplyAutorouting(ctx);
    OnAutoroutingChanged(on, ctx);
  }
  return result;
}

int GameObj::SetAutoscrapRequest(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kAutoscrapRequest, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyAutoscrapRequest(ctx);
    ViewAs(kEmpireView)->ApplyAutoscrapRequest(ctx);
    OnAutoscrapRequestChanged(on, ctx);
  }
  return result;
}

int GameObj::SetLocked(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kLocked, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyLocked(ctx);
    ViewAs(kEmpireView)->ApplyLocked(ctx);
    OnLockedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetReadyForDelete(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kReadyForDelete, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyReadyForDelete(ctx);
    ViewAs(kEmpireView)->ApplyReadyForDelete(ctx);
    OnReadyForDeleteChanged(on, ctx);
  }
  return result;
}

int GameObj::SetConstructed(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kConstructed, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyConstructed(ctx);
    ViewAs(kEmpireView)->ApplyConstructed(ctx);
    OnConstructedChanged(on, ctx);
  }
  return result;
}

int GameObj::SetDeployed(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(kDeployed, on, &m_stateFlags)) {
    ViewAs(kAllianceView)->ApplyDeployed(ctx);
    ViewAs(kEmpireView)->ApplyDeployed(ctx);
    OnDeployedChanged(on, ctx);
  }
  return result;
}

// ---- derived state: slots 33-36 ---------------------------------------------------------------------------------

int GameObj::UpdateUsable(NotifyContext* ctx)
{
  const bool usable = (m_stateFlags & kExisting) && (m_stateFlags & kCompleted) && !(m_stateFlags & kEnroute);
  return SetUsable(usable ? 1 : 0, ctx);
}

int GameObj::UpdateExisting(NotifyContext* ctx)
{
  const bool existing = (m_stateFlags & kCreated) && !(m_stateFlags & kDestroyed);
  return SetExisting(existing ? 1 : 0, ctx);
}

int GameObj::UpdateObservedByAlliance(NotifyContext* ctx)
{
  int observed = 0;
  int ok = 1;
  if (IsNotGameRoot()) {
    GameObjNotifier* parent = nullptr;
    ok = GetParent(&parent) != 0;
    if (parent && !(m_stateFlags & kEnrouteActive) && IsObservable()) {
      observed = (static_cast<GameObj*>(parent)->m_stateFlags >> 7) & 1;
    }
  }
  return SetObservedByAlliance(observed, ctx) && ok ? 1 : 0;
}

int GameObj::UpdateObservedByEmpire(NotifyContext* ctx)
{
  int observed = 0;
  int ok = 1;
  if (IsNotGameRoot()) {
    GameObjNotifier* parent = nullptr;
    ok = GetParent(&parent) != 0;
    if (parent && !(m_stateFlags & kEnrouteActive) && IsObservable()) {
      observed = (static_cast<GameObj*>(parent)->m_stateFlags >> 8) & 1;
    }
  }
  return SetObservedByEmpire(observed, ctx) && ok ? 1 : 0;
}
