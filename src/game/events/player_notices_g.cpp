#include "ai/ai_task_base.h"
#include "events/player_notices.h"

#include <new>

#include "ai/ai_common.h"
#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notifs_ext.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_type_db.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/message_template.h"

namespace
{

constexpr uint32_t kSystemBattlePending = 0x1000;
constexpr uint32_t kSystemGarrisoned = 0x100;
constexpr uint32_t kSystemUprising = 0x4;

} // namespace

// ---- Notice16 (kind 16) ----

Notice16::Notice16() : UiTextNotice()
{
  m_f64 = 0;
  m_f68 = 0;
  m_categories = Virtual11();
  m_unk40 = 0;
}

Notice16::Notice16(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_categories = Virtual11();
  m_f68 = 0;
  m_f64 = 0;
  Virtual16(payloads);
}

Notice16::~Notice16() = default;

UiNotice* Notice16::Virtual10()
{
  return NewOrNull<Notice16>();
}

void Notice16::OnMessage(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x303)
    return;
  const ObjectKey* keyA = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, keyA);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(b));
  if (object && system && m_f68 == 0)
  {
    m_f68 = 1;
    if (notice::inFamilies(*keyA, 0x20, 0x30))
    {
      m_unk40 = 1;
      m_f64 = 5;
    }
    if (m_f64 == 0)
      m_unk40 = 0;
    if (m_unk40 != 0)
    {
      ExpandMessageTemplate(&m_text0, 0x70b0, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x70b1, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      m_picture[1] = (m_owner != 1) + 0x3f7;
      m_unk28 = 3;
      m_key = ObjectKey(*reinterpret_cast<const ObjectKey*>(b));
    }
  }
}

void Notice16::Read(istream* in)
{
  UiTextNotice::Read(in);
  ReadValue32(in, &m_f64);
  ReadValue32(in, &m_f68);
}

void Notice16::Write(ostream* out)
{
  UiTextNotice::Write(out);
  WriteValue32(out, &m_f64);
  WriteValue32(out, &m_f68);
}

// ---- Notice14 (kind 14) ----

Notice14::Notice14() : UiTextNotice()
{
  m_f64 = 0;
  m_f68 = 0;
  m_categories = Virtual11();
  m_unk40 = 0;
}

Notice14::Notice14(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_f68 = 0;
  m_categories = Virtual11();
  m_f64 = 0;
  Virtual16(payloads);
}

Notice14::~Notice14() = default;

UiNotice* Notice14::Virtual10()
{
  return NewOrNull<Notice14>();
}

