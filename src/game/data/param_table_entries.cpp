#include "data/param_table_entries.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"

namespace {

void readValues(istream* in, int* values, size_t count) {
    for (size_t i = 0; i < count; ++i) ReadValue32(in, &values[i]);
}

void writeValues(ostream* out, const int* values, size_t count) {
    for (size_t i = 0; i < count; ++i) WriteValue32(out, &values[i]);
}

int checksumValues(Crc32* crc, TableEntry* entry, const int* values, size_t count) {
    const int result = entry->TableEntry::Checksum(crc);
    for (size_t i = 0; i < count; ++i) crc->AddInt(values[i]);
    return result;
}

int parseTextValues(TableEntry* entry, istream* in, int* values, size_t count) {
    ScopedStrstream token;
    int endOfLine = 0;
    ReadCsvField(in, token.get(), &endOfLine);
    const int ok = entry->ParseHeader(in, &endOfLine);
    if (ok) {
        for (size_t i = 0; i < count; ++i) ParseLongField(in, token.get(), &endOfLine, &values[i]);
    }
    SkipRestOfLine(in, &endOfLine);
    return ok;
}

}  // namespace

// ---- GeneralParamTableEntry --------------------------------------------------------------------------------

GeneralParamTableEntry::GeneralParamTableEntry() : m_values{} {}

GeneralParamTableEntry::~GeneralParamTableEntry() { REB_KEEP_VPTR_STORE(); }

int GeneralParamTableEntry::Read(istream* in) {
    TableEntry::Read(in);
    readValues(in, m_values, kValueCount);
    return 1;
}

int GeneralParamTableEntry::Write(ostream* out) {
    TableEntry::Write(out);
    writeValues(out, m_values, kValueCount);
    return 1;
}

int GeneralParamTableEntry::Checksum(Crc32* crc) { return checksumValues(crc, this, m_values, kValueCount); }

int GeneralParamTableEntry::ParseText(istream* in) { return parseTextValues(this, in, m_values, kValueCount); }

int GeneralParamTableEntry::GetValue(int index) const {
    if (index < 0 || index >= kValueCount) return 0;
    return m_values[index];
}

void* CreateGeneralParamTableEntry() { return NewOrNull<GeneralParamTableEntry>(); }

// ---- SideParamTableEntry -----------------------------------------------------------------------------------

SideParamTableEntry::SideParamTableEntry() : m_values{} {}

SideParamTableEntry::~SideParamTableEntry() { REB_KEEP_VPTR_STORE(); }

int SideParamTableEntry::Read(istream* in) {
    TableEntry::Read(in);
    readValues(in, &m_values[0][0], kValueCount);
    return 1;
}

int SideParamTableEntry::Write(ostream* out) {
    TableEntry::Write(out);
    writeValues(out, &m_values[0][0], kValueCount);
    return 1;
}

int SideParamTableEntry::Checksum(Crc32* crc) { return checksumValues(crc, this, &m_values[0][0], kValueCount); }

int SideParamTableEntry::ParseText(istream* in) { return parseTextValues(this, in, &m_values[0][0], kValueCount); }

int SideParamTableEntry::GetValue(int pair, int member) const {
    if (pair < 0 || pair >= kPairCount || member < 1 || member > 2) return 0;
    return m_values[pair][member - 1];
}

void* CreateSideParamTableEntry() { return NewOrNull<SideParamTableEntry>(); }
