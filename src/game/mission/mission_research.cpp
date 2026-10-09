#include "mission/mission_research.h"

#include "ai/ai_common.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/random.h"
#include "data/sd_record_bases.h"
#include "economy/manu_mgr.h"
#include "economy/side.h"
#include "galaxy/mission_decl.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "mission/mission_external.h"
#include "mission/mission_rules.h"

void DestroyMissionResearchEventKinds();  // the atexit destructor of the event kind set

extern int g_missionResearchEventKindsReady;      // 0x6bb984
extern EventKindSet g_missionResearchEventKinds;  // 0x6bb988

class DatRecord;

namespace {
constexpr uint32_t kFamilyMissionResearch = 0x53;
constexpr uint32_t kKeyShipyardResearch = 0x53000020;
constexpr uint32_t kKeyTrainingFacilResearch = 0x53000021;
constexpr uint32_t kKeyConstructionYardResearch = 0x53000022;
constexpr int kTaskStarted = 2;
constexpr int kStatusCategoryBlocked = 0x40;
constexpr int kStatusNoResearchSkill = 4;
constexpr int kStatusNoHostFacility = 0x2c;
constexpr int kStatusResearchDone = 0x32;
constexpr int kOutNoHostFacility = 0xc;
constexpr int kOutResearchDone = 0xe;

constexpr int kRefSetOffsets[] = {0x18, 0x1c, 0x20};
constexpr int kKeySetOffsets[] = {0x24, 0x28, 0x2c};

template <class T>
T& FieldAt(MissionCheckArgs* check, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(check) + offset);
}

bool StatusIsOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(mission_status::kOk); }

int ControlSideAsSideIndex(uint32_t controlSide) {
    if (controlSide == 1) return 1;
    return controlSide == 2 ? 2 : 0;
}

int ResearchKindOfCheck(MissionCheckArgs* check, int* kind) {
    ObjectKey key;
    return MissionResearch::ResearchKindFromKey(GetAiRecordKey(FieldAt<DatRecord*>(check, 0x14), &key), kind);
}

template <class Set, int (*Check)(Set*, MissionCheckArgs*, StatusCode*, int*)>
int CheckThreeSets(MissionCheckArgs* check, StatusCode* status, int* out, const int (&offsets)[3], int result) {
    if (!result) return result;
    if (StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[0]), check, status, out);
    if (!result) return result;
    if (StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[1]), check, status, out);
    if (result && StatusIsOk(status)) result = Check(FieldAt<Set*>(check, offsets[2]), check, status, out);
    return result;
}

void RejectMissingSkill(StatusCode* status, int* out) {
    *status = StatusCode(kStatusCategoryBlocked, kStatusNoResearchSkill);
    *out = -1;
}
}  // namespace

MissionResearch::MissionResearch() {
    m_rdPoints = 0;
    m_rdOrders = 0;
}

MissionResearch::~MissionResearch() { REB_KEEP_VPTR_STORE(); }

int MissionResearch::Read(istream* in) {
    const int result = static_cast<MissionRuleBase*>(this)->MissionRuleBase::ReadBody(in);
    if (result) {
        ReadValue32(in, &m_rdPoints);
        ReadValue32(in, &m_rdOrders);
    }
    return result;
}

int MissionResearch::Write(ostream* out) {
    const int result = WriteImpl(out);
    if (result) {
        WriteValue32(out, &m_rdPoints);
        WriteValue32(out, &m_rdOrders);
    }
    return result;
}

int MissionResearch::Checksum(Crc32* crc) {
    const int result = reinterpret_cast<MissionTypeBase*>(this)->MissionTypeBase::ChecksumBody(crc);
    if (result) {
        crc->AddInt(m_rdPoints);
        crc->AddInt(m_rdOrders);
    }
    return result;
}

int MissionResearch::Validate() { return this->Mission::ValidateImpl(); }

int MissionResearch::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllBody(ctx) : 0; }

int MissionResearch::CreateViews(int controlKind, NotifyContext* ctx) { return this->Mission::CreateViewsImpl(controlKind, ctx); }

int MissionResearch::ApplyAllBody(NotifyContext* ctx) {
    const int result = ApplyAllImpl(ctx);
    if (result && ctx->m_flag04 == 1) {
        ApplyRdPoints(ctx);
        ApplyRdOrders(ctx);
    }
    return result;
}

uint32_t MissionResearch::GetFamily() { return kFamilyMissionResearch; }

int MissionResearch::GetEventKinds(int unused, EventKindSet* out) {
    return CopyEventKinds(BuildEventKinds, &g_missionResearchEventKinds, out);
}

int MissionResearch::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = MakeStatus(mission_status::kNotSupported);
    const int built = BuildEventKinds();
    if (g_missionResearchEventKinds.Contains(orderKind)) *out = MakeStatus(mission_status::kOk);
    return built != 0;
}

int MissionResearch::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(MissionRuleBase::CheckMember);
    out->m_unk04 = reinterpret_cast<void*>(CheckTarget);
    return 1;
}

GameObj* MissionResearch::CreateViewObject() { return NewOrNull<MissionResearch>(); }

