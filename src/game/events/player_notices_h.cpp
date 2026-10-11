#include "ai/ai_task_base.h"
#include "events/player_notices.h"

#include <new>

#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notifs_ext.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/message_template.h"

namespace
{

// The notices that hold sets of objects tell the side's UI about every one of them when they are destroyed.
void notifyKeys(int owner, ObjectKeySet& set)
{
  for (const auto& entry : set.Entries())
  {
    ObjectKeyNode* node = static_cast<ObjectKeyNode*>(entry.get());
    ObjectKey key(node->m_key);
    UiSendNotificationToSide(owner, asArg(&key));
  }
}

} // namespace

// ---- Notice8 (kind 8) ----

Notice8::Notice8() : UiNotice()
{
  m_categories = Virtual11();
}

Notice8::Notice8(int owner, MessageBase* subject) : UiNotice(owner)
{
  m_categories = Virtual11();
  m_subject = subject;
  BuildText();
}

Notice8::~Notice8() = default;

UiNotice* Notice8::Virtual10()
{
  return NewOrNull<Notice8>();
}

void Notice8::AppendName(GameObjNotifier* object)
{
  if (object)
    ExpandMessageTemplate(&m_text1, 0x7054, notice::nameOf(object), nullptr, nullptr, nullptr);
}

void Notice8::Read(istream* in)
{
  UiNotice::Read(in);
  ReadLegacyString(in, m_text0);
  ReadLegacyString(in, m_text1);
}

void Notice8::Write(ostream* out)
{
  UiNotice::Write(out);
  WriteLegacyString(out, m_text0);
  WriteLegacyString(out, m_text1);
}

// ---- Notice1 (kind 1) ----

Notice1::Notice1() : UiNotice()
{
  m_categories = Virtual11();
  m_f50 = 0;
  m_f54 = 0;
  m_f58 = 0;
}

Notice1::Notice1(const AiNoticeGroup* payloads, int owner, Notice2* vote) : UiNotice(owner)
{
  m_categories = Virtual11();
  m_f50 = 0;
  m_f54 = 0;
  if (owner == 1)
    m_f58 = 1;
  else if (owner == 2)
    m_f58 = 2;
  else
    m_f58 = 0;
  ReadVote(vote);
  m_unk1c = DispatchAndFill(payloads);
}

Notice1::~Notice1()
{
  notifyKeys(m_owner, m_set78);
  notifyKeys(m_owner, m_set80);
}

UiNotice* Notice1::Virtual10()
{
  return NewOrNull<Notice1>();
}

void Notice1::Virtual08(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x14d)
    return;
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  if (!notice::inFamilies(*key, 0x90, 0x98))
    return;
  m_systemKey = ObjectKey(*key);
  GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &m_systemKey);
  if (system)
  {
    m_f50 = static_cast<System*>(system)->m_controlBits & 3;
    m_f54 = (static_cast<System*>(system)->m_controlBits >> 2) & 3;
  }
}

int Notice1::DispatchAndFill(const AiNoticeGroup* payloads)
{
  for (const auto& node : payloads->Entries())
    node.get()->DispatchToSink(this);
  if (m_f50 != 0)
  {
    if (m_f50 != m_f58)
    {
      FillOtherSide();
      return 1;
    }
    FillOwnSide();
    return 1;
  }
  FillNeutral();
  return 1;
}

void Notice1::Read(istream* in)
{
  UiNotice::Read(in);
  m_systemKey.Read(in);
  m_keyB.Read(in);
  ReadValue32(in, &m_f50);
  ReadValue32(in, &m_f54);
  ReadValue32(in, &m_f58);
  m_set60.Read(in);
  m_set68.Read(in);
  m_set78.Read(in);
  m_set80.Read(in);
  ReadLegacyString(in, m_text88);
  ReadLegacyString(in, m_text94);
  ReadLegacyString(in, m_texta0);
  ReadLegacyString(in, m_textac);
  ReadLegacyString(in, m_textb8);
}

