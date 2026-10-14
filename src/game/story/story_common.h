#pragma once
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/status_code.h"

class EventKindSet;

// Helpers shared by the story characters and missions (bodies the original compiled once per class).
namespace story
{

constexpr int kViewAlliance = 1;
constexpr int kViewEmpire = 2;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNotSupported = 0x17;

// The event kind set of a class is built on first use from its base's.
template <typename Init>
int buildKindsOnce(int* ready, Init init)
{
  int result = 1;
  if (!*ready)
  {
    *ready = 1;
    result = init();
  }
  return result;
}

template <typename Kinds>
int copyKinds(int ready, Kinds& kinds, Kinds* out)
{
  if (ready)
    *out = kinds;
  return ready;
}

template <typename Kinds>
int checkOrder(int ready, Kinds& kinds, int orderKind, StatusCode* out)
{
  *out = StatusCode(kStatusCategory, kStatusNotSupported);
  if (ready && kinds.Contains(orderKind))
    *out = StatusCode(kStatusCategory, kStatusOk);
  return ready;
}

// Flag property of a master object: flips `mask` of the field and has both side views copy it, then raises `changed`.
template <typename Object, typename Owner, typename Apply, typename Changed>
int setFlagProperty(Object* self, uint32_t Owner::* field, uint32_t mask, int on, Apply apply, Changed changed)
{
  const int result = self->IsMasterView();
  if (result && UpdateFlagBit(mask, on, &(self->*field)))
  {
    apply(static_cast<Object*>(self->ViewAs(kViewAlliance)));
    apply(static_cast<Object*>(self->ViewAs(kViewEmpire)));
    changed();
  }
  return result;
}

// Side view copy of a flag property: when the view is told about it, the master's bit is copied and `notify` runs with
// the new value.
template <typename Object, typename Owner, typename Notify>
int applyFlagProperty(Object* self, uint32_t Owner::* field, uint32_t mask, NotifyContext* ctx, Notify notify)
{
  int result = self->IsSideView();
  if (result)
  {
    auto* master = static_cast<Object*>(self->GetViewObject(0));
    if (self->MatchesSideMask(ctx->m_flag08))
    {
      if (UpdateFlagBits(mask, master->*field, &(self->*field)))
      {
        if (CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(self->ViewSide())))
          result = notify((self->*field & mask) != 0 ? 1 : 0);
      }
    }
  }
  return result;
}

} // namespace story
