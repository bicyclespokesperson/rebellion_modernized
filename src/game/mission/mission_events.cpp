#include "events/event_post.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_external.h"
#include "mission/mission_hub.h"

namespace
{

constexpr int kPhaseLeaderSelect = 2;
constexpr int kPhaseReadyToStart = 4;
constexpr int kPhaseTravel = 6;
constexpr int kPhaseOutcome = 8;
constexpr int kPhaseFinished = 0xb;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr int kEventPhaseStat = 0x38b;
constexpr int kEventPhaseChanged = 0x30d;
constexpr int kEventReadyForNextPhase = 0x30c;

MissionData* dataOf(Mission* mission)
{
  return static_cast<MissionData*>(mission->m_data);
}

int allOf(int a, int b)
{
  return a && b;
}

} // namespace

int Mission::OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx)
{
  return GameObj::OnParentChanged(oldParent, newParent, ctx) != 0;
}

int Mission::OnDestroyedChanged(int newValue, NotifyContext* ctx)
{
  int ok = GameObj::OnDestroyedChanged(newValue, ctx) != 0;
  if (newValue != 0) {
    ObjectKey cleared;
    ok = allOf(SetObjectiveKey(&cleared, ctx), ok);
  }
  return ok;
}

int Mission::OnCompletionStatusChanged(int oldValue, int newValue, NotifyContext* ctx)
{
  if (newValue == 0) {
    return 1;
  }
  const int advanced = AdvancePhase(ctx);
  const int scheduled = ScheduleTimerEvent(ctx);
  return scheduled && advanced ? 1 : 0;
}

int Mission::OnObjectiveKeyChanged(ObjectKey* oldKey, ObjectKey* newKey, NotifyContext* ctx)
{
  int ok = 1;
  if (oldKey->OrNull()) {
    auto* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(oldKey));
    ok = object->DecrementLock(ctx) != 0;
  }
  if (newKey->OrNull()) {
    auto* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(newKey));
    if (object->IncrementLock(ctx) && ok) {
      return 1;
    }
    ok = 0;
  }
  return ok;
}

int Mission::OnReadyForNextPhaseChanged(int newValue, NotifyContext* ctx)
{
  return PostSourceEventFlag(kEventReadyForNextPhase, this, newValue, ctx) != 0;
}

int Mission::OnPhaseChanged(int oldPhase, int newPhase, NotifyContext* ctx)
{
  if (newPhase != kPhaseFinished) {
    dataOf(this)->m_unk1c = newPhase;
  }
  uint32_t allCanAct;
  uint32_t ok = ComputeAllTeamCanAct(&allCanAct) != 0;

  switch (oldPhase) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case kPhaseFinished:
      break;
    case kPhaseOutcome:
      {
        const int updated = UpdatePhaseForTeam(ctx);
        const bool both = updated && ok;
        const int raised = ArmTimerAfterSetFlag(kEventPhaseStat, 0, this, &dataOf(this)->m_statA, ctx);
        ok = (raised && both) ? 1 : 0;
        break;
      }
    default:
      ok = 0;
  }

  int step = 0;
  bool normalise = true;
  switch (newPhase) {
    case 0:
    case 1:
    case 3:
    case 7:
    case 9:
    case 10:
      normalise = false;
      break;
    case 2:
      step = UpdateLeaderAndTeamKeys(ctx);
      break;
    case kPhaseReadyToStart:
      {
        step = UpdatePhaseForTeam(ctx);
        ok = (step && ok) ? 1 : 0;
        if (!HasCompletionStatus()) {
          RulesMgr* rules = GetRulesMgr();
          ok = rules != nullptr;
          if (rules) {
            MissionPersonIterator members(this);
            members.Begin();
            if (members.Current()) {
              do {
                GameObj* person = members.Current();
                const int moved = rules->BeginTravel(person, &m_originLocationKey, &m_targetLocationKey, ctx);
                const bool first = moved && ok;
                ObjectKey personKey;
                person->GetKey(&personKey);
                const int checked = rules->MoveUnitTo(&personKey, &m_targetKey, ctx);
                ok = (checked && first) ? 1 : 0;
                if (ok && TargetLocationDiffersFromOrigin()) {
                  ok = (person->m_stateFlags & 0xff) >> 5 & 1;
                }
                members.Advance();
              } while (members.Current());
            }
          }
        }
        step = UpdateTeamAndTarget(ctx);
        break;
      }
    case 5:
      if (HasCompletionStatus() || !TargetLocationDiffersFromOrigin()) {
        normalise = false;
        break;
      }
      step = UpdatePhaseForTeam(ctx);
      break;
    case kPhaseFinished:
      step = UpdatePhaseForTeam(ctx);
      break;
    case kPhaseTravel:
      normalise = false;
      if (!HasCompletionStatus() && TargetLocationDiffersFromOrigin()) {
        MissionPersonIterator members(this);
        members.Begin();
        while (members.Current()) {
          const int stopped = members.Current()->SetEnrouteActive(0, ctx);
          ok = (stopped && ok) ? 1 : 0;
          members.Advance();
        }
        const int updated = UpdatePhaseForTeam(ctx);
        ok = (updated && ok) ? 1 : 0;
      }
      break;
    case kPhaseOutcome:
      {
        const int cleared = SetTaskStatus(0, ctx);
        ok = (cleared && ok) ? 1 : 0;
        step = ArmTimerAfterSetFlag(kEventPhaseStat, 1, this, &dataOf(this)->m_statA, ctx);
        break;
      }
    default:
      ok = 0;
      normalise = false;
      break;
  }
  if (normalise) {
    ok = (step && ok) ? 1 : 0;
  }

  const int raised = PostSourceEvent(kEventPhaseChanged, this, oldPhase, newPhase, ctx);
  return (raised && ok) ? 1 : 0;
}

int Mission::CreateViewsImpl(int controlKind, NotifyContext* ctx)
{
  const int result = GameObj::CreateViews(controlKind, ctx);
  if (result) {
    MissionData* data = dataOf(this);
    data->m_statA.SetLevel(GetRecordTimerMinDays());
    data->m_statA.SetProgress(GetRecordTimerSpreadDays());
    data->m_statB.SetLevel(1);
    data->m_statB.SetProgress(0);
    MissionPhaseLevels levels;
    levels.m_levels = (((levels.m_levels & 0xfffffff0) ^ 2) & 0xffff411f) | 0x4110;
    AssignPhaseLevels(&levels);
  }
  return result;
}
