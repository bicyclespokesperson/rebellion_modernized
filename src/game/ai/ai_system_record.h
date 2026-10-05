#pragma once
#include <cstdint>

#include "ai/ai_posture_decl.h"
#include "ai/ai_selector.h"
#include "core/containers.h"
#include "core/layout.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The AI's plan for one system of its side (0x476140-0x479a40, VT 0x65a3b0, 0xe8 bytes): which ships, fleets, planners and
// agents are committed to the system, how much strength it still lacks (m_demand) and the posture (AiPosture) it follows.
// AiSystemRecordSet creates one per system the side wants to hold or attack. Step() is its state machine: it releases
// what is no longer needed, asks for reinforcements (messages to the AI's own orders) and updates the posture.
//
// m_flagsA (+0x60) and m_flagsB (+0x64) hold the summary bits of the committed forces, see UpdateShortage().

class AiSideController;
class AiMissionPlanner;
class AiAgentBase;

class AiSystemRecord : public IdNode {
public:
    AiSystemRecord();                                       // 0x476140
    AiSystemRecord(int view, AiSideController* owner);      // 0x4762b0
    virtual ~AiSystemRecord();                              // 0: 0x476290 (deleting), 0x476410
    virtual void Read(istream* in);                         // 1: 0x476520
    virtual void Write(ostream* out);                       // 2: 0x4766e0

    void SetSector(const ObjectKey* sector);                // 0x4768b0
    void SetTier(uint32_t tier);                            // 0x4769f0: propagates to the ships, fleets, planners, agents
    int ForwardToPosture();                                 // 0x476b70 (tail call of posture virtual 3)
    int HasWarships();                                      // 0x476b80
    void AddPlanner(AiMissionPlanner* planner);             // 0x476ba0
    void RemovePlanner(AiMissionPlanner* planner);          // 0x476c80
    int HasAgents();                                        // 0x476cf0
    void AddAgent(AiAgentBase* agent);                      // 0x476d10
    void RemoveAgent(AiAgentBase* agent);                   // 0x476de0
    int HasAgent(uint32_t id);                              // 0x476e50
    void GetOrderTargets(int orderKind, ObjectKey* first, ObjectKey* second);  // 0x476e80
    ObjectKey* ChooseReinforcementSource(ObjectKey* out);   // 0x476f90
    void Reset();                                           // 0x477190 (also 0x477620)
    int UpdatePosture();                                    // 0x4773f0
    int Step(int* done);                                    // 0x4774c0
    AiPosture* CreatePosture(int kind);                     // 0x477630: kinds 1-6
    void ReleaseLostShips();                                // 0x477810
    void ReleaseLostFleets();                               // 0x477950
    void* RequestReinforcements();                          // 0x477a40
    void SumCommittedStats();                               // 0x477cb0
    void CheckShortfall(int* done);                         // 0x477e80
    void CountPlanners();                                   // 0x478000
    void CountAgents();                                     // 0x478140
    void* FillShortfallByDemand();                          // 0x4782a0
    void* FillShortfallByClass();                           // 0x478690
    void* ReleaseSurplus();                                 // 0x478aa0
    int AddShip(const ObjectKey* ship);                     // 0x478e00
    int AddFleet(const ObjectKey* fleet);                   // 0x478fd0
    int RemoveShip(uint32_t ship);                          // 0x4790d0
    int RemoveFleet(uint32_t fleet);                        // 0x4791d0
    ObjectKey* ChooseSource(ObjectKey* out, const ObjectKey* hint);  // 0x479220
    void UpdateReserve();                                   // 0x479560
    void UpdateReserveFleet();                              // 0x4799d0
    void UpdateShortage();                                  // 0x479a00

    int m_state;                   // +0x1c
    int m_view;                    // +0x20
    ObjectKeySet m_ships;          // +0x24
    ObjectKeySet m_fleets;         // +0x2c
    ObjectKey m_sectorKey;         // +0x34
    ObjectKey m_reserveShip;       // +0x38
    ObjectKey m_reserveFleet;      // +0x3c
    ObjectKey m_systemKey;         // +0x40
    uint32_t m_tier;               // +0x44
    AiRankedSelector m_selector;   // +0x48
    AiSideController* m_owner;     // +0x58
    AiPosture* m_posture;          // +0x5c
    PersistentInt m_flagsA;        // +0x60
    PersistentInt m_flagsB;        // +0x64
    int m_dirty;                   // +0x68
    int m_sum[10];                 // +0x6c totals of the committed ships' statistics
    int m_x94;                     // +0x94
    int m_x98;                     // +0x98
    int m_x9c;                     // +0x9c
    int m_xa0;                     // +0xa0 planners per class (CountPlanners)
    int m_xa4;                     // +0xa4
    int m_xa8;                     // +0xa8
    int m_xac;                     // +0xac
    IdAvlTree m_planners;          // +0xb0
    int m_xbc;                     // +0xbc
    int m_xc0;                     // +0xc0
    int m_xc4;                     // +0xc4
    int m_xc8;                     // +0xc8 agents per class
    int m_xcc;                     // +0xcc
    int m_xd0;                     // +0xd0
    IdAvlTree m_agents;            // +0xd4
    int m_xe0;                     // +0xe0
    int m_xe4;                     // +0xe4
};

int PlannerClassOf(AiMissionPlanner* planner);   // 0x4794b0
int AgentClassOf(AiAgentBase* agent);            // 0x479520

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(AiSystemRecord, 0xe8);
REB_ASSERT_OFFSET(AiSystemRecord, m_ships, 0x24);
REB_ASSERT_OFFSET(AiSystemRecord, m_selector, 0x48);
REB_ASSERT_OFFSET(AiSystemRecord, m_owner, 0x58);
REB_ASSERT_OFFSET(AiSystemRecord, m_flagsA, 0x60);
REB_ASSERT_OFFSET(AiSystemRecord, m_planners, 0xb0);
REB_ASSERT_OFFSET(AiSystemRecord, m_agents, 0xd4);
#pragma clang diagnostic pop
