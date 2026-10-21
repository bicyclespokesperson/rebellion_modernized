#pragma once
// The commands, timers and notification events of the mission system (0x54cd80-0x54ea70). Each class is a leaf of one
// of the bases in events/event_base.h, events/timer_event.h and orders/command_decl.h and is created by kind through
// the factory table (0x51ef80); every class has a factory function next to it. The kinds:
//
//   0x250  MissionOrderCommand       command: create a mission from a mission order's team (VT 0x661e28, 0x74 bytes)
//   0x261  MissionActivityCommand    command: run an activity request on a mission (VT 0x661f20, 0x50 bytes)
//   0x38b  MissionStatATimer         timer on a mission's stat A: ResetImpliedTeamIfPhase8 (VT 0x661e50)
//   0x38c  MissionStatBTimer         timer on a mission's stat B: ResumeFromPhase11 (VT 0x661e88)
//   0x392  MissionMgrStatATimer      timer on the mission manager's stat at +0x5c (VT 0x661f48)
//   0x393  MissionMgrStatBTimer      timer on the mission manager's stat at +0x64 (VT 0x661f80)
//   0x30c  MissionMgrEvent30c        event handled by the mission manager with one argument (VT 0x661ec0)
//   0x30d  MissionMgrEvent30d        event handled by the mission manager with two arguments (VT 0x661ef0)
#include <cstdint>

#include "events/event_base.h"
#include "events/timer_event.h"
#include "gameobj/object_key.h"
#include "mission/mission_mgr_decl.h"
#include "orders/command_decl.h"

// Kind 0x250: the command a mission order (orders/mission_orders.h) queues. Its checks resolve the mission manager
// (key 0xf8000006 in m_key) and ask it whether the mission can be created; Execute creates it.
class MissionOrderCommand : public Command
{
  public:
  MissionOrderCommand();                                 // 0x54cd80
  MissionOrderCommand(const MissionOrderCommand& other); // 0x54cea0
  ~MissionOrderCommand() override;                       // 0x54ce20 (deleting 0x54ce00)

  Command* Clone() override;                     // 1, 0x54cf40
  void Read(istream* in) override;               // 2, 0x54d030
  void Write(ostream* out) override;             // 3, 0x54d090
  void Checksum(Crc32* crc) override;            // 4, 0x54d0f0 (slot is void in Command: callee EAX leaks)
  int CheckEnabled(StatusCode* status) override; // 6, 0x54d1a0
  int CheckValid(StatusCode* status) override;   // 7, 0x54d280
  int Execute() override;                        // 8, 0x54d360
  int GetKind() override
  {
    return 0x250;
  } // 9, 0x54cfb0

  int m_unk50;                    // +0x50
  int m_unk54;                    // +0x54
  int m_side;                     // +0x58, the side bits the order's validator found for the mission
  ObjectKey m_missionKey;         // +0x5c, starts at 0 (not at the unset value)
  ObjectKey m_resultKey;          // +0x60
  ObjectKeyValueSet m_team;       // +0x64
  ObjectKeyValueSet m_characters; // +0x6c
};

// Kind 0x261: a command on a mission (key family 0x40-0x7f) that forwards the request to the mission's activity
// checks (slots 112 / 113) and to slot 117. No fields of its own.
class MissionActivityCommand : public Command
{
  public:
  MissionActivityCommand();                                    // 0x54e090
  MissionActivityCommand(const MissionActivityCommand& other); // 0x54e0e0
  ~MissionActivityCommand() override;                          // 0x54e0d0 (deleting 0x54e0b0)

  Command* Clone() override; // 1, 0x54e100
  void Read(istream* in) override
  {
    ReadCommand(in);
  } // 2, shared 0x5635f0
  void Write(ostream* out) override
  {
    WriteCommand(out);
  } // 3, shared 0x563600
  void Checksum(Crc32* crc) override
  {
    ChecksumCommand(crc);
  } // 4, shared 0x563610
  int CheckEnabled(StatusCode* status) override; // 6, 0x54e170
  int CheckValid(StatusCode* status) override;   // 7, 0x54e240
  int Execute() override;                        // 8, 0x54e310
  int GetKind() override
  {
    return 0x261;
  } // 9, shared 0x479fd0
};

// Shared slot 10 of the mission timers / of the manager timers.
FamilyRange* GetMissionFamilyRange(FamilyRange* out);    // 0x54d920: {0x40, 0x80}
FamilyRange* GetMissionMgrFamilyRange(FamilyRange* out); // 0x54e880: {0xf8, 0xf9}

// The timers over the stat of a mission (0x38b, 0x38c) or of the mission manager (0x392, 0x393). All four live in the
// mission-manager family ranges: slot 10 of the mission timers returns {0x40, 0x80}, of the manager timers {0xf8,
// 0xf9}.
class MissionStatATimer : public TimerEvent
{
  public:
  MissionStatATimer();                               // 0x54d490
  MissionStatATimer(const MissionStatATimer& other); // 0x54d4e0
  ~MissionStatATimer() override;                     // 0x54d4d0 (deleting 0x54d4b0)

