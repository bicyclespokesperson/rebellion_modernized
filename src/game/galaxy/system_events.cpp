#include "character/role.h"
#include "events/event_post.h"
#include "galaxy/system.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_rules_fun_decl.h"
#include "gameobj/game_obj_data.h"

extern int g_setupComplete; // 0x6b90e0
extern int g_systemBlockadeStatLevelFleetSide; // 0x6bb3d8
extern int g_systemBlockadeStatLevelOther; // 0x6bb3fc

// OnXChanged: events on the master view. Most of them recompute the derived values that depend on the property.

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kAllSides = 3;
constexpr uint32_t kDestroyedState = 0x8;
constexpr uint32_t kEnrouteState = 0x10;
constexpr uint32_t kDisasterDestroyedReason = 0xb;
constexpr uint32_t kExistingChildren = 1;
constexpr uint32_t kUnitStateFilter = 3;

constexpr FamilyRange kFleets(0x08, 0x10);
constexpr FamilyRange kCapShips(0x14, 0x1c);
constexpr FamilyRange kDisasterVictimsA(0x28, 0x30);
constexpr FamilyRange kDisasterVictimsB(0x22, 0x28);

SystemData* dataOf(System* system)
{
  return system->GetData();
}

void initAllViews(NotifyContext& local)
{
  local.m_flag04 = 1;
  local.m_flag08 = kAllSides;
}

void applyToChildViews(GameObjNotifier* child, NotifyContext* ctx)
{
  auto* object = static_cast<GameObj*>(child);
  object->ApplyAllToView(kAllianceView, ctx);
  object->ApplyAllToView(kEmpireView, ctx);
}

int maybeDestroyChildren(const FamilyRange* range, System* system, NotifyContext* ctx, int result)
{
  ChildIterator it(system, range, kUnitStateFilter);
  for (it.Begin(); it.Current(); it.Advance())
  {
    auto* child = static_cast<GameObj*>(it.Current());
    if ((child->m_stateFlags & kEnrouteState) == 0)
    {
      int roll = 0;
      result = RollSupportChance(&roll) && result;
      if (roll != 0)
        result = child->SetDestroyedReason(kDisasterDestroyedReason, ctx) && result;
    }
  }
  return result;
}

} // namespace

int System::OnNameChanged(std::string* oldName, std::string* newName, NotifyContext* ctx)
{
  return GameObj::OnNameChanged(oldName, newName, ctx);
}

int System::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx)
{
  int ok = GameObj::OnControlKindChanged(oldKind, newKind, ctx) != 0;
  if (g_setupComplete != 0)
  {
    ok = this->System::RecomputeObservation(ctx) && ok;
    ok = this->System::RecomputeNeverBeenControlled(ctx) && ok;
    ok = this->System::RecomputeFastRepair(ctx) && ok;
    ok = SetUprising(0, ctx) && ok;
    ok = SetSmugglingPercent(0, ctx) && ok;
    ok = this->System::RecomputeSmugglingPercent(ctx) && ok;
    ok = this->System::RecomputeTroopRegRequired(ctx) && ok;
    ok = this->System::CheckHyperspaceBlocked(oldKind, newKind, ctx) && ok;
    ok = SetBlockadeAndBattlePendingManagementRequired(1, ctx) && ok;
  }
  return ok;
}

int System::OnDestroyedChanged(int newValue, NotifyContext* ctx)
{
  const int base = GameObj::OnDestroyedChanged(newValue, ctx);
  const int hub = PostSourceEventFlag(0x319, this, newValue, ctx);
  return hub && base ? 1 : 0;
}

int System::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  int ok = GameObj::OnExistingChanged(newValue, ctx) != 0;
  if (g_setupComplete != 0)
    ok = this->System::RecomputePopulated(ctx) && ok;
  return ok;
}

