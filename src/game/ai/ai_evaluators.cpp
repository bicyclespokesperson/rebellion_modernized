#include "ai/ai_evaluators.h"

#include "ai/ai_common.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "fleet/fleet_iterator.h"
#include "gameobj/child_iterator.h"
#include "gameobj/object_lookup.h"

using ai::KeyInFamilies;

namespace
{

constexpr float kMostAccepted = 0.9f;

bool IsSectorKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x80, 0x90) != 0;
}

// ---- ScoreVote: how much of the system's fleet strength the side's own fleets make up ------------------------------------

constexpr uint32_t kSystemVoteable = 1;
constexpr uint32_t kSystemLocked = 0x80000000;
constexpr uint32_t kFleetPresent = 1;

void ScoreVoteOf(AiTaskEvaluator* self, const ObjectKey* key, float* chance)
{
  AiSideController* owner = self->m_owner;
  float result = kMostAccepted;
  AiSystemEval* record = owner->m_systems.FindEval(*key);
  if (record && (record->m_flags30.m_value & kSystemVoteable) && 0 < record->Stat(0xac))
  {
    if (record->m_flags28.m_value & kSystemLocked)
    {
      result = 0.0f;
    }
    else if (GameObjNotifier* system = GetObjectInFamilies_90_98(self->m_view, key))
    {
      int present = 0;
      int absent = 0;
      FleetIterator fleets(system, 3);
      for (fleets.Begin(); fleets.Current(); fleets.Advance())
      {
        ObjectKey fleetKey;
        GetObjectKey(fleets.Current(), &fleetKey);
        AiFleetEval* fleet = owner->m_fleets.FindEval(fleetKey);
        if (!fleet)
          continue;
        (fleet->m_flags.m_value & kFleetPresent ? present : absent) += fleet->Stat(0x58);
      }
      result = static_cast<float>(present);
      if (0 < absent)
        result = static_cast<float>(static_cast<double>(present) / absent);
      if (kMostAccepted < result)
        result = kMostAccepted;
    }
  }
  *chance = result;
}

// ---- Prepare: choose the sectors the evaluator works on ----------------------------------------------------------------

// The flag masks of one candidate query: the sector must carry all of the `require` bits and none of the `forbid` bits.
struct Query
{
  uint32_t requireA;
  uint32_t requireC;
  uint32_t forbidC;
};

constexpr uint32_t kRankDescending = 1;
constexpr uint32_t kRankAscending = 2;
constexpr uint32_t kObjectSectorHeld = 0x8000000;
constexpr uint32_t kUnfilled = 1;

// One run of Prepare: asks the side controller for candidate sectors until `wanted` of them are taken into `picked`, then
// hands the sectors over to the evaluator's object set.
class SectorPass
{
public:
  explicit SectorPass(AiTaskEvaluator* evaluator, bool findOrigin)
  : e(evaluator),
    owner(evaluator->m_owner),
    sel(evaluator->m_selector)
  {
    const int existing = static_cast<uint16_t>(e->m_objects->Count());
    have = existing != 0;
    wanted = existing;
    if (e->m_x14 == 0)
    {
      if (e->m_x18 != 0)
        --wanted;
    }
    else
    {
      ++wanted;
    }
    if (findOrigin)
    {
      AvlNode* first = e->m_objects->First();
      if (first)
        origin = static_cast<AiTaskObject*>(first)->m_key;
    }
  }

  bool NeedsMore() const
  {
    return count < wanted;
  }
  bool HasWork() const
  {
    return 0 < wanted;
  }
  bool PickedSector() const
  {
    return IsSectorKey(key);
  }

