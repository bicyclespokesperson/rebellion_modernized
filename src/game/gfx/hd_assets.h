#pragma once

#include <cstdint>
#include <span>

class DibBitmap;
void LoadHighResolutionAsset(DibBitmap& bitmap, std::span<const uint8_t> originalDib);
