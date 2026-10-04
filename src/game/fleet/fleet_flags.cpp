#include <new>

#include "fleet/fleet.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_004ffdb0();

// Event hub of the strategic notification layer (cdecl).
int FUN_0053fdd0(int eventId, GameObj* object, int value, NotifyContext* ctx);

extern EventKindSet g_fleetEventKinds;       // 0x6b2b10
extern uint32_t g_fleetEventKindsReady;      // 0x6b2b18, set once the set has been filled

namespace {

constexpr uint32_t kFlag01 = 0x1;
constexpr uint32_t kFlag02 = 0x2;
constexpr uint32_t kFlag04 = 0x4;
constexpr uint32_t kBattle = 0x10;
constexpr uint32_t kBlockade = 0x20;
constexpr uint32_t kBombard = 0x40;
constexpr uint32_t kAssault = 0x80;

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kMasterView = 0;

constexpr int kEventBattle = 0x180;
constexpr int kEventBlockade = 0x181;
constexpr int kEventBombard = 0x182;
constexpr int kEventAssault = 0x183;

using ApplyFlag = int (Fleet::*)(NotifyContext*);
using FlagEvent = int (Fleet::*)(int, NotifyContext*);

Fleet* viewOf(Fleet* fleet, int view) { return static_cast<Fleet*>(fleet->ViewAs(view)); }

int setFleetFlag(Fleet* fleet, uint32_t mask, int on, NotifyContext* ctx, ApplyFlag apply, FlagEvent changed) {
    const int result = fleet->IsMasterView();
    if (result && UpdateFlagBit(mask, on, &fleet->m_fleetFlags)) {
        (viewOf(fleet, kAllianceView)->*apply)(ctx);
        (viewOf(fleet, kEmpireView)->*apply)(ctx);
        (fleet->*changed)(on, ctx);
    }
    return result;
}

int applyFleetFlag(Fleet* fleet, uint32_t mask, NotifyContext* ctx, FlagEvent notified) {
    int result = fleet->IsSideView();
    if (result) {
        Fleet* master = viewOf(fleet, kMasterView);
        if (fleet->MatchesSideMask(ctx->m_flag08)) {
            if (UpdateFlagBits(mask, master->m_fleetFlags, &fleet->m_fleetFlags)) {
                if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(fleet->ViewSide()))) {
                    result = (fleet->*notified)((fleet->m_fleetFlags & mask) != 0 ? 1 : 0, ctx);
                }
            }
        }
    }
    return result;
}

int notifyFleetFlag(Fleet* fleet, const char* tag, const char* property, int eventId, int value, NotifyContext* ctx) {
    int result = fleet->IsSideView();
    if (result) {
        fleet->LogFlag(tag, property, value, 0, ctx);
        result = FUN_0053fdd0(eventId, fleet, value, ctx);
    }
    return result;
}

}  // namespace

// ---- Set<Flag>: master only ---------------------------------------------------------------------------------

int Fleet::SetFlag01(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kFlag01, on, ctx, &Fleet::ApplyFlag01, &Fleet::OnFlag01Changed);
}

int Fleet::SetFlag02(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kFlag02, on, ctx, &Fleet::ApplyFlag02, &Fleet::OnFlag02Changed);
}

int Fleet::SetFlag04(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kFlag04, on, ctx, &Fleet::ApplyFlag04, &Fleet::OnFlag04Changed);
}

int Fleet::SetBattle(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kBattle, on, ctx, &Fleet::ApplyBattle, &Fleet::OnBattleChanged);
}

int Fleet::SetBlockade(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kBlockade, on, ctx, &Fleet::ApplyBlockade, &Fleet::OnBlockadeChanged);
}

int Fleet::SetBombard(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kBombard, on, ctx, &Fleet::ApplyBombard, &Fleet::OnBombardChanged);
}

int Fleet::SetAssault(int on, NotifyContext* ctx) {
    return setFleetFlag(this, kAssault, on, ctx, &Fleet::ApplyAssault, &Fleet::OnAssaultChanged);
}

