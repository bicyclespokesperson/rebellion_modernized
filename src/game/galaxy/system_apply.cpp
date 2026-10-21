#include <new>

#include "galaxy/system.h"
#include "gameobj/game_hooks.h"

// Apply<Property>(ctx): copies the master's value into this side view and raises the OnXNotified event on it. Only side
// views are touched, only when the context's side filter (m_flag08) matches the view and, for the event, when the
// strategic layer's hook and the context both ask for notifications on this view.

namespace
{

constexpr int kMasterView = 0;

bool wantsNotification(System* view, NotifyContext* ctx)
{
  return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

} // namespace

int System::ApplyLoyalty(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_loyalty;
      if (oldValue != master->m_loyalty) {
        m_loyalty = master->m_loyalty;
        if (wantsNotification(this, ctx)) {
          result = OnLoyaltyNotified(oldValue, m_loyalty, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyEnergyAllocated(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_energyAllocated;
      if (oldValue != master->m_energyAllocated) {
        m_energyAllocated = master->m_energyAllocated;
        if (wantsNotification(this, ctx)) {
          result = OnEnergyAllocatedNotified(oldValue, m_energyAllocated, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyEnergy(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_energy;
      if (oldValue != master->m_energy) {
        m_energy = master->m_energy;
        if (wantsNotification(this, ctx)) {
          result = OnEnergyNotified(oldValue, m_energy, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyRawMaterial(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_rawMaterial;
      if (oldValue != master->m_rawMaterial) {
        m_rawMaterial = master->m_rawMaterial;
        if (wantsNotification(this, ctx)) {
          result = OnRawMaterialNotified(oldValue, m_rawMaterial, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyRawMaterialAllocated(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_rawMaterialAllocated;
      if (oldValue != master->m_rawMaterialAllocated) {
        m_rawMaterialAllocated = master->m_rawMaterialAllocated;
        if (wantsNotification(this, ctx)) {
          result = OnRawMaterialAllocatedNotified(oldValue, m_rawMaterialAllocated, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplySmugglingPercent(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_smugglingPercent;
      if (oldValue != master->m_smugglingPercent) {
        m_smugglingPercent = master->m_smugglingPercent;
        if (wantsNotification(this, ctx)) {
          result = OnSmugglingPercentNotified(oldValue, m_smugglingPercent, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyProductionModifier(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_productionModifier;
      if (oldValue != master->m_productionModifier) {
        m_productionModifier = master->m_productionModifier;
        if (wantsNotification(this, ctx)) {
          result = OnProductionModifierNotified(oldValue, m_productionModifier, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyTroopRegWithdrawPercent(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_troopRegWithdrawPercent;
      if (oldValue != master->m_troopRegWithdrawPercent) {
        m_troopRegWithdrawPercent = master->m_troopRegWithdrawPercent;
        if (wantsNotification(this, ctx)) {
          result = OnTroopRegWithdrawPercentNotified(oldValue, m_troopRegWithdrawPercent, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyTroopRegSurplus(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_troopRegSurplus;
      if (oldValue != master->m_troopRegSurplus) {
        m_troopRegSurplus = master->m_troopRegSurplus;
        if (wantsNotification(this, ctx)) {
          result = OnTroopRegSurplusNotified(oldValue, m_troopRegSurplus, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyTroopRegRequired(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const int oldValue = m_troopRegRequired;
      if (oldValue != master->m_troopRegRequired) {
        m_troopRegRequired = master->m_troopRegRequired;
        if (wantsNotification(this, ctx)) {
          result = OnTroopRegRequiredNotified(oldValue, m_troopRegRequired, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyControlKindBattleWon(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const uint32_t oldBits = m_controlBits;
      const uint32_t masterValue = ControlBitsField(master->m_controlBits, 0);
      if (ControlBitsField(oldBits, 0) != masterValue) {
        m_controlBits = WithControlBitsField(oldBits, 0, masterValue);
        if (wantsNotification(this, ctx)) {
          result
              = OnControlKindBattleWonNotified(ControlBitsField(oldBits, 0), ControlBitsField(m_controlBits, 0), ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyControlKindBattleWithdrew(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const uint32_t oldBits = m_controlBits;
      const uint32_t masterValue = ControlBitsField(master->m_controlBits, 2);
      if (ControlBitsField(oldBits, 2) != masterValue) {
        m_controlBits = WithControlBitsField(oldBits, 2, masterValue);
        if (wantsNotification(this, ctx)) {
          result = OnControlKindBattleWithdrewNotified(ControlBitsField(oldBits, 2), ControlBitsField(m_controlBits, 2),
                                                       ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyControlKindUprising(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const uint32_t oldBits = m_controlBits;
      const uint32_t masterValue = ControlBitsField(master->m_controlBits, 4);
      if (ControlBitsField(oldBits, 4) != masterValue) {
        m_controlBits = WithControlBitsField(oldBits, 4, masterValue);
        if (wantsNotification(this, ctx)) {
          result = OnControlKindUprisingNotified(ControlBitsField(oldBits, 4), ControlBitsField(m_controlBits, 4), ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyControlKindBattleReady(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      const uint32_t oldBits = m_controlBits;
      const uint32_t masterValue = ControlBitsField(master->m_controlBits, 6);
      if (ControlBitsField(oldBits, 6) != masterValue) {
        m_controlBits = WithControlBitsField(oldBits, 6, masterValue);
        if (wantsNotification(this, ctx)) {
          result
              = OnControlKindBattleReadyNotified(ControlBitsField(oldBits, 6), ControlBitsField(m_controlBits, 6), ctx);
        }
      }
    }
  }
  return result;
}

SideBits* System::GetControlData(SideBits* out)
{
  new (out) SideBits(m_controlData);
  return out;
}

int System::ApplyControlData(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      SideBits masterData;
      if (!(m_controlData == *master->GetControlData(&masterData))) {
        const SideBits oldData(m_controlData);
        SideBits newData;
        m_controlData = *master->GetControlData(&newData);
        if (wantsNotification(this, ctx)) {
          result = OnControlDataNotified(&oldData, &m_controlData, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyPopulated(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kPopulated, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnPopulatedNotified((m_flags & system_flag::kPopulated) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyExplored(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kExplored, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnExploredNotified((m_flags & system_flag::kExplored) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyUprising(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kUprising, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnUprisingNotified((m_flags & system_flag::kUprising) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyNeverBeenControlled(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kNeverBeenControlled, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnNeverBeenControlledNotified((m_flags & system_flag::kNeverBeenControlled) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBattle(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBattle, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBattleNotified((m_flags & system_flag::kBattle) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBlockade(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBlockade, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBlockadeNotified((m_flags & system_flag::kBlockade) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBombard(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBombard, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBombardNotified((m_flags & system_flag::kBombard) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyAssault(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kAssault, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnAssaultNotified((m_flags & system_flag::kAssault) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyGarrisoned(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kGarrisoned, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnGarrisonedNotified((m_flags & system_flag::kGarrisoned) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplySuppressing(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kSuppressing, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnSuppressingNotified((m_flags & system_flag::kSuppressing) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyCombatUnitFastRepair(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kCombatUnitFastRepair, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnCombatUnitFastRepairNotified((m_flags & system_flag::kCombatUnitFastRepair) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyDeathStarNearby(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kDeathStarNearby, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnDeathStarNearbyNotified((m_flags & system_flag::kDeathStarNearby) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBattlePending(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBattlePending, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBattlePendingNotified((m_flags & system_flag::kBattlePending) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyLoyaltyCausedCurrentControlKind(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kLoyaltyCausedCurrentControlKind, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnLoyaltyCausedCurrentControlKindNotified(
              (m_flags & system_flag::kLoyaltyCausedCurrentControlKind) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBattlePendingCausedCurrentBlockade(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBattlePendingCausedCurrentBlockade, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBattlePendingCausedCurrentBlockadeNotified(
              (m_flags & system_flag::kBattlePendingCausedCurrentBlockade) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyUprisingIncident(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kUprisingIncident, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnUprisingIncidentNotified((m_flags & system_flag::kUprisingIncident) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyInformantIncident(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kInformantIncident, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnInformantIncidentNotified((m_flags & system_flag::kInformantIncident) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyDisasterIncident(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kDisasterIncident, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnDisasterIncidentNotified((m_flags & system_flag::kDisasterIncident) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyResourceIncident(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kResourceIncident, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnResourceIncidentNotified((m_flags & system_flag::kResourceIncident) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}

int System::ApplyBlockadeAndBattlePendingManagementRequired(NotifyContext* ctx)
{
  int result = IsSideView();
  if (result) {
    System* master = ViewAsSystem(kMasterView);
    if (MatchesSideMask(ctx->m_flag08)) {
      if (UpdateFlagBits(system_flag::kBlockadeAndBattlePendingManagementRequired, master->m_flags, &m_flags)) {
        if (wantsNotification(this, ctx)) {
          result = OnBlockadeAndBattlePendingManagementRequiredNotified(
              (m_flags & system_flag::kBlockadeAndBattlePendingManagementRequired) != 0 ? 1 : 0, ctx);
        }
      }
    }
  }
  return result;
}
