#include "events/player_notices.h"

#include <new>

#include "character/character.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notif_helpers.h"
#include "events/ui_notifs_ext.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "story/story_characters.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/message_template.h"

namespace
{

const uint32_t kHanKey = 0x33000243;

bool dagobahDone(GameObjNotifier* luke)
{
  return (static_cast<Luke*>(luke)->m_storyFlags & Luke::kDagobahCompleted) != 0;
}

} // namespace

// ---- Notice25 (kind 25) ----

Notice25::Notice25() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_unk40 = 0;
}

Notice25::Notice25(const LinkNode* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_categories = Virtual11();
  Virtual16(payloads);
}

Notice25::~Notice25() = default;

int Notice25::GetKind()
{
  return 25;
}

UiNotice* Notice25::Virtual10()
{
  return NewOrNull<Notice25>();
}

void Notice25::Virtual05(int kind, intptr_t payload)
{
  ObjectKey characterKey;
  ObjectKey nameKey;
  if (kind != 0x1e0)
    return;
  UiNotif1e0* notif = reinterpret_cast<UiNotif1e0*>(payload);
  characterKey = ObjectKey(notif->m_key);
  GameObjNotifier* character = GetObjectInFamilies_01_ff(m_owner, &characterKey);
  GameObjNotifier* place = nullptr;
  GameObjNotifier* first = nullptr;
  GameObjNotifier* otherSide = GetSideObject(m_owner, m_owner == 1 ? 2 : 1);
  m_sound[0] = 0x44c;
  m_picture[1] = (m_owner != 1) + 0x412;
  if (character)
  {
    ObjectKey unset;
    ObjectKey scratch;
    if (notif->CopySystemKey(&scratch)->m_value != unset.m_value)
    {
      place = GetObjectInFamilies_01_ff(m_owner, notif->CopySystemKey(&scratch));
    }
    if (!place)
    {
      ObjectKey locationCopy(notif->m_locationKey);
      if (locationCopy.m_value != unset.m_value)
        place = GetObjectInFamilies_01_ff(m_owner, &locationCopy);
      if (!place)
      {
        if (notif->CopyGrandparentKey(&scratch)->m_value != unset.m_value)
          place = GetObjectInFamilies_01_ff(m_owner, notif->CopyGrandparentKey(&scratch));
      }
    }
    if (notice::inFamilies(characterKey, 0x30, 0x3c))
    {
      m_key = ObjectKey(characterKey);
      m_unk40 = 1;
      Character* subject = static_cast<Character*>(character);
      bool sameSide = subject->GetRecordSide() == 1;
      if (m_owner == 2)
        sameSide = !sameSide;
      first = character;
      if (subject->m_state == 0)
      {
        ObjectKey parentKey;
        character->m_parent->GetKey(&parentKey);
        if (parentKey.m_value == 0x98000481)
        {
          m_picture[1] = 0x429;
          m_sound[0] = 0x478;
          m_f64 = 5;
        }
        else
        {
          m_picture[1] = (subject->GetRecordSide() != 1) + 0x418;
          m_f64 = sameSide ? 3 : 4;
          if (characterKey.m_value == kHanKey && (static_cast<StoryCharacter*>(character)->m_storyFlags & 2) != 0)
            m_unk40 = 0;
        }
      }
      else
      {
        m_picture[1] = 0x405;
        m_sound[0] = 0x469;
        m_f64 = sameSide ? 1 : 2;
      }
    }
    else
    {
      m_unk40 = 0;
    }
  }
  if (m_unk40 == 0)
    return;
  nameKey = ObjectKey(characterKey);
  bool storyCharacter = notice::inFamilies(nameKey, 0x30, 0x38);
  uint32_t id = nameKey.m_value & 0xffffff;
  switch (m_f64)
  {
    case 1:
      ExpandMessageTemplate(&m_text0, 0x715c, notice::nameOf(first), notice::nameOf(place), notice::nameOf(otherSide),
                            nullptr);
      ExpandMessageTemplate(&m_text1, 0x715d, notice::nameOf(first), notice::nameOf(place), notice::nameOf(otherSide),
                            nullptr);
      if (storyCharacter)
      {
        switch (id)
        {
          case 0x240:
            m_unk28 = 0x26;
            m_sound[1] = AdvisorPickAdviceLine(0xf6);
            break;
          case 0x241:
            m_unk28 = 0x18;
            m_sound[1] = AdvisorPickAdviceLine(0xe2);
            break;
          case 0x242:
            m_unk28 = 0x25;
            m_sound[1] = AdvisorPickAdviceLine(dagobahDone(first) ? 0xd8 : 0xce);
            break;
          case 0x243:
            m_unk28 = 0x19;
            m_sound[1] = AdvisorPickAdviceLine(0xec);
            break;
          case 0x280:
            m_unk28 = 0x27;
            m_sound[1] = AdvisorPickAdviceLine(0x10a);
            break;
          case 0x281:
            m_unk28 = 0x28;
            m_sound[1] = AdvisorPickAdviceLine(0x100);
            break;
        }
        return;
      }
      break;
    case 2:
      ExpandMessageTemplate(&m_text0, 0x715e, notice::nameOf(first), notice::nameOf(place), notice::nameOf(otherSide),
                            nullptr);
      ExpandMessageTemplate(&m_text1, 0x715f, notice::nameOf(first), notice::nameOf(place), notice::nameOf(otherSide),
                            nullptr);
      if (storyCharacter)
      {
        switch (id)
        {
          case 0x240:
            m_unk28 = 0x26;
            break;
          case 0x241:
            m_unk28 = 0x18;
            break;
          case 0x242:
            m_unk28 = 0x25;
            break;
          case 0x243:
            m_unk28 = 0x19;
            break;
          case 0x280:
            m_unk28 = 0x27;
            break;
          case 0x281:
            m_unk28 = 0x28;
            break;
        }
      }
      else
      {
        m_unk28 = 0x24;
      }
      return;
    case 3:
    case 4:
    {
      bool pending = m_f64 == 3;
      ExpandMessageTemplate(&m_text0, pending ? 0x7158 : 0x715a, notice::nameOf(first), notice::nameOf(place),
                            notice::nameOf(otherSide), nullptr);
      ExpandMessageTemplate(&m_text1, pending ? 0x7159 : 0x715b, notice::nameOf(first), notice::nameOf(place),
                            notice::nameOf(otherSide), nullptr);
      if (storyCharacter)
      {
        switch (id)
        {
          case 0x240:
            m_unk28 = 0x1f;
            break;
          case 0x241:
            m_unk28 = 0x20;
            break;
          case 0x242:
            m_unk28 = 0x1e;
            break;
          case 0x243:
            m_unk28 = 0x21;
            break;
          case 0x280:
            m_unk28 = 0x22;
            break;
          case 0x281:
            m_unk28 = 0x23;
            break;
        }
        return;
      }
      break;
    }
    case 5:
      ExpandMessageTemplate(&m_text0, 0x7156, notice::nameOf(first), nullptr, nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7157, notice::nameOf(first), nullptr, nullptr, nullptr);
      if (storyCharacter)
      {
        switch (id)
        {
          case 0x240:
            m_unk28 = 0x1f;
            break;
          case 0x241:
            m_unk28 = 0x20;
            break;
          case 0x242:
            m_unk28 = 0x1e;
            break;
          case 0x243:
            m_unk28 = 0x21;
            m_unk40 = 0;
            break;
          case 0x280:
            m_unk28 = 0x22;
            break;
          case 0x281:
            m_unk28 = 0x23;
            break;
        }
        return;
      }
      break;
    default:
      m_unk40 = 0;
      return;
  }
  m_unk28 = 0x15;
}

// ---- Notice24 (kind 24) ----

Notice24::Notice24() : UiTextNotice()
{
  m_categories = Virtual11();
}

Notice24::Notice24(const LinkNode* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_categories = Virtual11();
  Virtual16(payloads);
}

Notice24::~Notice24() = default;

int Notice24::GetKind()
{
  return 24;
}

UiNotice* Notice24::Virtual10()
{
  return NewOrNull<Notice24>();
}

void Notice24::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x153)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, key);
  m_key = ObjectKey(*key);
  if (!system || b == 0)
    return;
  uint32_t controller = system->ControlSide();
  bool show;
  if (controller == 1)
    show = m_owner == 2;
  else if (controller == 2)
    show = m_owner == 1;
  else
    show = true;
  if (show)
    m_unk40 = 1;
  ExpandMessageTemplate(&m_text0, 0x7150, notice::nameOf(system), nullptr, nullptr, nullptr);
  ExpandMessageTemplate(&m_text1, 0x7151, notice::nameOf(system), nullptr, nullptr, nullptr);
  m_picture[1] = 1000;
  m_unk28 = 0x1c;
  m_sound[0] = 0x44c;
}
