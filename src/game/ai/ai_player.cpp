#include "ai/ai_player.h"

#include "ai/ai_task_types.h"

#include "core/game_string.h"
#include "events/game_clock_hooks.h"
#include "gameobj/status_code.h"
#include "orders/order_base.h"
#include "strategy_ui/game_ui_facade.h"

namespace
{

constexpr int kAdvisorOrderKind = 0x203;
constexpr int kVoteTaskType = 0xd;

// The order the tasks are created in (their ids are the task types).
constexpr int kTaskCreationOrder[] = {0xd, 2, 1, 3, 6, 7, 4, 5, 8, 9, 10, 0xb, 0xc, 0xe};

} // namespace

AiPlayer::AiPlayer()
    : PlayerController()
    , m_notices()
    , m_tasks()
    , m_model()
{
  m_state = 0;
  m_currentTask = nullptr;
  m_tasksOff = 0;
  m_turnCount = 0;
  m_ordersSent = 0;
  m_turnsPerBudget = 0;
  m_ordersPerBudget = 0;
}

AiPlayer::AiPlayer(int side)
    : PlayerController(side)
    , m_notices()
    , m_tasks()
    , m_model()
{
  m_state = 1;
  m_currentTask = nullptr;
  m_model.SetSide(side);
  m_tasks.m_view = side;
  m_tasksOff = 0;
  m_turnCount = 0;
  m_ordersSent = 0;
  m_turnsPerBudget = 0;
  m_ordersPerBudget = 0;
}

AiPlayer::~AiPlayer() = default;

void AiPlayer::Read(istream* in)
{
  uint32_t currentId = 0;
  PlayerController::Read(in);
  m_model.Load(in);
  m_notices.Load(in);
  m_tasks.Load(in, &m_model);
  ReadValue32(in, &m_state);
  ReadValue32(in, &currentId);
  m_currentTask = static_cast<AiTaskBase*>(m_tasks.FindById(currentId));
  ReadValue32(in, &m_tasksOff);
  ReadValue32(in, &m_turnCount);
  ReadValue32(in, &m_ordersSent);
  ReadValue32(in, &m_turnsPerBudget);
  ReadValue32(in, &m_ordersPerBudget);
}

void AiPlayer::Write(ostream* out)
{
  uint32_t currentId = 0;
  PlayerController::Write(out);
  m_model.Save(out);
  m_notices.Save(out);
  m_tasks.Save(out);
  WriteValue32(out, &m_state);
  if (m_currentTask) {
    currentId = m_currentTask->m_id;
  }
  WriteValue32(out, &currentId);
  WriteValue32(out, &m_tasksOff);
  WriteValue32(out, &m_turnCount);
  WriteValue32(out, &m_ordersSent);
  WriteValue32(out, &m_turnsPerBudget);
  WriteValue32(out, &m_ordersPerBudget);
}

int AiPlayer::StepInitialization()
{
  if (m_state == 1) {
    if (m_model.Step() != 0) {
      m_state = 2;
    }
    return 0;
  }
  if (m_state != 2) {
    m_state = 1;
    return 0;
  }
  for (const int type : kTaskCreationOrder) {
    m_tasks.CreateTask(type, &m_model);
  }
  m_ordersPerBudget = 10;
  m_state = 4;
  return 1;
}

int AiPlayer::StepPlayer()
{
  int result = 0;
  if (m_state == 3) {
    bool ranTask = false;
    if (m_ordersSent < m_ordersPerBudget) {
      if (m_currentTask == nullptr) {
        m_currentTask = m_tasks.SelectNext();
        if (m_currentTask != nullptr) {
          return result;
        }
      } else {
        m_currentTask->m_priority = 0;
        OrderBase* order = m_currentTask->Step();
        if (order) {
          StatusCode status;
          ranTask = true;
          if (!AcceptTaskOrder(order) || order->CheckValid(&status) == 0) {
            m_currentTask->Refused(&result, order);
          } else {
            Send(order);
            if (order->GetKind() != kAdvisorOrderKind) {
              ++m_ordersSent;
            }
          }
        }
        if (m_currentTask->m_finished == 0) {
          return result;
        }
        m_currentTask->m_flags.m_value = 0;
        m_currentTask = nullptr;
        if (!ranTask && (static_cast<uint32_t>(m_model.m_flags.m_value) & 0x80000000) == 0) {
          return result;
        }
      }
    }
    m_state = 4;
    return 1;
  }
  if (m_state == 4) {
    if (m_model.Analyze() == 0) {
      return result;
    }
    m_state = 5;
    return result;
  }
  if (m_state != 5) {
    m_state = 4;
    return 0;
  }
  if (m_tasksOff == 0) {
    if (m_currentTask == nullptr) {
      m_currentTask = m_tasks.First();
    }
    m_currentTask->Evaluate();
    auto position = m_tasks.PositionOf(m_currentTask);
    if (position != m_tasks.Entries().end()) {
      ++position;
    }
    m_currentTask = position == m_tasks.Entries().end() ? nullptr : position->second.get();
  }
  if (m_currentTask != nullptr) {
    return result;
  }
  m_state = 3;
  return result;
}

