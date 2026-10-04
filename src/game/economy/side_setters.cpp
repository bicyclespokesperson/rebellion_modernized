#include "economy/side.h"

namespace {

using IntChanged = int (Side::*)(int, int, NotifyContext*);
using PairChanged = int (Side::*)(const IntPair*, const IntPair*, NotifyContext*);
using Apply = int (Side::*)(NotifyContext*);

bool sameValue(const IntPair& a, const IntPair& b) { return a.m_first == b.m_first && a.m_second == b.m_second; }

int setPair(Side* self, IntPair Side::*field, const IntPair* value, NotifyContext* ctx, Apply apply,
            PairChanged changed) {
    const int result = self->IsMasterView();
    if (result) {
        IntPair& current = self->*field;
        if (!sameValue(current, *value)) {
            const IntPair old = current;
            current = *value;
            (static_cast<Side*>(self->GetViewObject(1))->*apply)(ctx);
            (static_cast<Side*>(self->GetViewObject(2))->*apply)(ctx);
            (self->*changed)(&old, &current, ctx);
        }
    }
    return result;
}

int setInt(Side* self, int Side::*field, int value, NotifyContext* ctx, Apply apply, IntChanged changed) {
    const int result = self->IsMasterView();
    if (result) {
        const int old = self->*field;
        if (old != value) {
            self->*field = value;
            (static_cast<Side*>(self->GetViewObject(1))->*apply)(ctx);
            (static_cast<Side*>(self->GetViewObject(2))->*apply)(ctx);
            (self->*changed)(old, self->*field, ctx);
        }
    }
    return result;
}

}  // namespace

int Side::SetMaintState(const IntPair* value, NotifyContext* ctx) {
    return setPair(this, &Side::m_maintState, value, ctx, &Side::ApplyMaintState, &Side::OnMaintStateChanged);
}

int Side::SetMineState(const IntPair* value, NotifyContext* ctx) {
    return setPair(this, &Side::m_mineState, value, ctx, &Side::ApplyMineState, &Side::OnMineStateChanged);
}

int Side::SetRefineryState(const IntPair* value, NotifyContext* ctx) {
    return setPair(this, &Side::m_refineryState, value, ctx, &Side::ApplyRefineryState,
                   &Side::OnRefineryStateChanged);
}

int Side::SetMaintRequired(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_maintRequired, value, ctx, &Side::ApplyMaintRequired, &Side::OnMaintRequiredChanged);
}

int Side::SetMaintRequiredB(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_maintRequiredB, value, ctx, &Side::ApplyMaintRequiredB,
                  &Side::OnMaintRequiredBChanged);
}

int Side::SetRawMaterials(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_rawMaterials, value, ctx, &Side::ApplyRawMaterials, &Side::OnRawMaterialsChanged);
}

int Side::SetRefinedMaterials(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_refinedMaterials, value, ctx, &Side::ApplyRefinedMaterials,
                  &Side::OnRefinedMaterialsChanged);
}

int Side::SetWaitingRefineries(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_waitingRefineries, value, ctx, &Side::ApplyWaitingRefineries,
                  &Side::OnWaitingRefineriesChanged);
}

int Side::SetWaitingManufacturers(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_waitingManufacturers, value, ctx, &Side::ApplyWaitingManufacturers,
                  &Side::OnWaitingManufacturersChanged);
}

int Side::SetShipyardRdProgress(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_shipyardRdProgress, value, ctx, &Side::ApplyShipyardRdProgress,
                  &Side::OnShipyardRdProgressChanged);
}

int Side::SetTrainingFacilRdProgress(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_trainingFacilRdProgress, value, ctx, &Side::ApplyTrainingFacilRdProgress,
                  &Side::OnTrainingFacilRdProgressChanged);
}

int Side::SetConstructionYardRdProgress(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_constructionYardRdProgress, value, ctx, &Side::ApplyConstructionYardRdProgress,
                  &Side::OnConstructionYardRdProgressChanged);
}

int Side::SetShipyardRdOrder(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_shipyardRdOrder, value, ctx, &Side::ApplyShipyardRdOrder,
                  &Side::OnShipyardRdOrderChanged);
}

int Side::SetTrainingFacilRdOrder(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_trainingFacilRdOrder, value, ctx, &Side::ApplyTrainingFacilRdOrder,
                  &Side::OnTrainingFacilRdOrderChanged);
}

int Side::SetConstructionYardRdOrder(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_constructionYardRdOrder, value, ctx, &Side::ApplyConstructionYardRdOrder,
                  &Side::OnConstructionYardRdOrderChanged);
}

int Side::SetMaintDeficit(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_maintDeficit, value, ctx, &Side::ApplyMaintDeficit, &Side::OnMaintDeficitChanged);
}

int Side::SetShipyardRdDone(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_shipyardRdDone, value, ctx, &Side::ApplyShipyardRdDone,
                  &Side::OnShipyardRdDoneChanged);
}

int Side::SetTrainingFacilRdDone(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_trainingFacilRdDone, value, ctx, &Side::ApplyTrainingFacilRdDone,
                  &Side::OnTrainingFacilRdDoneChanged);
}

int Side::SetConstructionYardRdDone(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_constructionYardRdDone, value, ctx, &Side::ApplyConstructionYardRdDone,
                  &Side::OnConstructionYardRdDoneChanged);
}

int Side::SetRecruitmentDone(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_recruitmentDone, value, ctx, &Side::ApplyRecruitmentDone,
                  &Side::OnRecruitmentDoneChanged);
}

int Side::SetVictoryConditions(int value, NotifyContext* ctx) {
    return setInt(this, &Side::m_victoryConditions, value, ctx, &Side::ApplyVictoryConditions,
                  &Side::OnVictoryConditionsChanged);
}

int Side::SetRdProgress(int kind, int value, NotifyContext* ctx) {
    switch (kind) {
        case rd_kind::kShipyard:
            return SetShipyardRdProgress(value, ctx);
        case rd_kind::kConstructionYard:
            return SetConstructionYardRdProgress(value, ctx);
        case rd_kind::kTrainingFacil:
            return SetTrainingFacilRdProgress(value, ctx);
        default:
            return 0;
    }
}

int Side::GetRdProgress(int kind, int* out) {
    *out = 0;
    switch (kind) {
        case rd_kind::kShipyard:
            *out = m_shipyardRdProgress;
            return 1;
        case rd_kind::kConstructionYard:
            *out = m_constructionYardRdProgress;
            return 1;
        case rd_kind::kTrainingFacil:
            *out = m_trainingFacilRdProgress;
            return 1;
        default:
            return 0;
    }
}

int Side::GetRdOrder(int kind, int* out) {
    *out = 0;
    switch (kind) {
        case rd_kind::kShipyard:
            *out = m_shipyardRdOrder;
            return 1;
        case rd_kind::kConstructionYard:
            *out = m_constructionYardRdOrder;
            return 1;
        case rd_kind::kTrainingFacil:
            *out = m_trainingFacilRdOrder;
            return 1;
        default:
            return 0;
    }
}

int Side::GetRdDone(int kind, int* out) {
    *out = 0;
    switch (kind) {
        case rd_kind::kShipyard:
            *out = m_shipyardRdDone;
            return 1;
        case rd_kind::kConstructionYard:
            *out = m_constructionYardRdDone;
            return 1;
        case rd_kind::kTrainingFacil:
            *out = m_trainingFacilRdDone;
            return 1;
        default:
            return 0;
    }
}
