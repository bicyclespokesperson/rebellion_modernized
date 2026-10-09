#include "core/reb_abi.h"
#include "core/checked_new.h"
#include "ai/ai_common.h"
#include "core/vptr_store.h"
#include "events/event_post.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "mission/mission_external.h"
#include "mission/mission_rules.h"
#include "story/mission_mgr.h"
#include "story/story_common.h"

void DestroyMissionMgrEventKinds();
extern "C" int REB_CDECL atexit(void(REB_CDECL* function)(void));
extern "C" void* REB_CDECL operator_new(size_t size);

extern int g_missionMgrEventKindsReady;      // 0x6b92e4
extern EventKindSet g_missionMgrEventKinds;  // 0x6b92e8

namespace {

constexpr int kTimerStatA = 0x392;
constexpr int kTimerStatB = 0x393;
constexpr uint32_t kViewSideMask = 0x30;
constexpr uint32_t kFamilyMissionTypeBegin = 0x40;
constexpr uint32_t kFamilyMissionTypeEnd = 0x80;
constexpr int kEventLukeDagobah = 0x221;
constexpr int kEventFinalBattle = 0x220;
constexpr size_t kTypeTableHeader = alignof(MissionTypeCheckEntry) > 4 ? alignof(MissionTypeCheckEntry) : 4;  // keeps the entries aligned

bool isMaster(const GameObjRoot* object) { return (object->m_flags & kViewSideMask) == 0; }

DatRecord* nextRecord(DatRecord* record) { return reinterpret_cast<DatRecord*>(static_cast<AvlNode*>(record)->m_successor); }

}  // namespace

// ---- construction -----------------------------------------------------------------------------------------------------

MissionMgr::MissionMgr() : m_storyFlags(0) {
    SetId(6);
    ClearTypeTable();
}

MissionMgr::~MissionMgr() {
    FreeTypeTable();
    REB_KEEP_VPTR_STORE();
}

uint32_t MissionMgr::GetFamily() { return family::kMissionManager; }

int MissionMgr::GetOwnerKey(ObjectKey* out) {
    out->Reset();
    return 1;
}

GameObj* MissionMgr::CreateViewObject() { return NewOrNull<MissionMgr>(); }

GameObjNotifier* MissionMgr::Create() { return NewOrNull<MissionMgr>(); }

int MissionMgr::UpdateObservedByAlliance(NotifyContext* ctx) { return SetObservedByAlliance(1, ctx) != 0 ? 1 : 0; }

int MissionMgr::UpdateObservedByEmpire(NotifyContext* ctx) { return SetObservedByEmpire(1, ctx) != 0 ? 1 : 0; }

// ---- the mission-type table -------------------------------------------------------------------------------------------

int MissionMgr::ClearTypeTable() {
    m_typeCount = 0;
    m_typeTable = nullptr;
    return 1;
}

int MissionMgr::FreeTypeTable() {
    m_typeCount = 0;
    if (m_typeTable) {
        ::operator delete(reinterpret_cast<char*>(m_typeTable) - kTypeTableHeader);
        m_typeTable = nullptr;
    }
    return 1;
}

