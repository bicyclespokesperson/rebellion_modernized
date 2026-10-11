#pragma once
// The concrete command and event classes in 0x540c40-0x541d60 (VT 0x661af0-0x661bd8): one Command and five events that
// the character/mission code raises when a person is removed, resigns or is locked in a mission. Each leaf class
// forwards Read/Write/Checksum to its base class (the original shares those forwarders between many classes), has
// trivial CheckEnabled/CheckValid (status {1, -1}) and runs its job in Execute, which returns 0: the queue deletes
// the event afterwards. CreateXxx is the class's entry in the event factory table (0x51ef80).
#include "events/event_base.h"

// Kind 0x230: asks the role named by the command's key (family 0x30-0x3f) to carry out its ActivityRequest.
// CheckEnabled/CheckValid resolve the role and ask it Role::CheckActivity / Role::CheckActivityStatus; Execute runs
// Role::Virtual128 (the status check followed by the destruction of the role).
class RoleActivityCommand : public Command
{
public:
  RoleActivityCommand(); // 0x540c40
  RoleActivityCommand(const RoleActivityCommand& other); // 0x540c90
  ~RoleActivityCommand() override; // 0x540c60 (deleting), 0x540c80

  Command* Clone() override; // 1: 0x540cb0
  void Read(istream* in) override
  {
    ReadCommand(in);
  } // 0x5635f0 (shared)
  void Write(ostream* out) override
  {
    WriteCommand(out);
  } // 0x563600 (shared)
  void Checksum(Crc32* crc) override
  {
    ChecksumCommand(crc);
  } // 0x563610 (shared)
  int CheckEnabled(StatusCode* status) override; // 6: 0x540da0
  int CheckValid(StatusCode* status) override; // 7: 0x540e70
  int Execute() override; // 8: 0x540f40
  int GetKind() override; // 9: 0x540d20
};

// Kind 0x30b: Execute asks ProcessRemoveRequest (0x546240) to handle the removal of the object named by m_key; m_valueB
// is the request value.
class RemoveRequestEvent : public SourceEvent
{
public:
  RemoveRequestEvent(); // 0x541860
  RemoveRequestEvent(const RemoveRequestEvent& other); // 0x5418b0
  ~RemoveRequestEvent() override; // 0x541880 (deleting), 0x5418a0

  EventBase* Clone() override; // 1: 0x5418d0
  void Read(istream* in) override
  {
    ReadSource(in);
  } // 0x541180 (shared)
  void Write(ostream* out) override
  {
    WriteSource(out);
  } // 0x564d50 (shared)
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  } // 0x541700 (shared)
  int CheckEnabled(StatusCode* status) override; // 6: 0x5419d0
  int CheckValid(StatusCode* status) override; // 7: 0x541a40
  int Execute() override; // 8: 0x541ab0
  int GetKind() override; // 9: 0x541940
  int GetPriority() override; // 10: 0x541950, 0x150
};

// Kind 0x30e: Execute hands (m_key, m_valueB) to the role-state handler 0x515970.
class RoleStateEvent : public SourceEvent
{
public:
  RoleStateEvent(); // 0x541020
  RoleStateEvent(const RoleStateEvent& other); // 0x541070
  ~RoleStateEvent() override; // 0x541040 (deleting), 0x541060

  EventBase* Clone() override; // 1: 0x541090
  void Read(istream* in) override
  {
    ReadSource(in);
  }
  void Write(ostream* out) override
  {
    WriteSource(out);
  }
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  }
  int CheckEnabled(StatusCode* status) override; // 6: 0x541190
  int CheckValid(StatusCode* status) override; // 7: 0x541200
  int Execute() override; // 8: 0x541270
  int GetKind() override; // 9: 0x541100
  int GetPriority() override; // 10: 0x48b450, 0x200
};

// Kind 0x312: Execute asks OnPersonRemoved (0x545e90) to handle the loss of the person named by m_key, unless there is
// no game root.
class PersonRemovedEvent : public SourceEvent
{
public:
  PersonRemovedEvent(); // 0x5412e0
  PersonRemovedEvent(const PersonRemovedEvent& other); // 0x541330
  ~PersonRemovedEvent() override; // 0x541300 (deleting), 0x541320

  EventBase* Clone() override; // 1: 0x541350
  void Read(istream* in) override
  {
    ReadSource(in);
  }
  void Write(ostream* out) override
  {
    WriteSource(out);
  }
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  }
  int CheckEnabled(StatusCode* status) override; // 6: 0x541440
  int CheckValid(StatusCode* status) override; // 7: 0x5414b0
  int Execute() override; // 8: 0x541520
  int GetKind() override; // 9: 0x5413c0
};

// Kind 0x314: Execute asks ProcessResignRequest (0x546000) to handle the resignation of the person named by m_key.
class ResignRequestEvent : public SourceEvent
{
public:
  ResignRequestEvent(); // 0x541590
  ResignRequestEvent(const ResignRequestEvent& other); // 0x5415e0
  ~ResignRequestEvent() override; // 0x5415b0 (deleting), 0x5415d0

  EventBase* Clone() override; // 1: 0x541600
  void Read(istream* in) override
  {
    ReadSource(in);
  }
  void Write(ostream* out) override
  {
    WriteSource(out);
  }
  void Checksum(Crc32* crc) override
  {
    ChecksumSource(crc);
  }
  int CheckEnabled(StatusCode* status) override; // 6: 0x541710
  int CheckValid(StatusCode* status) override; // 7: 0x541780
  int Execute() override; // 8: 0x5417f0
  int GetKind() override; // 9: 0x541670
  int GetPriority() override; // 10: 0x541680, 0x152
};

// Kind 0x321: moves a lock from one object to another: Execute locks the master named by m_keyD and unlocks the one
// named by m_keyC (both families 0x40-0x7f).
class LockTransferEvent : public TwoKeyEvent
{
public:
  LockTransferEvent(); // 0x541b20
  LockTransferEvent(const LockTransferEvent& other); // 0x541b70
  ~LockTransferEvent() override; // 0x541b40 (deleting), 0x541b60

  EventBase* Clone() override; // 1: 0x541b90
  void Read(istream* in) override
  {
    ReadTwoKey(in);
  } // 0x57b940 (shared)
  void Write(ostream* out) override
  {
    WriteTwoKey(out);
  } // 0x57b950 (shared)
  void Checksum(Crc32* crc) override
  {
    ChecksumTwoKey(crc);
  } // 0x57b960 (shared)
  int CheckEnabled(StatusCode* status) override; // 6: 0x541c80
  int CheckValid(StatusCode* status) override; // 7: 0x541cf0
  int Execute() override; // 8: 0x541d60
  int GetKind() override; // 9: 0x541c00
};

// The Read (0x541180) and Checksum (0x541700) slots that every SourceEvent leaf class below shares with the other leaf
// classes of the family (0x564d50, the Write slot, belongs to the command classes).
void SourceEventRead(SourceEvent* event, istream* in);
void SourceEventChecksum(SourceEvent* event, Crc32* crc);

void* CreateRoleActivityCommand(); // 0x540d30
void* CreateRoleStateEvent(); // 0x541110
void* CreatePersonRemovedEvent(); // 0x5413d0
void* CreateResignRequestEvent(); // 0x541690
void* CreateRemoveRequestEvent(); // 0x541960
void* CreateLockTransferEvent(); // 0x541c10
