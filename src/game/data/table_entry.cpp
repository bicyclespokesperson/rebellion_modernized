#include "data/table_entry.h"

#include "core/iostream_lib.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"

struct TableEntryClassRegistration {
  char m_name[0x28];
  void* (*m_factory)();
};

extern int g_tableEntryClassCount;
extern TableEntryClassRegistration g_tableEntryClasses[];

namespace
{

constexpr int kMaxTableEntryClasses = 10;

// The original compares inline (the CRT's strcmp is not called).
bool sameName(const char* a, const char* b)
{
  while (*a == *b) {
    if (*a == '\0') {
      return true;
    }
    ++a;
    ++b;
  }
  return false;
}

} // namespace

// ---- TableEntry --------------------------------------------------------------------------------------------

TableEntry::TableEntry()
    : m_field1c(1)
    , m_field20(0)
{
}

TableEntry::~TableEntry() = default;

int TableEntry::Read(istream* in)
{
  ReadId(in);
  ReadValue32(in, &m_field1c);
  ReadValue32(in, &m_field20);
  return 1;
}

int TableEntry::Write(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_field1c);
  WriteValue32(out, &m_field20);
  return 1;
}

int TableEntry::Checksum(Crc32* crc)
{
  crc->AddInt(m_id);
  crc->AddInt(m_field1c);
  crc->AddInt(m_field20);
  return 1;
}

int TableEntry::ParseHeader(istream* in, int* endOfLine)
{
  ScopedStrstream token;
  bool ok = false;
  if (iostream_access::State(in) == 0) {
    ok = true;
    ReadCsvField(in, token.get(), endOfLine);
    streambuf* buffer = iostream_access::BufferOf(token.get());
    if (iostream_access::WrittenBytes(buffer) == 0) {
      ok = false;
    }
    if (strstreambuf_str(buffer) != nullptr && *strstreambuf_str(buffer) == '\0') {
      ok = false;
    }
    strstreambuf_freeze(buffer, 0);
    if (iostream_access::State(in) != 0) {
      ok = false;
    }
    if (ok) {
      istream_operator_rs_long(iostream_access::AsIstream(token.get()), &m_field20);
    }
  }
  return ok;
}

// ---- Class registry ----------------------------------------------------------------------------------------

int IsTableEntryClassRegistered(const char* className)
{
  bool found = false;
  for (int i = 0; i < g_tableEntryClassCount && !found; ++i) {
    found = sameName(className, g_tableEntryClasses[i].m_name);
  }
  return found;
}

int RegisterTableEntryClass(const char* className, void* (*factory)())
{
  const bool roomLeft = g_tableEntryClassCount + 1 < kMaxTableEntryClasses;
  if (roomLeft && !IsTableEntryClassRegistered(className)) {
    TableEntryClassRegistration& entry = g_tableEntryClasses[g_tableEntryClassCount];
    entry.m_factory = factory;
    char* out = entry.m_name;
    char c = *className;
    *out = c;
    int remaining = sizeof(entry.m_name);
    while (c != '\0' && --remaining != 0) {
      c = *++className;
      *++out = c;
    }
    ++g_tableEntryClassCount;
    entry.m_name[sizeof(entry.m_name) - 1] = '\0';
  }
  return 0;
}

void* CreateTableEntryByClassName(const char* className)
{
  void* entry = nullptr;
  for (int i = 0; i < g_tableEntryClassCount && entry == nullptr; ++i) {
    const TableEntryClassRegistration& registration = g_tableEntryClasses[i];
    if (sameName(className, registration.m_name)) {
      entry = registration.m_factory();
    }
  }
  return entry;
}

TableEntryClassRegistration g_tableEntryClasses[10]{};
int g_tableEntryClassCount{};
