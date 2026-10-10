#pragma once
// Events 0x303-0x31b that act on a systems (family 0x90-0x97) through the system handlers at 0x515c00-0x515f40.
#include <cstdint>

#include "events/event_base.h"


// system event: runs the system-side handler 0x515cd0 with the value at +0x50
class SystemEvent303 : public SourceEvent {
public:
    SystemEvent303();  // 0x5638e0
    SystemEvent303(const SystemEvent303& other);  // 0x563930
    virtual ~SystemEvent303();  // 0x563920 (deleting 0x563900)

    virtual SystemEvent303* Clone() override;  // 1: 0x563950
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x563a40
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x563ab0
    virtual int Execute() override;  // 8: 0x563b20
    virtual int GetKind() override { return 0x303; }  // 9: 0x5639c0
};

// system event: handler 0x515dc0
class SystemEvent304 : public SourceEvent {
public:
    SystemEvent304();  // 0x563b90
    SystemEvent304(const SystemEvent304& other);  // 0x563be0
    virtual ~SystemEvent304();  // 0x563bd0 (deleting 0x563bb0)

    virtual SystemEvent304* Clone() override;  // 1: 0x563c00
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x563cf0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x563d60
    virtual int Execute() override;  // 8: 0x563dd0
    virtual int GetKind() override { return 0x304; }  // 9: 0x563c70
};

// system event: handler 0x515f40
class SystemEvent305 : public SourceEvent {
public:
    SystemEvent305();  // 0x563e40
    SystemEvent305(const SystemEvent305& other);  // 0x563e90
    virtual ~SystemEvent305();  // 0x563e80 (deleting 0x563e60)

    virtual SystemEvent305* Clone() override;  // 1: 0x563eb0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x563f90
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x564000
    virtual int Execute() override;  // 8: 0x564070
    virtual int GetKind() override { return 0x305; }  // 9: 0x43ef00
};

// system event: handler 0x515e30
class SystemEvent309 : public SourceEvent {
public:
    SystemEvent309();  // 0x5640e0
    SystemEvent309(const SystemEvent309& other);  // 0x564130
    virtual ~SystemEvent309();  // 0x564120 (deleting 0x564100)

    virtual SystemEvent309* Clone() override;  // 1: 0x564150
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x564240
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x5642b0
    virtual int Execute() override;  // 8: 0x564320
    virtual int GetKind() override { return 0x309; }  // 9: 0x5641c0
};

// system event: handler 0x515e70
class SystemEvent310 : public SourceEvent {
public:
    SystemEvent310();  // 0x564390
    SystemEvent310(const SystemEvent310& other);  // 0x5643e0
    virtual ~SystemEvent310();  // 0x5643d0 (deleting 0x5643b0)

    virtual SystemEvent310* Clone() override;  // 1: 0x564400
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x5644f0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x564560
    virtual int Execute() override;  // 8: 0x5645d0
    virtual int GetKind() override { return 0x310; }  // 9: 0x564470
};

// system event: handler 0x515eb0
class SystemEvent30f : public SourceEvent {
public:
    SystemEvent30f();  // 0x564640
    SystemEvent30f(const SystemEvent30f& other);  // 0x564690
    virtual ~SystemEvent30f();  // 0x564680 (deleting 0x564660)

    virtual SystemEvent30f* Clone() override;  // 1: 0x5646b0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x5647a0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x564810
    virtual int Execute() override;  // 8: 0x564880
    virtual int GetKind() override { return 0x30f; }  // 9: 0x564720
};

// system event: handler 0x515c00 with the two side-bit sets at +0x4c/+0x50
class SystemEvent31b : public SourceEvent {
public:
    SystemEvent31b();  // 0x5648f0
    SystemEvent31b(const SystemEvent31b& other);  // 0x564940
    virtual ~SystemEvent31b();  // 0x564930 (deleting 0x564910)

    virtual SystemEvent31b* Clone() override;  // 1: 0x564960
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x564a50
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x564ac0
    virtual int Execute() override;  // 8: 0x564b30
    virtual int GetKind() override { return 0x31b; }  // 9: 0x5649d0
};

// system event: handler 0x515ef0; sorted at priority 0x400
class SystemEvent311 : public SourceEvent {
public:
    SystemEvent311();  // 0x564bf0
    SystemEvent311(const SystemEvent311& other);  // 0x564c40
    virtual ~SystemEvent311();  // 0x564c30 (deleting 0x564c10)

    virtual SystemEvent311* Clone() override;  // 1: 0x564c60
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x564d60
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x564dd0
    virtual int Execute() override;  // 8: 0x564e40
    virtual int GetKind() override { return 0x311; }  // 9: 0x564cd0
    virtual int GetPriority() override { return 0x400; }  // 10: 0x48e670
};

// system event: marks the system as changed for both sides (SyncViewForEvent kind 0xb); priority 0x801
class SystemEvent319 : public SourceEvent {
public:
    SystemEvent319();  // 0x564eb0
    SystemEvent319(const SystemEvent319& other);  // 0x564f00
    virtual ~SystemEvent319();  // 0x564ef0 (deleting 0x564ed0)

    virtual SystemEvent319* Clone() override;  // 1: 0x564f20
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x565010
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x565080
    virtual int Execute() override;  // 8: 0x5650f0
    virtual int GetKind() override { return 0x319; }  // 9: 0x564f90
    virtual int GetPriority() override { return 0x801; }  // 10: 0x5651c0
    ObjectKey* GetKey(ObjectKey* out);  // 0x5651a0: copies the event key
};

// Factories registered in the event factory table.
SystemEvent303* CreateSystemEvent303();  // 0x5639d0
SystemEvent304* CreateSystemEvent304();  // 0x563c80
SystemEvent305* CreateSystemEvent305();  // 0x563f20
SystemEvent309* CreateSystemEvent309();  // 0x5641d0
SystemEvent310* CreateSystemEvent310();  // 0x564480
SystemEvent30f* CreateSystemEvent30f();  // 0x564730
SystemEvent31b* CreateSystemEvent31b();  // 0x5649e0
SystemEvent311* CreateSystemEvent311();  // 0x564ce0
SystemEvent319* CreateSystemEvent319();  // 0x564fa0

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SystemEvent303, 0x54);
REB_ASSERT_SIZE(SystemEvent304, 0x54);
REB_ASSERT_SIZE(SystemEvent305, 0x54);
REB_ASSERT_SIZE(SystemEvent309, 0x54);
REB_ASSERT_SIZE(SystemEvent310, 0x54);
REB_ASSERT_SIZE(SystemEvent30f, 0x54);
REB_ASSERT_SIZE(SystemEvent31b, 0x54);
REB_ASSERT_SIZE(SystemEvent311, 0x54);
REB_ASSERT_SIZE(SystemEvent319, 0x54);
#pragma clang diagnostic pop
