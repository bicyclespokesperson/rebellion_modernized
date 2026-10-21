#pragma once

// The startup code (0x51ebc0) fills the game's three factory tables once, in this order:
//   object factories   family -> GameObjNotifier* (*)()   GameObjRoot::RegisterFactory, at most 0x3c entries
//   event factories    event kind -> factory               0x54f140, table 0x6b92f8, at most 0x3fe entries
//   order factories    order kind -> factory               0x51f930, table 0x6b6fe0, at most 0x3fe entries
// The table order is the registration order and is part of the behaviour (lookups return the first match).
void RegisterGameObjFactories(); // 0x51ebf0
void RegisterEventFactories();   // 0x51ef80
void RegisterOrderFactories();   // 0x51f4b0

// A fourth, bigger table (0x6b2fd8, 0x800 entries) maps arbitrary int keys to factories; the startup code at
// 0x41e580 registers about 100 of them. Both return 0 when nothing was created / registered.
void* CreateKeyedObject(int key);                      // 0x51f730
int RegisterKeyedFactory(int key, void* (*factory)()); // 0x51f770
