#include "data/table.h"

#include <cstring>

#include "core/checked_new.h"
#include "core/file_util.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "iostream/iostream.h"

// The directory callback that locates the game data on the CD (0x4fd320).
const char* GetGameDataDirectory();

extern GDataTables* g_gdataTables;
extern int g_tableClassCount;

struct TableClassRegistration
{
  char m_name[0x28];
  void* (*m_factory)();
};
extern TableClassRegistration g_tableClasses[];
extern int g_tableClassRegistered;

namespace
{

constexpr int kMaxTableClasses = 10;

struct TableFileSpec
{
  uint16_t m_secondId;
  uint32_t m_tableId;
};

constexpr uint16_t kTableDirectoryStringId = 0x642;

constexpr TableFileSpec kTableFiles[] = {
  {0x708, 0x01}, {0x709, 0x02}, {0x70b, 0x03}, {0x70c, 0x04}, {0x70d, 0x05}, {0x712, 0x0a}, {0x713, 0x0b},
  {0x714, 0x0c}, {0x715, 0x0d}, {0x71c, 0x14}, {0x71d, 0x15}, {0x71e, 0x16}, {0x71f, 0x17}, {0x720, 0x18},
  {0x721, 0x19}, {0x722, 0x1a}, {0x723, 0x1b}, {0x724, 0x1c}, {0x725, 0x1d}, {0x730, 0x28}, {0x731, 0x29},
  {0x732, 0x2a}, {0x733, 0x2b}, {0x734, 0x2c}, {0x744, 0x3c}, {0x745, 0x3d}, {0x758, 0x50}, {0x759, 0x51},
  {0x75a, 0x52}, {0x75b, 0x53}, {0x75c, 0x54}, {0x75d, 0x55}, {0x75e, 0x56}, {0, 0},
};

} // namespace

// ---- TableBase ---------------------------------------------------------------------------------------------

TableBase::TableBase() : m_version(1)
{
}

TableBase::~TableBase() = default;

int TableBase::ReadVersion(istream* in)
{
  ReadValue32(in, &m_version);
  return 1;
}

int TableBase::WriteVersion(ostream* out)
{
  WriteValue32(out, &m_version);
  return 1;
}

int TableBase::ChecksumVersion(Crc32* crc)
{
  crc->AddInt(m_version);
  return 1;
}

// ---- Table -------------------------------------------------------------------------------------------------

Table::Table()
{
  m_name[0] = '\0';
}

Table::~Table() = default;

void Table::Add(TableEntry* entry)
{
  if (entry)
    m_entries.AddWithId(entry);
}

TableEntry* Table::FindStepEntry(int value)
{
  auto position = m_entries.Entries().begin();
  if (position != m_entries.Entries().end())
  {
    AvlNode* node = position->get();
    do
    {
      if (value < static_cast<TableEntry*>(node)->m_field20)
        break;
      ++position;
      node = position == m_entries.Entries().end() ? nullptr : position->get();
    } while (position != m_entries.Entries().end());
    if (node)
    {
      if (position == m_entries.Entries().begin())
        return static_cast<TableEntry*>(node);
      return static_cast<TableEntry*>(std::prev(position)->get());
    }
  }
  return static_cast<TableEntry*>(m_entries.Last());
}

