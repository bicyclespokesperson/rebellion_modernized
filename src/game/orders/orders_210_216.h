#pragma once
class ObjectNameList;
// Orders 0x210-0x216 (0x536eb0-0x539290 minus the message tree, see order_list.h), registered by RegisterOrderFactories
// (0x51f4b0):
//
//   0x214  DestinationOrder   where the units built by manufacturing managers go; issued by dragging a facility list
//                             item onto a system window. Extends OrderBase directly.
//   0x215  RenameOrder215        a text message that names the team's single object.
//   0x210-0x212  ManufactureOrder*   an item type (+0x44) and a count (+0x48) that the team's managers produce and
//                             deliver to the order's target. Three kinds share one base class (0x6610f0, abstract).
//   0x213  Order213, 0x216  Order216   orders with no data of their own; 0x216 hands every command the negation of
//                             IsChecked().
//
// Every concrete order inherits Read/Write unchanged: the original has trivial forwarding overrides (shared by
// identical code folding, e.g. 0x53d0f0/0x53dd90), which these classes reproduce inline.
#include <cstdint>

#include "gameobj/object_key.h"
#include "orders/command_decl.h"
#include "orders/order_base.h"

// 0x214: vtable 0x661040, size 0x44.
class DestinationOrder : public OrderBase
{
  public:
  DestinationOrder();           // 0x537040
  ~DestinationOrder() override; // 0x537080 (scalar deleting destructor 0x537060)
  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 0x53d0f0 (shared)
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 0x53dd90 (shared)
  int GetKind() override;                            // 0x537100
  Command* CreateCommand() override;                 // 0x537110
  int CheckDestination(StatusCode* status) override; // 0x537180
};

void* CreateDestinationOrder(); // 0x537090

// 0x215: vtable 0x661098, size 0x50.
class RenameOrder215 : public TextMessage
{
  public:
  RenameOrder215();           // 0x537ba0
  ~RenameOrder215() override; // 0x537be0 (scalar deleting destructor 0x537bc0)
  int Read(istream* in) override
  {
    return TextMessage::Read(in);
  } // 0x53da80
  int Write(ostream* out) override
  {
    return TextMessage::Write(out);
  } // 0x53da90
  int GetKind() override; // 0x537c60
  // Refused (status {1,0x15}) when the order has more than one command.
  int CheckEnabled(StatusCode* status) override; // 0x537c70
  Command* CreateCommand() override;             // 0x537d30
  // Copies the text into every command.
  int CheckDestination(StatusCode* status) override; // 0x537da0
};

void* CreateRenameOrder215(); // 0x537bf0

// 0x6610f0, size 0x4c: the common part of the manufacture orders. +0x44 names the item type, +0x48 is how many.
class ManufactureOrderBase : public OrderBase
{
  public:
  ManufactureOrderBase();                            // 0x537ea0
  ~ManufactureOrderBase() override;                  // 0x537f20 (scalar deleting destructor 0x537f00)
  int Read(istream* in) override = 0;                // body 0x537f70
  int Write(ostream* out) override = 0;              // body 0x537fb0
  Command* CreateCommand() override;                 // 0x538580
  int CheckDestination(StatusCode* status) override; // 0x5385f0

  // Called by the manufacture dialog (0x437880). Empties `choices`, then, when the team's managers all belong to one
  // side, fills it with what that side can offer for the first manager's item kind (side object's 0x52e580).
  // Returns 0 when the team is not made of managers of one side.
  uint32_t FillChoices(ObjectNameList* choices); // 0x537ff0
  // Called by the manufacture dialog (0x439160). Totals for the item: out1 = record field 0x4c x count, out2 = field
  // 0x48 x count; out3/out4 = the largest per-manager figures from 0x555f00 and 0x555f90. Zero when the order is
  // refused.
  int ComputeTotals(int* out1, int* out2, int* out3, int* out4, StatusCode* status); // 0x538220
  // The item key as an ObjectKey (0x451110, shared by every class that keeps a key at +0x44).
  ObjectKey* GetItemKey(ObjectKey* out)
  {
    out->m_value = m_item.m_value;
    return out;
  }

  NullableKey m_item; // +0x44
  int m_count;        // +0x48
};

// 0x210/0x211/0x212: vtables 0x661148 / 0x6611a0 / 0x6611f8, size 0x4c. Read/Write forward to the base
// (0x538ee0/0x538ef0, shared by the three).
class ManufactureOrder210 : public ManufactureOrderBase
{
  public:
  ManufactureOrder210();           // 0x538c70
  ~ManufactureOrder210() override; // 0x538cb0 (scalar deleting destructor 0x538c90)
  int Read(istream* in) override
  {
    return ManufactureOrderBase::Read(in);
  } // 0x538ee0
  int Write(ostream* out) override
  {
    return ManufactureOrderBase::Write(out);
  } // 0x538ef0
  int GetKind() override; // 0x538d30
};

class ManufactureOrder211 : public ManufactureOrderBase
{
  public:
  ManufactureOrder211();           // 0x538d40
  ~ManufactureOrder211() override; // 0x538d80 (scalar deleting destructor 0x538d60)
  int Read(istream* in) override
  {
    return ManufactureOrderBase::Read(in);
  }
  int Write(ostream* out) override
  {
    return ManufactureOrderBase::Write(out);
  }
  int GetKind() override; // 0x538e00
};

class ManufactureOrder212 : public ManufactureOrderBase
{
  public:
  ManufactureOrder212();           // 0x538e10
  ~ManufactureOrder212() override; // 0x538e50 (scalar deleting destructor 0x538e30)
  int Read(istream* in) override
  {
    return ManufactureOrderBase::Read(in);
  }
  int Write(ostream* out) override
  {
    return ManufactureOrderBase::Write(out);
  }
  int GetKind() override; // 0x538ed0
};

void* CreateManufactureOrder210(); // 0x538cc0
void* CreateManufactureOrder211(); // 0x538d90
void* CreateManufactureOrder212(); // 0x538e60

// 0x213: vtable 0x661250, size 0x44.
class Order213 : public OrderBase
{
  public:
  Order213();           // 0x538f00
  ~Order213() override; // 0x538f40 (scalar deleting destructor 0x538f20)
  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 0x53d0f0 (shared)
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 0x53dd90 (shared)
  int GetKind() override;            // 0x538fc0
  Command* CreateCommand() override; // 0x538fd0
};

void* CreateOrder213(); // 0x538f50

// 0x216: vtable 0x6612a8, size 0x44.
class Order216 : public OrderBase
{
  public:
  Order216();           // 0x539040
  ~Order216() override; // 0x539080 (scalar deleting destructor 0x539060)
  int Read(istream* in) override
  {
    return OrderBase::Read(in);
  } // 0x53d0f0 (shared)
  int Write(ostream* out) override
  {
    return OrderBase::Write(out);
  } // 0x53dd90 (shared)
  int GetKind() override;                            // 0x539100
  Command* CreateCommand() override;                 // 0x539110
  int CheckDestination(StatusCode* status) override; // 0x539180
  int RequiresAllCommandsChecked() override
  {
    return 0;
  } // 0x6158b0 (shared)
};

void* CreateOrder216(); // 0x539090
