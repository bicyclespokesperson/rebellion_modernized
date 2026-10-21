// The right-hand column (tactical_combat.md §5.6, §6.12, §10.2): commander signals, the Death Star controls and the
// event log; during the paused start, the briefing (§4) instead, with the deployment handles on the battle plane.
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <ranges>

#include "battle_ui/battle_hud.h"
#include "battle_ui/command_hud.h"
#include "battle_ui/plane_shapes.h"
#include "battle_ui/hud_draw.h"
#include "battle_ui/selection.h"
#include "battle_ui/world_painter.h"
#include "space_battle/deployment_geometry.h"

namespace reb::battle_ui
{

using battle::kNoGroup;
using battle::kNoUnit;
using battle::UnitIndex;

namespace
{

constexpr float kEventRow = 18.0f;
constexpr size_t kEventRowsShown = 14;
constexpr float kRepeatSeconds = 10.0f;
constexpr float kHandleRadius = 12.0f;
constexpr float kDeployDragThreshold = 4.0f;

const battle::ViewUnit* DeathStar(const battle::ViewModel& view)
{
  const auto it = std::ranges::find_if(view.units, [](const battle::ViewUnit& unit) {
    return unit.kind == battle::ViewUnitKind::DeathStar && Selectable(unit);
  });
  return it == view.units.end() ? nullptr : &*it;
}

std::string Percent(float fraction)
{
  return std::to_string(static_cast<int>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * 100.0f))) + "%";
}

// A battery that fired within this long is shown as firing at its target.
constexpr float kBatteryActiveSeconds = 4.0f;

// The planetary batteries (§6.13) of `side` that the viewer knows of, with their index in the view.
std::vector<std::pair<int, const battle::ViewDefense*>> KnownBatteries(const battle::ViewModel& view, battle::Side side,
                                                                       battle::Side viewer)
{
  std::vector<std::pair<int, const battle::ViewDefense*>> batteries;
  for (size_t i = 0; i < view.defenses.size(); ++i) {
    if (view.defenses[i].side == side && DefenseVisible(view.defenses[i], viewer)) {
      batteries.emplace_back(static_cast<int>(i), &view.defenses[i]);
    }
  }
  return batteries;
}

std::string BatteryRole(const battle::ViewDefense& defense)
{
  return defense.weapon == battle::WeaponKind::Ion
             ? "Ion cannon: drains shields, then knocks out the engines of a ship whose shields are down."
             : "Turbolaser battery: damages shields and hulls.";
}

battle::Vec2 DeploymentTarget(const battle::ViewModel& view, const std::vector<UnitIndex>& members,
                              battle::Vec2 centroid, battle::Vec2 target, const battle::ViewDeployZone& zone)
{
  const battle::DeploymentBounds bounds{zone.center, zone.radius, view.planetCenter, view.planetRadius,
                                        view.boundsMax.x};
  std::vector<battle::DeploymentHull> hulls;
  for (UnitIndex index : members) {
    const battle::ViewUnit& unit = *FindUnit(view, index);
    const bool solid = unit.kind != battle::ViewUnitKind::Squadron;
    hulls.push_back({unit.position - centroid, solid ? unit.radius : 0.0f, solid});
  }
  battle::Vec2 anchor = centroid;
  if (!battle::DeploymentFits(hulls, anchor, bounds) && !battle::FindDeploymentAnchor(hulls, anchor, bounds)) {
    return centroid;
  }
  return battle::ClampDeploymentDrag(hulls, anchor, target, bounds);
}

} // namespace

void CommandHud::DrawRightColumn(Ui& ui)
{
  const Rect column = m_screen.Layout().events;
  if (column.Empty()) {
    return;
  }
  if (m_briefing) {
    DrawBriefing(ui, column);
    return;
  }
  const float used = DrawCommandPanel(ui, column);
  const float gap = ui.S(8.0f);
  DrawEvents(ui, {column.x, column.y + used + gap, column.w, column.h - used - gap});
}

