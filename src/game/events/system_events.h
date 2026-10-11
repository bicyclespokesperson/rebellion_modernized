#pragma once
// Events 0x303-0x31b that act on a systems (family 0x90-0x97) through the system handlers at 0x515c00-0x515f40.
#include <cstdint>

#include "events/event_base.h"

// system event: runs the system-side handler 0x515cd0 with the value at +0x50
class SystemEvent303 : public SourceEvent
{
public:
  SystemEvent303(); // 0x5638e0
  SystemEvent303(const SystemEvent303& other); // 0x563930
  ~SystemEvent303() override; // 0x563920 (deleting 0x563900)

  SystemEvent303* Clone() override; // 1: 0x563950
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x563a40
  int CheckValid(StatusCode* status) override; // 7: 0x563ab0
  int Execute() override; // 8: 0x563b20
  int GetKind() override
  {
    return 0x303;
  } // 9: 0x5639c0
};

// system event: handler 0x515dc0
class SystemEvent304 : public SourceEvent
{
public:
  SystemEvent304(); // 0x563b90
  SystemEvent304(const SystemEvent304& other); // 0x563be0
  ~SystemEvent304() override; // 0x563bd0 (deleting 0x563bb0)

  SystemEvent304* Clone() override; // 1: 0x563c00
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x563cf0
  int CheckValid(StatusCode* status) override; // 7: 0x563d60
  int Execute() override; // 8: 0x563dd0
  int GetKind() override
  {
    return 0x304;
  } // 9: 0x563c70
};

// system event: handler 0x515f40
class SystemEvent305 : public SourceEvent
{
public:
  SystemEvent305(); // 0x563e40
  SystemEvent305(const SystemEvent305& other); // 0x563e90
  ~SystemEvent305() override; // 0x563e80 (deleting 0x563e60)

  SystemEvent305* Clone() override; // 1: 0x563eb0
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x563f90
  int CheckValid(StatusCode* status) override; // 7: 0x564000
  int Execute() override; // 8: 0x564070
  int GetKind() override
  {
    return 0x305;
  } // 9: 0x43ef00
};

// system event: handler 0x515e30
class SystemEvent309 : public SourceEvent
{
public:
  SystemEvent309(); // 0x5640e0
  SystemEvent309(const SystemEvent309& other); // 0x564130
  ~SystemEvent309() override; // 0x564120 (deleting 0x564100)

  SystemEvent309* Clone() override; // 1: 0x564150
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x564240
  int CheckValid(StatusCode* status) override; // 7: 0x5642b0
  int Execute() override; // 8: 0x564320
  int GetKind() override
  {
    return 0x309;
  } // 9: 0x5641c0
};

// system event: handler 0x515e70
class SystemEvent310 : public SourceEvent
{
public:
  SystemEvent310(); // 0x564390
  SystemEvent310(const SystemEvent310& other); // 0x5643e0
  ~SystemEvent310() override; // 0x5643d0 (deleting 0x5643b0)

  SystemEvent310* Clone() override; // 1: 0x564400
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x5644f0
  int CheckValid(StatusCode* status) override; // 7: 0x564560
  int Execute() override; // 8: 0x5645d0
  int GetKind() override
  {
    return 0x310;
  } // 9: 0x564470
};

// system event: handler 0x515eb0
class SystemEvent30f : public SourceEvent
{
public:
  SystemEvent30f(); // 0x564640
  SystemEvent30f(const SystemEvent30f& other); // 0x564690
  ~SystemEvent30f() override; // 0x564680 (deleting 0x564660)

  SystemEvent30f* Clone() override; // 1: 0x5646b0
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x5647a0
  int CheckValid(StatusCode* status) override; // 7: 0x564810
  int Execute() override; // 8: 0x564880
  int GetKind() override
  {
    return 0x30f;
  } // 9: 0x564720
};

// system event: handler 0x515c00 with the two side-bit sets at +0x4c/+0x50
class SystemEvent31b : public SourceEvent
{
public:
  SystemEvent31b(); // 0x5648f0
  SystemEvent31b(const SystemEvent31b& other); // 0x564940
  ~SystemEvent31b() override; // 0x564930 (deleting 0x564910)

  SystemEvent31b* Clone() override; // 1: 0x564960
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x564a50
  int CheckValid(StatusCode* status) override; // 7: 0x564ac0
  int Execute() override; // 8: 0x564b30
  int GetKind() override
  {
    return 0x31b;
  } // 9: 0x5649d0
};

// system event: handler 0x515ef0; sorted at priority 0x400
class SystemEvent311 : public SourceEvent
{
public:
  SystemEvent311(); // 0x564bf0
  SystemEvent311(const SystemEvent311& other); // 0x564c40
  ~SystemEvent311() override; // 0x564c30 (deleting 0x564c10)

  SystemEvent311* Clone() override; // 1: 0x564c60
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x564d60
  int CheckValid(StatusCode* status) override; // 7: 0x564dd0
  int Execute() override; // 8: 0x564e40
  int GetKind() override
  {
    return 0x311;
  } // 9: 0x564cd0
  int GetPriority() override
  {
    return 0x400;
  } // 10: 0x48e670
};

// system event: marks the system as changed for both sides (SyncViewForEvent kind 0xb); priority 0x801
class SystemEvent319 : public SourceEvent
{
public:
  SystemEvent319(); // 0x564eb0
  SystemEvent319(const SystemEvent319& other); // 0x564f00
  ~SystemEvent319() override; // 0x564ef0 (deleting 0x564ed0)

  SystemEvent319* Clone() override; // 1: 0x564f20
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2: 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3: 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4: 0x541700
  int CheckEnabled(StatusCode* status) override; // 6: 0x565010
  int CheckValid(StatusCode* status) override; // 7: 0x565080
  int Execute() override; // 8: 0x5650f0
  int GetKind() override
  {
    return 0x319;
  } // 9: 0x564f90
  int GetPriority() override
  {
    return 0x801;
  } // 10: 0x5651c0
  ObjectKey* GetKey(ObjectKey* out); // 0x5651a0: copies the event key
};

// Factories registered in the event factory table.
SystemEvent303* CreateSystemEvent303(); // 0x5639d0
SystemEvent304* CreateSystemEvent304(); // 0x563c80
SystemEvent305* CreateSystemEvent305(); // 0x563f20
SystemEvent309* CreateSystemEvent309(); // 0x5641d0
SystemEvent310* CreateSystemEvent310(); // 0x564480
SystemEvent30f* CreateSystemEvent30f(); // 0x564730
SystemEvent31b* CreateSystemEvent31b(); // 0x5649e0
SystemEvent311* CreateSystemEvent311(); // 0x564ce0
SystemEvent319* CreateSystemEvent319(); // 0x564fa0