int System::OnLoyaltyChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    const int first = this->System::RecomputeControlData(ctx);
    const int second = this->System::RecomputeTroopRegRequired(ctx);
    int ok = second && first;
    ok = this->System::RecomputeSmugglingPercent(ctx) && ok;
    result = this->System::RecomputeProductionModifier(ctx) && ok;
    if (m_flags & system_flag::kBlockade)
    {
      uint32_t fleetSide;
      ok = IsBlockaderMoreLoyal(&fleetSide) && result;
      const int level = fleetSide != 0 ? g_systemBlockadeStatLevelFleetSide : g_systemBlockadeStatLevelOther;
      const int hub = ArmTimerAfterSetLevel(0x386, level, this, &dataOf(this)->m_stats[3], ctx);
      return hub && ok ? 1 : 0;
    }
  }
  return result;
}

int System::OnEnergyChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  const int first = this->System::RecomputeRawMaterial(ctx);
  const int second = this->System::RecomputeEnergyAllocated(ctx);
  return second && first ? 1 : 0;
}

int System::OnRawMaterialChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  const int first = this->System::RecomputeRawMaterial(ctx);
  const int second = this->System::RecomputeRawMaterialAllocated(ctx);
  return second && first ? 1 : 0;
}

int System::OnProductionModifierChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return this->System::RecomputeProductionModifiers(ctx) != 0;
}

int System::OnTroopRegWithdrawPercentChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
    ok = this->System::RecomputeTroopWithdrawPercents(ctx) != 0;
  return ok;
}

int System::OnControlKindBattleWonChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  NotifyContext allViews(*ctx);
  initAllViews(allViews);
  ViewAsSystem(kAllianceView)->ApplyControlKindBattleWon(&allViews);
  ViewAsSystem(kEmpireView)->ApplyControlKindBattleWon(&allViews);
  return 1;
}

int System::OnControlKindBattleWithdrewChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  NotifyContext allViews(*ctx);
  initAllViews(allViews);
  ViewAsSystem(kAllianceView)->ApplyControlKindBattleWithdrew(&allViews);
  ViewAsSystem(kEmpireView)->ApplyControlKindBattleWithdrew(&allViews);
  return 1;
}

int System::OnTroopRegSurplusChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    const int first = this->System::RecomputeUprising(ctx);
    const int second = this->System::RecomputeSuppressing(ctx);
    result = second && first ? 1 : 0;
  }
  return result;
}

int System::OnTroopRegRequiredChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
    ok = this->System::RecomputeTroopRegSurplus(ctx) != 0;
  return ok;
}

int System::OnControlDataChanged(const SideBits* oldValue, const SideBits* newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    if (((oldValue->m_value ^ newValue->m_value) & 3) != 0)
    {
      const int noLoyaltyOrGarrison = (oldValue->m_value & 0x30) == 0 && (newValue->m_value & 0x30) == 0;
      const int causedByLoyalty = SetLoyaltyCausedCurrentControlKind(noLoyaltyOrGarrison, ctx);
      const int kindSet = SetControlKind(newValue->m_value & 3, ctx);
      result = kindSet && causedByLoyalty ? 1 : 0;
    }
    const int hub = PostSourceEvent(0x31b, this, oldValue->m_value, newValue->m_value, ctx);
    if (hub && result)
      return 1;
    result = 0;
  }
  return result;
}

int System::OnPopulatedChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
  {
    if (newValue != 0)
    {
      int loyalty = 0x32;
      int side = 1;
      if (ControlSide() == 1)
      {
        loyalty = 100;
      }
      else if (ControlSide() == 2)
      {
        loyalty = 100;
        side = 2;
      }
      ok = SetLoyaltyFor(loyalty, side, ctx) != 0;
    }
    ok = this->System::RecomputeControlData(ctx) && ok;
    ok = this->System::RecomputeTroopRegRequired(ctx) && ok;
    ok = SetBlockadeAndBattlePendingManagementRequired(1, ctx) && ok;
    ok = this->System::RecomputeNeverBeenControlled(ctx) && ok;
  }
  return ok;
}

