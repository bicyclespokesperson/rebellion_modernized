#include "fleet/combat_unit.h"

#include <new>

#include "core/utility_classes.h"
#include "data/sd_record_bases.h"
#include "events/event_post.h"
#include "fleet/unit_data_decl.h"
#include "fleet/unit_view_flags.h"


extern int g_combatUnitEventKindsReady;      // 0x6bb358
extern EventKindSet g_combatUnitEventKinds;  // 0x6bb350


namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kApplyEverything = 1;
constexpr int kRepairStatEvent = 0x380;
constexpr uint32_t kUsableFlag = 0x1;
constexpr uint32_t kDamagedFlag = 0x200;
constexpr int kKindMoveOrder = 0x201;
constexpr int kKindOrder202 = 0x202;
constexpr int kKindOrder204 = 0x204;
constexpr int kKindOrder200 = 0x200;

SdRecordGrid* gridRecordOf(CombatUnit* unit) { return static_cast<SdRecordGrid*>(unit->m_record); }
}  // namespace

CombatUnit::CombatUnit() { m_combatFlags = 0; }

CombatUnit::~CombatUnit() = default;

int CombatUnit::GetEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    if (ready) *out = g_combatUnitEventKinds;
    return ready;
}

int CombatUnit::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int ready = EnsureEventKinds();
    if (ready && g_combatUnitEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return ready;
}

int CombatUnit::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

int CombatUnit::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

GameObjData* CombatUnit::CreateData() { return new CombatUnitData(); }

int CombatUnit::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) {
    return Detector::OnParentChanged(oldParent, newParent, ctx);
}

int CombatUnit::OnUsableChanged(int newValue, NotifyContext* ctx) {
    int result = Detector::OnUsableChanged(newValue, ctx);
    if (result) result = UpdateUnderRepair(ctx);
    return result;
}

int CombatUnit::OnDestroyedChanged(int newValue, NotifyContext* ctx) {
    return Detector::OnDestroyedChanged(newValue, ctx);
}

int CombatUnit::OnExistingChanged(int newValue, NotifyContext* ctx) {
    return Detector::OnExistingChanged(newValue, ctx);
}

int CombatUnit::OnDamagedChanged(int newValue, NotifyContext* ctx) {
    int result = Detector::OnDamagedChanged(newValue, ctx);
    if (result) result = UpdateUnderRepair(ctx);
    return result;
}

int CombatUnit::CreateViews(int controlKind, NotifyContext* ctx) {
    const int result = GameObj::CreateViews(controlKind, ctx);
    if (result) static_cast<CombatUnitData*>(m_data)->m_repairStat.SetLevel(GetRepairLevel());
    return result;
}

int CombatUnit::Read(istream* in) {
    const int result = Detector::Read(in);
    if (result) ReadValue32(in, &m_combatFlags);
    return result;
}

int CombatUnit::Write(ostream* out) {
    const int result = Detector::Write(out);
    if (result) WriteValue32(out, &m_combatFlags);
    return result;
}

int CombatUnit::Checksum(Crc32* crc) {
    const int result = Detector::Checksum(crc);
    if (result) crc->AddInt(m_combatFlags);
    return result;
}

int CombatUnit::Validate() { return Detector::Validate(); }

int CombatUnit::ApplyAll(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) result = Detector::ApplyAll(ctx);
    if (result && ctx->m_flag04 == kApplyEverything) {
        ApplyUnderRepair(ctx);
        ApplyFastRepair(ctx);
    }
    return result;
}

uint32_t CombatUnit::GetField60() { return m_record ? gridRecordOf(this)->m_field60 : 0; }

uint32_t CombatUnit::GetField64() { return m_record ? gridRecordOf(this)->m_field64 : 0; }

uint32_t CombatUnit::GetField68() { return m_record ? gridRecordOf(this)->m_field68 : 0; }

uint32_t CombatUnit::GetField6c() { return m_record ? gridRecordOf(this)->m_field6c : 0; }

uint32_t CombatUnit::GetField70() { return m_record ? gridRecordOf(this)->m_field70 : 0; }

uint32_t CombatUnit::GetGridTotal() { return GetRecordTotal(); }

uint32_t CombatUnit::GetGridColumnTotal(int column) { return GetRecordColumnTotal(column); }

