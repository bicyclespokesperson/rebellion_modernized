// The roster (tactical_combat.md §5.1, §10.2): the player's task forces and flights with their members, ship cards
// with hull, shields and critical systems, selection by click, and drag-and-drop regrouping.
#include <algorithm>
#include <cstdio>

#include "battle_ui/command_hud.h"
#include "battle_ui/hud_draw.h"
#include "battle_ui/selection.h"
#include "battle_ui/world_painter.h"

namespace reb::battle_ui
{

using battle::kNoGroup;
using battle::kNoUnit;
using battle::UnitIndex;

namespace
{

constexpr float kSectionHeight = 18.0f;
constexpr float kGroupHeight = 24.0f;
constexpr float kShipHeight = 34.0f;
constexpr float kSquadronHeight = 30.0f;
constexpr float kNewGroupHeight = 26.0f;
constexpr float kIndent = 16.0f;
constexpr float kDragThreshold = 6.0f;

struct SystemState {
  Icon icon;
  std::string_view name;
  int remaining;
  int maximum;
  bool present;
};

std::array<SystemState, 5> Systems(const battle::CriticalHits& c, bool tractor)
{
  const auto left = [](int hits, int maximum) {
    return std::clamp(maximum - hits, 0, maximum);
  };
  return {{
      {Icon::Engines, "Engines", left(c.speed, 4), 4, true},
      {Icon::ShieldGenerator, "Shield generator", left(c.shieldRecharge, 4), 4, true},
      {Icon::WeaponGenerator, "Weapon generator", left(c.weaponRecharge, 4), 4, true},
      {Icon::Tractor, "Tractor beam", left(c.tractor, 4), 4, tractor},
      {Icon::Hyperdrive, "Hyperdrive", left(c.hyperdrivePrimary, 1) + left(c.hyperdriveBackup, 1), 2, true},
  }};
}

std::string UnitStatus(const battle::ViewUnit* unit)
{
  if (!unit) {
    return "Withdrawn";
  }
  if (unit->flags & battle::kViewDestroyed) {
    return "Lost";
  }
  if (unit->flags & battle::kViewWithdrawn) {
    return "Withdrawn";
  }
  if (unit->flags & battle::kViewDocked) {
    return "In hangar";
  }
  if (unit->flags & battle::kViewWithdrawing) {
    char text[32];
    std::snprintf(text, sizeof text, "Jumping %d%%", static_cast<int>(unit->spool * 100.0f));
    return text;
  }
  if (unit->flags & battle::kViewTractored) {
    return "Tractored";
  }
  if (unit->flags & battle::kViewDisrupted) {
    return "Disrupted";
  }
  return {};
}

} // namespace

void CommandHud::DrawRoster(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  const battle::Side side = m_screen.PlayerSide();
  const Rect column = m_screen.Layout().roster;
  if (column.Empty()) {
    return;
  }
  const float titleHeight = ui.S(theme.rowHeight);
  if (m_rosterCollapsed) {
    const Rect panel{column.x, column.y, column.w, titleHeight};
    ui.Panel(panel, "FLEET");
    if (hud::FoldButton(ui, hud_id::CollapseRoster(), panel, true, "the fleet roster")) {
      m_rosterCollapsed = false;
    }
    Record(hud_id::CollapseRoster(), hud::FoldRect(panel));
    m_rosterDragging = false;
    return;
  }

  // A large fleet starts with its biggest groups folded, so every group header fits.
  if (!m_autoFolded && !view.groups.empty()) {
    m_autoFolded = true;
    std::vector<std::pair<float, battle::GroupId>> groups;
    float total = 2.0f * ui.S(kSectionHeight);
    for (const battle::ViewGroup& group : view.groups) {
      if (group.side == side && !group.members.empty()) {
        const float members
            = static_cast<float>(group.members.size()) * ui.S(group.flight ? kSquadronHeight : kShipHeight);
        groups.push_back({members, group.id});
        total += ui.S(kGroupHeight) + members;
      }
    }
    std::ranges::sort(groups, std::greater{});
    const float available = column.h - titleHeight - 2.0f * ui.S(theme.padding);
    for (const auto& [members, group] : groups) {
      if (total <= available) {
        break;
      }
      m_collapsedGroups.insert(group);
      total -= members;
    }
  }

  std::vector<RosterRow> rows;
  for (const bool flights : {false, true}) {
    rows.push_back({RosterRow::Kind::Section, kNoGroup, kNoUnit, flights, ui.S(kSectionHeight)});
    for (const battle::ViewGroup& group : view.groups) {
      if (group.side != side || group.flight != flights || group.members.empty()) {
        continue;
      }
      rows.push_back({RosterRow::Kind::Group, group.id, kNoUnit, flights, ui.S(kGroupHeight)});
      if (m_collapsedGroups.contains(group.id)) {
        continue;
      }
      for (UnitIndex unit : group.members) {
        rows.push_back({RosterRow::Kind::Unit, group.id, unit, flights, ui.S(flights ? kSquadronHeight : kShipHeight)});
      }
    }
    if (m_rosterDragging && m_dragFlight == flights) {
      rows.push_back({RosterRow::Kind::NewGroup, kNoGroup, kNoUnit, flights, ui.S(kNewGroupHeight)});
    }
  }

  float total = 0.0f;
  for (const RosterRow& row : rows) {
    total += row.height;
  }
  // The panel shrinks to its rows, so the battle stays visible below a small fleet.
  const float padding = ui.S(theme.padding);
  const Rect panel{column.x, column.y, column.w, std::min(column.h, titleHeight + total + 2.0f * padding)};
  const Rect content = ui.Panel(panel, "FLEET");
  const Rect titleBar{panel.x, panel.y, panel.w, titleHeight};
  if (hud::FoldButton(ui, hud_id::CollapseRoster(), titleBar, false, "the fleet roster")) {
    m_rosterCollapsed = true;
  }
  Record(hud_id::CollapseRoster(), hud::FoldRect(titleBar));
  const WidgetId listId = Id("hud.roster");
  const bool clicked = ui.Region(listId, content);
  const float scroll = ui.ScrollOffset(listId, total, content.h, ui.S(kShipHeight));
  std::vector<Rect> rects;
  float y = content.y - scroll;
  for (const RosterRow& row : rows) {
    rects.push_back({content.x, y, content.w - ui.S(4.0f), row.height});
    y += row.height;
  }
  const auto rowAt = [&](Point p) -> int {
    if (!content.Contains(p)) {
      return -1;
    }
    for (size_t i = 0; i < rects.size(); ++i) {
      if (rects[i].Contains(p)) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };

  // Drag a ship or squadron card onto another group (or the new-group slot) to regroup it.
  const bool pressing = ui.ActiveId() == listId;
  const bool wasDragging = m_rosterDragging;
  if (pressing && !m_rosterDragging && !m_pressAbandoned) {
    const int pressed = rowAt(ui.PressPoint());
    if (pressed >= 0 && rows[pressed].kind == RosterRow::Kind::Unit
        && Distance(ui.Mouse(), ui.PressPoint()) > ui.S(kDragThreshold)) {
      const UnitIndex grabbed = rows[pressed].unit;
      const battle::ViewUnit* unit = FindUnit(view, grabbed);
      if (unit && Selectable(*unit)) {
        m_rosterDragging = true;
        m_dragFlight = rows[pressed].flight;
        m_dragUnits = {grabbed};
        if (m_screen.GetSelection().Contains(grabbed)) {
          m_dragUnits.clear();
          for (UnitIndex selected : m_screen.GetSelection().Orderable(view, side)) {
            const battle::ViewUnit* other = FindUnit(view, selected);
            if (other && (other->kind == battle::ViewUnitKind::Squadron) == m_dragFlight) {
              m_dragUnits.push_back(selected);
            }
          }
        }
      }
    }
  }
  if (wasDragging && !pressing) {
    const int drop = rowAt(ui.Mouse());
    if (ui.ReleasedId() == listId) {
      FinishRosterDrag(drop >= 0 ? &rows[drop] : nullptr);
    }
    m_rosterDragging = false;
  }

  const int hoveredRow = ui.Hovered(listId) || m_rosterDragging ? rowAt(ui.Mouse()) : -1;
  const auto dropGroup = [&](int index) -> std::optional<battle::GroupId> {
    if (index < 0 || rows[index].flight != m_dragFlight || rows[index].kind == RosterRow::Kind::Section) {
      return std::nullopt;
    }
    return rows[index].group;
  };
  ui.Draw().PushClip(content);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rects[i].Bottom() < content.y || rects[i].y > content.Bottom()) {
      continue;
    }
    const bool dropTarget = m_rosterDragging && static_cast<int>(i) == hoveredRow && dropGroup(hoveredRow).has_value();
    DrawRosterRow(ui, rows[i], rects[i], static_cast<int>(i) == hoveredRow && !m_rosterDragging, dropTarget);
    if (rows[i].kind == RosterRow::Kind::Group) {
      Record(hud_id::RosterGroup(rows[i].group), rects[i]);
    } else if (rows[i].kind == RosterRow::Kind::Unit) {
      Record(hud_id::RosterUnit(rows[i].unit), rects[i]);
    } else if (rows[i].kind == RosterRow::Kind::NewGroup) {
      Record(hud_id::RosterNewGroup(rows[i].flight), rects[i]);
    }
  }
  ui.Draw().PopClip();
  if (total > content.h) {
    const float track = content.h * content.h / total;
    const float trackY = content.y + (content.h - track) * (scroll / (total - content.h));
    ui.Draw().FillRect({content.Right() - ui.S(3.0f), trackY, ui.S(3.0f), track}, theme.panelBorder);
  }