void Notice14::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x107)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
  GameObjNotifier* system = nullptr;
  if (!object)
    return;
  m_key = ObjectKey(*key);
  GameObjNotifier* walker = object->m_parent;
  do
  {
    if (!walker)
      break;
    ObjectKey walkerKey;
    walker->GetKey(&walkerKey);
    GameObjNotifier* candidate = walker;
    if (!notice::inFamilies(walkerKey, 0x90, 0x98))
    {
      walker = walker->m_parent;
      candidate = system;
    }
    system = candidate;
  } while (!system);
  GameObjNotifier* parent = object->m_parent;
  if (m_f68 != 0)
    return;
  m_unk40 = 1;
  m_f68 = 1;
  if (m_owner == 1)
  {
    if (object->ControlSide() != 1)
      m_unk40 = 0;
  }
  else if (m_owner != 2 || object->ControlSide() != 2)
  {
    m_unk40 = 0;
  }
  if (notice::inFamilies(*key, 0x18, 0x1c))
  {
    m_f64 = 4;
    m_categories = 0x80;
  }
  else if (notice::inFamilies(*key, 0x14, 0x1c))
  {
    m_categories = 0x80;
    m_f64 = 3;
  }
  else if (notice::inFamilies(*key, 0x1c, 0x20))
  {
    m_f64 = 1;
    ObjectKey parentKey;
    parent->GetKey(&parentKey);
    m_categories = notice::inFamilies(parentKey, 0x90, 0x98) ? 0x40 : 0x80;
  }
  else if (!notice::inFamilies(*key, 0x10, 0x14))
  {
    if (notice::inFamilies(*key, 0x20, 0x30))
    {
      if (notice::inFamilies(*key, 0x22, 0x28))
      {
        m_categories = 0x40;
        m_f64 = 5;
      }
      else if (notice::inFamilies(*key, 0x28, 0x2c))
      {
        m_categories = 8;
        m_f64 = 5;
      }
      else
      {
        if (notice::inFamilies(*key, 0x2c, 0x30))
          m_categories = 4;
        m_f64 = 5;
      }
    }
    else if (notice::inFamilies(*key, 0x3c, 0x40))
    {
      ObjectKey recordKey;
      if (GetObjectRecordKey(object, &recordKey)->m_value == 0x3c000009)
        m_unk40 = 0;
      m_categories = 0x10;
      m_f64 = 6;
    }
  }
  else
  {
    m_f64 = 2;
    ObjectKey parentKey;
    parent->GetKey(&parentKey);
    m_categories = notice::inFamilies(parentKey, 0x90, 0x98) ? 0x40 : 0x80;
  }
  if (!system)
    m_unk40 = 0;
  if (m_f64 == 0)
    m_unk40 = 0;
  if (m_unk40 == 0)
    return;
  switch (m_f64)
  {
    case 1:
      ExpandMessageTemplate(&m_text0, 0x708a, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x708b, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      m_sound[0] = (m_owner != 1) + 0x457;
      break;
    case 2:
    case 6:
      ExpandMessageTemplate(&m_text0, 0x7088, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7089, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x707f, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      break;
    case 3:
      ExpandMessageTemplate(&m_text0, 0x7086, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7087, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      break;
    case 4:
      ExpandMessageTemplate(&m_text0, 0x708e, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x708f, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      break;
    case 5:
      ExpandMessageTemplate(&m_text0, 0x708c, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x708d, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
      break;
    default:
      m_unk40 = 0;
  }
  m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(object));
  m_unk28 = 3;
}

// ---- Notice13 (kind 13) ----

Notice13::Notice13() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_f68 = 0;
  m_unk40 = 0;
}

Notice13::Notice13(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_f68 = 0;
  m_categories = Virtual11();
  m_f64 = 0;
  Virtual16(payloads);
}

Notice13::~Notice13() = default;

UiNotice* Notice13::Virtual10()
{
  return NewOrNull<Notice13>();
}

void Notice13::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x105)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
  GameObjNotifier* system = nullptr;
  bool systemFlag = false;
  if (!object)
    return;
  GameObjNotifier* walker = object->m_parent;
  m_key = ObjectKey(*key);
  do
  {
    if (!walker)
      break;
    ObjectKey walkerKey;
    walker->GetKey(&walkerKey);
    GameObjNotifier* candidate = walker;
    if (!notice::inFamilies(walkerKey, 0x90, 0x98))
    {
      walker = walker->m_parent;
      candidate = system;
    }
    system = candidate;
  } while (!system);
  GameObjNotifier* parent = object->m_parent;
  if (m_f68 == 0)
  {
    int owner = m_owner;
    m_unk40 = 1;
    m_f68 = 1;
    if (owner == 1)
    {
      if (object->ControlSide() != 1)
        m_unk40 = 0;
    }
    else if (owner != 2 || object->ControlSide() != 2)
    {
      m_unk40 = 0;
    }
    m_sound[0] = (owner != 1) + 0x45b;
    if (notice::inFamilies(*key, 8, 0x10))
    {
      m_sound[0] = 0x45a;
      m_f64 = 1;
      m_categories = 0x80;
    }
    else if (notice::inFamilies(*key, 0x20, 0x22))
    {
      m_sound[0] = 0x45d;
      m_f64 = 4;
      m_categories = 0x40;
    }
    else if (notice::inFamilies(*key, 0x14, 0x1c))
    {
      m_sound[0] = 0x45a;
      m_f64 = 2;
      m_categories = 0x80;
    }
    else if (!notice::inFamilies(*key, 0x30, 0x40))
    {
      m_f64 = 3;
      if (notice::inFamilies(*key, 0x22, 0x28))
      {
        m_categories = 0x40;
      }
      else if (notice::inFamilies(*key, 0x28, 0x2c))
      {
        m_categories = 8;
      }
      else if (notice::inFamilies(*key, 0x2c, 0x30))
      {
        m_categories = 8;
      }
      else if (notice::inFamilies(*key, 0x10, 0x14) || notice::inFamilies(*key, 0x1c, 0x20))
      {
        ObjectKey parentKey;
        parent->GetKey(&parentKey);
        m_categories = notice::inFamilies(parentKey, 0x90, 0x98) ? 0x40 : 0x80;
      }
    }
    if (system)
      systemFlag = (static_cast<System*>(system)->m_flags >> 12) & 1;
    if (!system)
      m_unk40 = 0;
    if (systemFlag)
      m_unk40 = 0;
    if (m_f64 == 0)
      m_unk40 = 0;
    if (m_unk40 == 0)
      goto finish;
    switch (m_f64)
    {
      case 1:
      case 2:
        ExpandMessageTemplate(&m_text0, m_f64 == 1 ? 0x7078 : 0x7076, notice::nameOf(object), notice::nameOf(system),
                              nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, m_f64 == 1 ? 0x7079 : 0x7077, notice::nameOf(object), notice::nameOf(system),
                              nullptr, nullptr);
        m_unk28 = 5;
        m_picture[1] = m_owner == 1 ? 0x3fa : 0x3fb;
        break;
      case 3:
        ExpandMessageTemplate(&m_text0, 0x707a, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x707b, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
        m_unk28 = 6;
        m_picture[1] = m_owner == 1 ? 0x3fc : 0x3fd;
        break;
      case 4:
        ExpandMessageTemplate(&m_text0, 0x707c, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x707d, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
        m_unk28 = 6;
        if (m_owner == 1)
          m_picture[1] = 0x3fe;
        else
          m_unk40 = 0;
        break;
      default:
        m_unk40 = 0;
    }
  }
finish:
  if (m_unk40 != 0 && (m_f64 == 2 || m_f64 == 3))
    ExpandMessageTemplate(&m_text1, 0x707e, notice::nameOf(object), notice::nameOf(system), nullptr, nullptr);
}

// ---- Notice12 (kind 12) ----

Notice12::Notice12() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
}

Notice12::Notice12(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_unk40 = 0;
  Virtual16(payloads);
}

Notice12::~Notice12() = default;

int Notice12::GetKind()
{
  return 12;
}

UiNotice* Notice12::Virtual10()
{
  return NewOrNull<Notice12>();
}

void Notice12::OnMessage(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x304)
    return;
  GameObjNotifier* first = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(a));
  GameObjNotifier* second = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(b));
  ObjectKey systemKey;
  GameObjNotifier* system = notice::containingSystem(second, &systemKey);
  if (system)
  {
    system->GetKey(&systemKey);
    m_key = ObjectKey(systemKey);
  }
  if (first && second)
  {
    if (m_f64 == 0)
    {
      m_f64 = 1;
      ExpandMessageTemplate(&m_text0, 0x7080, nullptr, nullptr, nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7081, notice::nameOf(first), notice::nameOf(second), notice::nameOf(system),
                            nullptr);
      m_unk28 = 0xc;
      m_sound[0] = 0x456;
    }
    ExpandMessageTemplate(&m_text1, 0x7082, notice::nameOf(first), notice::nameOf(second), nullptr, nullptr);
    m_unk40 = 1;
    uint32_t controller = first->ControlSide();
    if (controller == 1)
    {
      if (m_owner != 2)
        return;
    }
    else
    {
      if (controller != 2)
        return;
      if (m_owner != 1)
        return;
    }
    m_unk40 = 0;
  }
}

