#include "ai/ai_agent_bases.h"

#include "ai/ai_character_records.h"
#include "ai/ai_common.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "core/checked_new.h"
#include "gameobj/game_obj.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace
{

constexpr int kMoveOrder = 0x201;
constexpr int kReleaseOrder = 0x260;

constexpr uint32_t kCharacterFirst = 0x30;
constexpr uint32_t kCharacterLast = 0x40;
constexpr uint32_t kSpecialForceLast = 0x3c;

// AiCharacterRecord::m_flags
constexpr uint32_t kCharacterBusy = 0xa0;
constexpr uint32_t kCharacterReleased = 0x800000;
constexpr uint32_t kCharacterOnMission = 0x7000000;
constexpr uint32_t kCharacterUnavailable = 0x74000f1;

// AiAgentBase::m_flags
constexpr uint32_t kAgentHasCharacter = 1;
constexpr uint32_t kAgentCharacterAtTarget = 8;
constexpr uint32_t kAgentCharacterReady = 4;
constexpr uint32_t kAgentCharacterHome = 0x10;
constexpr uint32_t kAgentHasTarget = 2;

bool IsCharacterKey(const ObjectKey& key)
{
  return KeyInFamilies(key, kCharacterFirst, kCharacterLast);
}
bool IsSystemKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x90, 0x98);
}
bool IsShipKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x14, 0x1c);
}

AiCharacterRecord* FindCharacter(AiCharacterRecordSet* characters, const ObjectKey* key)
{
  return static_cast<AiCharacterRecord*>(characters->Find(key));
}

int AvailableCharacter(AiAgentBase* agent)
{
  int available = 1;
  if (IsCharacterKey(agent->m_characterKey))
  {
    AiCharacterRecord* character = FindCharacter(agent->m_characters, &agent->m_characterKey);
    if (character && (character->m_flags.m_value & kCharacterBusy) != 0)
      available = 0;
  }
  return available;
}

// Gives the character back (what both agent kinds do when they release it); the message that tells it to stop, or null.
OrderBase* ReleaseAssignedCharacter(AiAgentBase* agent)
{
  OrderBase* order = nullptr;
  agent->m_owner->RetireAgent(agent->m_id);
  if (IsCharacterKey(agent->m_characterKey))
  {
    AiCharacterRecord* character = FindCharacter(agent->m_characters, &agent->m_characterKey);
    if (character && character->m_agentId == static_cast<int>(agent->m_id))
    {
      character->m_flags.m_value |= kCharacterReleased;
      character->m_eligibilityRank = 0;
      if ((character->m_flags.m_value & kCharacterOnMission) != 0 && (character->m_flags.m_value & kCharacterBusy) == 0)
        order = agent->CreateCharacterMessage(kReleaseOrder);
    }
  }
  return order;
}

} // namespace

AiAgentBase::AiAgentBase(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: IdNode(),
  m_flags(0),
  m_targetKey(),
  m_characterKey(),
  m_selector()
{
  m_owner = owner;
  m_view = view;
  m_state = 0;
  m_characters = characters;
}

AiAgentBase::~AiAgentBase()
{
  m_characters = nullptr;
  m_owner = nullptr;
}

void AiAgentBase::Read(istream* in)
{
  ReadId(in);
  ReadValue32(in, &m_state);
  ReadValuePersistentInt(in, &m_flags);
  ReadValue32(in, &m_view);
  m_characterKey.Read(in);
  m_targetKey.Read(in);
  m_selector.Read(in);
}

void AiAgentBase::Write(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_state);
  WriteValuePersistentInt(out, &m_flags);
  WriteValue32(out, &m_view);
  m_characterKey.Write(out);
  m_targetKey.Write(out);
  m_selector.Write(out);
}

OrderBase* AiAgentBase::CreateCharacterMessage(int kind)
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  if (KeyInFamilies(m_characterKey, kCharacterFirst, kSpecialForceLast))
  {
    order = static_cast<OrderBase*>(CreateMessageByKind(kind));
    if (order)
    {
      auto* node = NewOrNull<ObjectKeyValueNode>(m_characterKey, 0);
      if (node)
      {
        team.Add(node);
        order->m_side = m_view;
        order->SetTeam(&team);
      }
    }
  }
  return order;
}

