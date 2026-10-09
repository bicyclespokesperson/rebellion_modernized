// LoadCursorA for cursors stored in a module's resources (RT_GROUP_CURSOR / RT_CURSOR): decoded into an SDL colour cursor.
#include <SDL3/SDL.h>

#define MINWIN_IMPLEMENTATION 1
#include <string.h>

#include "port/minwin_impl/user/user_internal.h"

namespace {

constexpr int kGroupCursor = 12;
constexpr int kCursor = 1;

uint16_t Read16(const uint8_t* p) { uint16_t v; memcpy(&v, p, sizeof v); return v; }
uint32_t Read32(const uint8_t* p) { uint32_t v; memcpy(&v, p, sizeof v); return v; }

const uint8_t* Locate(HINSTANCE instance, LPCSTR name, int type) {
    HINSTANCE module = instance ? instance : GetModuleHandleA(nullptr);
    HRSRC resource = FindResourceA(module, name, MAKEINTRESOURCE(type));
    if (!resource) return nullptr;
    return static_cast<const uint8_t*>(LockResource(LoadResource(module, resource)));
}

// One RT_CURSOR image: hotspot, a DIB header with doubled height, colour table, XOR bitmap and the 1-bit AND mask.
SDL_Cursor* DecodeCursor(const uint8_t* data) {
    const int hotX = Read16(data), hotY = Read16(data + 2);
    const uint8_t* dib = data + 4;
    const uint32_t headerSize = Read32(dib);
    const int width = static_cast<int32_t>(Read32(dib + 4));
    const int height = static_cast<int32_t>(Read32(dib + 8)) / 2;
    const int bits = Read16(dib + 14);
    uint32_t colors = Read32(dib + 32);
    if (!colors && bits <= 8) colors = 1u << bits;
    if (width <= 0 || height <= 0 || width > 128 || height > 128) return nullptr;
    const uint8_t* palette = dib + headerSize;
    const uint8_t* xorBits = palette + colors * 4;
    const int xorPitch = ((width * bits + 31) / 32) * 4;
    const uint8_t* andBits = xorBits + xorPitch * height;
    const int andPitch = ((width + 31) / 32) * 4;

    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    uint32_t* pixels = static_cast<uint32_t*>(surface->pixels);
    for (int y = 0; y < height; ++y) {
        const int row = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0;
            const uint8_t* source = xorBits + row * xorPitch;
            int index = 0;
            if (bits == 1) index = (source[x / 8] >> (7 - x % 8)) & 1;
            else if (bits == 4) index = (source[x / 2] >> ((x & 1) ? 0 : 4)) & 15;
            else if (bits == 8) index = source[x];
            if (bits <= 8) {
                b = palette[index * 4];
                g = palette[index * 4 + 1];
                r = palette[index * 4 + 2];
            } else if (bits == 24 || bits == 32) {
                const uint8_t* p = source + x * (bits / 8);
                b = p[0]; g = p[1]; r = p[2];
            }
            const bool transparent = (andBits[row * andPitch + x / 8] >> (7 - x % 8)) & 1;
            const bool inverting = transparent && (r || g || b);
            const uint32_t alpha = transparent && !inverting ? 0u : 0xffu;
            pixels[y * (surface->pitch / 4) + x] = r | (g << 8) | (b << 16) | (alpha << 24);
        }
    }
    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotX, hotY);
    SDL_DestroySurface(surface);
    return cursor;
}

}  // namespace

SDL_Cursor* CreateCursorFromResource(HINSTANCE instance, LPCSTR name) {
    if (const uint8_t* group = Locate(instance, name, kGroupCursor)) {
        const int count = Read16(group + 4);
        if (count > 0) {
            const uint16_t id = Read16(group + 6 + 12);
            if (const uint8_t* image = Locate(instance, MAKEINTRESOURCE(id), kCursor))
                if (SDL_Cursor* cursor = DecodeCursor(image)) return cursor;
        }
    }
    return SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
}