int System::OnUprisingChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    const int first = this->System::RecomputeFacilityBattleFlags(ctx);
    const int second = this->System::RefreshSiteKind0(ctx);
    int ok = second && first;
    ok = this->System::RefreshSiteKind2(ctx) && ok;
    ok = this->System::RefreshSiteKind1(ctx) && ok;
    ok = this->System::RecomputeTroopRegRequired(ctx) && ok;
    ok = this->System::RecomputeGarrisoned(ctx) && ok;
    ok = this->System::RecomputeControlKindUprising(ctx) && ok;
    ok = this->System::RecomputeSuppressing(ctx) && ok;
    ok = PostSourceEventFlag(0x303, this, newValue, ctx) && ok;
    ok = ArmTimerAfterSetFlag(900, newValue, this, &dataOf(this)->m_stats[1], ctx) && ok;
    ok = ArmTimerAfterSetFlag(0x38d, newValue, this, &dataOf(this)->m_stats[4], ctx) && ok;
    result = ok;
  }
  return result;
}

int System::OnBattleChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    if (newValue != 0)
    {
      const int first = this->System::SetData68(0, 1);
      const int second = this->System::SetData68(0, 2);
      int ok = second && first;
      result = this->System::SetData68(0, 3) && ok;
    }
    int ok = this->System::RecomputeFleetBattleFlags(ctx) && result;
    result = PostSourceEventFlag(0x309, this, newValue, ctx) && ok;

    NotifyContext allViews(*ctx);
    initAllViews(allViews);
    ViewAsSystem(kAllianceView)->ApplyBattle(&allViews);
    ViewAsSystem(kEmpireView)->ApplyBattle(&allViews);
    if (newValue != 0)
    {
      ChildIterator fleets(this, &kFleets, kExistingChildren);
      for (fleets.Begin(); fleets.Current(); fleets.Advance())
      {
        ChildIterator capShips(fleets.Current(), &kCapShips, kExistingChildren);
        for (capShips.Begin(); capShips.Current(); capShips.Advance())
        {
          RoleIterator roles(capShips.Current(), kExistingChildren);
          for (roles.Begin(); roles.Current(); roles.Advance())
            applyToChildViews(roles.Current(), &allViews);
        }
      }
    }
  }
  return result;
}

int System::OnBlockadeChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (g_setupComplete != 0)
  {
    const int first = this->System::RecomputeFleetBlockadeFlags(ctx);
    const int second = this->System::RecomputeFacilityBattleFlags(ctx);
    int ok = second && first;
    ok = this->System::RefreshSiteKind0(ctx) && ok;
    ok = this->System::RefreshSiteKind2(ctx) && ok;
    ok = this->System::RefreshSiteKind1(ctx) && ok;
    ok = this->System::RecomputeTroopRegWithdrawPercent(ctx) && ok;
    if (ok && newValue != 0)
    {
      const int firstSide = this->System::SetData48(0, 1);
      const int secondSide = this->System::SetData48(0, 2);
      ok = secondSide && firstSide;
      ok = this->System::SetData48(0, 3) && ok;
      ok = this->System::SetData54(0, 1) && ok;
      ok = this->System::SetData54(0, 2) && ok;
      ok = this->System::SetData54(0, 3) && ok;
      if (ok && (m_flags & system_flag::kBlockade))
      {
        uint32_t fleetSide;
        const int found = GetBlockadingSide(&fleetSide);
        const int adjusted = this->System::SetData60(fleetSide);
        ok = adjusted && found;
      }
    }
    ok = PostSourceEventFlag(0x305, this, newValue, ctx) && ok;
    ok = ArmTimerAfterSetFlag(0x386, newValue, this, &dataOf(this)->m_stats[3], ctx) && ok;
    result = ok;
  }
  return result;
}

int System::OnBombardChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if ((m_stateFlags & kDestroyedState) != 0 && newValue == 0)
  {
    NotifyContext allViews(*ctx);
    initAllViews(allViews);
    const int first = ViewAsSystem(kAllianceView)->ApplyBombard(&allViews);
    const int second = ViewAsSystem(kEmpireView)->ApplyBombard(&allViews);
    result = second && first ? 1 : 0;
  }
  if (g_setupComplete != 0)
    result = PostSourceEventFlag(0x310, this, newValue, ctx) && result;
  return result;
}

