#pragma once
#include <cstdint>

#include "audio/sound_manager.h"
#include "platform/win32.h"
#include "ui/cool_label.h"
#include "ui/cool_button_parts.h"
#include "ui/cool_timer.h"
#include "ui/cool_window.h"

class DibBitmap;
class String;
struct ResourceStringId;

// Bits of CoolStrobeButton::m_state. The single-bit values 0, 1, 2, 4 and 0x40 double as the "state" argument of Draw,
// SetStateColor, SetBitmap and GetBitmap (0 normal, 1 pressed, 2 or 0x40 disabled / highlighted, 4 checked).
enum CoolButtonState : uint32_t {
    kButtonPressed = 0x01,
    kButtonDisabled = 0x02,
    kButtonChecked = 0x04,
    kButtonToggle = 0x08,       // a click flips kButtonChecked
    kButtonPixelHit = 0x10,     // hit test by the opacity of the bitmap's pixel instead of the window rectangle
    kButtonShowLabel = 0x20,
    kButtonHighlight = 0x40,    // shows the "disabled" bitmap; also freezes the strobe animation
    kButtonOpaque = 0x80,       // the state bitmaps are drawn as they are, not keyed over the backdrop
    kButtonNeverDrawn = 0x10000,  // set at creation, cleared by the first Draw; clicks are ignored until then
    kButtonResized = 0x20000,   // a state bitmap differs in size from the window: compose into the buffer
};

constexpr uint32_t kButtonMessage = 0x40e;  // posted to the parent on a click: (WM_xxx << 16) | child id, lParam = click

extern HWND g_previousCapture;  // 0x6be5bc

// The simple bitmap push button, "CoolStrobeButton" in the window class registry. The look comes from up to four
// bitmaps (normal, pressed, disabled, checked); Draw keys the current one over a copy of the backdrop (the parent's
// background) kept in m_buffer. A click is reported to the parent as WM_COMMAND (id) on release. With a repeat rate
// it keeps re-posting the command while held (CoolRepeatTimer calls OnRepeatTimer).
class CoolStrobeButton : public CoolWindow {
public:
    // The bitmaps are loaded from `instance`'s RT_BITMAP resources (0 for none). The backdrop is not owned.
    // Bitmaps whose size differs from the window turn on kButtonResized. A repeat rate of 0 turns repeating
    // off. With pixelHit == 0 the hit test is by pixel (kButtonPixelHit).
    CoolStrobeButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
               uint32_t normalBitmapId, uint32_t pressedBitmapId, DibBitmap* backdrop, int repeatRate, int pixelHit);  // 0x602150
    virtual ~CoolStrobeButton();  // 0x602400 (scalar deleting destructor 0x6023e0)

    virtual void Show(int command) override;   // slot 1, 0x6037c0: a hidden button leaves its parent's linked windows
    virtual void Enable() override;            // slot 3, 0x602880
    virtual void Disable() override;           // slot 4, 0x6028a0
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x6028c0
    virtual void SetFont(int fontSlot) override;  // slot 6, 0x602ff0: also re-centres the label
    virtual void Draw(HDC dc, char state);     // slot 9, 0x602d30
    virtual int IsHot();                       // slot 10, 0x452fb0 (shared): m_hot
    virtual void OnRepeatTimer();               // slot 11, 0x6036a0

    uint32_t GetChildId();                                      // 0x602510
    void SetLabelFromResource(const ResourceStringId* id);      // 0x602520
    void SetLabelFromStringId(uint16_t id);                     // 0x6025a0 (LoadStringA of the string DLL)
    void SetLabel(const char* text);                            // 0x602650
    void CenterLabel();                                         // 0x6026e0
    void MoveLabel(int dx, int dy);                             // 0x6027e0
    const char* GetTooltipText();                               // 0x602830
    void SetClickSound(uint16_t soundId);                       // 0x602840
    SoundRequest* GetClickSound();                              // 0x602fe0
    void SetStateColor(int state, COLORREF color);              // 0x603010
    void SetStateFlags(uint32_t bits);                           // 0x6030c0: set bits and invalidate
    void ClearStateFlags(uint32_t bits);                         // 0x6030f0: clear bits and invalidate
    int HasStateFlags(uint32_t bits);                           // 0x603120
    uint32_t GetState();                                        // 0x603140
    void LoadStateBitmap(int state, uint32_t resourceId);  // 0x603150
    void ReplaceBitmap(int state, DibBitmap* bitmap);           // 0x603350: takes ownership
    int16_t GetStateResourceId(int state);                             // 0x6034a0
    DibBitmap* GetStateBitmap(int state);                            // 0x603550
    void UpdateHot(uint32_t lParam);                            // 0x6035f0: m_hot for the point (low, high word)
    void Click();                                               // 0x603710: press-and-release without the mouse
    void CancelPress();                                         // 0x603780

    DibBitmap* m_normal;          // +0x94 owned
    DibBitmap* m_pressed;         // +0x98 owned
    DibBitmap* m_disabled;        // +0x9c owned
    DibBitmap* m_checked;         // +0xa0 owned
    DibBitmap* m_backdrop;        // +0xa4 not owned
    DibBitmap* m_buffer;          // +0xa8 owned: backdrop copy the current bitmap is drawn on
    int16_t m_normalId;           // +0xac resource ids the bitmaps came from (0 if replaced from memory)
    int16_t m_pressedId;          // +0xae
    int16_t m_disabledId;         // +0xb0
    int16_t m_checkedId;          // +0xb2
    COLORREF m_normalColor;       // +0xb4 label colours
    COLORREF m_pressedColor;      // +0xb8
    COLORREF m_checkedColor;      // +0xbc
    COLORREF m_disabledColor;     // +0xc0
    uint32_t m_state;             // +0xc4 CoolButtonState
    String* m_label;              // +0xc8 owned
    RECT m_labelRect;             // +0xcc
    uint16_t m_clickSoundId;      // +0xdc
    uint8_t m_pad_de[2];          // +0xde
    SoundRequest m_clickSound;    // +0xe0
    uint32_t m_unknownF4;         // +0xf4
    int m_hot;                    // +0xf8 the cursor is over a clickable pixel
    int m_repeatRate;             // +0xfc repeats per second, 0: none
    CoolTimer* m_repeatTimer;     // +0x100 CoolRepeatTimer while a repeating button is held
    int m_repeatDelay;            // +0x104 ticks to wait before the first repeat
    int m_repeatTicks;            // +0x108
    int m_backdropX;              // +0x10c where the button sits in the backdrop (follows WM_MOVE)
    int m_backdropY;              // +0x110
};