// ---- Apply<Flag>: copy the master's bit into a side view and raise the notification -----------------------------

int Fleet::ApplyFlag01(NotifyContext* ctx) { return applyFleetFlag(this, kFlag01, ctx, &Fleet::OnFlag01Notified); }

int Fleet::ApplyFlag02(NotifyContext* ctx) { return applyFleetFlag(this, kFlag02, ctx, &Fleet::OnFlag02Notified); }

int Fleet::ApplyFlag04(NotifyContext* ctx) { return applyFleetFlag(this, kFlag04, ctx, &Fleet::OnFlag04Notified); }

int Fleet::ApplyBattle(NotifyContext* ctx) { return applyFleetFlag(this, kBattle, ctx, &Fleet::OnBattleNotified); }

int Fleet::ApplyBlockade(NotifyContext* ctx) {
    return applyFleetFlag(this, kBlockade, ctx, &Fleet::OnBlockadeNotified);
}

int Fleet::ApplyBombard(NotifyContext* ctx) { return applyFleetFlag(this, kBombard, ctx, &Fleet::OnBombardNotified); }

int Fleet::ApplyAssault(NotifyContext* ctx) { return applyFleetFlag(this, kAssault, ctx, &Fleet::OnAssaultNotified); }

// ---- On<Flag>Notified: events on a side view ---------------------------------------------------------------

int Fleet::OnFlag01Notified(int value, NotifyContext* ctx) { return IsSideView(); }

int Fleet::OnFlag02Notified(int value, NotifyContext* ctx) { return IsSideView(); }

int Fleet::OnFlag04Notified(int value, NotifyContext* ctx) { return IsSideView(); }

int Fleet::OnBattleNotified(int value, NotifyContext* ctx) {
    return notifyFleetFlag(this, "FleetBattleNotif", "Battle", kEventBattle, value, ctx);
}

int Fleet::OnBlockadeNotified(int value, NotifyContext* ctx) {
    return notifyFleetFlag(this, "FleetBlockadeNotif", "Blockade", kEventBlockade, value, ctx);
}

int Fleet::OnBombardNotified(int value, NotifyContext* ctx) {
    return notifyFleetFlag(this, "FleetBombardNotif", "Bombard", kEventBombard, value, ctx);
}

int Fleet::OnAssaultNotified(int value, NotifyContext* ctx) {
    return notifyFleetFlag(this, "FleetAssaultNotif", "Assault", kEventAssault, value, ctx);
}

// ---- event kinds -------------------------------------------------------------------------------------------

int Fleet::InitEventKinds() {
    int result = 1;
    if (g_fleetEventKindsReady == 0) {
        g_fleetEventKindsReady = 1;
        const int initialised = ::InitEventKinds(0, &g_fleetEventKinds);
        bool ok = g_fleetEventKinds.Add(0x201, 1) && initialised;
        ok = g_fleetEventKinds.Add(0x202, 1) && ok;
        ok = g_fleetEventKinds.Add(0x204, 1) && ok;
        ok = g_fleetEventKinds.Add(0x203, 1) && ok;
        ok = g_fleetEventKinds.Add(0x220, 1) && ok;
        ok = g_fleetEventKinds.Add(0x221, 1) && ok;
        ok = g_fleetEventKinds.Add(0x222, 1) && ok;
        ok = g_fleetEventKinds.Add(0x223, 1) && ok;
        ok = g_fleetEventKinds.Add(0x234, 1) && ok;
        const bool lastAdded = g_fleetEventKinds.Add(0x200, 1) != nullptr;
        if (lastAdded && ok) return 1;
        result = 0;
    }
    return result;
}

void FleetEventKindsConstruct() { new (&g_fleetEventKinds) EventKindSet(); }

void FleetEventKindsRegisterAtExit() { atexit(LAB_004ffdb0); }

void FleetStaticInit() {
    FleetEventKindsConstruct();
    FleetEventKindsRegisterAtExit();
}
