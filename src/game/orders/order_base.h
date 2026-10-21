#pragma once
// The order/message base classes (0x51f8a0-0x520570, with the middle class at 0x4f5c70).
//
// Every network message and every player order derives from MessageBase. Orders add a team (the objects the order
// is about), a target, and a queue of commands, and answer the "can this be done?" questions the UI asks before it
// submits them. The messages are created by kind through the factory table at 0x6b6fe0 (RegisterOrderFactory).
#include <cstdint>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"
#include "orders/command_decl.h"

// Base of every network/event message: an id node (the message id) plus a 32-bit flag set to 1 by the
// constructor. Kinds 1-5 and 0x10-0x15 are registered at 0x41ec50; order kinds 0x2xx at 0x51f4b0. Read/Write are pure
// virtual in the vtable but have bodies (0x51f9a0/0x51f9e0) that the derived classes call for the common part: the
// id and the flag.
class MessageBase : public IdNode
{
  public:
  MessageBase();
  ~MessageBase() override;
  virtual int Read(istream* in) = 0;
  virtual int Write(ostream* out) = 0;
  virtual int GetKind() = 0;

  uint32_t m_unk1c; // +0x1c, 1; OrderBase turns it into "owns a command queue"
};

// 0x4f5c70: a message with a side. The checking protocol (slots 4-8) is declared here and implemented by OrderBase;
// Read/Write have bodies (0x4f5e00/0x4f5e50) that derived classes call.
class SidedMessage : public MessageBase
{
  public:
  SidedMessage(); // 0x4f5c70
  ~SidedMessage() override;
  int Read(istream* in) override = 0;
  int Write(ostream* out) override = 0;

  virtual int IsListed()
  {
    return 1;
  } // 4: 0x40f340
  virtual int IsChecked()
  {
    return 0;
  } // 5: 0x6158b0
  virtual int CheckEnabled(StatusCode* status) = 0; // 6
  virtual int CheckValid(StatusCode* status) = 0;   // 7
  virtual int Execute(StatusCode* status) = 0;      // 8

  int m_side;  // +0x20
  int m_unk24; // +0x24, 1
  int m_unk28; // +0x28
};

// 0x51fa20, VT 0x65ed28.
class OrderBase : public SidedMessage
{
  public:
  OrderBase(); // 0x51fa20
  ~OrderBase() override;
  int Read(istream* in) override = 0;   // body 0x520120
  int Write(ostream* out) override = 0; // body 0x520170

  int IsChecked() override;                      // 5: 0x51fd30
  int CheckEnabled(StatusCode* status) override; // 6: 0x51fe20
  int CheckValid(StatusCode* status) override;   // 7: 0x51ff30
  int Execute(StatusCode* status) override;      // 8: 0x520040

  virtual void SetTeam(const ObjectKeyValueSet* team);         // 9: 0x51fb70
  virtual ObjectKeyValueSet* GetTeam();                        // 10: 0x51fb80
  virtual void SetTarget(const ObjectKey* target);             // 11: 0x51fbb0
  virtual ObjectKey* GetTarget(ObjectKey* out);                // 12: 0x51fbc0
  virtual ObjectKey* TargetRef();                              // 13: 0x51fbe0
  virtual void CopyTarget(ObjectKey* out);                     // 14: 0x51fbf0
  virtual int CollectTeamDependencies(ObjectKeyValueSet* out); // 15: 0x51fc00
  virtual int GetArg();                                        // 16: 0x51fba0
  virtual void SetArg(int arg);                                // 17: 0x51fb90
  virtual Command* CreateCommand() = 0;                        // 18: the command that carries this order out
  virtual int CheckCommand(StatusCode* status);                // 19: 0x5201c0
  virtual int CheckDestination(StatusCode* status);            // 20: 0x5202d0
  virtual int RequiresAllCommandsChecked()
  {
    return 1;
  } // 21: 0x40f340

  // Fills `request` from this order's side, argument, target and kind.
  void BuildRequest(ActivityRequest* request); // 0x51fcb0

  ObjectKeyValueSet m_team; // +0x2c
  ObjectKey m_target;       // +0x34
  int m_arg;                // +0x38, set to the game clock when the order executes
  int m_unk3c;              // +0x3c
  CommandQueue* m_commands; // +0x40
};

// 0x5203a0, VT 0x65ed80: an order with a text (the multiplayer chat line).
class TextMessage : public OrderBase
{
  public:
  TextMessage(); // 0x5203a0
  ~TextMessage() override;
  int Read(istream* in) override = 0;   // body 0x5204f0
  int Write(ostream* out) override = 0; // body 0x520520

  int CheckEnabled(StatusCode* status) override; // 0x520460
  int CheckValid(StatusCode* status) override;   // 0x520470
  int Execute(StatusCode* status) override;      // 0x520480
  Command* CreateCommand() override
  {
    return nullptr;
  }
  int CheckCommand(StatusCode* status) override;     // 0x520550
  int CheckDestination(StatusCode* status) override; // 0x520560

  std::string m_text; // +0x44
};

// Table of the order factories, filled by RegisterOrderFactories (0x51f4b0).
int RegisterOrderFactory(int kind, void* (*factory)()); // 0x51f930
// The factories return the class they build; the table stores plain pointers.
template <class T>
int RegisterOrderFactory(int kind, T* (*factory)())
{
  return RegisterOrderFactory(kind, reinterpret_cast<void* (*)()>(factory));
}
void* CreateOrderByKind(int kind); // 0x51f8f0, null if no factory is registered
// As CreateOrderByKind, but kinds below 0x200 other than 1, 2, 3, 5 and 0x11 are refused.
void* CreateMessageByKind(int kind); // 0x4f5cd0
