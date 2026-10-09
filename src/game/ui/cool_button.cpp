#include "ui/cool_button.h"

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_button_parts.h"
#include "ui/resource_dll.h"


namespace {

constexpr uint32_t kButtonWindowStyle = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr char kBitBltMode = '@';
constexpr uint32_t kBitmapResourceKind = 10;
constexpr uint32_t kFrameResourceKind = 2;

struct StrobeFrame {
    const void* m_vtable;
    DibBitmap* m_bitmap;  // +0x04
    StrobeFrame* m_next;  // +0x08
    StrobeFrame* m_previous;  // +0x0c
};

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) delete bitmap;
}

DibBitmap* LoadButtonBitmap(HINSTANCE instance, uint32_t resourceId) {
    if (static_cast<int16_t>(resourceId) == 0) return nullptr;
    return NewOrNull<DibBitmap>(instance, resourceId, kBitmapResourceKind);
}

void DiscardRepeatTimer(CoolStrobeButton* button) {
    if (!button->m_repeatTimer) return;
    button->m_repeatTimer->Stop();
    if (button->m_repeatTimer) delete button->m_repeatTimer;
    button->m_repeatTimer = nullptr;
}

void RestorePreviousCapture() {
    if (g_previousCapture) {
        SetCaptureWindow(g_previousCapture);
        g_previousCapture = nullptr;
    }
}

void DrawLabel(CoolStrobeButton* button, HDC dc) {
    if (button->m_label && button->m_label->m_text && (button->m_state & kButtonShowLabel)) {
        button->SelectCurrentFont(dc);
        SetBkMode(dc, TRANSPARENT);
        DrawTextA(dc, button->m_label->m_text, -1, &button->m_labelRect, DT_CENTER);
        button->RestoreFont(dc);
    }
}

void DrawStateImage(HDC dc, DibBitmap* image, DibBitmap* backdrop, DibBitmap* buffer, int sourceX, int sourceY,
                    bool keyed) {
    backdrop->Blit(buffer, 0, 0, sourceX, sourceY, 0, 0);
    if (keyed)
        image->DrawKeyed(buffer, 0, 0);
    else
        image->Blit(buffer, 0, 0, 0, 0, 0, 0);
    buffer->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
}

}  // namespace

// ---- CoolStrobeButton ---------------------------------------------------------------------------------------------

CoolStrobeButton::CoolStrobeButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent, uint32_t childId,
                       uint32_t normalBitmapId, uint32_t pressedBitmapId, DibBitmap* backdrop, int repeatRate,
                       int pixelHit)
    : CoolWindow(instance, "CoolStrobeButton", kButtonWindowStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), nullptr, nullptr, nullptr, 0, 0),
      m_clickSound(0, 0, 0, 0, 0, 0) {
    m_childId = childId;
    m_state = kButtonNeverDrawn | kButtonShowLabel;
    m_normal = LoadButtonBitmap(instance, normalBitmapId);
    m_pressed = LoadButtonBitmap(instance, pressedBitmapId);
    m_checked = nullptr;
    m_disabled = nullptr;
    m_normalId = static_cast<int16_t>(normalBitmapId);
    m_pressedId = static_cast<int16_t>(pressedBitmapId);
    m_disabledId = 0;
    m_checkedId = 0;
    m_label = nullptr;
    m_normalColor = PALETTERGB(0, 0, 0);
    m_pressedColor = PALETTERGB(255, 255, 255);
    m_disabledColor = PALETTERGB(0x40, 0x40, 0x40);
    m_checkedColor = PALETTERGB(0, 0x80, 0);
    m_clickSoundId = 0;
    m_backdrop = backdrop;
    m_backdropX = x;
    m_backdropY = y;
    m_buffer = NewOrNull<DibBitmap>(m_width, m_height, backdrop);

    const bool normalFits = !m_normal || (m_normal->Width() == m_width && m_normal->Height() == m_height);
    const bool pressedFits = !m_pressed || (m_pressed->Width() == m_width && m_pressed->Height() == m_height);
    if (!normalFits || !pressedFits) m_state |= kButtonResized;
    if (pixelHit == 0) m_state |= kButtonPixelHit;

    m_repeatTimer = nullptr;
    m_repeatRate = repeatRate;
    if (repeatRate < 1)
        m_repeatDelay = 0;
    else
        m_repeatDelay = 250 / repeatRate;
}

