#pragma once
// The events 0x300-0x320 on the SourceEvent / TwoKeyEvent bases (0x5863b0 / 0x5864e0) that hand their keys and values to a handler of another module: the plain forwarders of the world simulation.
#include <cstdint>

#include "events/event_base.h"


// calls FUN_00514510(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*)
class Event301 : public SourceEvent {
public:
    Event301();  // 0x579fa0
    Event301(const Event301& other);  // 0x579ff0
    virtual ~Event301();  // 0x579fe0 (deleting 0x579fc0)

    virtual Event301* Clone() override;  // 1: 0x57a010
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57a100
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57a170
    virtual int Execute() override;  // 8: 0x57a1e0
    virtual int GetKind() override { return 0x301; }  // 9: 0x57a080
};

// calls FUN_00514820(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*)
class Event31d : public SourceEvent {
public:
    Event31d();  // 0x57a250
    Event31d(const Event31d& other);  // 0x57a2a0
    virtual ~Event31d();  // 0x57a290 (deleting 0x57a270)

    virtual Event31d* Clone() override;  // 1: 0x57a2c0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57a3b0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57a420
    virtual int Execute() override;  // 8: 0x57a490
    virtual int GetKind() override { return 0x31d; }  // 9: 0x57a330
};

// calls FUN_005140c0(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*)
class Event300 : public SourceEvent {
public:
    Event300();  // 0x57a500
    Event300(const Event300& other);  // 0x57a550
    virtual ~Event300();  // 0x57a540 (deleting 0x57a520)

    virtual Event300* Clone() override;  // 1: 0x57a570
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57a650
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57a6c0
    virtual int Execute() override;  // 8: 0x57a730
    virtual int GetKind() override { return 0x300; }  // 9: 0x57b260
};

// calls FUN_005150c0(ObjectKey*, int, NotifyContext*)
class Event308 : public SourceEvent {
public:
    Event308();  // 0x57a7a0
    Event308(const Event308& other);  // 0x57a7f0
    virtual ~Event308();  // 0x57a7e0 (deleting 0x57a7c0)

    virtual Event308* Clone() override;  // 1: 0x57a810
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57a900
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57a970
    virtual int Execute() override;  // 8: 0x57a9e0
    virtual int GetKind() override { return 0x308; }  // 9: 0x57a880
};

// calls FUN_00515a50(ObjectKey*, int, NotifyContext*)
class Event306 : public SourceEvent {
public:
    Event306();  // 0x57aa50
    Event306(const Event306& other);  // 0x57aaa0
    virtual ~Event306();  // 0x57aa90 (deleting 0x57aa70)

    virtual Event306* Clone() override;  // 1: 0x57aac0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57abb0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57ac20
    virtual int Execute() override;  // 8: 0x57ac90
    virtual int GetKind() override { return 0x306; }  // 9: 0x57ab30
};

// calls FUN_00515bb0(ObjectKey*, int, NotifyContext*)
class Event316 : public SourceEvent {
public:
    Event316();  // 0x57ad00
    Event316(const Event316& other);  // 0x57ad50
    virtual ~Event316();  // 0x57ad40 (deleting 0x57ad20)

    virtual Event316* Clone() override;  // 1: 0x57ad70
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57ae60
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57aed0
    virtual int Execute() override;  // 8: 0x57af40
    virtual int GetKind() override { return 0x316; }  // 9: 0x57ade0
};

// calls FUN_00515480(ObjectKey*, int, NotifyContext*)
class Event307 : public SourceEvent {
public:
    Event307();  // 0x57afb0
    Event307(const Event307& other);  // 0x57b000
    virtual ~Event307();  // 0x57aff0 (deleting 0x57afd0)

    virtual Event307* Clone() override;  // 1: 0x57b020
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57b110
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57b180
    virtual int Execute() override;  // 8: 0x57b1f0
    virtual int GetKind() override { return 0x307; }  // 9: 0x57b090
    virtual int GetPriority() override { return 0x300; }  // 10: 0x57b260
};

// calls FUN_00514890(ObjectKey*, int, int, NotifyContext*)
class Event302 : public SourceEvent {
public:
    Event302();  // 0x57b270
    Event302(const Event302& other);  // 0x57b2c0
    virtual ~Event302();  // 0x57b2b0 (deleting 0x57b290)

    virtual Event302* Clone() override;  // 1: 0x57b2e0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57b3d0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57b440
    virtual int Execute() override;  // 8: 0x57b4b0
    virtual int GetKind() override { return 0x302; }  // 9: 0x57b350
};

// calls FUN_00514a20(ObjectKey*, int, int, NotifyContext*)
class Event318 : public SourceEvent {
public:
    Event318();  // 0x57b520
    Event318(const Event318& other);  // 0x57b570
    virtual ~Event318();  // 0x57b560 (deleting 0x57b540)

    virtual Event318* Clone() override;  // 1: 0x57b590
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57b680
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57b6f0
    virtual int Execute() override;  // 8: 0x57b760
    virtual int GetKind() override { return 0x318; }  // 9: 0x57b600
    virtual int GetPriority() override { return 0x800; }  // 10: 0x57b7d0
};

// calls FUN_00516360(ObjectKey*, int, int, NotifyContext*)
class Event315 : public SourceEvent {
public:
    Event315();  // 0x578800
    Event315(const Event315& other);  // 0x578850
    virtual ~Event315();  // 0x578840 (deleting 0x578820)

