#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/player_notices.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "ui/message_template.h"

namespace
{

constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateArrived = 0x20;

int otherSide(int owner)
{
  return 2 - (owner != 1);
}

uint32_t stateFlags(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object)->m_stateFlags;
}

void say(std::string* text,
         uint32_t id,
         GameObjNotifier* a = nullptr,
         GameObjNotifier* b = nullptr,
         GameObjNotifier* c = nullptr)
{
  ExpandMessageTemplate(text, id, notice::nameOf(a), notice::nameOf(b), notice::nameOf(c), nullptr);
}

bool inFleetFamilies(const ObjectKey& key)
{
  return notice::inFamilies(key, 8, 0x10);
}

} // namespace

// ---- Notice1: the report of a battle or blockade of a system ----------------------------------------------------

void Notice1::FillNeutral()
{
  GameObjNotifier* mine = GetSideObject(m_owner, m_owner);
  GameObjNotifier* other = GetSideObject(m_owner, otherSide(m_owner));
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_systemKey);
  if (!mine || !system || !other)
    return;
  say(&m_text88, 0x7001, system);
  say(&m_text94, 0x7004, system);
  say(&m_texta0, 0x700e, mine, other);
  uint32_t tail;
  if (m_f54 == 0)
  {
    say(&m_texta0, 0x700f, mine, other);
  }
  else
  {
    if (m_f54 == m_f58)
    {
      GameObjNotifier* third = GetObjectInFamilies_90_98(m_owner, &m_keyB);
      if (!third)
        return;
      say(&m_texta0, 0x700d, mine, third);
      say(&m_texta0, 0x700b, other);
      say(&m_textac, 0x7011, mine, system, third);
      say(&m_textb8, 0x7013);
      return;
    }
    say(&m_texta0, 0x700b, mine);
    say(&m_texta0, 0x700c, other);
  }
  say(&m_textac, 0x7010, mine, system);
  tail = 0x7014;
  say(&m_textb8, tail);
}

void Notice1::FillOwnSide()
{
  GameObjNotifier* mine = GetSideObject(m_owner, m_owner);
  GameObjNotifier* other = GetSideObject(m_owner, otherSide(m_owner));
  if (mine)
    say(&m_text94, 0x7002, mine);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_systemKey);
  if (!mine || !system || !other)
    return;
  say(&m_text88, 0x7001, system);
  say(&m_textac, 0x7012, mine, system);
  say(&m_textb8, 0x7013);
  const uint32_t controller = system->ControlSide();
  GameObjNotifier* second = mine;
  uint32_t id;
  if (controller == 0 || controller == 3)
  {
    second = other;
    id = 0x7007;
  }
  else if (static_cast<uint32_t>(m_f5c) == controller)
  {
    id = static_cast<uint32_t>(m_f58) == controller ? 0x7016 : 0x7015;
  }
  else if (controller == static_cast<uint32_t>(m_f58))
  {
    second = other;
    id = 0x7005;
  }
  else
  {
    id = 0x7006;
  }
  say(&m_texta0, id, system, second);
  say(&m_texta0, m_f54 == 0 ? 0x700b : 0x700c, other);
}

void Notice1::FillOtherSide()
{
  GameObjNotifier* mine = GetSideObject(m_owner, m_owner);
  GameObjNotifier* other = GetSideObject(m_owner, otherSide(m_owner));
  if (mine)
    say(&m_text94, 0x7003, mine);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_systemKey);
  if (!mine || !system || !other)
    return;
  say(&m_text88, 0x7001, system);
  const uint32_t controller = system->ControlSide();
  GameObjNotifier* second = other;
  uint32_t id;
  if (controller == 0 || controller == 3)
  {
    id = 0x700a;
  }
  else if (static_cast<uint32_t>(m_f5c) == controller)
  {
    id = static_cast<uint32_t>(m_f58) == controller ? 0x7015 : 0x7016;
  }
  else if (controller == static_cast<uint32_t>(m_f58))
  {
    id = 0x7008;
  }
  else
  {
    second = mine;
    id = 0x7009;
  }
  say(&m_texta0, id, system, second);
  uint32_t tail;
  if (m_f54 == 0)
  {
    say(&m_texta0, 0x700b, mine);
    say(&m_textac, 0x7010, mine, system);
    tail = 0x7014;
  }
  else
  {
    GameObjNotifier* third = GetObjectInFamilies_90_98(m_owner, &m_keyB);
    if (!third)
      return;
    say(&m_texta0, 0x700d, mine, third);
    say(&m_textac, 0x7011, mine, system, third);
    tail = 0x7013;
  }
  say(&m_textb8, tail);
}

namespace
{

void rememberParentAsKeyB(ObjectKey* keyB, GameObjNotifier* object, int owner, int ownerOfList)
{
  if (keyB->IsSet() == 0 && owner == ownerOfList)
  {
    ObjectKey parent;
    object->m_parent->GetKey(&parent);
    *keyB = ObjectKey(parent);
  }
}

void rememberOnce(ObjectKeySet* set, const ObjectKey& key)
{
  if (set->Find(&key) == nullptr)
    set->Add(NewOrNull<ObjectKeyNode>(key));
}

} // namespace

