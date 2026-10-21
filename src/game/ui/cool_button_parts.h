#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "platform/win32.h"
#include "ui/cool_timer.h"

class CoolStrobeButton;
class DibBitmap;

// Auto-repeat of a held CoolStrobeButton: a periodic CoolTimer that calls the button's OnRepeatTimer.
class ButtonRepeatTimer : public CoolTimer
{
  public:
  // A delay below 1 creates the timer paused.
  ButtonRepeatTimer(int delay, CoolStrobeButton* button); // 0x60f1e0
  ~ButtonRepeatTimer() override;                          // 0x60f270 (scalar deleting destructor 0x60f250)
  void OnTick() override;                                 // 0x60f2c0

  CoolStrobeButton* m_button; // +0x38
};

// One animation frame of a strobe button; its bitmap is owned exclusively by the frame.
class BitmapFrame
{
  public:
  explicit BitmapFrame(std::unique_ptr<DibBitmap> bitmap);
  ~BitmapFrame();
  BitmapFrame(const BitmapFrame&) = delete;
  BitmapFrame& operator=(const BitmapFrame&) = delete;
  DibBitmap* Bitmap() const;

  private:
  std::unique_ptr<DibBitmap> m_bitmap;
};

// Owns stable heap-allocated frames; pointers returned by First/Next are borrowed until Clear/destruction.
class BitmapFrameList
{
  public:
  BitmapFrameList() = default;
  BitmapFrameList(const BitmapFrameList&) = delete;
  BitmapFrameList& operator=(const BitmapFrameList&) = delete;

  void Append(std::unique_ptr<BitmapFrame> frame);
  void Clear();
  BitmapFrame* First() const;
  BitmapFrame* Next(const BitmapFrame* frame) const;
  std::size_t Count() const;

  private:
  std::vector<std::unique_ptr<BitmapFrame>> m_frames;
};