int Notice12::Virtual16(const AiNoticeGroup* payloads)
{
  int result = ForwardPayloads(this, payloads);
  if (m_unk40 != 0)
  {
    m_unk28 = 0xc;
    m_sound[0] = 0x456;
    if (m_owner == 1)
    {
      m_picture[1] = 0x3f5;
      return result;
    }
    m_picture[1] = 0x3f6;
  }
  return result;
}

// ---- Notice11 (kind 11) ----

Notice11::Notice11() : UiTextNotice()
{
  m_categories = Virtual11();
  m_unk40 = 0;
}

Notice11::Notice11(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  m_unk40 = 0;
  Virtual16(payloads);
}

Notice11::~Notice11() = default;

int Notice11::GetKind()
{
  return 11;
}

UiNotice* Notice11::Virtual10()
{
  return NewOrNull<Notice11>();
}

void Notice11::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c)
{
  if (kind != 0x148)
    return;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(a));
  m_key = ObjectKey(*reinterpret_cast<const ObjectKey*>(a));
  if (system)
  {
    uint32_t controller = system->ControlSide();
    if (controller == 1)
    {
      m_sound[0] = 0x452;
      m_unk40 = m_owner == 1;
      m_picture[1] = 0x3f0;
    }
    else if (controller == 2)
    {
      m_sound[0] = 0x453;
      m_unk40 = m_owner == 2;
      m_picture[1] = 0x3f1;
    }
    else
    {
      m_unk40 = 0;
    }
    if (m_unk40 == 0)
      return;
    uint32_t flags = static_cast<System*>(system)->m_flags;
    if ((flags & kSystemGarrisoned) != 0 && (flags & kSystemUprising) == 0 && c != 0 && b == 0)
      m_unk40 = 1;
    else
      m_unk40 = 0;
  }
  else
  {
    m_unk40 = 0;
  }
  if (m_unk40 != 0)
  {
    ExpandMessageTemplate(&m_text0, 0x7070, notice::nameOf(system), nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x7071, notice::nameOf(system), nullptr, nullptr, nullptr);
    m_unk28 = 2;
  }
}

