#include "ai/ai_agents.h"

#include "ai/ai_character_records.h"
#include "ai/ai_order_util.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"

using ai::IsSectorKey;
using ai::IsSystemKey;
using ai::KeyInFamilies;

namespace
{

constexpr uint32_t kCharacterFirst = 0x30;
constexpr uint32_t kCharacterLast = 0x40;

// AiCharacterRecord::m_flags
constexpr uint32_t kCharacterBusy = 0x50;
constexpr uint32_t kCharacterReleased = 0x800000;
constexpr uint32_t kCharacterAssignable = 0x800002;

// AiAgentBase::m_flags
constexpr uint32_t kAgentHasCharacter = 1;
constexpr uint32_t kAgentHasTarget = 2;

bool IsCharacterKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kCharacterFirst, kCharacterLast) != 0;
}

AiCharacterRecord* FindCharacter(AiAgentBase* agent, const ObjectKey& key)
{
  return static_cast<AiCharacterRecord*>(agent->m_characters->Find(&key));
}

// Gives the character back to the pool (what an agent does with one that stopped being usable).
void GiveBackCharacter(AiCharacterRecord* character)
{
  character->m_flags.m_value |= kCharacterReleased;
  character->m_x24 = 0;
  character->m_eligibilityRank = 0;
  character->m_agentId = 0;
}

// The agent takes the character: it becomes the agent's character and the record points back at the agent.
void TakeCharacter(AiAgentBase* agent, AiCharacterRecord* character, const ObjectKey& key, int rank)
{
  agent->m_characterKey = key;
  character->m_flags.m_value &= 0xff7fffff;
  character->m_x24 = 0;
  character->m_agentId = agent->m_id;
  character->m_eligibilityRank = rank;
  character->MarkChanged();
  agent->m_flags.m_value |= kAgentHasCharacter;
}

// Drops the agent's character if it no longer belongs to the agent; returns the record while it still does.
AiCharacterRecord* CurrentCharacter(AiAgentBase* agent)
{
  if (!IsCharacterKey(agent->m_characterKey)) {
    return nullptr;
  }
  return FindCharacter(agent, agent->m_characterKey);
}

} // namespace

// ---- kind 6
// -----------------------------------------------------------------------------------------------------------

AiAgentKind6::AiAgentKind6(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiAgentSimple(view, owner, characters)
{
}

AiAgentKind6::~AiAgentKind6() = default;

int AiAgentKind6::GetCountSlot()
{
  return 1;
}

void AiAgentKind6::Prepare()
{
  m_flags.m_value &= ~kAgentHasTarget;
  if (IsSystemKey(m_targetKey)) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_targetKey);
    if (system) {
      if ((system->m_flags30.m_value & 0x40000000) == 0 || (system->m_flags30.m_value & 1) == 0) {
        ReleaseHook();
      } else {
        m_flags.m_value |= kAgentHasTarget;
      }
    }
  }
  if ((m_flags.m_value & kAgentHasTarget) == 0) {
    ObjectKey scratch;
    m_targetKey.Reset();
    m_targetKey = *ChooseTarget(&scratch);
    if (IsSystemKey(m_targetKey)) {
      m_flags.m_value |= kAgentHasTarget;
    }
  }
}

int AiAgentKind6::Evaluate()
{
  ObjectKey picked;
  ObjectKey scratch;
  m_flags.m_value &= ~kAgentHasCharacter;
  if (AiCharacterRecord* character = CurrentCharacter(this)) {
    if (character->m_agentId == static_cast<int>(m_id)) {
      const uint32_t flags = character->m_flags.m_value;
      if ((flags & kCharacterBusy) == 0 && (flags & 0x400) != 0) {
        m_flags.m_value |= kAgentHasCharacter;
      } else {
        GiveBackCharacter(character);
        m_characterKey.Reset();
        ReleaseHook();
      }
    } else {
      m_characterKey.Reset();
    }
  }
  if ((m_flags.m_value & kAgentHasCharacter) == 0) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_targetKey);
    if (!system || (system->m_flags30.m_value & 1) == 0) {
      m_targetKey.Reset();
    } else {
      m_selector.Add(m_characters->RankByValue(0x402, 0, 0x74000f1, 0, 6, 0, 10000, 1));
      picked = *m_selector.PickBest(&scratch);
      m_selector.Reset();
      if (IsCharacterKey(picked)) {
        AiCharacterRecord* character = FindCharacter(this, picked);
        if (character && CanReach(&picked) != 0) {
          TakeCharacter(this, character, picked, 0);
        }
      }
    }
  }
  return (m_flags.m_value & kAgentHasCharacter) != 0 ? 1 : 0;
}