float CommandHud::DrawCommandPanel(Ui& ui, const Rect& column)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  const battle::ViewSide& own = view.sides[static_cast<int>(m_screen.PlayerSide())];
  const battle::ViewUnit* deathStar = DeathStar(view);
  const float title = ui.S(theme.rowHeight);
  const float pad = ui.S(6.0f);
  const float signalRow = ui.S(26.0f);
  const float deathStarHeight = deathStar ? ui.S(62.0f) : 0.0f;
  const battle::Side player = m_screen.PlayerSide();
  const std::array<std::vector<std::pair<int, const battle::ViewDefense*>>, 2> batteries{
      KnownBatteries(view, player, player), KnownBatteries(view, Enemy(), player)};
  const float batteryRow = ui.S(17.0f);
  const float batteryCaption = ui.S(17.0f);
  float batteriesHeight = 0.0f;
  for (const auto& list : batteries) {
    batteriesHeight += list.empty() ? 0.0f : batteryCaption + batteryRow * static_cast<float>(list.size()) + pad;
  }
  const float height = m_commandCollapsed ? title
                                          : title + pad + ui.S(16.0f) + 2.0f * signalRow + ui.S(4.0f) + deathStarHeight
                                                + batteriesHeight + pad;
  const Rect panel{column.x, column.y, column.w, std::min(height, column.h)};
  ui.Panel(panel, "COMMAND");
  const Rect titleBar{panel.x, panel.y, panel.w, title};
  if (hud::FoldButton(ui, hud_id::CollapseCommand(), titleBar, m_commandCollapsed, "signals and options")) {
    m_commandCollapsed = !m_commandCollapsed;
  }
  Record(hud_id::CollapseCommand(), hud::FoldRect(titleBar));
  const Rect gear{titleBar.Right() - 2.0f * title, titleBar.y, title, title};
  if (ui.Region(hud_id::Options(), gear)) {
    OpenPopupAt(PopupKind::Options, gear);
  }
  DrawIcon(ui.Draw(), Icon::Gear, gear.Inset(title * 0.25f),
           ui.Hovered(hud_id::Options()) || m_popup.kind == PopupKind::Options ? theme.text : theme.textDim,
           ui.Scale());
  ui.Tooltip(hud_id::Options(), "Options: auto-pause events, command phases, colours and HUD size.");
  Record(hud_id::Options(), gear);
  if (m_commandCollapsed) {
    return panel.h;
  }

  float y = panel.y + title + pad;
  const float x = panel.x + ui.S(8.0f);
  const float w = panel.w - ui.S(16.0f);
  hud::Caption(ui, {x, y, w, ui.S(16.0f)}, "SIGNALS");
  for (int i = 0; i < own.signalsMax; ++i) {
    const Point pip{x + w - static_cast<float>(own.signalsMax - i) * ui.S(11.0f) + ui.S(5.0f), y + ui.S(8.0f)};
    if (i < own.signalsReady) {
      ui.Draw().FillCircle(pip, ui.S(4.0f), Rgb(0xffd25a));
    } else {
      ui.Draw().Circle(pip, ui.S(4.0f), theme.textDim, Stroke{ui.S(1.0f)});
    }
  }
  if (own.signalsMax == 0) {
    ui.Label({x, y, w, ui.S(16.0f)}, "no fleet admiral", {10.0f, theme.textDim, Align::End});
  }
  y += ui.S(16.0f);
  const float half = (w - ui.S(4.0f)) * 0.5f;
  for (int i = 0; i < 4; ++i) {
    const auto signal = static_cast<battle::SignalKind>(i);
    const OptionText& text = Describe(signal);
    const WidgetId id = hud_id::Signal(signal);
    const Rect button{x + static_cast<float>(i % 2) * (half + ui.S(4.0f)),
                      y + static_cast<float>(i / 2) * (signalRow + ui.S(4.0f)), half, signalRow};
    const bool ready = own.signalsReady > 0 && !view.finished;
    const bool armed = signal == battle::SignalKind::ConcentrateFire && m_pick.mode == PickMode::ConcentrateFire;
    if (hud::Button(ui, id, button, SignalIcon(signal), text.shortName, ready, armed)) {
      if (armed) {
        m_pick = {};
      } else if (signal == battle::SignalKind::ConcentrateFire) {
        StartPick(PickMode::ConcentrateFire);
      } else {
        battle::Command command;
        command.order = battle::OrderSignal{signal, kNoUnit};
        if (Submit(std::move(command))) {
          AddToast("Signal: " + std::string(text.name));
        }
      }
    }
    std::string tip = std::string(text.name) + ": " + std::string(text.help);
    tip += own.signalsMax == 0 ? " Needs a fleet admiral."
                               : " Uses one of the admiral's charges, which recharge slowly.";
    ui.Tooltip(id, tip);
    Record(id, button);
  }
  y += 2.0f * signalRow + ui.S(4.0f) + pad;

  for (size_t list = 0; list < batteries.size(); ++list) {
    if (batteries[list].empty()) {
      continue;
    }
    const bool own = list == 0;
    ui.Draw().FillRect({x, y - pad * 0.5f, w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(80));
    hud::Caption(ui, {x, y, w, ui.S(16.0f)}, own ? "PLANETARY DEFENSES" : "ENEMY PLANETARY DEFENSES");
    y += batteryCaption;
    for (const auto& [index, defense] : batteries[list]) {
      const Rect row{x, y, w, batteryRow};
      const Color weapon = defense->weapon == battle::WeaponKind::Ion ? theme.ion : FactionColor(theme, defense->side);
      const bool active = defense->sinceFired >= 0.0f && defense->sinceFired < kBatteryActiveSeconds
                          && defense->target != kNoUnit;
      DrawIcon(ui.Draw(), Icon::PlanetaryBattery, {row.x, row.y + ui.S(1.0f), ui.S(15.0f), ui.S(15.0f)},
               active ? weapon : weapon.WithAlpha(150), ui.Scale());
      const std::string status = active ? "firing at " + m_screen.Label(defense->target).name : "no target in range";
      const float statusWidth = std::min(w * 0.55f, ui.TextWidth(status, 10.0f) + ui.S(4.0f));
      ui.Label({row.x + ui.S(19.0f), row.y, w - statusWidth - ui.S(21.0f), row.h},
               defense->name.empty() ? "Battery" : defense->name, {11.0f, theme.text});
      ui.Label({row.x, row.y, w, row.h}, status, {10.0f, active ? weapon : theme.textDim, Align::End});
      const WidgetId id = hud_id::Battery(index);
      ui.Region(id, row);
      Record(id, row);
      ui.Tooltip(id, defense->name + ". " + BatteryRole(*defense) + " Fires on "
                         + (own ? "enemy capital ships" : "your capital ships")
                         + " inside the ring around the planet; it cannot be destroyed in this battle."
                         + (own ? " It joins your Concentrate Fire signal when the target is in reach." : ""));
      y += batteryRow;
    }
    y += pad;
  }

  if (!deathStar) {
    return panel.h;
  }
  const bool ours = deathStar->side == m_screen.PlayerSide();
  const float charge = std::max(0.0f, view.superlaserCharge);
  ui.Draw().FillRect({x, y - pad * 0.5f, w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(80));
  hud::Caption(ui, {x, y, w, ui.S(16.0f)}, ours ? "SUPERLASER" : "ENEMY DEATH STAR");
  y += ui.S(17.0f);
  const Color fill = charge >= 1.0f ? Rgb(0x8cff6a) : Rgb(0x6ad25a, 220);
  if (ours) {
    const float buttonWidth = ui.S(64.0f);
    const float valueWidth = ui.S(46.0f);
    ui.Bar({x, y + ui.S(7.0f), w - buttonWidth - valueWidth - ui.S(8.0f), ui.S(8.0f)}, charge, fill);
    ui.Label({x, y, w - buttonWidth - ui.S(6.0f), ui.S(22.0f)}, charge >= 1.0f ? "READY" : Percent(charge),
             {11.0f, charge >= 1.0f ? fill : theme.text, Align::End, VAlign::Middle, FontWeight::Bold});
    const Rect fire{x + w - buttonWidth, y, buttonWidth, ui.S(22.0f)};
    const bool armed = m_pick.mode == PickMode::Superlaser;
    if (hud::Button(ui, hud_id::FireSuperlaser(), fire, Icon::DeathStar, "Fire", charge >= 1.0f && !view.finished,
                    armed)) {
      if (armed) {
        m_pick = {};
      } else {
        StartPick(PickMode::Superlaser);
      }
    }
    ui.Tooltip(hud_id::FireSuperlaser(), charge >= 1.0f ? "Fire the superlaser at an enemy capital ship you pick; it "
                                                          "is destroyed outright and the recharge takes five minutes."
                                                        : "The superlaser is recharging.");
    Record(hud_id::FireSuperlaser(), fire);
    y += ui.S(24.0f);
    ui.Label({x, y, w, ui.S(14.0f)}, "Cannot be damaged by ordinary weapons", {10.0f, theme.textDim});
  } else {
    ui.Bar({x, y + ui.S(7.0f), w - ui.S(70.0f), ui.S(8.0f)}, charge, fill);
    ui.Label({x, y, w, ui.S(22.0f)}, charge >= 1.0f ? "CHARGED" : Percent(charge),
             {11.0f, charge >= 1.0f ? theme.hullPoor : theme.text, Align::End, VAlign::Middle, FontWeight::Bold});
    y += ui.S(24.0f);
    ui.Label({x, y, w, ui.S(14.0f)}, view.deathStarRunOpen ? "Trench run open: select a flight" : "Trench run closed",
             {10.0f, view.deathStarRunOpen ? theme.hullGood : theme.textDim});
  }
  return panel.h;
}

void CommandHud::DrawEvents(Ui& ui, const Rect& rect)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  if (rect.h < ui.S(theme.rowHeight)) {
    return;
  }
  const float title = ui.S(theme.rowHeight);
  // Newest first; a repeat of the same report within a few seconds folds into one row with a count.
  struct Row {
    const battle::ViewEvent* event;
    int count;
  };
  std::vector<Row> rowsShown;
  for (const battle::ViewEvent& event : view.recentEvents | std::views::reverse) {
    const auto same = std::ranges::find_if(rowsShown, [&](const Row& row) {
      return row.event->text == event.text && row.event->unit == event.unit && row.event->side == event.side
             && row.event->time - event.time <= kRepeatSeconds;
    });
    if (same != rowsShown.end()) {
      ++same->count;
    } else {
      rowsShown.push_back({&event, 1});
    }
  }
  const float rowHeight = ui.S(kEventRow);
  const float wanted = title + 2.0f * ui.S(theme.padding)
                       + rowHeight * static_cast<float>(std::clamp<size_t>(rowsShown.size(), 1, kEventRowsShown));
  const Rect panel{rect.x, rect.y, rect.w, m_eventsCollapsed ? title : std::min(rect.h, wanted)};
  const Rect content = ui.Panel(panel, "EVENTS");
  const Rect titleBar{panel.x, panel.y, panel.w, title};
  if (hud::FoldButton(ui, hud_id::CollapseEvents(), titleBar, m_eventsCollapsed, "the event log")) {
    m_eventsCollapsed = !m_eventsCollapsed;
  }
  Record(hud_id::CollapseEvents(), hud::FoldRect(titleBar));
  if (m_eventsCollapsed) {
    return;
  }
  if (rowsShown.empty()) {
    ui.Label({content.x, content.y, content.w, rowHeight}, "Nothing to report yet.", {11.0f, theme.textDim});
    return;
  }
  const WidgetId listId = Id("hud.events");
  const bool clicked = ui.Region(listId, content);
  const float row = rowHeight;
  const int count = static_cast<int>(rowsShown.size());
  const float scroll = ui.ScrollOffset(listId, row * static_cast<float>(count), content.h, row);
  // The unit an event is about: the simulator says so, or failing that the event text starts with its name.
  const auto subjectOf = [&](const battle::ViewEvent& event) -> UnitIndex {
    if (event.unit != kNoUnit) {
      return event.unit;
    }
    UnitIndex best = kNoUnit;
    size_t bestLength = 0;
    for (const battle::ViewUnit& unit : view.units) {
      const std::string& name = m_screen.Label(unit.unit).name;
      if (!name.empty() && name.size() > bestLength && event.text.starts_with(name)) {
        best = unit.unit;
        bestLength = name.size();
      }
    }
    return best;
  };
  ui.Draw().PushClip(content);
  for (int i = 0; i < count; ++i) {
    const battle::ViewEvent& event = *rowsShown[i].event;
    std::string text = event.text;
    // Snapshot names can be empty, which leaves the simulator's text starting at the verb: name the unit here.
    if (event.unit != kNoUnit && (text.empty() || text.front() == ' ' || text.front() == ':')) {
      text = m_screen.Label(event.unit).name + text;
    }
    if (rowsShown[i].count > 1) {
      text += "  x" + std::to_string(rowsShown[i].count);
    }
    const Rect line{content.x, content.y + static_cast<float>(i) * row - scroll, content.w - ui.S(4.0f), row};
    if (line.Bottom() < content.y || line.y > content.Bottom()) {
      continue;
    }
    const bool hovered = ui.Hovered(listId) && line.Contains(ui.Mouse());
    const UnitIndex unit = subjectOf(event);
    if (hovered && unit != kNoUnit) {
      ui.Draw().FillRect(line, theme.rowHover);
    }
    ui.Draw().FillRect({line.x, line.y + ui.S(3.0f), ui.S(3.0f), line.h - ui.S(6.0f)}, FactionColor(theme, event.side));
    ui.Label({line.x + ui.S(7.0f), line.y, ui.S(36.0f), line.h}, FormatClock(event.time), {10.0f, theme.textDim});
    ui.Label({line.x + ui.S(44.0f), line.y, line.w - ui.S(46.0f), line.h}, text,
             {11.0f, i == 0 ? theme.text : Mix(theme.text, theme.textDim, 0.4f)});
    Record(hud_id::EventRow(i), line);
    if (hovered) {
      ui.Tooltip(listId, text + (unit != kNoUnit ? ". Click to centre the view on it." : "."));
    }
    if (clicked && line.Contains(ui.ClickPoint()) && unit != kNoUnit) {
      m_screen.CenterOn(unit);
    }
  }
  ui.Draw().PopClip();
}

