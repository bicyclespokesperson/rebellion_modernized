#include "economy/production_facility.h"

#include "core/utility_classes.h"
#include "events/event_post.h"
#include "gameobj/game_hooks.h"


namespace {

constexpr int kAllianceView = 1;
constexpr int kEmpireView = 2;
constexpr int kMasterView = 0;
constexpr int kDefaultProductionModifier = 100;
constexpr uint32_t kUsable = 0x1;
constexpr uint32_t kControlSideMask = 0xc0;
constexpr int kApplyEverything = 1;
constexpr FamilyRange kProductionFacilityFamilies(0x2c, 0x30);

ProductionFacility* viewOf(ProductionFacility* facility, int view) {
    return static_cast<ProductionFacility*>(facility->ViewAs(view));
}

bool sameWork(const IntPair& a, const IntPair& b) { return a.m_first == b.m_first && a.m_second == b.m_second; }

IntPair addWork(const IntPair& a, const IntPair& b) { return IntPair(a.m_first + b.m_first, a.m_second + b.m_second); }

bool controlledByNeutral(const ProductionFacility* facility) {
    return (facility->m_flags & kControlSideMask) == kControlSideMask;
}

}  // namespace

ProductionFacility::ProductionFacility() : m_work(), m_productionModifier(kDefaultProductionModifier) {}

ProductionFacility::~ProductionFacility() {  }

IntPair* ProductionFacility::GetWork(IntPair* out) {
    *out = m_work;
    return out;
}

// ---- events on the master -----------------------------------------------------------------------------------

int ProductionFacility::OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) {
    int result = ProcFacil::OnControlKindChanged(oldKind, newKind, ctx);
    IntPair work;
    GetWork(&work);
    work.m_second = 0;
    SetWork(&work, ctx);
    if (result) {
        result = UpdateEligibility(ctx);
        if (result) result = ResetWorkIfUnavailable(ctx);
    }
    return result;
}

int ProductionFacility::OnUsableChanged(int newValue, NotifyContext* ctx) {
    int result = ProcFacil::OnUsableChanged(newValue, ctx);
    if (result) {
        result = UpdateEligibility(ctx);
        if (result) result = ResetWorkIfUnavailable(ctx);
    }
    return result;
}

int ProductionFacility::OnCreatedChanged(int newValue, NotifyContext* ctx) { return ProcFacil::OnCreatedChanged(newValue, ctx); }

int ProductionFacility::OnExistingChanged(int newValue, NotifyContext* ctx) { return ProcFacil::OnExistingChanged(newValue, ctx); }

int ProductionFacility::OnStateChanged(int oldState, int newState, NotifyContext* ctx) {
    int result = ProcFacil::OnStateChanged(oldState, newState, ctx);
    if (result) result = UpdateEligibility(ctx);
    return result;
}

int ProductionFacility::OnSuspendedChanged(int value, NotifyContext* ctx) {
    int result = ProcFacil::OnSuspendedChanged(value, ctx);
    if (result) {
        result = UpdateEligibility(ctx);
        if (result) result = ResetWorkIfUnavailable(ctx);
    }
    return result;
}

int ProductionFacility::OnStartupCycleChanged(int value, NotifyContext* ctx) {
    int result = ProcFacil::OnStartupCycleChanged(value, ctx);
    if (result) result = UpdateDuration(ctx);
    return result;
}

int ProductionFacility::OnWorkChanged(const IntPair* oldWork, const IntPair* newWork, NotifyContext* ctx) {
    int result = UpdateEligibility(ctx);
    if (result) result = UpdateDuration(ctx);
    return result;
}

int ProductionFacility::OnProductionModifierChanged(int oldValue, int newValue, NotifyContext* ctx) { return UpdateDuration(ctx); }

int ProductionFacility::OnWorkNotified(const IntPair* oldWork, const IntPair* newWork, NotifyContext* ctx) { return IsSideView(); }

int ProductionFacility::OnProductionModifierNotified(int oldValue, int newValue, NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) LogInt("(ProcFacilProductionModifierNotif)", "ProductionModifier", newValue, oldValue, 0, ctx);
    return result;
}

// ---- setters ---------------------------------------------------------------------------------------------------

int ProductionFacility::SetWork(const IntPair* value, NotifyContext* ctx) {
    const int result = IsMasterView();
    if (result && !sameWork(m_work, *value)) {
        const IntPair oldWork = m_work;
        m_work = *value;
        viewOf(this, kAllianceView)->ApplyWork(ctx);
        viewOf(this, kEmpireView)->ApplyWork(ctx);
        OnWorkChanged(&oldWork, &m_work, ctx);
    }
    return result;
}

int ProductionFacility::SetProductionModifier(int value, NotifyContext* ctx) {
    int result = 0;
    if (IsMasterView()) result = IsInRange(value, 0, value, "Invalid ProductionModifier value!");
    if (result && m_productionModifier != value) {
        const int oldValue = m_productionModifier;
        m_productionModifier = value;
        viewOf(this, kAllianceView)->ApplyProductionModifier(ctx);
        viewOf(this, kEmpireView)->ApplyProductionModifier(ctx);
        OnProductionModifierChanged(oldValue, m_productionModifier, ctx);
    }
    return result;
}

int ProductionFacility::AdvanceWork(int limit, const int* ratio, int* added, NotifyContext* ctx) {
    int target = 0;
    if (ratio[0] != 0) {
        IntPair work;
        target = (ratio[1] * static_cast<int>(GetWork(&work)->m_first)) / ratio[0];
    }
    IntPair work;
    *added = (target - static_cast<int>(GetWork(&work)->m_second)) + 1;
    if (*added < 0) *added = 0;
    if (limit < *added) *added = limit;
    if (static_cast<int>(m_work.m_first) - static_cast<int>(m_work.m_second) < *added) {
        *added = static_cast<int>(m_work.m_first) - static_cast<int>(m_work.m_second);
    }
    if (0 < *added) {
        const IntPair delta(0, *added);
        IntPair current;
        const IntPair updated = addWork(*GetWork(&current), delta);
        SetWork(&updated, ctx);
    }
    return 1;
}

