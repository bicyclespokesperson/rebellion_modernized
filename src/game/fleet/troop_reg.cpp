#include "fleet/troop_reg.h"

#include <new>

#include "core/utility_classes.h"
#include "data/random.h"
#include "events/event_post.h"
#include "fleet/unit_view_flags.h"
#include "gameobj/game_obj_iterator.h"


extern int g_troopRegEventKindsReady;      // 0x6b2b68
extern EventKindSet g_troopRegEventKinds;  // 0x6b2b70


namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kApplyEverything = 1;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kLockedFlag = 0x4000;
constexpr uint32_t kAlwaysFilter = 4;
constexpr int kFullWithdrawPercent = 100;
constexpr int kReasonRunningBlockade = 0xd;
constexpr int kRunningBlockadeEvent = 0x340;
constexpr int kKindMoveOrder = 0x201;
constexpr int kKindOrder202 = 0x202;
constexpr int kKindOrder204 = 0x204;
constexpr int kKindOrder200 = 0x200;
constexpr FamilyRange kTroopFamilies(0x10, 0x14);
}  // namespace

TroopReg::TroopReg() {
    m_runningBlockadeFlags = 0;
    m_withdrawPercent = kFullWithdrawPercent;
}

TroopReg::~TroopReg() = default;

int TroopReg::Read(istream* in) { return ReadState(in); }

int TroopReg::Write(ostream* out) { return WriteState(out); }

int TroopReg::Checksum(Crc32* crc) { return ChecksumState(crc); }

int TroopReg::ApplyAll(NotifyContext* ctx) { return ApplyAllState(ctx); }

int TroopReg::ReadState(istream* in) {
    const int result = Detector::Read(in);
    if (result) {
        ReadValue32(in, &m_runningBlockadeFlags);
        ReadValue32(in, &m_withdrawPercent);
    }
    return result;
}

int TroopReg::WriteState(ostream* out) {
    const int result = Detector::Write(out);
    if (result) {
        WriteValue32(out, &m_runningBlockadeFlags);
        WriteValue32(out, &m_withdrawPercent);
    }
    return result;
}

int TroopReg::ApplyAllState(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        Detector::ApplyAll(ctx);
        if (ctx->m_flag04 == kApplyEverything) {
            ApplyDestroyedRunningBlockade(ctx);
            ApplyWithdrawPercent(ctx);
        }
    }
    return result;
}

int TroopReg::ChecksumState(Crc32* crc) {
    const int result = Detector::Checksum(crc);
    if (result) {
        crc->AddInt(m_runningBlockadeFlags);
        crc->AddInt(m_withdrawPercent);
    }
    return result;
}

int TroopReg::Validate() {
    uint32_t valid = Detector::Validate();
    GameObjIterator children(this, kAlwaysFilter);
    children.Begin();
    if (children.Current()) valid = 0;
    GameObjNotifier* parent = nullptr;
    uint32_t result = 0;
    if (valid && (result = GetParent(&parent)) != 0) {
        if ((m_stateFlags & kDestroyedFlag) == 0 || (m_stateFlags & kLockedFlag) == 0) {
            result = (~static_cast<GameObj*>(parent)->m_stateFlags >> 3) & 1;
        }
    }
    return result;
}

int TroopReg::GetEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    if (ready) *out = g_troopRegEventKinds;
    return ready;
}

int TroopReg::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int ready = EnsureEventKinds();
    if (ready && g_troopRegEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return ready;
}

int TroopReg::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

int TroopReg::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

int TroopReg::CreateViews(int controlKind, NotifyContext* ctx) { return GameObj::CreateViews(controlKind, ctx); }

GameObj* TroopReg::CreateViewObject() { return new TroopReg(); }

int TroopReg::OnUsableChanged(int newValue, NotifyContext* ctx) {
    int result = Detector::OnUsableChanged(newValue, ctx);
    if (result && newValue) result = SetWithdrawPercent(kFullWithdrawPercent, ctx);
    return result;
}

int TroopReg::OnEnrouteActiveChanged(int newValue, NotifyContext* ctx) {
    int result = UnitObjBase::OnEnrouteActiveChanged(newValue, ctx);
    if (result && newValue) {
        if (PercentChance(m_withdrawPercent) == 0) result = SetDestroyedRunningBlockade(1, ctx);
    }
    return result;
}

