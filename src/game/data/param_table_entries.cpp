#include "data/param_table_entries.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"

namespace {

template <size_t N>
void readValues(istream* in, int (&values)[N]) {
    for (int& value : values) ReadValue32(in, &value);
}

template <size_t N>
void writeValues(ostream* out, int (&values)[N]) {
    for (int& value : values) WriteValue32(out, &value);
}

template <size_t N>
int checksumValues(Crc32* crc, TableEntry* entry, const int (&values)[N]) {
    const int result = entry->TableEntry::Checksum(crc);
    for (int value : values) crc->AddInt(value);
    return result;
}

template <size_t N>
int parseTextValues(TableEntry* entry, istream* in, int (&values)[N]) {
    ScopedStrstream token;
    int endOfLine = 0;
    ReadCsvField(in, token.get(), &endOfLine);
    const int ok = entry->ParseHeader(in, &endOfLine);
    if (ok) {
        for (int& value : values) ParseLongField(in, token.get(), &endOfLine, &value);
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
    readValues(in, m_values);
    return 1;
}

int GeneralParamTableEntry::Write(ostream* out) {
    TableEntry::Write(out);
    writeValues(out, m_values);
    return 1;
}

int GeneralParamTableEntry::Checksum(Crc32* crc) { return checksumValues(crc, this, m_values); }

int GeneralParamTableEntry::ParseText(istream* in) { return parseTextValues(this, in, m_values); }

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
    for (auto& pair : m_values) readValues(in, pair);
    return 1;
}

int SideParamTableEntry::Write(ostream* out) {
    TableEntry::Write(out);
    for (auto& pair : m_values) writeValues(out, pair);
    return 1;
}

int SideParamTableEntry::Checksum(Crc32* crc) {
    const int result = TableEntry::Checksum(crc);
    for (const auto& pair : m_values)
        for (int value : pair) crc->AddInt(value);
    return result;
}

int SideParamTableEntry::ParseText(istream* in) {
    ScopedStrstream token;
    int endOfLine = 0;
    ReadCsvField(in, token.get(), &endOfLine);
    const int ok = ParseHeader(in, &endOfLine);
    if (ok) {
        for (auto& pair : m_values)
            for (int& value : pair) ParseLongField(in, token.get(), &endOfLine, &value);
    }
    SkipRestOfLine(in, &endOfLine);
    return ok;
}

int SideParamTableEntry::GetValue(int pair, int member) const {
    if (pair < 0 || pair >= kPairCount || member < 1 || member > 2) return 0;
    return m_values[pair][member - 1];
}

void* CreateSideParamTableEntry() { return NewOrNull<SideParamTableEntry>(); }