OrderBase* AiAgentBase::CreateMoveMessage(const ObjectKey* target)
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  const bool sendable =
    KeyInFamilies(m_characterKey, kCharacterFirst, kSpecialForceLast) && KeyInFamilies(*target, 1, 0xff);
  if (sendable)
  {
    order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
    if (order)
    {
      auto* node = NewOrNull<ObjectKeyValueNode>(m_characterKey, 0);
      if (node)
      {
        team.Add(node);
        order->m_side = m_view;
        order->SetTeam(&team);
        order->SetTarget(target);
      }
    }
  }
  return order;
}

int AiAgentBase::CanReach(const ObjectKey* characterKey)
{
  int reachable = 0;
  ObjectKey location;
  AiCharacterRecord* character = FindCharacter(&m_owner->m_characters, characterKey);
  if (character)
  {
    location = character->m_location;
    reachable = 1;
    if (IsSystemKey(location) && (character->m_flags.m_value & 0x20) == 0)
    {
      AiSystemEval* system = m_owner->m_systems.FindEval(location);
      if (system && (system->m_flags28.m_value & 2) != 0)
        reachable = 0;
    }
  }
  return reachable;
}

AiAgentSimple::AiAgentSimple(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiAgentBase(view, owner, characters)
{
  m_x48 = 0;
}

AiAgentSimple::~AiAgentSimple() = default;

void AiAgentSimple::Read(istream* in)
{
  AiAgentBase::Read(in);
  ReadValue32(in, &m_x48);
}

void AiAgentSimple::Write(ostream* out)
{
  AiAgentBase::Write(out);
  WriteValue32(out, &m_x48);
}

int AiAgentSimple::GetAgentClass()
{
  return 2;
}

int AiAgentSimple::IsAvailable()
{
  return AvailableCharacter(this);
}

void AiAgentSimple::SetTier(int)
{
}

int AiAgentSimple::GetTier()
{
  return 0;
}

OrderBase* AiAgentSimple::Step(int* done)
{
  OrderBase* order = nullptr;
  *done = 0;
  switch (m_state)
  {
    case 1:
      if ((static_cast<uint32_t>(m_flags.m_value) & 0x80000000) == 0)
      {
        m_state = 3;
      }
      else
      {
        order = ReleaseCharacter();
        *done = 1;
      }
      break;
    case 2:
      if (Evaluate() != 0)
      {
        m_state = 4;
        break;
      }
      *done = 1;
      break;
    case 3:
      Prepare();
      m_state = 2;
      break;
    case 4:
      order = Execute();
      *done = 1;
      break;
    default:
      m_state = 1;
  }
  if (*done != 0)
    m_state = 0;
  return order;
}

OrderBase* AiAgentSimple::ReleaseCharacter()
{
  OrderBase* order = ReleaseAssignedCharacter(this);
  ReleaseHook();
  m_targetKey.Reset();
  return order;
}

OrderBase* AiAgentSimple::Execute()
{
  OrderBase* order = nullptr;
  if (!IsCharacterKey(m_characterKey))
    return order;
  AiCharacterRecord* character = FindCharacter(m_characters, &m_characterKey);
  if (!character || (character->m_flags.m_value & kCharacterUnavailable) != 0)
    return order;
  if (character->m_location.m_value == m_targetKey.m_value)
  {
    if ((character->m_flags.m_value & kCharacterOnMission) == 0 && GetKind() != 6)
      order = CreateCharacterMessage(GetOrderKind());
  }
  else if (IsSystemKey(m_targetKey))
  {
    order = CreateMoveMessage(&m_targetKey);
  }
  return order;
}

AiAgentTargeted::AiAgentTargeted(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiAgentBase(view, owner, characters),
  m_homeKey(),
  m_sectorKey()
{
  m_systemRecordId = 0;
  m_assignment = 0;
  m_x50 = 0;
}

AiAgentTargeted::~AiAgentTargeted() = default;

void AiAgentTargeted::Read(istream* in)
{
  AiAgentBase::Read(in);
  ReadValue32(in, &m_systemRecordId);
  ReadValue32(in, &m_assignment);
  ReadValue32(in, &m_x50);
  m_sectorKey.Read(in);
  m_homeKey.Read(in);
}

void AiAgentTargeted::Write(ostream* out)
{
  AiAgentBase::Write(out);
  WriteValue32(out, &m_systemRecordId);
  WriteValue32(out, &m_assignment);
  WriteValue32(out, &m_x50);
  m_sectorKey.Write(out);
  m_homeKey.Write(out);
}

int AiAgentTargeted::GetAgentClass()
{
  return 1;
}

int AiAgentTargeted::IsAvailable()
{
  return AvailableCharacter(this);
}

void AiAgentTargeted::SetTier(int tier)
{
  m_assignment = tier;
  if (!IsCharacterKey(m_characterKey))
    return;
  AiCharacterRecord* character = FindCharacter(m_characters, &m_characterKey);
  if (character && character->m_agentId == static_cast<int>(m_id))
    character->m_eligibilityRank = tier;
}

int AiAgentTargeted::GetTier()
{
  return m_assignment;
}

OrderBase* AiAgentTargeted::Step(int* done)
{
  OrderBase* order = nullptr;
  *done = 0;
  switch (m_state)
  {
    case 1:
      if ((static_cast<uint32_t>(m_flags.m_value) & 0x80000000) == 0)
      {
        m_state = 3;
      }
      else
      {
        order = ReleaseCharacter();
        *done = 1;
      }
      break;
    case 2:
      if (Evaluate() != 0)
      {
        m_state = 4;
        break;
      }
      *done = 1;
      break;
    case 3:
      FindTarget();
      m_state = 2;
      break;
    case 4:
      order = Execute();
      *done = 1;
      break;
    default:
      m_state = 1;
  }
  if (*done != 0)
    m_state = 0;
  return order;
}

OrderBase* AiAgentTargeted::ReleaseCharacter()
{
  OrderBase* order = ReleaseAssignedCharacter(this);
  m_targetKey.Reset();
  return order;
}

void AiAgentTargeted::FindTarget()
{
  m_flags.m_value &= ~kAgentHasTarget;
  auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_systemRecordId));
  if (!record)
    return;
  m_homeKey = record->m_reserveSystem;
  m_targetKey = record->m_reserveShip;
  if (IsShipKey(m_targetKey) && IsSystemKey(m_homeKey))
    m_flags.m_value |= kAgentHasTarget;
}

