#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"

// A saved UI record (a pending player action) of the human player's order tree: an id node that knows how to read
// and write itself and which kind of record it is (the index of its handler in g_uiHandlers).
class UiRecord : public IdNode {
public:
    virtual void Read(istream* in);    // slot 1
    virtual void Write(ostream* out);  // slot 2
    virtual int GetKind();             // slot 3
};

// One singleton per record kind (the player-action modules 0x48aac0-0x49bda0 own the classes). Only the factory
// slot is used here.
class UiHandler {
public:
    virtual ~UiHandler();
    virtual void Virtual01();
    virtual void Virtual02();
    virtual void Virtual03();
    virtual void Virtual04();
    virtual void Virtual05();
    virtual void Virtual06();
    virtual void Virtual07();
    virtual void Virtual08();
    virtual void Virtual09();
    virtual UiRecord* CreateRecord();  // slot 10: a new, empty record of this kind
};

// The tree of saved UI records. Its constructor fills g_uiHandlers the first time any tree is built: every handler
// singleton is constructed (once, guarded by a bit in its guard byte) and entered under its kind.
class UiRecordTree : public IdAvlTree {
public:
    UiRecordTree();           // 0x436a40
    virtual ~UiRecordTree();  // 0x437690 (scalar deleting destructor 0x437670)
    virtual void Read(istream* in);    // slot 3, 0x4376a0: 16-bit count, then kind and body of every record
    virtual void Write(ostream* out);  // slot 4, 0x437720
};

// The new, empty record of `kind` (0x437780: calls the handler's CreateRecord).
UiRecord* CreateUiRecord(int kind);  // 0x437780

// Static initializers of three UI globals (a zeroed pointer each, destroyed at exit).
void StaticInitGlobalA();  // 0x437790
void InitGlobalA();        // 0x4377a0
void RegisterGlobalADestructor();  // 0x4377b0
void StaticInitGlobalB();  // 0x4377e0
void InitGlobalB();        // 0x4377f0
void RegisterGlobalBDestructor();  // 0x437800
void StaticInitGlobalC();  // 0x437830
void InitGlobalC();        // 0x437840
void RegisterGlobalCDestructor();  // 0x437850

constexpr int kUiHandlerCount = 0x2e;
extern UiHandler* g_uiHandlers[kUiHandlerCount];  // 0x6b1f40
