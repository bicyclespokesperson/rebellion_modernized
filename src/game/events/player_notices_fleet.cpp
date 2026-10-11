#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/player_notices.h"
#include "fleet/capship.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/object_lookup.h"
#include "ui/message_template.h"

namespace
{

constexpr uint32_t kEventFleetArrived = 0x182;
constexpr uint32_t kEventFleetLeft = 0x183;
constexpr uint32_t kEventSystemTaken = 0x14f;
constexpr uint32_t kEventSystemLost = 0x150;

ObjectKeyNode* keyNodeOf(GameObjNotifier* object)
{
  ObjectKey key;
  object->GetKey(&key);
  return NewOrNull<ObjectKeyNode>(key);
}

bool isCharacter(GameObjNotifier* object)
{
  ObjectKey key;
  object->GetKey(&key);
  return notice::inFamilies(key, 0x30, 0x3c);
}

bool isCaptured(GameObjNotifier* character)
{
  return (static_cast<Character*>(character)->m_flagsAc & 1) != 0;
}

void say(std::string* text,
         uint32_t id,
         GameObjNotifier* a = nullptr,
         GameObjNotifier* b = nullptr,
         GameObjNotifier* c = nullptr)
{
  ExpandMessageTemplate(text, id, notice::nameOf(a), notice::nameOf(b), notice::nameOf(c), nullptr);
}

} // namespace

// Notices of kind 15 file the objects of a fleet or system into sets (the ones that left, and the captured characters
// apart) and then compose the text out of the two sides' names.
void Notice15::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind == 2)
  {
    GameObjNotifier* controller = GetSideObject(m_owner, m_f6c == 1 ? 1 : 2);
    GameObjNotifier* other = nullptr;
    if (m_f68 == 1)
      other = GetSideObject(m_owner, 1);
    else if (m_f68 == 2)
      other = GetSideObject(m_owner, 2);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_key);
    if (m_f64 == 0)
    {
      if (m_owner != m_f6c)
        m_unk28 = 0x2e;
      say(&m_text0, 0x7098, system);
      if (!other)
        say(&m_text1, 0x709a, controller, system);
      else
        say(&m_text1, 0x7099, controller, other, system);
      if (m_set80.First())
        m_picture[1] = 0x2b9b;
      else if (m_set88.First())
        m_picture[1] = 0x2b9c;
      else
        m_picture[1] = 0x2b9a;
    }
    else
    {
      if (m_owner != m_f6c)
        m_unk28 = 0x2f;
      say(&m_text0, 0x7090, system);
      const uint32_t systemController = system->ControlSide();
      const bool mismatch = systemController != static_cast<uint32_t>(m_f6c);
      m_picture[1] = m_f6c == 1 ? 0x2b98 : 0x2b99;
      if (!other)
      {
        say(&m_text1, mismatch ? 0x7094 : 0x7093, controller, system);
      }
      else
      {
        say(&m_text1, mismatch ? 0x7092 : 0x7091, controller, other, system);
      }
    }
    return;
  }
  if (kind != kEventFleetArrived && kind != kEventFleetLeft)
    return;
  GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_owner, reinterpret_cast<const ObjectKey*>(a));
  if (!fleet)
    return;
  if (kind == kEventFleetArrived)
    m_f6c = fleet->ControlSide();
  if (b == 0)
  {
    CapShipIterator capitals(fleet, 1);
    capitals.Begin();
    capitals.Begin();
    while (capitals.Current())
    {
      m_set78.Add(keyNodeOf(capitals.Current()));
      GameObjIterator crew(capitals.Current(), 1);
      for (crew.Begin(); crew.Current(); crew.Advance())
      {
        ObjectKeyNode* node = keyNodeOf(crew.Current());
        const bool apart = isCharacter(crew.Current()) && isCaptured(crew.Current());
        (apart ? m_set90 : m_set78).Add(node);
      }
      capitals.Advance();
    }
  }
  if (kind == kEventFleetArrived)
    m_unk40 = 1;
  else
    m_f6c = fleet->ControlSide();
}

void Notice15::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c)
{
  if (kind != kEventSystemTaken && kind != kEventSystemLost)
    return;
  const bool lost = kind == kEventSystemLost;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(a));
  if (lost)
    m_f64 = 1;
  if (!system)
    return;
  ObjectKey systemKey;
  system->GetKey(&systemKey);
  m_key = ObjectKey(systemKey);
  if (b == 0)
  {
    m_unk40 = 1;
    GameObjIterator children(system, 1);
    for (children.Begin(); children.Current(); children.Advance())
    {
      GameObjNotifier* child = children.Current();
      ObjectKeyNode* node = keyNodeOf(child);
      ObjectKeySet* target = &m_set70;
      ObjectKey childKey;
      child->GetKey(&childKey);
      if (!lost)
      {
        if (isCharacter(child) && isCaptured(child))
          target = &m_set98;
      }
      else if (notice::inFamilies(childKey, 0x10, 0x14))
      {
        target = child->ControlSide() == static_cast<uint32_t>(m_f68) ? &m_set70 : &m_set78;
      }
      else if (notice::inFamilies(childKey, 0x30, 0x3c) && isCaptured(child))
      {
        target = static_cast<Character*>(child)->GetRecordSide() == m_f68 ? &m_set90 : &m_set98;
      }
      target->Add(node);
    }
    return;
  }
  m_f68 = static_cast<int>(c);
}
