#pragma once
// Functions and classes of other modules that the application shell calls. They are declared here by their
// original addresses (the owning modules name them); the link step resolves FUN_ names to the original code.
// Drop a declaration when the owning module's header provides it.
#include <cstdint>

#include "multiplayer/message_codec.h"

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "platform/win32.h"
#include "ui/cool_window.h"
#include "ui/game_window_object.h"
#include "gfx/directdraw_host.h"

class SaveHeader;
class AppCommand;

// ---- Child windows the application creates (the owning modules reconstruct them) -------------------------

// The strategic game window (galaxy view, 0x421c70, 0x49c bytes). Only what the shell touches is declared.
class StrategicWindow : public GameWindowObject {
public:
    virtual void Virtual0B() override;  // slot 11, called by the main loop on every pass while it is the active window
    virtual void Virtual0F(void* stream);  // slot 15: save the window's state
    virtual void Virtual10(void* stream);  // slot 16: restore the window's state

    uint8_t m_pad9c[0x10];  // +0x9c
    int m_unknownAc;        // +0xac non-zero while a modal sub-window is up
    uint8_t m_padB0[0x49c - 0xb0];
};

class TacticalWindow : public GameWindowObject {
public:
    uint8_t m_pad9c[0x114 - 0x9c];
};

void* __thiscall FUN_0059a8d0(void* memory, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                              void* directDrawHost);  // tactical window constructor


int __thiscall FUN_0059cb60(TacticalWindow* window, void* stream);  // tactical window: save its state
int __thiscall FUN_0059cb80(TacticalWindow* window, void* stream);  // tactical window: restore its state

// ---- Multiplayer glue (comm2, 0x596fa0-0x597530) ------------------------------------------------------------
void FUN_00596a00();

// Setup record handed to the battle-flow object when a game is created (0x14 bytes, ctor 0x51c740, vtable 0x65ece0).
struct GameSetup {
    void* m_vftable;  // +0x00
    int m_unknown04;  // +0x04 RebellionApplication::m_param8
    int m_unknown08;  // +0x08 RebellionApplication::m_param10
    int m_networkMode;  // +0x0c
    int m_unknown10;  // +0x10 RebellionApplication::m_paramF0
};

// Serialises a command into a buffer for the other players (the multiplayer module's MessageCodec, 0x51c300).
using CommandExecutor = MessageCodec;

// The 0x38-byte command (ctor 0x4210c0, module app_control_cmds) that carries a save request to the other players.
class AppCommand {
public:
    virtual ~AppCommand();
    void SetName(String name);  // 0x416560 (this module; the argument is passed by value)

    uint8_t m_pad04[0x1c];  // +0x04
    int m_unknown20;        // +0x20
    int m_unknown24;        // +0x24
    String m_name;          // +0x28
    int m_unknown34;        // +0x34
};

// ---- Appended by the save/load slice -------------------------------------------------------------------------
extern const char* g_autosaveName;  // 0x6a81d8: pointer to the "Autosave" text

// ---- Added by the screens slice (app_screens.cpp) --------------------------------------------------------------
int DestroyAppDialog();   // 0x416d90 (app_dialogs.h declares it too)
extern DirectDrawHost g_directDrawHost;   // 0x6b1460 the DirectDrawHost passed to the tactical window
