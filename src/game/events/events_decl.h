#pragma once
// STAND-IN declarations for classes and functions of the events subsystem that are reconstructed elsewhere (the event
// queue base 0x54ea70, the posted-event objects 0x43a2d0-0x440ea0, the save/load helpers 0x568160-0x568fe0 and the
// battle code 0x516710-0x518460). Everything here is declared by original address; drop a declaration when the owning
// module's header provides it.
#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/seed_key.h"
#include "events/event_queue.h"
#include "gameobj/notif_payload.h"
#include "gameobj/object_key.h"

class Crc32;
class GameRoot;
class SidedMessage;

class PostedEventQueue; // game_manager_state.h
class TickTable; // game_manager_state.h

// Battle / follow-up code run on the game root (0x516710-0x518460).
int PublishVictoryState();
