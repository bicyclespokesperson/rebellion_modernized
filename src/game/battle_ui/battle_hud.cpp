#include "battle_ui/battle_hud.h"

#include <cstdio>

#include "battle_ui/world_painter.h"

namespace reb::battle_ui
{

using battle::Side;

namespace
{

const char* SideName(Side side)
{
  return side == Side::Alliance ? "ALLIANCE" : "EMPIRE";
}

std::string Percent(float fraction)
{
  return std::to_string(static_cast<int>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * 100.0f))) + "%";
}

void DrawPlayIcon(DrawList& draw, const Rect& rect, bool paused, Color color, float s)
{
  const Point c = rect.Center();
  if (paused) {
    draw.FillTriangle({c.x - 4.0f * s, c.y - 6.0f * s}, {c.x + 6.0f * s, c.y}, {c.x - 4.0f * s, c.y + 6.0f * s}, color);
    return;
  }
  draw.FillRect({c.x - 5.0f * s, c.y - 6.0f * s, 3.5f * s, 12.0f * s}, color);
  draw.FillRect({c.x + 1.5f * s, c.y - 6.0f * s, 3.5f * s, 12.0f * s}, color);
}

void SpeedLabel(char* out, size_t size, float speed)
{
  if (speed < 1.0f) {
    std::snprintf(out, size, "%.1fx", speed);
  } else {
    std::snprintf(out, size, "%dx", static_cast<int>(speed));
  }
}

// A labelled bar row: name, bar, value text.
void BarRow(Ui& ui, float x, float y, float width, std::string_view name, float fraction, Color fill,
            std::string_view value)
{
  const float rowHeight = ui.S(16.0f);
  const float nameWidth = ui.S(56.0f);
  const float valueWidth = ui.S(48.0f);
  const Theme& theme = ui.GetTheme();
  ui.Label({x, y, nameWidth, rowHeight}, name, {11.0f, theme.textDim});
  const float barWidth = width - nameWidth - valueWidth;
  ui.Bar({x + nameWidth, y + rowHeight * 0.5f - ui.S(3.5f), barWidth, ui.S(7.0f)}, fraction, fill);
  ui.Label({x + nameWidth + barWidth, y, valueWidth, rowHeight}, value, {11.0f, theme.text, Align::End});
}

void SystemPips(Ui& ui, float x, float y, std::string_view name, std::initializer_list<int> hits, int pipsPerDrive)
{
  const Theme& theme = ui.GetTheme();
  ui.Label({x, y, ui.S(30.0f), ui.S(14.0f)}, name, {10.0f, theme.textDim});
  float px = x + ui.S(30.0f);
  for (const int hit : hits) {
    for (int pip = 0; pip < pipsPerDrive; ++pip) {
      const bool working = pip >= hit;
      const Rect box{px, y + ui.S(3.0f), ui.S(5.0f), ui.S(8.0f)};
      ui.Draw().FillRect(box, working ? theme.hullGood.WithAlpha(220) : theme.hullPoor.WithAlpha(200));
      px += ui.S(7.0f);
    }
    px += ui.S(3.0f);
  }
}

std::string StatusLine(const battle::ViewUnit& unit)
{
  std::string status;
  const auto add = [&status](const std::string& text) {
    status += status.empty() ? text : "  " + text;
  };
  if (unit.flags & battle::kViewWithdrawing) {
    add("Withdrawing " + Percent(unit.spool));
  }
  if (unit.flags & battle::kViewInEnemyWell) {
    add("In enemy gravity well");
  }
  if (unit.flags & battle::kViewUnderPlanetaryGuns) {
    add("Under planetary guns");
  }
  if (unit.flags & battle::kViewTractored) {
    add("Held by tractor");
  }
  if (unit.flags & battle::kViewTractoring) {
    add("Tractor lock");
  }
  if (unit.flags & battle::kViewDisrupted) {
    add("Ion disrupted");
  }
  if (unit.flags & battle::kViewFlagship) {
    add("Flagship");
  }
  return status;
}