CoolStrobeButton::~CoolStrobeButton() {
    DeleteBitmap(m_normal);
    DeleteBitmap(m_pressed);
    DeleteBitmap(m_disabled);
    DeleteBitmap(m_checked);
    if (m_label) delete m_label;
    DeleteBitmap(m_buffer);
    if (m_repeatTimer) delete m_repeatTimer;
    m_repeatTimer = nullptr;
    REB_KEEP_VPTR_STORE();
}

void CoolStrobeButton::Show(int command) {
    if (command == 0) m_parent->RemoveLinkedWindow(this);
    CoolWindow::Show(command);
}

void CoolStrobeButton::Enable() {
    ClearStateFlags(kButtonDisabled);
    CoolWindow::Enable();
}

void CoolStrobeButton::Disable() {
    SetStateFlags(kButtonDisabled);
    CoolWindow::Disable();
}

void CoolStrobeButton::SetFont(int fontSlot) {
    CoolWindow::SetFont(fontSlot);
    CenterLabel();
}

uint32_t CoolStrobeButton::GetChildId() { return m_childId; }

void CoolStrobeButton::SetLabelFromResource(const ResourceStringId* id) {
    if (m_label) delete m_label;
    m_label = NewOrNull<String>(id);
    CenterLabel();
}

void CoolStrobeButton::SetLabelFromStringId(uint16_t id) {
    char text[0x100];
    LoadResourceString(id, text, sizeof(text));
    if (m_label) delete m_label;
    m_label = NewOrNull<String>(static_cast<const char*>(text));
    CenterLabel();
}

void CoolStrobeButton::SetLabel(const char* text) {
    if (m_label) delete m_label;
    if (text)
        m_label = NewOrNull<String>(text);
    else
        m_label = nullptr;
    CenterLabel();
}

void CoolStrobeButton::CenterLabel() {
    HWND hwnd = m_hwnd;
    const int width = m_clientWidth;
    const int height = m_clientHeight;
    if (!m_label || !m_label->m_text) return;
    HDC dc = GetDC(hwnd);
    SelectCurrentFont(dc);
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, 0);
    m_labelRect.left = 0;
    m_labelRect.right = width;
    m_labelRect.top = 0;
    m_labelRect.bottom = 0;
    m_labelRect.bottom = DrawTextA(dc, m_label->m_text, -1, &m_labelRect, DT_CENTER | DT_CALCRECT);
    RestoreFont(dc);
    const int textWidth = m_labelRect.right;
    const int textHeight = m_labelRect.bottom;
    const int left = (width - textWidth) / 2;
    m_labelRect.left = left;
    const int top = (height - textHeight) / 2;
    m_labelRect.right = left + textWidth;
    m_labelRect.top = top;
    m_labelRect.bottom = top + textHeight;
    ReleaseDC(hwnd, dc);
}

void CoolStrobeButton::MoveLabel(int dx, int dy) {
    m_labelRect.top += dy;
    m_labelRect.bottom += dy;
    m_labelRect.left += dx;
    m_labelRect.right += dx;
}

const char* CoolStrobeButton::GetTooltipText() { return m_tooltipText; }

void CoolStrobeButton::SetClickSound(uint16_t soundId) {
    m_clickSoundId = soundId;
    m_clickSound.m_soundId = soundId;
    m_clickSound.m_kind = kSoundKindWave;
    m_clickSound.m_moduleIndex = static_cast<int>(FindSlotOfModule(m_instance));
}

SoundRequest* CoolStrobeButton::GetClickSound() { return &m_clickSound; }

void CoolStrobeButton::SetStateColor(int state, COLORREF color) {
    switch (state) {
    case 0:
        m_normalColor = color;
        break;
    case 1:
        m_pressedColor = color;
        break;
    case 2:
    case kButtonHighlight:
        m_disabledColor = color;
        break;
    case 4:
        m_checkedColor = color;
        break;
    }
}