int MissionMgr::BuildTypeTable() {
    const int cleared = FreeTypeTable();
    if (cleared) {
        for (DatRecord* record = GetFirstRecordOfType(kFamilyMissionTypeBegin); record; record = nextRecord(record)) ++m_typeCount;
        if (cleared && m_typeCount != 0) {
            const int count = m_typeCount;
            void* block = ::operator new(count * sizeof(MissionTypeCheckEntry) + kTypeTableHeader);
            MissionTypeCheckEntry* entries = nullptr;
            if (block) {
                *static_cast<int*>(block) = count;
                entries = reinterpret_cast<MissionTypeCheckEntry*>(static_cast<char*>(block) + kTypeTableHeader);
                for (int i = 0; i < count; ++i) new (&entries[i].m_recordKey) ObjectKey();
            }
            m_typeTable = entries;
            if (entries) {
                DatRecord* record = GetFirstRecordOfType(kFamilyMissionTypeBegin);
                if (!record) return 1;
                bool ok = true;
                MissionTypeCheckEntry* entry = entries;
                do {
                    ObjectKey recordKey;
                    GameObj* mission = static_cast<GameObj*>(CreateObjectForKey(GetAiRecordKey(record, &recordKey)));
                    ok = mission != nullptr && ok;
                    if (mission) {
                        ObjectKey storedKey;
                        entry->m_recordKey = *GetAiRecordKey(record, &storedKey);
                        const int reported = static_cast<Mission*>(mission)->GetCheckFunctions(
                            reinterpret_cast<MissionCheckFunctions*>(&entry->m_checkMember));
                        ok = reported != 0 && ok;
                        delete mission;
                    }
                    record = nextRecord(record);
                    ++entry;
                } while (record);
                return 1;
            }
            m_typeCount = 0;
        }
    }
    FreeTypeTable();
    return 1;
}

int MissionMgr::FindTypeChecks(const ObjectKey* typeKey, MissionCheckFunctions* out) {
    bool known = false;
    if (m_typeCount != 0) known = typeKey->Family() >= kFamilyMissionTypeBegin && typeKey->Family() < kFamilyMissionTypeEnd;
    int found = 0;
    if (known) {
        for (int i = 0; i < m_typeCount && !found; ++i) {
            const MissionTypeCheckEntry& entry = m_typeTable[i];
            if (entry.m_recordKey.m_value == typeKey->m_value) {
                found = 1;
                out->m_unk00 = entry.m_checkMember;
                out->m_unk04 = entry.m_checkTarget;
            }
        }
    }
    return found;
}

// ---- persistence ------------------------------------------------------------------------------------------------------

int MissionMgr::Read(istream* in) { return ReadBody(in); }

int MissionMgr::Write(ostream* out) { return WriteBody(out); }

int MissionMgr::Checksum(Crc32* crc) { return ChecksumBody(crc); }

int MissionMgr::Validate() { return ValidateBody(); }

int MissionMgr::ApplyAll(NotifyContext* ctx) { return IsSideView() ? ApplyAllBody(ctx) : 0; }

int MissionMgr::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsBody(controlKind, ctx); }

int MissionMgr::ReadBody(istream* in) {
    int result = GameObj::Read(in);
    if (!result) return result;
    ReadValue32(in, &m_storyFlags);
    result = 0;
    if (m_statA.Read(in)) {
        result = m_statB.Read(in);
        if (result && isMaster(this)) result = BuildTypeTable();
    }
    return result;
}

int MissionMgr::WriteBody(ostream* out) {
    int result = GameObj::Write(out);
    if (!result) return result;
    WriteValue32(out, &m_storyFlags);
    result = 0;
    if (m_statA.Write(out)) result = m_statB.Write(out);
    return result;
}

int MissionMgr::ChecksumBody(Crc32* crc) {
    int result = GameObj::Checksum(crc);
    if (!result) return result;
    crc->AddInt(static_cast<int>(m_storyFlags));
    result = 0;
    if (m_statA.Checksum(crc)) result = m_statB.Checksum(crc);
    return result;
}

int MissionMgr::ApplyAllBody(NotifyContext* ctx) {
    const int result = GameObj::ApplyAll(ctx);
    if (result) {
        GameObj::ApplyAll(ctx);
        if (ctx->m_flag04 == 1) {
            ApplyLukeDagobahRequired(ctx);
            ApplyLukeDagobah(ctx);
            ApplyLukePalace(ctx);
            ApplyHanCapturedAtPalace(ctx);
            ApplyLeiaPalace(ctx);
            ApplyChewbaccaPalace(ctx);
            ApplyDarthPickup(ctx);
            ApplyDarthToLukeFinalBattle(ctx);
            ApplyDarthToEmperorFinalBattle(ctx);
            ApplyFinalBattleReady(ctx);
            ApplyFinalBattle(ctx);
            ApplyBountyHuntersActive(ctx);
        }
    }
    return result;
}

