#include "core/game_string.h"
#include "data/gnprtb.h"
#include "economy/side.h"
#include "economy/side_decl.h"
#include "events/event_post.h"
#include "gameobj/object_type_db.h"

namespace
{

// Capacity of the maintenance state is the smaller of the mines'/refineries' capacity, its allocation the larger of
// their allocations (never above the capacity).
int recomputeMaintState(Side* side, NotifyContext* ctx)
{
  const uint32_t capacity = side->m_mineState.m_first < side->m_refineryState.m_first ? side->m_mineState.m_first
                                                                                      : side->m_refineryState.m_first;
  uint32_t allocated = static_cast<int>(side->m_mineState.m_second) > static_cast<int>(side->m_refineryState.m_second)
                           ? side->m_mineState.m_second
                           : side->m_refineryState.m_second;
  if (static_cast<int>(allocated) > static_cast<int>(capacity)) {
    allocated = capacity;
  }
  const IntPair state(capacity, allocated);
  return side->SetMaintState(&state, ctx);
}

using ResearchLookup = int (*)(int view, int level, int* found, int* foundLevel, int* extraValue);

// When the accumulated points of a kind cover the next level's (scaled) cost, advance the research order and spend
// the points; with no next level the kind is done.
int advanceResearch(Side* side, int order, int progress, ResearchLookup lookup,
                    int (Side::*setDone)(int, NotifyContext*), int (Side::*setOrder)(int, NotifyContext*),
                    int (Side::*setProgress)(int, NotifyContext*), NotifyContext* ctx)
{
  ObjectKey key;
  side->GetKey(&key);
  int found = 0;
  int nextOrder = 0;
  int cost = 0;
  int result = lookup(SideNumberFromKey(&key), order, &found, &nextOrder, &cost);
  if (result) {
    cost = RulesScalePercent(cost, side->m_rdScalePercent);
    if (found == 0) {
      result = (side->*setDone)(1, ctx);
    } else if (cost <= progress) {
      const int orderSet = (side->*setOrder)(nextOrder, ctx);
      result = 0;
      if (orderSet) {
        result = (side->*setProgress)(progress - cost, ctx);
      }
    }
  }
  return result;
}

} // namespace

// ---- OnXChanged ---------------------------------------------------------------------------------------------

int Side::OnMaintStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx)
{
  return UpdateMaintDeficit(ctx);
}

int Side::OnMineStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx)
{
  return recomputeMaintState(this, ctx);
}

int Side::OnRefineryStateChanged(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx)
{
  return recomputeMaintState(this, ctx);
}

int Side::OnMaintRequiredChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return UpdateMaintDeficit(ctx);
}

int Side::OnMaintRequiredBChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return UpdateMaintDeficit(ctx);
}

int Side::OnRawMaterialsChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (m_waitingRefineries > 0 && newValue > 0 && oldValue == 0) {
    result = ScheduleRefineryEvent();
  }
  return result;
}

int Side::OnRefinedMaterialsChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  int result = 1;
  if (m_waitingManufacturers > 0 && newValue > 0 && oldValue == 0) {
    result = ScheduleManufacturerEvent();
  }
  return result;
}

int Side::OnShipyardRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return advanceResearch(this, m_shipyardRdOrder, m_shipyardRdProgress, FindNextTier28, &Side::SetShipyardRdDone,
                         &Side::SetShipyardRdOrder, &Side::SetShipyardRdProgress, ctx);
}

int Side::OnTrainingFacilRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return advanceResearch(this, m_trainingFacilRdOrder, m_trainingFacilRdProgress, FindNextTier29,
                         &Side::SetTrainingFacilRdDone, &Side::SetTrainingFacilRdOrder,
                         &Side::SetTrainingFacilRdProgress, ctx);
}

int Side::OnConstructionYardRdProgressChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return advanceResearch(this, m_constructionYardRdOrder, m_constructionYardRdProgress, FindNextTier2a,
                         &Side::SetConstructionYardRdDone, &Side::SetConstructionYardRdOrder,
                         &Side::SetConstructionYardRdProgress, ctx);
}

int Side::OnMaintDeficitChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  return ArmTimerAfterSetFlag(side_event::kMaintDeficit, newValue, this, &m_statCc, ctx);
}

// A new master starts its two tracked stats from the rules' initial levels.
int Side::OnExistingChanged(int newValue, NotifyContext* ctx)
{
  int result = GameObj::OnExistingChanged(newValue, ctx);
  if (result && newValue) {
    m_statCc.SetLevel(g_gnprtb1C00);
    m_statD4.SetLevel(g_gnprtb1C01);
    m_statD4.SetProgress(g_gnprtb1C02);
    result = ArmTimerAfterSetFlag(side_event::kTrackedStatReset, 1, this, &m_statD4, ctx);
  }
  return result;
}