void CoolStrobeButton::SetStateFlags(uint32_t bits) {
    m_state |= bits;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolStrobeButton::ClearStateFlags(uint32_t bits) {
    m_state &= ~bits;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

int CoolStrobeButton::HasStateFlags(uint32_t bits) { return (m_state & bits) != 0; }

uint32_t CoolStrobeButton::GetState() { return m_state; }

void CoolStrobeButton::LoadStateBitmap(int state, uint32_t resourceId) {
    DibBitmap* bitmap = NewOrNull<DibBitmap>(m_instance, resourceId, kFrameResourceKind);
    // BUG(original): if the allocation failed the original asks the null bitmap whether it is loaded and crashes.
    if (!bitmap) return;
    const int16_t id = static_cast<int16_t>(resourceId);
    switch (state) {
    case 0:
        if (m_normal) {
            delete m_normal;
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        m_normal = bitmap;
        m_normalId = id;
        break;
    case 1:
        if (m_pressed) {
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
            delete m_pressed;
        }
        m_pressed = bitmap;
        m_pressedId = id;
        break;
    case 2:
    case kButtonHighlight:
        if (m_disabled) {
            delete m_disabled;
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        m_disabled = bitmap;
        m_disabledId = id;
        break;
    case 4:
        if (m_checked) {
            if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
            delete m_checked;
        }
        m_checked = bitmap;
        m_checkedId = id;
        break;
    }
}

void CoolStrobeButton::ReplaceBitmap(int state, DibBitmap* bitmap) {
    // BUG(original): a null bitmap is asked whether it is loaded and the original crashes.
    if (!bitmap) return;
    switch (state) {
    case 0:
        DeleteBitmap(m_normal);
        m_normal = bitmap;
        m_normalId = 0;
        break;
    case 1:
        DeleteBitmap(m_pressed);
        m_pressed = bitmap;
        m_pressedId = 0;
        break;
    case 2:
    case kButtonHighlight:
        DeleteBitmap(m_disabled);
        m_disabled = bitmap;
        m_disabledId = 0;
        break;
    case 4:
        DeleteBitmap(m_checked);
        m_checked = bitmap;
        m_checkedId = 0;
        break;
    }
}

int16_t CoolStrobeButton::GetStateResourceId(int state) {
    switch (state) {
    case 0:
        return m_normalId;
    case 1:
        return m_pressedId;
    case 2:
    case kButtonHighlight:
        return m_disabledId;
    case 4:
        return m_checkedId;
    }
    return 0;
}

DibBitmap* CoolStrobeButton::GetStateBitmap(int state) {
    switch (state) {
    case 0:
        return m_normal;
    case 1:
        return m_pressed;
    case 2:
    case kButtonHighlight:
        return m_disabled;
    case 4:
        return m_checked;
    }
    return nullptr;
}

void CoolStrobeButton::UpdateHot(uint32_t lParam) {
    const uint32_t state = m_state;
    DibBitmap* image = m_normal;
    // BUG(original): `state & kButtonDisabled | kButtonHighlight` is always non-zero, so the checked bitmap is never
    // considered and the disabled bitmap decides.
    if (state & kButtonDisabled | kButtonHighlight) {
        if (m_disabled) image = m_disabled;
    } else if (state & kButtonChecked) {
        if (m_checked) image = m_checked;
    }
    const int x = lParam & 0xffff;
    const int y = lParam >> 16;
    if ((state & kButtonDisabled) || !image) {
        m_hot = 0;
        return;
    }
    if (state & kButtonPixelHit) {
        m_hot = image->IsPixelOpaque(x, y);
        return;
    }
    m_hot = (x >= 0 && x <= m_clientWidth && y >= 0 && y <= m_clientHeight) ? 1 : 0;
}

void CoolStrobeButton::OnRepeatTimer() {
    const int ticks = m_repeatTicks;
    m_repeatTicks = ticks + 1;
    if (ticks > m_repeatDelay) {
        const SHORT keyState = GetAsyncKeyState(VK_LBUTTON);
        GetCapture();
        if (static_cast<int>(keyState) & 0x10000000) {
            PostMessageA(m_parent->m_hwnd, WM_COMMAND, GetChildId(), 0);
            return;
        }
        PostMessageA(m_hwnd, WM_COMMAND, m_childId, 0);
    }
}

void CoolStrobeButton::Click() {
    m_hot = 1;
    if (m_clickSoundId != 0) SoundManager::Play(&m_clickSound);
    SetStateFlags(kButtonPressed);
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    PostMessageA(m_parent->m_hwnd, WM_COMMAND, GetChildId(), 0);
}

void CoolStrobeButton::CancelPress() {
    if (m_hot) {
        ClearStateFlags(kButtonPressed);
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
        m_hot = 0;
        m_repeatTicks = 0;
    }
}

int CoolStrobeButton::IsHot() { return m_hot; }

void CoolStrobeButton::Draw(HDC dc, char state) {
    SelectPalette(dc, g_palette, FALSE);
    if (m_state & kButtonDisabled) {
        m_hot = 0;
        state = kButtonDisabled;
    }
    DibBitmap* image;
    switch (state) {
    case 0:
        goto normal;
    case 1:
        if (m_hot) {
            SetTextColor(dc, m_pressedColor);
            image = m_pressed;
            goto havePicture;
        }
        goto plainNormal;
    case 2:
    case kButtonHighlight:
        image = m_disabled;
        if (!image) image = m_normal;
        SetTextColor(dc, m_disabledColor);
        goto havePicture;
    default:
        return;
    case 4:
        break;
    }
    m_state |= kButtonChecked;
normal:
    if (!(m_state & kButtonChecked)) {
        if (!(m_state & kButtonHighlight)) {
        plainNormal:
            SetTextColor(dc, m_normalColor);
            image = m_normal;
        } else {
            SetTextColor(dc, m_disabledColor);
            image = m_disabled;
            if (!image) image = m_normal;
        }
    } else {
        SetTextColor(dc, m_checkedColor);
        image = m_checked;
        if (!image) image = m_normal;
    }
havePicture:
    if (image) {
        if (!(m_state & kButtonOpaque)) {
            DrawStateImage(dc, image, m_backdrop, m_buffer, m_backdropX, m_backdropY, true);
        } else if (m_state & kButtonResized) {
            DrawStateImage(dc, image, m_backdrop, m_buffer, m_backdropX, m_backdropY, false);
        } else {
            image->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
        }
    }
    DrawLabel(this, dc);
    m_state &= ~kButtonNeverDrawn;
}

LRESULT __stdcall CoolStrobeButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(m_hwnd, &paint);
        Draw(dc, static_cast<char>(m_state & kButtonPressed));
        EndPaint(m_hwnd, &paint);
        m_state &= ~kButtonNeverDrawn;
        return 0;
    }
    case WM_CREATE: {
        LRESULT result = CoolWindow::WindowProc(WM_CREATE, wParam, lParam);
        HDC dc = GetDC(m_hwnd);
        if (g_palette) {
            HPALETTE previous = SelectPalette(dc, g_palette, FALSE);
            RealizePalette(dc);
            SelectPalette(dc, previous, FALSE);
        }
        ReleaseDC(m_hwnd, dc);
        m_parent->AddLinkedWindow(this);
        return result;
    }
    case WM_MOVE:
        m_backdropX = static_cast<uint32_t>(lParam) & 0xffff;
        m_backdropY = static_cast<uint32_t>(lParam) >> 16;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        DiscardRepeatTimer(this);
        if (!(m_state & kButtonPressed)) return 0;
        ReleaseCapture();
        RestorePreviousCapture();
        return 0;
    case WM_COMMAND:
        if (wParam != m_childId) return 0;
        ClearStateFlags(kButtonPressed);
        if (GetCapture() == m_hwnd) {
            ReleaseCapture();
            RestorePreviousCapture();
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        DiscardRepeatTimer(this);
        m_repeatTicks = 0;
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        UpdateHot(static_cast<uint32_t>(lParam));
        if (m_state & kButtonNeverDrawn) m_hot = 0;
        if (m_hot == 0) return 0;
        SetStateFlags(kButtonPressed);
        g_previousCapture = SetCaptureWindow(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        PostMessageA(m_parent->m_hwnd, kButtonMessage, (message << 16) | (m_childId & 0xffff), lParam);
        if (m_clickSoundId != 0) SoundManager::Play(&m_clickSound);
        if (m_repeatRate == 0) return 0;
        m_repeatTicks = 0;
        DiscardRepeatTimer(this);
        {
            void* memory = operator new(sizeof(CoolTimer) + sizeof(void*));
            m_repeatTimer = memory ? static_cast<CoolTimer*>(new (memory) ButtonRepeatTimer(m_repeatRate, this)) : nullptr;
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_hot == 0) return 0;
        if (GetCapture() != m_hwnd) return 0;
        ClearStateFlags(kButtonPressed);
        if (GetCapture() == m_hwnd) ReleaseCapture();
        RestorePreviousCapture();
        UpdateHot(static_cast<uint32_t>(lParam));
        if (m_hot != 0) {
            if (m_state & kButtonToggle) m_state ^= kButtonChecked;
            PostMessageA(m_parent->m_hwnd, WM_COMMAND, GetChildId(), 0);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        DiscardRepeatTimer(this);
        m_repeatTicks = 0;
        return 0;
    case WM_PALETTECHANGED:
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcA(m_hwnd, message, wParam, lParam);
}

// ---- CoolFrameButton ---------------------------------------------------------------------------------------

CoolFrameButton::CoolFrameButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                   uint32_t childId, uint32_t firstFrameId, uint32_t pressedBitmapId,
                                   uint16_t frameCount, DibBitmap* backdrop, int repeatRate, int pixelHit)
    : CoolStrobeButton(instance, x, y, width, height, parent, childId, 0, pressedBitmapId, backdrop, repeatRate, pixelHit) {
    new (m_frames) BitmapFrameList();
    LoadFrames(firstFrameId, frameCount);
}

CoolFrameButton::~CoolFrameButton() {
    m_normal = nullptr;
    reinterpret_cast<BitmapFrameList*>(m_frames)->BitmapFrameList::~BitmapFrameList();
    REB_KEEP_VPTR_STORE();
}

LRESULT __stdcall CoolFrameButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CREATE) {
        m_normal = m_currentFrame ? static_cast<StrobeFrame*>(m_currentFrame)->m_bitmap : nullptr;
        return CoolStrobeButton::WindowProc(WM_CREATE, wParam, lParam);
    }
    if (message != WM_PAINT) return CoolStrobeButton::WindowProc(message, wParam, lParam);
    HWND hwnd = m_hwnd;
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);
    Draw(dc, static_cast<char>(m_state & kButtonPressed));
    EndPaint(hwnd, &paint);
    return 0;
}

void CoolFrameButton::StepFrame() {
    if (HasStateFlags(kButtonHighlight)) return;
    if (m_currentFrame) {
        StrobeFrame* next = static_cast<StrobeFrame*>(m_currentFrame)->m_next;
        m_currentFrame = next;
        m_normal = next->m_bitmap;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    } else {
        m_normal = nullptr;
    }
}

void CoolFrameButton::LoadFrames(uint32_t firstId, uint32_t count) {
    HINSTANCE instance = m_instance;
    uint32_t index = 0;
    if (count != 0) {
        do {
            DibBitmap* bitmap = NewOrNull<DibBitmap>(instance, index + firstId, kFrameResourceKind);
            // BUG(original): a frame that fails to load is leaked.
            if (bitmap && bitmap->IsLoaded()) {
                void* memory = operator new(sizeof(StrobeFrame));
                void* node = memory ? new (memory) BitmapFrame(bitmap) : nullptr;
                reinterpret_cast<BitmapFrameList*>(m_frames)->Append(reinterpret_cast<BitmapFrame*>(node));
            }
            ++index;
        } while ((index & 0xffff) < count);
    }
    m_currentFrame = *reinterpret_cast<void**>(m_frames + 8);
}

// ---- CoolStrobeTimer ------------------------------------------------------------------------------------------

CoolStrobeTimer::CoolStrobeTimer(void (*callback)()) : CoolTimer(0x42, TIME_PERIODIC, 10) {
    Pause();
    m_callback = callback;
}

CoolStrobeTimer::~CoolStrobeTimer() {
    Pause();
    REB_KEEP_VPTR_STORE();
}

void CoolStrobeTimer::OnTick() {
    if (m_callback) m_callback();
}

// ---- IndexedRectNode ------------------------------------------------------------------------------------------

IndexedRectNode::IndexedRectNode() {
    m_bottom = 0;
    m_top = 0;
    m_right = 0;
    m_left = 0;
    m_index = 0;
}

IndexedRectNode::~IndexedRectNode() { REB_KEEP_VPTR_STORE(); }

// ---- CoolCycleButton ------------------------------------------------------------------------------------------

CoolCycleButton::CoolCycleButton(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                 uint32_t childId, uint32_t id0, uint32_t id1, uint32_t id2, uint32_t id3,
                                 uint32_t id4, DibBitmap* backdrop, int repeatRate)
    : CoolStrobeButton(instance, x, y, width, height, parent, childId, id2, id4, backdrop, repeatRate, 0) {
    m_image0 = NewOrNull<DibBitmap>(instance, id0, kBitmapResourceKind);
    m_image1 = NewOrNull<DibBitmap>(instance, id1, kBitmapResourceKind);
    m_image2 = NewOrNull<DibBitmap>(instance, id2, kBitmapResourceKind);
    m_image3 = NewOrNull<DibBitmap>(instance, id3, kBitmapResourceKind);
    m_image4 = NewOrNull<DibBitmap>(instance, id4, kBitmapResourceKind);
    m_imageId0 = static_cast<int16_t>(id0);
    m_imageId1 = static_cast<int16_t>(id1);
    m_imageId2 = static_cast<int16_t>(id2);
    m_imageId3 = static_cast<int16_t>(id3);
    m_imageId4 = static_cast<int16_t>(id4);
    m_position = 2;
    m_direction = 1;
    m_backdropX2 = x;
    m_backdropY2 = y;
    m_backdrop2 = backdrop;
    m_buffer2 = NewOrNull<DibBitmap>(width, height, backdrop);
}

CoolCycleButton::~CoolCycleButton() {
    DeleteBitmap(m_image0);
    DeleteBitmap(m_image1);
    DeleteBitmap(m_image2);
    DeleteBitmap(m_image3);
    DeleteBitmap(m_image4);
    DeleteBitmap(m_buffer2);
    REB_KEEP_VPTR_STORE();
}

LRESULT __stdcall CoolCycleButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_LBUTTONDOWN) {
        DibBitmap* saved = m_normal;
        const uint32_t position = m_position & 3;
        if (position == 0)
            m_normal = m_image4;
        else if (position == 1)
            m_normal = m_image2;
        else if (position == 2)
            m_normal = m_image0;
        LRESULT result = CoolStrobeButton::WindowProc(WM_LBUTTONDOWN, wParam, lParam);
        m_normal = saved;
        return result;
    }
    if (message != WM_LBUTTONUP) return CoolStrobeButton::WindowProc(message, wParam, lParam);
    DibBitmap* saved = m_normal;
    const uint32_t position = m_position & 3;
    if (position != 0) {
        if (position == 1) {
            m_normal = (m_direction == 1) ? m_image0 : m_image4;
            LRESULT result = CoolStrobeButton::WindowProc(WM_LBUTTONUP, wParam, lParam);
            m_normal = saved;
            return result;
        }
        if (position != 2) {
            LRESULT result = CoolStrobeButton::WindowProc(WM_LBUTTONUP, wParam, lParam);
            m_normal = saved;
            return result;
        }
    }
    m_normal = m_image2;
    LRESULT result = CoolStrobeButton::WindowProc(WM_LBUTTONUP, wParam, lParam);
    m_normal = saved;
    return result;
}

