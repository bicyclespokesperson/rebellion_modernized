#pragma once
#include <cstddef>
#include <cstdint>

#include "core/game_string.h"
#include "core/stream_io.h"

// Secondary base of GameObj and the SD record classes (at +0x30 / +0x1c in them). Its single pure virtual
// appends the object's localized name to a String, with a prefix chosen by `kind` (1, 2, 4, 0x20, 0x21 ...);
// there is no virtual destructor.
class ObjectBase
{
public:
  ObjectBase();
  ~ObjectBase();
  virtual void WriteName(std::string* out, int kind) = 0;
};

// A 32-bit value that saves and loads itself.
class PersistentInt
{
public:
  explicit PersistentInt(int value);
  int Get() const;
  void WriteBytes(ostream* out);
  void Read(istream* in);
  void Write(ostream* out);

  int m_value; // +0x00
};

void ReadPersistentInt(istream* in, PersistentInt* value);
void WritePersistentInt(ostream* out, PersistentInt* value);
void ReadValuePersistentInt(istream* in, PersistentInt* value);
void WriteValuePersistentInt(ostream* out, PersistentInt* value);

// Park-Miller style generator (multiplier 41358, modulus 2^31-1).
class RandomGenerator
{
public:
  using result_type = uint32_t;
  static constexpr result_type min()
  {
    return 1;
  }
  static constexpr result_type max()
  {
    return 0x7ffffffe;
  }
  RandomGenerator();
  explicit RandomGenerator(int seed);
  virtual ~RandomGenerator();

  int Seed(int seed);
  // Uniform value in [0, limit); zero without consuming state when limit <= 1.
  int Next(int limit);
  // Uniform value between either pair of inclusive bounds, normalizing reversed bounds.
  int Between(int low, int high);
  result_type operator()();
  int GenerateSeed();
  int Step(int state);
  void Read(istream* in);
  void Write(ostream* out);

  int m_seed; // +0x04, -1 until seeded
};

// CRC-32 (polynomial 0x04c11db7, MSB first, no final xor other than the complement) of everything fed to it.
class Crc32
{
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

  uint32_t m_state; // +0x04
};