int ProductionFacility::RewindWork(int limit, const int* ratio, int* removed, NotifyContext* ctx) {
    int target = 0;
    if (ratio[0] != 0) {
        IntPair work;
        target = ((ratio[1] - 1) * static_cast<int>(GetWork(&work)->m_first)) / ratio[0];
    }
    IntPair work;
    *removed = static_cast<int>(GetWork(&work)->m_second) - target;
    if (*removed < 0) *removed = 0;
    if (limit < *removed) *removed = limit;
    if (0 < *removed) {
        const IntPair delta(0, -*removed);
        IntPair current;
        const IntPair updated = addWork(*GetWork(&current), delta);
        SetWork(&updated, ctx);
    }
    return 1;
}

// ---- derived state -------------------------------------------------------------------------------------------------

int ProductionFacility::UpdateEligibility(NotifyContext* ctx) {
    const bool notIdle = m_state != kStateIdle;
    int eligible = 0;
    if ((m_stateFlags & kUsable) && !(m_procFlags & kSuspended) && !controlledByNeutral(this)) {
        IntPair work;
        GetWork(&work);
        eligible = CanProcess(&work) != 0;
    }
    int result = 1;
    if (eligible) {
        if (!notIdle) result = Restart(ctx);
    } else if (notIdle) {
        result = ResetToIdle(ctx);
    }
    return result;
}

int ProductionFacility::ResetWorkIfUnavailable(NotifyContext* ctx) {
    if (!(m_stateFlags & kUsable) || (m_procFlags & kSuspended) || controlledByNeutral(this)) {
        IntPair work;
        GetWork(&work);
        work.m_second = 0;
        SetWork(&work, ctx);
    }
    return 1;
}

int ProductionFacility::UpdateDuration(NotifyContext* ctx) {
    int duration = 0;
    const int modifier = m_productionModifier;
    IntPair work;
    GetWork(&work);
    const int notStartupCycle = (~m_procFlags >> 4) & 1;
    if (ComputeDuration(&work, modifier, notStartupCycle, &duration) == 0) duration = 0;
    if (m_data) static_cast<ProcFacilData*>(m_data)->m_duration = duration;
    return 1;
}

// ---- views ---------------------------------------------------------------------------------------------------------

int ProductionFacility::ApplyWork(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        ProductionFacility* master = viewOf(this, kMasterView);
        if (MatchesSideMask(ctx->m_flag08)) {
            IntPair masterWork;
            master->GetWork(&masterWork);
            if (!sameWork(m_work, masterWork)) {
                const IntPair oldWork = m_work;
                master->GetWork(&masterWork);
                m_work = masterWork;
                if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide()))) {
                    result = OnWorkNotified(&oldWork, &m_work, ctx);
                }
            }
        }
    }
    return result;
}

int ProductionFacility::ApplyProductionModifier(NotifyContext* ctx) {
    int result = IsSideView();
    if (result) {
        ProductionFacility* master = viewOf(this, kMasterView);
        if (MatchesSideMask(ctx->m_flag08)) {
            const int oldValue = m_productionModifier;
            if (oldValue != master->m_productionModifier) {
                m_productionModifier = master->m_productionModifier;
                if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(ViewSide()))) {
                    result = OnProductionModifierNotified(oldValue, m_productionModifier, ctx);
                }
            }
        }
    }
    return result;
}

// ---- serialisation, checksum, validation ---------------------------------------------------------------------------

int ProductionFacility::ReadImpl(istream* in) {
    const int result = ProcFacil::ReadImpl(in);
    if (result) {
        ReadValue32(in, &m_work.m_first);
        ReadValue32(in, &m_work.m_second);
        ReadValue32(in, &m_productionModifier);
    }
    return result;
}

int ProductionFacility::WriteImpl(ostream* out) {
    const int result = ProcFacil::WriteImpl(out);
    if (result) {
        WriteValue32(out, &m_work.m_first);
        WriteValue32(out, &m_work.m_second);
        WriteValue32(out, &m_productionModifier);
    }
    return result;
}

int ProductionFacility::ChecksumImpl(Crc32* crc) {
    const int result = ProcFacil::ChecksumImpl(crc);
    if (result) {
        crc->AddInt(m_work.m_first);
        crc->AddInt(m_work.m_second);
        crc->AddInt(m_productionModifier);
    }
    return result;
}

int ProductionFacility::ApplyAllImpl(NotifyContext* ctx) {
    const int result = IsSideView();
    if (result) {
        ProcFacil::ApplyAllImpl(ctx);
        if (ctx->m_flag04 == kApplyEverything) {
            ApplyWork(ctx);
            ApplyProductionModifier(ctx);
        }
    }
    return result;
}

int ProductionFacility::ValidateImpl() {
    ProcFacil::ValidateImpl();
    const bool notIdle = m_state != kStateIdle;
    int eligible = 0;
    if ((m_stateFlags & kUsable) && !(m_procFlags & kSuspended) && !controlledByNeutral(this)) {
        IntPair work;
        GetWork(&work);
        eligible = CanProcess(&work) != 0;
    }
    return notIdle == (eligible != 0);
}

// ---- iterator ------------------------------------------------------------------------------------------------------

ProductionFacilityIterator::ProductionFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kProductionFacilityFamilies, stateFilter) {}

ProductionFacilityIterator::~ProductionFacilityIterator() {  }
