#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "tactical/tactical_units.h"

// FSIN on the x87 stack, as the original uses it (the C library call would give a different rounding).
inline double X87Sin(double x) {
    __asm__("fsin" : "+t"(x));
    return x;
}
inline double X87Sqrt(double x) {
    __asm__("fsqrt" : "+t"(x));
    return x;
}
inline double X87Cos(double x) {
    __asm__("fcos" : "+t"(x));
    return x;
}

// Classes of the tactical engine that live outside the unit modules (capital ship 0x5af4e0, fighter 0x5b8540, Death
// Star 0x5ba3a0) but that the unit classes are built from. Their owners (ship groups and AI / formations, visual
// resources) have not named or reconstructed them yet, so this header declares just what the unit classes need. It is a
// stand-in that can be dropped when the owning modules provide the real definitions.

// A 4-component vector whose last component caches the length (-1 while unknown); 0x59faf0 / 0x59fb10.
struct TacticalVector {
    float m_x;
    float m_y;
    float m_z;
    float m_cachedLength;

    TacticalVector() : m_x(0), m_y(0), m_z(0), m_cachedLength(-1.0f) {}
    TacticalVector(float x, float y, float z) : m_x(x), m_y(y), m_z(z), m_cachedLength(-1.0f) {}
};

// First base of every battle unit: its orientation and position frame (VT 0x66c648, 0xd4 bytes). The unit classes
// extend its vtable. Slots 3-6 are inherited unchanged by the capital ship and the fighter.
class TacticalShipObject {
public:
    TacticalShipObject();               // inlined into the unit constructors
    virtual ~TacticalShipObject() { REB_KEEP_VPTR_STORE(); }  // 0x5cd2c0 (scalar deleting destructor 0x5b2420)
    virtual double Turn(float step, float rate);   // 0x5cd460
    virtual void Advance(float milliseconds);     // 0x5cd640
    virtual int Virtual03();                      // 0x5cd6b0: returns 0
    virtual void Read(istream* in);               // 0x5cd000
    virtual void Write(ostream* out);             // 0x5cd0f0
    virtual void Checksum(Crc32* crc);            // 0x5cd1e0

    float m_unk04;                 // +0x04
    float m_unk08;                 // +0x08
    float m_unk0c;                 // +0x0c
    TacticalVector m_forward;      // +0x10 unit direction the ship faces (z=1 at creation)
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

inline TacticalShipObject::TacticalShipObject()
    : m_unk04(1.0f), m_forward(0, 0, 1.0f), m_unk50(0) {}

// The four small interface bases that follow TacticalUnit in a battle unit. Their destructors (scalar deleting) are
// 0x5a04d0 / 0x5a06a0 / 0x5a07d0 / 0x5a09a0; the third one also has read / write slots (0x5a0750 / 0x5a0770).
class TacticalShipFacetA {
public:
    TacticalShipFacetA() : m_fA_04(0), m_fA_08(0) {}
    virtual ~TacticalShipFacetA() { REB_KEEP_VPTR_STORE(); }
    int m_fA_04;  // +0x04
    int m_fA_08;  // +0x08
};

class TacticalShipFacetB {
public:
    TacticalShipFacetB() : m_fB_04(0) {}
    virtual ~TacticalShipFacetB() { REB_KEEP_VPTR_STORE(); }
    int m_fB_04;  // +0x04
};

class TacticalShipFacetC {
public:
    TacticalShipFacetC() : m_fC_04(0) {}
    virtual ~TacticalShipFacetC() { REB_KEEP_VPTR_STORE(); }
    virtual void Read(istream* in);    // 0x5a0750
    virtual void Write(ostream* out);  // 0x5a0770
    int m_fC_04;                       // +0x04
};

class TacticalShipFacetD {
public:
    TacticalShipFacetD() : m_fD_04(0), m_fD_08(0) {}
    virtual ~TacticalShipFacetD() { REB_KEEP_VPTR_STORE(); }
    int m_fD_04;  // +0x04
    int m_fD_08;  // +0x08
};

class TacticalShipBase;

// A node of a TacticalUnitTree: a small wrapper (0x20 bytes) holding a battle unit and its handle.
class TacticalUnitNode {
public:
    virtual void Slot00();
    virtual int GetHandle();                 // slot 1 (+0x04)
    virtual TacticalUnitNode* Next();        // slot 2 (+0x08)
    virtual void Slot03();
    virtual TacticalShipBase* Value();       // slot 4 (+0x10)
};

// An id-ordered tree of units (VT 0x66bdd0, 0xc bytes): IdAvlTree plus three accessors. It owns the wrapper nodes, not
// the units.
class TacticalUnitTree : public IdAvlTree {
public:
    TacticalUnitTree() {}
    virtual ~TacticalUnitTree() { REB_KEEP_VPTR_STORE(); }  // 0x5c5ff0 (scalar deleting destructor 0x5a0bd0)
    virtual int AddIfAbsent(TacticalUnitNode* node);  // 0x5c6000
    virtual TacticalUnitNode* FirstNode();            // 0x5f6590
    virtual int Virtual05();                          // 0x5c6030

    void Read(istream* in);                      // 0x5c5de0
    void Write(ostream* out);                    // 0x5c5e50
    void Checksum(Crc32* crc);                   // 0x5c5ed0
    int Add(TacticalShipBase* unit);             // 0x5c5f20 wraps the unit unless its handle is in the tree already
    int RemoveById(int handle);                  // 0x5c60d0
    int RemoveUnit(TacticalShipBase* unit);      // 0x5c5fb0
    TacticalUnitNode* Find(int handle);          // 0x5ca4b0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(TacticalVector, 0x10);
REB_ASSERT_SIZE(TacticalShipObject, 0xd4);
REB_ASSERT_OFFSET(TacticalShipObject, m_unkBc, 0xbc);
REB_ASSERT_SIZE(TacticalShipFacetA, 0x0c);
REB_ASSERT_SIZE(TacticalShipFacetB, 0x08);
REB_ASSERT_SIZE(TacticalShipFacetC, 0x08);
REB_ASSERT_SIZE(TacticalShipFacetD, 0x0c);
REB_ASSERT_SIZE(TacticalUnitTree, 0x0c);
#pragma clang diagnostic pop
