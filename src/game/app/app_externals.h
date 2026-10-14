#pragma once
// Application shell declarations without a dedicated owning header.
#include <cstdint>

#include "multiplayer/message_codec.h"

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "gfx/directdraw_host.h"
#include "platform/win32.h"
#include "ui/cool_window.h"
#include "ui/game_window_object.h"

class SaveHeader;

// ---- Child windows the application creates (the owning modules reconstruct them) -------------------------

void* FUN_0059a8d0(void* memory,
                   HINSTANCE instance,
                   int x,
                   int y,
                   int width,
                   int height,
                   CoolWindow* parent,
                   void* directDrawHost); // tactical window constructor

#ifndef REB_NO_TACTICAL_3D
class TacticalWindow : public GameWindowObject
{
public:
  uint8_t m_pad9c[0x114 - 0x9c];
};
int FUN_0059cb60(TacticalWindow* window, void* stream); // tactical window: save its state
int FUN_0059cb80(TacticalWindow* window, void* stream); // tactical window: restore its state
#endif

// ---- Multiplayer glue (comm2, 0x596fa0-0x597530) ------------------------------------------------------------
void FUN_00596a00();

// Serialises a command into a buffer for the other players (the multiplayer module's MessageCodec, 0x51c300).
using CommandExecutor = MessageCodec;

// ---- Appended by the save/load slice -------------------------------------------------------------------------
extern const char* g_autosaveName; // 0x6a81d8: pointer to the "Autosave" text

// ---- Added by the screens slice (app_screens.cpp) --------------------------------------------------------------
int DestroyAppDialog(); // 0x416d90 (app_dialogs.h declares it too)
