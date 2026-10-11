#include "data/sd_record_bases.h"


namespace {

template <size_t N>
void readValues(istream* in, uint32_t (&values)[N]) {
    for (uint32_t& value : values) ReadValue32(in, &value);
}

template <size_t N>
void writeValues(ostream* out, uint32_t (&values)[N]) {
    for (uint32_t& value : values) WriteValue32(out, &value);
}

template <size_t N>
void addValuesToChecksum(Crc32* crc, const uint32_t (&values)[N]) {
    for (uint32_t value : values) crc->AddInt(value);
}

template <size_t N>
void parseValues(istream* in, strstream* token, int* endOfLine, uint32_t (&values)[N]) {
    for (uint32_t& value : values) ParseLongField(in, token, endOfLine, &value);
}

}  // namespace

// ---- SdRecordExt2 ------------------------------------------------------------------------------------------

SdRecordExt2::SdRecordExt2() : m_field58(0), m_field5c(0) {}

SdRecordExt2::~SdRecordExt2() = default;

int SdRecordExt2::Read(istream* in) {
    SdRecord::Read(in);
    ReadValue32(in, &m_field58);
    ReadValue32(in, &m_field5c);
    return 1;
}

int SdRecordExt2::Write(ostream* out) {
    SdRecord::Write(out);
    WriteValue32(out, &m_field58);
    WriteValue32(out, &m_field5c);
    return 1;
}

int SdRecordExt2::Checksum(Crc32* crc) {
    SdRecord::Checksum(crc);
    crc->AddInt(m_field58);
    crc->AddInt(m_field5c);
    return 1;
}

int SdRecordExt2::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    SdRecord::ParseFields(in, endOfLine);
    ParseLongField(in, token.get(), endOfLine, &m_field58);
    ParseLongField(in, token.get(), endOfLine, &m_field5c);
    return 1;
}

// ---- SdRecordGrid ------------------------------------------------------------------------------------------

SdRecordGrid::SdRecordGrid()
    : m_field60(0), m_field64(0), m_field68(0), m_field6c(0), m_field70(0), m_grid{}, m_fieldA4{},
      m_columnTotals{}, m_total(0) {}

SdRecordGrid::~SdRecordGrid() = default;

uint32_t SdRecordGrid::GetGridValue(int row, int column) const {
    if (row < 0 || row >= kGridRows || column < 0 || column >= kGridColumns) return 0;
    return m_grid[row][column];
}

uint32_t SdRecordGrid::GetFieldA4(int index) const {
    if (index < 0 || index >= kGridColumns) return 0;
    return m_fieldA4[index];
}

uint32_t SdRecordGrid::GetColumnTotal(int index) const {
    if (index < 0 || index >= kGridColumns) return 0;
    return m_columnTotals[index];
}

int SdRecordGrid::Read(istream* in) {
    SdRecordExt2::Read(in);
    ReadValue32(in, &m_field60);
    ReadValue32(in, &m_field64);
    ReadValue32(in, &m_field68);
    ReadValue32(in, &m_field6c);
    ReadValue32(in, &m_field70);
    for (auto& row : m_grid) readValues(in, row);
    readValues(in, m_fieldA4);
    readValues(in, m_columnTotals);
    ReadValue32(in, &m_total);
    return 1;
}

int SdRecordGrid::Write(ostream* out) {
    SdRecordExt2::Write(out);
    WriteValue32(out, &m_field60);
    WriteValue32(out, &m_field64);
    WriteValue32(out, &m_field68);
    WriteValue32(out, &m_field6c);
    WriteValue32(out, &m_field70);
    for (auto& row : m_grid) writeValues(out, row);
    writeValues(out, m_fieldA4);
    writeValues(out, m_columnTotals);
    WriteValue32(out, &m_total);
    return 1;
}

int SdRecordGrid::Checksum(Crc32* crc) {
    SdRecordExt2::Checksum(crc);
    crc->AddInt(m_field60);
    crc->AddInt(m_field64);
    crc->AddInt(m_field68);
    crc->AddInt(m_field6c);
    crc->AddInt(m_field70);
    for (const auto& row : m_grid) addValuesToChecksum(crc, row);
    addValuesToChecksum(crc, m_fieldA4);
    addValuesToChecksum(crc, m_columnTotals);
    crc->AddInt(m_total);
    return 1;
}