int AiAgentTargeted::Evaluate()
{
  int ready = 0;
  ObjectKey sectorKey;
  ObjectKey systemKey;
  ObjectKey picked;
  ObjectKey scratch;
  m_flags.m_value &= 0xffffffe2;
  if (IsCharacterKey(m_characterKey))
  {
    AiCharacterRecord* character = FindCharacter(m_characters, &m_characterKey);
    if (character)
    {
      bool settled = false;
      if (character->m_agentId == static_cast<int>(m_id))
      {
        if ((character->m_flags.m_value & 0x50) == 0)
        {
          m_flags.m_value |= kAgentHasCharacter;
          if (character->m_location.m_value == m_targetKey.m_value)
          {
            const uint32_t flags = m_flags.m_value;
            m_flags.m_value = flags | kAgentCharacterAtTarget;
            if ((character->m_flags.m_value & kCharacterOnMission) != 0)
              m_flags.m_value = flags | (kAgentCharacterAtTarget | kAgentCharacterReady);
          }
          settled = true;
        }
        else
        {
          character->m_flags.m_value |= kCharacterReleased;
          character->m_x24 = 0;
          character->m_eligibilityRank = 0;
          character->m_agentId = 0;
        }
      }
      if (!settled)
        m_characterKey.Reset();
    }
  }
  if ((m_flags.m_value & kAgentHasCharacter) == 0)
  {
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_systemRecordId));
    if (record)
    {
      sectorKey = record->m_sectorKey;
      m_selector.Add(m_characters->RankByValue(0x800002, m_x50, 0x4f4000f1, 0, 6, 0x32, 10000, 1));
      if (IsSystemKey(m_homeKey))
      {
        systemKey = m_homeKey;
      }
      else
      {
        auto* sector = static_cast<AiSectorEval*>(m_owner->m_sectors.Find(&sectorKey));
        if (sector)
          systemKey = *sector->GetSystemKey(&scratch, 0);
      }
      m_selector.Add(m_characters->RankByDistance(&systemKey, 0x800002, m_x50, 0x4f4000f1, 0, 2));
      picked = *m_selector.PickBest(&scratch);
      while (IsCharacterKey(picked) && (m_flags.m_value & kAgentHasCharacter) == 0)
      {
        AiCharacterRecord* candidate = FindCharacter(m_characters, &picked);
        if (candidate && CanReach(&picked) != 0)
        {
          m_characterKey = picked;
          candidate->m_flags.m_value &= ~kCharacterReleased;
          candidate->m_x24 = 0;
          candidate->m_agentId = m_id;
          candidate->m_eligibilityRank = m_assignment;
          candidate->MarkChanged();
          m_flags.m_value |= kAgentHasCharacter;
        }
        picked = *m_selector.PickBest(&scratch);
      }
      m_selector.Reset();
    }
    if ((m_flags.m_value & kAgentHasCharacter) == 0)
      return ready;
  }
  ready = 1;
  return ready;
}

