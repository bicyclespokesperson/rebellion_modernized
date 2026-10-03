#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// Base of the entries of the parameter / lookup tables (GNPRTB, the *MSTB tables, the seed tables). Entries are
// created by class name through the table-entry class registry (see param_table_entries.h and
// RegisterTableEntryClasses) when a Table is loaded, then Read() from the table file.
//
// Owned by the SD record classes module (0x58e570-0x5923a0); declared here with the members the entry classes in
// this module need. Read/Write/Checksum below are pure virtual with bodies (0x58f970, 0x58f9b0, 0x58f9f0) that
// the concrete entries call first; ParseHeader (0x58fa30) is the text-format counterpart.
class TableEntry : public IdNode {
public:
    TableEntry();
    virtual ~TableEntry();
    virtual int Write(ostream* out) = 0;
    virtual int Read(istream* in) = 0;
    virtual int ParseText(istream* in) = 0;
    virtual int Checksum(Crc32* crc) = 0;

    // Reads the entry's leading text fields (id and +0x20); false if the line is empty or the stream failed.
    bool ParseHeader(istream* in, int* endOfLine);

    int m_field1c;  // +0x1c, 1 after construction
    int m_field20;  // +0x20
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TableEntry, 0x24);
REB_ASSERT_OFFSET(TableEntry, m_field1c, 0x1c);
REB_ASSERT_OFFSET(TableEntry, m_field20, 0x20);
#pragma clang diagnostic pop
