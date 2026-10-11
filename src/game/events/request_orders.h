#pragma once
// The player-request classes (0x580530-0x582a00): the order leaves and one message that carry the player's requests
// to the game. Each order builds the command (events/command_classes2.h, events/command_classes.h) that carries it out.
//
//   0x250  Order250        OrderBase leaf, command MissionActivityCommand (0x261)
//   ----   PayloadMessage  SidedMessage kind 3 (VT 0x669bc8): schedules a PayloadEvent3f0 for a side
//   0x270  Order270        move request, command MoveCommand (0x201), checks the room at the destination
//   ----   RootCommandOrder  abstract (VT 0x669c60): at most one command, command RootCommand240
//   0x260-0x263  RootOrder260..263  its leaves
//   0x268  Order268        OrderBase leaf, command RootCommand242
//   0x234  Order234        order over a second command queue (+0x44), command FleetCommand221
//   ----   FleetCommandOrder  the same shape over command FleetCommand220 (abstract, VT 0x669ec8)
//   0x221, 0x223, 0x222, 0x220  Order221, Order223, Order222, Order220  its leaves
//
// The checks of Order234 / FleetCommandOrder end in two helpers (0x581ae0 / 0x582440) that decide whether the fleets a
// request depends on can carry out the request (troops aboard / ships and Death Stars).
#include <cstdint>

#include "events/command_classes.h"
#include "events/command_classes2.h"
#include "events/event_base.h"
#include "orders/order_base.h"

class PayloadEvent3f0;

class Order250 : public OrderBase
{
public:
  Order250(); // 0x580530
  ~Order250() override; // 0x580570 (deleting 0x580550)

  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 1, shared 0x53d0f0
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 2, shared 0x53dd90
  int GetKind() override
  {
    return 0x250;
  } // 3, shared 0x54cfb0
  Command* CreateCommand() override; // 18: 0x5805f0
};

class PayloadMessage : public SidedMessage
{
public:
  PayloadMessage(); // 0x580660
  ~PayloadMessage() override; // 0x5806a0 (deleting 0x580680)

  int Read(istream* in) override; // 1: 0x5806b0
  int Write(ostream* out) override; // 2: 0x580700
  int GetKind() override
  {
    return 3;
  } // 3, shared 0x526f00
  int CheckEnabled(StatusCode* status) override; // 6: 0x580750
  int CheckValid(StatusCode* status) override; // 7: 0x5807c0
  int Execute(StatusCode* status) override; // 8: 0x580830

  int m_delay; // +0x2c, days until the event runs
  int m_valueA; // +0x30
  int m_valueB; // +0x34
};

class Order270 : public OrderBase
{
public:
  Order270(); // 0x5809c0
  ~Order270() override; // 0x580a00 (deleting 0x5809e0)

  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 1, shared 0x53d0f0
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 2, shared 0x53dd90
  int GetKind() override; // 3: 0x580a80
  Command* CreateCommand() override; // 18: 0x580a90
  int CheckDestination(StatusCode* status) override; // 20: 0x580b00
};

// Abstract (Read, Write and GetKind are the leaves'): an order with at most one command.
class RootCommandOrder : public OrderBase
{
public:
  RootCommandOrder(); // 0x580d30
  ~RootCommandOrder() override; // 0x580d70 (deleting 0x580d50)

  int IsChecked() override; // 5: 0x580d80
  int CheckEnabled(StatusCode* status) override; // 6: 0x580db0
  Command* CreateCommand() override; // 18: 0x580e70
};

class RootOrder260 : public RootCommandOrder
{
public:
  RootOrder260(); // 0x580ee0
  ~RootOrder260() override; // 0x580f20 (deleting 0x580f00)

  int Read(istream* in) override; // 1: 0x580fb0, shared by the four leaves
  int Write(ostream* out) override; // 2: 0x53d010, shared
  int GetKind() override
  {
    return 0x260;
  } // 3: 0x580fa0
};

class RootOrder261 : public RootCommandOrder
{
public:
  RootOrder261(); // 0x580fc0
  ~RootOrder261() override; // 0x581000 (deleting 0x580fe0)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x261;
  } // 3, shared 0x479fd0
};

class RootOrder262 : public RootCommandOrder
{
public:
  RootOrder262(); // 0x581080
  ~RootOrder262() override; // 0x5810c0 (deleting 0x5810a0)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x262;
  } // 3, shared 0x47a400
};

