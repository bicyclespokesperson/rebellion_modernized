#include "core/utility_classes.h"

#include "core/runtime_environment.h"

#include <algorithm>
#include <random>

#include "iostream/iostream.h"

extern uint32_t g_crcTable[256];
extern int g_crcTableReady;

// ---- ObjectBase --------------------------------------------------------------------------------------------

ObjectBase::ObjectBase() = default;

ObjectBase::~ObjectBase() = default;

// ---- PersistentInt -----------------------------------------------------------------------------------------

PersistentInt::PersistentInt(int value)
    : m_value(value)
{
}

int PersistentInt::Get() const
{
  return m_value;
}

void PersistentInt::WriteBytes(ostream* out)
{
  StreamWrite32(out, this);
}

void PersistentInt::Read(istream* in)
{
  StreamRead32(in, this);
}

void PersistentInt::Write(ostream* out)
{
  WriteBytes(out);
}

void ReadPersistentInt(istream* in, PersistentInt* value)
{
  value->Read(in);
}

void WritePersistentInt(ostream* out, PersistentInt* value)
{
  value->WriteBytes(out);
}

void ReadValuePersistentInt(istream* in, PersistentInt* value)
{
  ReadPersistentInt(in, value);
}

void WriteValuePersistentInt(ostream* out, PersistentInt* value)
{
  WritePersistentInt(out, value);
}

// ---- RandomGenerator ---------------------------------------------------------------------------------------

RandomGenerator::RandomGenerator()
    : m_seed(-1)
{
}

RandomGenerator::RandomGenerator(int seed)
{
  Seed(seed);
}

RandomGenerator::~RandomGenerator() = default;

int RandomGenerator::Seed(int seed)
{
  m_seed = -1;
  if (seed < 1 || seed > 0x7ffffffe) {
    seed = GenerateSeed();
  }
  m_seed = seed;
  for (int round = 0; round < 16; ++round) {
    m_seed = Step(m_seed);
  }
  return seed;
}

int RandomGenerator::Next(int limit)
{
  if (limit <= 1) {
    return 0;
  }
  return std::uniform_int_distribution<int>(0, limit - 1)(*this);
}

int RandomGenerator::Between(int low, int high)
{
  if (low > high) {
    std::swap(low, high);
  }
  if (low == high) {
    return low;
  }
  return std::uniform_int_distribution<int>(low, high)(*this);
}

RandomGenerator::result_type RandomGenerator::operator()()
{
  if (m_seed == -1) {
    Seed(-1);
  }
  const int state = Step(m_seed);
  m_seed = state;
  return static_cast<result_type>(state);
}

int RandomGenerator::GenerateSeed()
{
  auto state = static_cast<uint32_t>(GameSeedSeconds());
  const auto draw = [&state]() {
    state = state * 214013u + 2531011u;
    return static_cast<int>((state >> 16) & 0x7fff);
  };
  int seed;
  do {
    const int a = draw();
    const int b = draw();
    const int c = draw();
    const int d = draw();
    seed = a * b + c * d;
  } while (seed < 1 || seed > 0x7ffffffe);
  return seed;
}

int RandomGenerator::Step(int state)
{
  if (state < 1 || state > static_cast<int>(max())) {
    return 0;
  }
  constexpr uint64_t kModulus = 0x7fffffff;
  constexpr uint64_t kMultiplier = 0xa18e;
  return static_cast<int>((static_cast<uint64_t>(state) * kMultiplier) % kModulus);
}

void RandomGenerator::Read(istream* in)
{
  int seed = 0;
  ReadValue32(in, &seed);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0) {
    return;
  }
  if (seed != -1 && (seed < 1 || seed > static_cast<int>(max()))) {
    iostream_access::IosOf(in)->m_state |= iostream_flags::kFail;
    return;
  }
  m_seed = seed;
}

void RandomGenerator::Write(ostream* out)
{
  WriteValue32(out, &m_seed);
}

// ---- Crc32 -------------------------------------------------------------------------------------------------

Crc32::Crc32()
{
  Reset();
}

Crc32::Crc32(const Crc32& other) = default;

Crc32::~Crc32() = default;

Crc32& Crc32::operator=(const Crc32& other) = default;

void Crc32::Reset()
{
  m_state = 0xffffffff;
}

uint32_t Crc32::Value() const
{
  return ~m_state;
}

__attribute__((noinline)) void BuildCrcTable()
{
  for (uint32_t index = 0; index < 256; ++index) {
    uint32_t value = index << 24;
    for (int bit = 0; bit < 8; ++bit) {
      value = (value & 0x80000000) ? (value * 2) ^ 0x04c11db7 : value << 1;
    }
    g_crcTable[index] = value;
  }
  g_crcTableReady = 1;
}

void Crc32::Update(const uint8_t* data, int count)
{
  if (!g_crcTableReady) {
    BuildCrcTable();
  }
  for (; count != 0; --count) {
    m_state = g_crcTable[*data++ ^ (m_state >> 24)] ^ (m_state << 8);
  }
}

void Crc32::AddByte(uint8_t value)
{
  Update(&value, 1);
}

void Crc32::AddInt(int value)
{
  Update(reinterpret_cast<const uint8_t*>(&value), 4);
}

void Crc32::AddShort(int16_t value)
{
  Update(reinterpret_cast<const uint8_t*>(&value), 2);
}

int Crc32::LoadState(istream* in)
{
  StreamRead32(in, &m_state);
  return 1;
}

int Crc32::SaveState(ostream* out)
{
  StreamWrite32(out, &m_state);
  return 1;
}

void Crc32::Read(istream* in)
{
  LoadState(in);
}

void Crc32::Write(ostream* out)
{
  SaveState(out);
}

int g_crcTableReady{};
uint32_t g_crcTable[256]{};