void AiPlayer::ProcessPlayerFrame()
{
  AiNoticeGroup* group = m_notices.DetachFinished();
  if (group) {
    delete group;
  }
}

void AiPlayer::BeginVote(int id, ObjectKey* key)
{
  ClearGameTime(&m_voteTime);
  if (m_tasksOff == 0) {
    auto* task = static_cast<AiTaskBase*>(m_tasks.FindById(kVoteTaskType));
    if (task) {
      static_cast<AiTask13*>(task)->ForwardVote(key, &m_voteTime);
    }
  }
  m_voteId = id;
  m_voteKey = *key;
  m_voteState = 1;
}

void AiPlayer::ResetVote(int id, ObjectKey* key)
{
  m_voteState = 3;
  m_voteId = id;
  m_voteKey = *key;
  ClearGameTime(&m_voteTime);
}

void AiPlayer::HandleNotification(NotifPayload* notice)
{
  if (!notice) {
    return;
  }
  notice->Dispatch(this);
  if (notice->m_value0c != 0) {
    notice->DispatchToSink(nullptr);
  }
  m_notices.Add(std::unique_ptr<NotifPayload>(notice));
}

void AiPlayer::AdvanceOrderBudget()
{
  ++m_turnCount;
  if (m_turnsPerBudget <= m_turnCount) {
    m_turnCount = 0;
    m_ordersSent = 0;
  }
}

void AiPlayer::UpdateMaintenanceAndAdvisorState(intptr_t value)
{
  m_model.NoteValueA(reinterpret_cast<const int*>(static_cast<intptr_t>(value)));
}

void AiPlayer::ClearAdvisorStateAndUpdateMaintenance(int value)
{
  m_model.NoteValueB(value);
}

void AiPlayer::HandleAdvisorFlags(unsigned flags)
{
  m_model.NoteMask(flags);
}

void AiPlayer::OnMissionEvent(const ObjectKey* key, int argument)
{
  m_model.OnMissionEvent(reinterpret_cast<const int*>(key), argument);
}

void AiPlayer::OnPlannerEvent(const ObjectKey* key, int argument)
{
  m_model.OnPlannerEvent(reinterpret_cast<const int*>(key), argument);
}

void AiPlayer::OnObjectAndSystemChanged(const ObjectKey* key, intptr_t, const ObjectKey* system)
{
  m_model.NoteObject(key);
  m_model.NoteSystemTouched(system);
}

void AiPlayer::OnObjectMoved(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to)
{
  m_model.NoteMoved(key, from, to);
}

void AiPlayer::OnSystemChanged(const ObjectKey* key, int)
{
  m_model.NoteSystemChanged(key);
  m_model.NoteObject(key);
}

void AiPlayer::NotifyShipChanged(const ObjectKey* key)
{
  m_model.NoteShipChanged(key);
}

void AiPlayer::OnObjectReparented(const ObjectKey* key, const ObjectKey* newParent)
{
  m_model.NoteObject(newParent);
  m_model.NoteObjectReplaced(key);
}

void AiPlayer::RequestSyncTarget(unsigned target)
{
  m_syncTarget = target;
  UiAckSync(m_side);
}

void AiPlayer::AcknowledgeSync(int side)
{
  if (m_side == side) {
    m_syncTarget = 0;
    return;
  }
  if (m_syncTarget != 0) {
    UiAckSync(m_side);
  }
}
