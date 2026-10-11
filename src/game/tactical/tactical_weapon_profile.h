#pragma once
#include <cstdint>

#include "core/stream_io.h"
#include "core/utility_classes.h"

// How well one weapon kind hurts a target class (visual-resources module, 0x5d2360-0x5d2570; 0x14 bytes): a damage factor
// and one accuracy factor per target class. A weapon arc owns one profile per weapon kind; the kinds differ only in the
// constants their constructor sets. The vtables have one slot, the scalar deleting destructor.
class WeaponProfile {
public:
    WeaponProfile();           // 0x5d2360
    virtual ~WeaponProfile();  // 0x5d23a0 (scalar deleting destructor 0x5d2380)

    void Read(istream* in);     // 0x5d23b0
    void Write(ostream* out);   // 0x5d2400
    void Checksum(Crc32* crc);  // 0x5d2450

    float m_f04;  // +0x04 damage factor (0.2667)
    float m_f08;  // +0x08 factor against fighters
    float m_f0c;  // +0x0c factor against capital ships
    float m_f10;  // +0x10
};

class WeaponProfileA : public WeaponProfile {  // VT 0x66d140
public:
    WeaponProfileA();           // 0x5d2530
    ~WeaponProfileA() override;  // 0x5d2570 (scalar deleting destructor 0x5d2550)
};

class WeaponProfileB : public WeaponProfile {  // VT 0x66d13c
public:
    WeaponProfileB();           // 0x5d24e0
    ~WeaponProfileB() override;  // 0x5d2520 (scalar deleting destructor 0x5d2500)
};

class WeaponProfileC : public WeaponProfile {  // VT 0x66d138
public:
    WeaponProfileC();           // 0x5d2490
    ~WeaponProfileC() override;  // 0x5d24d0 (scalar deleting destructor 0x5d24b0)
};
