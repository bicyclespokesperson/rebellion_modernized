#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Decode a Windows RLE8 stream into a tightly packed, top-down pixel array.
// Runs may end early (the remaining pixels are palette index zero); malformed
// or truncated commands return false and leave the output empty.
bool DecodeRle8(std::span<const uint8_t> encoded, int width, int height, bool topDown,
                std::vector<uint8_t>& pixels);