// ---- Notice10 (kind 10) ----

Notice10::Notice10() : UiTextNotice()
{
  m_categories = Virtual11();
}

Notice10::Notice10(const char* text, int owner, int showFirst) : UiTextNotice(owner)
{
  int side = showFirst == 0 ? m_owner : 2 - (owner != 1);
  m_categories = Virtual11();
  m_unk40 = 1;
  if (side == 1)
  {
    m_picture[1] = 0x410;
    m_sound[0] = 0x461;
  }
  else
  {
    m_picture[1] = 0x411;
    m_sound[0] = 0x462;
  }
  SetSecondText(text);
  GameObjNotifier* other = GetSideObject(m_owner, 2 - (m_owner != 1));
  if (showFirst == 0)
  {
    ExpandMessageTemplate(&m_text0, 0x71f1, notice::nameOf(other), nullptr, nullptr, nullptr);
  }
  else
  {
    ExpandMessageTemplate(&m_text0, 0x71f0, notice::nameOf(other), nullptr, nullptr, nullptr);
    m_unk28 = 0x29;
  }
}

Notice10::~Notice10() = default;

int Notice10::GetKind()
{
  return 10;
}

UiNotice* Notice10::Virtual10()
{
  return NewOrNull<Notice10>();
}

void Notice10::SetSecondText(const char* text)
{
  m_text1 = text;
}

// ---- Notice9 (kind 9) ----

Notice9::Notice9() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_f68 = 0;
}

Notice9::Notice9(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_f64 = 0;
  m_categories = Virtual11();
  m_f68 = 0;
  Virtual16(payloads);
}

Notice9::~Notice9() = default;

int Notice9::GetKind()
{
  return 9;
}

UiNotice* Notice9::Virtual10()
{
  return NewOrNull<Notice9>();
}

void Notice9::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c)
{
  if (kind != 0x145)
    return;
  if (b != 0 && c != 0)
  {
    m_unk40 = 0;
    return;
  }
  m_unk40 = 1;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(a));
  bool increase = false;
  if (!system)
  {
    m_unk40 = 0;
    return;
  }
  uint32_t controller = system->ControlSide();
  if (controller == 1)
  {
    m_f68 = m_owner == 1 ? 1 : 0;
    increase = c > -1;
  }
  else if (controller == 2)
  {
    m_f68 = m_owner == 2 ? 1 : 0;
    increase = c < 1;
  }
  else
  {
    m_f68 = 0;
  }
  if (increase)
  {
    uint32_t secondText;
    if (b == 0)
    {
      if (m_f68 == 0)
      {
        ExpandMessageTemplate(&m_text0, 0x7063, notice::nameOf(system), nullptr, nullptr, nullptr);
        secondText = 0x7067;
      }
      else
      {
        ExpandMessageTemplate(&m_text0, 0x7061, notice::nameOf(system), nullptr, nullptr, nullptr);
        secondText = 0x7065;
      }
    }
    else
    {
      m_sound[0] = 0x44f;
      if (m_f68 == 0)
      {
        ExpandMessageTemplate(&m_text0, 0x7062, notice::nameOf(system), nullptr, nullptr, nullptr);
        secondText = 0x7066;
      }
      else
      {
        ExpandMessageTemplate(&m_text0, 0x7060, notice::nameOf(system), nullptr, nullptr, nullptr);
        secondText = 0x7064;
      }
    }
    ExpandMessageTemplate(&m_text1, secondText, notice::nameOf(system), nullptr, nullptr, nullptr);
  }
  else
  {
    m_unk40 = 0;
  }
  m_picture[1] = 0x3ec;
  ObjectKey scratch;
  system->GetKey(&scratch);
  m_key = ObjectKey(scratch);
}

