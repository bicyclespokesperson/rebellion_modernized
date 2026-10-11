#include "economy/defense_facility.h"

#include <new>

#include "economy/manu_mgr_leaves.h"
#include "gameobj/game_obj_forwarders.h"
#include "gameobj/object_family.h"


extern EventKindSet g_allFacilityEventKinds;  // 0x6b8ff8
extern int g_allFacilityEventKindsReady;      // 0x6b9000

namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNoSuchOrder = 1;
constexpr int kKindAllFacility1 = 0x201;
constexpr int kKindAllFacility2 = 0x202;
constexpr int kKindAllFacility3 = 0x204;
constexpr int kDestroyedReasonParentLost = 4;
constexpr uint32_t kAllFacilityFamily = 0x20;

constexpr FamilyRange kAllFacilityFamilies(0x20, 0x22);
constexpr FamilyRange kDefenseFacilityFamilies(0x22, 0x28);
constexpr FamilyRange kDefenseFacility22Families(0x22, 0x23);
constexpr FamilyRange kDefenseFacility23Families(0x23, 0x24);
constexpr FamilyRange kDefenseFacility24Families(0x24, 0x25);
constexpr FamilyRange kDefenseFacility25Families(0x25, 0x26);

DefenseFacilitySD* defenseRecordOf(DefenseFacility* facility) { return static_cast<DefenseFacilitySD*>(facility->m_record); }
}  // namespace

// ---- AllFacility ----------------------------------------------------------------------------------------------------

AllFacility::AllFacility() { SetId(5); }

AllFacility::~AllFacility() {  }

uint32_t AllFacility::GetFamily() { return kAllFacilityFamily; }

int AllFacility::Read(istream* in) { return ReadStep(in); }

int AllFacility::Write(ostream* out) { return WriteStep(out); }

int AllFacility::Checksum(Crc32* crc) { return ChecksumStep(crc); }

int AllFacility::ApplyAll(NotifyContext* ctx) {
    if (IsSideView()) return ApplyAllStep(ctx);
    return 0;
}

int AllFacility::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_allFacilityEventKinds;
    return result;
}

int AllFacility::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_allFacilityEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int AllFacility::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    return 1;
}

int AllFacility::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (!result) *out = StatusCode(kStatusCategory, kStatusNoSuchOrder);
    return result;
}

int AllFacility::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubDefense(this, controlKind, ctx); }

GameObj* AllFacility::CreateViewObject() { return new AllFacility(); }

int AllFacility::ReadStep(istream* in) { return LeafReadForward(this, in); }

int AllFacility::WriteStep(ostream* out) { return LeafWriteForward(this, out); }

int AllFacility::ChecksumStep(Crc32* crc) { return LeafChecksumForward(this, crc); }

int AllFacility::ApplyAllStep(NotifyContext* ctx) { return ApplyAllImpl(ctx); }

int AllFacility::BuildEventKinds() {
    if (g_allFacilityEventKindsReady != 0) return 1;
    g_allFacilityEventKindsReady = 1;
    return UnitObjBase::InitEventKinds(0, &g_allFacilityEventKinds) && g_allFacilityEventKinds.Add(kKindAllFacility1, 1) &&
           g_allFacilityEventKinds.Add(kKindAllFacility2, 1) && g_allFacilityEventKinds.Add(kKindAllFacility3, 1);
}


GameObjNotifier* CreateAllFacility() { return new AllFacility(); }

int CountAllFacilities(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kAllFacilityFamilies, side, filter);
}

AllFacilityIterator::AllFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kAllFacilityFamilies, stateFilter) {}

AllFacilityIterator::AllFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kAllFacilityFamilies, stateFilter) {}

AllFacilityIterator::~AllFacilityIterator() {  }

// ---- DefenseFacility ------------------------------------------------------------------------------------------------

DefenseFacility::DefenseFacility() {}

DefenseFacility::~DefenseFacility() {  }

int DefenseFacility::Read(istream* in) { return LeafReadForward(this, in); }

int DefenseFacility::Write(ostream* out) { return LeafWriteForward(this, out); }

int DefenseFacility::Checksum(Crc32* crc) { return LeafChecksumForward(this, crc); }

int DefenseFacility::ApplyAll(NotifyContext* ctx) {
    if (IsSideView()) return ApplyAllLeafStep(ctx);
    return 0;
}

