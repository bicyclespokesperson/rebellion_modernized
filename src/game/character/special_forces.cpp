#include "character/special_forces.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "data/sd_records.h"
#include "economy/manu_mgr_leaves.h"
#include "gameobj/object_family.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
extern "C" void __cdecl LAB_00504000();

extern int g_specialForcesEventKindsReady;       // 0x6b2b50
extern EventKindSet g_specialForcesEventKinds;   // 0x6b2b58


namespace {

constexpr uint32_t kSpecialForcesFamily = 0x3c;
constexpr uint32_t kOnMissionFlag = 0x80;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kEnrouteActiveFlag = 0x20;
constexpr uint32_t kEnrouteOrDestroyed = 0xc;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;
constexpr int kDestroyedReasonSideLost = 3;
constexpr int kDestroyedReasonSpecial = 9;

}  // namespace

// Constructs g_specialForcesEventKinds (0x503fe0) and registers its destructor (0x503ff0); both run from the static
// initializer 0x503fd0.
void ConstructSpecialForcesEventKinds() { new (&g_specialForcesEventKinds) EventKindSet(); }

void RegisterSpecialForcesEventKindsAtExit() { atexit(LAB_00504000); }

SpecialForces::SpecialForces() {}

SpecialForces::~SpecialForces() { REB_KEEP_VPTR_STORE(); }

uint32_t SpecialForces::GetFamily() { return kSpecialForcesFamily; }

int SpecialForces::Read(istream* in) { return ReadImpl(in); }

int SpecialForces::Write(ostream* out) { return WriteImpl(out); }

int SpecialForces::Checksum(Crc32* crc) { return ChecksumImpl(crc); }

int SpecialForces::Validate() { return Role::Validate(); }

int SpecialForces::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return ApplyAllImpl(ctx);
    return isSideView;
}

int SpecialForces::ReadImpl(istream* in) { return Role::Read(in); }

int SpecialForces::WriteImpl(ostream* out) { return Role::Write(out); }

int SpecialForces::ApplyAllImpl(NotifyContext* ctx) { return Role::ApplyAll(ctx); }

int SpecialForces::ChecksumImpl(Crc32* crc) { return Role::Checksum(crc); }

int SpecialForces::GetEventKinds(int unused, EventKindSet* out) {
    const int result = BuildEventKinds();
    if (result) *out = g_specialForcesEventKinds;
    return result;
}

int SpecialForces::CheckOrder(int a, int orderKind, StatusCode* out) {
    *out = StatusCode(kStatusCategory, kStatusNotSupported);
    const int result = BuildEventKinds();
    if (result && g_specialForcesEventKinds.Contains(orderKind)) *out = StatusCode(kStatusCategory, kStatusOk);
    return result;
}

uint32_t SpecialForces::GetRecordMissionBits() {
    if (!m_record) return 0;
    return static_cast<SpecialForceSD*>(m_record)->m_missionId;
}

int SpecialForces::Virtual44(NotifyContext* ctx) {
    int result = 1;
    if (!(m_flags78 & kOnMissionFlag) && !(m_stateFlags & kDestroyedFlag)) {
        GameObjNotifier* parent = nullptr;
        result = GetParent(&parent) != 0;
        if (parent) {
            const uint32_t side = parent->ControlSide();
            if (side == 0 || side > 3) return 0;
            if (CanBeControlledBy(side)) return SetControlKind(side, ctx);
            result = ChangeDestroyedReason(kDestroyedReasonSideLost, ctx);
        }
    }
    return result;
}

int SpecialForces::Virtual45(int a, NotifyContext* ctx) {
    int result = 1;
    if (!(m_stateFlags & kEnrouteActiveFlag) && a != 0 && !(m_stateFlags & kDestroyedFlag)) {
        result = InheritDestroyedReason(ctx);
    }
    if (!(m_flags78 & kOnMissionFlag) && !(m_stateFlags & kDestroyedFlag) && a == 0) {
        result = SetAutorouting(1, ctx);
    }
    return result;
}

int SpecialForces::Virtual46(NotifyContext* ctx) {
    int result = 1;
    if (!(m_flags78 & kOnMissionFlag) && !(m_stateFlags & kEnrouteOrDestroyed)) result = SetAutorouting(1, ctx);
    return result;
}

int SpecialForces::CreateViews(int controlKind, NotifyContext* ctx) { return CreateViewsStubShared(this, controlKind, ctx); }

GameObj* SpecialForces::CreateViewObject() { return NewOrNull<SpecialForces>(); }

GameObjNotifier* SpecialForces::Create() { return NewOrNull<SpecialForces>(); }

int SpecialForces::GetDiplomacy() { return m_baseDiplomacy; }

int SpecialForces::GetEspionage() { return m_baseEspionage; }

int SpecialForces::GetShipyardRD() { return m_baseShipyardRD; }

int SpecialForces::GetTrainingFacilRD() { return m_baseTrainingFacilRD; }

int SpecialForces::GetConstructionYardRD() { return m_baseConstructionYardRD; }

int SpecialForces::GetCombat() { return m_baseCombat; }

int SpecialForces::GetLeadership() { return m_baseLeadership; }

int SpecialForces::GetLoyalty() { return m_baseLoyalty; }

int SpecialForces::Virtual130(NotifyContext* ctx) { return Role::Virtual130(ctx); }

int SpecialForces::Virtual131(const ObjectKey* opponent, NotifyContext* ctx) {
    if (Role::Virtual131(opponent, ctx)) return ChangeDestroyedReason(kDestroyedReasonSpecial, ctx);
    return 0;
}

int SpecialForces::BuildEventKinds() {
    int result = 1;
    if (!g_specialForcesEventKindsReady) {
        g_specialForcesEventKindsReady = 1;
        result = Role::InitEventKinds(0, &g_specialForcesEventKinds);
    }
    return result;
}

void SpecialForces::InitStatics() {
    ConstructSpecialForcesEventKinds();
    RegisterSpecialForcesEventKindsAtExit();
}

SpecialForceIterator::SpecialForceIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &family::kSpecialForces, stateFilter) {}

SpecialForceIterator::SpecialForceIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &family::kSpecialForces, stateFilter) {}

SpecialForceIterator::~SpecialForceIterator() { REB_KEEP_VPTR_STORE(); }
