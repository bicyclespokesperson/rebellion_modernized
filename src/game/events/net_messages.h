#pragma once
// The small synchronisation messages the player controllers exchange (0x4403c0-0x440e30, registered with
// RegisterOrderFactory by RegisterMultiplayerOrderFactories). The kind numbers name the classes; the field names are
// provisional and follow what the senders (0x485520, 0x485640, 0x488750) and the controller's vote state store.
//
//   kind 1     NetMsg1   SidedMessage: the sender's sync point and its checksums (vote/sync protocol, PlayerController)
//   kind 2     NetMsg2   SidedMessage: acknowledges a sync point
//   kind 4     NetMsg4   MessageBase: one vote (id, state, time, object key) as PlayerController::Virtual18 takes it
//   kind 5     NetMsg5   SidedMessage with a single value
//   kind 0x13  NetMsg13  MessageBase with no data
//   kind 0x15  NetMsg15  MessageBase with three values
#include <cstdint>

#include "app/player_controller_decl.h"
#include "events/notif_event_payloads.h"
#include "gameobj/object_key.h"
#include "orders/order_base.h"

class NetMsg1 : public SidedMessage
{
public:
  NetMsg1(); // 0x440910
  explicit NetMsg1(int syncPoint); // 0x440980
  ~NetMsg1() override; // 0x4409e0 (deleting 0x440960)

  int Read(istream* in) override; // 1: 0x440aa0
  int Write(ostream* out) override; // 2: 0x440ad0
  int GetKind() override
  {
    return 1;
  } // 3, shared 0x40f340
  int CheckEnabled(StatusCode* status) override; // 6: 0x440b00
  int CheckValid(StatusCode* status) override; // 7: 0x440b70
  int Execute(StatusCode* status) override; // 8: 0x440be0

  int m_syncPoint; // +0x2c
  ChecksumRecord m_checksums; // +0x30
};

class NetMsg2 : public SidedMessage
{
public:
  NetMsg2(); // 0x440c60
  ~NetMsg2() override; // 0x440ca0 (deleting 0x440c80)

  int Read(istream* in) override; // 1: 0x628ee0
  int Write(ostream* out) override; // 2: 0x440d20
  int GetKind() override
  {
    return 2;
  } // 3, shared 0x43c1f0
  int CheckEnabled(StatusCode* status) override; // 6: 0x440d50
  int CheckValid(StatusCode* status) override; // 7: 0x440dc0
  int Execute(StatusCode* status) override; // 8: 0x440e30

  int m_syncPoint; // +0x2c
};

class NetMsg4 : public MessageBase
{
public:
  NetMsg4(); // 0x440630
  ~NetMsg4() override; // 0x4406c0 (deleting 0x4406a0)

  int Read(istream* in) override; // 1: 0x440790
  int Write(ostream* out) override; // 2: 0x4407e0
  int GetKind() override
  {
    return 4;
  } // 3, shared 0x4047d0

  int m_voteId; // +0x20
  int m_voteState; // +0x24
  GameTime m_voteTime; // +0x28
  ObjectKey m_voteKey; // +0x2c
};

class NetMsg5 : public SidedMessage
{
public:
  NetMsg5(); // 0x4403c0
  ~NetMsg5() override; // 0x440400 (deleting 0x4403e0)

  int Read(istream* in) override; // 1: 0x440480
  int Write(ostream* out) override; // 2: 0x4404b0
  int GetKind() override
  {
    return 5;
  } // 3, shared 0x404c20
  int CheckEnabled(StatusCode*) override
  {
    return 1;
  } // 6, shared 0x51ebb0
  int CheckValid(StatusCode*) override
  {
    return 1;
  } // 7, shared 0x51ebb0
  int Execute(StatusCode* status) override; // 8: 0x4404e0

  int m_value; // +0x2c
};

class NetMsg13 : public MessageBase
{
public:
  NetMsg13(); // 0x440830
  ~NetMsg13() override; // 0x440870 (deleting 0x440850)

  int Read(istream* in) override; // 1: 0x4408f0
  int Write(ostream* out) override; // 2: 0x440900
  int GetKind() override
  {
    return 0x13;
  } // 3, shared 0x412fb0
};

class NetMsg15 : public MessageBase
{
public:
  NetMsg15(); // 0x4404f0
  ~NetMsg15() override; // 0x440530 (deleting 0x440510)

  int Read(istream* in) override; // 1: 0x4405b0
  int Write(ostream* out) override; // 2: 0x4405f0
  int GetKind() override
  {
    return 0x15;
  } // 3, shared 0x412120

  int m_valueA; // +0x20
  int m_valueB; // +0x24
  int m_valueC; // +0x28
};

// Kind 0x280 (code at 0x628e10, among the library functions): tells every machine to store a value in a side's
// unknown field +0xc4 (Side::SetUnknownC4, ignored when zero).
class NetMsg280 : public SidedMessage
{
public:
  NetMsg280(); // 0x628e10
  ~NetMsg280() override; // 0x628e50 (deleting 0x628e30)

  int Read(istream* in) override; // 1: shared 0x628ee0 (NetMsg2::Read)
  int Write(ostream* out) override; // 2: shared 0x440d20 (NetMsg2::Write)
  int GetKind() override; // 3: 0x628ed0
  int CheckEnabled(StatusCode* status) override; // 6: 0x628f10
  int CheckValid(StatusCode* status) override; // 7: 0x628f80
  int Execute(StatusCode* status) override; // 8: 0x628ff0

  int m_value; // +0x2c
};

void* CreateNetMsg1(); // 0x440a30
void* CreateNetMsg2(); // 0x440cb0
void* CreateNetMsg4(); // 0x440720
void* CreateNetMsg5(); // 0x440410
void* CreateNetMsg13(); // 0x440880
void* CreateNetMsg15(); // 0x440540
void* CreateNetMsg280(); // 0x628e60