void CommandHud::DrawBriefing(Ui& ui, const Rect& column)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  const battle::Side side = m_screen.PlayerSide();
  const Rect content = ui.Panel(column, "BRIEFING");
  const float title = ui.S(theme.rowHeight);
  const Rect gear{column.Right() - title, column.y, title, title};
  if (ui.Region(hud_id::Options(), gear)) {
    OpenPopupAt(PopupKind::Options, gear);
  }
  DrawIcon(ui.Draw(), Icon::Gear, gear.Inset(title * 0.25f), ui.Hovered(hud_id::Options()) ? theme.text : theme.textDim,
           ui.Scale());
  ui.Tooltip(hud_id::Options(), "Options: auto-pause events, command phases, colours and HUD size.");
  Record(hud_id::Options(), gear);

  float y = content.y;
  const float line = ui.Text().LineHeight(ui.S(11.0f)) + ui.S(2.0f);
  const auto paragraph = [&](std::string_view text, Color color) {
    for (const std::string& wrapped : WrapText(ui.Text(), text, ui.S(11.0f), content.w)) {
      ui.Label({content.x, y, content.w, line}, wrapped, {11.0f, color});
      y += line;
    }
  };
  paragraph("The battle waits for you. Drag a group's handle to place it inside your deployment zone, set doctrine "
            "on the card below, then engage.",
            theme.text);
  y += ui.S(6.0f);

  for (const battle::Side of : {side, Enemy()}) {
    const battle::ViewWell* first = nullptr;
    int count = 0;
    for (const battle::ViewWell& well : view.wells) {
      if (well.side == of) {
        if (!first) {
          first = &well;
        }
        ++count;
      }
    }
    if (!first) {
      continue;
    }
    std::string projectors = m_screen.Label(first->projector).name;
    if (count > 1) {
      projectors += " + " + std::to_string(count - 1) + " more";
    }
    const bool several = count > 1;
    std::string line = std::string(of == side ? "Your " : "Enemy ") + projectors
                       + (several ? " project gravity wells: " : " projects a gravity well: ");
    if (of == side) {
      line += several ? "enemy ships inside them can't jump to hyperspace."
                      : "enemy ships inside it can't jump to hyperspace.";
    } else {
      line += several ? "your ships inside them can't jump to hyperspace until the projectors are destroyed or "
                        "disabled by ion fire."
                      : "your ships inside it can't jump to hyperspace until the projector is destroyed or "
                        "disabled by ion fire.";
    }
    paragraph(line, theme.textDim);
    y += ui.S(6.0f);
  }

  for (const battle::Side of : {side, Enemy()}) {
    std::vector<const battle::ViewDefense*> known;
    bool ion = false;
    for (const auto& [index, defense] : KnownBatteries(view, of, side)) {
      known.push_back(defense);
      ion = ion || defense->weapon == battle::WeaponKind::Ion;
    }
    if (known.empty()) {
      continue;
    }
    const bool several = known.size() > 1;
    const std::string fire = several ? " fire" : " fires";
    std::string text;
    if (of == side) {
      text = "Your planet's " + DescribeBatteries(known) + fire + " on enemy capital ships inside the ring around it."
             + (ion ? " Ion fire drains shields, then knocks out engines." : "");
      const bool underGuns = std::ranges::any_of(view.groups, [&](const battle::ViewGroup& group) {
        return group.side == side && !group.flight && group.shipDoctrine.approach == battle::Approach::UnderGuns;
      });
      if (underGuns) {
        text += " Your battle line starts Under the guns: it engages the enemy without leaving the ring.";
      }
    } else {
      text = "The enemy's " + DescribeBatteries(known) + fire + " on your capital ships inside the ring around the planet"
             + (ion ? ": ion fire drains shields, then knocks out engines." : ".")
             + (several ? " They cannot" : " It cannot")
             + " be destroyed in this battle; fight outside the ring or accept the fire.";
    }
    paragraph(text, theme.textDim);
    y += ui.S(6.0f);
  }

  const auto summarize = [&](battle::Side of, bool ownSide) {
    std::map<std::string, std::pair<int, int>> ships;
    std::map<std::string, std::pair<int, int>> squadrons;
    for (const battle::ViewUnit& unit : view.units) {
      if (unit.side != of || (unit.flags & battle::kViewDestroyed)) {
        continue;
      }
      if (!ownSide && !m_screen.Detail(unit.unit).briefingKnown[static_cast<size_t>(side)]) {
        continue;
      }
      std::string name = m_screen.Label(unit.unit).className;
      if (unit.kind == battle::ViewUnitKind::DeathStar) {
        name = "Death Star";
      }
      auto& entry = (unit.kind == battle::ViewUnitKind::Squadron ? squadrons : ships)[name.empty() ? "Unknown" : name];
      ++entry.first;
      entry.second += unit.craft;
    }
    hud::Caption(ui, {content.x, y, content.w, ui.S(16.0f)}, ownSide ? "YOUR FLEET" : "KNOWN ENEMY FORCES");
    ui.Draw().FillRect({content.x, y + ui.S(16.0f), content.w, ui.S(1.0f)}, FactionColor(theme, of).WithAlpha(120));
    y += ui.S(19.0f);
    const auto list = [&](const std::map<std::string, std::pair<int, int>>& entries, bool craft) {
      for (const auto& [name, count] : entries) {
        if (y + line > content.Bottom() - ui.S(44.0f)) {
          ui.Label({content.x, y, content.w, line}, "...", {11.0f, theme.textDim});
          y += line;
          return;
        }
        const std::string amount
            = craft ? "x" + std::to_string(count.first) + ", " + std::to_string(count.second) + " craft"
                    : "x" + std::to_string(count.first);
        const float amountWidth = ui.TextWidth(amount, 11.0f) + ui.S(4.0f);
        ui.Label({content.x + ui.S(4.0f), y, content.w - amountWidth - ui.S(6.0f), line}, name, {11.0f, theme.text});
        ui.Label({content.x, y, content.w, line}, amount, {11.0f, theme.textDim, Align::End});
        y += line;
      }
    };
    if (!ownSide && ships.empty() && squadrons.empty()) {
      paragraph("No enemy composition intelligence available.", theme.textDim);
    }
    list(ships, false);
    list(squadrons, true);
    y += ui.S(8.0f);
  };
  summarize(side, true);
  summarize(Enemy(), false);

  const Rect engage{content.x, content.Bottom() - ui.S(34.0f), content.w, ui.S(34.0f)};
  const bool clicked = ui.Region(hud_id::Engage(), engage);
  const bool hovered = ui.Hovered(hud_id::Engage());
  ui.Draw().FillRect(engage, hovered ? Mix(theme.buttonOn, theme.text, 0.2f) : theme.buttonOn);
  ui.Draw().StrokeRect(engage, theme.accent, ui.S(1.5f));
  ui.Label(engage, "ENGAGE", {15.0f, theme.text, Align::Center, VAlign::Middle, FontWeight::Bold});
  ui.Tooltip(hud_id::Engage(), "Start the battle (Enter). You can pause at any time with Space.");
  Record(hud_id::Engage(), engage);
  if (clicked) {
    Engage();
  }
}