  if (hoveredRow >= 0 && !m_rosterDragging) {
    const RosterRow& row = rows[hoveredRow];
    if (row.kind == RosterRow::Kind::Unit) {
      const BattleScreen::UnitLabel& label = m_screen.Label(row.unit);
      std::string tip
          = label.name + (label.className.empty() || label.className == label.name ? "" : ", " + label.className);
      if (const battle::ViewUnit* unit = FindUnit(view, row.unit); unit && !row.flight) {
        tip += ". Shields " + std::to_string(static_cast<int>(unit->shieldFraction * 100.0f)) + "%, hull "
               + std::to_string(static_cast<int>(unit->hullFraction * 100.0f)) + "%.";
        for (const SystemState& system : Systems(unit->criticals, Facts(row.unit).tractor)) {
          if (system.present) {
            tip += " " + std::string(system.name) + " " + std::to_string(system.remaining) + "/"
                   + std::to_string(system.maximum) + ".";
          }
        }
      }
      ui.Tooltip(listId, tip + " Click to select; drag onto another group to move it there.");
    } else if (row.kind == RosterRow::Kind::Group) {
      ui.Tooltip(listId, "Click to select the group and edit its doctrine; double-click to centre on it. Click the "
                         "arrow to fold it.");
    }
  }

  if (!clicked || wasDragging || hoveredRow < 0) {
    return;
  }
  const int clickedRow = rowAt(ui.ClickPoint());
  if (clickedRow < 0 || clickedRow != rowAt(ui.PressPoint())) {
    return; // pressed on one row and released on another
  }
  const RosterRow& row = rows[clickedRow];
  if (row.kind == RosterRow::Kind::Group) {
    if (ui.ClickPoint().x < rects[clickedRow].x + ui.S(18.0f)) {
      if (!m_collapsedGroups.erase(row.group)) {
        m_collapsedGroups.insert(row.group);
      }
      return;
    }
    FocusGroup(row.group);
    if (ui.ClickCount() >= 2) {
      const std::vector<UnitIndex> members = m_screen.GetSelection().Units();
      if (!members.empty()) {
        battle::Vec2 sum;
        for (UnitIndex unit : members) {
          sum.x += FindUnit(view, unit)->position.x;
          sum.y += FindUnit(view, unit)->position.y;
        }
        m_screen.CenterAt({sum.x / members.size(), sum.y / members.size()});
      }
    }
  } else if (row.kind == RosterRow::Kind::Unit) {
    const battle::ViewUnit* unit = FindUnit(view, row.unit);
    if (!unit) {
      return;
    }
    if (!Selectable(*unit)) {
      if (unit->flags & battle::kViewDocked) {
        FocusGroup(row.group);
      }
      return;
    }
    m_screen.GetSelection().Set({row.unit});
    if (ui.ClickCount() >= 2) {
      m_screen.CenterOn(row.unit);
    }
  }
}