  void Rank(const Query& q, uint32_t stat, int descending)
  {
    sel.Add(owner->RankSectors(q.requireA, 0, q.requireC, 0, 0, q.forbidC, stat, descending));
  }
  void RankByAge(const Query& q, int descending)
  {
    sel.Add(owner->RankSectorsByAge(q.requireA, 0, q.requireC, 0, 0, q.forbidC, descending));
  }
  void NearOrigin(const Query& q)
  {
    if (IsSectorKey(origin))
      sel.Add(owner->RankSectorsByDistance(&origin, q.requireA, 0, q.requireC, 0, 0, q.forbidC, kRankAscending));
  }
  void Spread(const Query& q)
  {
    if (!have)
      return;
    sel.Add(owner->RankSectorsByX3c(q.requireA, 0, q.requireC, 0, 0, q.forbidC, kRankAscending));
    sel.Add(owner->RankSectorsByX3c(q.requireA, 0, q.requireC, 0, 0, q.forbidC, kRankAscending));
  }
  void PickKey()
  {
    key = *sel.PickBest(&scratch);
  }
  void PickAndReset()
  {
    PickKey();
    sel.Reset();
  }

  // Rank by one query and the three spreads, take the best.
  void ChooseWith(const Query& q, uint32_t stat, int descending, bool nearOrigin = true)
  {
    Rank(q, stat, descending);
    if (nearOrigin)
      NearOrigin(q);
    Spread(q);
    PickAndReset();
  }

  bool TakePicked(int first, int second)
  {
    if (picked.FindByKey(&key))
      return false;
    picked.AddWithId(NewOrNull<AiTaskObject>(key, first, second));
    ++count;
    return true;
  }

  void TakeSinglePicked(int first, int second = 0)
  {
    if (PickedSector())
      TakePicked(first, second);
  }

  void FillGroup(const Query& q, uint32_t stat, int firstValue)
  {
    if (!NeedsMore())
      return;
    Rank(q, stat, kRankDescending);
    Spread(q);
    PickKey();
    while (PickedSector() && NeedsMore())
    {
      TakePicked(firstValue, 0);
      PickKey();
    }
    sel.Reset();
  }

  void FillFromGroups(uint32_t lastForbid)
  {
    FillGroup({0x40000000, 4, 0}, 0x26, 3);
    FillGroup({0, 0x16, 8}, 0x26, 3);
    FillGroup({0, 6, 0}, 0x26, 3);
    FillGroup({0, 0x80000, lastForbid}, 4, 1);
    if (NeedsMore())
      e->m_flags.m_value |= kUnfilled;
    Commit();
  }

  void Commit()
  {
    if (picked.Count() == 0)
      return;
    for (AvlNode* node = e->m_objects->First(); node; node = node->m_successor)
    {
      if (AiSectorEval* sector = FindSector(static_cast<AiTaskObject*>(node)))
      {
        sector->m_flags2c.m_value &= ~kObjectSectorHeld;
        sector->ClearTarget();
        sector->m_x3c = 0;
      }
    }
    e->m_objects->Clear();
    AvlNode* node = picked.First();
    while (node)
    {
      AiTaskObject* object = static_cast<AiTaskObject*>(node);
      if (AiSectorEval* sector = FindSector(object))
      {
        sector->m_flags2c.m_value |= kObjectSectorHeld;
        sector->SetTarget(object->m_first, object->m_second);
        sector->m_x3c = object->m_id;
      }
      AvlNode* next = node->m_successor;
      picked.DetachById(object->m_id);
      e->m_objects->AddWithId(object);
      node = next;
    }
  }

  AiTaskEvaluator* e;
  AiSideController* owner;
  AiRankedSelector& sel;
  AiTaskObjectSet picked;
  ObjectKey origin;
  ObjectKey key;
  ObjectKey scratch;
  bool have;
  int wanted;
  int count = 0;

private:
  AiSectorEval* FindSector(AiTaskObject* object)
  {
    return static_cast<AiSectorEval*>(owner->m_sectors.Find(&object->m_key));
  }
};

// The first choice of the plans 2 and 7 (and the second of plan 7): the sector nearest the sectors the evaluator already has.
void ChooseNearOwnSectors(SectorPass& pass, int firstValue)
{
  const Query near{0, 0, 2};
  pass.ChooseWith(near, 4, kRankAscending);
  if (pass.PickedSector())
    pass.TakePicked(firstValue, 0);
}

} // namespace

// ---- AiPlanEvaluator ----------------------------------------------------------------------------------------------------

