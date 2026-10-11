#pragma once
#include <cstdint>

#include "core/containers.h"
#include "platform/win32.h"

class DibBitmap;
class istream;
class ostream;

// A picture a CoolGameWindow draws over its background at (m_x, m_y), keyed by an id.
class BitmapOverlay : public IdNode
{
public:
  BitmapOverlay(); // 0x60f4f0
  ~BitmapOverlay() override; // 0x60f530 (scalar deleting destructor 0x60f510): deletes the bitmap

  virtual DibBitmap* GetBitmap(); // slot 1, 0x60f630
  virtual void SetBitmap(DibBitmap* bitmap); // slot 2, 0x60f5d0 (linker-folded with other +0x1c setters)
  virtual void Read(istream* in); // slot 3, 0x60f5a0: the id only; forgets the bitmap
  virtual void Write(ostream* out); // slot 4, 0x60f5c0: the id only
  // The bitmap's rectangle; `rect` is left untouched without a bitmap.
  virtual void GetRect(RECT* rect); // slot 5, 0x60f5e0

  DibBitmap* m_bitmap; // +0x1c owned
  int m_x; // +0x20
  int m_y; // +0x24
};

// The overlays of a CoolGameWindow in drawing order: by the vertical centre of their bitmaps, so lower ones
// are drawn over higher ones.
class BitmapOverlayTree : public IdAvlTree
{
public:
  BitmapOverlayTree(); // 0x60f420
  ~BitmapOverlayTree() override; // 0x60f460 (scalar deleting destructor 0x60f440)
  int IsLess(AvlNode* a, AvlNode* b) override; // 0x60f490

  // Linear search (the tree is not ordered by id).
  BitmapOverlay* FindOverlay(uint32_t id); // 0x60f470
};