OrderBase* AiAgentTargeted::Execute()
{
  OrderBase* order = nullptr;
  if (!IsCharacterKey(m_characterKey))
    return order;
  AiCharacterRecord* character = FindCharacter(m_characters, &m_characterKey);
  if (!character || (character->m_flags.m_value & kCharacterUnavailable) != 0)
    return order;
  if (character->m_location.m_value == m_targetKey.m_value)
  {
    if (!IsShipKey(m_targetKey))
      return order;
    if ((character->m_flags.m_value & kCharacterOnMission) != 0)
      return order;
    m_flags.m_value |= kAgentCharacterReady;
    return CreateCharacterMessage(GetOrderKind());
  }
  if (!IsShipKey(m_targetKey))
  {
    if (!IsSystemKey(m_homeKey))
      return order;
    if (character->m_location.m_value != m_homeKey.m_value)
      return CreateMoveMessage(&m_homeKey);
    m_flags.m_value |= kAgentCharacterHome;
    return order;
  }
  const ObjectKey characterLocation = character->m_location;
  AiShipRecord* targetShip = nullptr;
  uint32_t targetShipFlags = 0;
  if (IsShipKey(characterLocation))
  {
    auto* locationShip = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&characterLocation));
    targetShip = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_targetKey));
    if (!locationShip || !targetShip || (locationShip->m_flags.m_value & 0x800) != 0)
      return order;
    const ObjectKey locationFleetKey = locationShip->m_fleetKey;
    const ObjectKey targetFleetKey = targetShip->m_fleetKey;
    if (locationFleetKey.m_value == targetFleetKey.m_value)
    {
      if ((targetShip->m_flags.m_value & 0x800) == 0)
      {
        m_flags.m_value |= kAgentCharacterAtTarget;
        order = CreateMoveMessage(&m_targetKey);
      }
      return order;
    }
    AiFleetEval* locationFleet = m_owner->m_fleets.FindEval(locationFleetKey);
    AiFleetEval* targetFleet = m_owner->m_fleets.FindEval(targetFleetKey);
    if (!locationFleet || !targetFleet)
      return order;
    if (locationFleet->m_systemKey.m_value != targetFleet->m_systemKey.m_value)
      return CreateMoveMessage(&m_homeKey);
    targetShipFlags = targetShip->m_flags.m_value;
  }
  else
  {
    targetShip = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_targetKey));
    if (!targetShip)
      return order;
    const ObjectKey targetFleetKey = targetShip->m_fleetKey;
    AiFleetEval* targetFleet = m_owner->m_fleets.FindEval(targetFleetKey);
    if (!targetFleet)
      return order;
    if (characterLocation.m_value != targetFleet->m_systemKey.m_value)
    {
      if (characterLocation.m_value == m_homeKey.m_value)
        m_flags.m_value |= kAgentCharacterHome;
      else
        order = CreateMoveMessage(&m_homeKey);
      return order;
    }
    targetShipFlags = targetShip->m_flags.m_value;
  }
  if ((targetShipFlags & 0x800) == 0)
  {
    m_flags.m_value |= kAgentCharacterAtTarget;
    order = CreateMoveMessage(&m_targetKey);
  }
  return order;
}
