// Developer entry point: REB_SPACE_BATTLE_DEMO=<snapshot file> opens straight into a live battle screen on the
// simulator, with both sides under the AI until the player gives an order; Esc or the end of the battle quits.
// REB_SPACE_BATTLE_DEMO=fabricated uses the fabricated stand-in battle instead (REB_SPACE_BATTLE_DEMO_FLEETS copies
// of its fleets), REB_SPACE_BATTLE_SIDE=empire commands the Empire, and REB_SPACE_BATTLE_DEMO_SECONDS=<s> quits after
// that much real time.
// With REB_SPACE_BATTLE_REVIEW_DIR=<dir> it instead renders a fixed sequence of review captures to PNG files in a
// hidden window and exits (the renderer smoke of docs/design/tactical_combat.md §11.7). REB_SPACE_BATTLE_REVIEW_SIZE
// (WxH, default 1920x1080) and REB_BATTLE_UI_SCALE set the capture size and UI scale.
#pragma once
#include <optional>

namespace reb::native::space_battle
{

// The process exit code when the demo was requested and ran; nullopt otherwise.
std::optional<int> RunDemoIfRequested();

} // namespace reb::native::space_battle
