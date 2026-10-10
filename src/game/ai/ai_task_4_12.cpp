#include "ai/ai_agent_bases.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_planner.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_types.h"
#include "core/vptr_store.h"
#include "orders/order_base.h"

namespace {

template <class Record>
Record* NextAfter(Record* node) {
    return static_cast<Record*>(node->m_successor);
}

}  // namespace

// ---- type 4 -------------------------------------------------------------------------------------------------------------

AiTask4::AiTask4(int view) : AiStagedTask(4, view), m_selector() {
    m_recordId = 0;
    m_recordCursor = 0;
    m_plannerId = 0;
    m_plannerCursor = 0;
}

AiTask4::~AiTask4() { REB_KEEP_VPTR_STORE(); }

void AiTask4::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_recordId);
    ReadValue32(in, &m_recordCursor);
    ReadValue32(in, &m_plannerId);
    ReadValue32(in, &m_plannerCursor);
    m_selector.Read(in);
}

void AiTask4::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_recordId);
    WriteValue32(out, &m_recordCursor);
    WriteValue32(out, &m_plannerId);
    WriteValue32(out, &m_plannerCursor);
    m_selector.Write(out);
}

OrderBase* AiTask4::Step() { return m_flags.Get() == 1 ? StepRun() : nullptr; }

OrderBase* AiTask4::StepRun() {
    OrderBase* order = nullptr;
    int done = 1;
    switch (m_step) {
    case 1:
        PrepareSidePlanner();
        m_step = 2;
        return nullptr;
    case 2:
        m_step = NextRecordWithPlanners() != 0 ? 3 : 5;
        return nullptr;
    case 3:
        PrepareRecordPlanner();
        m_step = 5;
        return nullptr;
    default:
        m_step = 1;
        break;
    case 5:
        if (NextPlanner() != 0) {
            m_step = 6;
            return nullptr;
        }
        m_step = 0;
        m_finished = 1;
        return nullptr;
    case 6:
        order = RunPlanner(&done);
        if (order) {
            if (done == 0) ++m_priority;
            m_finished = 1;
        }
        if (done != 0) {
            m_step = 0;
            m_finished = 1;
            return order;
        }
        break;
    }
    return order;
}

int AiTask4::PrepareSidePlanner() {
    int created = 0;
    if (m_owner->m_missionTarget != 0) {
        m_owner->m_planners.ReleasePlanner(m_owner->m_missionTarget);
        m_owner->m_missionTarget = 0;
    }
    if (m_owner->m_missionKind != 0) {
        const uint32_t id = m_owner->m_planners.AddPlanner(m_owner->m_missionKind, m_owner->m_missionSubtype);
        AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(id));
        m_owner->m_missionKind = 0;
        if (planner && planner->Virtual04() == 2) {
            m_owner->CountPlanner(planner);
            created = 1;
        }
    }
    return created;
}

int AiTask4::NextRecordWithPlanners() {
    int found = 0;
    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordCursor));
    if (!record) record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.First());
    if (record) {
        do {
            if (found) break;
            found = record->HasWarships();
            if (!found) record = NextAfter(record);
        } while (record);
        if (record) {
            m_recordId = record->m_id;
            AiSystemRecord* next = NextAfter(record);
            m_recordCursor = next ? next->m_id : 0;
            return found;
        }
    }
    m_recordId = 0;
    m_recordCursor = 0;
    return found;
}

int AiTask4::PrepareRecordPlanner() {
    int created = 0;
    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordId));
    if (!record) return created;
    if (record->m_xc4 != 0) {
        m_owner->m_planners.ReleasePlanner(record->m_xc4);
        record->m_xc4 = 0;
    }
    if (record->m_xbc != 0) {
        const uint32_t id = m_owner->m_planners.AddPlanner(record->m_xbc, record->m_xc0);
        AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(id));
        record->m_xbc = 0;
        if (planner && planner->Virtual04() == 1) {
            record->AddPlanner(planner);
            created = 1;
        }
    }
    return created;
}

int AiTask4::NextPlanner() {
    int found = 0;
    AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(m_plannerCursor));
    if (!planner) planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.First());
    if (planner) {
        do {
            if (found) break;
            found = planner->IsActive();
            if (!found) planner = NextAfter(planner);
        } while (planner);
        if (planner) {
            m_plannerId = planner->m_id;
            AiMissionPlanner* next = NextAfter(planner);
            m_plannerCursor = next ? next->m_id : 0;
            return found;
        }
    }
    m_plannerId = 0;
    m_plannerCursor = 0;
    return found;
}

OrderBase* AiTask4::RunPlanner(int* done) {
    AiMissionPlanner* planner = static_cast<AiMissionPlanner*>(m_owner->m_planners.FindById(m_plannerId));
    if (planner) return planner->Step(done);
    *done = 1;
    return nullptr;
}