int SdRecordGrid::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    SdRecordExt2::ParseFields(in, endOfLine);
    ParseLongField(in, token.get(), endOfLine, &m_field60);
    ParseLongField(in, token.get(), endOfLine, &m_field64);
    ParseLongField(in, token.get(), endOfLine, &m_field68);
    ParseLongField(in, token.get(), endOfLine, &m_field6c);
    ParseLongField(in, token.get(), endOfLine, &m_field70);
    for (auto& row : m_grid) parseValues(in, token.get(), endOfLine, row);
    parseValues(in, token.get(), endOfLine, m_fieldA4);
    for (uint32_t& total : m_columnTotals) total = 0;
    m_total = 0;
    for (const auto& row : m_grid)
        for (int column = 0; column < kGridColumns; ++column) m_columnTotals[column] += row[column];
    for (const auto& row : m_grid)
        for (uint32_t value : row) m_total += value;
    return 1;
}

// ---- SdRecordExt16 -----------------------------------------------------------------------------------------

SdRecordExt16::SdRecordExt16() : m_fields{} {}

SdRecordExt16::~SdRecordExt16() = default;

namespace {

// Pair members are stored interleaved: index 0 at +0x68, 1 at +0x78, 2 at +0x70 (and +4 for the second member).
int pairFieldOffset(int index) {
    switch (index) {
        case 0: return 0x68;
        case 1: return 0x78;
        case 2: return 0x70;
        default: return -1;
    }
}

}  // namespace

int SdRecordExt16::GetPairFirst(int index, uint32_t* value) const {
    *value = 0;
    const int offset = pairFieldOffset(index);
    if (offset < 0) return 0;
    *value = m_fields[(offset - 0x58) / 4];
    return 1;
}

int SdRecordExt16::GetPairSecond(int index, uint32_t* value) const {
    *value = 0;
    const int offset = pairFieldOffset(index);
    if (offset < 0) return 0;
    *value = m_fields[(offset + 4 - 0x58) / 4];
    return 1;
}

int SdRecordExt16::Read(istream* in) {
    SdRecord::Read(in);
    readValues(in, m_fields);
    return 1;
}

int SdRecordExt16::Write(ostream* out) {
    SdRecord::Write(out);
    writeValues(out, m_fields);
    return 1;
}

int SdRecordExt16::Checksum(Crc32* crc) {
    SdRecord::Checksum(crc);
    addValuesToChecksum(crc, m_fields);
    return 1;
}

int SdRecordExt16::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    SdRecord::ParseFields(in, endOfLine);
    parseValues(in, token.get(), endOfLine, m_fields);
    return 1;
}

// ---- SdRecordExt25 -----------------------------------------------------------------------------------------

SdRecordExt25::SdRecordExt25() : m_moreFields{} {}

SdRecordExt25::~SdRecordExt25() = default;

int SdRecordExt25::Read(istream* in) {
    SdRecordExt16::Read(in);
    readValues(in, m_moreFields);
    return 1;
}

int SdRecordExt25::Write(ostream* out) {
    SdRecordExt16::Write(out);
    writeValues(out, m_moreFields);
    return 1;
}

int SdRecordExt25::Checksum(Crc32* crc) {
    SdRecordExt16::Checksum(crc);
    addValuesToChecksum(crc, m_moreFields);
    return 1;
}

int SdRecordExt25::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    SdRecordExt16::ParseFields(in, endOfLine);
    parseValues(in, token.get(), endOfLine, m_moreFields);
    return 1;
}

// ---- SdRecordExt1 ------------------------------------------------------------------------------------------

SdRecordExt1::SdRecordExt1() : m_field58(0) {}

SdRecordExt1::~SdRecordExt1() = default;

int SdRecordExt1::Read(istream* in) {
    SdRecord::Read(in);
    ReadValue32(in, &m_field58);
    return 1;
}

int SdRecordExt1::Write(ostream* out) {
    SdRecord::Write(out);
    WriteValue32(out, &m_field58);
    return 1;
}

int SdRecordExt1::Checksum(Crc32* crc) {
    SdRecord::Checksum(crc);
    crc->AddInt(m_field58);
    return 1;
}

int SdRecordExt1::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    SdRecord::ParseFields(in, endOfLine);
    ParseLongField(in, token.get(), endOfLine, &m_field58);
    return 1;
}
