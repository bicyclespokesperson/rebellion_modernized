#include "galaxy/system.h"
#include "events/event_post.h"

namespace {
constexpr uint32_t kEmpireSide = 2;
}  // namespace

// OnXNotified: events raised on a side view (the "System*Notif" strings). The debug log calls are dead in the shipped
// game; the hub calls forward the change to the UI (ids 0x140-0x156).

int System::OnLoyaltyNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        String text;
        const String delta("Delta=");
        text += delta;
        text.AppendSigned(newValue - oldValue);
        LogInt("SystemLoyaltyNotif", "Loyalty", newValue, oldValue, static_cast<int>(reinterpret_cast<intptr_t>(text.m_text)), ctx);
        int shownOld = oldValue;
        int shownNew = newValue;
        if (ViewSide() == kEmpireSide) {
            shownNew = 100 - newValue;
            shownOld = 100 - oldValue;
        }
        result = PostPayloadInts(0x140, this, shownOld, shownNew, ctx);
    }
    return result;
}

int System::OnEnergyNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemSystemEnergyNotif", "SystemEnergy", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x141, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnEnergyAllocatedNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemSystemEnergyAllocatedNotif", "SystemEnergyAllocated", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x142, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnRawMaterialNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemSystemRawMaterialNotif", "SystemRawMaterial", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x143, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnRawMaterialAllocatedNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemSystemRawMaterialAllocatedNotif", "SystemRawMaterialAllocated", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x144, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnSmugglingPercentNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemSmugglingPercentNotif", "SmugglingPercent", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x145, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnProductionModifierNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemProductionModifierNotif", "ProductionModifier", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x146, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnTroopRegWithdrawPercentNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemTroopRegWithdrawPercentNotif", "TroopRegWithdrawPercent", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x147, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnTroopRegSurplusNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemTroopRegSurplusNotif", "TroopRegSurplus", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x148, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnTroopRegRequiredNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogInt("SystemTroopRegRequiredNotif", "TroopRegRequired", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x156, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnControlKindBattleWonNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogSide("(SystemControlKindBattleWonNotif)", "ControlKindBattleWon", newValue, oldValue, 0, ctx);
    return result;
}

int System::OnControlKindBattleWithdrewNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogSide("(SystemControlKindBattleWithdrewNotif)", "ControlKindBattleWithdrew", newValue, oldValue, 0, ctx);
    return result;
}

int System::OnControlKindUprisingNotified(int oldValue, int newValue, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogSide("SystemControlKindUprisingNotif", "ControlKindUprising", newValue, oldValue, 0, ctx);
        result = PostPayloadInts(0x151, this, oldValue, newValue, ctx);
    }
    return result;
}

int System::OnControlKindBattleReadyNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogSide("(SystemControlKindBattleReadyNotif)", "ControlKindBattleReady", newValue, oldValue, 0, ctx);
    return result;
}

int System::OnControlDataNotified(const SideBits* oldValue, const SideBits* newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        String newText;
        String oldText;
        const_cast<SideBits*>(newValue)->AppendDescription(&newText);
        const_cast<SideBits*>(oldValue)->AppendDescription(&oldText);
        LogChange("(SystemControlDataNotif)", "ControlData", newText.m_text, oldText.m_text, nullptr, ctx);
    }
    return result;
}

int System::OnPopulatedNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemPopulatedNotif", "Populated", value, 0, ctx);
        result = PostPayloadFlag(0x149, this, value, ctx);
    }
    return result;
}

int System::OnExploredNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemExploredNotif", "Explored", value, 0, ctx);
        result = PostPayloadFlag(0x14a, this, value, ctx);
    }
    return result;
}

int System::OnUprisingNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemUprisingNotif", "Uprising", value, 0, ctx);
        result = PostPayloadFlag(0x14b, this, value, ctx);
    }
    return result;
}

int System::OnNeverBeenControlledNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemNeverBeenControlledNotif", "NeverBeenControlled", value, 0, ctx);
        result = PostPayloadFlag(0x14c, this, value, ctx);
    }
    return result;
}

int System::OnBattleNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemBattleNotif", "Battle", value, 0, ctx);
        result = PostPayloadFlag(0x14d, this, value, ctx);
    }
    return result;
}

int System::OnBlockadeNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemBlockadeNotif", "Blockade", value, 0, ctx);
        result = PostPayloadFlag(0x14e, this, value, ctx);
    }
    return result;
}

int System::OnBombardNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemBombardNotif", "Bombard", value, 0, ctx);
        result = PostPayloadInts(0x14f, this, ControlSide(), value, ctx);
    }
    return result;
}

int System::OnAssaultNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemAssaultNotif", "Assault", value, 0, ctx);
        result = PostPayloadInts(0x150, this, ControlSide(), value, ctx);
    }
    return result;
}

int System::OnGarrisonedNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemGarrisonedNotif)", "Garrisoned", value, 0, ctx);
    return result;
}

int System::OnSuppressingNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemSuppressingNotif)", "Suppressing", value, 0, ctx);
    return result;
}

int System::OnCombatUnitFastRepairNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemCombatUnitFastRepairNotif)", "CombatUnitFastRepair", value, 0, ctx);
    return result;
}

int System::OnDeathStarNearbyNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemDeathStarNearbyNotif)", "DeathStarNearby", value, 0, ctx);
    return result;
}

int System::OnBattlePendingNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemBattlePendingNotif)", "BattlePending", value, 0, ctx);
    return result;
}

int System::OnLoyaltyCausedCurrentControlKindNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemLoyaltyCausedCurrentControlKindNotif)", "LoyaltyCausedCurrentControlKind", value, 0, ctx);
    return result;
}

int System::OnBattlePendingCausedCurrentBlockadeNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemBattlePendingCausedCurrentBlockade)", "BattlePendingCausedCurrentBlockade", value, 0, ctx);
    return result;
}

int System::OnUprisingIncidentNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemUprisingIncidentNotif", "UprisingIncident", value, 0, ctx);
        result = PostPayloadFlag(0x152, this, value, ctx);
    }
    return result;
}

int System::OnInformantIncidentNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemInformantIncidentNotif", "InformantIncident", value, 0, ctx);
        result = PostPayloadFlag(0x153, this, value, ctx);
    }
    return result;
}

int System::OnDisasterIncidentNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemDisasterIncidentNotif", "DisasterIncident", value, 0, ctx);
        result = PostPayloadFlag(0x154, this, value, ctx);
    }
    return result;
}

int System::OnResourceIncidentNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("SystemResourceIncidentNotif", "ResourceIncident", value, 0, ctx);
        result = PostPayloadFlag(0x155, this, value, ctx);
    }
    return result;
}

int System::OnBlockadeAndBattlePendingManagementRequiredNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(SystemBlockadeAndBattlePendingManagementRequiredNotif)", "BlockadeAndBattlePendingManagementRequired", value, 0, ctx);
    return result;
}