int Table::Read(istream* in)
{
  m_entries.Clear();
  int ok = ReadVersion(in);
  if (!ok || (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  uint32_t count = 0;
  int nameLength = 0;
  ReadValue32(in, &count);
  ReadValue32(in, &nameLength);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || nameLength < 1 ||
      nameLength >= static_cast<int>(sizeof(m_name)))
  {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    return 0;
  }
  istream_read(in, m_name, nameLength);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  m_name[nameLength] = '\0';
  for (uint32_t index = 0; index < count; ++index)
  {
    TableEntry* entry = static_cast<TableEntry*>(CreateTableEntryByClassName(m_name));
    if (!entry)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    ok = entry->Read(in);
    if (!ok || (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || m_entries.AddWithId(entry) == 0)
    {
      delete entry;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
  }
  return ok;
}

int Table::Write(ostream* out)
{
  int ok = WriteVersion(out);
  if (!ok)
    return 0;
  if (!WriteCount32(out, m_entries.Count()))
    return 0;
  int length = static_cast<int>(std::strlen(m_name));
  WriteValue32(out, &length);
  ostream_write(out, m_name, length);
  for (const auto& entry : m_entries.Entries())
  {
    AvlNode* node = entry.get();
    ok = static_cast<TableEntry*>(node)->Write(out);
    if (!ok)
      return 0;
  }
}

int Table::Checksum(Crc32* crc)
{
  int ok = ChecksumVersion(crc);
  if (!ok)
    return ok;
  crc->AddInt(static_cast<int32_t>(m_entries.Count()));
  const int length = static_cast<int>(std::strlen(m_name));
  crc->AddInt(length);
  crc->Update(reinterpret_cast<const uint8_t*>(m_name), length);
  for (const auto& entry : m_entries.Entries())
  {
    AvlNode* node = entry.get();
    ok = static_cast<TableEntry*>(node)->Checksum(crc);
    if (!ok)
      break;
  }
  return ok;
}

int Table::ParseText(istream* in)
{
  ScopedStrstream token;
  int endOfLine = 0;
  // The original first calls 0x51ebb0, a stub that always returns 1.
  int ok = 1;
  ReadCsvField(in, token.get(), &endOfLine);
  istream_operator_rs_charptr(iostream_access::AsIstream(ReadCsvField(in, token.get(), &endOfLine)), m_name);
  SkipRestOfLine(in, &endOfLine);
  ok = IsTableEntryClassRegistered(m_name);
  if (!ok)
    return ok;
  SkipTableHeader(in);
  if (iostream_access::State(in) != 0)
    return ok;
  do
  {
    TableEntry* entry = static_cast<TableEntry*>(CreateTableEntryByClassName(m_name));
    if (entry)
    {
      if (entry->ParseText(in))
        Add(entry);
      else
        delete entry;
    }
  } while (iostream_access::State(in) == 0);
  return ok;
}

int TableBase::Open(uint16_t firstId, uint16_t secondId, const char* directory)
{
  ScopedIfstream file;
  ScopedStrstream prefixedName;
  ScopedStrstream plainName;
  ostream* prefixedOut = iostream_access::AsOstream(prefixedName.get());
  ostream* plainOut = iostream_access::AsOstream(plainName.get());
  if (directory)
    ostream_operator_ls_cstr(ostream_operator_ls_cstr(prefixedOut, directory), "\\");
  ostream* afterFirst = ostream_operator_ls_cstr(plainOut, LoadRcdataString(firstId));
  ostream* afterSecond = ostream_operator_ls_cstr(afterFirst, LoadRcdataString(secondId));
  ostream_operator_ls_char(afterSecond, 0);
  char* fileName = strstreambuf_str(iostream_access::BufferOf(plainName.get()));
  OpenInputFile(file.get(), fileName);
  if (file.FileDescriptor() == -1)
  {
    ostream_operator_ls_cstr(prefixedOut, fileName);
    OpenInputFile(file.get(), strstreambuf_str(iostream_access::BufferOf(prefixedName.get())));
    strstreambuf_freeze(iostream_access::BufferOf(prefixedName.get()), 0);
  }
  strstreambuf_freeze(iostream_access::BufferOf(plainName.get()), 0);
  if (file.FileDescriptor() != -1)
    Read(file.get()->In());
  CloseFileStream(file.get());
  return 1;
}

// ---- TableFile / TableFileList -----------------------------------------------------------------------------

TableFile::TableFile()
{
  m_version = m_table.m_version;
}

TableFile::~TableFile() = default;

int TableFile::Load(uint16_t firstId, uint16_t secondId, const char* directory)
{
  return m_table.Open(firstId, secondId, directory);
}

TableFileList::TableFileList() : m_ready(1)
{
}

TableFileList::~TableFileList() = default;

// ---- GDataTables -------------------------------------------------------------------------------------------

GDataTables::GDataTables() : m_tables(nullptr)
{
  m_ready = CreateTableList();
}

GDataTables::~GDataTables()
{
  DeleteTableList();
}

void GDataTables::DeleteTableList()
{
  if (m_tables)
  {
    delete m_tables;
    m_tables = nullptr;
  }
}

int GDataTables::CreateTableList()
{
  DeleteTableList();
  m_tables = NewOrNull<TableFileList>();
  return m_tables ? m_tables->m_ready : 0;
}

uint32_t GDataTables::LoadTable(uint16_t firstId, uint16_t secondId, uint32_t tableId)
{
  TableFile* file = NewOrNull<TableFile>();
  if (!file)
    return 0;
  if (file->m_version == 0 || !file->Load(firstId, secondId, GetGameDataDirectory()))
  {
    delete file;
    return 0;
  }
  file->SetId(tableId);
  const uint32_t id = m_tables->AddWithId(file);
  if (id == 0)
    delete file;
  return id;
}

int GDataTables::RegisterAllTables()
{
  if (!CreateTableList())
    return 0;
  uint32_t result = 0;
  // The terminator keeps the compiler from forming a pointer just past the table, which the post-link vtable patcher
  // would mistake for the address of the next object.
  for (const TableFileSpec* spec = kTableFiles; spec->m_secondId != 0; ++spec)
  {
    result = LoadTable(kTableDirectoryStringId, spec->m_secondId, spec->m_tableId);
    if (!result)
      return 0;
  }
  return static_cast<int>(result);
}

int GDataTables::ChecksumAll(Crc32* crc)
{
  int result = 1;
  for (const auto& entry : m_tables->Entries())
  {
    if (!result)
      break;
    result = static_cast<TableFile*>(entry.get())->m_table.Checksum(crc);
  }
  return result;
}

TableEntry* GDataTables::Lookup(uint32_t tableId, int value)
{
  TableFile* file = static_cast<TableFile*>(m_tables->FindById(tableId));
  return file ? file->m_table.FindStepEntry(value) : nullptr;
}

GDataTables* GetGDataTables()
{
  return g_gdataTables;
}

void DestroyGDataTables()
{
  if (g_gdataTables)
  {
    delete g_gdataTables;
    g_gdataTables = nullptr;
  }
}

void InitGDataTables()
{
  DestroyGDataTables();
  g_gdataTables = NewOrNull<GDataTables>();
  if (g_gdataTables && g_gdataTables->m_ready && g_gdataTables->RegisterAllTables())
    return;
  DestroyGDataTables();
}

// ---- Table class registry ----------------------------------------------------------------------------------

int RegisterTableClass(const char* className, void* (*factory)())
{
  if (g_tableClassCount + 1 < kMaxTableClasses)
  {
    TableClassRegistration& entry = g_tableClasses[g_tableClassCount];
    entry.m_factory = factory;
    char* out = entry.m_name;
    char c = *className;
    *out = c;
    int remaining = sizeof(entry.m_name);
    while (c != '\0' && --remaining != 0)
    {
      c = *++className;
      *++out = c;
    }
    ++g_tableClassCount;
    entry.m_name[sizeof(entry.m_name) - 1] = '\0';
  }
  return 0;
}

void* CreateTable()
{
  return NewOrNull<Table>();
}

void InitTableClassRegistration()
{
  g_tableClassRegistered = RegisterTableClass("Table", CreateTable);
}

GDataTables* g_gdataTables{};
TableClassRegistration g_tableClasses[10]{};
int g_tableClassCount{};
int g_tableClassRegistered{};