// ---- type 12 ------------------------------------------------------------------------------------------------------------

AiTask12::AiTask12(int view) : AiStagedTask(12, view), m_selector() {
    m_recordId = 0;
    m_recordCursor = 0;
    m_agentId = 0;
    m_agentCursor = 0;
}

AiTask12::~AiTask12() { REB_KEEP_VPTR_STORE(); }

void AiTask12::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_stage);
    ReadValue32(in, &m_step);
    ReadValue32(in, &m_recordId);
    ReadValue32(in, &m_recordCursor);
    ReadValue32(in, &m_agentId);
    ReadValue32(in, &m_agentCursor);
    m_selector.Read(in);
}

void AiTask12::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_stage);
    WriteValue32(out, &m_step);
    WriteValue32(out, &m_recordId);
    WriteValue32(out, &m_recordCursor);
    WriteValue32(out, &m_agentId);
    WriteValue32(out, &m_agentCursor);
    m_selector.Write(out);
}

OrderBase* AiTask12::Step() { return m_flags.Get() == 1 ? StepRun() : nullptr; }

OrderBase* AiTask12::StepRun() {
    OrderBase* order = nullptr;
    int done = 1;
    switch (m_step) {
    case 1:
        PrepareSideAgent();
        m_step = 2;
        return nullptr;
    case 2:
        m_step = NextRecordWithAgents() != 0 ? 3 : 4;
        return nullptr;
    case 3:
        PrepareRecordAgent();
        m_step = 4;
        return nullptr;
    case 4:
        if (NextAgent() == 0) {
            m_step = 0;
            m_finished = 1;
            return nullptr;
        }
        m_step = 5;
        return nullptr;
    case 5:
        order = RunAgent(&done);
        if (order) {
            if (done == 0) ++m_priority;
            m_finished = 1;
        }
        if (done != 0) {
            m_step = 0;
            m_finished = 1;
            return order;
        }
        break;
    default:
        m_step = 1;
    }
    return order;
}

int AiTask12::PrepareSideAgent() {
    int created = 0;
    if (m_owner->m_agentSystem != 0) {
        m_owner->m_agents.ReleaseAgent(m_owner->m_agentSystem);
        m_owner->m_agentSystem = 0;
    }
    if (m_owner->m_agentTarget != 0) {
        const uint32_t id = m_owner->m_agents.AddAgent(m_owner->m_agentTarget);
        AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(id));
        m_owner->m_agentTarget = 0;
        if (agent && agent->GetAgentClass() == 2) {
            m_owner->CountAgent(static_cast<AiAgentSimple*>(agent));
            created = 1;
        }
    }
    return created;
}

int AiTask12::NextRecordWithAgents() {
    int found = 0;
    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordCursor));
    if (!record) record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.First());
    if (record) {
        do {
            if (found) break;
            found = record->HasAgents();
            if (!found) record = NextAfter(record);
        } while (record);
        if (record) {
            m_recordId = record->m_id;
            AiSystemRecord* next = NextAfter(record);
            m_recordCursor = next ? next->m_id : 0;
            return found;
        }
    }
    m_recordId = 0;
    m_recordCursor = 0;
    return found;
}

int AiTask12::PrepareRecordAgent() {
    int created = 0;
    AiSystemRecord* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_recordId));
    if (!record) return created;
    if (record->m_xe4 != 0) {
        m_owner->m_agents.ReleaseAgent(record->m_xe4);
        record->m_xe4 = 0;
    }
    if (record->m_xe0 != 0) {
        const uint32_t id = m_owner->m_agents.AddAgent(record->m_xe0);
        m_agentId = id;
        AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(id));
        if (agent) {
            if (agent->GetAgentClass() == 1) record->AddAgent(static_cast<AiAgentTargeted*>(agent));
            created = 1;
        }
    }
    return created;
}

int AiTask12::NextAgent() {
    int found = 0;
    AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(m_agentCursor));
    if (!agent) agent = static_cast<AiAgentBase*>(m_owner->m_agents.First());
    if (agent) {
        do {
            if (found) break;
            found = agent->IsAvailable();
            if (!found) agent = NextAfter(agent);
        } while (agent);
        if (agent) {
            m_agentId = agent->m_id;
            AiAgentBase* next = NextAfter(agent);
            m_agentCursor = next ? next->m_id : 0;
            return found;
        }
    }
    m_agentId = 0;
    m_agentCursor = 0;
    return found;
}

OrderBase* AiTask12::RunAgent(int* done) {
    AiAgentBase* agent = static_cast<AiAgentBase*>(m_owner->m_agents.FindById(m_agentId));
    if (agent) return agent->Step(done);
    *done = 1;
    return nullptr;
}
