#pragma once

#include "ai/ai_player.h"

// Strategic campaign commander; the inherited staff handles production, research,
// diplomacy and notices under the same information and order budget as Classic.
class CodexAiPlayer final : public AiPlayer
{
  public:
  CodexAiPlayer() = default;
  explicit CodexAiPlayer(int side)
      : AiPlayer(side)
  {
  }

  int GetKind() override
  {
    return kPlayerAiCodex;
  }
  void Read(istream* in) override;
  void Write(ostream* out) override;
  int StepPlayer() override;

  protected:
  bool AcceptTaskOrder(OrderBase* order) override;

  private:
  void PlanCampaign();
  AiFleetEval* ChooseFlagship();
  AiSystemEval* ChooseObjective(AiFleetEval& flagship);
  bool IssueOrder(int kind, const ObjectKey& unit, const ObjectKey& target);
  bool Reinforce(AiFleetEval& flagship, AiSystemEval& target);
  bool LoadTroops(AiFleetEval& flagship, AiSystemEval& system, int wanted);
  bool Invade(AiFleetEval& flagship, AiSystemEval& system);

  ObjectKey m_flagship;
  ObjectKey m_objective;
  ObjectKey m_reinforcement;
  int m_nextPlanDay = 0;
  int m_objectiveDay = 0;
};
