#include <limits>
#include <new>

#include "ai/ai_advisor_tasks.h"
#include "app/player_controller_decl.h"
#include "app/rebapp_globals.h"
#include "core/stream_io.h"
#include "events/game_clock_hooks.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_decl.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/human_player.h"

namespace
{

constexpr int kNetworkModeSkipsSpeech1 = 1;
constexpr int kNetworkModeSkipsSpeech2 = 4;
constexpr uint32_t kFlagSpeechOff = 0x8000;
constexpr uint32_t kFlagNoticesCollected = 0x10000000;
constexpr int kTaskReady = 2;
constexpr int kNoticeKindMask = 0xf;
constexpr uint32_t kSpeechIdleTicks = 300;
constexpr int kInitialMood = 8;
constexpr int kInitialBatch = 5;
constexpr int kPrimaryTaskId = 0x17;
constexpr int kTaskIdA = 0x14;
constexpr int kTaskIdB = 0x15;

int TaskState(AdvisorTaskBase* task)
{
  return task->m_flags.Get();
}
void SetTaskStateRaw(AdvisorTaskBase* task, int value)
{
  task->m_flags.m_value = value;
}
int NoticeKind(int notice)
{
  return notice & kNoticeKindMask;
}

void ReadNotices(istream* in, std::map<uint32_t, int>& notices)
{
  uint16_t count = 0;
  ReadValue16(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
    return;
  }
  uint32_t nextId = 1;
  while (count-- != 0) {
    uint32_t id = 0;
    int value = 0;
    ReadValue32(in, &id);
    ReadValue32(in, &value);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
      return;
    }
    if (id == 0) {
      if (nextId == 0) {
        iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
        return;
      }
      id = nextId++;
    } else if (nextId != 0 && nextId <= id) {
      nextId = id + 1;
    }
    if (!notices.emplace(id, value).second) {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

void WriteNotices(ostream* out, const std::map<uint32_t, int>& notices)
{
  if (notices.size() > std::numeric_limits<uint16_t>::max()) {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return;
  }
  auto count = static_cast<uint16_t>(notices.size());
  WriteValue16(out, &count);
  for (const auto& [id, value] : notices) {
    uint32_t idValue = id;
    int noticeValue = value;
    WriteValue32(out, &idValue);
    WriteValue32(out, &noticeValue);
  }
}

} // namespace

// ---- Construction / persistence -------------------------------------------------------------------------------

AdvisorBase::AdvisorBase(int side, HumanPlayer* player)
    : m_flags(0)
{
  m_unk04 = 1;
  m_enabled = 1;
  m_state = 1;
  m_player = player;
  m_mood = kInitialMood;
  m_currentTask = nullptr;
  m_unk38 = 0;
  m_side = side;
  m_aiHelper.Init(side);
  m_taskTree.m_view = side;
  m_unk160 = 0;
  g_advisorNagCount = 0;
  m_cueCount = 0;
  m_cueDue = nullptr;
  m_cueCooldown = nullptr;
  g_advisorLastRun = 0;
  g_advisorNow = 0;
  m_lastSpeechTime = 0;
  m_unk14c = 8;
  g_advisorMaySpeak = 0;
  g_advisorFlag0 = 0;
  g_humanUiFlags.m_value = 0;
  g_advisorPendingMode = 0;
  g_advisorPendingArg = 0;
  m_statusRepeat = 0;
  m_lastStatus.m_first = 0;
  m_lastStatus.m_second = 0;
  m_primaryTask = nullptr;
  m_unk188 = 0;
  m_unk18c = 0;
  m_unk190 = 0;
  const int networkMode = AppGetNetworkMode();
  if (networkMode != kNetworkModeSkipsSpeech1 && networkMode != kNetworkModeSkipsSpeech2) {
    g_humanUiFlags.m_value |= kFlagSpeechOff;
  }
}

AdvisorBase::~AdvisorBase()
{
  m_player = nullptr;
  m_primaryTask = nullptr;
  if (m_cueCooldown) {
    ::operator delete(m_cueCooldown);
    m_cueCooldown = nullptr;
  }
}

void AdvisorBase::Read(istream* in, HumanPlayer* player)
{
  int taskId = 0;
  m_player = player;
  ReadValue32(in, &m_unk04);
  ReadValue32(in, &m_enabled);
  ReadValue32(in, &m_side);
  ReadValue32(in, &m_state);
  ReadValue32(in, &g_advisorMaySpeak);
  ReadValue32(in, &g_advisorFlag0);
  ReadValuePersistentInt(in, &g_humanUiFlags);
  ReadValue32(in, &g_advisorPendingMode);
  ReadValue32(in, &g_advisorPendingArg);
  m_aiHelper.Load(in);
  m_cueTree.Load(in);
  ReadValue32(in, &m_unk38);
  m_taskTree.Load(in, &m_aiHelper);
  ReadValue32(in, &taskId);
  m_currentTask = static_cast<AdvisorTaskBase*>(Tasks()->FindById(taskId));
  ReadValue8(in, &m_mood);
  ReadValue32(in, &m_unk160);
  ReadValue32(in, &g_advisorNagCount);
  ReadValue32(in, &m_cueCount);
  ReadValue32(in, &m_unk188);
  ReadValue32(in, &m_unk18c);
  ReadValue32(in, &m_unk190);
  if (m_cueDue) {
    ::operator delete(m_cueDue);
  }
  if (m_cueCooldown) {
    ::operator delete(m_cueCooldown);
  }
  if (m_cueCount != 0) {
    m_cueDue = static_cast<uint32_t*>(::operator new(m_cueCount * sizeof(uint32_t)));
    m_cueCooldown = static_cast<uint32_t*>(::operator new(m_cueCount * sizeof(uint32_t)));
    for (uint32_t i = 0; i < m_cueCount; ++i) {
      ReadValue32(in, &m_cueDue[i]);
      m_cueCooldown[i] = 0;
    }
  }
  ReadValue32(in, &m_lastSpeechTime);
  ReadValue32(in, &m_unk14c);
  m_flags.Read(in);
  m_notices.clear();
  ReadNotices(in, m_notices);
  m_primaryTask = static_cast<AdvisorTask23*>(Tasks()->FindById(kPrimaryTaskId));
  m_primaryTask->m_advisor = this;
}

void AdvisorBase::Write(ostream* out)
{
  uint32_t taskId = 0;
  WriteValue32(out, &m_unk04);
  WriteValue32(out, &m_enabled);
  WriteValue32(out, &m_side);
  WriteValue32(out, &m_state);
  WriteValue32(out, &g_advisorMaySpeak);
  WriteValue32(out, &g_advisorFlag0);
  WriteValuePersistentInt(out, &g_humanUiFlags);
  WriteValue32(out, &g_advisorPendingMode);
  WriteValue32(out, &g_advisorPendingArg);
  m_aiHelper.Save(out);
  m_cueTree.Save(out);
  WriteValue32(out, &m_unk38);
  m_taskTree.Save(out);
  if (m_currentTask) {
    taskId = m_currentTask->m_id;
  }
  WriteValue32(out, &taskId);
  WriteValue8(out, &m_mood);
  WriteValue32(out, &m_unk160);
  WriteValue32(out, &g_advisorNagCount);
  WriteValue32(out, &m_cueCount);
  WriteValue32(out, &m_unk188);
  WriteValue32(out, &m_unk18c);
  WriteValue32(out, &m_unk190);
  if (m_cueCount != 0) {
    for (uint32_t i = 0; i < m_cueCount; ++i) {
      WriteValue32(out, &m_cueDue[i]);
    }
  }
  WriteValue32(out, &m_lastSpeechTime);
  WriteValue32(out, &m_unk14c);
  m_flags.Write(out);
  WriteNotices(out, m_notices);
}

// ---- The task machine -----------------------------------------------------------------------------------------

int AdvisorBase::StepStartup()
{
  switch (m_state) {
    case 1:
      LoadCueScripts();
      m_state = 2;
      break;
    case 2:
      if (m_aiHelper.Step() != 0) {
        m_state = 3;
      }
      break;
    case 3:
      {
        m_taskTree.CreateTask(kTaskIdA, &m_aiHelper);
        m_taskTree.CreateTask(kTaskIdB, &m_aiHelper);
        m_taskTree.CreateTask(kPrimaryTaskId, &m_aiHelper);
        m_unk18c = kInitialBatch;
        ResetTasks();
        m_primaryTask = static_cast<AdvisorTask23*>(Tasks()->FindById(kPrimaryTaskId));
        m_primaryTask->m_advisor = this;
        m_state = 5;
        return 1;
      }
  }
  return 0;
}

int AdvisorBase::StepTasks()
{
  int result = 0;
  if (m_state == 4) {
    AdvisorTaskBase* task = m_currentTask;
    if (!task) {
      m_currentTask = m_taskTree.SelectNext();
      if (m_currentTask) {
        return 0;
      }
    } else if (m_unk188 < m_unk18c || task->m_id == static_cast<uint32_t>(kPrimaryTaskId)) {
      task->m_count = 0;
      OrderBase* order = task->Step();
      if (order) {
        IntPair status;
        if (order->CheckValid(&status) == 0) {
          task->Refused(reinterpret_cast<int*>(&status), order);
        } else {
          m_player->SubmitMessage(static_cast<PlayerMessage*>(order));
          if (task->m_id != static_cast<uint32_t>(kPrimaryTaskId)) {
            ++m_unk188;
          }
        }
      }
      if (task->m_finished == 0) {
        return 0;
      }
      if (TaskState(task) != kTaskReady) {
        SetTaskStateRaw(task, 0);
      }
      m_currentTask = nullptr;
    }
    m_state = 5;
    result = 1;
  } else if (m_state == 5) {
    if (m_aiHelper.AnalyzeStep() != 0) {
      m_state = 6;
      return 0;
    }
  } else if (m_state == 6) {
    auto taskPosition = m_currentTask ? Tasks()->PositionOf(m_currentTask) : Tasks()->Entries().begin();
    if (taskPosition == Tasks()->Entries().end()) {
      taskPosition = Tasks()->Entries().begin();
    }
    if (taskPosition == Tasks()->Entries().end()) {
      return 0;
    }
    m_currentTask = taskPosition->second.get();
    m_currentTask->Evaluate();
    ++taskPosition;
    m_currentTask = taskPosition == Tasks()->Entries().end() ? nullptr : taskPosition->second.get();
    if (!m_currentTask) {
      m_state = 4;
      return 0;
    }
  } else {
    m_state = 5;
    return 0;
  }
  return result;
}

int AdvisorBase::Tick()
{
  return Update();
}

// ---- Speech ---------------------------------------------------------------------------------------------------

void AdvisorBase::SpeakIfIdle()
{
  if ((g_humanUiFlags.m_value & kFlagSpeechOff) == 0 && m_enabled != 0
      && m_lastSpeechTime + kSpeechIdleTicks <= g_advisorNow) {
    SpeakNow();
  }
}

void AdvisorBase::ResetSpeech()
{
  m_lastStatus.m_first = 0;
  m_statusRepeat = 0;
  g_advisorNagCount = 0;
}

int AdvisorBase::IsBusy()
{
  return m_primaryTask && TaskState(m_primaryTask) == kTaskReady;
}

void AdvisorBase::SetTaskState(const ObjectKey* target)
{
  if (m_primaryTask) {
    m_primaryTask->SetTarget(target);
  }
}

void AdvisorBase::SelectTaskMode(int command)
{
  if (m_enabled == 0) {
    return;
  }
  if (command == 0x110) {
    m_primaryTask->Start(2);
  } else if (command == 0x111) {
    m_primaryTask->Start(3);
  } else if (command == 0x112) {
    m_primaryTask->Start(1);
  }
}

void AdvisorBase::ToggleTask(unsigned command)
{
  AdvisorTaskBase* task = nullptr;
  unsigned toggledOn = command;
  unsigned toggledOff = command;
  if (m_enabled != 0) {
    if (command == 0x115) {
      task = static_cast<AdvisorTaskBase*>(Tasks()->FindById(kTaskIdB));
      toggledOn = 0x100;
      toggledOff = 0x10;
    } else if (command == 0x116) {
      task = static_cast<AdvisorTaskBase*>(Tasks()->FindById(kTaskIdA));
      toggledOn = 0x200;
      toggledOff = 0x20;
    }
  }
  if (!task) {
    return;
  }
  if (TaskState(task) == kTaskReady) {
    SetTaskStateRaw(task, 0);
    ++m_unk160;
    if (m_unk160 > 3) {
      g_humanUiFlags.m_value |= 0x1000000;
      return;
    }
    g_humanUiFlags.m_value |= toggledOff;
    return;
  }
  g_humanUiFlags.m_value |= toggledOn;
  SetTaskStateRaw(task, kTaskReady);
  --m_unk160;
}

int AdvisorBase::IsTaskReady(int command)
{
  AdvisorTaskBase* task = nullptr;
  if (m_enabled != 0) {
    if (command == 0x115) {
      task = static_cast<AdvisorTaskBase*>(Tasks()->FindById(kTaskIdB));
    } else if (command == 0x116) {
      task = static_cast<AdvisorTaskBase*>(Tasks()->FindById(kTaskIdA));
    }
  }
  if (task && TaskState(task) != kTaskReady) {
    return 1;
  }
  return 0;
}

void AdvisorToggleUiFlag(int command)
{ // 0x439e80
  if (command == 0x119) {
    return;
  }
  if (command == 0x11b) {
    g_humanUiFlags.m_value ^= 0x1000;
  } else if (command == 0x11e) {
    g_humanUiFlags.m_value ^= kFlagSpeechOff;
  }
}

void AdvisorSetPendingCommand(int mode, int argument)
{ // 0x439eb0
  g_advisorPendingMode = mode;
  g_advisorPendingArg = argument;
}

void AdvisorNoteNag()
{ // 0x439ed0
  g_humanUiFlags.m_value |= 0x2000000;
  ++g_advisorNagCount;
}

void AdvisorAllowSpeech()
{ // 0x439ef0
  g_advisorMaySpeak = 1;
  if (g_advisorLastRun + 0x32 < g_advisorNow) {
    g_humanUiFlags.m_value |= 0x80000000;
  }
}

void AdvisorSetFlag0()
{
  g_advisorFlag0 = 1;
} // 0x43a1f0

void AdvisorRequestIdle()
{
  g_humanUiFlags.m_value |= 0x2000;
} // 0x43a200

void AdvisorBase::FlushPending()
{
  if ((m_flags.m_value & kFlagNoticesCollected) != 0) {
    return;
  }
  if (LoadAdvisorNotices(m_side, &m_notices) != 0) {
    m_flags.m_value |= kFlagNoticesCollected;
  }
  auto notice = m_notices.end();
  while (notice != m_notices.begin()) {
    --notice;
    if (NoticeKind(notice->second) == 7 || g_advisorFlushAll != 0) {
      m_player->Dispatch()->AddCharacterNotice(notice->second);
      notice = m_notices.erase(notice);
    }
  }
}

void AdvisorBase::SpeakNow()
{
  auto noticeEnd = m_notices.end();
  if ((m_flags.m_value & kFlagNoticesCollected) == 0) {
    FlushPending();
  }
  auto noticePosition = m_notices.begin();
  uint32_t found = 0;
  while (noticePosition != noticeEnd && found == 0) {
    switch (noticePosition->second & kNoticeKindMask) {
      default:
        found = 1;
        break;
      case 2:
        found = (m_flags.m_value >> 1) & 1;
        break;
      case 3:
        found = (m_flags.m_value >> 2) & 1;
        break;
      case 4:
        found = (m_flags.m_value >> 4) & 1;
        break;
      case 5:
        found = (m_flags.m_value >> 3) & 1;
        break;
      case 6:
        found = m_flags.m_value & 1;
    }
    if (found == 0) {
      ++noticePosition;
    } else {
      m_player->Dispatch()->AddCharacterNotice(noticePosition->second);
      m_notices.erase(noticePosition);
      break;
    }
  }
  m_lastSpeechTime = GameHookTickCount();
}

void AdvisorBase::SpeakTopic(unsigned topic)
{
  if ((g_humanUiFlags.m_value & kFlagSpeechOff) != 0 || m_enabled == 0) {
    return;
  }
  if ((m_flags.m_value & kFlagNoticesCollected) == 0) {
    FlushPending();
  }
  if (topic == 0 || (m_flags.m_value & topic) != 0) {
    return;
  }
  m_flags.m_value |= topic;
  uint32_t kind = 0;
  switch (topic) {
    case 1:
      kind = 6;
      break;
    case 2:
      kind = 2;
      break;
    case 4:
      kind = 3;
      break;
    case 8:
      kind = 5;
      break;
    case 0x10:
      kind = 4;
  }
  if (kind == 0) {
    return;
  }
  bool told = false;
  auto noticePosition = m_notices.begin();
  auto noticeEnd = m_notices.end();
  while (noticePosition != noticeEnd && !told) {
    if (static_cast<uint32_t>(NoticeKind(noticePosition->second)) == kind) {
      m_player->Dispatch()->AddCharacterNotice(noticePosition->second);
      told = true;
      m_lastSpeechTime = GameHookTickCount();
      m_notices.erase(noticePosition);
      break;
    }
    ++noticePosition;
  }
}

void AdvisorBase::SetAiHelperFlag(intptr_t value)
{
  m_aiHelper.SetFocusKey(reinterpret_cast<const uint32_t*>(static_cast<intptr_t>(value)));
}
void AdvisorBase::ClearAiHelperFlag(int value)
{
  m_aiHelper.SetFocusValue(value);
}

void AdvisorBase::ResetTasks()
{
  for (const auto& ownedNode : Tasks()->Entries()) {
    ownedNode.second->m_flags.m_value = kTaskReady;
  }
}

int AdvisorBase::PickAdvice()
{
  if (m_unk190 > 0) {
    --m_unk190;
    return UiSessionRandom(6) / 2 + 1;
  }
  m_unk190 = 3;
  return PickMoodCue();
}

void AdvisorBase::SetRefusedOrder(const void* order)
{
  m_refusedOrder = order;
}

void AdvisorBase::OnOrderSubmitted(const void* order)
{
  if ((g_humanUiFlags.m_value & kFlagNoticesCollected) != 0 && order == m_refusedOrder) {
    g_humanUiFlags.m_value |= 1;
  }
  g_humanUiFlags.m_value &= ~kFlagNoticesCollected;
}
