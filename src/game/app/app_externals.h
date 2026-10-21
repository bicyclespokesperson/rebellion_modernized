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

// Serialises a command into a buffer for the other players (the multiplayer module's MessageCodec, 0x51c300).
using CommandExecutor = MessageCodec;

// ---- Appended by the save/load slice -------------------------------------------------------------------------
extern const char* g_autosaveName; // 0x6a81d8: pointer to the "Autosave" text

// ---- Added by the screens slice (app_screens.cpp) --------------------------------------------------------------
int DestroyAppDialog(); // 0x416d90 (app_dialogs.h declares it too)