ObjectKey* AiAgentKind6::ChooseTarget(ObjectKey* out)
{
  ObjectKey sector;
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectorsRandomly(0x40000000, 0, 0x40000000, 0, 0, 0, 1));
  sector = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0x80000000, 0, 0x40000000, 0, 0, 0, 1));
  sector = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  *out = sector;
  return out;
}

// ---- kind 1
// -----------------------------------------------------------------------------------------------------------

AiAgentKind1::AiAgentKind1(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiAgentSimple(view, owner, characters)
{
}

AiAgentKind1::~AiAgentKind1() = default;

int AiAgentKind1::GetCountSlot()
{
  return 0;
}

void AiAgentKind1::ReleaseHook()
{
  AiSystemEval* system = m_owner->FindSystemEval(&m_targetKey);
  if (system) {
    system->m_flags30.m_value &= 0xdfffffff;
  }
}

void AiAgentKind1::Prepare()
{
  m_flags.m_value &= ~kAgentHasTarget;
  if (IsSystemKey(m_targetKey)) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_targetKey);
    if (system) {
      if ((system->m_flags30.m_value & 0x10000000) == 0) {
        ReleaseHook();
      } else {
        m_flags.m_value |= kAgentHasTarget;
      }
    }
  }
  if ((m_flags.m_value & kAgentHasTarget) == 0) {
    ObjectKey scratch;
    m_targetKey.Reset();
    m_targetKey = *ChooseTarget(&scratch);
    if (IsSystemKey(m_targetKey)) {
      m_flags.m_value |= kAgentHasTarget;
    }
  }
}

int AiAgentKind1::Evaluate()
{
  ObjectKey picked;
  ObjectKey scratch;
  m_flags.m_value &= ~kAgentHasCharacter;
  if (AiCharacterRecord* character = CurrentCharacter(this)) {
    if (character->m_agentId == static_cast<int>(m_id)) {
      if ((character->m_flags.m_value & kCharacterBusy) == 0) {
        m_flags.m_value |= kAgentHasCharacter;
      } else {
        GiveBackCharacter(character);
        m_characterKey.Reset();
        ReleaseHook();
      }
    } else {
      m_characterKey.Reset();
    }
  }
  if ((m_flags.m_value & kAgentHasCharacter) == 0) {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_targetKey);
    if (!system || (system->m_flags30.m_value & 0x20000000) != 0) {
      m_targetKey.Reset();
    } else {
      m_selector.Add(m_characters->RankByValue(kCharacterAssignable, 0x4000000, 0x4f4000f1, 0, 6, 0x32, 10000, 1));
      m_selector.Add(m_characters->RankByDistance(&m_targetKey, kCharacterAssignable, 0x4000000, 0x4f4000f1, 0, 2));
      picked = *m_selector.PickBest(&scratch);
      while (IsCharacterKey(picked) && (m_flags.m_value & kAgentHasCharacter) == 0) {
        AiCharacterRecord* character = FindCharacter(this, picked);
        if (character && CanReach(&picked) != 0) {
          TakeCharacter(this, character, picked, 0);
          system->m_flags30.m_value |= 0x20000000;
        }
        picked = *m_selector.PickBest(&scratch);
      }
      m_selector.Reset();
    }
  }
  return (m_flags.m_value & kAgentHasCharacter) != 0 ? 1 : 0;
}

ObjectKey* AiAgentKind1::ChooseTarget(ObjectKey* out)
{
  ObjectKey chosen;
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectors(0x40000000, 0, 0x10000000, 0, 0, 0, 0x26, 1));
  chosen = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&chosen, 0x80000000, 0, 0x10000000, 0, 0, 0x20000000, 0x21, 1));
  chosen = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystemKey(chosen)) {
    m_selector.Add(m_owner->RankSectors(0, 0, 0x10000000, 0, 0, 0, 0x26, 1));
    chosen = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(m_owner->RankSystemsOfSector(&chosen, 0, 0, 0x10000000, 0, 0, 0x20000000, 0x21, 1));
    chosen = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  *out = chosen;
  return out;
}

