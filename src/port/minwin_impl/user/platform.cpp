// SDL3: the one window, the event pump, presentation of the 8-bit screen surface, MessageBox.
#include <SDL3/SDL.h>

#define MINWIN_IMPLEMENTATION 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"

namespace minwin {

void ServiceMmTimers();

namespace {

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
bool g_failed = false;
bool g_everFocused = false;
bool g_outlines = false;
bool g_pumping = false;
uint32_t g_pixels[minwin_gdi::kScreenWidth * minwin_gdi::kScreenHeight];
uint32_t g_palette[256];
uint32_t g_paletteGeneration = 0xffffffffu;
unsigned g_lastPresent = 0;

int ScaleFromEnvironment() {
    const char* text = getenv("REB_MINWIN_SCALE");
    const int scale = text ? atoi(text) : 1;
    return scale >= 1 && scale <= 8 ? scale : 1;
}

bool OptionEnabled(const char* name, const char* value) {
    const char* text = getenv(name);
    return text && strstr(text, value);
}

void PostCloseRequest() {
    Window* target = ActiveWindow();
    if (!target) target = FirstTopLevel();
    if (target) Post(target, WM_SYSCOMMAND, 0xf060, 0);
}

void HandleEvent(const SDL_Event& original) {
    SDL_Event e = original;
    switch (e.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            PostCloseRequest();
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            g_everFocused = true;
            InputFocusChanged(true);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (g_everFocused) InputFocusChanged(false);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            SDL_ConvertEventToRenderCoordinates(g_renderer, &e);
            InputMouseMotion(static_cast<int>(e.motion.x), static_cast<int>(e.motion.y));
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            SDL_ConvertEventToRenderCoordinates(g_renderer, &e);
            const int button = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 3;
            InputMouseButton(button, e.button.down, static_cast<unsigned>(e.button.timestamp / 1000000u), static_cast<int>(e.button.x), static_cast<int>(e.button.y));
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            InputKey(static_cast<int>(e.key.scancode), e.key.down, e.key.repeat, 0);
            break;
        case SDL_EVENT_TEXT_INPUT:
            InputText(e.text.text);
            break;
        default:
            break;
    }
}

void DrawOutline(uint32_t* pixels, RECT r, uint32_t color) {
    r.left = r.left < 0 ? 0 : r.left;
    r.top = r.top < 0 ? 0 : r.top;
    r.right = r.right > minwin_gdi::kScreenWidth ? minwin_gdi::kScreenWidth : r.right;
    r.bottom = r.bottom > minwin_gdi::kScreenHeight ? minwin_gdi::kScreenHeight : r.bottom;
    if (IsEmpty(r)) return;
    for (int x = r.left; x < r.right; ++x) {
        pixels[r.top * minwin_gdi::kScreenWidth + x] = color;
        pixels[(r.bottom - 1) * minwin_gdi::kScreenWidth + x] = color;
    }
    for (int y = r.top; y < r.bottom; ++y) {
        pixels[y * minwin_gdi::kScreenWidth + r.left] = color;
        pixels[y * minwin_gdi::kScreenWidth + r.right - 1] = color;
    }
}

void OutlineVisitor(Window* w, void* raw) {
    static const uint32_t kColors[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xffff00ff, 0xff00ffff};
    int depth = 0;
    for (const Window* p = w->parent; p; p = p->parent) ++depth;
    DrawOutline(static_cast<uint32_t*>(raw), WindowRectOnScreen(w), kColors[depth % 6]);
}

}  // namespace

bool OutlinesEnabled() { return g_outlines; }

unsigned PlatformTicksMs() { return static_cast<unsigned>(SDL_GetTicks()); }

bool EnsurePlatform() {
    if (g_window) return true;
    if (g_failed) return false;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "minwin: SDL_Init failed: %s\n", SDL_GetError());
        g_failed = true;
        return false;
    }
    const int scale = ScaleFromEnvironment();
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Star Wars: Rebellion");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, minwin_gdi::kScreenWidth * scale);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, minwin_gdi::kScreenHeight * scale);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    g_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (g_window) g_renderer = SDL_CreateRenderer(g_window, "software");
    if (g_renderer) {
        SDL_SetRenderLogicalPresentation(g_renderer, minwin_gdi::kScreenWidth, minwin_gdi::kScreenHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, minwin_gdi::kScreenWidth,
                                      minwin_gdi::kScreenHeight);
    }
    if (!g_window || !g_renderer || !g_texture) {
        fprintf(stderr, "minwin: cannot create the window: %s\n", SDL_GetError());
        g_failed = true;
        return false;
    }
    SDL_SetTextureScaleMode(g_texture, SDL_SCALEMODE_NEAREST);
    SDL_StartTextInput(g_window);
    g_outlines = OptionEnabled("REB_MINWIN_TRACE", "outlines");
    InitInput();
    return true;
}

