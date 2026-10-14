#include "ai/ai_task_base.h"
#include "events/player_notices.h"

#include <new>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notif_helpers.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/message_template.h"

namespace
{

void notifyKeys(int owner, ObjectKeySet& set)
{
  for (const auto& entry : set.Entries())
  {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    ObjectKey key(node->m_key);
    UiSendNotificationToSide(owner, asArg(&key));
  }
}

} // namespace

// ---- Notice23 (kind 23) ----

Notice23::Notice23() : UiTextNotice()
{
  m_f64 = 0;
  m_kind = 0x17;
  m_f80 = 0;
  m_class = 3;
  m_categories = GetCategories();
  m_unk40 = 0;
}

Notice23::Notice23(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_f64 = 0;
  m_kind = 0x17;
  m_f80 = 0;
  m_class = 3;
  m_categories = GetCategories();
  m_unk40 = 0;
  DispatchPayloads(payloads);
}

Notice23::~Notice23()
{
  notifyKeys(m_owner, m_keys);
}

int Notice23::GetKind()
{
  return m_kind;
}

int Notice23::GetClass()
{
  return m_class;
}

UiNotice* Notice23::CreateEmptyNotice()
{
  return NewOrNull<Notice23>();
}

void Notice23::OnMessage(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x302)
    return;
  auto* node = NewOrNull<ObjectKeyNode>(*reinterpret_cast<const ObjectKey*>(a));
  m_keys.Add(node);
}

int Notice23::DispatchPayloads(const AiNoticeGroup* payloads)
{
  m_unk28 = 0x14;
  if (!payloads)
    return 0;
  for (const auto& node : payloads->Entries())
    node.get()->DispatchToSink(this);
  return 1;
}

void Notice23::Read(istream* in)
{
  UiTextNotice::Read(in);
  ReadValue32(in, &m_f64);
  ReadValue32(in, &m_kind);
  ReadValue32(in, &m_f80);
  ReadValue32(in, &m_class);
  m_key78.Read(in);
  m_key74.Read(in);
}

void Notice23::Write(ostream* out)
{
  UiTextNotice::Write(out);
  WriteValue32(out, &m_f64);
  WriteValue32(out, &m_kind);
  WriteValue32(out, &m_f80);
  WriteValue32(out, &m_class);
  m_key78.Write(out);
  m_key74.Write(out);
}

// ---- Notice22 (kind 22) ----

Notice22::Notice22() : UiTextNotice()
{
  m_f64 = 0;
  m_kind = 0x16;
  m_f80 = 0;
  m_class = 3;
  m_categories = GetCategories();
  m_order = nullptr;
  m_payloads = nullptr;
  m_unk40 = 0;
}

Notice22::Notice22(const AiNoticeGroup* payloads, int owner) : UiTextNotice(owner)
{
  m_f64 = 0;
  m_kind = 0x16;
  m_categories = GetCategories();
  m_f80 = 0;
  m_class = 3;
  m_order = nullptr;
  m_payloads = nullptr;
  m_unk40 = 0;
  DispatchPayloads(payloads);
}

Notice22::~Notice22()
{
  notifyKeys(m_owner, m_keys);
  delete m_order;
}

UiNotice* Notice22::CreateEmptyNotice()
{
  return NewOrNull<Notice22>();
}

void Notice22::OnMessage(int kind, intptr_t a, intptr_t b)
{
  if (kind != 0x302)
    return;
  auto* node = NewOrNull<ObjectKeyNode>(*reinterpret_cast<const ObjectKey*>(a));
  m_keys.Add(node);
}

void Notice22::OnAccept()
{
  if (m_order)
  {
    delete m_order;
    m_order = nullptr;
  }
}

void Notice22::OnDecline()
{
  if (!m_order)
    return;
  GameObjNotifier* mission = GetObjectInFamilies_40_80(m_owner, &m_key78);
  if (mission)
  {
    UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_order), 1);
    m_order = nullptr;
    return;
  }
  delete m_order;
  m_order = nullptr;
}

int Notice22::DispatchPayloads(const AiNoticeGroup* payloads)
{
  m_unk28 = 0x14;
  m_payloads = payloads;
  if (!payloads)
    return 0;
  for (const auto& node : payloads->Entries())
    node.get()->DispatchToSink(this);
  m_payloads = nullptr;
  return 1;
}

void Notice22::ListSecondKeys()
{
  if (!m_payloads)
    return;
  bool first = true;
  for (const auto& node : m_payloads->Entries())
  {
    NotifPayload* payload = node.get();
    if (payload->GetKind() != 0x370)
      continue;
    ObjectKey scratch;
    GameObjNotifier* object =
      GetObjectInFamilies_90_98(m_owner, copySecondKeyOf(static_cast<KeyPairPayload*>(payload), &scratch));
    if (!object)
      continue;
    if (first)
    {
      ExpandMessageTemplate(&m_text1, 0x710e, nullptr, nullptr, nullptr, nullptr);
      first = false;
    }
    ExpandMessageTemplate(&m_text1, 0x710f, notice::nameOf(object), nullptr, nullptr, nullptr);
  }
}

void Notice22::Read(istream* in)
{
  UiTextNotice::Read(in);
  ReadValue32(in, &m_f64);
  ReadValue32(in, &m_kind);
  ReadValue32(in, &m_f80);
  ReadValue32(in, &m_class);
  m_key78.Read(in);
  m_key74.Read(in);
  unsigned hasOrder;
  ReadValue32(in, &hasOrder);
  if (hasOrder != 0)
  {
    auto* order = static_cast<OrderBase*>(CreateMessageByKind(0x250));
    m_order = order;
    if (order)
    {
      order->m_side = m_owner;
      ObjectKeyValueSet team;
      team.AddKeyValue(&m_key78, 0);
      order->SetTeam(&team);
      ObjectKey unset;
      order->SetTarget(&unset);
    }
  }
}

void Notice22::Write(ostream* out)
{
  unsigned hasOrder = m_order != nullptr;
  UiTextNotice::Write(out);
  WriteValue32(out, &m_f64);
  WriteValue32(out, &m_kind);
  WriteValue32(out, &m_f80);
  WriteValue32(out, &m_class);
  m_key78.Write(out);
  m_key74.Write(out);
  WriteValue32(out, &hasOrder);
}
