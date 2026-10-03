#include "data/dat_record.h"

#include <cstring>

#include "core/utility_classes.h"
#include "core/vptr_store.h"

extern int g_recordFactoryCount;
extern RecordFactoryEntry g_recordFactories[];

namespace {

constexpr int kMaxRecordFactories = 0x3c;
constexpr int16_t kIndefiniteArticleA = 0x4da1;
constexpr int16_t kIndefiniteArticleAn = 0x4da2;
constexpr int16_t kArticleGroup = 2;

int ResourceArgument(const ResourceStringId* id) { return static_cast<int>(reinterpret_cast<uintptr_t>(id)); }

void appendWithArticle(String* out, const String& name) {
    const ResourceStringId article = {name.StartsWithNonVowel() == 0 ? kIndefiniteArticleAn : kIndefiniteArticleA,
                                      kArticleGroup};
    {
        String articleText(ResourceArgument(&article));
        *out += articleText;
    }
    *out += name;
}

}  // namespace

// ---- DatRecord ---------------------------------------------------------------------------------------------

DatRecord::DatRecord() : m_first(0), m_second(0), m_third(0), m_small(0), m_small2(0), m_name("<not set>") {}

DatRecord::~DatRecord() { REB_KEEP_VPTR_STORE(); }

void DatRecord::AppendName(String* out, int kind) {
    switch (kind) {
        case 1:
        case 2:
        case 4:
            *out += m_name;
            break;
        case 0x20:
        case 0x21: {
            const String name(m_name);
            appendWithArticle(out, name);
            break;
        }
        default:
            break;
    }
}

// The vtable slot at +0x1c is entered with `this` pointing at the NameSource base.
void __thiscall AppendRecordName(NameSource* source, String* out, int kind) {
    static_cast<DatRecord*>(source)->AppendName(out, kind);
}

int DatRecord::Parse(istream* in) {
    ScopedStrstream token;
    int endOfLine = 0;
    int ok = 1;
    if (iostream_access::State(in) != 0) ok = 0;
    if (ok) {
        ReadCsvField(in, token.get(), &endOfLine);
        if (iostream_access::AvailableBytes(iostream_access::BufferOf(token.get())) == 0) ok = 0;
        if (iostream_access::State(in) != 0) ok = 0;
        if (ok) ok = ParseFields(in, &endOfLine);
    }
    while (endOfLine == 0) ReadCsvField(in, token.get(), &endOfLine);
    return ok;
}

int DatRecord::Read(istream* in) {
    ReadId(in);
    ReadValue32(in, &m_hasData);
    uint32_t first;
    uint32_t second;
    ReadValue32(in, &first);
    ReadValue32(in, &second);
    ReadValue32(in, &m_third);
    ReadValue16(in, &m_small);
    ReadValue16(in, &m_small2);
    m_first = first;
    m_second = second;
    return 1;
}

int DatRecord::Write(ostream* out) {
    WriteId(out);
    WriteValue32(out, &m_hasData);
    uint32_t first = m_first;
    WriteValue32(out, &first);
    uint32_t second = m_second;
    WriteValue32(out, &second);
    WriteValue32(out, &m_third);
    WriteValue16(out, &m_small);
    WriteValue16(out, &m_small2);
    return 1;
}

int DatRecord::Checksum(Crc32* crc) {
    crc->AddInt(m_id);
    crc->AddInt(m_hasData);
    crc->AddInt(m_first);
    crc->AddInt(m_second);
    crc->AddInt(m_third);
    crc->AddShort(m_small);
    crc->AddShort(m_small2);
    return 1;
}

int DatRecord::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    unsigned int id = 0;
    m_hasData = 1;
    ParseUnsignedField(in, token.get(), endOfLine, &id);
    SetId(id);
    ParseUnsignedField(in, token.get(), endOfLine, &m_third);
    ParseUnsignedShortField(in, token.get(), endOfLine, &m_small);
    return 1;
}

// ---- SdRecord ----------------------------------------------------------------------------------------------

SdRecord::SdRecord() : m_field40(0), m_field44(0), m_field48(0), m_field4c(0), m_field50(0), m_field54(0) {}

SdRecord::~SdRecord() { REB_KEEP_VPTR_STORE(); }

uint32_t SdRecord::GetField48() const { return m_field48; }

uint32_t SdRecord::GetField4c() const { return m_field4c; }

int SdRecord::Read(istream* in) {
    DatRecord::Read(in);
    ReadValue32(in, &m_field40);
    ReadValue32(in, &m_field44);
    ReadValue32(in, &m_field48);
    ReadValue32(in, &m_field4c);
    ReadValue32(in, &m_field50);
    ReadValue32(in, &m_field54);
    return 1;
}

int SdRecord::Write(ostream* out) {
    DatRecord::Write(out);
    WriteValue32(out, &m_field40);
    WriteValue32(out, &m_field44);
    WriteValue32(out, &m_field48);
    WriteValue32(out, &m_field4c);
    WriteValue32(out, &m_field50);
    WriteValue32(out, &m_field54);
    return 1;
}

