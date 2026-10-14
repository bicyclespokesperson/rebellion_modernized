#pragma once
#include <cstdint>

#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"

// Shared shape of the flag properties that the fleet classes add to GameObj's (IsDecoyed, UnderRepair, FastRepair, ...):
// the master's setter flips the bit and applies it to both side views, the view's Apply copies it from the master and
// raises the OnXNotified event when the game hook and the context ask for it.
namespace fleet
{

template <typename Object, typename Notify>
int applyFlagToView(Object* self, uint32_t Object::* field, uint32_t mask, NotifyContext* ctx, Notify notify)
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
        {
          result = notify((self->*field & mask) != 0 ? 1 : 0);
        }
      }
    }
  }
  return result;
}

} // namespace fleet