class RootOrder263 : public RootCommandOrder
{
public:
  RootOrder263(); // 0x581140
  ~RootOrder263() override; // 0x581180 (deleting 0x581160)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x263;
  } // 3, shared 0x47a040
};

class Order268 : public OrderBase
{
public:
  Order268(); // 0x581200
  ~Order268() override; // 0x581240 (deleting 0x581220)

  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 1, shared 0x53d0f0
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 2, shared 0x53dd90
  int GetKind() override
  {
    return 0x268;
  } // 3: 0x5812c0
  int IsListed() override
  {
    return 0;
  } // 4, shared 0x6158b0
  Command* CreateCommand() override; // 18: 0x5812d0
};

// An order that keeps its commands in a queue of its own (+0x44) and checks them against the fleets they depend on.
class Order234 : public OrderBase
{
public:
  Order234(); // 0x581340
  ~Order234() override; // 0x5813f0 (deleting 0x5813d0)

  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 1, shared 0x53d0f0
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 2, shared 0x53dd90
  int GetKind() override
  {
    return 0x234;
  } // 3: 0x5814c0
  int CheckEnabled(StatusCode* status) override; // 6: 0x5814d0
  int CheckValid(StatusCode* status) override; // 7: 0x5815f0
  int Execute(StatusCode* status) override; // 8: 0x5816f0
  Command* CreateCommand() override; // 18: 0x5817e0
  int CheckCommand(StatusCode* status) override; // 19: 0x581850
  int CheckDestination(StatusCode* status) override; // 20: 0x581a10

  CommandQueue* m_queue; // +0x44
};

class FleetCommandOrder : public OrderBase
{
public:
  FleetCommandOrder(); // 0x581d70
  ~FleetCommandOrder() override; // 0x581e20 (deleting 0x581e00)

  int CheckEnabled(StatusCode* status) override; // 6: 0x581e80
  int CheckValid(StatusCode* status) override; // 7: 0x581fa0
  int Execute(StatusCode* status) override; // 8: 0x5820a0
  Command* CreateCommand() override; // 18: 0x582190
  int CheckCommand(StatusCode* status) override; // 19: 0x582200
  int CheckDestination(StatusCode* status) override; // 20: 0x582390

  CommandQueue* m_queue; // +0x44
};

class Order221 : public FleetCommandOrder
{
public:
  Order221(); // 0x5826c0
  ~Order221() override; // 0x582700 (deleting 0x5826e0)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x221;
  } // 3: 0x582780
};

class Order223 : public FleetCommandOrder
{
public:
  Order223(); // 0x582790
  ~Order223() override; // 0x5827d0 (deleting 0x5827b0)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x223;
  } // 3: 0x582850
};

class Order222 : public FleetCommandOrder
{
public:
  Order222(); // 0x582860
  ~Order222() override; // 0x5828a0 (deleting 0x582880)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x222;
  } // 3: 0x582920
};

class Order220 : public FleetCommandOrder
{
public:
  Order220(); // 0x582930
  ~Order220() override; // 0x582970 (deleting 0x582950)

  int Read(istream* in) override; // 1, shared 0x580fb0
  int Write(ostream* out) override; // 2, shared 0x53d010
  int GetKind() override
  {
    return 0x220;
  } // 3: 0x5829f0
};

// The fleet checks of Order234 / FleetCommandOrder (stdcall: request, the order's team, status).
int CheckFleetsCanInvade(ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status); // 0x581ae0
int CheckFleetsCanBeUsed(ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status); // 0x582440

// Factories registered in the order factory table (0x51f4b0).
void* CreateOrder250(); // 0x580580
void* CreatePayloadMessage(); // 0x580950
void* CreateOrder270(); // 0x580a10
void* CreateRootOrder260(); // 0x580f30
void* CreateRootOrder261(); // 0x581010
void* CreateRootOrder262(); // 0x5810d0
void* CreateRootOrder263(); // 0x581190
void* CreateOrder268(); // 0x581250
void* CreateOrder234(); // 0x581450
void* CreateOrder221(); // 0x582710
void* CreateOrder223(); // 0x5827e0
void* CreateOrder222(); // 0x5828b0
void* CreateOrder220(); // 0x582980
