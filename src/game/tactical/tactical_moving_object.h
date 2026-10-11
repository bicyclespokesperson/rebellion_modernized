#pragma once
#include <cstdint>

#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "tactical/tactical_math.h"

// The movement state shared by everything that flies: the first base of every battle unit (TacticalShipBase) and of every
// formation (VT 0x66c648, 0xd4 bytes). Its orientation and position frame; the unit classes and formations override the
// slots. Slots 3-6 are inherited unchanged by the capital ship and the fighter.
class MovingObject {
public:
    MovingObject();           // 0x5ebc90 (inlined into the unit constructors)
    virtual ~MovingObject();  // 0x5cd2c0 (scalar deleting destructor 0x5b2420)
    virtual double Turn(float step, float rate);                // 0x5cd460
    virtual void Advance(float milliseconds);                   // 0x5cd640
    virtual int Virtual03(int a, int b, int c) { return 0; }    // 0x5cd6b0 (shared stub, pops 12 bytes)
    virtual void Read(istream* in);                             // 0x5cd000
    virtual void Write(ostream* out);                           // 0x5cd0f0
    virtual void Checksum(Crc32* crc);                          // 0x5cd1e0

    float m_scale;                 // +0x04 1.0 at creation
    float m_unk08;                 // +0x08
    float m_radius;                // +0x0c
    TacticalVector m_forward;      // +0x10 unit direction the ship faces (z=1 at creation); a formation's heading
    TacticalVector m_forwardGoal;  // +0x20
    TacticalVector m_position;     // +0x30
    TacticalVector m_velocity;     // +0x40
    float m_unk50;                 // +0x50
    uint8_t m_pad54[8];            // +0x54
    TacticalVector m_unk5c;        // +0x5c
    TacticalVector m_unk6c;        // +0x6c
    TacticalVector m_unk7c;        // +0x7c
    TacticalVector m_unk8c;        // +0x8c
    TacticalVector m_turnAxis;     // +0x9c
    TacticalVector m_unkAc;        // +0xac
    TacticalVector m_unkBc;        // +0xbc
    uint8_t m_padCc[8];            // +0xcc
};