// Sorts the objects of the arrival notice `vote` into this notice's sets, side by side (the first list is side 1's).
void Notice1::ReadVote(Notice2* vote)
{
  m_f5c = vote->m_f4c;
  ObjectKeyNode* firstList = vote->m_set6c.First();
  ObjectKeyNode* secondList = vote->m_set74.First();
  struct List
  {
    ObjectKeyNode* first;
    int side;
    ObjectKeySet* destroyed;
  };
  const List lists[] = {{firstList, 1, &m_set78}, {secondList, 2, &m_set80}};
  for (const List& list : lists)
  {
    for (ObjectKeyNode* node = list.first; node; node = static_cast<ObjectKeyNode*>(node->m_successor))
    {
      ObjectKey key(node->m_key);
      GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, &key);
      if (!object)
        continue;
      const uint32_t flags = stateFlags(object);
      if ((flags & kStateDestroyed) != 0)
      {
        if (list.destroyed->Find(&key) == nullptr)
          list.destroyed->Add(NewOrNull<ObjectKeyNode>(*node));
      }
      else if ((flags & kStateArrived) != 0)
      {
        if (inFleetFamilies(key))
        {
          rememberParentAsKeyB(&m_keyB, object, m_owner, list.side);
          rememberOnce(&m_set60, key);
        }
        else if (notice::inFamilies(key, 0x1c, 0x20))
        {
          rememberParentAsKeyB(&m_keyB, object, m_owner, list.side);
          rememberOnce(&m_set68, key);
        }
        else if (notice::inFamilies(key, 0x30, 0x3c))
        {
          rememberOnce(&m_set70, key);
        }
      }
      if (inFleetFamilies(key) && (flags & kStateDestroyed) == 0 && m_owner == list.side && m_keyC.IsSet() == 0)
        m_keyC = ObjectKey(key);
    }
  }
}

// ---- Notice2: the report of a fleet arriving at a system --------------------------------------------------------

void Notice2::AddObjectKeys(GameObjNotifier* object)
{
  if (!object)
    return;
  ObjectKey key;
  object->GetKey(&key);
  ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(key);
  if (!node)
    return;
  const uint32_t controller = object->ControlSide();
  if (controller == 1)
  {
    m_set6c.Add(node);
  }
  else if (controller == 2)
  {
    m_set74.Add(node);
  }
}

int Notice2::FillText()
{
  GameObjNotifier* mine = GetSideObject(m_owner, m_owner);
  GameObjNotifier* other = GetSideObject(m_owner, otherSide(m_owner));
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_systemKey);
  if (!mine || !system || !other)
    return 1;
  say(&m_text54, 0x7020, system);
  const uint32_t controller = system->ControlSide();
  const uint32_t battleReady = ControlBitsField(static_cast<System*>(system)->m_controlBits, 6);
  m_f4c = battleReady;
  GameObjNotifier* first;
  GameObjNotifier* second;
  GameObjNotifier* third = system;
  uint32_t id;
  if (controller == static_cast<uint32_t>(m_side))
  {
    if (battleReady == controller)
    {
      first = mine;
      second = system;
      third = other;
      id = 0x7022;
    }
    else
    {
      first = other;
      second = system;
      third = mine;
      id = 0x7021;
    }
  }
  else if (controller == 0 || controller == 3)
  {
    first = mine;
    second = other;
    third = system;
    id = 0x7025;
  }
  else if (battleReady == controller)
  {
    first = other;
    second = mine;
    id = 0x7024;
  }
  else
  {
    first = mine;
    second = system;
    third = other;
    id = 0x7023;
  }
  say(&m_text60, id, first, second, third);

  FleetIterator fleets(system, 1);
  FightSquadIterator squads(system, 1);
  CharacterIterator characters(system, 1);
  for (fleets.Begin(); fleets.Current(); fleets.Advance())
  {
    CapShipIterator capitals(fleets.Current(), 1);
    AddObjectKeys(fleets.Current());
    for (capitals.Begin(); capitals.Current(); capitals.Advance())
    {
      FightSquadIterator capitalSquads(capitals.Current(), 1);
      CharacterIterator capitalCharacters(capitals.Current(), 1);
      TroopRegIterator capitalTroops(capitals.Current(), 1);
      AddObjectKeys(capitals.Current());
      for (capitalSquads.Begin(); capitalSquads.Current(); capitalSquads.Advance())
        AddObjectKeys(capitalSquads.Current());
      for (capitalTroops.Begin(); capitalTroops.Current(); capitalTroops.Advance())
        AddObjectKeys(capitalTroops.Current());
      for (capitalCharacters.Begin(); capitalCharacters.Current(); capitalCharacters.Advance())
        AddObjectKeys(capitalCharacters.Current());
    }
  }
  for (squads.Begin(); squads.Current(); squads.Advance())
    AddObjectKeys(squads.Current());
  for (characters.Begin(); characters.Current(); characters.Advance())
    AddObjectKeys(characters.Current());
  return 1;
}
