#pragma once
#include "ai/ai_task_base.h"

// The seven strategy evaluators of AiTask13 (0x4e60c0-0x4ebae0), one per kind of plan. They share the figures they put into the
// side controller (what planner to start or retire, which agent to want, the task record count) and the way they vote; each one
// differs in the sectors it asks the side to work on (Prepare) and in the plan kind it reports.
class AiPlanEvaluator : public AiTaskEvaluator {
public:
    using AiTaskEvaluator::AiTaskEvaluator;
    void Read(istream* in) override;    // 1: 0x4e94d0
    void Write(ostream* out) override;  // 2: 0x4e94e0
    void GetPlannerRequest(int* missionKind, int* missionSubtype, int* plannerKind,
                                   int* plannerSubtype) override;  // 6: 0x4e8290
    void GetAgentRequest(int* agentTarget, int* wantAgent) override;  // 7: 0x4eaaa0
    // 8: keeps the number of task records at what the side's figures ask for
    void Finish() override;  // 0x4e69d0
};

#define REB_PLAN_EVALUATOR(Name)                                                \
    class Name : public AiPlanEvaluator {                                       \
    public:                                                                     \
        Name(int view, AiSideController* owner);                                \
        virtual ~Name();                                                        \
        virtual int GetKind() override;                                         \
        virtual void ScoreVote(const ObjectKey* key, float* chance) override;   \
        virtual void Prepare() override;                                        \
    }

// kind 1 (VT 0x65c8c0): scrap and pool sectors only. Scalar deleting destructor 0x4e60c0, constructor 0x4e60e0, destructor
// 0x4e6100, kind 0x5a66f0 (shared), ScoreVote 0x4e6110, Prepare 0x4e62b0.
REB_PLAN_EVALUATOR(AiEvaluatorPlan1);
// kind 2 (VT 0x65c920): 0x4e77a0, 0x4e77c0, 0x4e77e0, kind 0x4e8740, 0x4e77f0, Prepare 0x4e7990.
REB_PLAN_EVALUATOR(AiEvaluatorPlan2);
// kind 3 (VT 0x65c950): 0x4e8750, 0x4e8770, 0x4e8790, kind 0x5a6730 (shared), 0x4e87a0, Prepare 0x4e8940.
REB_PLAN_EVALUATOR(AiEvaluatorPlan3);
// kind 4 (VT 0x65c980): 0x4e9480, 0x4e94a0, 0x4e94c0, kind 0x5a6700 (shared), 0x4e94f0, Prepare 0x4e9690.
REB_PLAN_EVALUATOR(AiEvaluatorPlan4);
// kind 5 (VT 0x65c9b0): 0x4e9e70, 0x4e9e90, 0x4e9eb0, kind 0x499cd0 (shared), 0x4e9ec0, Prepare 0x4ea060.
REB_PLAN_EVALUATOR(AiEvaluatorPlan5);
// kind 6 (VT 0x65c9e0): 0x4eab60, 0x4eab80, 0x4eaba0, kind 0x48d640 (shared), 0x4eabb0, Prepare 0x4ead50.
REB_PLAN_EVALUATOR(AiEvaluatorPlan6);
// kind 7 (VT 0x65c8f0): 0x4e6a10, 0x4e6a30, 0x4e6a50, kind 0x4e7790, 0x4e6a60, Prepare 0x4e6c00.
REB_PLAN_EVALUATOR(AiEvaluatorPlan7);

#undef REB_PLAN_EVALUATOR
