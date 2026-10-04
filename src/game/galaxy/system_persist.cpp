#include "core/utility_classes.h"
#include "galaxy/system_rules.h"
#include "galaxy/system_rules_decl.h"
#include "gameobj/game_hooks.h"

using namespace system_rules;

// The bodies behind the pure persistence slots of System (the leaf classes call them qualified), and the view creation
// that fills in the data block.

int System::Read(istream* in) {
    const int result = GameObj::Read(in);
    if (result) {
        ReadValue32(in, &m_loyalty);
        ReadValue32(in, &m_energy);
        ReadValue32(in, &m_energyAllocated);
        ReadValue32(in, &m_rawMaterial);
        ReadValue32(in, &m_rawMaterialAllocated);
        ReadValue32(in, &m_smugglingPercent);
        ReadValue32(in, &m_productionModifier);
        ReadValue32(in, &m_troopRegWithdrawPercent);
        ReadValue32(in, &m_flags);
        ReadValue32(in, &m_controlBits);
        ReadValue32(in, &m_troopRegSurplus);
        ReadValue32(in, &m_troopRegRequired);
        m_controlData.Read(in);
    }
    return result;
}

int System::Write(ostream* out) {
    const int result = GameObj::Write(out);
    if (result) {
        WriteValue32(out, &m_loyalty);
        WriteValue32(out, &m_energy);
        WriteValue32(out, &m_energyAllocated);
        WriteValue32(out, &m_rawMaterial);
        WriteValue32(out, &m_rawMaterialAllocated);
        WriteValue32(out, &m_smugglingPercent);
        WriteValue32(out, &m_productionModifier);
        WriteValue32(out, &m_troopRegWithdrawPercent);
        WriteValue32(out, &m_flags);
        WriteValue32(out, &m_controlBits);
        WriteValue32(out, &m_troopRegSurplus);
        WriteValue32(out, &m_troopRegRequired);
        m_controlData.Write(out);
    }
    return result;
}

int System::Checksum(Crc32* crc) {
    const int result = GameObj::Checksum(crc);
    if (result) {
        crc->AddInt(m_loyalty);
        crc->AddInt(m_energy);
        crc->AddInt(m_energyAllocated);
        crc->AddInt(m_rawMaterial);
        crc->AddInt(m_rawMaterialAllocated);
        crc->AddInt(m_smugglingPercent);
        crc->AddInt(m_productionModifier);
        crc->AddInt(m_troopRegWithdrawPercent);
        crc->AddInt(m_flags);
        crc->AddInt(m_controlBits & 3);
        crc->AddInt((m_controlBits >> 2) & 3);
        crc->AddInt((m_controlBits >> 4) & 3);
        crc->AddInt((m_controlBits >> 6) & 3);
        crc->AddInt(m_troopRegSurplus);
        crc->AddInt(m_troopRegRequired);
        m_controlData.Checksum(crc);
    }
    return result;
}

int System::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) {
        GameObj::ApplyAll(ctx);
        if (ctx->m_flag04 == 1) {
            ApplyLoyalty(ctx);
            ApplyEnergy(ctx);
            FUN_005037f0(this, ctx);
            ApplyRawMaterial(ctx);
            ApplyRawMaterialAllocated(ctx);
            ApplySmugglingPercent(ctx);
            ApplyProductionModifier(ctx);
            ApplyTroopRegWithdrawPercent(ctx);
            ApplyControlKindBattleWon(ctx);
            ApplyControlKindBattleWithdrew(ctx);
            ApplyControlKindUprising(ctx);
            ApplyControlKindBattleReady(ctx);
            ApplyTroopRegSurplus(ctx);
            ApplyTroopRegRequired(ctx);
            ApplyControlData(ctx);
            ApplyPopulated(ctx);
            ApplyExplored(ctx);
            ApplyUprising(ctx);
            ApplyNeverBeenControlled(ctx);
            ApplyBattle(ctx);
            ApplyBlockade(ctx);
            ApplyBombard(ctx);
            ApplyAssault(ctx);
            ApplyGarrisoned(ctx);
            ApplySuppressing(ctx);
            ApplyCombatUnitFastRepair(ctx);
            ApplyDeathStarNearby(ctx);
            ApplyBattlePending(ctx);
            ApplyLoyaltyCausedCurrentControlKind(ctx);
            ApplyBattlePendingCausedCurrentBlockade(ctx);
            ApplyUprisingIncident(ctx);
            ApplyInformantIncident(ctx);
            ApplyDisasterIncident(ctx);
            ApplyResourceIncident(ctx);
            ApplyBlockadeAndBattlePendingManagementRequired(ctx);
        } else if (ctx->m_flag04 == 2) {
            ApplyParent(ctx);
            ApplyUsable(ctx);
            ApplyCreated(ctx);
            ApplyCompleted(ctx);
            ApplyExisting(ctx);
        }
    }
    return isSideView;
}

namespace {

int createStartingChild(System* system, uint32_t keyValue, NotifyContext* ctx) {
    ObjectKey key;
    key.m_value = keyValue;
    return system->CreateChild(&key, system->ControlSide(), ctx, 1, 1) != nullptr;
}

}  // namespace

int System::CreateViews(int controlKind, NotifyContext* ctx) {
    const int created = GameObj::CreateViews(controlKind, ctx);
    if (!created) return 0;
    if (!createStartingChild(this, 0xa2000006, ctx)) return 0;
    if (!createStartingChild(this, 0xa4000007, ctx)) return 0;
    if (!createStartingChild(this, 0xa0000005, ctx)) return 0;

    SystemData* data = static_cast<SystemData*>(m_data);
    data->m_stats[0].SetLevel(DAT_006bb3a0);
    data->m_stats[0].SetProgress(DAT_006bb3a4);
    data->m_stats[1].SetLevel(DAT_006bb3c4);
    data->m_stats[2].SetLevel(DAT_006bb470);
    data->m_stats[3].SetLevel(DAT_006bb3d8);
    data->m_stats[4].SetLevel(DAT_006bb420);
    data->m_stats[4].SetProgress(DAT_006bb49c);
    data->m_stats[5].SetLevel(DAT_006bb3d0);
    data->m_stats[5].SetProgress(DAT_006bb484);
    NotifyContext context(*ctx);
    context.m_reserved14 = CallGameHook2AFC();
    return CheckHyperspaceBlocked(3, controlKind, &context);
}
