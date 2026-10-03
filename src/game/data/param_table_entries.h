#pragma once
#include <cstdint>

#include "data/table_entry.h"

// Entry of GNPRTB.DAT: eight general game parameters.
class GeneralParamTableEntry : public TableEntry {
public:
    static constexpr int kValueCount = 8;

    GeneralParamTableEntry();
    virtual ~GeneralParamTableEntry();
    virtual int Write(ostream* out) override;
    virtual int Read(istream* in) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc) override;

    // 0 for an index outside 0..7.
    int GetValue(int index) const;

    int m_values[kValueCount];  // +0x24
};

// Entry of SDPRTB.DAT: eight (first, second) parameter pairs.
class SideParamTableEntry : public TableEntry {
public:
    static constexpr int kPairCount = 8;

    SideParamTableEntry();
    virtual ~SideParamTableEntry();
    virtual int Write(ostream* out) override;
    virtual int Read(istream* in) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc) override;

    // `member` is 1 (first) or 2 (second); 0 for any other member or an index outside 0..7.
    int GetValue(int pair, int member) const;

    int m_values[kPairCount][2];  // +0x24
};

// Factories registered under the entry class names.
void* CreateGeneralParamTableEntry();
void* CreateSideParamTableEntry();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GeneralParamTableEntry, 0x44);
REB_ASSERT_OFFSET(GeneralParamTableEntry, m_values, 0x24);
REB_ASSERT_SIZE(SideParamTableEntry, 0x64);
REB_ASSERT_OFFSET(SideParamTableEntry, m_values, 0x24);
#pragma clang diagnostic pop
