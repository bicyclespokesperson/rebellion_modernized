#pragma once
// STAND-IN declarations for the code the source_events classes call (modules owned elsewhere); drop a
// declaration when the owning module's header provides it.
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/object_key.h"

int ApplyEvent301(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int ApplyEvent31d(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int ApplyEvent300(ObjectKey*, ObjectKey*, ObjectKey*, int, NotifyContext*);
int ApplyEvent308(ObjectKey*, int, NotifyContext*);
int ApplyEvent306(ObjectKey*, int, NotifyContext*);
int ApplyEvent316(ObjectKey*, int, NotifyContext*);
int ApplyEvent307(ObjectKey*, int, NotifyContext*);
int ApplyEvent302(ObjectKey*, int, int, NotifyContext*);
int ApplyEvent318(ObjectKey*, int, int, NotifyContext*);
int ApplyEvent315(ObjectKey*, int, int, NotifyContext*);
int ApplyEvent30a(ObjectKey*, int, int, NotifyContext*);
int ApplyEvent320(ObjectKey*, ObjectKey*, ObjectKey*, NotifyContext*);
