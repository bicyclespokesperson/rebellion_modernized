#include "data/game_math.h"
#include "events/event_post.h"
#include "galaxy/system.h"

// Validated setters of the System properties: master view only; each stores the value, has both side views copy it
// (ApplyX) and finally raises the virtual OnXChanged event.

namespace
{

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;

} // namespace

int System::SetLoyalty(int value, NotifyContext* ctx)
{
  int valid = IsMasterView();
  if (valid) {
    valid = (m_flags & system_flag::kPopulated) != 0;
  }
  if (valid) {
    valid = value >= 0 && value <= 100;
  }
  if (valid && m_loyalty != value) {
    const int oldValue = m_loyalty;
    m_loyalty = value;
    ViewAsSystem(kAllianceView)->ApplyLoyalty(ctx);
    ViewAsSystem(kEmpireView)->ApplyLoyalty(ctx);
    OnLoyaltyChanged(oldValue, m_loyalty, ctx);
  }
  return valid;
}

int System::SetEnergy(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, value, "Invalid SystemEnergy value!");
  }
  if (valid && m_energy != value) {
    const int oldValue = m_energy;
    m_energy = value;
    ViewAsSystem(kAllianceView)->ApplyEnergy(ctx);
    ViewAsSystem(kEmpireView)->ApplyEnergy(ctx);
    OnEnergyChanged(oldValue, m_energy, ctx);
  }
  return valid;
}

int System::SetEnergyAllocated(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, value, "Invalid SystemEnergyAllocated value!");
  }
  if (valid && m_energyAllocated != value) {
    const int oldValue = m_energyAllocated;
    m_energyAllocated = value;
    ViewAsSystem(kAllianceView)->ApplyEnergyAllocated(ctx);
    ViewAsSystem(kEmpireView)->ApplyEnergyAllocated(ctx);
    OnEnergyAllocatedChanged(oldValue, m_energyAllocated, ctx);
  }
  return valid;
}

int System::SetRawMaterial(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, value, "Invalid SystemRawMaterial value!");
  }
  if (valid && m_rawMaterial != value) {
    const int oldValue = m_rawMaterial;
    m_rawMaterial = value;
    ViewAsSystem(kAllianceView)->ApplyRawMaterial(ctx);
    ViewAsSystem(kEmpireView)->ApplyRawMaterial(ctx);
    OnRawMaterialChanged(oldValue, m_rawMaterial, ctx);
  }
  return valid;
}

int System::SetRawMaterialAllocated(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, value, "Invalid SystemRawMaterialAllocated value!");
  }
  if (valid && m_rawMaterialAllocated != value) {
    const int oldValue = m_rawMaterialAllocated;
    m_rawMaterialAllocated = value;
    ViewAsSystem(kAllianceView)->ApplyRawMaterialAllocated(ctx);
    ViewAsSystem(kEmpireView)->ApplyRawMaterialAllocated(ctx);
    OnRawMaterialAllocatedChanged(oldValue, m_rawMaterialAllocated, ctx);
  }
  return valid;
}

int System::SetSmugglingPercent(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, -100, 100, "Invalid SmugglingPercent value!");
  }
  if (valid && m_smugglingPercent != value) {
    const int oldValue = m_smugglingPercent;
    m_smugglingPercent = value;
    ViewAsSystem(kAllianceView)->ApplySmugglingPercent(ctx);
    ViewAsSystem(kEmpireView)->ApplySmugglingPercent(ctx);
    OnSmugglingPercentChanged(oldValue, m_smugglingPercent, ctx);
  }
  return valid;
}

int System::SetProductionModifier(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, value, "Invalid ProductionModifier value!");
  }
  if (valid && m_productionModifier != value) {
    const int oldValue = m_productionModifier;
    m_productionModifier = value;
    ViewAsSystem(kAllianceView)->ApplyProductionModifier(ctx);
    ViewAsSystem(kEmpireView)->ApplyProductionModifier(ctx);
    OnProductionModifierChanged(oldValue, m_productionModifier, ctx);
  }
  return valid;
}

int System::SetTroopRegWithdrawPercent(int value, NotifyContext* ctx)
{
  int valid = 0;
  if (IsMasterView()) {
    valid = IsInRange(value, 0, 100, "Invalid TroopRegWithdrawPercent value!");
  }
  if (valid && m_troopRegWithdrawPercent != value) {
    const int oldValue = m_troopRegWithdrawPercent;
    m_troopRegWithdrawPercent = value;
    ViewAsSystem(kAllianceView)->ApplyTroopRegWithdrawPercent(ctx);
    ViewAsSystem(kEmpireView)->ApplyTroopRegWithdrawPercent(ctx);
    OnTroopRegWithdrawPercentChanged(oldValue, m_troopRegWithdrawPercent, ctx);
  }
  return valid;
}

