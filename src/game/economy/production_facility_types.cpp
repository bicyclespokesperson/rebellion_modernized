#include "economy/production_facility_types.h"

#include "core/checked_new.h"
#include "gameobj/object_family.h"
#include "mission/mission_rules.h"

namespace {

constexpr FamilyRange kType2DFamilies(0x2d, 0x2e);
constexpr FamilyRange kType2CFamilies(0x2c, 0x2d);
constexpr uint32_t kInitialWorkTotal = 0x32;

int createdChanged(ProductionFacility* facility, int newValue, NotifyContext* ctx) {
    const int result = facility->ProductionFacility::OnCreatedChanged(newValue, ctx);
    if (result) {
        const IntPair work(kInitialWorkTotal, 0);
        facility->SetWork(&work, ctx);
    }
    return result;
}

int computeDuration(ProductionFacility* facility, const IntPair* work, int modifier, int notStartupCycle, int* duration) {
    return ComputeProcessingDuration(static_cast<int>(facility->GetRecordField5c()), work, modifier, notStartupCycle, duration);
}

int applyAll(ProductionFacility* facility, NotifyContext* ctx) {
    int result = facility->IsSideView();
    if (result) result = ProductionFacilityApplyAllBody(facility, ctx);
    return result;
}

}  // namespace

int ProductionFacilityReadBody(ProductionFacility* self, istream* in) { return self->ReadImpl(in); }

int ProductionFacilityWriteBody(ProductionFacility* self, ostream* out) { return self->WriteImpl(out); }

int ProductionFacilityApplyAllBody(ProductionFacility* self, NotifyContext* ctx) { return self->ApplyAllImpl(ctx); }

int ProductionFacilityChecksumBody(ProductionFacility* self, Crc32* crc) { return self->ChecksumImpl(crc); }

// ---- family 0x2d ---------------------------------------------------------------------------------------------

ProductionFacilityType2D::ProductionFacilityType2D() {}

ProductionFacilityType2D::~ProductionFacilityType2D() {  }

int ProductionFacilityType2D::Read(istream* in) { return ProductionFacilityReadBody(this, in); }

int ProductionFacilityType2D::Write(ostream* out) { return ProductionFacilityWriteBody(this, out); }

int ProductionFacilityType2D::Checksum(Crc32* crc) { return ProductionFacilityChecksumBody(this, crc); }

int ProductionFacilityType2D::Validate() { return ValidateImpl(); }

int ProductionFacilityType2D::ApplyAll(NotifyContext* ctx) { return applyAll(this, ctx); }

int ProductionFacilityType2D::CreateViews(int controlKind, NotifyContext* ctx) { return GameObj::CreateViews(controlKind, ctx); }

GameObj* ProductionFacilityType2D::CreateViewObject() { return NewOrNull<ProductionFacilityType2D>(); }

int ProductionFacilityType2D::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    return ProductionFacility::OnControlKindChanged(oldKind, newKind, ctx);
}

int ProductionFacilityType2D::OnUsableChanged(int newValue, NotifyContext* ctx) {
    return ProductionFacility::OnUsableChanged(newValue, ctx);
}

int ProductionFacilityType2D::OnCreatedChanged(int newValue, NotifyContext* ctx) { return createdChanged(this, newValue, ctx); }

int ProductionFacilityType2D::CanProcess(const IntPair* work) { return 1; }

int ProductionFacilityType2D::ComputeDuration(const IntPair* work, int modifier, int notStartupCycle, int* duration) {
    return computeDuration(this, work, modifier, notStartupCycle, duration);
}

GameObjNotifier* CreateProductionFacilityType2D() { return NewOrNull<ProductionFacilityType2D>(); }

int CountProductionFacilityType2D(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kType2DFamilies, side, filter);
}

ProductionFacilityType2DIterator::ProductionFacilityType2DIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kType2DFamilies, stateFilter) {}

ProductionFacilityType2DIterator::ProductionFacilityType2DIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kType2DFamilies, stateFilter) {}

ProductionFacilityType2DIterator::~ProductionFacilityType2DIterator() {  }

// ---- family 0x2c ---------------------------------------------------------------------------------------------

ProductionFacilityType2C::ProductionFacilityType2C() {}

ProductionFacilityType2C::~ProductionFacilityType2C() {  }

int ProductionFacilityType2C::Read(istream* in) { return ProductionFacilityReadBody(this, in); }

int ProductionFacilityType2C::Write(ostream* out) { return ProductionFacilityWriteBody(this, out); }

int ProductionFacilityType2C::Checksum(Crc32* crc) { return ProductionFacilityChecksumBody(this, crc); }

int ProductionFacilityType2C::Validate() { return ValidateImpl(); }

int ProductionFacilityType2C::ApplyAll(NotifyContext* ctx) { return applyAll(this, ctx); }

int ProductionFacilityType2C::CreateViews(int controlKind, NotifyContext* ctx) { return GameObj::CreateViews(controlKind, ctx); }

GameObj* ProductionFacilityType2C::CreateViewObject() { return NewOrNull<ProductionFacilityType2C>(); }

int ProductionFacilityType2C::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    return ProductionFacility::OnControlKindChanged(oldKind, newKind, ctx);
}

int ProductionFacilityType2C::OnUsableChanged(int newValue, NotifyContext* ctx) {
    return ProductionFacility::OnUsableChanged(newValue, ctx);
}

int ProductionFacilityType2C::OnCreatedChanged(int newValue, NotifyContext* ctx) { return createdChanged(this, newValue, ctx); }

int ProductionFacilityType2C::OnExistingChanged(int newValue, NotifyContext* ctx) {
    return ProductionFacility::OnExistingChanged(newValue, ctx);
}

int ProductionFacilityType2C::CanProcess(const IntPair* work) { return 1; }

int ProductionFacilityType2C::ComputeDuration(const IntPair* work, int modifier, int notStartupCycle, int* duration) {
    return computeDuration(this, work, modifier, notStartupCycle, duration);
}

GameObjNotifier* CreateProductionFacilityType2C() { return NewOrNull<ProductionFacilityType2C>(); }

int CountProductionFacilityType2C(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kType2CFamilies, side, filter);
}

ProductionFacilityType2CIterator::ProductionFacilityType2CIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kType2CFamilies, stateFilter) {}

ProductionFacilityType2CIterator::ProductionFacilityType2CIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kType2CFamilies, stateFilter) {}

ProductionFacilityType2CIterator::~ProductionFacilityType2CIterator() {  }
