#include "ai/ai_task_base.h"
#include "events/player_notices.h"

#include <new>

#include "character/character.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notif_helpers.h"
#include "events/ui_notifs_ext.h"
#include "fleet/capship_subclasses.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "story/story_characters.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/message_template.h"

ObjectKey* GetObjectRecordKey(GameObjNotifier* object, ObjectKey* out); // 0x402d80

namespace
{

bool dagobahDone(GameObjNotifier* luke)
{
  return (static_cast<Luke*>(luke)->m_storyFlags & Luke::kDagobahCompleted) != 0;
}

} // namespace

// ---- Notice21 (kind 21) ----

Notice21::Notice21() : UiKeyNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_f6c = 0;
  m_unk40 = 0;
}

Notice21::Notice21(const AiNoticeGroup* payloads, int owner) : UiKeyNotice(owner)
{
  m_unk40 = 0;
  m_categories = Virtual11();
  m_f64 = 0;
  m_f6c = 0;
  Virtual14(payloads);
}

Notice21::~Notice21() = default;

int Notice21::GetKind()
{
  return 21;
}

int Notice21::GetClass()
{
  return 6;
}

UiNotice* Notice21::Virtual10()
{
  return NewOrNull<Notice21>();
}

void Notice21::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 2)
  {
    if (kind != 0x182)
      return;
    GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_owner, reinterpret_cast<const ObjectKey*>(a));
    if (!fleet)
      return;
    DeathStarIterator it(fleet, 3);
    it.Begin();
    if (it.Current())
    {
      ObjectKey key;
      it.Current()->GetKey(&key);
      m_subject = ObjectKey(key);
    }
    m_unk40 = 1;
    return;
  }
  GetSideObject(m_owner, 2);
  GameObjNotifier* side = nullptr;
  if (m_f64 == 1)
    side = GetSideObject(m_owner, 1);
  else if (m_f64 == 2)
    side = GetSideObject(m_owner, 2);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_key);
  GameObjNotifier* unit = GetObjectInFamilies_01_ff(m_owner, &m_subject);
  ExpandMessageTemplate(&m_text0, 0x70c8, notice::nameOf(unit), notice::nameOf(system), nullptr, nullptr);
  if (!side)
    ExpandMessageTemplate(&m_text1, 0x70ca, notice::nameOf(unit), notice::nameOf(system), nullptr, nullptr);
  else
    ExpandMessageTemplate(&m_text1, 0x70c9, notice::nameOf(side), notice::nameOf(unit), notice::nameOf(system),
                          nullptr);
  m_value = 0x6f;
}

void Notice21::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c)
{
  if (kind != 0x14f)
    return;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(a));
  if (!system)
    return;
  ObjectKey key;
  system->GetKey(&key);
  m_key = ObjectKey(key);
  if (b == 0)
  {
    m_unk40 = 1;
    return;
  }
  m_f64 = static_cast<int>(c);
}

void Notice21::Read(istream* in)
{
  UiKeyNotice::Read(in);
  ReadValue32(in, &m_f6c);
  ReadValue32(in, &m_f64);
  m_subject.Read(in);
}

void Notice21::Write(ostream* out)
{
  UiKeyNotice::Write(out);
  WriteValue32(out, &m_f6c);
  WriteValue32(out, &m_f64);
  m_subject.Write(out);
}

// ---- Notice20 (kind 20) ----

Notice20::Notice20() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_unk40 = 0;
}

Notice20::Notice20(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_f64 = 0;
  m_categories = Virtual11();
  m_unk40 = 0;
  Virtual16(payloads);
}

Notice20::~Notice20() = default;

int Notice20::GetKind()
{
  return 20;
}

UiNotice* Notice20::Virtual10()
{
  return NewOrNull<Notice20>();
}

