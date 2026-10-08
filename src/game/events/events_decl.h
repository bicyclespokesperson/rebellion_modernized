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

class PostedEventQueue;  // game_manager_state.h
class TickTable;         // game_manager_state.h

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

// The three checksum sources the per-tick snapshot folds in (0x51cce0, 0x55bef0 is NotifyTables).
void FUN_0051cce0(Crc32* crc);

// Battle / follow-up code run on the game root (0x516710-0x518460).
int __thiscall FUN_00516710(GameRoot* root, ObjectKey* out);
int __thiscall FUN_00516730(GameRoot* root, int side, uint32_t* a, uint32_t* b, int* c, uint32_t* d);
int __thiscall FUN_00516d50(GameRoot* root);
int __thiscall FUN_00516ef0(GameRoot* root);
int __thiscall FUN_00517270(GameRoot* root);
int FUN_00518460();
