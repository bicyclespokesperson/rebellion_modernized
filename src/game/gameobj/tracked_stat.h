#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// A counted 8-byte value the object data blocks use for the numbers that change during play (GameObjData
// subclasses keep six of them). Every real change bumps m_changeCount, which the owners compare before and after a
// call to find out whether anything happened (0x53fa60/0x53fab0).
//
// m_packed: bit 0 flag, bits 1-15 progress (stored 0-0x3fff), bits 16-31 level (stored 0-0x7fff).
class TrackedStat {
public:
    TrackedStat();                           // 0x5401b0
    TrackedStat(const TrackedStat& other);   // 0x5401c0
    TrackedStat& operator=(const TrackedStat& other);  // 0x5401e0

    void Reset();                           // 0x540330
    void CopyFrom(const TrackedStat& other);  // 0x540340
    void SetFlag(uint32_t on);              // 0x540200
    void SetLevel(uint32_t level);          // 0x540230, clamps to 0x7fff; a negative level also clears the flag
    void SetProgress(uint32_t progress);    // 0x540270, clamps to 0x3fff
    int Read(istream* in);                  // 0x540310
    int Write(ostream* out);                // 0x540320
    // ICF-merged with every other class that checksums two 32-bit fields.
    int Checksum(Crc32* crc);               // 0x51c980
    int ReadFields(istream* in);            // 0x5402b0
    int WriteFields(ostream* out);          // 0x5402e0

    uint32_t Flag() const { return m_packed & 1; }
    uint32_t Progress() const { return (m_packed >> 1) & 0x7fff; }
    uint32_t Level() const { return m_packed >> 16; }

    int m_changeCount;   // +0x00
    uint32_t m_packed;   // +0x04
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TrackedStat, 0x08);
REB_ASSERT_OFFSET(TrackedStat, m_packed, 0x04);
#pragma clang diagnostic pop