// ---- OnXNotified --------------------------------------------------------------------------------------------

namespace
{

// Appends "(first/second)" to *text. The pieces are built in the original's order (their heap blocks end up in the
// same places).
void appendPairText(std::string* text, const IntPair* pair)
{
  const std::string close(")");
  const std::string slash("/");
  const std::string open("(");
  const uint32_t second = pair->m_second;
  const uint32_t first = pair->m_first;
  *text += open;
  text->append(std::to_string(static_cast<long>(first)));
  *text += slash;
  text->append(std::to_string(static_cast<long>(second)));
  *text += close;
}

} // namespace

int Side::OnMaintStateNotified(const IntPair* oldValue, const IntPair* newValue, NotifyContext* ctx)
{
  const int sideView = IsSideView();
  if (!sideView) {
    return 0;
  }
  std::string newText;
  std::string oldText;
  appendPairText(&newText, newValue);
  appendPairText(&oldText, oldValue);
  LogChange("SideMaintStateNotif", "MaintState (Capacity/Allocated)", newText.c_str(), oldText.c_str(), nullptr, ctx);
  return PostPayloadFourInts(side_event::kMaintState, this, oldValue->m_first, oldValue->m_second, newValue->m_first,
                             newValue->m_second, ctx);
}

int Side::OnMaintRequiredNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogInt("SideMaintRequiredNotif", "Maint Required", newValue, oldValue, 0, ctx);
  return PostPayloadInts(side_event::kMaintRequired, this, oldValue, newValue, ctx);
}

int Side::OnRawMaterialsNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  return IsSideView() ? PostPayloadInts(side_event::kRawMaterials, this, oldValue, newValue, ctx) : 0;
}

int Side::OnRefinedMaterialsNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  return IsSideView() ? PostPayloadInts(side_event::kRefinedMaterials, this, oldValue, newValue, ctx) : 0;
}

int Side::OnWaitingRefineriesNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  return IsSideView() ? PostPayloadInts(side_event::kWaitingRefineries, this, oldValue, newValue, ctx) : 0;
}

int Side::OnWaitingManufacturersNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  return IsSideView() ? PostPayloadInts(side_event::kWaitingManufacturers, this, oldValue, newValue, ctx) : 0;
}

int Side::OnShipyardRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogInt("SideShipyardRdOrderNotif", "ShipyardRdOrder", newValue, oldValue, 0, ctx);
  return PostPayloadInts(side_event::kShipyardRdOrder, this, oldValue, newValue, ctx);
}

int Side::OnTrainingFacilRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogInt("SideTrainingFacilRdOrderNotif", "TrainingFacilRdOrder", newValue, oldValue, 0, ctx);
  return PostPayloadInts(side_event::kTrainingFacilRdOrder, this, oldValue, newValue, ctx);
}

int Side::OnConstructionYardRdOrderNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogInt("SideConstructionYardRdOrderNotif", "ConstructionYardRdOrder", newValue, oldValue, 0, ctx);
  return PostPayloadInts(side_event::kConstructionYardRdOrder, this, oldValue, newValue, ctx);
}

int Side::OnShipyardRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogFlag("(SideShipyardRdDoneNotif)", "ShipyardRdDone", newValue, 0, ctx);
  return PostPayloadFlag(side_event::kShipyardRdDone, this, newValue, ctx);
}

int Side::OnTrainingFacilRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogFlag("(SideTrainingFacilRdDoneNotif)", "TrainingFacilRdDone", newValue, 0, ctx);
  return PostPayloadFlag(side_event::kTrainingFacilRdDone, this, newValue, ctx);
}

int Side::OnConstructionYardRdDoneNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogFlag("(SideConstructionYardRdDoneNotif)", "ConstructionYardRdDone", newValue, 0, ctx);
  return PostPayloadFlag(side_event::kConstructionYardRdDone, this, newValue, ctx);
}

int Side::OnRecruitmentDoneNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  LogFlag("(SideRecruitmentDoneNotif)", "RecruitmentDone", newValue, 0, ctx);
  return PostPayloadFlag(side_event::kRecruitmentDone, this, newValue, ctx);
}

int Side::OnVictoryConditionsNotified(int oldValue, int newValue, NotifyContext* ctx)
{
  if (!IsSideView()) {
    return 0;
  }
  std::string newText;
  std::string oldText;
  int formatted = FormatVictoryConditions(static_cast<uint32_t>(oldValue), &oldText);
  if (formatted) {
    formatted = FormatVictoryConditions(static_cast<uint32_t>(newValue), &newText);
  }
  if (formatted) {
    LogString("SideVictoryConditionsNotif", "VictoryConditions", &newText, &oldText, 0, ctx);
  }
  return PostPayloadInts(side_event::kVictoryConditions, this, oldValue, newValue, ctx);
}
