// Presents device-resolution frames while retaining the game's 640x480 logical coordinates.
// Owns display fitting, fullscreen, mouse mapping, cursor scaling, screenshots, and idle pacing.
#pragma once
#include <SDL3/SDL.h>

#include <stdint.h>

namespace minwin::present
{

bool Create(); // window, renderer, frame texture; false (with SDL_GetError set) on failure
void Destroy();
SDL_Window* Window();
SDL_Renderer* Renderer();
void Present(const uint32_t* xrgbPixels, int width, int height); // device-resolution frame; letterboxed to output
int RenderScale();                        // integer device-pixel scale for the GDI screen surface, capped at 4
int GameplayWidth();                      // logical width for the current drawable aspect at height 480
void LogicalCanvasChanged();              // refresh fitting and cursor scale after the active canvas changes
bool CaptureScreenshot(const char* path); // saves the last rendered output as a BMP
bool CaptureScreenshotPixels(const uint32_t* xrgbPixels, int width, int height, const char* path); // saves pixels as a BMP
bool RequestScreenshot(); // saves the next rendered frame to REB_SCREENSHOT_PATH or a unique user-data path

// Window coordinates (SDL mouse events) -> game pixels, floor-rounded and clamped to the screen.
void MapMouse(float windowX, float windowY, int* x, int* y);

// Window events that concern the presentation (resize, display change, fullscreen). True when the frame should be drawn
// again now.
bool HandleWindowEvent(const SDL_Event& event);
void ToggleFullscreen();

// Integer scale of the frame in window units (the unit SDL cursors use); the game's cursors are enlarged by it.
int CursorScale();
// SDL cursor from an RGBA image enlarged to the current scale (nearest neighbour), hot spot scaled with it.
SDL_Cursor* CreateScaledCursor(const uint8_t* rgba, int width, int height, int hotX, int hotY);

// The game captured the mouse (SetCapture): keep receiving motion and the release outside the window.
void SyncMouseCapture(bool captured);

// Called when the main loop found nothing to do: sleeps so the loop does not spin a core. No-op with
// REB_DETERMINISTIC=1.
void PaceIdleLoop();

} // namespace minwin::present