void DrawSingle(Ui& ui, BattleScreen& screen, const battle::ViewUnit& unit)
{
  const Theme& theme = ui.GetTheme();
  const BattleScreen::UnitLabel& label = screen.Label(unit.unit);
  const Color faction = FactionColor(theme, unit.side);
  const Rect content = ui.Panel(screen.Layout().infoBox, label.name);
  float y = content.y;
  const float row = ui.S(17.0f);
  ui.Label({content.x, y, content.w * 0.7f, row}, label.className, {11.0f, theme.textDim});
  ui.Label({content.x, y, content.w, row}, SideName(unit.side),
           {11.0f, faction, Align::End, VAlign::Middle, FontWeight::Bold});
  y += row + ui.S(2.0f);
  if (unit.kind == battle::ViewUnitKind::Squadron) {
    const float craft = unit.craftMax > 0 ? static_cast<float>(unit.craft) / static_cast<float>(unit.craftMax) : 0.0f;
    BarRow(ui, content.x, y, content.w, "Craft", craft, HullColor(theme, craft),
           std::to_string(unit.craft) + " / " + std::to_string(unit.craftMax));
    y += row;
    BarRow(ui, content.x, y, content.w, "Shields", unit.shieldFraction, theme.shield, Percent(unit.shieldFraction));
    y += row;
  } else {
    BarRow(ui, content.x, y, content.w, "Shields", unit.shieldFraction, theme.shield, Percent(unit.shieldFraction));
    y += row;
    BarRow(ui, content.x, y, content.w, "Hull", unit.hullFraction, HullColor(theme, unit.hullFraction),
           Percent(unit.hullFraction));
    y += row + ui.S(2.0f);
    const battle::CriticalHits& c = unit.criticals;
    const float column = content.w / 5.0f;
    SystemPips(ui, content.x, y, "ENG", {c.speed}, 4);
    SystemPips(ui, content.x + column, y, "SHD", {c.shieldRecharge}, 4);
    SystemPips(ui, content.x + column * 2.0f, y, "WPN", {c.weaponRecharge}, 4);
    SystemPips(ui, content.x + column * 3.0f, y, "TRC", {c.tractor}, 4);
    SystemPips(ui, content.x + column * 4.0f, y, "HYP", {c.hyperdrivePrimary, c.hyperdriveBackup}, 1);
    y += row;
  }
  const float textSize = ui.S(11.0f);
  const float lineHeight = ui.Text().LineHeight(textSize) + ui.S(1.0f);
  for (const std::string& line : WrapText(ui.Text(), unit.intent, textSize, content.w, 2)) {
    ui.Label({content.x, y, content.w, lineHeight}, line, {11.0f, theme.text});
    y += lineHeight;
  }
  if (const std::string status = StatusLine(unit); !status.empty() && y + lineHeight <= content.Bottom() + ui.S(2.0f)) {
    ui.Label({content.x, content.Bottom() - lineHeight, content.w, lineHeight}, status, {11.0f, theme.hullFair});
  }
}

void DrawMany(Ui& ui, BattleScreen& screen)
{
  const Theme& theme = ui.GetTheme();
  const std::vector<battle::UnitIndex> units = screen.GetSelection().Units();
  const Rect content = ui.Panel(screen.Layout().infoBox, std::to_string(units.size()) + " selected");
  const float rowHeight = ui.S(19.0f);
  const int clicked
      = ui.List(Id("info.selection"), content, static_cast<int>(units.size()), rowHeight, -1,
                [&](int index, const Rect& row, bool, bool) {
                  const battle::ViewUnit* unit = FindUnit(screen.View(), units[index]);
                  if (!unit) {
                    return;
                  }
                  const float barWidth = ui.S(70.0f);
                  ui.Label({row.x + ui.S(2.0f), row.y, row.w - barWidth - ui.S(10.0f), row.h},
                           screen.Label(unit->unit).name, {12.0f, theme.text});
                  const bool squadron = unit->kind == battle::ViewUnitKind::Squadron;
                  const float fraction = squadron && unit->craftMax > 0
                                             ? static_cast<float>(unit->craft) / unit->craftMax
                                             : unit->hullFraction;
                  ui.Bar({row.Right() - barWidth - ui.S(6.0f), row.Center().y - ui.S(3.0f), barWidth, ui.S(6.0f)},
                         fraction, HullColor(theme, fraction));
                });
  if (clicked >= 0) {
    screen.GetSelection().Set({units[clicked]});
  }
}

} // namespace

std::string FormatClock(float seconds)
{
  const int total = std::max(0, static_cast<int>(seconds));
  char text[16];
  std::snprintf(text, sizeof text, "%02d:%02d", total / 60, total % 60);
  return text;
}

