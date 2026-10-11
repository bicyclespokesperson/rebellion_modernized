#pragma once
#include <cstdint>

#include "data/dat_record.h"

// Intermediate bases of the concrete SD record classes (SystemSD, CapitalShipSD, ...): each adds a block of
// 32-bit table columns to SdRecord and provides its Read/Write/Checksum/ParseFields as non-virtual base
// implementations (the vtable slots stay pure; the concrete record classes override them and call these first).

// SdRecord plus two numbers.
class SdRecordExt2 : public SdRecord {
public:
    SdRecordExt2();
    virtual ~SdRecordExt2();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    uint32_t m_field58;  // +0x58
    uint32_t m_field5c;  // +0x5c
};

// SdRecordExt2 plus a 4x3 grid of counts with the per-column totals (m_columnTotals) and the grand total
// (m_total) of the grid.
class SdRecordGrid : public SdRecordExt2 {
public:
    static constexpr int kGridRows = 4;
    static constexpr int kGridColumns = 3;

    SdRecordGrid();
    virtual ~SdRecordGrid();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    // 0 for an out-of-range index.
    uint32_t GetGridValue(int row, int column) const;
    uint32_t GetFieldA4(int index) const;
    uint32_t GetColumnTotal(int index) const;

    uint32_t m_field60;                                // +0x60
    uint32_t m_field64;                                // +0x64
    uint32_t m_field68;                                // +0x68
    uint32_t m_field6c;                                // +0x6c
    uint32_t m_field70;                                // +0x70
    uint32_t m_grid[kGridRows][kGridColumns];          // +0x74
    uint32_t m_fieldA4[kGridColumns];                  // +0xa4
    uint32_t m_columnTotals[kGridColumns];             // +0xb0, accumulated (not reset) by ParseFields
    uint32_t m_total;                                  // +0xbc, accumulated by ParseFields
};

// SdRecord plus sixteen numbers. Fields 0x68..0x7c form three (first, second) pairs.
class SdRecordExt16 : public SdRecord {
public:
    SdRecordExt16();
    virtual ~SdRecordExt16();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    // Fetch the first/second member of pair `index` (0, 1 or 2 map to +0x68/+0x78/+0x70 and +0x6c/+0x7c/+0x74);
    // for any other index *value is set to 0 and 0 is returned.
    int GetPairFirst(int index, uint32_t* value) const;
    int GetPairSecond(int index, uint32_t* value) const;

    uint32_t m_fields[16];  // +0x58
};

// SdRecordExt16 plus nine numbers.
class SdRecordExt25 : public SdRecordExt16 {
public:
    SdRecordExt25();
    virtual ~SdRecordExt25();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    uint32_t m_moreFields[9];  // +0x98
};

// SdRecord plus one number.
class SdRecordExt1 : public SdRecord {
public:
    SdRecordExt1();
    virtual ~SdRecordExt1();
    virtual int Write(ostream* out) override = 0;
    virtual int Read(istream* in) override = 0;
    virtual int Checksum(Crc32* crc) override = 0;
    virtual int ParseFields(istream* in, int* endOfLine) override = 0;

    uint32_t m_field58;  // +0x58
};
