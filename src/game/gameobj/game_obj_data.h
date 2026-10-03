#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"
#include "gameobj/tracked_stat.h"

class Crc32;

// The per-object data block GameObj keeps behind m_data (+0x54) for master views (VT 0x662140, 0x18 bytes; ctor
// 0x553e90). GameObj::CreateData (slot 58) makes one; classes with more state to save derive from it (SystemData below,
// the pass-through UnitObjData at 0x5409d0, ...). Only master views own one, side views leave m_data null.
class GameObjData {
public:
    GameObjData();                          // 0x553e90
    virtual ~GameObjData();                 // 0x553ef0 (deleting), 0x553f10
    virtual int Read(istream* in);          // 0x553f60
    virtual int Write(ostream* out);        // 0x553fa0
    virtual int Checksum(Crc32* crc);       // 0x553fe0
    virtual int IsValid() { return 1; }     // slot 4, 0x40f340

    int m_field04;         // +0x04, plain int with a getter/setter pair on GameObj (0x4f6df0/0x4f7f60)
    int m_lockCount;       // +0x08, GameObj::Lock/Unlock count; the Locked state flag is "count != 0"
    ObjectKey m_key;       // +0x0c
    TrackedStat m_stat;    // +0x10
};

// The data of the galaxy object class at 0x507130 (the base of systems, VT 0x65e640; created by its CreateData through
// 0x512c90): six counted stats and 15 plain words, all saved and checksummed in field order. The word at +0x80 is a
// bit set (tests of bits 0 and 3 at 0x5092f0/0x509310). 0x84 bytes, VT 0x662760.
class SystemData : public GameObjData {
public:
    SystemData();                           // 0x55a100
    virtual ~SystemData();                  // 0x55a1c0 (deleting), 0x55a1e0
    virtual int Read(istream* in) override;       // 0x55a280
    virtual int Write(ostream* out) override;     // 0x55a3b0
    virtual int Checksum(Crc32* crc) override;    // 0x55a4e0
    // Slot 4 is 0x55a5f0, a jump to the shared "return 1" stub.

    TrackedStat m_stats[6];  // +0x18
    int m_word48[3];         // +0x48
    int m_word54[3];         // +0x54
    int m_word60;            // +0x60
    int m_word64;            // +0x64
    int m_word68[3];         // +0x68
    int m_word74;            // +0x74
    int m_word78;            // +0x78
    int m_word7c;            // +0x7c
    uint32_t m_flags80;      // +0x80
};

// Body of the galaxy class's CreateData (slot 58, 0x512c90, shared by the classes at 0x65e640/0x663698/0x664098): a new
// SystemData, or null if allocation fails. The class that implements the slot calls this (it has no symbol row here).
GameObjData* NewSystemData();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameObjData, 0x18);
REB_ASSERT_OFFSET(GameObjData, m_lockCount, 0x08);
REB_ASSERT_OFFSET(GameObjData, m_key, 0x0c);
REB_ASSERT_OFFSET(GameObjData, m_stat, 0x10);
REB_ASSERT_SIZE(SystemData, 0x84);
REB_ASSERT_OFFSET(SystemData, m_stats, 0x18);
REB_ASSERT_OFFSET(SystemData, m_word48, 0x48);
REB_ASSERT_OFFSET(SystemData, m_word54, 0x54);
REB_ASSERT_OFFSET(SystemData, m_word60, 0x60);
REB_ASSERT_OFFSET(SystemData, m_word68, 0x68);
REB_ASSERT_OFFSET(SystemData, m_flags80, 0x80);
#pragma clang diagnostic pop
