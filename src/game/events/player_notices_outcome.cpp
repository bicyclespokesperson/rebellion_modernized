#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/notif_event_payloads.h"
#include "events/player_notice_util.h"
#include "events/player_notices.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "story/story_characters.h"
#include "strategy_ui/advisor_notices.h"
#include "ui/message_template.h"

namespace
{

constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kStoryFlagSpecial = 0x8;

uint32_t stateFlags(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object)->m_stateFlags;
}
bool isCaptured(GameObjNotifier* character)
{
  return (static_cast<Character*>(character)->m_flagsAc & kCapturedFlag) != 0;
}
uint16_t sideVariant(int owner, uint16_t base)
{
  return static_cast<uint16_t>((owner != 1) + base);
}

void say(std::string* text,
         uint32_t id,
         GameObjNotifier* a = nullptr,
         GameObjNotifier* b = nullptr,
         GameObjNotifier* c = nullptr)
{
  ExpandMessageTemplate(text, id, notice::nameOf(a), notice::nameOf(b), notice::nameOf(c), nullptr);
}

// The first living member of the team that sits in a fleet, or else the system of the first one that sits in one.
GameObjNotifier* findTeamLocation(int owner, ObjectKeyValueSet& members)
{
  GameObjNotifier* found = nullptr;
  bool keepGoing = true;
  for (auto position = members.Entries().begin(); position != members.Entries().end() && (keepGoing); ++position)
  {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ObjectKey nodeKey(node->m_key);
    found = GetObjectInFamilies_01_ff(owner, &nodeKey);
    if (!found)
      continue;
    if ((stateFlags(found) & kStateDestroyed) != 0)
      found = nullptr;
    else if (notice::inFamilies(nodeKey, 0x30, 0x3c) && isCaptured(found))
      found = nullptr;
    while (found && keepGoing)
    {
      ObjectKey key;
      found->GetKey(&key);
      if (notice::inFamilies(key, 0x90, 0x98))
      {
        keepGoing = false;
        continue;
      }
      if (notice::inFamilies(key, 8, 0x10))
      {
        keepGoing = false;
        break;
      }
      found = found->m_parent;
    }
  }
  return found;
}

} // namespace

