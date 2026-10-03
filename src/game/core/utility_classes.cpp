#include <ctime>
#include "core/utility_classes.h"
#include "core/vptr_store.h"

extern uint32_t g_crcTable[256];
extern int g_crcTableReady;

// The game's statically linked C runtime (the UCRT headers would declare these as DLL imports).
extern "C" {
int __cdecl rand(void);
void __cdecl srand(unsigned int seed);
}

// ---- ObjectBase --------------------------------------------------------------------------------------------

ObjectBase::ObjectBase() {}

ObjectBase::~ObjectBase() { REB_KEEP_VPTR_STORE(); }

// ---- PersistentInt -----------------------------------------------------------------------------------------

PersistentInt::PersistentInt(int value) : m_value(value) {}

int PersistentInt::Get() const { return m_value; }

void PersistentInt::WriteBytes(ostream* out) { StreamWrite32(out, this); }

void PersistentInt::Read(istream* in) { StreamRead32(in, this); }

void PersistentInt::Write(ostream* out) { WriteBytes(out); }

void ReadPersistentInt(istream* in, PersistentInt* value) { value->Read(in); }

void WritePersistentInt(ostream* out, PersistentInt* value) { value->WriteBytes(out); }

void ReadValuePersistentInt(istream* in, PersistentInt* value) { ReadPersistentInt(in, value); }

void WriteValuePersistentInt(ostream* out, PersistentInt* value) { WritePersistentInt(out, value); }

// ---- RandomGenerator ---------------------------------------------------------------------------------------

RandomGenerator::RandomGenerator() : m_seed(-1) {}

RandomGenerator::RandomGenerator(int seed) { m_seed = Seed(seed); }

RandomGenerator::~RandomGenerator() { REB_KEEP_VPTR_STORE(); }

int RandomGenerator::Seed(int seed) {
    m_seed = -1;
    if (seed < 1 || seed > 0x7ffffffe) seed = GenerateSeed();
    m_seed = seed;
    for (int round = 0; round < 16; ++round) m_seed = Step(m_seed);
    return seed;
}

int RandomGenerator::Next(int limit) {
    if (limit <= 1) return 0;
    if (m_seed == -1) m_seed = Seed(-1);
    const int state = Step(m_seed);
    m_seed = state;
    return state / (0x7ffffffe / limit);
}

int RandomGenerator::GenerateSeed() {
    srand(static_cast<unsigned>(time(nullptr)));
    int seed;
    do {
        const int a = rand();
        const int b = rand();
        const int c = rand();
        const int d = rand();
        seed = a * b + c * d;
    } while (seed > 0x7ffffffe);
    return seed;
}

int RandomGenerator::Step(int state) {
    constexpr uint32_t kMultiplier = 0xa18e;
    constexpr int kQuotient = 0xcad4;
    constexpr int kModulus = 0x7fffffff;
    int next = static_cast<int>(static_cast<uint32_t>(state) * kMultiplier -
                                static_cast<uint32_t>(state / kQuotient) * static_cast<uint32_t>(kModulus));
    if (next < 0) next += kModulus;
    return next;
}

void RandomGenerator::Read(istream* in) { ReadValue32(in, &m_seed); }

void RandomGenerator::Write(ostream* out) { WriteValue32(out, &m_seed); }

// ---- Crc32 -------------------------------------------------------------------------------------------------

Crc32::Crc32() { Reset(); }

Crc32::Crc32(const Crc32& other) : m_state(other.m_state) {}

Crc32::~Crc32() { REB_KEEP_VPTR_STORE(); }

Crc32& Crc32::operator=(const Crc32& other) {
    m_state = other.m_state;
    return *this;
}

void Crc32::Reset() { m_state = 0xffffffff; }

uint32_t Crc32::Value() const { return ~m_state; }

__attribute__((noinline)) void BuildCrcTable() {
    for (uint32_t index = 0; index < 256; ++index) {
        uint32_t value = index << 24;
        for (int bit = 0; bit < 8; ++bit) value = (value & 0x80000000) ? (value * 2) ^ 0x04c11db7 : value << 1;
        g_crcTable[index] = value;
    }
    g_crcTableReady = 1;
}

void Crc32::Update(const uint8_t* data, int count) {
    if (!g_crcTableReady) BuildCrcTable();
    for (; count != 0; --count) m_state = g_crcTable[*data++ ^ (m_state >> 24)] ^ (m_state << 8);
}

void Crc32::AddByte(uint8_t value) { Update(&value, 1); }

void Crc32::AddInt(int value) { Update(reinterpret_cast<const uint8_t*>(&value), 4); }

void Crc32::AddShort(int16_t value) { Update(reinterpret_cast<const uint8_t*>(&value), 2); }

int Crc32::LoadState(istream* in) {
    StreamRead32(in, &m_state);
    return 1;
}

int Crc32::SaveState(ostream* out) {
    StreamWrite32(out, &m_state);
    return 1;
}

void Crc32::Read(istream* in) { LoadState(in); }

void Crc32::Write(ostream* out) { SaveState(out); }
