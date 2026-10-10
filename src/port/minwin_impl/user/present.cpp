// Presentation of the finished frame: see present.h.
#include "port/minwin_impl/user/present.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "port/minwin_impl/config/config.h"
#include "port/minwin_impl/gdi_api.h"

namespace minwin {
void RefreshCursor();  // misc.cpp: re-applies the current cursor (it is rebuilt at the new scale)
}

namespace minwin::present {
namespace {

constexpr int kWidth = minwin_gdi::kScreenWidth;
constexpr int kHeight = minwin_gdi::kScreenHeight;
constexpr int kMaxScale = 8;
constexpr int kDecorationReservePx = 48;  // title bar and borders that the display's usable area does not account for

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
int g_cursorScale = 1;
bool g_mouseCaptured = false;

bool VirtualClockRequested() {
    const char* text = getenv("REB_DETERMINISTIC");
    return text && atoi(text) != 0;
}

int ClampScale(int scale) { return scale < 1 ? 1 : (scale > kMaxScale ? kMaxScale : scale); }

int LargestScaleThatFits() {
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) || usable.w <= 0 || usable.h <= 0) return 1;
    const int byWidth = usable.w / kWidth;
    const int byHeight = (usable.h - kDecorationReservePx) / kHeight;
    return ClampScale(byWidth < byHeight ? byWidth : byHeight);
}

int RequestedScale() {
    if (const char* text = getenv("REB_MINWIN_SCALE")) {
        if (*text) return ClampScale(atoi(text));
    }
    const int configured = mw::config::Global().WindowScale();
    return configured > 0 ? ClampScale(configured) : LargestScaleThatFits();
}

bool FullscreenRequested() {
    if (const char* text = getenv("REB_MINWIN_FULLSCREEN")) {
        if (*text) return atoi(text) != 0;
    }
    return atoi(mw::config::Global().Get("fullscreen", "0").c_str()) != 0;
}

SDL_Renderer* CreateRenderer(SDL_Window* window) {
#if defined(_WIN32)
    return SDL_CreateRenderer(window, "software");
#else
    const std::string wanted = mw::config::Global().Get("renderer");
    SDL_Renderer* renderer = SDL_CreateRenderer(window, wanted.empty() ? nullptr : wanted.c_str());
    return renderer ? renderer : SDL_CreateRenderer(window, "software");
#endif
}

int ComputeCursorScale() {
    if (!g_renderer || !g_window) return 1;
    SDL_FRect rect;
    if (!SDL_GetRenderLogicalPresentationRect(g_renderer, &rect) || rect.w <= 0) return 1;
    const float density = SDL_GetWindowPixelDensity(g_window);
    const float inWindowUnits = rect.w / static_cast<float>(kWidth) / (density > 0 ? density : 1.0f);
    const int scale = static_cast<int>(floorf(inWindowUnits + 0.5f));
    return scale < 1 ? 1 : scale;
}

bool UpdateCursorScale() {
    const int scale = ComputeCursorScale();
    if (scale == g_cursorScale) return false;
    g_cursorScale = scale;
    RefreshCursor();
    return true;
}

std::vector<uint8_t> Enlarge(const uint8_t* rgba, int width, int height, int factor) {
    std::vector<uint8_t> out(static_cast<size_t>(width) * factor * height * factor * 4);
    for (int y = 0; y < height * factor; ++y)
        for (int x = 0; x < width * factor; ++x)
            memcpy(&out[(static_cast<size_t>(y) * width * factor + x) * 4], rgba + (static_cast<size_t>(y / factor) * width + x / factor) * 4, 4);
    return out;
}

}  // namespace

SDL_Window* Window() { return g_window; }

