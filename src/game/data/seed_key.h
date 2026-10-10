#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// Five-field key that identifies a seeded game object (mission seeds, manufacturing seeds, ...). Field 0 is a
// game object key (family in the top byte, id in the low 24 bits). Plain data with a lexicographic order.
class SeedKey {
public:
    SeedKey();
    SeedKey(const SeedKey& other);
    SeedKey& operator=(const SeedKey& other);

    int operator==(const SeedKey& other) const;
    // Lexicographic over (u32, u32, i32, u32, i32).
    int operator<(const SeedKey& other) const;

    int Read(istream* in);
    int Write(ostream* out) const;
    int AddToChecksum(Crc32* crc) const;
    // Thin forwarding wrappers the original instantiates for the table entry classes.
    int ReadValue(istream* in);
    int WriteValue(ostream* out);

    uint32_t m_key;     // +0x00
    uint32_t m_second;  // +0x04
    int32_t m_third;    // +0x08
    uint32_t m_fourth;  // +0x0c
    int32_t m_fifth;    // +0x10
};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SeedKey, 0x14);
REB_ASSERT_OFFSET(SeedKey, m_fifth, 0x10);
#pragma clang diagnostic pop
