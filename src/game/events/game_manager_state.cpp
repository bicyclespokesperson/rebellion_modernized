#include "events/game_manager_state.h"

#include <cstring>
#include <limits>
#include <new>

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/utility_classes.h"
#include "events/events_decl.h"

extern const char* g_emptyText; // 0x6b120c
void* CreateKeyedObject(int key); // 0x51f730

// ---- legacy option blocks ---------------------------------------------------------------------------------------

void LegacyBlockA::Reset()
{
  m_value[0] = 0;
  m_value[1] = 0;
  m_value[2] = 1;
  for (int i = 3; i < 8; ++i)
    m_value[i] = 0;
  m_value[8] = 1;
  m_value[9] = 1;
  m_value[10] = 0;
  m_value[11] = 0;
  m_value[12] = 0;
  m_value[13] = 10;
  m_value[14] = 10;
  m_value[15] = 0x78;
  for (int i = 16; i < 20; ++i)
    m_value[i] = 0;
}

int LegacyBlockA::Read(istream* in)
{
  for (int32_t& value : m_value)
    StreamRead32(in, &value);
  return 1;
}

int LegacyBlockA::Write(ostream* out)
{
  for (int32_t& value : m_value)
    StreamWrite32(out, &value);
  return 1;
}

LegacyBlockB::LegacyBlockB()
{
  for (int32_t& value : m_value)
    value = -1;
}

int LegacyBlockB::Read(istream* in)
{
  for (int32_t& value : m_value)
    StreamRead32(in, &value);
  return 1;
}

int LegacyBlockB::Write(ostream* out)
{
  for (int32_t& value : m_value)
    StreamWrite32(out, &value);
  return 1;
}

int ReadLegacyBlocks(istream* in)
{
  g_legacyBlockA.Read(in);
  g_legacyBlockB[0].Read(in);
  g_legacyBlockB[1].Read(in);
  g_legacyBlockB[2].Read(in);
  return 1;
}

int WriteLegacyBlocks(ostream* out)
{
  g_legacyBlockA.Write(out);
  g_legacyBlockB[0].Write(out);
  g_legacyBlockB[1].Write(out);
  g_legacyBlockB[2].Write(out);
  return 1;
}

// ---- legacy text slots ------------------------------------------------------------------------------------------

void ResetLegacyTexts()
{
  g_legacyTextCount = 0;
  g_legacyTextExtra = 0;
  for (std::string& text : g_legacyTexts)
    text.clear();
}

void ReadLegacyTexts(istream* in)
{
  int count = 0;
  ReadValue32(in, &count);
  ReadValue32(in, &g_legacyTextExtra);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || count < 0 || count > 10)
  {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    return;
  }
  g_legacyTextCount = count;
  std::string* text = g_legacyTexts;
  for (int i = 0; i < g_legacyTextCount; ++i, ++text)
    ReadLegacyString(in, *text);
}

void WriteLegacyTexts(ostream* out)
{
  WriteValue32(out, &g_legacyTextCount);
  WriteValue32(out, &g_legacyTextExtra);
  std::string* text = g_legacyTexts;
  for (int i = 0; i < g_legacyTextCount; ++i, ++text)
    WriteLegacyString(out, *text);
}

LegacyTextTable::LegacyTextTable()
{
  SetDefaults();
  AllocateSlots();
}

LegacyTextTable::~LegacyTextTable()
{
  Reset();
}

int LegacyTextTable::ReadFrom(istream* in)
{
  Reset();
  StreamRead32(in, &m_count);
  StreamRead32(in, &m_width);
  StreamRead32(in, &m_unk0c);
  int hasSlots = 0;
  StreamRead32(in, &hasSlots);
  if (hasSlots != 0)
  {
    AllocateSlots();
    if (m_slots)
      istream_read(in, m_slots, m_width * m_count);
  }
  return 1;
}

int LegacyTextTable::Read(istream* in)
{
  return ReadFrom(in);
}

void LegacyTextTable::SetDefaults()
{
  m_count = 100;
  m_width = 200;
  m_unk0c = 0;
  m_slots = nullptr;
}

void LegacyTextTable::Reset()
{
  m_count = 0;
  m_width = 0;
  m_unk0c = 0;
  if (m_slots)
  {
    operator delete(m_slots);
    m_slots = nullptr;
  }
}

void LegacyTextTable::AllocateSlots()
{
  if (m_count == 0 || m_width == 0 || m_slots != nullptr)
    return;
  m_slots = static_cast<char*>(operator new(m_width * m_count));
  if (!m_slots)
    return;
  for (int i = 0; i < m_count; ++i)
    memcpy(m_slots + i * m_width, g_emptyText, strlen(g_emptyText) + 1);
}

