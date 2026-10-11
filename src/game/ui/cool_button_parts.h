#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_timer.h"

class CoolStrobeButton;
class DibBitmap;

// Auto-repeat of a held CoolStrobeButton: a periodic CoolTimer that calls the button's OnRepeatTimer.
class ButtonRepeatTimer : public CoolTimer {
public:
    // A delay below 1 creates the timer paused.
    ButtonRepeatTimer(int delay, CoolStrobeButton* button);  // 0x60f1e0
    ~ButtonRepeatTimer() override;                            // 0x60f270 (scalar deleting destructor 0x60f250)
    void OnTick() override;                          // 0x60f2c0

    CoolStrobeButton* m_button;  // +0x38
};

// One animation frame of a strobe button; owns its bitmap.
class BitmapFrame {
public:
    explicit BitmapFrame(DibBitmap* bitmap);  // 0x60f3c0
    virtual ~BitmapFrame();                   // 0x60f400 (scalar deleting destructor 0x60f3e0)

    DibBitmap* m_bitmap;  // +0x04
    BitmapFrame* m_next;  // +0x08
    BitmapFrame* m_prev;  // +0x0c
};

// Circular doubly linked list of BitmapFrames (a strobe button's animation); owns the frames.
class BitmapFrameList {
public:
    BitmapFrameList();           // 0x60f2d0
    virtual ~BitmapFrameList();  // 0x60f310 (scalar deleting destructor 0x60f2f0)

    // Appends after the last frame. Nothing happens for a null frame.
    void Append(BitmapFrame* frame);  // 0x60f320
    void Clear();                     // 0x60f370: deletes every frame

    int m_count;          // +0x04
    BitmapFrame* m_head;  // +0x08
};
