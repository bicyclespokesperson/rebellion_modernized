#include <cstdlib>
#include <cstring>

#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{

void* Alloc(size_t n)
{
  return calloc(1, n ? n : 1);
}
void Free(void* p)
{
  free(p);
}

char* DuplicateString(const char* s)
{
  if (!s) {
    return nullptr;
  }
  const size_t n = strlen(s) + 1;
  char* copy = static_cast<char*>(Alloc(n));
  if (copy) {
    memcpy(copy, s, n);
  }
  return copy;
}

Region::~Region()
{
  free(rects_);
}

void Region::Reserve(int n)
{
  if (n <= capacity_) {
    return;
  }
  int capacity = capacity_ ? capacity_ : 8;
  while (capacity < n) {
    capacity *= 2;
  }
  rects_ = static_cast<RECT*>(realloc(rects_, sizeof(RECT) * static_cast<size_t>(capacity)));
  capacity_ = capacity;
}

void Region::Set(const RECT& r)
{
  count_ = 0;
  if (IsEmpty(r)) {
    return;
  }
  Reserve(1);
  rects_[count_++] = r;
}

void Region::CopyFrom(const Region& other)
{
  count_ = 0;
  Reserve(other.count_);
  if (other.count_) {
    memcpy(rects_, other.rects_, sizeof(RECT) * static_cast<size_t>(other.count_));
  }
  count_ = other.count_;
}

void Region::Subtract(const RECT& cut)
{
  if (IsEmpty(cut)) {
    return;
  }
  const int original = count_;
  Reserve(original * 4 + 4);
  int out = 0;
  // Pieces of each rectangle that survive go to the front of the array; the unchanged ones are compacted in place
  // first.
  RECT* pieces = static_cast<RECT*>(malloc(sizeof(RECT) * static_cast<size_t>(original * 4 + 4)));
  for (int i = 0; i < original; ++i) {
    const RECT r = rects_[i];
    RECT overlap;
    if (!Intersect(&overlap, r, cut)) {
      pieces[out++] = r;
      continue;
    }
    if (overlap.top > r.top) {
      pieces[out++] = MakeRect(r.left, r.top, r.right, overlap.top);
    }
    if (overlap.left > r.left) {
      pieces[out++] = MakeRect(r.left, overlap.top, overlap.left, overlap.bottom);
    }
    if (overlap.right < r.right) {
      pieces[out++] = MakeRect(overlap.right, overlap.top, r.right, overlap.bottom);
    }
    if (overlap.bottom < r.bottom) {
      pieces[out++] = MakeRect(r.left, overlap.bottom, r.right, r.bottom);
    }
  }
  memcpy(rects_, pieces, sizeof(RECT) * static_cast<size_t>(out));
  free(pieces);
  count_ = out;
}

void Region::IntersectWith(const RECT& clip)
{
  int out = 0;
  for (int i = 0; i < count_; ++i) {
    RECT r;
    if (Intersect(&r, rects_[i], clip)) {
      rects_[out++] = r;
    }
  }
  count_ = out;
}

RECT Region::Bounds() const
{
  RECT bounds = MakeRect(0, 0, 0, 0);
  for (int i = 0; i < count_; ++i) {
    bounds = Union(bounds, rects_[i]);
  }
  return bounds;
}

bool Region::Contains(int x, int y) const
{
  for (int i = 0; i < count_; ++i) {
    if (x >= rects_[i].left && x < rects_[i].right && y >= rects_[i].top && y < rects_[i].bottom) {
      return true;
    }
  }
  return false;
}

} // namespace minwin