void TopBarPanel::Draw(Ui& ui, BattleScreen& screen)
{
  const Theme& theme = ui.GetTheme();
  const Rect bar = screen.Layout().topBar;
  const battle::ViewModel& view = screen.View();
  ui.Panel(bar);
  const float pad = ui.S(6.0f);
  const float buttonHeight = ui.S(24.0f);
  const float y = bar.y + (bar.h - buttonHeight) * 0.5f;
  float x = bar.x + pad;

  const WidgetId pauseId = Id("top.pause");
  const Rect pause{x, y, buttonHeight + ui.S(6.0f), buttonHeight};
  if (ui.Selectable(pauseId, pause, "", screen.Paused())) {
    screen.SetPaused(!screen.Paused());
  }
  DrawPlayIcon(ui.Draw(), pause, screen.Paused(), theme.text, ui.Scale());
  ui.Tooltip(pauseId, screen.Paused() ? "Resume (Space)" : "Pause (Space)");
  x = pause.Right() + ui.S(4.0f);

  for (int i = 0; i < static_cast<int>(BattleScreen::kSpeeds.size()); ++i) {
    char label[8];
    SpeedLabel(label, sizeof label, BattleScreen::kSpeeds[i]);
    const WidgetId id = Id(Id("top.speed"), i);
    const Rect button{x, y, ui.S(40.0f), buttonHeight};
    if (ui.Selectable(id, button, label, !screen.Paused() && screen.SpeedIndex() == i)) {
      screen.SetSpeedIndex(i);
      screen.SetPaused(false);
    }
    ui.Tooltip(id, "Battle speed (- and =)");
    x = button.Right() + ui.S(2.0f);
  }
  x += ui.S(14.0f);
  ui.Label({x, bar.y, ui.S(60.0f), bar.h}, FormatClock(view.battleSeconds),
           {16.0f, theme.title, Align::Start, VAlign::Middle, FontWeight::Bold});
  x += ui.S(70.0f);

  const WidgetId viewId = Id("top.view");
  const Rect viewButton{x, y, ui.S(64.0f), buttonHeight};
  if (ui.Selectable(viewId, viewButton, "Angled", screen.AngledView())) {
    screen.ToggleView();
  }
  ui.Tooltip(viewId, screen.AngledView() ? "Back to the top-down view (V)" : "Angled view of the battle plane (V)");
  x = viewButton.Right() + ui.S(4.0f);

  const WidgetId followId = Id("top.follow");
  const Rect followButton{x, y, ui.S(64.0f), buttonHeight};
  const bool following = screen.Following() != battle::kNoUnit;
  if (ui.Selectable(followId, followButton, "Follow", following,
                    following || !screen.GetSelection().Empty())) {
    screen.ToggleFollow();
  }
  ui.Tooltip(followId, following ? "Following " + screen.Label(screen.Following()).name
                                       + ": click, press F or pan the view to release the camera"
                                 : "Keep the camera on the selected ship as it moves (F)");
  x = followButton.Right() + ui.S(16.0f);

  const Side player = screen.PlayerSide();
  const Side enemy = player == Side::Alliance ? Side::Empire : Side::Alliance;
  const float signalsWidth = ui.S(110.0f);
  const float available = bar.Right() - pad - signalsWidth - x;
  const float block = std::min(ui.S(300.0f), available * 0.5f);
  for (const Side side : {player, enemy}) {
    if (block < ui.S(150.0f)) {
      break;
    }
    const battle::ViewSide& state = view.sides[static_cast<int>(side)];
    const Color color = FactionColor(theme, side);
    const float fraction = state.initialStrength > 0.0f ? state.strength / state.initialStrength : 0.0f;
    const float nameWidth = ui.S(70.0f);
    const float valueWidth = ui.S(40.0f);
    ui.Label({x, bar.y, nameWidth, bar.h}, SideName(side),
             {12.0f, color, Align::Start, VAlign::Middle, FontWeight::Bold});
    const float barWidth = block - nameWidth - valueWidth - ui.S(16.0f);
    ui.Bar({x + nameWidth, bar.Center().y - ui.S(4.0f), barWidth, ui.S(8.0f)}, fraction, color.WithAlpha(210));
    ui.Label({x + nameWidth + barWidth, bar.y, valueWidth, bar.h}, state.withdrawing ? "RET" : Percent(fraction),
             {12.0f, theme.text, Align::End});
    x += block;
  }
  const battle::ViewSide& own = view.sides[static_cast<int>(player)];
  if (own.signalsMax > 0) {
    ui.Label({bar.Right() - pad - signalsWidth, bar.y, signalsWidth, bar.h},
             "Signals " + std::to_string(own.signalsReady) + "/" + std::to_string(own.signalsMax),
             {12.0f, theme.textDim, Align::End});
  }
}

void InfoBoxPanel::Draw(Ui& ui, BattleScreen& screen)
{
  const std::vector<battle::UnitIndex>& units = screen.GetSelection().Units();
  if (units.empty() || !screen.InfoBoxVisible()) {
    return;
  }
  if (units.size() == 1) {
    if (const battle::ViewUnit* unit = FindUnit(screen.View(), units.front())) {
      DrawSingle(ui, screen, *unit);
    }
    return;
  }
  DrawMany(ui, screen);
}

void KeyHintsPanel::Draw(Ui& ui, BattleScreen& screen)
{
  ui.Label(
      screen.Layout().hints,
      "Space pause   -/= speed   Tab overlay   V angled view   Q/E rotate   Home frame   F follow   Ctrl+1-9 group",
      {11.0f, ui.GetTheme().textDim, Align::End, VAlign::Middle});
}

} // namespace reb::battle_ui