int DefenseFacility::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubDefense(this, controlKind, ctx); }

int DefenseFacility::Virtual46(NotifyContext* ctx) {
    GameObjNotifier* parent;
    GetParent(&parent);
    return ChangeDestroyedReason(kDestroyedReasonParentLost, ctx);
}

int DefenseFacility::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    return Facility::OnControlKindChanged(oldKind, newKind, ctx);
}

int DefenseFacility::OnUsableChanged(int newValue, NotifyContext* ctx) { return Facility::OnUsableChanged(newValue, ctx); }

int DefenseFacility::Virtual111() { return 3; }

uint32_t DefenseFacility::GetAttackStrength() { return m_record ? defenseRecordOf(this)->m_attackStrength : 0; }

uint32_t DefenseFacility::GetShieldStrength() { return m_record ? defenseRecordOf(this)->m_shieldStrength : 0; }

int DefenseFacility::ApplyAllLeafStep(NotifyContext* ctx) { return ApplyAllBaseStep(ctx); }

int DefenseFacility::ApplyAllBaseStep(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) ApplyAllImpl(ctx);
    return result;
}

int CountDefenseFacilities(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kDefenseFacilityFamilies, side, filter);
}

DefenseFacilityIterator::DefenseFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDefenseFacilityFamilies, stateFilter) {}

DefenseFacilityIterator::DefenseFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kDefenseFacilityFamilies, stateFilter) {}

DefenseFacilityIterator::~DefenseFacilityIterator() {  }

// ---- leaves ---------------------------------------------------------------------------------------------------------

DefenseFacility22::DefenseFacility22() {}
DefenseFacility22::~DefenseFacility22() {  }
uint32_t DefenseFacility22::GetFamily() { return 0x22; }
GameObj* DefenseFacility22::CreateViewObject() { return new DefenseFacility22(); }
GameObjNotifier* CreateDefenseFacility22() { return new DefenseFacility22(); }
int CountDefenseFacilities22(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kDefenseFacility22Families, side, filter);
}
DefenseFacility22Iterator::DefenseFacility22Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDefenseFacility22Families, stateFilter) {}
DefenseFacility22Iterator::DefenseFacility22Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kDefenseFacility22Families, stateFilter) {}
DefenseFacility22Iterator::~DefenseFacility22Iterator() {  }

DefenseFacility23::DefenseFacility23() {}
DefenseFacility23::~DefenseFacility23() {  }
uint32_t DefenseFacility23::GetFamily() { return 0x23; }
GameObj* DefenseFacility23::CreateViewObject() { return new DefenseFacility23(); }
GameObjNotifier* CreateDefenseFacility23() { return new DefenseFacility23(); }
int CountDefenseFacilities23(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kDefenseFacility23Families, side, filter);
}
DefenseFacility23Iterator::DefenseFacility23Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDefenseFacility23Families, stateFilter) {}
DefenseFacility23Iterator::~DefenseFacility23Iterator() {  }

DefenseFacility24::DefenseFacility24() {}
DefenseFacility24::~DefenseFacility24() {  }
uint32_t DefenseFacility24::GetFamily() { return 0x24; }
GameObj* DefenseFacility24::CreateViewObject() { return new DefenseFacility24(); }
GameObjNotifier* CreateDefenseFacility24() { return new DefenseFacility24(); }
int CountDefenseFacilities24(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kDefenseFacility24Families, side, filter);
}
DefenseFacility24Iterator::DefenseFacility24Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDefenseFacility24Families, stateFilter) {}
DefenseFacility24Iterator::DefenseFacility24Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kDefenseFacility24Families, stateFilter) {}
DefenseFacility24Iterator::~DefenseFacility24Iterator() {  }

DefenseFacility25::DefenseFacility25() {}
DefenseFacility25::~DefenseFacility25() {  }
uint32_t DefenseFacility25::GetFamily() { return 0x25; }
GameObj* DefenseFacility25::CreateViewObject() { return new DefenseFacility25(); }
GameObjNotifier* CreateDefenseFacility25() { return new DefenseFacility25(); }
int CountDefenseFacilities25(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kDefenseFacility25Families, side, filter);
}
DefenseFacility25Iterator::DefenseFacility25Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDefenseFacility25Families, stateFilter) {}
DefenseFacility25Iterator::~DefenseFacility25Iterator() {  }
EventKindSet g_allFacilityEventKinds{};
int g_allFacilityEventKindsReady{};
