#pragma once
#include <cstddef>
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"

class String;

// Secondary base of GameObj and the SD record classes (at +0x30 / +0x1c in them). Its single pure virtual
// appends the object's localized name to a String, with a prefix chosen by `kind` (1, 2, 4, 0x20, 0x21 ...);
// there is no virtual destructor.
class ObjectBase {
public:
    ObjectBase();
    ~ObjectBase();
    virtual void WriteName(String* out, int kind) = 0;
};

// A 32-bit value that saves and loads itself.
class PersistentInt {
public:
    explicit PersistentInt(int value);
    int Get() const;
    void WriteBytes(ostream* out);
    void Read(istream* in);
    void Write(ostream* out);

    int m_value;  // +0x00
};

void ReadPersistentInt(istream* in, PersistentInt* value);
void WritePersistentInt(ostream* out, PersistentInt* value);
void ReadValuePersistentInt(istream* in, PersistentInt* value);
void WriteValuePersistentInt(ostream* out, PersistentInt* value);

// Park-Miller style generator (multiplier 41358, modulus 2^31-1) used by the game's gameplay randomness.
// BUG(original): Seed() returns the requested seed, not the warmed-up state, and the constructors and Next()
// store that return value in m_seed, discarding the 16 warm-up steps.
class RandomGenerator {
public:
    RandomGenerator();
    explicit RandomGenerator(int seed);
    virtual ~RandomGenerator();

    int Seed(int seed);
    // Uniform-ish value in [0, limit); 0 unless limit > 1.
    int Next(int limit);
    int GenerateSeed();
    int Step(int state);
    void Read(istream* in);
    void Write(ostream* out);

    int m_seed;  // +0x04, -1 until seeded
};

// CRC-32 (polynomial 0x04c11db7, MSB first, no final xor other than the complement) of everything fed to it.
class Crc32 {
public:
    Crc32();
    Crc32(const Crc32& other);
    virtual ~Crc32();
    Crc32& operator=(const Crc32& other);

    void Reset();
    uint32_t Value() const;
    void Update(const uint8_t* data, int count);
    void AddByte(uint8_t value);
    void AddInt(int value);
    void AddShort(int16_t value);
    int LoadState(istream* in);
    int SaveState(ostream* out);
    void Read(istream* in);
    void Write(ostream* out);

    uint32_t m_state;  // +0x04
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectBase, 0x04);
REB_ASSERT_SIZE(PersistentInt, 0x04);
REB_ASSERT_SIZE(RandomGenerator, 0x08);
REB_ASSERT_OFFSET(RandomGenerator, m_seed, 0x04);
REB_ASSERT_SIZE(Crc32, 0x08);
REB_ASSERT_OFFSET(Crc32, m_state, 0x04);
#pragma clang diagnostic pop
