#include "ui/cool_button_parts.h"

#include <algorithm>
#include <utility>

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

BitmapFrame::BitmapFrame(std::unique_ptr<DibBitmap> bitmap) : m_bitmap(std::move(bitmap)) {}

BitmapFrame::~BitmapFrame() = default;

DibBitmap* BitmapFrame::Bitmap() const { return m_bitmap.get(); }

void BitmapFrameList::Append(std::unique_ptr<BitmapFrame> frame) {
    if (frame) m_frames.push_back(std::move(frame));
}

void BitmapFrameList::Clear() { m_frames.clear(); }

BitmapFrame* BitmapFrameList::First() const { return m_frames.empty() ? nullptr : m_frames.front().get(); }

BitmapFrame* BitmapFrameList::Next(const BitmapFrame* frame) const {
    const auto found = std::find_if(m_frames.begin(), m_frames.end(), [frame](const auto& candidate) {
        return candidate.get() == frame;
    });
    if (found == m_frames.end() || m_frames.empty()) return nullptr;
    const auto next = std::next(found);
    return next == m_frames.end() ? m_frames.front().get() : next->get();
}

std::size_t BitmapFrameList::Count() const { return m_frames.size(); }