bool Create() {
    const int scale = RequestedScale();
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Star Wars: Rebellion");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, kWidth * scale);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, kHeight * scale);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, FullscreenRequested());
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    g_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (g_window) {
        SDL_SetWindowMinimumSize(g_window, kWidth, kHeight);
        g_renderer = CreateRenderer(g_window);
    }
    if (g_renderer) {
        SDL_SetRenderLogicalPresentation(g_renderer, kWidth, kHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
        SDL_SetRenderVSync(g_renderer, atoi(mw::config::Global().Get("vsync", "0").c_str()) != 0 ? 1 : 0);
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
    }
    if (!g_window || !g_renderer || !g_texture) return false;
    SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
    g_cursorScale = ComputeCursorScale();
    if (getenv("REB_MINWIN_TRACE"))
        fprintf(stderr, "minwin: window scale %d, cursor scale %d, renderer %s\n", scale, g_cursorScale, SDL_GetRendererName(g_renderer));
    return true;
}

void Destroy() {
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_texture = nullptr;
    g_renderer = nullptr;
    g_window = nullptr;
}

void Present(const uint32_t* pixels) {
    if (!g_window || (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MINIMIZED)) return;
    SDL_UpdateTexture(g_texture, nullptr, pixels, kWidth * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

void MapMouse(float windowX, float windowY, int* x, int* y) {
    float logicalX = windowX, logicalY = windowY;
    if (g_renderer) SDL_RenderCoordinatesFromWindow(g_renderer, windowX, windowY, &logicalX, &logicalY);
    const int ix = static_cast<int>(floorf(logicalX)), iy = static_cast<int>(floorf(logicalY));
    *x = ix < 0 ? 0 : (ix >= kWidth ? kWidth - 1 : ix);
    *y = iy < 0 ? 0 : (iy >= kHeight ? kHeight - 1 : iy);
}

bool HandleWindowEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
        case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            UpdateCursorScale();
            return true;
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_RESTORED:
            return true;
        default:
            return false;
    }
}

void ToggleFullscreen() {
    if (!g_window) return;
    const bool fullscreen = (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(g_window, !fullscreen);
}

int CursorScale() { return g_cursorScale; }

SDL_Cursor* CreateScaledCursor(const uint8_t* rgba, int width, int height, int hotX, int hotY) {
    const int scale = g_cursorScale;
    const int density = g_window ? static_cast<int>(floorf(SDL_GetWindowPixelDensity(g_window) + 0.5f)) : 1;
    std::vector<uint8_t> base = Enlarge(rgba, width, height, scale);
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width * scale, height * scale, SDL_PIXELFORMAT_RGBA32, base.data(), width * scale * 4);
    if (!surface) return nullptr;
    std::vector<uint8_t> sharp;
    SDL_Surface* alternate = nullptr;
    if (density > 1) {
        sharp = Enlarge(rgba, width, height, scale * density);
        alternate = SDL_CreateSurfaceFrom(width * scale * density, height * scale * density, SDL_PIXELFORMAT_RGBA32, sharp.data(),
                                          width * scale * density * 4);
        if (alternate) SDL_AddSurfaceAlternateImage(surface, alternate);
    }
    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX * scale, hotY * scale);
    if (alternate) SDL_DestroySurface(alternate);
    SDL_DestroySurface(surface);
    return cursor;
}

void SyncMouseCapture(bool captured) {
    if (captured == g_mouseCaptured) return;
    g_mouseCaptured = captured;
    SDL_CaptureMouse(captured);
}

void PaceIdleLoop() {
    static const uint64_t periodNs = [] {
        if (VirtualClockRequested()) return uint64_t{0};
        const char* text = getenv("REB_MINWIN_PACING_US");
        return (text && *text ? static_cast<uint64_t>(atoll(text)) : uint64_t{250}) * 1000;
    }();
    static uint64_t last = 0;
    if (!periodNs) return;
    const uint64_t now = SDL_GetTicksNS();
    if (now < last + periodNs) SDL_DelayNS(last + periodNs - now);
    last = SDL_GetTicksNS();
}

}  // namespace minwin::present
