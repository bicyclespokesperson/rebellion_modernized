#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "core/stream_io.h"

class Crc32;

// Base of the entries of the parameter / lookup tables (GNPRTB, the *MSTB tables, the seed tables). Entries are
// created by class name through the table-entry class registry (see RegisterTableEntryClass) when a Table is
// loaded, then Read() from the table file. Each entry is an IdNode keyed by the id stored in the file.
//
// Write/Read/Checksum below are pure virtual with bodies that the concrete entries call first; ParseHeader is the
// text-format counterpart of the common part.
class TableEntry : public IdNode
{
public:
  TableEntry();
  ~TableEntry() override;
  virtual int Write(ostream* out) = 0;
  virtual int Read(istream* in) = 0;
  virtual int ParseText(istream* in) = 0;
  virtual int Checksum(Crc32* crc) = 0;
  virtual int FirstValue() const = 0;

  // Reads the next text field into +0x20; false if the stream has failed, the field is empty or reading it
  // failed. (The entry's id is not part of the text form.)
  int ParseHeader(istream* in, int* endOfLine);

  int m_field1c; // +0x1c, 1 after construction
  int m_field20; // +0x20
};

// The registry of entry classes by name (at most 9; names are truncated to 39 characters). Registering a name that
// is already registered, or beyond the limit, does nothing. Both lookups compare names case-sensitively.
// CreateTableEntryByClassName returns a new entry of the class, or null if the class is unknown or its factory
// failed.
void* CreateTableEntryByClassName(const char* className);
int RegisterTableEntryClass(const char* className, void* (*factory)());
int IsTableEntryClassRegistered(const char* className);
