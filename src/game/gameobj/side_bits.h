#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "core/layout.h"
#include "core/stream_io.h"

class Crc32;

// Three 2-bit side fields (0 none, 1 Alliance, 2 Empire, 3 neutral/both) packed into one int, as the galaxy objects
// keep them (the object at +0x84 of the class at 0x507130). The default is 0xf: both low fields neutral, the third
// empty. The field names come from the description text "<controller> (L=<loyalty>,G=<garrison>)" and from how
// 0x50b610 derives them: the controller is the garrison side when a side has units there, else the loyalty.
class SideBits {
public:
    SideBits();                                // 0x558820 (the field reset is 0x5589d0)
    SideBits(const SideBits& other);           // 0x558840
    // Starts at the default and takes `value` only if both low fields are non-zero and nothing is set above bit 5.
    explicit SideBits(uint32_t value);         // 0x558880
    SideBits& operator=(const SideBits& other);  // 0x558860
    bool operator==(const SideBits& other) const;  // 0x558950

    void Reset();                              // 0x5589d0
    void Assign(const SideBits& other);        // 0x5589e0
    void SetController(uint32_t side);         // 0x5588f0, bits 0-1
    void SetLoyalty(uint32_t side);            // 0x558910, bits 2-3
    void SetGarrison(uint32_t side);           // 0x558930, bits 4-5
    // The shared "one 32-bit field" Read/Write/Checksum bodies: the linker folded them (ICF) with every other class
    // that streams a single int, so many unrelated callers reach these addresses.
    int Read(istream* in);                     // 0x558970
    int Write(ostream* out);                   // 0x558990
    int Checksum(Crc32* crc);                  // 0x5589b0
    // Appends "Neutral  (L=Alliance,G=Empire  )"-style text (side names are padded to 8 characters). Returns 1.
    int AppendDescription(String* out);        // 0x5589f0

    uint32_t Controller() const { return m_value & 3; }
    uint32_t Loyalty() const { return (m_value >> 2) & 3; }
    uint32_t Garrison() const { return (m_value >> 4) & 3; }

    uint32_t m_value;  // +0x00
};

// GNPRTB 0x1200/0x1201 (the "rules 12" block): fleet values read through a state bit (see 0x503140).
extern int g_gnprtb1200;  // 0x6bb38c
extern int g_gnprtb1201;  // 0x6bb390

// Binds both parameters for the current difficulty; every binder runs even after a failure. Returns 1 if both succeed.
int BindGnprtb12();  // 0x5587e0

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SideBits, 0x04);
#pragma clang diagnostic pop
