#include "events/event_base.h"

#include "core/utility_classes.h"

struct EventFactoryEntry
{
  int m_kind;
  void* (*m_create)();
};

namespace
{
constexpr int kMaxEventFactories = 0x3ff;
}

extern EventFactoryEntry g_eventFactories[]; // 0x6b92f8
extern int g_eventFactoryCount; // 0x6b92f0

// ---- ActivityRequest (serialization) ---------------------------------------------------------------------------

int ActivityRequest::Read(istream* in)
{
  StreamRead32(in, &m_side);
  StreamRead32(in, &m_unk04);
  m_target.Read(in);
  StreamRead32(in, &m_kind);
  return 1;
}

int ActivityRequest::Write(ostream* out)
{
  StreamWrite32(out, &m_side);
  StreamWrite32(out, &m_unk04);
  m_target.Write(out);
  StreamWrite32(out, &m_kind);
  return 1;
}

int ActivityRequest::Checksum(Crc32* crc)
{
  crc->AddInt(m_side);
  crc->AddInt(m_unk04);
  m_target.Checksum(crc);
  crc->AddInt(m_kind);
  return 1;
}

// ---- EventBase -------------------------------------------------------------------------------------------------

EventBase::EventBase() : AvlNode(), m_flag(1), m_unk1c(0), m_context(), m_key()
{
}

EventBase::EventBase(const EventBase& other)
: AvlNode(),
  m_flag(other.m_flag),
  m_unk1c(other.m_unk1c),
  m_context(other.m_context),
  m_key(other.m_key)
{
}

EventBase::~EventBase() = default;

void EventBase::ReadBase(istream* in)
{
  ReadValue32(in, &m_flag);
  ReadValue32(in, &m_unk1c);
  m_key.Read(in);
  m_context.Read(in);
}

void EventBase::WriteBase(ostream* out)
{
  WriteValue32(out, &m_flag);
  WriteValue32(out, &m_unk1c);
  m_key.Write(out);
  m_context.Write(out);
}

void EventBase::ChecksumBase(Crc32* crc)
{
  crc->AddInt(m_flag);
  crc->AddInt(m_unk1c);
  m_key.Checksum(crc);
  m_context.Checksum(crc);
}

// ---- TimedEvent ------------------------------------------------------------------------------------------------

TimedEvent::TimedEvent() : EventBase(), m_dueDay(0)
{
}

TimedEvent::TimedEvent(const TimedEvent& other) = default;

TimedEvent::~TimedEvent() = default;

int TimedEvent::ReadTimed(istream* in)
{
  ReadBase(in);
  ReadValue32(in, &m_dueDay);
  return 1;
}

int TimedEvent::WriteTimed(ostream* out)
{
  WriteBase(out);
  WriteValue32(out, &m_dueDay);
  return 1;
}

int TimedEvent::ChecksumTimed(Crc32* crc)
{
  ChecksumBase(crc);
  crc->AddInt(m_dueDay);
  return 1;
}

// ---- Command ---------------------------------------------------------------------------------------------------

Command::Command() : EventBase(), m_request()
{
}

Command::Command(const Command& other) = default;

Command::~Command() = default;

void Command::SetRequest(const ActivityRequest* request)
{
  m_request = *request;
}

void Command::StampContext()
{
  if (m_request.m_unk04 == 0)
    return;
  NotifyContext context(m_context);
  context.m_reserved14 = m_request.m_unk04;
  context.m_reserved18 = GetKind();
  m_context = context;
}

void Command::ReadCommand(istream* in)
{
  ReadBase(in);
  m_request.Read(in);
}

void Command::WriteCommand(ostream* out)
{
  WriteBase(out);
  m_request.Write(out);
}

int Command::ChecksumCommand(Crc32* crc)
{
  ChecksumBase(crc);
  m_request.Checksum(crc);
  return 1;
}

// ---- PlainEvent ------------------------------------------------------------------------------------------------

PlainEvent::PlainEvent() : EventBase()
{
}

PlainEvent::PlainEvent(const PlainEvent& other) = default;

PlainEvent::~PlainEvent() = default;

int PlainEvent::ReadPlain(istream* in)
{
  ReadBase(in);
  return 1;
}

