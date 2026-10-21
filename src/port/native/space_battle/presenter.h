// The live battle screen's presenter (docs/design/tactical_combat.md §11.6), for game code; SDL-free like
// port/minwin_impl/user/canvas.h. While open it owns the game window: it draws battle_ui draw lists at the full output
// resolution through the existing SDL_Renderer and turns SDL input into battle_ui events, while minwin's 640x480
// palette presentation and Win32 input are suspended. Close returns the window to the strategic game.
#pragma once
#include <string>
#include <vector>

#include "battle_ui/battle_screen.h"
#include "battle_ui/draw_list.h"
#include "battle_ui/input.h"
#include "battle_ui/sprite_catalog.h"
#include "battle_ui/text_measure.h"

namespace reb::native::space_battle
{

// A real video driver with a renderer (not the dummy driver the headless smokes use).
bool Available();
bool Open();
void Close();
bool IsOpen();

battle_ui::ScreenInfo Screen();
// Input received since the last call, in order. Events arrive while the platform is pumped: by the game's message
// loop, or by PumpEvents for a loop of its own.
std::vector<battle_ui::InputEvent> TakeInput();
void PumpEvents();
// Draws one frame and presents it (paced by vsync while open).
void Present(const battle_ui::DrawList& frame);
// One frame of a live battle screen: hands it the input received since the last frame, advances it by the real time
// since then (at most a quarter of a second, so a stall does not jump the battle), then draws and presents it.
// Returns false once the screen has asked to close. The caller keeps the platform pumped (the game's message loop or
// PumpEvents).
bool DriveFrame(battle_ui::BattleScreen& screen);
// Review scripts advance the screen without DriveFrame; they use the same audio path here.
void UpdateAudio(battle_ui::BattleScreen& screen);
// Draws a frame offscreen at the given size and saves it as a PNG; for review captures.
bool Capture(const battle_ui::DrawList& frame, int width, int height, const std::string& pngPath);
// Draws a frame offscreen and waits for the renderer to finish it; for timing.
void Render(const battle_ui::DrawList& frame, int width, int height);

// The faces the presenter draws text with; usable for layout before Open.
const battle_ui::TextMeasurer& Text();
const battle_ui::SpriteCatalog& Sprites();
// A monotonic clock in seconds, for advancing the battle in real time.
double ClockSeconds();

} // namespace reb::native::space_battle