void CommandHud::FinishRosterDrag(const RosterRow* dropRow)
{
  if (!dropRow || m_dragUnits.empty()) {
    return;
  }
  const RosterRow& row = *dropRow;
  if (row.kind == RosterRow::Kind::Section || row.flight != m_dragFlight) {
    return;
  }
  const battle::GroupId target = row.kind == RosterRow::Kind::NewGroup ? kNoGroup : row.group;
  const bool alreadyThere = target != kNoGroup && std::ranges::all_of(m_dragUnits, [&](UnitIndex unit) {
                              const battle::ViewUnit* found = FindUnit(m_screen.View(), unit);
                              return found && found->group == target;
                            });
  if (alreadyThere) {
    return;
  }
  battle::Command command;
  command.target.units = m_dragUnits;
  command.order = battle::OrderAssignGroup{target};
  if (Submit(std::move(command))) {
    const battle::ViewGroup* group = FindGroup(m_screen.View(), target);
    AddToast(target == kNoGroup ? std::string(m_dragFlight ? "Formed a new flight" : "Formed a new task force")
                                : "Moved to " + (group ? GroupTitle(*group) : std::string("group")));
  }
}

void CommandHud::DrawRosterRow(Ui& ui, const RosterRow& row, const Rect& rect, bool hovered, bool dropTarget)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  DrawList& draw = ui.Draw();
  if (dropTarget) {
    draw.FillRect(rect, theme.accent.WithAlpha(60));
    draw.StrokeRect(rect, theme.accent, ui.S(1.5f));
  }
  switch (row.kind) {
    case RosterRow::Kind::Section:
      {
        ui.Label({rect.x + ui.S(2.0f), rect.y, rect.w, rect.h}, row.flight ? "FLIGHTS" : "TASK FORCES",
                 {10.0f, theme.textDim, Align::Start, VAlign::Bottom, FontWeight::Bold});
        draw.FillRect({rect.x, rect.Bottom() - ui.S(1.0f), rect.w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(70));
        return;
      }
    case RosterRow::Kind::NewGroup:
      {
        const Rect slot = rect.Inset(ui.S(2.0f), ui.S(3.0f));
        draw.StrokeRect(slot, dropTarget ? theme.accent : theme.textDim, ui.S(1.0f));
        DrawIcon(draw, Icon::Plus, {slot.x + ui.S(6.0f), slot.y, ui.S(12.0f), slot.h}, theme.text, ui.Scale());
        ui.Label({slot.x + ui.S(24.0f), slot.y, slot.w, slot.h},
                 row.flight ? "Drop here for a new flight" : "Drop here for a new task force", {11.0f, theme.text});
        return;
      }
    case RosterRow::Kind::Group:
      {
        const battle::ViewGroup* group = FindGroup(view, row.group);
        if (!group) {
          return;
        }
        const bool focused = m_subject.IsGroup() && m_subject.group == row.group;
        if (focused) {
          draw.FillRect(rect, theme.rowSelected);
        } else if (hovered) {
          draw.FillRect(rect, theme.rowHover);
        }
        draw.FillRect({rect.x, rect.y + ui.S(1.0f), rect.w, ui.S(1.0f)}, theme.panelBorder.WithAlpha(50));
        DrawIcon(draw, m_collapsedGroups.contains(row.group) ? Icon::ChevronRight : Icon::ChevronDown,
                 Rect{rect.x, rect.y, ui.S(16.0f), rect.h}.Inset(ui.S(3.0f)), theme.textDim, ui.Scale());
        float x = rect.x + ui.S(18.0f);
        if (const std::optional<int> key = HotkeyFor(row.group)) {
          const Rect badge{x, rect.Center().y - ui.S(8.0f), ui.S(16.0f), ui.S(16.0f)};
          draw.FillRect(badge, theme.buttonFace);
          draw.StrokeRect(badge, theme.buttonBorder, ui.S(1.0f));
          ui.Label(badge, std::to_string(*key), {11.0f, theme.title, Align::Center, VAlign::Middle, FontWeight::Bold});
          x = badge.Right() + ui.S(6.0f);
        }
        std::string detail;
        if (group->flight) {
          int craft = 0;
          int craftMax = 0;
          for (UnitIndex member : group->members) {
            if (const battle::ViewUnit* unit = FindUnit(view, member)) {
              craft += (unit->flags & battle::kViewDestroyed) ? 0 : unit->craft;
              craftMax += unit->craftMax;
            }
          }
          detail
              = std::string(Option(DoctrineField::Mission, static_cast<int>(group->flightDoctrine.mission)).shortName)
                + "  " + std::to_string(craft) + "/" + std::to_string(craftMax);
        } else {
          detail = std::string(Option(DoctrineField::Role, static_cast<int>(group->shipDoctrine.role)).shortName) + "  "
                   + std::to_string(ActiveMembers(view, *group).size());
        }
        const float detailWidth = ui.TextWidth(detail, 11.0f) + ui.S(4.0f);
        ui.Label({x, rect.y, rect.Right() - x - detailWidth - ui.S(4.0f), rect.h}, GroupTitle(*group),
                 {12.0f, theme.text, Align::Start, VAlign::Middle, FontWeight::Bold});
        ui.Label({rect.x, rect.y, rect.w - ui.S(2.0f), rect.h}, detail, {11.0f, theme.title, Align::End});
        return;
      }
    case RosterRow::Kind::Unit:
      break;
  }

  const battle::ViewUnit* unit = FindUnit(view, row.unit);
  const bool active = unit && Selectable(*unit);
  const bool selected = m_screen.GetSelection().Contains(row.unit);
  const bool dragged = m_rosterDragging && std::ranges::find(m_dragUnits, row.unit) != m_dragUnits.end();
  if (selected) {
    draw.FillRect(rect.Inset(ui.S(1.0f), ui.S(1.0f)), theme.rowSelected.WithAlpha(dragged ? 90 : 170));
  } else if (hovered && active) {
    draw.FillRect(rect.Inset(ui.S(1.0f), ui.S(1.0f)), theme.rowHover);
  }
  const float x = rect.x + ui.S(kIndent);
  const float w = rect.Right() - x - ui.S(3.0f);
  const Color nameColor = active ? theme.text : theme.textDim;
  const std::string status = UnitStatus(unit);
  const float statusWidth = status.empty() ? 0.0f : ui.TextWidth(status, 10.0f) + ui.S(6.0f);
  float nameRight = x + w - statusWidth;
  const battle::DoctrineOverrideMask overrides = active ? m_screen.Detail(row.unit).overrides : 0;
  if (overrides) {
    const Rect badge{nameRight - ui.S(11.0f), rect.y + ui.S(4.0f), ui.S(9.0f), ui.S(9.0f)};
    DrawIcon(draw, Icon::Override, badge, Rgb(0xffc864), ui.Scale());
    nameRight = badge.x - ui.S(3.0f);
  }
  if (unit && (unit->flags & battle::kViewFlagship)) {
    const Rect badge{nameRight - ui.S(11.0f), rect.y + ui.S(4.0f), ui.S(9.0f), ui.S(9.0f)};
    DrawIcon(draw, Icon::HoldTheLine, badge, Rgb(0xffd860), ui.Scale());
    nameRight = badge.x - ui.S(3.0f);
  }
  ui.Label({x, rect.y + ui.S(2.0f), nameRight - x, ui.S(14.0f)}, m_screen.Label(row.unit).name,
           {12.0f, nameColor, Align::Start, VAlign::Middle});
  if (!status.empty()) {
    const bool alarming = unit && (unit->flags & (battle::kViewTractored | battle::kViewDisrupted));
    ui.Label({x, rect.y + ui.S(2.0f), w, ui.S(14.0f)}, status,
             {10.0f, alarming ? theme.hullPoor : theme.hullFair, Align::End});
  }
  if (!unit || (unit->flags & battle::kViewDestroyed)) {
    return;
  }
  const float barsY = rect.y + ui.S(19.0f);
  if (row.flight) {
    const float fraction
        = unit->craftMax > 0 ? static_cast<float>(unit->craft) / static_cast<float>(unit->craftMax) : 0.0f;
    const float barWidth = w * 0.55f;
    ui.Bar({x, barsY + ui.S(1.0f), barWidth, ui.S(4.0f)}, fraction, HullColor(theme, fraction));
    ui.Bar({x, barsY + ui.S(6.0f), barWidth, ui.S(2.0f)}, unit->shieldFraction, theme.shield);
    ui.Label({x + barWidth + ui.S(6.0f), barsY - ui.S(2.0f), w - barWidth - ui.S(6.0f), ui.S(12.0f)},
             std::to_string(unit->craft) + "/" + std::to_string(unit->craftMax) + " craft",
             {10.0f, theme.textDim, Align::Start, VAlign::Middle});
    return;
  }
  const float barWidth = w * 0.36f;
  ui.Bar({x, barsY, barWidth, ui.S(3.0f)}, unit->shieldFraction, theme.shield);
  ui.Bar({x, barsY + ui.S(5.0f), barWidth, ui.S(4.0f)}, unit->hullFraction, HullColor(theme, unit->hullFraction));
  const float slot = (w - barWidth - ui.S(6.0f)) / 5.0f;
  float sx = x + barWidth + ui.S(6.0f);
  for (const SystemState& system : Systems(unit->criticals, Facts(row.unit).tractor)) {
    Color color = theme.textDim;
    if (!system.present) {
      color = theme.disabledText.WithAlpha(110);
    } else if (system.remaining == 0) {
      color = theme.hullPoor;
    } else if (system.remaining < system.maximum) {
      color = theme.hullFair;
    }
    DrawIcon(draw, system.icon, {sx, barsY - ui.S(2.0f), ui.S(10.0f), ui.S(10.0f)}, color, ui.Scale() * 0.8f);
    if (system.present) {
      const float pip = ui.S(2.0f);
      for (int i = 0; i < system.maximum; ++i) {
        const Rect box{sx + ui.S(12.0f),
                       barsY - ui.S(1.0f) + static_cast<float>(system.maximum - 1 - i) * ui.S(2.5f)
                           - (system.maximum == 2 ? ui.S(-2.5f) : 0.0f),
                       pip * 1.5f, pip};
        draw.FillRect(box, i < system.remaining ? color : theme.barBack);
      }
    }
    sx += slot;
  }
}

} // namespace reb::battle_ui