int System::OnAssaultChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
    ok = PostSourceEventFlag(0x30f, this, newValue, ctx) != 0;
  return ok;
}

int System::OnGarrisonedChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0 && newValue != 0 && (m_flags & system_flag::kPopulated))
  {
    int loyaltySide = 0;
    int sideKnown = 0;
    if (ControlSide() == 1)
    {
      loyaltySide = 1;
      sideKnown = 1;
    }
    else if (ControlSide() == 2)
    {
      loyaltySide = 2;
      sideKnown = 1;
    }
    const int loyalty = GetLoyaltyFor(loyaltySide);
    const uint32_t side = ControlSide();
    const int strength = ThresholdBonus(loyalty);
    ok = this->System::AdjustLoyalty(strength, side, ctx) && sideKnown;
  }
  const int hub = ArmTimerAfterSetFlag(0x385, newValue, this, &dataOf(this)->m_stats[2], ctx);
  return hub && ok ? 1 : 0;
}

int System::OnSuppressingChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
    ok = ArmTimerAfterSetFlag(899, newValue, this, &dataOf(this)->m_stats[0], ctx) != 0;
  return ok;
}

int System::OnCombatUnitFastRepairChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (g_setupComplete != 0)
    ok = this->System::RecomputeFastRepair(ctx) != 0;
  return ok;
}

int System::OnDeathStarNearbyChanged(int newValue, NotifyContext* ctx)
{
  const int first = this->System::RecomputeTroopRegRequired(ctx);
  const int second = this->System::RecomputeSmugglingPercent(ctx);
  return second && first ? 1 : 0;
}

int System::OnBattlePendingChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (newValue != 0)
  {
    const int first = this->System::SetDataFlag8(newValue);
    const int second = this->System::SetData64(0);
    ok = second && first;
    ok = this->System::SetDataFlag4(0) && ok;
    ok = SetControlKindBattleWon(0, ctx) && ok;
    ok = SetControlKindBattleWithdrew(0, ctx) && ok;
  }
  const int hub = PostSourceEventFlag(0x304, this, newValue, ctx);
  return hub && ok ? 1 : 0;
}

int System::OnUprisingIncidentChanged(int newValue, NotifyContext* ctx)
{
  int ok = 1;
  if (newValue != 0)
    ok = this->System::ResolveUprisingTurn(ctx) != 0;
  return ok;
}

int System::OnInformantIncidentChanged(int newValue, NotifyContext* ctx)
{
  NotifyContext allViews(*ctx);
  initAllViews(allViews);
  const int first = ViewAsSystem(kAllianceView)->ApplyInformantIncident(&allViews);
  const int second = ViewAsSystem(kEmpireView)->ApplyInformantIncident(&allViews);
  int result = second && first ? 1 : 0;
  if (newValue != 0)
    result = this->System::RollAndSyncEvent(ctx) && result;
  return result;
}

int System::OnDisasterIncidentChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (newValue != 0)
  {
    int rawMaterial = m_rawMaterial;
    int energy = m_energy;
    int ok = ResolveMutualLosses(&rawMaterial, &energy);
    ok = SetEnergy(energy, ctx) && ok;
    result = SetRawMaterial(rawMaterial, ctx) && ok;
    result = maybeDestroyChildren(&kDisasterVictimsA, this, ctx, result);
    result = maybeDestroyChildren(&kDisasterVictimsB, this, ctx, result);
  }
  return result;
}

int System::OnBlockadeAndBattlePendingManagementRequiredChanged(int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (newValue != 0)
  {
    const int hub = PostSourceEventFlag(0x311, this, 1, ctx);
    const int cleared = SetBlockadeAndBattlePendingManagementRequired(0, ctx);
    result = cleared && hub ? 1 : 0;
  }
  return result;
}
