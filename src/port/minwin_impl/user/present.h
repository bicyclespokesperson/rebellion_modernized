// How the finished 640x480 frame reaches the screen: the SDL window and renderer, integer scaling, fullscreen, the mapping of
// window coordinates back to game pixels, the scale the game's own cursors are drawn at, and the idle pacing of the main loop.
// Settings: config keys window_scale and fullscreen (src/port/minwin_impl/config/config.h), environment REB_MINWIN_SCALE,
// REB_MINWIN_PACING_US (docs/native.md).
#pragma once
#include <SDL3/SDL.h>

#include <stdint.h>

namespace minwin::present {

bool Create();                                  // window, renderer, frame texture; false (with SDL_GetError set) on failure
void Destroy();
SDL_Window* Window();
void Present(const uint32_t* xrgbPixels);       // the 640x480 frame; letterboxed at an integer scale

// Window coordinates (SDL mouse events) -> game pixels, floor-rounded and clamped to the screen.
void MapMouse(float windowX, float windowY, int* x, int* y);

// Window events that concern the presentation (resize, display change, fullscreen). True when the frame should be drawn again now.
bool HandleWindowEvent(const SDL_Event& event);
void ToggleFullscreen();

// Integer scale of the frame in window units (the unit SDL cursors use); the game's cursors are enlarged by it.
int CursorScale();
// SDL cursor from an RGBA image enlarged to the current scale (nearest neighbour), hot spot scaled with it.
SDL_Cursor* CreateScaledCursor(const uint8_t* rgba, int width, int height, int hotX, int hotY);

// The game captured the mouse (SetCapture): keep receiving motion and the release outside the window.
void SyncMouseCapture(bool captured);

// Called when the main loop found nothing to do: sleeps so the loop does not spin a core. No-op with REB_DETERMINISTIC=1.
void PaceIdleLoop();

}  // namespace minwin::present