void AiPlanEvaluator::Read(istream* in)
{
  AiTaskEvaluator::Read(in);
}

void AiPlanEvaluator::Write(ostream* out)
{
  AiTaskEvaluator::Write(out);
}

namespace
{

struct PlannerRequest
{
  int* missionKind;
  int* missionSubtype;
  int* plannerKind;
  int* plannerSubtype;

  void Start(int kind, int subtype)
  {
    *missionKind = kind;
    *missionSubtype = subtype;
  }
  void Retire(int kind, int subtype)
  {
    *plannerKind = kind;
    *plannerSubtype = subtype;
  }
  bool CanStart() const
  {
    return *missionKind == 0;
  }
  bool CanRetire() const
  {
    return *plannerKind == 0;
  }
};

} // namespace

void AiPlanEvaluator::GetPlannerRequest(int* missionKind, int* missionSubtype, int* plannerKind, int* plannerSubtype)
{
  PlannerRequest request{missionKind, missionSubtype, plannerKind, plannerSubtype};
  *missionKind = 0;
  *missionSubtype = 0;
  *plannerKind = 0;
  *plannerSubtype = 0;
  AiSideController* owner = m_owner;
  const int* planners = owner->m_plannerCount;
  const int usableCharacters = owner->m_characters.m_usableCount;
  const auto flagSet = [owner](uint32_t mask)
  {
    return (owner->m_flags.Get() & mask) != 0;
  };
  const int records = static_cast<uint16_t>(owner->m_systemRecords.Count());

  int count = planners[0];
  if (request.CanStart() && count < 2 && 0 < usableCharacters)
  {
    request.Start(0x55, 0);
  }
  else if (request.CanRetire() && (2 < count || (usableCharacters == 0 && 0 < count)))
  {
    request.Retire(0x55, 0);
  }

  count = planners[1];
  if (request.CanStart() && !flagSet(0x2000000) && count < owner->Stat(0x2cc))
  {
    request.Start(0x53, 1);
  }
  else if (request.CanRetire() && (owner->Stat(0x2cc) < count || (flagSet(0x2000000) && 0 < count)))
  {
    request.Retire(0x53, 1);
  }

  count = planners[2];
  if (request.CanStart() && !flagSet(0x4000000) && count < owner->Stat(0x2d0))
  {
    request.Start(0x53, 2);
  }
  else if (request.CanRetire() && (owner->Stat(0x2d0) < count || (flagSet(0x4000000) && 0 < count)))
  {
    request.Retire(0x53, 2);
  }

  count = planners[3];
  if (request.CanStart() && !flagSet(0x8000000) && count < owner->Stat(0x2c8))
  {
    request.Start(0x53, 3);
  }
  else if (request.CanRetire() && (owner->Stat(0x2c8) < count || (flagSet(0x8000000) && 0 < count)))
  {
    request.Retire(0x53, 3);
  }

  count = planners[11];
  if (request.CanStart() && count < owner->Stat(0x214))
  {
    request.Start(0x51, 0);
  }
  else if (request.CanRetire() && owner->Stat(0x214) < count)
  {
    request.Retire(0x51, 0);
  }

  count = planners[4];
  if (request.CanStart() && flagSet(0x100) && count < owner->Stat(0x21c) + 1)
  {
    request.Start(0x57, 0);
  }
  else if (request.CanRetire() && (owner->Stat(0x21c) + 1 < count || (!flagSet(0x100) && 0 < count)))
  {
    request.Retire(0x57, 0);
  }

  count = planners[12];
  if (request.CanStart() && count < owner->Stat(0x218))
  {
    request.Start(0x56, 0);
  }
  else if (request.CanRetire() && owner->Stat(0x218) < count)
  {
    request.Retire(0x56, 0);
  }

  if (m_view == 1)
  {
    count = planners[5];
    if (request.CanStart() && count < 1 && flagSet(4))
    {
      request.Start(0x6a, 0);
    }
    else if (request.CanRetire() && (1 < count || (!flagSet(4) && 0 < count)))
    {
      request.Retire(0x6a, 0);
    }
  }

  count = planners[6];
  if (request.CanStart() && count < 1)
  {
    request.Start(0x62, 0);
  }
  else if (request.CanRetire() && 1 < count)
  {
    request.Retire(0x62, 0);
  }

  count = planners[8];
  if (request.CanStart() && flagSet(0x200) && count < 1)
  {
    request.Start(0x61, 0);
  }
  else if (request.CanRetire() && (1 < count || (!flagSet(0x200) && 0 < count)))
  {
    request.Retire(0x61, 0);
  }

  count = planners[9];
  if (request.CanStart() && count < records)
  {
    request.Start(0x54, 5);
  }
  else if (request.CanRetire() && records < count)
  {
    request.Retire(0x54, 5);
  }

  count = planners[10];
  if (request.CanStart() && count < owner->Stat(0x1bc) - records)
  {
    request.Start(0x52, 6);
  }
  else if (request.CanRetire() && owner->Stat(0x1bc) - records < count)
  {
    request.Retire(0x52, 6);
  }
}

