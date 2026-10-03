#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// Localized string resource id as the game passes it to the string loader: the String(int) constructor and
// LoadStringResource receive the *address* of one of these.
struct ResourceStringId {
    int16_t m_id;
    int16_t m_group;
};

// Interface at offset 0x1c of every game object and data record (the second base after IdNode / GameObj):
// one virtual that appends the object's display name to a String. `kind` selects the form: 1, 2 and 4 append
// the plain name, 0x20 and 0x21 prefix an indefinite article ("a" / "an" resource strings); other kinds append
// nothing.
class NameSource {
public:
    virtual void AppendName(String* out, int kind) = 0;
};

// Base of the records loaded from the GData/*.DAT tables (the "SD" tables). Each record is an IdNode (the key
// is the node id) with a name and a few common numbers. Records are read from the binary table files with
// Read(), saved with Write(), summed into the multiplayer-sync checksum with Checksum() and parsed from the
// comma-separated text form with ParseFields(); the concrete record classes override those four and call these
// base implementations first.
class DatRecord : public IdNode, public NameSource {
public:
    DatRecord();
    virtual ~DatRecord();
    virtual int Write(ostream* out) = 0;
    virtual int Read(istream* in) = 0;
    // Reads one text line: the first field through ParseFields(), then discards the rest of the line. Returns
    // ParseFields()' result, or 0 if the stream is exhausted.
    virtual int Parse(istream* in);
    virtual int Checksum(Crc32* crc) = 0;
    virtual int ParseFields(istream* in, int* endOfLine) = 0;
    virtual void AppendName(String* out, int kind) override;

    int m_hasData;     // +0x20, uninitialised by the constructor; set to 1 by ParseFields
    uint32_t m_first;  // +0x24
    uint32_t m_second; // +0x28
    uint32_t m_third;  // +0x2c
    uint16_t m_small;  // +0x30
    uint16_t m_small2; // +0x32
    String m_name;     // +0x34, "<not set>" until set
};

// Record base with six more numbers. 0x48 and 0x4c have getters.
class SdRecord : public DatRecord {
public:
    SdRecord();
    virtual ~SdRecord();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    uint32_t GetField48() const;
    uint32_t GetField4c() const;

    uint32_t m_field40;  // +0x40
    uint32_t m_field44;  // +0x44
    uint32_t m_field48;  // +0x48
    uint32_t m_field4c;  // +0x4c
    uint32_t m_field50;  // +0x50
    uint32_t m_field54;  // +0x54
};

// Entries of the record-factory registry: records are created from the (low, high) pair registered for their
// class (a range of object types the SD tables use).
struct RecordFactoryEntry {
    int m_low;
    int m_high;
    void* (*m_factory)();
};

void* CreateRecordForKey(int low, int high);
int RegisterRecordFactory(const int* key, void* (*factory)());

// Text table parsing (comma separated, fields optionally "quoted"; a doubled quote inside a quoted field stands
// for itself). Reads the next field of the current line into `token`'s buffer as a NUL-terminated string and
// leaves *endOfLine non-zero once the line (or the stream) has ended. Returns `token`.
strstream* ReadCsvField(istream* in, strstream* token, int* endOfLine);
// Reads the next field and parses it into *value (istream::operator>>(long&), unsigned long& and unsigned short&
// on the field's text).
inline void ParseLongField(istream* in, strstream* token, int* endOfLine, void* value) {
    istream_operator_rs_long(iostream_access::AsIstream(ReadCsvField(in, token, endOfLine)), static_cast<int*>(value));
}
inline void ParseUnsignedField(istream* in, strstream* token, int* endOfLine, void* value) {
    istream_operator_rs_ulong(iostream_access::AsIstream(ReadCsvField(in, token, endOfLine)),
                              static_cast<unsigned int*>(value));
}
inline void ParseUnsignedShortField(istream* in, strstream* token, int* endOfLine, void* value) {
    istream_operator_rs_ushort(iostream_access::AsIstream(ReadCsvField(in, token, endOfLine)),
                               static_cast<unsigned short*>(value));
}
// Discards the rest of the current line.
int SkipRestOfLine(istream* in, int* endOfLine);
// Discards lines until the first field of a line starts with "****", then the rest of that line.
int SkipTableHeader(istream* in);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(DatRecord, 0x40);
REB_ASSERT_OFFSET(DatRecord, m_hasData, 0x20);
REB_ASSERT_OFFSET(DatRecord, m_first, 0x24);
REB_ASSERT_OFFSET(DatRecord, m_third, 0x2c);
REB_ASSERT_OFFSET(DatRecord, m_small, 0x30);
REB_ASSERT_OFFSET(DatRecord, m_name, 0x34);
REB_ASSERT_SIZE(SdRecord, 0x58);
REB_ASSERT_OFFSET(SdRecord, m_field40, 0x40);
REB_ASSERT_OFFSET(SdRecord, m_field54, 0x54);
#pragma clang diagnostic pop
