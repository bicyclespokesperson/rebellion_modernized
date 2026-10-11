#include "ui/cool_button_parts.h"

#include "gfx/dib_bitmap.h"
#include "ui/cool_button.h"

namespace {

constexpr UINT kTimerResolutionMs = 10;

}  // namespace

ButtonRepeatTimer::ButtonRepeatTimer(int delay, CoolStrobeButton* button)
    : CoolTimer(delay, TIME_PERIODIC, kTimerResolutionMs) {
    if (delay < 1) Pause();
    m_button = button;
}

ButtonRepeatTimer::~ButtonRepeatTimer() { Pause(); }

void ButtonRepeatTimer::OnTick() { m_button->OnRepeatTimer(); }

BitmapFrame::BitmapFrame(DibBitmap* bitmap) : m_bitmap(bitmap), m_next(nullptr), m_prev(nullptr) {}

BitmapFrame::~BitmapFrame() {
    if (m_bitmap) delete m_bitmap;

}

BitmapFrameList::BitmapFrameList() : m_count(0), m_head(nullptr) {}

BitmapFrameList::~BitmapFrameList() {

    Clear();
}

void BitmapFrameList::Append(BitmapFrame* frame) {
    if (!frame) return;
    if (!m_head) {
        m_head = frame;
        frame->m_next = frame;
        frame->m_prev = m_head;
    } else {
        BitmapFrame* last = m_head->m_prev;
        if (!last) return;
        last->m_next = frame;
        frame->m_next = m_head;
        frame->m_prev = last;
        m_head->m_prev = frame;
    }
    ++m_count;
}

void BitmapFrameList::Clear() {
    BitmapFrame* frame = m_head;
    if (!frame) return;
    while (frame != frame->m_next) {
        BitmapFrame* prev = frame->m_prev;
        BitmapFrame* next = frame->m_next;
        prev->m_next = next;
        next->m_prev = prev;
        delete frame;
        frame = next;
    }
    delete frame;
    m_head = nullptr;
    m_count = 0;
}