void AiPlanEvaluator::GetAgentRequest(int* agentTarget, int* wantAgent)
{
  *agentTarget = 0;
  *wantAgent = 0;
  AiSideController* owner = m_owner;
  const int agents = owner->m_agentCount[0];
  const int wanted = owner->Stat(0x220);
  if (*agentTarget == 0 && agents < wanted)
    *agentTarget = 1;
  else if (wanted < agents)
    *wantAgent = 1;

  if (m_view != 2)
    return;
  const int specialAgents = owner->m_agentCount[1];
  const bool characterWanted = (owner->m_flags.Get() & 0x80) != 0;
  if (*agentTarget == 0 && owner->m_characters.m_usableCount == 0 && characterWanted && specialAgents == 0)
  {
    *agentTarget = 6;
    return;
  }
  if (*wantAgent == 0 && (0 < owner->m_characters.m_usableCount || !characterWanted) && specialAgents == 1)
    *wantAgent = 6;
}

void AiPlanEvaluator::Finish()
{
  AiSideController* owner = m_owner;
  const int records = static_cast<uint16_t>(owner->m_tasks.Count());
  const int wanted = owner->Stat(0x224);
  if (records < wanted)
    owner->m_tasks.CreateRecord();
  else if (wanted < records)
    owner->m_tasks.DeleteFirstUnassigned();
}

// ---- plan 1 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan1::AiEvaluatorPlan1(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan1::~AiEvaluatorPlan1() = default;

int AiEvaluatorPlan1::GetKind()
{
  return 1;
}

void AiEvaluatorPlan1::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan1::Prepare()
{
  SectorPass pass(this, false);
  pass.FillFromGroups(4);
}

// ---- plan 2 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan2::AiEvaluatorPlan2(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan2::~AiEvaluatorPlan2() = default;

int AiEvaluatorPlan2::GetKind()
{
  return 2;
}

void AiEvaluatorPlan2::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan2::Prepare()
{
  SectorPass pass(this, true);
  if (pass.HasWork())
    ChooseNearOwnSectors(pass, 5);
  pass.FillFromGroups(4);
}

// ---- plan 3 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan3::AiEvaluatorPlan3(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan3::~AiEvaluatorPlan3() = default;

int AiEvaluatorPlan3::GetKind()
{
  return 3;
}

void AiEvaluatorPlan3::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan3::Prepare()
{
  SectorPass pass(this, true);
  if (pass.HasWork())
  {
    pass.ChooseWith({0, 0x408, 0x40000}, 0xc, kRankAscending);
    if (!pass.PickedSector())
    {
      const Query fallback{0, 0x408, 0};
      pass.Rank(fallback, 0x48, kRankAscending);
      pass.Spread(fallback);
      pass.PickAndReset();
    }
    int kind = 6;
    if (!pass.PickedSector())
    {
      kind = 5;
      pass.ChooseWith({0, 0, 2}, 3, kRankDescending);
    }
    pass.TakeSinglePicked(kind);
  }
  pass.FillFromGroups(0);
}

// ---- plan 4 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan4::AiEvaluatorPlan4(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan4::~AiEvaluatorPlan4() = default;

