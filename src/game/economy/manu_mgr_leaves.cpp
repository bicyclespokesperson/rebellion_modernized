#include "economy/manu_mgr_leaves.h"

#include <new>

#include "gameobj/object_family.h"


extern EventKindSet g_trainingFacilManuMgrEventKinds;  // 0x6bb4d0
extern int g_trainingFacilManuMgrEventKindsReady;  // 0x6bb4d8
extern EventKindSet g_constructionYardManuMgrEventKinds;  // 0x6bb4e8
extern int g_constructionYardManuMgrEventKindsReady;  // 0x6bb4e0
extern EventKindSet g_shipyardManuMgrEventKinds;  // 0x6bb500
extern int g_shipyardManuMgrEventKindsReady;  // 0x6bb4f8

namespace {
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kDefaultEventKindValue = 1;
constexpr FamilyRange kTrainingFacilManuMgrFamilies(0xa4, 0xa6);
constexpr FamilyRange kConstructionYardManuMgrFamilies(0xa0, 0xa2);
constexpr FamilyRange kShipyardManuMgrFamilies(0xa2, 0xa4);

int applyAllOnSideView(ManuMgr* manager, NotifyContext* ctx) {
    if (manager->IsSideView()) return ManuMgrApplyAllBody(manager, ctx);
    return 0;
}
}  // namespace

// ---- shared bodies and stubs ----------------------------------------------------------------------------------------

int ManuMgrReadBody(ManuMgr* self, istream* in) { return self->ManuMgr::Read(in); }

int ManuMgrWriteBody(ManuMgr* self, ostream* out) { return self->ManuMgr::Write(out); }

int ManuMgrApplyAllBody(ManuMgr* self, NotifyContext* ctx) { return self->ManuMgr::ApplyAll(ctx); }

int ManuMgrChecksumBody(ManuMgr* self, Crc32* crc) { return self->ManuMgr::Checksum(crc); }

int CreateViewsStubShared(GameObj* self, int controlKind, NotifyContext* ctx) {
    return self->GameObj::CreateViews(controlKind, ctx);
}

int CreateViewsStubDefense(GameObj* self, int controlKind, NotifyContext* ctx) {
    return self->GameObj::CreateViews(controlKind, ctx);
}

// ---- TrainingFacilManuMgr (family 0xa4) -------------------------------------------------------------------------------------------

TrainingFacilManuMgr::TrainingFacilManuMgr() : ManuMgr() { SetFlag31(GetFlag31()); }

TrainingFacilManuMgr::~TrainingFacilManuMgr() {  }

int TrainingFacilManuMgr::Read(istream* in) { return ManuMgrReadBody(this, in); }

int TrainingFacilManuMgr::Write(ostream* out) { return ManuMgrWriteBody(this, out); }

int TrainingFacilManuMgr::Checksum(Crc32* crc) { return ManuMgrChecksumBody(this, crc); }

int TrainingFacilManuMgr::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int TrainingFacilManuMgr::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_trainingFacilManuMgrEventKinds;
    return result;
}

int TrainingFacilManuMgr::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_trainingFacilManuMgrEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int TrainingFacilManuMgr::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubShared(this, controlKind, ctx); }

GameObj* TrainingFacilManuMgr::CreateViewObject() { return new TrainingFacilManuMgr(); }

int TrainingFacilManuMgr::BuildEventKinds() {
    if (g_trainingFacilManuMgrEventKindsReady != 0) return 1;
    g_trainingFacilManuMgrEventKindsReady = 1;
    const int result = ManuMgr::InitEventKinds(0, &g_trainingFacilManuMgrEventKinds) &&
           g_trainingFacilManuMgrEventKinds.Add(0x212, kDefaultEventKindValue);
    g_trainingFacilManuMgrEventKindsReady = 1;
    return result;
}


GameObjNotifier* CreateTrainingFacilManuMgr() { return new TrainingFacilManuMgr(); }

int CountTrainingFacilManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kTrainingFacilManuMgrFamilies, side, filter);
}

// ---- ConstructionYardManuMgr (family 0xa0) -------------------------------------------------------------------------------------------

ConstructionYardManuMgr::ConstructionYardManuMgr() : ManuMgr() { SetFlag31(GetFlag31()); }

