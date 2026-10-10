#include "economy/manufacturing_facility.h"

#include "core/vptr_store.h"
#include "economy/manu_mgr_leaves.h"

namespace {
const FamilyRange kManufacturingFamilies(0x28, 0x2c);
const FamilyRange kFamily28(0x28, 0x29);
const FamilyRange kFamily29(0x29, 0x2a);
const FamilyRange kFamily2A(0x2a, 0x2b);
constexpr uint32_t kFilterAll = 1;

int applyAllOnSideView(ManufacturingFacility* facility, NotifyContext* ctx) {
    const int result = facility->IsSideView();
    if (result) facility->ApplyAllForward(ctx);
    return result;
}
}  // namespace

ManufacturingFacility::ManufacturingFacility() {}

ManufacturingFacility::~ManufacturingFacility() { REB_KEEP_VPTR_STORE(); }

int ManufacturingFacility::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    return ProcFacil::OnControlKindChanged(oldKind, newKind, ctx);
}

int ManufacturingFacility::OnUsableChanged(int newValue, NotifyContext* ctx) { return ProcFacil::OnUsableChanged(newValue, ctx); }

int ManufacturingFacility::OnSuspendedChanged(int value, NotifyContext* ctx) { return ProcFacil::OnSuspendedChanged(value, ctx); }

int ManufacturingFacility::ReadBody(istream* in) { return ReadImpl(in); }

int ManufacturingFacility::WriteBody(ostream* out) { return WriteImpl(out); }

int ManufacturingFacility::ChecksumBody(Crc32* crc) { return ChecksumImpl(crc); }

int ManufacturingFacility::ApplyAllBody(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) ApplyAllImpl(ctx);
    return result;
}

int ManufacturingFacility::ReadForward(istream* in) { return ReadBody(in); }

int ManufacturingFacility::WriteForward(ostream* out) { return WriteBody(out); }

int ManufacturingFacility::ChecksumForward(Crc32* crc) { return ChecksumBody(crc); }

int ManufacturingFacility::ApplyAllForward(NotifyContext* ctx) { return ApplyAllBody(ctx); }

int ManufacturingFacility::CreateViewsForward(int controlKind, NotifyContext* ctx) { return CreateViewsImpl(controlKind, ctx); }

int ManufacturingFacility::CreateViewsImpl(int controlKind, NotifyContext* ctx) {
    const int result = CreateViewsStubShared(this, controlKind, ctx);
    if (result) {
        const int duration = static_cast<int>(Facility::GetRecordField5c());
        if (m_data) static_cast<ProcFacilData*>(m_data)->m_duration = duration;
    }
    return result;
}

// ---- family 0x2a --------------------------------------------------------------------------------------------------

ManufacturingFacility2A::ManufacturingFacility2A() {}

ManufacturingFacility2A::~ManufacturingFacility2A() { REB_KEEP_VPTR_STORE(); }

int ManufacturingFacility2A::Read(istream* in) { return ReadForward(in); }

int ManufacturingFacility2A::Write(ostream* out) { return WriteForward(out); }

int ManufacturingFacility2A::Checksum(Crc32* crc) { return ChecksumForward(crc); }

int ManufacturingFacility2A::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int ManufacturingFacility2A::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsForward(controlKind, ctx); }

GameObj* ManufacturingFacility2A::CreateViewObject() { return new ManufacturingFacility2A(); }

GameObjNotifier* CreateManufacturingFacility2A() { return new ManufacturingFacility2A(); }

// ---- family 0x29 --------------------------------------------------------------------------------------------------

ManufacturingFacility29::ManufacturingFacility29() {}

ManufacturingFacility29::~ManufacturingFacility29() { REB_KEEP_VPTR_STORE(); }

int ManufacturingFacility29::Read(istream* in) { return ReadForward(in); }

int ManufacturingFacility29::Write(ostream* out) { return WriteForward(out); }

int ManufacturingFacility29::Checksum(Crc32* crc) { return ChecksumForward(crc); }

int ManufacturingFacility29::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int ManufacturingFacility29::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsForward(controlKind, ctx); }

GameObj* ManufacturingFacility29::CreateViewObject() { return new ManufacturingFacility29(); }

GameObjNotifier* CreateManufacturingFacility29() { return new ManufacturingFacility29(); }

// ---- family 0x28 --------------------------------------------------------------------------------------------------

ManufacturingFacility28::ManufacturingFacility28() {}

ManufacturingFacility28::~ManufacturingFacility28() { REB_KEEP_VPTR_STORE(); }

int ManufacturingFacility28::Read(istream* in) { return ReadForward(in); }

int ManufacturingFacility28::Write(ostream* out) { return WriteForward(out); }

int ManufacturingFacility28::Checksum(Crc32* crc) { return ChecksumForward(crc); }

int ManufacturingFacility28::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

int ManufacturingFacility28::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsForward(controlKind, ctx); }

GameObj* ManufacturingFacility28::CreateViewObject() { return new ManufacturingFacility28(); }

GameObjNotifier* CreateManufacturingFacility28() { return new ManufacturingFacility28(); }

// ---- child counts ---------------------------------------------------------------------------------------------------

int CountManufacturingFacilityChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kManufacturingFamilies, side, filter);
}

int CountManufacturingFacility2AChildren(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kFamily2A, side, filter);
}

int CountManufacturingFacility29Children(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kFamily29, side, filter);
}

int CountManufacturingFacility28Children(GameObj* parent, uint32_t side, uint32_t filter) {
    return parent->CountChildren(&kFamily28, side, filter);
}

// ---- iterators ------------------------------------------------------------------------------------------------------

ManufacturingFacilityIterator::ManufacturingFacilityIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &kManufacturingFamilies, kFilterAll) {}

ManufacturingFacilityIterator::ManufacturingFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kManufacturingFamilies, stateFilter) {}

ManufacturingFacilityIterator::~ManufacturingFacilityIterator() { REB_KEEP_VPTR_STORE(); }

ManufacturingFacility2AIterator::ManufacturingFacility2AIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kFamily2A, stateFilter) {}

ManufacturingFacility2AIterator::ManufacturingFacility2AIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kFamily2A, stateFilter) {}

ManufacturingFacility2AIterator::~ManufacturingFacility2AIterator() { REB_KEEP_VPTR_STORE(); }

ManufacturingFacility29Iterator::ManufacturingFacility29Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kFamily29, stateFilter) {}

ManufacturingFacility29Iterator::ManufacturingFacility29Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kFamily29, stateFilter) {}

ManufacturingFacility29Iterator::~ManufacturingFacility29Iterator() { REB_KEEP_VPTR_STORE(); }

ManufacturingFacility28Iterator::ManufacturingFacility28Iterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kFamily28, stateFilter) {}

ManufacturingFacility28Iterator::ManufacturingFacility28Iterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kFamily28, stateFilter) {}

ManufacturingFacility28Iterator::~ManufacturingFacility28Iterator() { REB_KEEP_VPTR_STORE(); }