int MissionMgr::ValidateBody() {
    int result = GameObj::Validate();
    if (!result) return result;
    const FamilyRange persons(0x30, 0x40);
    for (GameObjNotifier* node = GetFirstMasterInFamilies_30_40(); node;) {
        Role* person = static_cast<Role*>(node);
        ObjectKey missionKey(person->m_missionKey);
        if (missionKey.OrNull()) {
            Mission* mission = static_cast<Mission*>(GetMasterInFamilies_40_80(&missionKey));
            result = mission != nullptr;
            if (result) {
                ObjectKey personKey;
                result = mission->IsMember(person->GetKey(&personKey));
            }
        }
        node = static_cast<GameObj*>(person)->FindNextSibling(&persons, 1);
        if (!result) break;
    }
    return result;
}

int MissionMgr::CreateViewsBody(int controlKind, NotifyContext* ctx) {
    int ok = GameObj::CreateViews(controlKind, ctx) ? 1 : 0;
    if (isMaster(this)) ok = BuildTypeTable() && ok ? 1 : 0;

    m_statA.SetLevel(g_gnprtb1802);
    m_statA.SetProgress(g_gnprtb1803);
    NotifyContext armContext;
    armContext = *ctx;
    armContext.m_reserved14 = CallGameHook2AFC();
    ok = ArmTimerAfterSetFlag(kTimerStatA, 1, this, &m_statA, &armContext) && ok ? 1 : 0;

    m_statB.SetLevel(g_gnprtb1804);
    m_statB.SetProgress(g_gnprtb1805);
    armContext = *ctx;
    armContext.m_reserved14 = CallGameHook2AFC();
    return SetBountyHuntersActive(1, &armContext) && ok ? 1 : 0;
}

// ---- event kinds ------------------------------------------------------------------------------------------------------

int MissionMgr::BuildEventKinds() {
    return story::buildKindsOnce(&g_missionMgrEventKindsReady, [] { return InitEventKinds(0, &g_missionMgrEventKinds); });
}

int MissionMgr::GetEventKinds(int unused, EventKindSet* out) {
    return story::copyKinds(BuildEventKinds(), g_missionMgrEventKinds, out);
}

int MissionMgr::CheckOrder(int a, int orderKind, StatusCode* out) {
    return story::checkOrder(BuildEventKinds(), g_missionMgrEventKinds, orderKind, out);
}

void ConstructMissionMgrEventKinds() { new (&g_missionMgrEventKinds) EventKindSet(); }

void RegisterMissionMgrEventKindsAtExit() { atexit(DestroyMissionMgrEventKinds); }

void MissionMgr::InitStatics() {
    ConstructMissionMgrEventKinds();
    RegisterMissionMgrEventKindsAtExit();
}

// ---- the story flag properties ----------------------------------------------------------------------------------------