int PlainEvent::WritePlain(ostream* out)
{
  WriteBase(out);
  return 1;
}

int PlainEvent::ChecksumPlain(Crc32* crc)
{
  ChecksumBase(crc);
  return 1;
}

// ---- KeyedEvent ------------------------------------------------------------------------------------------------

KeyedEvent::KeyedEvent() : PlainEvent(), m_keyA(), m_keyB(), m_value(0)
{
}

KeyedEvent::KeyedEvent(const KeyedEvent& other) = default;

KeyedEvent::~KeyedEvent() = default;

int KeyedEvent::ReadKeyed(istream* in)
{
  ReadPlain(in);
  int ok = 0;
  if (m_keyA.Read(in) != 0)
  {
    ok = m_keyB.Read(in);
    if (ok != 0)
      ReadValue32(in, &m_value);
  }
  return ok;
}

int KeyedEvent::WriteKeyed(ostream* out)
{
  WritePlain(out);
  int ok = 0;
  if (m_keyA.Write(out) != 0)
  {
    ok = m_keyB.Write(out);
    if (ok != 0)
      WriteValue32(out, &m_value);
  }
  return ok;
}

int KeyedEvent::ChecksumKeyed(Crc32* crc)
{
  ChecksumPlain(crc);
  m_keyA.Checksum(crc);
  m_keyB.Checksum(crc);
  crc->AddInt(m_value);
  return 1;
}

// ---- SourceEvent -----------------------------------------------------------------------------------------------

SourceEvent::SourceEvent() : KeyedEvent(), m_valueA(0), m_valueB(0)
{
}

SourceEvent::SourceEvent(const SourceEvent& other) = default;

SourceEvent::~SourceEvent() = default;

int SourceEvent::ReadSource(istream* in)
{
  ReadKeyed(in);
  ReadValue32(in, &m_valueA);
  ReadValue32(in, &m_valueB);
  return 1;
}

int SourceEvent::WriteSource(ostream* out)
{
  WriteKeyed(out);
  WriteValue32(out, &m_valueA);
  WriteValue32(out, &m_valueB);
  return 1;
}

int SourceEvent::ChecksumSource(Crc32* crc)
{
  ChecksumKeyed(crc);
  crc->AddInt(m_valueA);
  crc->AddInt(m_valueB);
  return 1;
}

// ---- TwoKeyEvent -----------------------------------------------------------------------------------------------

TwoKeyEvent::TwoKeyEvent() : KeyedEvent(), m_keyC(), m_keyD()
{
}

TwoKeyEvent::TwoKeyEvent(const TwoKeyEvent& other) = default;

TwoKeyEvent::~TwoKeyEvent() = default;

void TwoKeyEvent::ReadTwoKey(istream* in)
{
  ReadKeyed(in);
  if (m_keyC.Read(in) != 0)
    m_keyD.Read(in);
}

void TwoKeyEvent::WriteTwoKey(ostream* out)
{
  WriteKeyed(out);
  if (m_keyC.Write(out) != 0)
    m_keyD.Write(out);
}

int TwoKeyEvent::ChecksumTwoKey(Crc32* crc)
{
  ChecksumKeyed(crc);
  m_keyC.Checksum(crc);
  m_keyD.Checksum(crc);
  return 1;
}

// ---- factory table ---------------------------------------------------------------------------------------------

void* CreateEventById(int kind)
{
  void* (*create)() = nullptr;
  for (int i = 0; i < g_eventFactoryCount && create == nullptr; ++i)
  {
    if (g_eventFactories[i].m_kind == kind)
      create = g_eventFactories[i].m_create;
  }
  return create ? create() : nullptr;
}

int RegisterEventFactory(int kind, void* (*factory)())
{
  bool absent = true;
  for (int i = 0; i < g_eventFactoryCount && absent; ++i)
    absent = g_eventFactories[i].m_kind != kind;
  if (absent && g_eventFactoryCount + 1 < kMaxEventFactories)
  {
    g_eventFactories[g_eventFactoryCount].m_kind = kind;
    g_eventFactories[g_eventFactoryCount].m_create = factory;
    ++g_eventFactoryCount;
  }
  return 0;
}

EventFactoryEntry g_eventFactories[0x3ff]{};
int g_eventFactoryCount{};