uint32_t CombatUnit::GetGridValue(int row, int column) { return GetRecordGridValue(row, column); }

uint32_t CombatUnit::GetGridFieldA4(int column) { return GetRecordFieldA4(column); }

uint32_t CombatUnit::GetRecordGridValue(int row, int column) {
    return m_record ? gridRecordOf(this)->GetGridValue(row, column) : 0;
}

uint32_t CombatUnit::GetRecordFieldA4(int column) { return m_record ? gridRecordOf(this)->GetFieldA4(column) : 0; }

uint32_t CombatUnit::GetRecordColumnTotal(int column) {
    return m_record ? gridRecordOf(this)->GetColumnTotal(column) : 0;
}

uint32_t CombatUnit::GetRecordTotal() { return m_record ? gridRecordOf(this)->m_total : 0; }

int CombatUnit::OnUnderRepairChanged(int newValue, NotifyContext* ctx) {
    return ArmTimerAfterSetFlag(kRepairStatEvent, newValue, this, &static_cast<CombatUnitData*>(m_data)->m_repairStat, ctx);
}

int CombatUnit::OnFastRepairChanged(int newValue, NotifyContext* ctx) {
    return ArmTimerAfterSetLevel(kRepairStatEvent, GetRepairLevel(), this, &static_cast<CombatUnitData*>(m_data)->m_repairStat,
                        ctx);
}

int CombatUnit::OnUnderRepairNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CombatUnitUnderRepairNotif)", "UnderRepair", value, 0, ctx);
    return result;
}

int CombatUnit::OnFastRepairNotified(int value, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogFlag("(CombatUnitFastRepairNotif)", "FastRepair", value, 0, ctx);
    return result;
}

int CombatUnit::SetUnderRepair(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(kUnderRepairFlag, on, &m_combatFlags)) {
        static_cast<CombatUnit*>(GetViewObject(1))->ApplyUnderRepair(ctx);
        static_cast<CombatUnit*>(GetViewObject(2))->ApplyUnderRepair(ctx);
        OnUnderRepairChanged(on, ctx);
    }
    return result;
}

int CombatUnit::SetFastRepair(int on, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && UpdateFlagBit(kFastRepairFlag, on, &m_combatFlags)) {
        static_cast<CombatUnit*>(GetViewObject(1))->ApplyFastRepair(ctx);
        static_cast<CombatUnit*>(GetViewObject(2))->ApplyFastRepair(ctx);
        OnFastRepairChanged(on, ctx);
    }
    return result;
}

int CombatUnit::UpdateUnderRepair(NotifyContext* ctx) {
    const bool damagedAndUsable = (m_stateFlags & kDamagedFlag) != 0 && (m_stateFlags & kUsableFlag) != 0;
    return SetUnderRepair(damagedAndUsable ? 1 : 0, ctx);
}

int CombatUnit::ApplyUnderRepair(NotifyContext* ctx) {
    return fleet::applyFlagToView(this, &CombatUnit::m_combatFlags, kUnderRepairFlag, ctx,
                                  [&](int value) { return OnUnderRepairNotified(value, ctx); });
}

int CombatUnit::ApplyFastRepair(NotifyContext* ctx) {
    return fleet::applyFlagToView(this, &CombatUnit::m_combatFlags, kFastRepairFlag, ctx,
                                  [&](int value) { return OnFastRepairNotified(value, ctx); });
}

int CombatUnit::EnsureEventKinds() {
    if (g_combatUnitEventKindsReady != 0) return 1;
    g_combatUnitEventKindsReady = 1;
    return Detector::InitEventKinds(0, &g_combatUnitEventKinds) && g_combatUnitEventKinds.Add(kKindMoveOrder, 1) &&
           g_combatUnitEventKinds.Add(kKindOrder202, 1) && g_combatUnitEventKinds.Add(kKindOrder204, 1) &&
           g_combatUnitEventKinds.Add(kKindOrder200, 1);
}

int CombatUnit::InitEventKinds(int unused, EventKindSet* out) {
    const int ready = EnsureEventKinds();
    *out = g_combatUnitEventKinds;
    return ready;
}
EventKindSet g_combatUnitEventKinds{};
int g_combatUnitEventKindsReady{};