void CoolCycleButton::Draw(HDC dc, char state) {
    DibBitmap* image = nullptr;
    SelectPalette(dc, g_palette, FALSE);
    if (m_state & kButtonDisabled) {
        m_hot = 0;
        state = kButtonDisabled;
    }
    switch (state) {
    case 0:
        goto normal;
    case 1: {
        if (m_hot == 0) {
            SetTextColor(dc, m_normalColor);
            goto drawImage;
        }
        SetTextColor(dc, m_pressedColor);
        if (m_direction == 0) {
            m_position = m_position + 1;
            const uint32_t position = m_position & 3;
            if (position == 1) {
                image = m_image3;
            } else if (position == 2) {
                image = m_image1;
            } else {
                image = m_image1;
                m_direction = 1;
                m_position = 1;
            }
        } else {
            m_position = m_position - 1;
            const uint32_t position = m_position & 3;
            if (position == 0) {
                image = m_image3;
            } else if (position == 1) {
                image = m_image1;
            } else {
                image = m_image3;
                m_direction = 0;
                m_position = 1;
            }
        }
        SoundRequest sound(*GetClickSound());
        if (sound.m_soundId != 0) SoundManager::Play(&sound);
        if (m_state & kButtonToggle) m_state ^= kButtonChecked;
        goto drawImage;
    }
    case 2:
        image = m_disabled;
        SetTextColor(dc, m_disabledColor);
        goto drawImage;
    default:
        return;
    case 4:
        break;
    }
    m_state |= kButtonChecked;
normal : {
    const uint32_t position = m_position & 3;
    if (position == 0)
        image = m_image4;
    else if (position == 1)
        image = m_image2;
    else if (position != 2)
        image = m_image4;
    else
        image = m_image0;
    if (!(m_state & kButtonChecked)) {
        SetTextColor(dc, m_normalColor);
    } else {
        SetTextColor(dc, m_checkedColor);
        if (m_checked) image = m_checked;
    }
}
drawImage:
    if (image) {
        m_backdrop2->Blit(m_buffer2, 0, 0, m_backdropX2, m_backdropY2, 0, 0);
        image->DrawKeyed(m_buffer2, 0, 0);
        m_buffer2->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
    }
    DrawLabel(this, dc);
}