void Notice20::Virtual05(int kind, intptr_t payload)
{
  ObjectKey characterKey;
  if (kind != 0x200)
    return;
  const UiNotif200* notif = reinterpret_cast<const UiNotif200*>(payload);
  characterKey = ObjectKey(notif->m_key);
  ObjectKey missionKey(notif->m_missionKey);
  GameObjNotifier* character = GetObjectInFamilies_01_ff(m_owner, &characterKey);
  GameObjNotifier* place = nullptr;
  bool showName = true;
  if (m_f64 == 0)
  {
    m_f64 = 1;
    m_picture[1] = (m_owner != 1) + 0x412;
    m_sound[0] = (m_owner != 1) + 0x47c;
  }
  if (!character || notif->m_valueB != 0 || (static_cast<GameObj*>(character)->m_stateFlags & 0x20) != 0)
  {
    m_unk40 = 0;
  }
  else
  {
    GameObjNotifier* walker = character->m_parent;
    do
    {
      if (!walker)
        break;
      ObjectKey walkerKey;
      walker->GetKey(&walkerKey);
      GameObjNotifier* candidate = walker;
      if (!notice::inFamilies(walkerKey, 0x90, 0x98))
      {
        walker->GetKey(&walkerKey);
        candidate = place;
        if (notice::inFamilies(walkerKey, 0x98, 0xa0))
        {
          walker = nullptr;
          m_unk40 = 0;
        }
        else
        {
          walker = walker->m_parent;
        }
      }
      place = candidate;
    } while (!place);
    m_unk40 = place ? 1 : 0;
    if (!notice::inFamilies(missionKey, 0x40, 0x50))
      m_unk40 = 0;
    ObjectKey recordKey;
    if (GetObjectRecordKey(character, &recordKey)->m_value == 0x3c000009)
      m_unk40 = 0;
  }
  if (m_unk40 != 0)
  {
    if (notif->m_leadsMission != 0)
    {
      m_unitKey = ObjectKey(characterKey);
      m_key = ObjectKey(characterKey);
      if (notice::inFamilies(m_unitKey, 0x30, 0x38))
      {
        ExpandMessageTemplate(&m_text0, 0x70c1, notice::nameOf(character), notice::nameOf(place), nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, m_f78 < 2 ? 0x70c5 : 0x70c4, notice::nameOf(character), notice::nameOf(place),
                              nullptr, nullptr);
        showName = false;
        switch (m_unitKey.m_value & 0xffffff)
        {
          case 0x240:
            m_unk28 = 0x17;
            m_sound[1] = AdvisorPickAdviceLine(0xf0);
            break;
          case 0x241:
            m_unk28 = 0x18;
            m_sound[1] = AdvisorPickAdviceLine(0xdc);
            break;
          case 0x242:
            m_unk28 = 0x16;
            m_sound[1] = AdvisorPickAdviceLine(dagobahDone(character) ? 0xd2 : 200);
            break;
          case 0x243:
            m_unk28 = 0x19;
            m_sound[1] = AdvisorPickAdviceLine(0xe6);
            break;
          case 0x280:
            m_unk28 = 0x1a;
            m_sound[1] = AdvisorPickAdviceLine(0x104);
            break;
          case 0x281:
            m_unk28 = 0x1b;
            m_sound[1] = AdvisorPickAdviceLine(0xfa);
            break;
        }
      }
      else
      {
        ExpandMessageTemplate(&m_text0, 0x70c0, notice::nameOf(character), notice::nameOf(place), nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x70c3, notice::nameOf(character), notice::nameOf(place), nullptr, nullptr);
        m_unk28 = 0x15;
      }
      uint32_t family = character->GetFamily();
      if (family >= 0x30 && family < 0x3c)
        m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(character));
    }
    if (m_unk40 != 0 && showName)
      ExpandMessageTemplate(&m_name, 0x70c2, notice::nameOf(character), notice::nameOf(place), nullptr, nullptr);
  }
  if (!m_key.IsSet())
    m_unk40 = 0;
}

int Notice20::Virtual16(const AiNoticeGroup* payloads)
{
  int result = 1;
  m_unk28 = 0x14;
  if (!payloads)
    return 0;
  m_f78 = CountUnscheduled(payloads);
  for (const auto& node : payloads->Entries())
    node.get()->DispatchToSink(this);
  if (m_unk40 != 0)
  {
    if (m_picture[0] == 0 && notice::inFamilies(m_unitKey, 0x3c, 0x40))
    {
      GameObjNotifier* unit = GetObjectInFamilies_01_ff(m_owner, &m_unitKey);
      if (unit)
        m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(unit));
    }
    std::string suffix(m_name.c_str());
    m_text1 += suffix;
  }
  return result;
}

int Notice20::CountUnscheduled(const AiNoticeGroup* payloads)
{
  int count = 0;
  for (const auto& node : payloads->Entries())
  {
    NotifPayload* payload = node.get();
    if (payload->GetKind() == 0x200 && static_cast<IntPairPayload*>(payload)->m_valueB == 0)
      ++count;
  }
  return count;
}

void Notice20::Read(istream* in)
{
  UiTextNotice::Read(in);
  ReadValue32(in, &m_f64);
  m_unitKey.Read(in);
}

void Notice20::Write(ostream* out)
{
  UiTextNotice::Write(out);
  WriteValue32(out, &m_f64);
  m_unitKey.Write(out);
}
