#include "ui/sprite_stage.h"

#include <algorithm>

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "gfx/indexed_sprite_filter.h"
#include "ui/cool_window.h"
#include "ui/sprite.h"
#include "ui/sprite_animator.h"

#if defined(REB_MINWIN)
#include "port/minwin_impl/gdi_api.h"
#endif

SpriteTree::SpriteTree()
{
  m_nextId = 0;
}

SpriteTree::~SpriteTree()
{
  Clear();
}

bool SpriteTree::SpriteDepthLess::operator()(const std::unique_ptr<Sprite>& first,
                                             const std::unique_ptr<Sprite>& second) const
{
  return first->m_depth < second->m_depth;
}

void SpriteTree::Add(Sprite* sprite)
{
  if (!sprite || std::ranges::any_of(m_sprites,
                                     [sprite](const auto& owned)
                                     {
                                       return owned.get() == sprite;
                                     }))
    return;
  if (sprite->m_id == 0)
    sprite->m_id = ++m_nextId;
  if (m_nextId < sprite->m_id)
    m_nextId = sprite->m_id;
  std::unique_ptr<Sprite> pending(sprite);
  try
  {
    m_sprites.insert(std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
}

Sprite* SpriteTree::FindById(uint32_t id)
{
  for (const auto& entry : m_sprites)
  {
    auto* sprite = entry.get();
    if (sprite->m_id == id)
      return sprite;
  }
  return nullptr;
}

void SpriteTree::Remove(Sprite* sprite)
{
  const auto found = std::find_if(m_sprites.begin(), m_sprites.end(),
                                  [sprite](const auto& owned)
                                  {
                                    return owned.get() == sprite;
                                  });
  if (found != m_sprites.end())
  {
    auto detached = m_sprites.extract(found);
    detached.value().release();
  }
}

void SpriteTree::Clear()
{
  decltype(m_sprites) detached;
  detached.swap(m_sprites);
}

AnimatorTree::AnimatorTree() = default;

AnimatorTree::~AnimatorTree()
{
  Clear();
}

uint32_t AnimatorTree::AddWithId(SpriteAnimator* animator)
{
  return m_animators.AddWithId(animator);
}

SpriteAnimator* AnimatorTree::FindById(uint32_t id) const
{
  return m_animators.FindById(id);
}

void AnimatorTree::Remove(SpriteAnimator* animator)
{
  m_animators.Remove(animator);
}

void AnimatorTree::Clear()
{
  m_animators.Clear();
}

namespace
{

void ClearBitmap(DibBitmap* bitmap)
{
  bitmap->FillRect(0, 0, bitmap->Width(), bitmap->Height(), 0);
}

} // namespace

SpriteStage::SpriteStage(CoolWindow* window, DibBitmap* format, int x, int y, int width, int height, uint32_t frameMs)
{
  m_window = window;
  m_hwnd = window->m_hwnd;
  m_x = x;
  m_y = y;
  if (width < 1)
    width = window->m_clientWidth - x;
  if (height < 1)
    height = window->m_clientHeight - y;
  m_rect.left = x;
  m_rect.top = y;
  m_rect.right = width + x;
  m_rect.bottom = height + y;
  HDC dc = GetDC(m_hwnd);
  m_buffers[0] = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), format, dc);
  m_buffers[1] = NewOrNull<DibBitmap>(static_cast<LONG>(width), static_cast<LONG>(height), format, dc);
  ClearBitmap(m_buffers[0]);
  ClearBitmap(m_buffers[1]);
  m_front = 0;
  ReleaseDC(m_hwnd, dc);
  m_frame = -2;
  m_frameMs = frameMs;
  m_startTime = timeGetTime();
  m_paused = 0;
  m_disabled = 1;
  m_alive = 1;
  m_originX = 0;
  m_originY = 0;
}

SpriteStage::~SpriteStage()
{
  m_alive = 0;
  if (DibBitmap* buffer = m_buffers[0])
  {
    delete buffer;
    m_buffers[0] = nullptr;
  }
  if (DibBitmap* buffer = m_buffers[1])
  {
    delete buffer;
    m_buffers[1] = nullptr;
  }
}

void SpriteStage::Draw(DibBitmap* dest)
{
  DrawAt(dest, m_x, m_y);
}

void SpriteStage::DrawAt(DibBitmap* dest, int x, int y)
{
  m_buffers[m_front]->BlitKeyed(dest, x, y, 0, 0, 0, 0, 0);
}

