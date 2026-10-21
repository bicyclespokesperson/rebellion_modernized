#pragma once
#include <cstdint>
#include <optional>
#include <vector>

#include "space_battle/commands.h"
#include "space_battle/view_model.h"

// Mouse gestures on the battle view -> live orders (tactical_combat.md §5.4).
namespace reb::battle_ui
{

struct OrderContext {
  battle::Side side = battle::Side::Alliance;
  int64_t tick = 0;
  std::vector<battle::UnitIndex> units; // the orderable selection
  bool queue = false;                   // Shift: after the current orders
};

// Right-click. On an enemy: attack it (`addTarget`, Ctrl, appends it to the attack list instead). On a friendly unit
// outside the selection: escort it. On the plane: move there.
std::optional<battle::Command> ClickOrder(const OrderContext& context, const battle::ViewModel& view,
                                          battle::UnitIndex clicked, battle::Vec2 world, bool addTarget);

// Right-drag. From an enemy: approach it, hooking round the side the drag points to (Direct when the drag is along
// the line of attack). From the plane: move to the start, then face along the drag.
std::optional<battle::Command> DragOrder(const OrderContext& context, const battle::ViewModel& view,
                                         battle::UnitIndex startUnit, battle::Vec2 from, battle::Vec2 to);

// Any other order for the selection (Hold, Withdraw, ...); nullopt when nothing is selected.
std::optional<battle::Command> SelectionOrder(const OrderContext& context, battle::OrderPayload order);

} // namespace reb::battle_ui
