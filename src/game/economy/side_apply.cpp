#include "economy/side.h"

namespace {

using IntNotified = int (Side::*)(int, int, NotifyContext*);
using PairNotified = int (Side::*)(const IntPair*, const IntPair*, NotifyContext*);

bool sameValue(const IntPair& a, const IntPair& b) { return a.m_first == b.m_first && a.m_second == b.m_second; }

// Copies the master's int into this side view when the context says this view is to be told, then raises the
// notification (if the strategic layer allows and this view's side is switched on).
int applyInt(Side* self, int Side::*field, IntNotified notified, NotifyContext* ctx) {
    int result = self->IsSideView();
    if (result) {
        Side* master = static_cast<Side*>(self->GetViewObject(0));
        if (self->MatchesSideMask(ctx->m_flag08)) {
            const int old = self->*field;
            if (old != master->*field) {
                self->*field = master->*field;
                if (CallGameHook2AF0() && ctx->GetViewFlag(self->ViewSide())) {
                    result = (self->*notified)(old, self->*field, ctx);
                }
            }
        }
    }
    return result;
}

int applyPair(Side* self, IntPair Side::*field, PairNotified notified, NotifyContext* ctx) {
    int result = self->IsSideView();
    if (result) {
        Side* master = static_cast<Side*>(self->GetViewObject(0));
        if (self->MatchesSideMask(ctx->m_flag08)) {
            IntPair& current = self->*field;
            if (!sameValue(current, master->*field)) {
                const IntPair old = current;
                current = master->*field;
                if (CallGameHook2AF0() && ctx->GetViewFlag(self->ViewSide())) {
                    result = (self->*notified)(&old, &current, ctx);
                }
            }
        }
    }
    return result;
}

}  // namespace

int Side::ApplyMaintState(NotifyContext* ctx) {
    return applyPair(this, &Side::m_maintState, &Side::OnMaintStateNotified, ctx);
}

int Side::ApplyMineState(NotifyContext* ctx) {
    return applyPair(this, &Side::m_mineState, &Side::OnMineStateNotified, ctx);
}

int Side::ApplyRefineryState(NotifyContext* ctx) {
    return applyPair(this, &Side::m_refineryState, &Side::OnRefineryStateNotified, ctx);
}

int Side::ApplyMaintRequired(NotifyContext* ctx) {
    return applyInt(this, &Side::m_maintRequired, &Side::OnMaintRequiredNotified, ctx);
}

int Side::ApplyMaintRequiredB(NotifyContext* ctx) {
    return applyInt(this, &Side::m_maintRequiredB, &Side::OnMaintRequiredBNotified, ctx);
}

int Side::ApplyRawMaterials(NotifyContext* ctx) {
    return applyInt(this, &Side::m_rawMaterials, &Side::OnRawMaterialsNotified, ctx);
}

int Side::ApplyRefinedMaterials(NotifyContext* ctx) {
    return applyInt(this, &Side::m_refinedMaterials, &Side::OnRefinedMaterialsNotified, ctx);
}

int Side::ApplyWaitingRefineries(NotifyContext* ctx) {
    return applyInt(this, &Side::m_waitingRefineries, &Side::OnWaitingRefineriesNotified, ctx);
}

int Side::ApplyWaitingManufacturers(NotifyContext* ctx) {
    return applyInt(this, &Side::m_waitingManufacturers, &Side::OnWaitingManufacturersNotified, ctx);
}

int Side::ApplyShipyardRdProgress(NotifyContext* ctx) {
    return applyInt(this, &Side::m_shipyardRdProgress, &Side::OnShipyardRdProgressNotified, ctx);
}

int Side::ApplyTrainingFacilRdProgress(NotifyContext* ctx) {
    return applyInt(this, &Side::m_trainingFacilRdProgress, &Side::OnTrainingFacilRdProgressNotified, ctx);
}

int Side::ApplyConstructionYardRdProgress(NotifyContext* ctx) {
    return applyInt(this, &Side::m_constructionYardRdProgress, &Side::OnConstructionYardRdProgressNotified, ctx);
}

int Side::ApplyShipyardRdOrder(NotifyContext* ctx) {
    return applyInt(this, &Side::m_shipyardRdOrder, &Side::OnShipyardRdOrderNotified, ctx);
}

int Side::ApplyTrainingFacilRdOrder(NotifyContext* ctx) {
    return applyInt(this, &Side::m_trainingFacilRdOrder, &Side::OnTrainingFacilRdOrderNotified, ctx);
}

int Side::ApplyConstructionYardRdOrder(NotifyContext* ctx) {
    return applyInt(this, &Side::m_constructionYardRdOrder, &Side::OnConstructionYardRdOrderNotified, ctx);
}

int Side::ApplyMaintDeficit(NotifyContext* ctx) {
    return applyInt(this, &Side::m_maintDeficit, &Side::OnMaintDeficitNotified, ctx);
}

int Side::ApplyShipyardRdDone(NotifyContext* ctx) {
    return applyInt(this, &Side::m_shipyardRdDone, &Side::OnShipyardRdDoneNotified, ctx);
}

int Side::ApplyTrainingFacilRdDone(NotifyContext* ctx) {
    return applyInt(this, &Side::m_trainingFacilRdDone, &Side::OnTrainingFacilRdDoneNotified, ctx);
}

int Side::ApplyConstructionYardRdDone(NotifyContext* ctx) {
    return applyInt(this, &Side::m_constructionYardRdDone, &Side::OnConstructionYardRdDoneNotified, ctx);
}

int Side::ApplyRecruitmentDone(NotifyContext* ctx) {
    return applyInt(this, &Side::m_recruitmentDone, &Side::OnRecruitmentDoneNotified, ctx);
}

int Side::ApplyVictoryConditions(NotifyContext* ctx) {
    return applyInt(this, &Side::m_victoryConditions, &Side::OnVictoryConditionsNotified, ctx);
}