  MissionStatATimer* Clone() override; // 1, 0x54d500
  int GetKind() override
  {
    return 0x38b;
  } // 9, 0x54d570
  FamilyRange* GetTargetRange(FamilyRange* out) override
  {
    return GetMissionFamilyRange(out);
  } // 10, shared 0x54d920
  TrackedStat* GetTargetSpec() override; // 11, 0x54d5f0
  void Fire() override;                  // 12, 0x54d6c0
};

class MissionStatBTimer : public TimerEvent
{
  public:
  MissionStatBTimer();                               // 0x54d7c0
  MissionStatBTimer(const MissionStatBTimer& other); // 0x54d810
  ~MissionStatBTimer() override;                     // 0x54d800 (deleting 0x54d7e0)

  MissionStatBTimer* Clone() override; // 1, 0x54d830
  int GetKind() override
  {
    return 0x38c;
  } // 9, 0x54d8a0
  FamilyRange* GetTargetRange(FamilyRange* out) override
  {
    return GetMissionFamilyRange(out);
  } // 10, shared 0x54d920
  TrackedStat* GetTargetSpec() override; // 11, 0x54d940
  void Fire() override;                  // 12, 0x54da10
};

class MissionMgrStatATimer : public TimerEvent
{
  public:
  MissionMgrStatATimer();                                  // 0x54e3f0
  MissionMgrStatATimer(const MissionMgrStatATimer& other); // 0x54e440
  ~MissionMgrStatATimer() override;                        // 0x54e430 (deleting 0x54e410)

  MissionMgrStatATimer* Clone() override; // 1, 0x54e460
  int GetKind() override
  {
    return 0x392;
  } // 9, 0x54e4d0
  FamilyRange* GetTargetRange(FamilyRange* out) override
  {
    return GetMissionMgrFamilyRange(out);
  } // 10, shared 0x54e880
  TrackedStat* GetTargetSpec() override; // 11, 0x54e550
  void Fire() override;                  // 12, 0x54e620
};

class MissionMgrStatBTimer : public TimerEvent
{
  public:
  MissionMgrStatBTimer();                                  // 0x54e720
  MissionMgrStatBTimer(const MissionMgrStatBTimer& other); // 0x54e770
  ~MissionMgrStatBTimer() override;                        // 0x54e760 (deleting 0x54e740)

  MissionMgrStatBTimer* Clone() override; // 1, 0x54e790
  int GetKind() override
  {
    return 0x393;
  } // 9, 0x54e800
  FamilyRange* GetTargetRange(FamilyRange* out) override
  {
    return GetMissionMgrFamilyRange(out);
  } // 10, shared 0x54e880
  TrackedStat* GetTargetSpec() override; // 11, 0x54e8a0
  void Fire() override;                  // 12, 0x54e970
};

// Kinds 0x30c / 0x30d: events whose Execute hands the key and arguments to the mission manager. Their checks always
// succeed and Read/Write/Checksum are the shared bodies 0x541180 / 0x564d50 / 0x541700.
class MissionMgrEvent30c : public SourceEvent
{
  public:
  MissionMgrEvent30c();                                // 0x54db10
  MissionMgrEvent30c(const MissionMgrEvent30c& other); // 0x54db60
  ~MissionMgrEvent30c() override;                      // 0x54db50 (deleting 0x54db30)

  MissionMgrEvent30c* Clone() override; // 1, 0x54db80
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2, 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3, 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4, 0x541700
  int CheckEnabled(StatusCode* status) override; // 6, 0x54dc70
  int CheckValid(StatusCode* status) override;   // 7, 0x54dce0
  int Execute() override;                        // 8, 0x54dd50
  int GetKind() override
  {
    return 0x30c;
  } // 9, 0x54dbf0
  int GetPriority() const override
  {
    return 0x30c;
  } // 10, shared 0x54dbf0
};

class MissionMgrEvent30d : public SourceEvent
{
  public:
  MissionMgrEvent30d();                                // 0x54ddc0
  MissionMgrEvent30d(const MissionMgrEvent30d& other); // 0x54de10
  ~MissionMgrEvent30d() override;                      // 0x54de00 (deleting 0x54dde0)

  MissionMgrEvent30d* Clone() override; // 1, 0x54de30
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 2, 0x541180
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 3, 0x564d50
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 4, 0x541700
  int CheckEnabled(StatusCode* status) override; // 6, 0x54df30
  int CheckValid(StatusCode* status) override;   // 7, 0x54dfa0
  int Execute() override;                        // 8, 0x54e010
  int GetKind() override
  {
    return 0x30d;
  } // 9, 0x54dea0
  int GetPriority() const override
  {
    return 0x156;
  } // 10, 0x54deb0
};

// Factories registered for the kinds (0x51ef80); each returns a default-constructed object or null.
MissionOrderCommand* CreateMissionOrderCommand();   // 0x54cfc0
MissionStatATimer* CreateMissionStatATimer();       // 0x54d580
MissionStatBTimer* CreateMissionStatBTimer();       // 0x54d8b0
MissionMgrEvent30c* CreateMissionMgrEvent30c();     // 0x54dc00
MissionMgrEvent30d* CreateMissionMgrEvent30d();     // 0x54dec0
MissionMgrStatATimer* CreateMissionMgrStatATimer(); // 0x54e4e0
MissionMgrStatBTimer* CreateMissionMgrStatBTimer(); // 0x54e810