GameObjNotifier* MissionResearch::Create() { return NewOrNull<MissionResearch>(); }

int MissionResearch::SetRdPoints(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        const int oldValue = m_rdPoints;
        if (oldValue != value) {
            m_rdPoints = value;
            static_cast<MissionResearch*>(ViewAs(1))->ApplyRdPoints(ctx);
            static_cast<MissionResearch*>(ViewAs(2))->ApplyRdPoints(ctx);
            OnRdPointsChanged(oldValue, m_rdPoints, ctx);
        }
    }
    return result;
}

int MissionResearch::SetRdOrders(int value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result) {
        const int oldValue = m_rdOrders;
        if (oldValue != value) {
            m_rdOrders = value;
            static_cast<MissionResearch*>(ViewAs(1))->ApplyRdOrders(ctx);
            static_cast<MissionResearch*>(ViewAs(2))->ApplyRdOrders(ctx);
            OnRdOrdersChanged(oldValue, m_rdOrders, ctx);
        }
    }
    return result;
}

int MissionResearch::ApplyRdPoints(NotifyContext* ctx) {
    int result = IsSideView();
    if (!result) return result;
    MissionResearch* master = static_cast<MissionResearch*>(GetViewObject(0));
    if (!MatchesSideMask(ctx->m_flag08)) return result;
    const int oldValue = m_rdPoints;
    if (oldValue == master->m_rdPoints) return result;
    m_rdPoints = master->m_rdPoints;
    if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
        result = OnRdPointsNotified(oldValue, m_rdPoints, ctx);
    return result;
}

int MissionResearch::ApplyRdOrders(NotifyContext* ctx) {
    int result = IsSideView();
    if (!result) return result;
    MissionResearch* master = static_cast<MissionResearch*>(GetViewObject(0));
    if (!MatchesSideMask(ctx->m_flag08)) return result;
    const int oldValue = m_rdOrders;
    if (oldValue == master->m_rdOrders) return result;
    m_rdOrders = master->m_rdOrders;
    if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide())))
        result = OnRdOrdersNotified(oldValue, m_rdOrders, ctx);
    return result;
}

int MissionResearch::OnRdPointsNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(RdPointsNotif)", "RdPoints", newValue, oldValue, 0, ctx);
    return result;
}

int MissionResearch::OnRdOrdersNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(RdOrdersNotif)", "RdOrders", newValue, oldValue, 0, ctx);
    return result;
}

int MissionResearch::ResearchKindFromKey(ObjectKey* key, int* out) {
    *out = 0;
    if (key->m_value == kKeyShipyardResearch) {
        *out = 0;
        return 1;
    }
    if (key->m_value == kKeyTrainingFacilResearch) {
        *out = 1;
        return 1;
    }
    if (key->m_value == kKeyConstructionYardResearch) {
        *out = 2;
        return 1;
    }
    return 0;
}

int MissionResearch::GetResearchKind(int* out) {
    ObjectKey key;
    return ResearchKindFromKey(GetObjectRecordKey(this, &key), out);
}

int MissionResearch::CheckCandidateKeys(ObjectKeySet* keys, MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int kind = 0;
    int result = 1;
    if (StatusIsOk(status)) result = ResearchKindOfCheck(check, &kind);
    for (ObjectKeyNode* node = keys->First(); node && result && StatusIsOk(status);
         node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        node->GetKey(&key);
        GameObj* character = nullptr;
        result = 0;
        if (ResolveRoleFromKey(&key, &character)) {
            int skill = 0;
            result = static_cast<Role*>(character)->GetEffectiveRdForKind(kind, &skill);
            if (result && skill == 0) RejectMissingSkill(status, out);
        }
    }
    return result;
}

int MissionResearch::CheckCandidateRefs(ObjectRefList* refs, MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int kind = 0;
    int result = 1;
    if (StatusIsOk(status)) result = ResearchKindOfCheck(check, &kind);
    for (AvlNode* node = refs->First(); node && result && StatusIsOk(status); node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectRefNode*>(node)->GetKey(&key);
        DatRecord* person = nullptr;
        result = 0;
        if (ResolvePersonRecord(&key, &person)) {
            SdRecordExt16* record = static_cast<SdRecordExt16*>(person);
            uint32_t first = 0;
            uint32_t second = 0;
            if (record->GetPairFirst(kind, &first)) {
                result = record->GetPairSecond(kind, &second);
                if (result && first == 0 && second == 0) RejectMissingSkill(status, out);
            }
        }
    }
    return result;
}