// ---- Notice3 (kind 3) ----

Notice3::Notice3() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f64 = 0;
}

Notice3::Notice3(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  m_f64 = 0;
  m_unk40 = 0;
  Virtual16(payloads);
}

Notice3::~Notice3() = default;

UiNotice* Notice3::Virtual10()
{
  return NewOrNull<Notice3>();
}

void Notice3::Virtual08(int kind, intptr_t a, intptr_t b)
{
  GameObjNotifier* other = nullptr;
  uint32_t sameSide = 0;
  if (kind != 0x14b)
    return;
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(a));
  m_key = ObjectKey(*reinterpret_cast<const ObjectKey*>(a));
  bool show = false;
  if (system)
  {
    uint32_t controller = system->ControlSide();
    if (controller == 1)
    {
      other = GetSideObject(m_owner, 1);
      m_picture[1] = 0x3f3;
      sameSide = m_owner == 1;
    }
    else if (controller == 2)
    {
      other = GetSideObject(m_owner, 2);
      m_picture[1] = 0x3f4;
      sameSide = m_owner == 2;
    }
    else
    {
      m_unk40 = 0;
    }
    if ((static_cast<System*>(system)->m_flags & kSystemUprising) == 0)
    {
      if (b == 0)
        show = true;
    }
    else if (b != 0)
    {
      show = true;
    }
  }
  m_unk40 = show ? 1 : 0;
  if (!other)
    m_unk40 = 0;
  if (m_unk40 != 0)
  {
    if (b == 0)
    {
      m_f64 = 0;
      m_sound[0] = 0x455;
      ExpandMessageTemplate(&m_text0, 0x7031, notice::nameOf(system), nullptr, nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7032, notice::nameOf(other), notice::nameOf(system), nullptr, nullptr);
    }
    else
    {
      m_f64 = 1;
      m_sound[0] = 0x454;
      ExpandMessageTemplate(&m_text0, 0x7030, notice::nameOf(system), nullptr, nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7033, notice::nameOf(other), notice::nameOf(system), nullptr, nullptr);
      m_picture[1] = 0x3f2;
    }
    if (sameSide != static_cast<uint32_t>(m_f64))
    {
      m_unk28 = 1;
      return;
    }
    m_unk28 = 2;
  }
}

// ---- Notice4 (kind 4) ----

Notice4::Notice4() : UiTextNotice()
{
  m_categories = Virtual11();
}

Notice4::Notice4(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  Virtual16(payloads);
}

Notice4::~Notice4() = default;

UiNotice* Notice4::Virtual10()
{
  return NewOrNull<Notice4>();
}

void Notice4::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c)
{
  if (kind != 0x100)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* system = nullptr;
  if (notice::inFamilies(*key, 0x90, 0x98))
  {
    system = GetObjectInFamilies_90_98(m_owner, key);
    m_key = ObjectKey(*key);
    bool applies = system && system->ControlSide() == static_cast<uint32_t>(b) &&
                   (static_cast<System*>(system)->m_flags & 0x2000) != 0;
    m_unk40 = applies ? 1 : 0;
  }
  if (m_unk40 == 0 || !system)
    return;
  GameObjNotifier* ownSide = nullptr;
  bool sameSide = false;
  m_f64 = static_cast<int>(b);
  if (b == 1)
  {
    ownSide = GetSideObject(m_owner, 1);
    sameSide = m_owner == 1;
  }
  else if (b == 2)
  {
    ownSide = GetSideObject(m_owner, 2);
    sameSide = m_owner == 2;
  }
  GameObjNotifier* previous = nullptr;
  if (c == 1)
    previous = GetSideObject(m_owner, 1);
  else if (c == 2)
    previous = GetSideObject(m_owner, 2);
  uint32_t secondText;
  GameObjNotifier* partner;
  if (!ownSide)
  {
    m_sound[0] = 0x451;
    ExpandMessageTemplate(&m_text0, 0x703a, notice::nameOf(system), notice::nameOf(previous), nullptr, nullptr);
    secondText = 0x703d;
    partner = previous;
  }
  else
  {
    m_sound[0] = 0x450;
    if (sameSide)
    {
      m_unk28 = 1;
      ExpandMessageTemplate(&m_text0, 0x7038, notice::nameOf(system), notice::nameOf(ownSide), nullptr, nullptr);
      secondText = 0x703b;
    }
    else
    {
      m_unk28 = 2;
      ExpandMessageTemplate(&m_text0, 0x7039, notice::nameOf(system), notice::nameOf(ownSide), nullptr, nullptr);
      secondText = 0x703c;
    }
    partner = ownSide;
  }
  ExpandMessageTemplate(&m_text1, secondText, notice::nameOf(system), notice::nameOf(partner), nullptr, nullptr);
  m_picture[1] = 0x3ed;
}