ConstructionYardManuMgr::~ConstructionYardManuMgr() {  }

int ConstructionYardManuMgr::Read(istream* in) { return ManuMgrReadBody(this, in); }

int ConstructionYardManuMgr::Write(ostream* out) { return ManuMgrWriteBody(this, out); }

int ConstructionYardManuMgr::Checksum(Crc32* crc) { return ManuMgrChecksumBody(this, crc); }

int ConstructionYardManuMgr::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int ConstructionYardManuMgr::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_constructionYardManuMgrEventKinds;
    return result;
}

int ConstructionYardManuMgr::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_constructionYardManuMgrEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int ConstructionYardManuMgr::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubShared(this, controlKind, ctx); }

GameObj* ConstructionYardManuMgr::CreateViewObject() { return new ConstructionYardManuMgr(); }

int ConstructionYardManuMgr::BuildEventKinds() {
    if (g_constructionYardManuMgrEventKindsReady != 0) return 1;
    g_constructionYardManuMgrEventKindsReady = 1;
    const int result = ManuMgr::InitEventKinds(0, &g_constructionYardManuMgrEventKinds) &&
           g_constructionYardManuMgrEventKinds.Add(0x210, kDefaultEventKindValue);
    g_constructionYardManuMgrEventKindsReady = 1;
    return result;
}


GameObjNotifier* CreateConstructionYardManuMgr() { return new ConstructionYardManuMgr(); }

int CountConstructionYardManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kConstructionYardManuMgrFamilies, side, filter);
}

// ---- ShipyardManuMgr (family 0xa2) -------------------------------------------------------------------------------------------

ShipyardManuMgr::ShipyardManuMgr() : ManuMgr() { SetFlag31(GetFlag31()); }

ShipyardManuMgr::~ShipyardManuMgr() {  }

int ShipyardManuMgr::Read(istream* in) { return ManuMgrReadBody(this, in); }

int ShipyardManuMgr::Write(ostream* out) { return ManuMgrWriteBody(this, out); }

int ShipyardManuMgr::Checksum(Crc32* crc) { return ManuMgrChecksumBody(this, crc); }

int ShipyardManuMgr::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int ShipyardManuMgr::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_shipyardManuMgrEventKinds;
    return result;
}

int ShipyardManuMgr::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_shipyardManuMgrEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

int ShipyardManuMgr::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubShared(this, controlKind, ctx); }

GameObj* ShipyardManuMgr::CreateViewObject() { return new ShipyardManuMgr(); }

int ShipyardManuMgr::BuildEventKinds() {
    if (g_shipyardManuMgrEventKindsReady != 0) return 1;
    g_shipyardManuMgrEventKindsReady = 1;
    const int result = ManuMgr::InitEventKinds(0, &g_shipyardManuMgrEventKinds) &&
           g_shipyardManuMgrEventKinds.Add(0x215, kDefaultEventKindValue) &&
           g_shipyardManuMgrEventKinds.Add(0x211, kDefaultEventKindValue);
    g_shipyardManuMgrEventKindsReady = 1;
    return result;
}


GameObjNotifier* CreateShipyardManuMgr() { return new ShipyardManuMgr(); }

int CountShipyardManuMgrChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kShipyardManuMgrFamilies, side, filter);
}

// ---- shipyard slots 129 and 132 -------------------------------------------------------------------------------------

int ShipyardManuMgr::OnRemainingGameObjCountChanged(int oldValue, int newValue, NotifyContext* ctx) {
    return ManuMgr::OnRemainingGameObjCountChanged(oldValue, newValue, ctx);
}

int ShipyardManuMgr::OnSeedKeyChanged(NullableKey* oldKey, NullableKey* newKey, NotifyContext* ctx) {
    return ManuMgr::OnSeedKeyChanged(oldKey, newKey, ctx);
}
EventKindSet g_constructionYardManuMgrEventKinds{};
EventKindSet g_shipyardManuMgrEventKinds{};
EventKindSet g_trainingFacilManuMgrEventKinds{};
int g_constructionYardManuMgrEventKindsReady{};
int g_shipyardManuMgrEventKindsReady{};
int g_trainingFacilManuMgrEventKindsReady{};