// ---- the targeted kinds
// ------------------------------------------------------------------------------------------------

AiAgentKind5::AiAgentKind5(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiAgentTargeted(view, owner, characters)
{
  m_x50 = 0x2000000;
}

AiAgentKind5::~AiAgentKind5() = default;

int AiAgentKind5::GetOrderKind()
{
  return 0x261;
}

AiAgentKind3::AiAgentKind3(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiAgentTargeted(view, owner, characters)
{
  m_x50 = 0x1000000;
}

AiAgentKind3::~AiAgentKind3() = default;

int AiAgentKind3::GetOrderKind()
{
  return 0x262;
}

AiAgentKind4::AiAgentKind4(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiAgentTargeted(view, owner, characters)
{
  m_x50 = 0x4000000;
}

AiAgentKind4::~AiAgentKind4() = default;

int AiAgentKind4::GetOrderKind()
{
  return 0x263;
}

int AiAgentKind4::Evaluate()
{
  int result = 0;
  ObjectKey sectorKey;
  ObjectKey systemKey;
  ObjectKey picked;
  ObjectKey scratch;
  m_flags.m_value &= 0xffffffe2;
  bool decided = false;
  if (AiCharacterRecord* character = CurrentCharacter(this)) {
    if (character->m_agentId == static_cast<int>(m_id)) {
      if ((character->m_flags.m_value & kCharacterBusy) == 0) {
        m_flags.m_value |= kAgentHasCharacter;
        if (character->m_location.m_value == m_targetKey.m_value) {
          const uint32_t flags = m_flags.m_value;
          m_flags.m_value = flags | 8;
          if ((character->m_flags.m_value & 0x7000000) != 0) {
            m_flags.m_value = flags | 0xc;
          }
        }
        decided = true;
      } else {
        GiveBackCharacter(character);
      }
    }
    if (!decided) {
      m_characterKey.Reset();
    }
  }
  if ((m_flags.m_value & kAgentHasCharacter) == 0) {
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_systemRecordId));
    if (record) {
      sectorKey = record->m_sectorKey;
      m_selector.Add(m_characters->RankByValue(kCharacterAssignable, m_x50, 0x474000f1, 0, 6, 0x32, 10000, 1));
      if (IsSystemKey(m_homeKey)) {
        systemKey = m_homeKey;
      } else {
        auto* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&sectorKey));
        if (sector) {
          systemKey = *sector->GetSystemKey(&scratch, 0);
        }
      }
      m_selector.Add(m_characters->RankByDistance(&systemKey, kCharacterAssignable, m_x50, 0x474000f1, 0, 2));
      picked = *m_selector.PickBest(&scratch);
      while (IsCharacterKey(picked) && (m_flags.m_value & kAgentHasCharacter) == 0) {
        AiCharacterRecord* character = FindCharacter(this, picked);
        if (character && CanReach(&picked) != 0) {
          TakeCharacter(this, character, picked, m_assignment);
        }
        picked = *m_selector.PickBest(&scratch);
      }
      m_selector.Reset();
    }
    if ((m_flags.m_value & kAgentHasCharacter) == 0) {
      return 0;
    }
  }
  result = 1;
  return result;
}

const char* (*g_gameHook2AE8)() = nullptr;
int (*g_gameHook2AD4)(HookArg) = nullptr;
int (*g_gameHook2AEC)() = nullptr;
int (*g_gameHook2AF0)() = nullptr;
int (*g_gameHook2AF4)() = nullptr;
int (*g_gameHook2AFC)() = nullptr;
int (*g_gameHook2B04)() = nullptr;
int (*g_gameHook2B08)() = nullptr;
void (*g_gameHook2AD8)(HookArg, int) = nullptr;
void (*g_gameHook2AE0)(HookArg) = nullptr;
void (*g_gameHook2AF8)(HookArg) = nullptr;
void (*g_gameHook2B00)(HookArg) = nullptr;