void ShutdownPlatform() {
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_texture = nullptr;
    g_renderer = nullptr;
    g_window = nullptr;
}

// REB_MINWIN_DUMP=<directory>: when the scenario driver leaves "dump_request" (a name) there, write <name>.idx: the 8-bit frame
// (640x480 palette indices) followed by the 256-entry palette (RGBA bytes), for tools/pixel_compare.py.
void DumpFrameOnRequest(const minwin_gdi::ScreenSurface& screen, const uint32_t* rgb) {
    static const char* directory = getenv("REB_MINWIN_DUMP");
    if (!directory) return;
    char path[512];
    snprintf(path, sizeof path, "%s/dump_request", directory);
    FILE* request = fopen(path, "rb");
    if (!request) return;
    char name[128] = {};
    size_t length = fread(name, 1, sizeof name - 1, request);
    fclose(request);
    remove(path);
    while (length && (name[length - 1] == '\n' || name[length - 1] == '\r' || name[length - 1] == ' ')) name[--length] = 0;
    snprintf(path, sizeof path, "%s/%s.idx", directory, length ? name : "frame");
    FILE* out = fopen(path, "wb");
    if (!out) return;
    for (int y = 0; y < minwin_gdi::kScreenHeight; ++y)
        fwrite(screen.bits + static_cast<size_t>(y) * static_cast<size_t>(screen.pitch), 1, minwin_gdi::kScreenWidth, out);
    for (int i = 0; i < 256; ++i) {
        const uint8_t entry[4] = {static_cast<uint8_t>(rgb[i] >> 16), static_cast<uint8_t>(rgb[i] >> 8), static_cast<uint8_t>(rgb[i]), 255};
        fwrite(entry, 1, 4, out);
    }
    fclose(out);
}

void PresentScreen() {
    if (!g_window) return;
    uint32_t generation = 0;
    uint32_t rgb[256];
    minwin_gdi::CurrentPalette(rgb, &generation);
    if (generation != g_paletteGeneration) {
        memcpy(g_palette, rgb, sizeof g_palette);
        g_paletteGeneration = generation;
    }
    const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
    DumpFrameOnRequest(screen, g_palette);
    for (int y = 0; y < minwin_gdi::kScreenHeight; ++y) {
        const uint8_t* row = screen.bits + static_cast<size_t>(y) * static_cast<size_t>(screen.pitch);
        uint32_t* out = g_pixels + y * minwin_gdi::kScreenWidth;
        for (int x = 0; x < minwin_gdi::kScreenWidth; ++x) out[x] = 0xff000000u | g_palette[row[x]];
    }
    DrawCaretOverlay(g_pixels, minwin_gdi::kScreenWidth);
    if (g_outlines) ForEachVisibleWindow(OutlineVisitor, g_pixels);
    SDL_UpdateTexture(g_texture, nullptr, g_pixels, minwin_gdi::kScreenWidth * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(g_renderer);
    SDL_RenderTexture(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

void PumpPlatform() {
    if (g_window) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) HandleEvent(e);
    }
    if (!g_pumping) {
        g_pumping = true;
        ServiceMmTimers();
        g_pumping = false;
    }
    const unsigned now = PlatformTicksMs();
    if (g_window && now - g_lastPresent >= 15) {
        g_lastPresent = now;
        PresentScreen();
    }
}

void WaitForPlatformEvent(unsigned maxMs) {
    if (!maxMs) return;
    if (g_window)
        SDL_WaitEventTimeout(nullptr, static_cast<Sint32>(maxMs));
    else
        SDL_Delay(maxMs);
}

void SetSdlTitle(const char* text) {
    if (g_window && text) SDL_SetWindowTitle(g_window, text);
}

int ShowMessageBox(const char* text, const char* caption, unsigned type) {
    SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
    if (type & 0x10) flags = SDL_MESSAGEBOX_ERROR;
    else if (type & 0x30) flags = SDL_MESSAGEBOX_WARNING;
    if (!g_window) SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowSimpleMessageBox(flags, caption ? caption : "", text ? text : "", g_window);
    return 1;
}

}  // namespace minwin