void CommandHud::DrawDeployment(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  const battle::Side side = m_screen.PlayerSide();
  const Camera& camera = m_screen.GetCamera();
  DrawList& draw = ui.Draw();
  draw.PushClip(camera.Viewport());
  for (const battle::Side zoneSide : {Enemy(), side}) {
    const battle::ViewDeployZone& zone = view.deployZones[static_cast<int>(zoneSide)];
    if (zone.radius <= 0.0f) {
      continue;
    }
    const bool own = zoneSide == side;
    const Point center = camera.WorldToScreen(zone.center);
    const float radius = camera.PixelsFor(zone.radius) * camera.VerticalScale(); // on screen, above the centre
    const Color color = own ? theme.accent : FactionColor(theme, zoneSide);
    if (own) {
      PlaneDisc(draw, camera, zone.center, zone.radius, color.WithAlpha(16));
    }
    PlaneCircle(draw, camera, zone.center, zone.radius, color.WithAlpha(own ? 200 : 90),
                Stroke{ui.S(own ? 1.5f : 1.0f), ui.S(9.0f), ui.S(6.0f)});
    draw.Text(own ? "YOUR DEPLOYMENT ZONE" : "ENEMY DEPLOYMENT", {center.x, center.y - radius - ui.S(6.0f)},
              ui.S(10.0f), color.WithAlpha(own ? 220 : 140), Align::Center, VAlign::Bottom, FontWeight::Bold);
  }
  draw.PopClip();

  const battle::ViewDeployZone& zone = view.deployZones[static_cast<int>(side)];
  const battle::Vec2 mouseWorld = camera.ScreenToWorld(ui.Mouse());
  // The drag ends when its handle lets go of the button, wherever the handle is now; only a real release deploys.
  if (m_deployDrag != kNoGroup && ui.ActiveId() != hud_id::DeployHandle(m_deployDrag)) {
    if (ui.ReleasedId() == hud_id::DeployHandle(m_deployDrag)) {
      battle::Command command;
      command.target.groups = {m_deployDrag};
      command.order = battle::OrderDeploy{mouseWorld, std::nullopt};
      Submit(std::move(command));
    }
    m_deployDrag = kNoGroup;
  }
  for (const battle::ViewGroup& group : view.groups) {
    if (group.side != side) {
      continue;
    }
    std::vector<UnitIndex> members = ActiveMembers(view, group);
    std::ranges::sort(members);
    if (members.empty()) {
      continue;
    }
    battle::Vec2 centroid;
    for (UnitIndex unit : members) {
      const battle::Vec2 p = FindUnit(view, unit)->position;
      centroid += p;
    }
    centroid = centroid * (1.0f / static_cast<float>(members.size()));
    const WidgetId id = hud_id::DeployHandle(group.id);
    const Point at = camera.WorldToScreen(centroid);
    const float r = ui.S(kHandleRadius);
    const Rect hit{at.x - r, at.y - r, 2.0f * r, 2.0f * r};
    if (!camera.Viewport().Contains(at) && m_deployDrag != group.id) {
      continue;
    }
    const bool clicked = ui.Region(id, hit);
    Record(id, hit);
    const bool pressing = ui.ActiveId() == id;
    if (pressing && m_deployDrag == kNoGroup && !m_pressAbandoned
        && Distance(ui.Mouse(), ui.PressPoint()) > ui.S(kDeployDragThreshold)) {
      m_deployDrag = group.id;
    }
    const bool dragging = m_deployDrag == group.id;
    const battle::Vec2 target = DeploymentTarget(view, members, centroid, mouseWorld, zone);
    if (clicked && !dragging && Distance(ui.ClickPoint(), ui.PressPoint()) <= ui.S(kDeployDragThreshold)) {
      FocusGroup(group.id);
    }
    const Color faction = FactionColor(theme, side);
    if (dragging) {
      const Point to = camera.WorldToScreen(target);
      draw.Line(at, to, theme.accent.WithAlpha(160), Stroke{ui.S(1.5f), ui.S(6.0f), ui.S(4.0f)});
      for (UnitIndex unit : members) {
        const battle::ViewUnit* found = FindUnit(view, unit);
        const Point ghost = camera.WorldToScreen(
            {found->position.x + target.x - centroid.x, found->position.y + target.y - centroid.y});
        draw.Circle(ghost, std::max(camera.PixelsFor(found->radius), ui.S(5.0f)), faction.WithAlpha(170),
                    Stroke{ui.S(1.5f)});
      }
      draw.FillCircle(to, r, theme.panel);
      draw.Circle(to, r, theme.accent, Stroke{ui.S(2.0f)});
    }
    const bool hovered = ui.Hovered(id);
    draw.FillCircle(at, r, theme.panel);
    draw.Circle(at, r, hovered || dragging ? theme.accent : faction, Stroke{ui.S(hovered ? 2.0f : 1.5f)});
    const std::optional<int> key = HotkeyFor(group.id);
    const std::string mark = key ? std::to_string(*key) : GroupTitle(group).substr(0, 1);
    draw.Text(mark, at, ui.S(12.0f), theme.text, Align::Center, VAlign::Middle, FontWeight::Bold);
    if (hovered && !dragging) {
      draw.Text(GroupTitle(group), {at.x, at.y - r - ui.S(4.0f)}, ui.S(11.0f), theme.text, Align::Center,
                VAlign::Bottom);
    }
    ui.Tooltip(id, "Drag to deploy " + GroupTitle(group) + " anywhere inside your zone; click to select it.");
  }
}

} // namespace reb::battle_ui