int SpriteStage::Tick()
{
  if (m_paused || m_disabled)
    return 0;
  const int frame = static_cast<int>((timeGetTime() - m_startTime) / m_frameMs);
  if (frame == m_frame)
    return 0;
  if (frame < m_frame)
  {
    m_frame = frame;
    return 0;
  }
  Render(frame);
  return 1;
}

void SpriteStage::Render(int frame)
{
  RECT area;
  int changed = 0;
  int anyChanged = 0;
  if (!m_alive)
    return;
  ClearBitmap(BackBuffer());
  m_resynced = 0;
  for (const auto& entry : m_animators.Entries())
  {
    SpriteAnimator* animator = entry.get();
    changed = 0;
    for (int step = m_frame; step < frame && !m_resynced; ++step)
      animator->Step(&area, &changed);
    if (changed)
    {
      const int dx = m_x - m_originX;
      area.left += dx;
      area.right += dx;
      const int dy = m_y - m_originY;
      area.top += dy;
      area.bottom += dy;
      anyChanged = 1;
      InvalidateRect(m_hwnd, &m_rect, FALSE);
    }
  }
  if (!m_resynced)
    m_frame = frame;
  m_resynced = 0;
  for (const auto& entry : m_sprites.Entries())
  {
#if defined(REB_MINWIN)
    if (m_indexedSpriteFilter && minwin_gdi::DeviceScale() > 1 && entry->m_drawMode == kSpriteKeyed &&
        entry->GetBitmap())
      m_indexedSpriteFilter->EnsureDetail(*entry->GetBitmap());
#endif
    entry->DrawOnto(BackBuffer(), m_originX, m_originY, &area);
  }
  if (anyChanged)
    m_front = 1 - m_front;
}

void SpriteStage::EnableIndexedSpriteFiltering()
{
  m_indexedSpriteFilter = std::make_unique<IndexedSpriteFilter>();
}

void SpriteStage::Pause()
{
  m_paused = 1;
}

void SpriteStage::Resume()
{
  if (!m_paused)
    return;
  const DWORD now = timeGetTime();
  m_paused = 0;
  m_frame = static_cast<int>((now - m_startTime) / m_frameMs);
}

void SpriteStage::Disable()
{
  m_disabled = 1;
}

void SpriteStage::Enable()
{
  m_disabled = 0;
}

void SpriteStage::Resync()
{
  const DWORD now = timeGetTime();
  m_resynced = 1;
  m_frame = static_cast<int>((now - m_startTime) / m_frameMs);
  InvalidateRect(m_hwnd, &m_rect, FALSE);
}

uint32_t SpriteStage::AddAnimator(SpriteAnimator* animator)
{
  m_animators.AddWithId(animator);
  return animator->m_id;
}

int SpriteStage::RemoveAnimator(SpriteAnimator* animator)
{
  int removed = 0;
  if (animator)
  {
    SpriteAnimator* found = m_animators.FindById(animator->m_id);
    if (found && found == animator)
    {
      removed = 1;
      m_animators.Remove(animator);
    }
  }
  return removed;
}

uint32_t SpriteStage::AddSprite(Sprite* sprite)
{
  m_sprites.Add(sprite);
  return sprite->m_id;
}

int SpriteStage::RemoveSprite(uint32_t id)
{
  Sprite* sprite = m_sprites.FindById(id);
  if (sprite)
  {
    m_sprites.Remove(sprite);
    delete sprite;
  }
  return sprite != nullptr;
}

void SpriteStage::Scroll(int dx, int dy)
{
  m_originX += dx;
  m_originY += dy;
  RECT area;
  area.left = m_x;
  area.right = BackBuffer()->Width() + m_x;
  area.top = m_y;
  area.bottom = BackBuffer()->Height() + m_y;
  InvalidateRect(m_hwnd, &area, FALSE);
}

void SpriteStage::StartAnimator(uint32_t id, void* sequence, int a, int b, int c)
{
  auto* animator = static_cast<SpriteAnimator*>(m_animators.FindById(id));
  if (animator)
    animator->Start(a, static_cast<const uint32_t*>(sequence), this, b, c, 0);
}

void SpriteStage::StopAnimator(uint32_t id)
{
  auto* animator = static_cast<SpriteAnimator*>(m_animators.FindById(id));
  if (animator)
    animator->Stop();
}

uint32_t SpriteStage::HitTest(int x, int y)
{
  const int dx = m_originX - m_x;
  const int dy = m_originY - m_y;
  uint32_t id = 0;
  for (const auto& entry : m_animators.Entries())
  {
    SpriteAnimator* animator = entry.get();
    if (animator->HitTest(x + dx, y + dy))
    {
      id = animator->m_id;
      break;
    }
  }
  return id;
}
