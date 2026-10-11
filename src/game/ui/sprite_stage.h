#pragma once
#include <cstdint>
#include <memory>
#include <set>

#include "core/id_registry.h"
#include "platform/win32.h"

class CoolWindow;
class DibBitmap;
class Sprite;
class SpriteAnimator;

// The sprites of a SpriteStage in drawing order (by Sprite::m_depth); hands out sprite ids.
class SpriteTree
{
public:
  SpriteTree(); // 0x4aa7f0
  ~SpriteTree(); // 0x4aa830 (scalar deleting destructor 0x4aa810)
  SpriteTree(const SpriteTree&) = delete;
  SpriteTree& operator=(const SpriteTree&) = delete;

  // Gives the sprite the next id unless it has one, then inserts it.
  void Add(Sprite* sprite); // 0x4aa840
  Sprite* FindById(uint32_t id); // 0x4aa870: linear search
  void Remove(Sprite* sprite);
  void Clear();
  const auto& Entries() const
  {
    return m_sprites;
  }

private:
  struct SpriteDepthLess
  {
    bool operator()(const std::unique_ptr<Sprite>& first, const std::unique_ptr<Sprite>& second) const;
  };
  std::multiset<std::unique_ptr<Sprite>, SpriteDepthLess> m_sprites;
  uint32_t m_nextId; // +0x08
};

// The animators of a SpriteStage, by id.
class AnimatorTree
{
public:
  AnimatorTree(); // 0x4aa8c0
  ~AnimatorTree(); // 0x4aa900 (scalar deleting destructor 0x4aa8e0)
  AnimatorTree(const AnimatorTree&) = delete;
  AnimatorTree& operator=(const AnimatorTree&) = delete;
  uint32_t AddWithId(SpriteAnimator* animator);
  SpriteAnimator* FindById(uint32_t id) const;
  void Remove(SpriteAnimator* animator);
  void Clear();
  auto Entries() const
  {
    return m_animators.Entries();
  }

private:
  IdRegistry<SpriteAnimator> m_animators;
};

// A double-buffered animation area inside a window (the galaxy view's advisor/droid animations, 0x42adb0): each
// frame the animators move their sprites, the sprites are drawn into the back buffer and the buffers are
// swapped. Frames are counted in wall-clock time (timeGetTime) in units of m_frameMs.
class SpriteStage
{
public:
  // A width or height below 1 extends the stage to the window's right or bottom edge.
  SpriteStage(CoolWindow* window, DibBitmap* format, int x, int y, int width, int height,
              uint32_t frameMs); // 0x441a60
  ~SpriteStage(); // 0x441bf0 (not virtual)

  virtual void Draw(DibBitmap* dest); // slot 0, 0x441cc0: DrawAt(m_x, m_y)
  virtual void DrawAt(DibBitmap* dest, int x, int y); // slot 1, 0x441c90: the front buffer, colour-keyed
  // Renders the current frame if the clock moved on; 1 if it did.
  virtual int Tick(); // slot 2, 0x441d70
  virtual void Render(int frame); // slot 3, 0x441dc0
  virtual void Pause(); // slot 4, 0x441ce0
  virtual void Resume(); // slot 5, 0x441cf0
  virtual void Disable(); // slot 6, 0x441d50
  virtual void Enable(); // slot 7, 0x441d60
  // Re-reads the clock and interrupts a Render in progress.
  virtual void Resync(); // slot 8, 0x441d20
  virtual uint32_t AddAnimator(SpriteAnimator* animator); // slot 9, 0x441f30: returns its id
  virtual int RemoveAnimator(SpriteAnimator* animator); // slot 10, 0x441f50: not deleted
  virtual uint32_t AddSprite(Sprite* sprite); // slot 11, 0x441f90: returns its id
  virtual int RemoveSprite(uint32_t id); // slot 12, 0x441fb0: deletes it
  virtual void Scroll(int dx, int dy); // slot 13, 0x441ff0
  virtual void StartAnimator(uint32_t id, void* sequence, int a, int b, int c); // slot 14, 0x442070
  virtual void StopAnimator(uint32_t id); // slot 15, 0x442110

  // Id of the first animator with an opaque sprite pixel at window position (x, y), or 0.
  uint32_t HitTest(int x, int y); // 0x4420b0

  DibBitmap* BackBuffer()
  {
    return m_buffers[1 - m_front];
  }

  DibBitmap* m_buffers[2]; // +0x04 DIB sections; m_buffers[m_front] is shown
  int m_front; // +0x0c
  CoolWindow* m_window; // +0x10
  HWND m_hwnd; // +0x14
  AnimatorTree m_animators; // +0x18
  SpriteTree m_sprites; // +0x24
  int m_frame; // +0x30 last rendered frame, -2 at first
  uint32_t m_frameMs; // +0x34
  DWORD m_startTime; // +0x38
  int m_paused; // +0x3c
  int m_disabled; // +0x40 set by the constructor
  int m_resynced; // +0x44 not initialised by the constructor
  int m_originX; // +0x48 scroll position
  int m_originY; // +0x4c
  int m_x; // +0x50 position in the window
  int m_y; // +0x54
  RECT m_rect; // +0x58
  int m_alive; // +0x68 cleared by the destructor
};