int SdRecord::Checksum(Crc32* crc) {
    DatRecord::Checksum(crc);
    crc->AddInt(m_field40);
    crc->AddInt(m_field44);
    crc->AddInt(m_field48);
    crc->AddInt(m_field4c);
    crc->AddInt(m_field50);
    crc->AddInt(m_field54);
    return 1;
}

int SdRecord::ParseFields(istream* in, int* endOfLine) {
    ScopedStrstream token;
    DatRecord::ParseFields(in, endOfLine);
    ParseLongField(in, token.get(), endOfLine, &m_field40);
    ParseLongField(in, token.get(), endOfLine, &m_field44);
    ParseLongField(in, token.get(), endOfLine, &m_field48);
    ParseLongField(in, token.get(), endOfLine, &m_field4c);
    ParseLongField(in, token.get(), endOfLine, &m_field50);
    ParseLongField(in, token.get(), endOfLine, &m_field54);
    return 1;
}

// ---- Record factory registry -------------------------------------------------------------------------------

void* CreateRecordForKey(int low, int high) {
    void* (*factory)() = nullptr;
    for (int i = 0; i < g_recordFactoryCount; ++i) {
        const RecordFactoryEntry& entry = g_recordFactories[i];
        if (entry.m_low == low && entry.m_high == high) factory = entry.m_factory;
        if (factory) break;
    }
    return factory ? factory() : nullptr;
}

int RegisterRecordFactory(const int* key, void* (*factory)()) {
    bool found = false;
    for (int i = 0; i < g_recordFactoryCount && !found; ++i)
        found = g_recordFactories[i].m_low == key[0] && g_recordFactories[i].m_high == key[1];
    if (!found && static_cast<unsigned>(g_recordFactoryCount) + 1 < kMaxRecordFactories + 1u) {
        RecordFactoryEntry& entry = g_recordFactories[g_recordFactoryCount];
        entry.m_low = key[0];
        entry.m_high = key[1];
        entry.m_factory = factory;
        ++g_recordFactoryCount;
    }
    return 0;
}

// ---- Text table parsing ------------------------------------------------------------------------------------

strstream* ReadCsvField(istream* in, strstream* token, int* endOfLine) {
    ios* tokenIos = iostream_access::IosOf(token);
    const int initialEndOfLine = *endOfLine;
    iostream_access::Lock(tokenIos);
    const int lockCount = tokenIos->m_lockCount;
    tokenIos->m_state = 0;
    if (lockCount < 0) mtx_unlock(tokenIos->m_lock);
    ostream_seekp_pos(iostream_access::AsOstream(token), 0);
    istream_seekg_pos(iostream_access::AsIstream(token), 0);

    bool atFieldStart = true;
    bool quoted = false;
    bool closingQuoteSeen = false;
    bool fieldEnded = false;
    char previous = 0;
    if (initialEndOfLine == 0) {
        bool stop;
        do {
            const char current = static_cast<char>(istream_get(in));
            const bool ended = iostream_access::State(in) != 0 || current == '\n';
            *endOfLine = ended;
            bool emit = true;
            if (!ended) {
                if (atFieldStart) {
                    quoted = current == '"';
                    emit = !quoted;
                    atFieldStart = false;
                    fieldEnded = current == ',';
                } else if (quoted) {
                    if (current == '"') {
                        if (previous == '"') {
                            closingQuoteSeen = false;
                        } else {
                            emit = false;
                            closingQuoteSeen = true;
                        }
                    } else if (previous == '"' && closingQuoteSeen) {
                        fieldEnded = true;
                    }
                } else {
                    fieldEnded = current == ',';
                }
            }
            stop = ended || fieldEnded;
            if (!stop) {
                if (emit) ostream_operator_ls_char(iostream_access::AsOstream(token), current);
                previous = current;
            }
        } while (!stop);
    }
    ostream_operator_ls_char(iostream_access::AsOstream(token), 0);
    return token;
}

int SkipRestOfLine(istream* in, int* endOfLine) {
    ScopedStrstream token;
    while (iostream_access::State(in) == 0 && *endOfLine == 0) ReadCsvField(in, token.get(), endOfLine);
    *endOfLine = 0;
    return 1;
}

int SkipTableHeader(istream* in) {
    ScopedStrstream token;
    int endOfLine = 0;
    char marker[8];
    marker[0] = '\0';
    while (iostream_access::State(in) == 0 && std::strcmp(marker, "****") != 0) {
        strstream* field = ReadCsvField(in, token.get(), &endOfLine);
        ios* fieldIos = iostream_access::IosOf(field);
        iostream_access::Lock(fieldIos);
        iostream_access::GetlineCount(field)++;
        istream_getline(iostream_access::AsIstream(field), marker, 5, 0);
        iostream_access::Unlock(fieldIos);
        endOfLine = 0;
    }
    endOfLine = 0;
    SkipRestOfLine(in, &endOfLine);
    return 1;
}