#define REB_DEFINE_STORY_FLAG(NAME, MASK)                                                                                 \
    int MissionMgr::Set##NAME(int on, NotifyContext* ctx) {                                                                \
        return story::setFlagProperty(                                                                                    \
            this, &MissionMgr::m_storyFlags, MASK, on, [&](MissionMgr* view) { view->Apply##NAME(ctx); },                 \
            [&] { On##NAME##Changed(on, ctx); });                                                                         \
    }                                                                                                                     \
    int MissionMgr::Apply##NAME(NotifyContext* ctx) {                                                                      \
        return story::applyFlagProperty(this, &MissionMgr::m_storyFlags, MASK, ctx,                                       \
                                        [&](int value) { return On##NAME##Notified(value, ctx); });                       \
    }

REB_DEFINE_STORY_FLAG(LukeDagobahRequired, story_flag::kLukeDagobahRequired)
REB_DEFINE_STORY_FLAG(LukeDagobah, story_flag::kLukeDagobah)
REB_DEFINE_STORY_FLAG(LukePalace, story_flag::kLukePalace)
REB_DEFINE_STORY_FLAG(HanCapturedAtPalace, story_flag::kHanCapturedAtPalace)
REB_DEFINE_STORY_FLAG(LeiaPalace, story_flag::kLeiaPalace)
REB_DEFINE_STORY_FLAG(ChewbaccaPalace, story_flag::kChewbaccaPalace)
REB_DEFINE_STORY_FLAG(DarthPickup, story_flag::kDarthPickup)
REB_DEFINE_STORY_FLAG(DarthToLukeFinalBattle, story_flag::kDarthToLukeFinalBattle)
REB_DEFINE_STORY_FLAG(DarthToEmperorFinalBattle, story_flag::kDarthToEmperorFinalBattle)
REB_DEFINE_STORY_FLAG(FinalBattleReady, story_flag::kFinalBattleReady)
REB_DEFINE_STORY_FLAG(FinalBattle, story_flag::kFinalBattle)
REB_DEFINE_STORY_FLAG(BountyHuntersActive, story_flag::kBountyHuntersActive)

#undef REB_DEFINE_STORY_FLAG

int MissionMgr::OnBountyHuntersActiveChanged(int on, NotifyContext* ctx) {
    return ArmTimerAfterSetFlag(kTimerStatB, on, this, &m_statB, ctx) ? 1 : 0;
}

int MissionMgr::OnLukeDagobahRequiredNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrLukeDagobahRequiredNotif)", "LukeDagobahRequired", on, 0, ctx);
    return result;
}

int MissionMgr::OnLukeDagobahNotified(int on, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("MissionMgrLukeDagobahNotif", "LukeDagobah", on, 0, ctx);
        result = PostPayloadFlag(kEventLukeDagobah, this, on, ctx);
    }
    return result;
}

int MissionMgr::OnLukePalaceNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrLukePalaceNotif)", "LukePalace", on, 0, ctx);
    return result;
}

int MissionMgr::OnHanCapturedAtPalaceNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrHanCapturedAtPalaceNotif)", "HanCapturedAtPalace", on, 0, ctx);
    return result;
}

int MissionMgr::OnLeiaPalaceNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrLeiaPalaceNotif)", "LeiaPalace", on, 0, ctx);
    return result;
}

int MissionMgr::OnChewbaccaPalaceNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrChewbaccaPalaceNotif)", "ChewbaccaPalace", on, 0, ctx);
    return result;
}

int MissionMgr::OnDarthPickupNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrDarthPickupNotif)", "DarthPickup", on, 0, ctx);
    return result;
}

int MissionMgr::OnDarthToLukeFinalBattleNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrDarthToLukeFinalBattleNotif)", "DarthToLukeFinalBattle", on, 0, ctx);
    return result;
}

int MissionMgr::OnDarthToEmperorFinalBattleNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrDarthToEmperorFinalBattleNotif)", "DarthToEmperorFinalBattle", on, 0, ctx);
    return result;
}

int MissionMgr::OnFinalBattleReadyNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrFinalBattleReadyNotif)", "FinalBattleReady", on, 0, ctx);
    return result;
}

int MissionMgr::OnFinalBattleNotified(int on, NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        LogFlag("MissionMgrFinalBattleNotif", "FinalBattle", on, 0, ctx);
        result = PostPayloadFlag(kEventFinalBattle, this, on, ctx);
    }
    return result;
}

int MissionMgr::OnBountyHuntersActiveNotified(int on, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(MissionMgrBountyHuntersActiveNotif)", "BountyHuntersActive", on, 0, ctx);
    return result;
}

// ---- destructors registered with atexit ----------------------------------------------------

extern uint8_t g_missionMgrEventKindsDestroyedGuard;  // 0x6b92e0

void DestroyMissionMgrEventKinds() {
    if (!(g_missionMgrEventKindsDestroyedGuard & 0x1)) {
        g_missionMgrEventKindsDestroyedGuard |= 0x1;
        g_missionMgrEventKinds.~EventKindSet();
    }
}
