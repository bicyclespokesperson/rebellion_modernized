#include "battle_ui/orders.h"

#include <algorithm>
#include <cmath>

#include "battle_ui/selection.h"

namespace reb::battle_ui
{

using battle::Vec2;

namespace
{

constexpr float kHookThreshold = 0.35f; // sine of the drag's angle off the line of attack

Vec2 Centroid(const battle::ViewModel& view, const std::vector<battle::UnitIndex>& units)
{
  Vec2 sum;
  int count = 0;
  for (battle::UnitIndex unit : units) {
    if (const battle::ViewUnit* found = FindUnit(view, unit)) {
      sum.x += found->position.x;
      sum.y += found->position.y;
      ++count;
    }
  }
  return count ? Vec2{sum.x / count, sum.y / count} : sum;
}

} // namespace

std::optional<battle::Command> SelectionOrder(const OrderContext& context, battle::OrderPayload order)
{
  if (context.units.empty()) {
    return std::nullopt;
  }
  battle::Command command;
  command.tick = context.tick;
  command.side = context.side;
  command.target.units = context.units;
  command.order = std::move(order);
  command.queue = context.queue;
  return command;
}

std::optional<battle::Command> ClickOrder(const OrderContext& context, const battle::ViewModel& view,
                                          battle::UnitIndex clicked, Vec2 world, bool addTarget)
{
  const battle::ViewUnit* target = clicked == battle::kNoUnit ? nullptr : FindUnit(view, clicked);
  if (target && target->side != context.side) {
    OrderContext attack = context;
    attack.queue = context.queue || addTarget;
    return SelectionOrder(attack, battle::OrderAttack{{clicked}});
  }
  if (target && std::ranges::find(context.units, clicked) == context.units.end()) {
    return SelectionOrder(context, battle::OrderEscort{clicked});
  }
  return SelectionOrder(context, battle::OrderMove{world, std::nullopt});
}

std::optional<battle::Command> DragOrder(const OrderContext& context, const battle::ViewModel& view,
                                         battle::UnitIndex startUnit, Vec2 from, Vec2 to)
{
  const battle::ViewUnit* target = startUnit == battle::kNoUnit ? nullptr : FindUnit(view, startUnit);
  if (target && target->side != context.side) {
    const Vec2 origin = Centroid(view, context.units);
    const Vec2 attack{target->position.x - origin.x, target->position.y - origin.y};
    const Vec2 drag{to.x - from.x, to.y - from.y};
    const float lengths = std::hypot(attack.x, attack.y) * std::hypot(drag.x, drag.y);
    const float sine = lengths > 0.0f ? (attack.x * drag.y - attack.y * drag.x) / lengths : 0.0f;
    battle::Approach approach = battle::Approach::Direct;
    if (sine > kHookThreshold) {
      approach = battle::Approach::LeftHook;
    } else if (sine < -kHookThreshold) {
      approach = battle::Approach::RightHook;
    }
    return SelectionOrder(context, battle::OrderApproach{startUnit, approach});
  }
  return SelectionOrder(context, battle::OrderMove{from, std::atan2(to.y - from.y, to.x - from.x)});
}

} // namespace reb::battle_ui
