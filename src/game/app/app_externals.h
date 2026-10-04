#pragma once
// Functions and classes of other modules that the application shell calls. They are declared here by their
// original addresses (the owning modules name them); the link step resolves FUN_ names to the original code.
// Drop a declaration when the owning module's header provides it.
#include <cstdint>

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "platform/win32.h"
#include "ui/cool_window.h"
#include "ui/game_window_object.h"

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

void* __thiscall FUN_00421c70(void* memory, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                              int side, int firstGame);  // strategic window constructor
void* __thiscall FUN_0059a8d0(void* memory, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                              void* directDrawHost);  // tactical window constructor
void* __thiscall FUN_0040d7a0(void* memory, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                              int a, int b, int c, int d, int e, int f, int g, int h);  // multiplayer lobby window
void* __thiscall FUN_00406950(void* memory, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                              int inGame);  // game options window
void* __thiscall FUN_00404c30(void* memory, HINSTANCE instance, int x, int y, int width, int height,
                              CoolWindow* parent);  // front-end (main menu) window
void* __thiscall FUN_004213e0(void* memory, int movieId, int arg2, HINSTANCE instance, int x, int y, int width,
                              int height, CoolWindow* parent, int arg9);  // movie window

void __thiscall FUN_004109e0(CoolWindow* multiplayerWindow);  // closes the lobby's dialogs
void __thiscall FUN_00408240(CoolWindow* optionsWindow, int a, int b);

int __thiscall FUN_0059cb60(TacticalWindow* window, void* stream);  // tactical window: save its state
int __thiscall FUN_0059cb80(TacticalWindow* window, void* stream);  // tactical window: restore its state

// ---- Multiplayer glue (comm2, 0x596fa0-0x597530) ------------------------------------------------------------
void FUN_00597460();
void FUN_00597310();
int FUN_00597090();
void FUN_00597050();
void FUN_005974c0();
int FUN_00597480();
void FUN_00597340(const char* text);
void FUN_00597360();
void FUN_00597030();
void FUN_005975a0();
void FUN_005970d0(int a, int b, int c, int d);
void FUN_00597170();
void FUN_00596a00();

// ---- Battle flow / session (0x409230-0x40a050) --------------------------------------------------------------
void* FUN_00409270();            // the battle-flow object, null before a game is created
void FUN_00409280();             // create the battle-flow object
void FUN_00409370();             // destroy it
void __thiscall FUN_00409480(void* battleFlow, void* stream);   // restore a saved game
void __thiscall FUN_004095b0(void* battleFlow, void* stream);   // write a saved game
void __thiscall FUN_00409fe0(void* battleFlow, void* setup, int gameType, int side);
void __thiscall FUN_0040a050(void* battleFlow, int* stepped);
void __thiscall FUN_00409bd0(void* battleFlow);
int __thiscall FUN_00409390(void* battleFlow);
void __thiscall FUN_004093b0(void* battleFlow, int value);

// ---- Strategic game helpers (0x41cxxx, 0x4fdxxx, 0x51cxxx) ----------------------------------------------------------
int FUN_0041dad0();
void FUN_0041dae0(int value);
int FUN_0041cdb0();
void FUN_0041d310(int side);
int FUN_0041cf80();
int FUN_0041dc10();
int FUN_0041dc30();
void FUN_0041dbf0();
void FUN_0041dbc0();
int FUN_004fd100();
int FUN_004fd120();
void FUN_004fd140(int value);
int FUN_004fd160();
int FUN_004fd180();
void FUN_004fd1a0(int value);
int FUN_004fd1c0();
int FUN_004fd1e0();
void FUN_004fd200(int value);
int FUN_004fcee0();
int FUN_00421360(WPARAM wParam);
int FUN_00421380();

// Setup record handed to the battle-flow object when a game is created (0x14 bytes, ctor 0x51c740, vtable 0x65ece0).
struct GameSetup {
    void* m_vftable;  // +0x00
    int m_unknown04;  // +0x04 RebellionApplication::m_param8
    int m_unknown08;  // +0x08 RebellionApplication::m_param10
    int m_networkMode;  // +0x0c
    int m_unknown10;  // +0x10 RebellionApplication::m_paramF0
};
GameSetup* __thiscall FUN_0051c740(GameSetup* setup);
void __thiscall FUN_0051c780(GameSetup* setup);

// Serialises a command into a buffer for the other players (0x1c bytes, ctor 0x51c300, owner: the commands module).
class CommandExecutor {
public:
    virtual ~CommandExecutor();
    int m_valid;       // +0x04
    int m_unknown08;   // +0x08
    void* m_stream;    // +0x0c
    int m_capacity;    // +0x10
    int m_size;        // +0x14 bytes written
    void* m_buffer;    // +0x18
};
void* __thiscall FUN_0051c300(void* memory);
int __thiscall FUN_0051c410(CommandExecutor* executor, AppCommand* command);

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
void* __thiscall FUN_004210c0(void* memory);

// ---- Added by the screens slice (app_screens.cpp) --------------------------------------------------------------
void FUN_00614290();   // DirectDraw host teardown wrapper (acts on the global host, no arguments)
void __thiscall FUN_00422620(CoolWindow* window);  // strategic window: handler of the modal child it is showing
extern uint8_t g_directDrawHost[];   // 0x6b1460 the DirectDrawHost passed to the tactical window
int DestroyAppDialog();   // 0x416d90 (app_dialogs.h declares it too)