int MissionResearch::CheckTarget(MissionCheckArgs* check, StatusCode* status, int* out) {
    *status = MakeStatus(mission_status::kOk);
    *out = 0;
    int result = MissionSystemTargetBase::CheckTarget(check, status, out);
    if (FieldAt<int>(check, 4) != 0) {
        if (FieldAt<int>(check, 8) == 0)
            result = CheckThreeSets<ObjectRefList, CheckCandidateRefs>(check, status, out, kRefSetOffsets, result);
        else
            result = CheckThreeSets<ObjectKeySet, CheckCandidateKeys>(check, status, out, kKeySetOffsets, result);
    }
    if (FieldAt<int>(check, 0xc) == 0 || !result) return result;
    if (StatusIsOk(status)) {
        int kind = 0;
        result = ResearchKindOfCheck(check, &kind);
        GameObjNotifier* manager = nullptr;
        if (result) manager = FieldAt<System*>(check, 0x38)->FindManufacturingManagerBySlot123(kind);
        if (!manager || !static_cast<ManuMgr*>(manager)->HasHostFacility(0)) {
            *status = StatusCode(kStatusCategoryBlocked, kStatusNoHostFacility);
            *out = kOutNoHostFacility;
        }
    }
    if (result && StatusIsOk(status)) {
        int kind = 0;
        result = ResearchKindOfCheck(check, &kind);
        const int sideIndex = result ? ControlSideAsSideIndex(FieldAt<int>(check, 0x10)) : 0;
        Side* sideObject = sideIndex ? static_cast<Side*>(GetGameRootSideMemberThunk(sideIndex)) : nullptr;
        result = sideObject != nullptr;
        int done = 0;
        if (result) {
            result = sideObject->GetRdDone(kind, &done);
            if (result && done != 0) {
                *status = StatusCode(kStatusCategoryBlocked, kStatusResearchDone);
                *out = kOutResearchDone;
            }
        }
    }
    return result;
}

int MissionResearch::Virtual157(GameObj* person, int* out) {
    *out = 0;
    return 1;
}

int MissionResearch::Virtual158(NotifyContext* ctx) {
    const int pointsCleared = SetRdPoints(0, ctx);
    const int ordersCleared = SetRdOrders(0, ctx);
    return ordersCleared != 0 && pointsCleared != 0 ? 1 : 0;
}

int MissionResearch::Virtual159(GameObj* person, int roll, int* out, NotifyContext* ctx) {
    *out = 0;
    System* system = nullptr;
    const int found = static_cast<MissionSystemTargetBase*>(this)->MissionSystemTargetBase::GetObjectiveSystem(reinterpret_cast<GameObj**>(&system));
    int kind = 0;
    int ok = GetResearchKind(&kind) != 0 && found != 0;
    if (!system) return ok;
    Role* role = static_cast<Role*>(person);
    int skill = 0;
    ok = role->GetEffectiveRdForKind(kind, &skill) != 0 && ok;
    int chance = 0;
    ok = CopyMissionValue(skill, &chance) != 0 && ok;
    if (!PercentChance(chance)) return ok;
    int bonus = 0;
    ok = RollResearchBonus(&bonus) != 0 && ok;
    if (bonus == 0) return ok;
    ok = SetRdPoints(m_rdPoints + bonus, ctx) != 0 && ok;
    if (!role->Virtual118()) return ok;
    int baseSkill = 0;
    ok = role->GetBaseRdForKind(kind, &baseSkill) != 0 && ok;
    ok = role->SetBaseRdForKind(kind, baseSkill + g_gnprtb180E, ctx) != 0 && ok;
    return ok;
}

int MissionResearch::Virtual160(NotifyContext* ctx) {
    const int sideIndex = ControlSideAsSideIndex(ControlSide());
    Side* sideObject = sideIndex ? static_cast<Side*>(GetGameRootSideMemberThunk(sideIndex)) : nullptr;
    int ok = sideObject != nullptr;
    if (!sideObject) return ok;
    int kind = 0;
    ok = GetResearchKind(&kind) != 0 && ok;
    int ordersBefore = 0;
    ok = sideObject->GetRdOrder(kind, &ordersBefore) != 0 && ok;
    int progress = 0;
    ok = sideObject->GetRdProgress(kind, &progress) != 0 && ok;
    ok = sideObject->SetRdProgress(kind, progress + m_rdPoints, ctx) != 0 && ok;
    int ordersAfter = 0;
    ok = sideObject->GetRdOrder(kind, &ordersAfter) != 0 && ok;
    ok = SetRdOrders(ordersAfter - ordersBefore, ctx) != 0 && ok;
    ok = SetTaskStatus((m_rdPoints != 0) + kTaskStarted, ctx) != 0 && ok;
    return ok;
}

int MissionResearch::BuildEventKinds() {
    if (!g_missionResearchEventKindsReady) {
        g_missionResearchEventKindsReady = 1;
        return MissionSystemTargetBase::InitEventKinds(0, &g_missionResearchEventKinds);
    }
    return 1;
}

void ConstructMissionResearchEventKinds() { new (&g_missionResearchEventKinds) EventKindSet(); }

void RegisterMissionResearchEventKindsAtExit() { atexit(DestroyMissionResearchEventKinds); }

void MissionResearch::InitStatics() {
    ConstructMissionResearchEventKinds();
    RegisterMissionResearchEventKindsAtExit();
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_missionResearchEventKindsDestroyedGuard;  // 0x6bb980

void DestroyMissionResearchEventKinds() {
    if (!(g_missionResearchEventKindsDestroyedGuard & 0x1)) {
        g_missionResearchEventKindsDestroyedGuard |= 0x1;
        g_missionResearchEventKinds.~EventKindSet();
    }
}