// ---- posted-event queues ----------------------------------------------------------------------------------------

PostedEventQueue::PostedEventQueue() : m_flag(1)
{
}

PostedEventQueue::~PostedEventQueue() = default;

void PostedEventQueue::Clear()
{
  m_payloads.clear();
}

void PostedEventQueue::Enqueue(std::unique_ptr<NotifPayload> payload)
{
  if (payload)
    m_payloads.push_back(std::move(payload));
}

std::unique_ptr<NotifPayload> PostedEventQueue::Dequeue()
{
  if (m_payloads.empty())
    return nullptr;
  auto payload = std::move(m_payloads.front());
  m_payloads.pop_front();
  return payload;
}

int PostedEventQueue::Read(istream* in)
{
  Clear();
  ReadValue32(in, &m_flag);
  uint32_t count = 0;
  ReadValue32(in, &count);
  if (iostream_access::IosOf(in)->m_state & iostream_flags::kFail)
    return 0;
  for (uint32_t i = 0; i < count; ++i)
  {
    int kind = 0;
    ReadValue32(in, &kind);
    if (iostream_access::IosOf(in)->m_state & iostream_flags::kFail)
      return 0;
    std::unique_ptr<NotifPayload> payload(static_cast<NotifPayload*>(CreateKeyedObject(kind)));
    if (!payload)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    payload->Read(in);
    if (iostream_access::IosOf(in)->m_state & iostream_flags::kFail)
      return 0;
    Enqueue(std::move(payload));
  }
  return 1;
}

int PostedEventQueue::Write(ostream* out)
{
  WriteValue32(out, &m_flag);
  if (!WriteCount32(out, m_payloads.size()))
    return 0;
  for (const auto& payload : m_payloads)
  {
    int kind = payload->GetKind();
    WriteValue32(out, &kind);
    payload->Write(out);
  }
  return (iostream_access::IosOf(out)->m_state & iostream_flags::kFail) == 0;
}

// ---- tick table -------------------------------------------------------------------------------------------------

TickTable::TickTable() : m_ready(1)
{
}

TickTable::~TickTable()
{
  Clear();
}

void TickTable::Clear()
{
  if (m_clearing)
    return;
  m_clearing = true;
  decltype(m_nodes) detached;
  detached.swap(m_nodes);
  detached.clear();
  m_clearing = false;
}

int TickTable::Insert(SeedKeyCountNode* node)
{
  if (m_clearing || !node)
    return 0;
  auto position = m_nodes.emplace(node->m_key, nullptr);
  position->second.reset(node);
  return 1;
}

int TickTable::Read(istream* in)
{
  return ReadNodes(in);
}

int TickTable::ReadNodes(istream* in)
{
  Clear();
  ReadValue32(in, &m_ready);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  for (uint32_t i = 0; i < count; ++i)
  {
    auto* node = NewOrNull<SeedKeyCountNode>();
    if (!node)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    node->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || !Insert(node))
    {
      delete node;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
  }
  return 1;
}

int TickTable::Write(ostream* out)
{
  return WriteNodes(out);
}

int TickTable::WriteNodes(ostream* out)
{
  if (Count() > std::numeric_limits<uint32_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return 0;
  }
  WriteValue32(out, &m_ready);
  auto count = static_cast<uint32_t>(Count());
  WriteValue32(out, &count);
  for (auto nodePosition = Entries().begin(); nodePosition != Entries().end(); ++nodePosition)
  {
    nodePosition->second->Write(out);
  }
  return 1;
}

int TickTable::Checksum(Crc32* crc)
{
  int result = 1;
  crc->AddInt(m_ready);
  crc->AddInt(static_cast<int32_t>(Count()));
  for (auto nodePosition = Entries().begin(); nodePosition != Entries().end(); ++nodePosition)
  {
    result = nodePosition->second->Checksum(crc);
  }
  return result;
}

SeedKeyCountNode* TickTable::Find(SeedKey* key)
{
  auto cursor = m_nodes.lower_bound(*key);
  return cursor != m_nodes.end() && cursor->first == *key ? cursor->second.get() : nullptr;
}

SeedKeyCountNode* TickTable::Create(SeedKey* key)
{
  auto* node = NewOrNull<SeedKeyCountNode>(*key);
  AddNode(node);
  return node;
}

void TickTable::AddNode(SeedKeyCountNode* node)
{
  if (node)
    Insert(node);
}