int AiEvaluatorPlan4::GetKind()
{
  return 4;
}

void AiEvaluatorPlan4::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan4::Prepare()
{
  SectorPass pass(this, false);
  if (pass.HasWork())
  {
    const int facilityKind = ChooseSector(pass.have, &pass.key);
    pass.TakeSinglePicked(4, facilityKind);
  }
  pass.FillFromGroups(0);
}

// ---- plan 5 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan5::AiEvaluatorPlan5(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan5::~AiEvaluatorPlan5() = default;

int AiEvaluatorPlan5::GetKind()
{
  return 5;
}

void AiEvaluatorPlan5::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan5::Prepare()
{
  SectorPass pass(this, true);
  if (pass.HasWork())
  {
    pass.ChooseWith({0, 0x40000, 0}, 0x48, kRankDescending);
    if (!pass.PickedSector())
    {
      const Query unowned{0, 8, 0x20000000};
      pass.RankByAge(unowned, kRankDescending);
      pass.NearOrigin(unowned);
      pass.Spread(unowned);
      pass.PickAndReset();
    }
    pass.TakeSinglePicked(2);
  }
  pass.FillFromGroups(0);
}

// ---- plan 6 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan6::AiEvaluatorPlan6(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan6::~AiEvaluatorPlan6() = default;

int AiEvaluatorPlan6::GetKind()
{
  return 6;
}

void AiEvaluatorPlan6::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan6::Prepare()
{
  SectorPass pass(this, true);
  if (pass.HasWork())
  {
    int kind = 6;
    const bool empireWithAccess = (m_owner->m_flags.Get() & 0x400) != 0 && m_view == 2;
    if (empireWithAccess)
    {
      pass.Rank({0, 0x80000408, 0}, 0x4b, kRankDescending);
      pass.PickAndReset();
    }
    if (!pass.PickedSector())
      pass.ChooseWith({0, 0x408, 0}, 0x4b, kRankDescending);
    if (!pass.PickedSector())
    {
      pass.Rank({0, 0x408, 0}, 0x30, kRankDescending);
      pass.NearOrigin({0, 8, 0});
      pass.Spread({0, 0x408, 0});
      pass.PickAndReset();
    }
    if (!pass.PickedSector())
    {
      kind = 1;
      pass.ChooseWith({0, 0x80100, 0}, 4, kRankDescending);
    }
    if (!pass.PickedSector())
    {
      kind = 5;
      pass.ChooseWith({0, 0, 2}, 3, kRankDescending);
    }
    pass.TakeSinglePicked(kind);
  }
  pass.FillFromGroups(0);
}

// ---- plan 7 -------------------------------------------------------------------------------------------------------------

AiEvaluatorPlan7::AiEvaluatorPlan7(int view, AiSideController* owner) : AiPlanEvaluator(view, owner)
{
}

AiEvaluatorPlan7::~AiEvaluatorPlan7() = default;

int AiEvaluatorPlan7::GetKind()
{
  return 7;
}

void AiEvaluatorPlan7::ScoreVote(const ObjectKey* key, float* chance)
{
  ScoreVoteOf(this, key, chance);
}

void AiEvaluatorPlan7::Prepare()
{
  SectorPass pass(this, true);
  if (pass.HasWork())
    ChooseNearOwnSectors(pass, 5);
  if (pass.NeedsMore())
  {
    const Query best{0, 0x408, 0x40000};
    pass.Rank(best, 0xc, kRankAscending);
    pass.Spread(best);
    pass.PickKey();
    if (!pass.PickedSector())
    {
      pass.sel.Reset();
      const Query fallback{0, 0x408, 0};
      pass.Rank(fallback, 0x48, kRankAscending);
      pass.Spread(fallback);
      pass.PickKey();
    }
    bool taken = false;
    while (pass.PickedSector() && !taken)
    {
      taken = pass.TakePicked(6, 0);
      pass.PickKey();
    }
    pass.sel.Reset();
  }
  pass.FillFromGroups(4);
}

ObjectDatabase* g_objectDatabase{};
ObjectKeySet* g_unresolvedKeys{};