    virtual Event315* Clone() override;  // 1: 0x578870
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x578960
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x5789d0
    virtual int Execute() override;  // 8: 0x578a40
    virtual int GetKind() override { return 0x315; }  // 9: 0x5788e0
};

// calls FUN_00516220(ObjectKey*, int, int, NotifyContext*)
class Event30a : public SourceEvent {
public:
    Event30a();  // 0x57c7c0
    Event30a(const Event30a& other);  // 0x57c810
    virtual ~Event30a();  // 0x57c800 (deleting 0x57c7e0)

    virtual Event30a* Clone() override;  // 1: 0x57c830
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57c920
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57c990
    virtual int Execute() override;  // 8: 0x57ca00
    virtual int GetKind() override { return 0x30a; }  // 9: 0x57c8a0
};

// two-key event: calls FUN_00514a60(ObjectKey*, ObjectKey*, ObjectKey*, NotifyContext*)
class Event320 : public TwoKeyEvent {
public:
    Event320();  // 0x57b7e0
    Event320(const Event320& other);  // 0x57b830
    virtual ~Event320();  // 0x57b820 (deleting 0x57b800)

    virtual Event320* Clone() override;  // 1: 0x57b850
    virtual void Read(istream* in) override { ReadTwoKey(in); }  // 2: 0x57b940
    virtual void Write(ostream* out) override { WriteTwoKey(out); }  // 3: 0x57b950
    virtual void Checksum(Crc32* crc) override { ChecksumTwoKey(crc); }  // 4: 0x57b960
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57b970
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57b9e0
    virtual int Execute() override;  // 8: 0x57ba50
    virtual int GetKind() override { return 0x320; }  // 9: 0x57b8c0
};

// mission manager event: hands the key and the value at +0x50 to the manager at root member +0xb8 (0x546e30)
class Event313 : public SourceEvent {
public:
    Event313();  // 0x57d790
    Event313(const Event313& other);  // 0x57d7e0
    virtual ~Event313();  // 0x57d7d0 (deleting 0x57d7b0)

    virtual Event313* Clone() override;  // 1: 0x57d800
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57d8f0
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57d960
    virtual int Execute() override;  // 8: 0x57d9d0
    virtual int GetKind() override { return 0x313; }  // 9: 0x57d870
};

// deletes the views of the object it names (DeleteObjectViews); priority 0x999
class Event31a : public SourceEvent {
public:
    Event31a();  // 0x57bac0
    Event31a(const Event31a& other);  // 0x57bb10
    virtual ~Event31a();  // 0x57bb00 (deleting 0x57bae0)

    virtual Event31a* Clone() override;  // 1: 0x57bb30
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57bc20
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57bc90
    virtual int Execute() override;  // 8: 0x57bd00
    virtual int GetKind() override { return 0x31a; }  // 9: 0x57bba0
    virtual int GetPriority() override { return 0x999; }  // 10: 0x57bd20
};

// character event: hands the character and the value at +0x50 to the root member at +0xc0 (0x561990)
class Event317 : public SourceEvent {
public:
    Event317();  // 0x57d430
    Event317(const Event317& other);  // 0x57d480
    virtual ~Event317();  // 0x57d470 (deleting 0x57d450)

    virtual Event317* Clone() override;  // 1: 0x57d4a0
    virtual void Read(istream* in) override { ReadSource(in); }  // 2: 0x541180
    virtual void Write(ostream* out) override { WriteSource(out); }  // 3: 0x564d50
    virtual void Checksum(Crc32* crc) override { ChecksumSource(crc); }  // 4: 0x541700
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x57d590
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x57d600
    virtual int Execute() override;  // 8: 0x57d670
    virtual int GetKind() override { return 0x317; }  // 9: 0x57d510
};

// Factories registered in the event factory table.
Event301* CreateEvent301();  // 0x57a090
Event31d* CreateEvent31d();  // 0x57a340
Event300* CreateEvent300();  // 0x57a5e0
Event308* CreateEvent308();  // 0x57a890
Event306* CreateEvent306();  // 0x57ab40
Event316* CreateEvent316();  // 0x57adf0
Event307* CreateEvent307();  // 0x57b0a0
Event302* CreateEvent302();  // 0x57b360
Event318* CreateEvent318();  // 0x57b610
Event315* CreateEvent315();  // 0x5788f0
Event30a* CreateEvent30a();  // 0x57c8b0
Event320* CreateEvent320();  // 0x57b8d0
Event313* CreateEvent313();  // 0x57d880
Event31a* CreateEvent31a();  // 0x57bbb0
Event317* CreateEvent317();  // 0x57d520

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(Event301, 0x54);
REB_ASSERT_SIZE(Event31d, 0x54);
REB_ASSERT_SIZE(Event300, 0x54);
REB_ASSERT_SIZE(Event308, 0x54);
REB_ASSERT_SIZE(Event306, 0x54);
REB_ASSERT_SIZE(Event316, 0x54);
REB_ASSERT_SIZE(Event307, 0x54);
REB_ASSERT_SIZE(Event302, 0x54);
REB_ASSERT_SIZE(Event318, 0x54);
REB_ASSERT_SIZE(Event315, 0x54);
REB_ASSERT_SIZE(Event30a, 0x54);
REB_ASSERT_SIZE(Event320, 0x54);
REB_ASSERT_SIZE(Event313, 0x54);
REB_ASSERT_SIZE(Event31a, 0x54);
REB_ASSERT_SIZE(Event317, 0x54);
#pragma clang diagnostic pop
