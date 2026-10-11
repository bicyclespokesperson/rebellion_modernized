#pragma once
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"

class DibBitmap;

// Orientation of a CoolSlider: the knob moves along the y axis (1) or the x axis (2).
enum CoolSliderOrientation : int {
    kSliderVertical = 1,
    kSliderHorizontal = 2,
};

// A slider ("slider" window class): a track bitmap with a knob bitmap that the mouse drags. The value is the knob's
// position scaled to 0..m_range and is posted to the parent as WM_COMMAND (child id, value) while dragging and on
// release. The track is drawn over a copy of the backdrop (the parent's background).
class CoolSlider : public CoolWindow {
public:
    // The window is as large as the track bitmap. The backdrop is not owned. `value` is the initial position in the
    // range 0..range.
    CoolSlider(HINSTANCE instance, int x, int y, CoolWindow* parent, uint32_t childId, int orientation,
               DibBitmap* backdrop, uint32_t trackBitmapId, uint32_t knobBitmapId, uint32_t value, uint32_t range);  // 0x604570
    ~CoolSlider() override;  // 0x604850 (scalar deleting destructor 0x604830)

    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x604b80

    int IsCursorOverTrack();  // 0x6048f0
    void TrackCursor();        // 0x604950: moves the knob to the cursor if the cursor is inside the track
    void Draw(HDC dc);         // 0x604a50: also derives m_value from the knob position

    int m_backdropX;        // +0x94 where the slider sits in the backdrop
    int m_backdropY;        // +0x98
    int m_knobWidth;        // +0x9c
    int m_knobHeight;       // +0xa0
    int m_orientation;      // +0xa4 CoolSliderOrientation
    DibBitmap* m_track;     // +0xa8 owned
    DibBitmap* m_knob;      // +0xac owned
    DibBitmap* m_backdrop;  // +0xb0 not owned
    DibBitmap* m_buffer;    // +0xb4 owned: backdrop copy the track and knob are drawn on
    int m_trackLeft;        // +0xb8
    int m_trackTop;         // +0xbc
    int m_trackRight;       // +0xc0
    int m_trackBottom;      // +0xc4
    POINT m_position;       // +0xc8 knob centre along the track (the other coordinate is the cursor's)
    int m_knobOffset;       // +0xd0 knob position across the track
    uint32_t m_zeroX;       // +0xd4 position at which a horizontal slider reads 0
    uint32_t m_zeroY;       // +0xd8 position at which a vertical slider reads 0 (the track height)
    uint32_t m_value;       // +0xdc
    uint32_t m_range;       // +0xe0
};