int TroopReg::OnDestroyedRunningBlockadeChanged(int newValue, NotifyContext* ctx) {
    return ChangeDestroyedReason(kReasonRunningBlockade, ctx);
}

int TroopReg::OnDestroyedRunningBlockadeNotified(int value, NotifyContext* ctx) {
    int result = IsSideView();
    ObjectKey parentKey;
    if (result) {
        result = GetValidParentKey(&parentKey);
        if (result && value != 0) {
            ObjectKey logKey;
            LogKey("(TroopRegDestroyedRunningBlockadeNotif)", "TroopRegDestroyedRunningBlockade", &parentKey, &logKey,
                   0, ctx);
            ObjectKey eventKey;
            result = PostPayloadKeysA(kRunningBlockadeEvent, this, &parentKey, &eventKey, ctx);
        }
    }
    return result;
}

int TroopReg::OnWithdrawPercentNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(TroopRegWithdrawPercentNotif)", "WithdrawPercent", newValue, oldValue, 0, ctx);
    return result;
}

int TroopReg::SetDestroyedRunningBlockade(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(kDestroyedRunningBlockadeFlag, on, &m_runningBlockadeFlags)) {
        static_cast<TroopReg*>(GetViewObject(1))->ApplyDestroyedRunningBlockade(ctx);
        static_cast<TroopReg*>(GetViewObject(2))->ApplyDestroyedRunningBlockade(ctx);
        OnDestroyedRunningBlockadeChanged(on, ctx);
    }
    return result;
}

int TroopReg::SetWithdrawPercent(int value, NotifyContext* ctx) {
    const int master = IsMasterView();
    int result = 0;
    if (master) result = IsInRange(value, 0, 100, "TroopReg.cpp: Invalid WithdrawPercent value!");
    if (result && m_withdrawPercent != value) {
        const int oldValue = m_withdrawPercent;
        m_withdrawPercent = value;
        static_cast<TroopReg*>(GetViewObject(1))->ApplyWithdrawPercent(ctx);
        static_cast<TroopReg*>(GetViewObject(2))->ApplyWithdrawPercent(ctx);
        OnWithdrawPercentChanged(oldValue, m_withdrawPercent, ctx);
    }
    return result;
}

int TroopReg::ApplyDestroyedRunningBlockade(NotifyContext* ctx) {
    return fleet::applyFlagToView(this, &TroopReg::m_runningBlockadeFlags, kDestroyedRunningBlockadeFlag, ctx,
                                  [&](int value) { return OnDestroyedRunningBlockadeNotified(value, ctx); });
}

int TroopReg::ApplyWithdrawPercent(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        TroopReg* master = static_cast<TroopReg*>(GetViewObject(0));
        if (MatchesSideMask(ctx->m_flag08)) {
            const int oldValue = m_withdrawPercent;
            if (oldValue != master->m_withdrawPercent) {
                m_withdrawPercent = master->m_withdrawPercent;
                if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide()))) {
                    result = OnWithdrawPercentNotified(oldValue, m_withdrawPercent, ctx);
                }
            }
        }
    }
    return result;
}

int TroopReg::EnsureEventKinds() {
    if (g_troopRegEventKindsReady != 0) return 1;
    g_troopRegEventKindsReady = 1;
    return Detector::InitEventKinds(0, &g_troopRegEventKinds) && g_troopRegEventKinds.Add(kKindMoveOrder, 1) &&
           g_troopRegEventKinds.Add(kKindOrder202, 1) && g_troopRegEventKinds.Add(kKindOrder204, 1) &&
           g_troopRegEventKinds.Add(kKindOrder200, 1);
}

int CountTroopRegs(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kTroopFamilies, side, filter);
}

GameObj* CreateTroopReg() { return new TroopReg(); }

TroopRegIterator::TroopRegIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kTroopFamilies, stateFilter) {}

TroopRegIterator::TroopRegIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kTroopFamilies, stateFilter) {}

TroopRegIterator::~TroopRegIterator() = default;
EventKindSet g_troopRegEventKinds{};
int g_troopRegEventKindsReady{};
