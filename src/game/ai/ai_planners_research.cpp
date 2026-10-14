#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"

using namespace ai_planner;

// ---- 0x53 Research, subtype 1 --------------------------------------------------------------------------------------------

AiResearchPlanner1::AiResearchPlanner1(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiFreePlanner(view, owner, characters)
{
  m_missionKey = ObjectKey::Make(0x53, 0x20);
}

AiResearchPlanner1::~AiResearchPlanner1() = default;

int AiResearchPlanner1::GetPriority()
{
  return SumLeaderValue(this, 2);
}

int AiResearchPlanner1::ShouldDeferAbort()
{
  return NoteTargetSystem(this, 4);
}

int AiResearchPlanner1::GetKindIndexOrSelectLeaders()
{
  return 1;
}

void AiResearchPlanner1::Prepare()
{
  ChooseLeader(this, 0x800, 2, 10);
}

int AiResearchPlanner1::ChooseTarget()
{
  return ai_planner::ChooseTarget(this, 1, 4, 0x39);
}

// ---- 0x53 Research, subtype 2 --------------------------------------------------------------------------------------------

AiResearchPlanner2::AiResearchPlanner2(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiFreePlanner(view, owner, characters)
{
  m_missionKey = ObjectKey::Make(0x53, 0x22);
}

AiResearchPlanner2::~AiResearchPlanner2() = default;

int AiResearchPlanner2::GetPriority()
{
  return SumLeaderValue(this, 3);
}

int AiResearchPlanner2::ShouldDeferAbort()
{
  return NoteTargetSystem(this, 8);
}

int AiResearchPlanner2::GetKindIndexOrSelectLeaders()
{
  return 2;
}

void AiResearchPlanner2::Prepare()
{
  ChooseLeader(this, 0x1000, 3, 10);
}

int AiResearchPlanner2::ChooseTarget()
{
  return ai_planner::ChooseTarget(this, 2, 8, 0x3a);
}

// ---- 0x53 Research, subtype 3 --------------------------------------------------------------------------------------------

AiResearchPlanner3::AiResearchPlanner3(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiFreePlanner(view, owner, characters)
{
  m_missionKey = ObjectKey::Make(0x53, 0x21);
}

AiResearchPlanner3::~AiResearchPlanner3() = default;

int AiResearchPlanner3::GetPriority()
{
  return SumLeaderValue(this, 4);
}

int AiResearchPlanner3::ShouldDeferAbort()
{
  return NoteTargetSystem(this, 0x10);
}

int AiResearchPlanner3::GetKindIndexOrSelectLeaders()
{
  return 3;
}

void AiResearchPlanner3::Prepare()
{
  ChooseLeader(this, 0x2000, 4, 10);
}

int AiResearchPlanner3::ChooseTarget()
{
  return ai_planner::ChooseTarget(this, 4, 0x10, 0x3b);
}

// ---- 0x57 Subdue -----------------------------------------------------------------------------------------------------------

AiSubduePlanner::AiSubduePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiFreePlanner(view, owner, characters)
{
  m_missionKey = ObjectKey::Make(0x57, 0x80);
}

AiSubduePlanner::~AiSubduePlanner() = default;

int AiSubduePlanner::GetPriority()
{
  return SumLeaderValue(this, 6);
}

int AiSubduePlanner::ShouldDeferAbort()
{
  return NoteTargetSystem(this, 0x20);
}

int AiSubduePlanner::GetKindIndexOrSelectLeaders()
{
  return 4;
}

void AiSubduePlanner::Prepare()
{
  ChooseLeader(this, 0x20, 6, 0x31);
}

int AiSubduePlanner::ChooseTarget()
{
  return ai_planner::ChooseTarget(this, 0x10, 0x20, 0x3d);
}
