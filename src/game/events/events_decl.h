#pragma once
// STAND-IN declarations for classes and functions of the events subsystem that are reconstructed elsewhere (the event
// queue base 0x54ea70, the posted-event objects 0x43a2d0-0x440ea0, the save/load helpers 0x568160-0x568fe0 and the
// battle code 0x516710-0x518460). Everything here is declared by original address; drop a declaration when the owning
// module's header provides it.
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/seed_key.h"
#include "events/event_queue.h"
#include "gameobj/object_key.h"

class Crc32;
class GameRoot;
class SidedMessage;

// The key-value queues at GameManager +0xb4/+0xb8 (ctor 0x568840, VT 0x66398c): a NodeQueue plus a flag at +0x10.
class PostedEventQueue : public NodeQueue {
public:
    int m_flag;  // +0x10, 1
};

// The table at GameManager +0xbc (ctor 0x568b40, VT 0x663990): an ordered tree of five-field keys.
class TickTable {
public:
    virtual ~TickTable();            // 0
    virtual void Clear();            // 1
    virtual int IsLess(void* a, void* b);  // 2
    virtual int Read(istream* in);   // 3
    virtual int Virtual04();         // 4
    virtual int Write(ostream* out);  // 5

    uint8_t m_pad04[0x08];  // +0x04, the tree root and a flag at +0x08 that is 1 once built
};

// An object created from the keyed factory table (0x51f730): the notification handed to the scheduler hooks. Only the
// slots and fields the game manager touches are declared. Slot 4 answers the kind (1-4).
class PostedEvent : public LinkNode {
public:
    virtual int GetFactoryKey();  // 3: the key of the keyed factory that builds this class (+0xc)
    virtual int GetKind();        // 4: 1-4, picks the handler (+0x10)
    virtual void Prepare();       // 5: +0x14, run before the event is queued

    int m_result;     // +0x0c
    int m_field10;    // +0x10
    int m_field14;    // +0x14
    int m_field18;    // +0x18
    int m_unk1c;      // +0x1c
    ObjectKey m_key20;  // +0x20
};

// Save/load helpers of the saveload module that the game manager calls (0x568160-0x568fe0).
void FUN_00568a30(istream* in);
void FUN_00568a80(ostream* out);
int FUN_00568560(istream* in);
int FUN_005685a0(ostream* out);
void FUN_00568a00();
int __thiscall FUN_005688d0(PostedEventQueue* queue, istream* in);
int __thiscall FUN_00568980(PostedEventQueue* queue, ostream* out);

// The six-field legacy block that old saves carry (0x5686a0 / 0x568770 / 0x5686e0, VT 0x663988).
struct LegacySaveBlock {
    uint8_t m_bytes[0x1c];
};
void __thiscall FUN_005686a0(LegacySaveBlock* block);
void __thiscall FUN_00568770(LegacySaveBlock* block, istream* in);
void __thiscall FUN_005686e0(LegacySaveBlock* block);

// Constructors of the objects the game manager owns, called on raw memory (the originals check allocation first).
PostedEventQueue* __thiscall FUN_00568840(void* memory);
TickTable* __thiscall FUN_00568b40(void* memory);

// The tick-table lookup and the node factory (0x568d70 / 0x568de0). Nodes are 0x30 bytes.
struct TickNode {
    uint8_t m_pad00[0x20];
    int m_a;       // +0x20
    int m_pad24;   // +0x24
    int m_c;       // +0x28
    int m_value;   // +0x2c, the hook-assigned value
};

TickNode* __thiscall FUN_00568de0(TickTable* table, SeedKey* key);
TickNode* __thiscall FUN_00568d70(TickTable* table, SeedKey* key);

// The three checksum sources the per-tick snapshot folds in (0x51cce0, 0x55bef0 is NotifyTables).
void FUN_0051cce0(Crc32* crc);

// Battle / follow-up code run on the game root (0x516710-0x518460).
int __thiscall FUN_00516710(GameRoot* root, ObjectKey* out);
int __thiscall FUN_00516730(GameRoot* root, int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d);
int __thiscall FUN_00516d50(GameRoot* root);
int __thiscall FUN_00516ef0(GameRoot* root);
int __thiscall FUN_00517270(GameRoot* root);
int FUN_00518460();