// The strobe animation of the main screen's console buttons: a ring of frames that CoolStrobeTimer steps through.
class CoolFrameButton : public CoolStrobeButton {
public:
    // `firstFrameId` .. +frameCount-1 are the frame bitmaps, the pressed bitmap is as for CoolStrobeButton.
    CoolFrameButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                     uint32_t firstFrameId, uint32_t pressedBitmapId, uint16_t frameCount, DibBitmap* backdrop,
                     int repeatRate, int pixelHit);  // 0x603870
    virtual ~CoolFrameButton();  // 0x603940 (scalar deleting destructor 0x603920)

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x6039a0

    void StepFrame();                                  // 0x603a50: the timer callback shows the next frame
    void LoadFrames(uint32_t firstId, uint32_t count);  // 0x603aa0

    BitmapFrameList m_frames;  // +0x114 ring of frame nodes (batch B list class, vftable 0x66e2b8; count +4, head +8)
    void* m_currentFrame;    // +0x120 node whose bitmap is m_normal; node: vptr, bitmap +4, next +8, previous +0xc
};

// A button that cycles through three images (up, middle, down) on every click: "tri-state" slider-like switch.
class CoolCycleButton : public CoolStrobeButton {
public:
    CoolCycleButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                    uint32_t id0, uint32_t id1, uint32_t id2, uint32_t id3, uint32_t id4, DibBitmap* backdrop,
                    int repeatRate);  // 0x603cd0
    virtual ~CoolCycleButton();  // 0x603f10 (scalar deleting destructor 0x603ef0)

    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x604000
    virtual void Draw(HDC dc, char state) override;  // slot 9, 0x604170

    DibBitmap* m_image0;        // +0x114 owned images of the three positions (+0x11c middle)
    DibBitmap* m_image1;        // +0x118
    DibBitmap* m_image2;        // +0x11c
    DibBitmap* m_image3;        // +0x120
    DibBitmap* m_image4;        // +0x124
    int16_t m_imageId0;         // +0x128
    int16_t m_imageId1;         // +0x12a
    int16_t m_imageId2;         // +0x12c
    int16_t m_imageId3;         // +0x12e
    int16_t m_imageId4;         // +0x130
    uint8_t m_pad_132[2];       // +0x132
    uint32_t m_position;        // +0x134 low two bits: which image is shown
    int m_direction;            // +0x138 1: cycling up, 0: down
    int m_backdropX2;           // +0x13c
    int m_backdropY2;           // +0x140
    DibBitmap* m_backdrop2;     // +0x144 not owned
    DibBitmap* m_buffer2;       // +0x148 owned
};

// Timer that calls a plain function every 66 ms (the strobe step of the main screen).
class CoolStrobeTimer : public CoolTimer {
public:
    explicit CoolStrobeTimer(void (*callback)());  // 0x603b90
    virtual ~CoolStrobeTimer();                    // 0x603c10 (scalar deleting destructor 0x603bf0)
    virtual void OnTick() override;                // slot 1, 0x603c60

    void (*m_callback)();  // +0x38
};

// A rectangle of the main screen that stands for item `m_index`.
class IndexedRectNode : public RectNode {
public:
    IndexedRectNode();            // 0x603c70
    virtual ~IndexedRectNode();   // 0x603cc0 (scalar deleting destructor 0x603ca0)

    int m_index;  // +0x28
};
