#pragma once
// Events 0x370-0x372 on the plain event base (0x54f4e0): side queue servicing and the root reparenting event.
#include <cstdint>

#include "events/event_base.h"


// kind 0x370: serves the refinery queue of the side it names (Side::ServeRefineryQueue); re-queued while work remains; priority 0x20
class RefineryQueueEvent : public PlainEvent {
public:
    RefineryQueueEvent();  // 0x5777c0
    RefineryQueueEvent(const RefineryQueueEvent& other);  // 0x577810
    virtual ~RefineryQueueEvent();  // 0x577800 (deleting 0x5777e0)

    virtual RefineryQueueEvent* Clone() override;  // 1: 0x577830
    virtual void Read(istream* in) override { ReadPlain(in); }  // 2: 0x577910
    virtual void Write(ostream* out) override { WritePlain(out); }  // 3: 0x577920
    virtual void Checksum(Crc32* crc) override { ChecksumPlain(crc); }  // 4: 0x577930
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x577940
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x5779b0
    virtual int Execute() override;  // 8: 0x577a20
    virtual int GetKind() override { return 0x370; }  // 9: 0x43cc40
    virtual int GetPriority() override { return 0x20; }  // 10: 0x526bd0
};

// kind 0x371: serves the manufacturer queue of the side it names; re-queued while work remains; priority 0x21
class ManufacturerQueueEvent : public PlainEvent {
public:
    ManufacturerQueueEvent();  // 0x577b00
    ManufacturerQueueEvent(const ManufacturerQueueEvent& other);  // 0x577b50
    virtual ~ManufacturerQueueEvent();  // 0x577b40 (deleting 0x577b20)

    virtual ManufacturerQueueEvent* Clone() override;  // 1: 0x577b70
    virtual void Read(istream* in) override { ReadPlain(in); }  // 2: 0x577910
    virtual void Write(ostream* out) override { WritePlain(out); }  // 3: 0x577920
    virtual void Checksum(Crc32* crc) override { ChecksumPlain(crc); }  // 4: 0x577930
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x577c60
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x577cd0
    virtual int Execute() override;  // 8: 0x577d40
    virtual int GetKind() override { return 0x371; }  // 9: 0x577be0
    virtual int GetPriority() override { return 0x21; }  // 10: 0x577e20
};

// kind 0x372: moves the object it names under the root member at +0xb0 when its parent differs; priority 0x998
class ReparentToRootEvent : public PlainEvent {
public:
    ReparentToRootEvent();  // 0x579bf0
    ReparentToRootEvent(const ReparentToRootEvent& other);  // 0x579c40
    virtual ~ReparentToRootEvent();  // 0x579c30 (deleting 0x579c10)

    virtual ReparentToRootEvent* Clone() override;  // 1: 0x579c60
    virtual void Read(istream* in) override { ReadPlain(in); }  // 2: 0x577910
    virtual void Write(ostream* out) override { WritePlain(out); }  // 3: 0x577920
    virtual void Checksum(Crc32* crc) override { ChecksumPlain(crc); }  // 4: 0x577930
    virtual int CheckEnabled(StatusCode* status) override;  // 6: 0x579d50
    virtual int CheckValid(StatusCode* status) override;  // 7: 0x579dc0
    virtual int Execute() override;  // 8: 0x579e30
    virtual int GetKind() override { return 0x372; }  // 9: 0x579cd0
    virtual int GetPriority() override { return 0x998; }  // 10: 0x579f90
};

// Factories registered in the event factory table.
RefineryQueueEvent* CreateRefineryQueueEvent();  // 0x5778a0
ManufacturerQueueEvent* CreateManufacturerQueueEvent();  // 0x577bf0
ReparentToRootEvent* CreateReparentToRootEvent();  // 0x579ce0