// Event 0x1f1: how a mission of the side's character ended.
void Notice23::OnPayload(int kind, intptr_t payload)
{
  if (kind != 0x1f1)
    return;
  const auto* outcome = reinterpret_cast<const IntPairPayload*>(payload);
  ObjectKey missionKey(outcome->m_key);
  GameObjNotifier* missionObject = GetObjectInFamilies_40_80(m_owner, &missionKey);
  GameObjNotifier* leader = nullptr;
  GameObjNotifier* target = nullptr;
  ObjectKey leaderKey;
  bool ownSide = true;
  m_unk40 = 0;
  if (missionObject)
  {
    auto* mission = static_cast<Mission*>(missionObject);
    leaderKey = ObjectKey(mission->m_leaderKey);
    leader = GetObjectInFamilies_01_ff(m_owner, &leaderKey);
    const uint32_t controller = mission->ControlSide();
    if (controller == 1)
      ownSide = m_owner == 1;
    else if (controller == 2)
      ownSide = m_owner == 2;
    m_key = ObjectKey(mission->m_targetLocationKey);
    target = GetObjectInFamilies_90_98(m_owner, &m_key);
  }
  if (leader && !notice::inFamilies(m_key, 0x90, 0x98))
  {
    leader = nullptr;
    m_unk40 = 0;
  }
  if (!leader || !ownSide)
  {
    if (!ownSide && missionObject)
    {
      m_unk40 = 1;
      say(&m_text0, 0x713e, missionObject);
      say(&m_text1, 0x713f, missionObject, target);
      m_picture[1] = sideVariant(m_owner, 0x431);
      m_sound[0] = sideVariant(m_owner, 0x47c);
    }
    return;
  }

  m_unk40 = 1;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_key);
  if ((stateFlags(leader) & kStateDestroyed) == 0)
  {
    const bool character = notice::inFamilies(leaderKey, 0x30, 0x3c);
    if (!character || !isCaptured(leader))
      m_key = ObjectKey(leaderKey);
  }
  GameObjNotifier* location = findTeamLocation(m_owner, static_cast<Mission*>(missionObject)->m_finishedMembers);

  if (notice::inFamilies(leaderKey, 0x30, 0x38))
  {
    switch (leaderKey.Id())
    {
      case 0x240:
        m_unk28 = 0x17;
        m_sound[1] = AdvisorPickAdviceLine(0xf3);
        break;
      case 0x241:
        m_unk28 = 0x18;
        m_sound[1] = AdvisorPickAdviceLine(0xdf);
        break;
      case 0x242:
        m_unk28 = 0x16;
        m_sound[1] = AdvisorPickAdviceLine(
          (static_cast<StoryCharacter*>(leader)->m_storyFlags & kStoryFlagSpecial) == 0 ? 0xcb : 0xd5);
        break;
      case 0x243:
        m_unk28 = 0x19;
        m_sound[1] = AdvisorPickAdviceLine(0xe9);
        break;
      case 0x280:
        m_unk28 = 0x1a;
        m_sound[1] = AdvisorPickAdviceLine(0x107);
        break;
      case 0x281:
        m_unk28 = 0x1b;
        m_sound[1] = AdvisorPickAdviceLine(0xfd);
        break;
      default:
        break;
    }
  }
  else if (notice::inFamilies(leaderKey, 0x38, 0x3c))
  {
    m_unk28 = 0x15;
  }
  m_picture[1] = sideVariant(m_owner, 0x412);
  m_sound[0] = sideVariant(m_owner, 0x47c);
  int hasSecondPicture = 0;
  if (notice::inFamilies(leaderKey, 0x30, 0x3c))
  {
    m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(leader));
  }
  else
  {
    m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(leader));
    hasSecondPicture = missionKey.Family() == 0x54;
  }

  const bool story = notice::inFamilies(leaderKey, 0x30, 0x38);
  switch (outcome->m_valueB)
  {
    case 3:
    case 4:
    case 5:
      m_sound[0] = 0x480;
      if (hasSecondPicture == 0)
        m_picture[1] = sideVariant(m_owner, 0x416);
      if (story)
      {
        say(&m_text0, 29000, leader, missionObject);
        say(&m_text1, 0x7149, leader, missionObject, system);
      }
      else
      {
        say(&m_text0, 0x714a, leader, missionObject);
        say(&m_text1, 0x714b, leader, missionObject, system);
      }
      break;
    case 6:
    case 7:
    case 9:
    case 10:
      if (missionKey.Family() == 0x53)
      {
        m_unk40 = 0;
      }
      else if (story)
      {
        say(&m_text0, 0x7138, leader, missionObject);
        say(&m_text1, 0x7139, leader, missionObject, system);
      }
      else
      {
        say(&m_text0, 0x713a, leader, missionObject);
        say(&m_text1, 0x713b, leader, missionObject, system);
      }
      break;
    case 8:
    {
      GameObjNotifier* side = GetSideObject(m_owner, system->ControlSide() == 1 ? 1 : 2);
      if (story)
      {
        say(&m_text0, 0x7140, leader, missionObject);
        if (system->ControlSide() == 3)
          say(&m_text1, 0x7144, leader, missionObject, system);
        else
          ExpandMessageTemplate(&m_text1, 0x7141, notice::nameOf(leader), notice::nameOf(missionObject),
                                notice::nameOf(system), notice::nameOf(side));
      }
      else
      {
        say(&m_text0, 0x7142, leader, missionObject);
        if (system->ControlSide() == 3)
          say(&m_text1, 0x7145, leader, missionObject, system);
        else
          ExpandMessageTemplate(&m_text1, 0x7143, notice::nameOf(leader), notice::nameOf(missionObject),
                                notice::nameOf(system), notice::nameOf(side));
      }
      break;
    }
    case 12:
      if (missionKey.Family() == 0x53)
      {
        if (story)
        {
          say(&m_text0, 0x7138, leader, missionObject);
          say(&m_text1, 0x713c, leader, missionObject, system);
        }
        else
        {
          say(&m_text0, 0x713a, leader, missionObject);
          say(&m_text1, 0x713d, leader, missionObject, system);
        }
      }
      else
      {
        m_unk40 = 0;
      }
      break;
    default:
      m_unk40 = 0;
      break;
  }
  say(&m_text1, location ? 0x70e6 : 0x70e7, location);
}