void Notice1::Write(ostream* out)
{
  UiNotice::Write(out);
  m_systemKey.Write(out);
  m_keyB.Write(out);
  WriteValue32(out, &m_f50);
  WriteValue32(out, &m_f54);
  WriteValue32(out, &m_f58);
  m_set60.Write(out);
  m_set68.Write(out);
  m_set78.Write(out);
  m_set80.Write(out);
  WriteLegacyString(out, m_text88);
  WriteLegacyString(out, m_text94);
  WriteLegacyString(out, m_texta0);
  WriteLegacyString(out, m_textac);
  WriteLegacyString(out, m_textb8);
}

// ---- Notice2 (kind 2) ----

Notice2::Notice2() : UiNotice()
{
  m_categories = Virtual11();
  m_side = 0;
  m_attack = 0;
}

Notice2::Notice2(const ObjectKey* key, int owner) : UiNotice(owner)
{
  m_categories = Virtual11();
  m_systemKey = ObjectKey(*key);
  m_attack = 0;
  if (owner == 1)
    m_side = 1;
  else if (owner == 2)
    m_side = 2;
  else
    m_side = 0;
  m_unk1c = FillText();
}

Notice2::~Notice2() = default;

UiNotice* Notice2::Virtual10()
{
  return NewOrNull<Notice2>();
}

void Notice2::Read(istream* in)
{
  UiNotice::Read(in);
  m_systemKey.Read(in);
  ReadValue32(in, &m_side);
  ReadValue32(in, &m_attack);
  ReadValue32(in, &m_f4c);
  m_set6c.Read(in);
  m_set74.Read(in);
  ReadLegacyString(in, m_text54);
  ReadLegacyString(in, m_text60);
}

void Notice2::Write(ostream* out)
{
  UiNotice::Write(out);
  m_systemKey.Write(out);
  WriteValue32(out, &m_side);
  WriteValue32(out, &m_attack);
  WriteValue32(out, &m_f4c);
  m_set6c.Write(out);
  m_set74.Write(out);
  WriteLegacyString(out, m_text54);
  WriteLegacyString(out, m_text60);
}

// ---- Notice15 (kind 15) ----

Notice15::Notice15() : UiTextNotice()
{
  m_categories = Virtual11();
  m_f68 = 0;
  m_f6c = 0;
  m_fa0 = 0;
  m_f64 = 0;
  m_unk40 = 0;
}

Notice15::Notice15(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_unk40 = 0;
  m_categories = Virtual11();
  m_f68 = 0;
  m_f6c = 0;
  m_fa0 = 0;
  m_f64 = 0;
  Virtual16(payloads);
}

Notice15::~Notice15()
{
  notifyKeys(m_owner, m_set88);
  notifyKeys(m_owner, m_set80);
}

int Notice15::GetClass()
{
  return (m_owner == 1 ? m_f6c != 1 : m_f6c != 2) ? 3 : 7;
}

UiNotice* Notice15::Virtual10()
{
  return NewOrNull<Notice15>();
}

void Notice15::OnMessage(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x302)
  {
    UiNotice::OnMessage(kind, a, b);
    return;
  }
  const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
  ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(*key);
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
  bool done = object && (static_cast<GameObj*>(object)->m_stateFlags & 4) == 0;
  ObjectKeySet* target = &m_set80;
  if (!done && !notice::inFamilies(*reinterpret_cast<const ObjectKey*>(b), 0x90, 0x98))
    target = &m_set88;
  target->Add(node);
}

void Notice15::Read(istream* in)
{
  UiTextNotice::Read(in);
  ReadValue32(in, &m_fa0);
  ReadValue32(in, &m_f64);
  ReadValue32(in, &m_f68);
  ReadValue32(in, &m_f6c);
  m_set70.Read(in);
  m_set80.Read(in);
  m_set78.Read(in);
  m_set88.Read(in);
}

void Notice15::Write(ostream* out)
{
  UiTextNotice::Write(out);
  WriteValue32(out, &m_fa0);
  WriteValue32(out, &m_f64);
  WriteValue32(out, &m_f68);
  WriteValue32(out, &m_f6c);
  m_set70.Write(out);
  m_set80.Write(out);
  m_set78.Write(out);
  m_set88.Write(out);
}