int System::SetTroopRegSurplus(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_troopRegSurplus != value) {
    const int oldValue = m_troopRegSurplus;
    m_troopRegSurplus = value;
    ViewAsSystem(kAllianceView)->ApplyTroopRegSurplus(ctx);
    ViewAsSystem(kEmpireView)->ApplyTroopRegSurplus(ctx);
    OnTroopRegSurplusChanged(oldValue, m_troopRegSurplus, ctx);
  }
  return result;
}

int System::SetTroopRegRequired(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && m_troopRegRequired != value) {
    const int oldValue = m_troopRegRequired;
    m_troopRegRequired = value;
    ViewAsSystem(kAllianceView)->ApplyTroopRegRequired(ctx);
    ViewAsSystem(kEmpireView)->ApplyTroopRegRequired(ctx);
    OnTroopRegRequiredChanged(oldValue, m_troopRegRequired, ctx);
  }
  return result;
}

int System::SetControlKindBattleWon(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result) {
    const uint32_t oldBits = m_controlBits;
    const uint32_t oldValue = ControlBitsField(oldBits, 0);
    if (oldValue != static_cast<uint32_t>(value)) {
      m_controlBits = WithControlBitsField(oldBits, 0, value);
      ViewAsSystem(kAllianceView)->ApplyControlKindBattleWon(ctx);
      ViewAsSystem(kEmpireView)->ApplyControlKindBattleWon(ctx);
      OnControlKindBattleWonChanged(oldValue, ControlBitsField(m_controlBits, 0), ctx);
    }
  }
  return result;
}

int System::SetControlKindBattleWithdrew(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result) {
    const uint32_t oldBits = m_controlBits;
    const uint32_t oldValue = ControlBitsField(oldBits, 2);
    if (oldValue != static_cast<uint32_t>(value)) {
      m_controlBits = WithControlBitsField(oldBits, 2, value);
      ViewAsSystem(kAllianceView)->ApplyControlKindBattleWithdrew(ctx);
      ViewAsSystem(kEmpireView)->ApplyControlKindBattleWithdrew(ctx);
      OnControlKindBattleWithdrewChanged(oldValue, ControlBitsField(m_controlBits, 2), ctx);
    }
  }
  return result;
}

int System::SetControlKindUprising(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result) {
    const uint32_t oldBits = m_controlBits;
    const uint32_t oldValue = ControlBitsField(oldBits, 4);
    if (oldValue != static_cast<uint32_t>(value)) {
      m_controlBits = WithControlBitsField(oldBits, 4, value);
      ViewAsSystem(kAllianceView)->ApplyControlKindUprising(ctx);
      ViewAsSystem(kEmpireView)->ApplyControlKindUprising(ctx);
      OnControlKindUprisingChanged(oldValue, ControlBitsField(m_controlBits, 4), ctx);
    }
  }
  return result;
}

int System::SetControlKindBattleReady(int value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result) {
    const uint32_t oldBits = m_controlBits;
    const uint32_t oldValue = ControlBitsField(oldBits, 6);
    if (oldValue != static_cast<uint32_t>(value)) {
      m_controlBits = WithControlBitsField(oldBits, 6, value);
      ViewAsSystem(kAllianceView)->ApplyControlKindBattleReady(ctx);
      ViewAsSystem(kEmpireView)->ApplyControlKindBattleReady(ctx);
      OnControlKindBattleReadyChanged(oldValue, ControlBitsField(m_controlBits, 6), ctx);
    }
  }
  return result;
}

int System::SetControlData(const SideBits* value, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && !(m_controlData == *value)) {
    const SideBits oldData(m_controlData);
    m_controlData = *value;
    ViewAsSystem(kAllianceView)->ApplyControlData(ctx);
    ViewAsSystem(kEmpireView)->ApplyControlData(ctx);
    OnControlDataChanged(&oldData, &m_controlData, ctx);
  }
  return result;
}

int System::SetPopulated(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kPopulated, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyPopulated(ctx);
    ViewAsSystem(kEmpireView)->ApplyPopulated(ctx);
    OnPopulatedChanged(on, ctx);
  }
  return result;
}

int System::SetExplored(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kExplored, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyExplored(ctx);
    ViewAsSystem(kEmpireView)->ApplyExplored(ctx);
    OnExploredChanged(on, ctx);
  }
  return result;
}

int System::SetUprising(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kUprising, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyUprising(ctx);
    ViewAsSystem(kEmpireView)->ApplyUprising(ctx);
    OnUprisingChanged(on, ctx);
  }
  return result;
}

