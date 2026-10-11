#pragma once
#include "core/id_registry.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <ranges>

#include "core/entry_identity.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "data/table_entry.h"

class Crc32;

// A serialized object that starts with a 32-bit version number (+4, 1 for a new object). Read/Write/ParseText are
// supplied by the concrete table class; the ...Version helpers handle the version field they have in common (the
// machine code of ReadVersion is shared with an unrelated class by the linker).
class TableBase
{
public:
  TableBase();
  ~TableBase();
  virtual int Read(istream* in) = 0;
  virtual int Write(ostream* out) = 0;
  virtual int ParseText(istream* in) = 0;

  int ReadVersion(istream* in);
  int WriteVersion(ostream* out);
  int ChecksumVersion(Crc32* crc);
  // Reads the table from the file `<resource string firstId><resource string secondId>`, trying it relative to
  // the current directory first and then under `directory`. Returns 1 even if no file could be opened.
  int Open(uint16_t firstId, uint16_t secondId, const char* directory);

  int m_version; // +0x04
};

// One game data table (a GData/*.DAT file): a version, the class name of its entries, and the entries, which are
// TableEntry objects keyed by their ids. Entries are created by name through the table-entry class registry.
class Table : public TableBase
{
public:
  static constexpr int kNameSize = 0x28;

  Table();
  ~Table();
  int Read(istream* in) override;
  int Write(ostream* out) override;
  int ParseText(istream* in) override;
  virtual int Checksum(Crc32* crc);

  void Add(TableEntry* entry);
  uint32_t AddEntry(TableEntry* entry);
  // The entry with the largest threshold (TableEntry::m_field20) not above `value`; the first entry if every
  // threshold is above it, the last entry if none is.
  TableEntry* FindStepEntry(int value);

  char m_name[kNameSize]; // +0x08, entry class name; uninitialised beyond its terminator
  IdRegistry<TableEntry> m_entries; // +0x30
};

// A Table loaded under a table id. m_version mirrors the table's version when the object is built.
class TableFile : public IdNode
{
public:
  TableFile();
  ~TableFile() override;

  int Load(uint16_t firstId, uint16_t secondId, const char* directory);

  int m_version; // +0x1c
  Table m_table; // +0x20
};

// All loaded tables, keyed by table id.
class TableFileList
{
public:
  void Clear()
  {
    m_registry.Clear();
  }
  size_t Count() const
  {
    return m_registry.Count();
  }
  bool Empty() const
  {
    return m_registry.Empty();
  }
  auto Entries() const
  {
    return m_registry.Entries();
  }
  auto PositionOf(const TableFile* node) const
  {
    return m_registry.PositionOf(node);
  }
  TableFile* First() const
  {
    return m_registry.First();
  }
  TableFile* Last() const
  {
    return m_registry.Last();
  }
  TableFile* FindById(uint32_t id) const
  {
    return m_registry.FindById(id);
  }
  TableFile* DetachById(uint32_t id)
  {
    return m_registry.DetachById(id);
  }
  void DeleteById(uint32_t id)
  {
    m_registry.DeleteById(id);
  }
  void Remove(TableFile* node)
  {
    m_registry.Remove(node);
  }
  uint32_t AddWithId(TableFile* node)
  {
    return m_registry.AddWithId(node);
  }
  TableFileList();
  ~TableFileList();

  int m_ready; // +0x0c, 1
private:
  IdRegistry<TableFile> m_registry;
};

// The registry of every GData table of the game.
class GDataTables
{
public:
  GDataTables();
  virtual ~GDataTables();

  int CreateTableList();
  void DeleteTableList();
  // Loads all 33 tables; returns 0 as soon as one id cannot be added.
  int RegisterAllTables();
  uint32_t LoadTable(uint16_t firstId, uint16_t secondId, uint32_t tableId);
  int ChecksumAll(Crc32* crc);
  TableEntry* Lookup(uint32_t tableId, int value);

  int m_ready; // +0x04, the list's m_ready
  TableFileList* m_tables; // +0x08
};

GDataTables* GetGDataTables();
void InitGDataTables();
void DestroyGDataTables();

// Registry of table classes by name (the "Table" class registers itself at start-up).
int RegisterTableClass(const char* className, void* (*factory)());
void* CreateTable();
void InitTableClassRegistration();
