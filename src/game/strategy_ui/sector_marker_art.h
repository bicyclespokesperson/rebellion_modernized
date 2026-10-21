#pragma once

#include <cstdint>

class DibBitmap;

// Adds geometric 4x artwork while retaining the resource's logical pixels and hit mask.
void AddSectorMarkerDetail(DibBitmap& bitmap, uint16_t resourceId);
