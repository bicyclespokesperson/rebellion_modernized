#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "platform/win32.h"

class DibBitmap;

// One frame of a sprite animation stored as a difference to the previous frame: custom resource type 302 in the
// sprite DLLs (ALSPRITE/EMSPRITE, ALBRIEF/EMBRIEF). The resource is
//   u16 width, u16 height, u32 dataSize, u8 flag, i16 left, top, right, bottom,
//   u32 rowOffsets[height] (into the data), u8 data[dataSize]
// and each row is a run list that alternates a skip count with a literal count followed by that many bytes, starting
// with a skip. Applying a frame adds the literal bytes to the pixels (the frame holds palette-index deltas).
// The animation resource classes (0x404440, 0x412c40) keep the frames by frame number.
class DeltaFrame : public IdNode
{
  public:
  DeltaFrame(HMODULE module, uint32_t resourceId); // 0x41c590
  ~DeltaFrame() override;                          // 0x41c610 (scalar deleting destructor 0x41c570)

  // Forgets the current frame, then loads resource `resourceId` (low 16 bits) of type 302 from `module`.
  void Load(HMODULE module, uint32_t resourceId); // 0x41c680
  // Copies the frame out of the resource (which it frees) and computes its bounds.
  void Parse(HGLOBAL resource); // 0x41c6c0
  // Adds the frame to `target`'s pixels. Fails (0) for an RLE target, or a loaded target that is not 8-bit or
  // not the frame's size.
  int ApplyTo(DibBitmap* target); // 0x41c7a0
  void Reset();                   // 0x41c8f0
  // The rectangle the frame changes: columns [m_left, m_right), rows counted from the bottom.
  void ComputeBounds(); // 0x41c930
  // Stores the bounds in `rect` (`replace` non-zero) or grows `rect` to include them.
  void GetBounds(RECT* rect, int replace); // 0x41ca10

  int16_t m_width;     // +0x1c
  int16_t m_height;    // +0x1e
  int16_t m_left;      // +0x20
  int16_t m_top;       // +0x22
  int16_t m_right;     // +0x24
  int16_t m_bottom;    // +0x26
  uint32_t m_dataSize; // +0x28
  uint8_t m_flag;      // +0x2c
  uint8_t m_pad2d[3];  // +0x2d
  uint8_t** m_rows;    // +0x30 m_height pointers into m_data
  uint8_t* m_data;     // +0x34
  uint32_t m_unk38;    // +0x38 zero
};

// How a Sprite is drawn (Sprite::m_drawMode).
enum SpriteDrawMode : uint16_t {
  kSpriteKeyed = 1,            // colour-keyed copy (the default)
  kSpriteRemapDestination = 2, // DibBitmap::BlitRemapDestination with m_remapTable
  kSpriteRemapped = 3,         // DibBitmap::BlitRemapped with m_remapTable
  kSpriteHidden = 4,
  kSpriteRemappedOverlay = 5, // DibBitmap::BlitRemappedOverlay with m_remapTable
};

// A bitmap placed at (m_x, m_y) on a SpriteStage, which hands out m_id.
class Sprite : public AvlNode
{
  public:
  explicit Sprite(DibBitmap* bitmap);        // 0x41ca70
  ~Sprite() override;                        // 0x41cad0 (scalar deleting destructor 0x41cab0)
  virtual DibBitmap* GetBitmap();            // slot 1, 0x525f20 (linker-folded +0x2c getter)
  virtual void SetBitmap(DibBitmap* bitmap); // slot 2, 0x41cae0 (linker-folded +0x2c setter)

  // Whether the pixel at stage position (x, y) is opaque.
  int HitTest(int x, int y); // 0x41caf0
  // Draws the sprite into `dest` with the stage scrolled to (originX, originY); `drawn` receives the
  // rectangle covered. Nothing is drawn (0) when the sprite's top-left corner is outside `dest`, or it is hidden
  // or has no bitmap.
  int DrawOnto(DibBitmap* dest, int originX, int originY, RECT* drawn); // 0x41cb30

  uint32_t m_id;         // +0x18
  int m_x;               // +0x1c
  int m_y;               // +0x20
  int m_depth;           // +0x24 drawing order on the stage (SpriteTree)
  int m_unk28;           // +0x28
  DibBitmap* m_bitmap;   // +0x2c not owned
  uint8_t* m_remapTable; // +0x30 optional remap table, initially null
  uint16_t m_drawMode;   // +0x34 SpriteDrawMode
  uint16_t m_pad36;      // +0x36
};