int System::SetNeverBeenControlled(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kNeverBeenControlled, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyNeverBeenControlled(ctx);
    ViewAsSystem(kEmpireView)->ApplyNeverBeenControlled(ctx);
    OnNeverBeenControlledChanged(on, ctx);
  }
  return result;
}

int System::SetBattle(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBattle, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBattle(ctx);
    ViewAsSystem(kEmpireView)->ApplyBattle(ctx);
    OnBattleChanged(on, ctx);
  }
  return result;
}

int System::SetBlockade(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBlockade, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBlockade(ctx);
    ViewAsSystem(kEmpireView)->ApplyBlockade(ctx);
    OnBlockadeChanged(on, ctx);
  }
  return result;
}

int System::SetBombard(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBombard, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBombard(ctx);
    ViewAsSystem(kEmpireView)->ApplyBombard(ctx);
    OnBombardChanged(on, ctx);
  }
  return result;
}

int System::SetAssault(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kAssault, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyAssault(ctx);
    ViewAsSystem(kEmpireView)->ApplyAssault(ctx);
    OnAssaultChanged(on, ctx);
  }
  return result;
}

int System::SetGarrisoned(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kGarrisoned, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyGarrisoned(ctx);
    ViewAsSystem(kEmpireView)->ApplyGarrisoned(ctx);
    OnGarrisonedChanged(on, ctx);
  }
  return result;
}

int System::SetSuppressing(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kSuppressing, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplySuppressing(ctx);
    ViewAsSystem(kEmpireView)->ApplySuppressing(ctx);
    OnSuppressingChanged(on, ctx);
  }
  return result;
}

int System::SetCombatUnitFastRepair(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kCombatUnitFastRepair, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyCombatUnitFastRepair(ctx);
    ViewAsSystem(kEmpireView)->ApplyCombatUnitFastRepair(ctx);
    OnCombatUnitFastRepairChanged(on, ctx);
  }
  return result;
}

int System::SetDeathStarNearby(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kDeathStarNearby, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyDeathStarNearby(ctx);
    ViewAsSystem(kEmpireView)->ApplyDeathStarNearby(ctx);
    OnDeathStarNearbyChanged(on, ctx);
  }
  return result;
}

int System::SetBattlePending(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBattlePending, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBattlePending(ctx);
    ViewAsSystem(kEmpireView)->ApplyBattlePending(ctx);
    OnBattlePendingChanged(on, ctx);
  }
  return result;
}

int System::SetLoyaltyCausedCurrentControlKind(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kLoyaltyCausedCurrentControlKind, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyLoyaltyCausedCurrentControlKind(ctx);
    ViewAsSystem(kEmpireView)->ApplyLoyaltyCausedCurrentControlKind(ctx);
    OnLoyaltyCausedCurrentControlKindChanged(on, ctx);
  }
  return result;
}

int System::SetBattlePendingCausedCurrentBlockade(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBattlePendingCausedCurrentBlockade, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBattlePendingCausedCurrentBlockade(ctx);
    ViewAsSystem(kEmpireView)->ApplyBattlePendingCausedCurrentBlockade(ctx);
    OnBattlePendingCausedCurrentBlockadeChanged(on, ctx);
  }
  return result;
}

int System::SetUprisingIncident(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kUprisingIncident, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyUprisingIncident(ctx);
    ViewAsSystem(kEmpireView)->ApplyUprisingIncident(ctx);
    OnUprisingIncidentChanged(on, ctx);
  }
  return result;
}

int System::SetInformantIncident(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kInformantIncident, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyInformantIncident(ctx);
    ViewAsSystem(kEmpireView)->ApplyInformantIncident(ctx);
    OnInformantIncidentChanged(on, ctx);
  }
  return result;
}

int System::SetDisasterIncident(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kDisasterIncident, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyDisasterIncident(ctx);
    ViewAsSystem(kEmpireView)->ApplyDisasterIncident(ctx);
    OnDisasterIncidentChanged(on, ctx);
  }
  return result;
}

int System::SetResourceIncident(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kResourceIncident, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyResourceIncident(ctx);
    ViewAsSystem(kEmpireView)->ApplyResourceIncident(ctx);
    OnResourceIncidentChanged(on, ctx);
  }
  return result;
}

int System::SetBlockadeAndBattlePendingManagementRequired(int on, NotifyContext* ctx)
{
  const int result = IsMasterView();
  if (result && UpdateFlagBit(system_flag::kBlockadeAndBattlePendingManagementRequired, on, &m_flags)) {
    ViewAsSystem(kAllianceView)->ApplyBlockadeAndBattlePendingManagementRequired(ctx);
    ViewAsSystem(kEmpireView)->ApplyBlockadeAndBattlePendingManagementRequired(ctx);
    OnBlockadeAndBattlePendingManagementRequiredChanged(on, ctx);
  }
  return result;
}
