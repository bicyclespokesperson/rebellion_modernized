#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/object_key.h"

class Crc32;

// The per-object data block GameObj keeps behind m_data (+0x54) for master views (VT 0x662140, 0x18 bytes; ctor
// 0x553e90). Subclasses (VT 0x662760 ...) extend it for particular object families. Skeleton: the owner of the
// 0x553e90-0x554020 functions fills in the rest.
class GameObjData {
public:
    GameObjData();
    virtual ~GameObjData();
    virtual int Read(istream* in);
    virtual int Write(ostream* out);
    virtual int Checksum(Crc32* crc);
    virtual int IsValid() { return 1; }

    int m_field04;         // +0x04
    int m_lockCount;       // +0x08, GameObj::Lock/Unlock count; the Locked state flag is "count != 0"
    ObjectKey m_key;       // +0x0c
    uint8_t m_unk10[8];    // +0x10
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameObjData, 0x18);
REB_ASSERT_OFFSET(GameObjData, m_lockCount, 0x08);
REB_ASSERT_OFFSET(GameObjData, m_key, 0x0c);
#pragma clang diagnostic pop