// ---- Notice5 (kind 5) ----

Notice5::Notice5() : UiTextNotice()
{
  m_categories = Virtual11();
}

Notice5::Notice5(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  Virtual16(payloads);
}

Notice5::~Notice5() = default;

int Notice5::GetKind()
{
  return 5;
}

UiNotice* Notice5::Virtual10()
{
  return NewOrNull<Notice5>();
}

void Notice5::Virtual08(int kind, intptr_t a, intptr_t b)
{
  DatRecord* record = nullptr;
  if (kind == 0x125)
  {
    m_unk40 = 1;
    GetTierRecord28(m_owner, static_cast<int>(b), &record);
    if (!record)
      return;
    ExpandMessageTemplate(&m_text0, 0x7048, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x7049, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
  }
  else if (kind == 0x126)
  {
    m_unk40 = 1;
    GetTierRecord29(m_owner, static_cast<int>(b), &record);
    if (!record)
      return;
    ExpandMessageTemplate(&m_text0, 0x7048, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x7049, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
  }
  else if (kind == 0x127)
  {
    m_unk40 = 1;
    GetTierRecord2a(m_owner, static_cast<int>(b), &record);
    if (!record)
      return;
    ExpandMessageTemplate(&m_text0, 0x7048, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x7049, static_cast<ObjectBase*>(record), nullptr, nullptr, nullptr);
  }
  if (record && m_unk40 != 0)
  {
    ObjectKey scratch;
    m_target.m_value = GetAiRecordKey(record, &scratch)->m_value;
    m_unk28 = 4;
    m_sound[0] = 0x459;
    m_picture[1] = 0x3f9;
  }
}

// ---- Notice6 (kind 6) ----

Notice6::Notice6() : UiTextNotice()
{
  m_categories = Virtual11();
}

Notice6::Notice6(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_categories = Virtual11();
  Virtual16(payloads);
}

Notice6::~Notice6() = default;

UiNotice* Notice6::Virtual10()
{
  return NewOrNull<Notice6>();
}

void Notice6::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x106)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
  GameObjNotifier* parent = nullptr;
  bool squadron = false;
  m_unk40 = 0;
  if (b == 0 && object)
  {
    if (notice::inFamilies(*key, 0x14, 0x1c))
    {
      object = GetObjectInFamilies_14_1c(m_owner, key);
      if (object)
        parent = object->m_parent;
      m_categories = 0x80;
    }
    else if (notice::inFamilies(*key, 0x1c, 0x20))
    {
      squadron = true;
      object = GetObjectInFamilies_1c_20(m_owner, key);
      parent = object->m_parent;
      ObjectKey parentKey;
      parent->GetKey(&parentKey);
      if (notice::inFamilies(parentKey, 0x14, 0x1c))
      {
        parent = parent->m_parent;
        m_categories = 0x80;
      }
      else
      {
        m_categories = 0x40;
      }
    }
    if (object && parent)
      m_unk40 = 1;
    bool keep;
    if (m_owner == 1)
      keep = object->ControlSide() != 2;
    else if (m_owner != 2)
      keep = true;
    else
      keep = object->ControlSide() != 1;
    if (!keep)
      m_unk40 = 0;
  }
  if (m_unk40 != 0)
  {
    m_key = ObjectKey(*key);
    if (squadron)
    {
      m_unk28 = 9;
      m_sound[0] = (m_owner != 1) + 0x457;
      ExpandMessageTemplate(&m_text0, 0x7040, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7042, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
    }
    else
    {
      m_unk28 = 8;
      ExpandMessageTemplate(&m_text0, 0x7041, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      ExpandMessageTemplate(&m_text1, 0x7043, notice::nameOf(object), notice::nameOf(parent), nullptr, nullptr);
      m_sound[0] = 0x45e;
    }
    m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(object));
  }
}
