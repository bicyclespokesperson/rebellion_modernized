// The doctrine card and orders bar (tactical_combat.md §5.2, §5.4, §10.2): who is being commanded, one line of
// doctrine settings that open into their options, the live orders, power routing, fleet-wide actions and the intent
// line.
#include <algorithm>
#include <bit>
#include <map>
#include <ranges>

#include "battle_ui/command_hud.h"
#include "battle_ui/hud_draw.h"
#include "battle_ui/selection.h"
#include "battle_ui/world_painter.h"

namespace reb::battle_ui
{

using battle::kNoUnit;
using battle::UnitIndex;

namespace
{

constexpr float kHeader = 20.0f;
constexpr float kChips = 32.0f;
constexpr float kOrders = 26.0f;
constexpr float kIntent = 16.0f;
constexpr float kGap = 4.0f;
constexpr float kPad = 6.0f;
constexpr Color kOverrideColor = Rgb(0xffc864);

struct OrderSpec {
  OrderButton order;
  Icon icon;
  std::string_view label;
  std::string_view help;
  bool enabled = true;
};

constexpr std::array kPowerLabels{"Bal", "Wpn", "Shd", "Eng"};

std::string ChipValue(DoctrineField field, bool flight, const battle::ShipDoctrine& ship,
                      const battle::FlightDoctrine& flightDoctrine)
{
  if (field == DoctrineField::Priorities) {
    const std::string text = PriorityText(flight ? flightDoctrine.priorities : ship.priorities);
    return text.empty() ? "None" : text;
  }
  return std::string(Option(field, GetOption(field, ship, flightDoctrine)).shortName);
}

} // namespace

void CommandHud::DrawCard(Ui& ui)
{
  const Rect area = m_screen.Layout().card;
  if (area.Empty()) {
    return;
  }
  const float collapsedHeight = ui.S(2.0f * kPad + kHeader + kGap + kOrders);
  const Rect panel = m_cardCollapsed ? Rect{area.x, area.Bottom() - collapsedHeight, area.w, collapsedHeight} : area;
  ui.Panel(panel);
  const float x = panel.x + ui.S(8.0f);
  const float w = panel.w - ui.S(16.0f);
  float y = panel.y + ui.S(kPad);
  const CardDoctrine doctrine = ReadCardDoctrine();
  DrawCardHeader(ui, {x, y, w, ui.S(kHeader)}, doctrine);
  y += ui.S(kHeader + kGap);
  if (!m_cardCollapsed) {
    DrawChips(ui, {x, y, w, ui.S(kChips)}, doctrine);
    y += ui.S(kChips + kGap);
  }
  DrawOrders(ui, {x, y, w, ui.S(kOrders)}, doctrine);
  y += ui.S(kOrders + kGap);
  if (!m_cardCollapsed) {
    DrawIntent(ui, {x, y - ui.S(1.0f), w, ui.S(kIntent)});
  }
}

void CommandHud::DrawCardHeader(Ui& ui, const Rect& row, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  float right = row.Right();
  const Rect titleBar{row.x, row.y, row.w + ui.S(4.0f), row.h};
  if (hud::FoldButton(ui, hud_id::CollapseCard(), titleBar, m_cardCollapsed, "the doctrine settings and intent line")) {
    m_cardCollapsed = !m_cardCollapsed;
  }
  Record(hud_id::CollapseCard(), hud::FoldRect(titleBar));
  right -= row.h + ui.S(6.0f);
  if (!m_subject.Empty()) {
    const float width = hud::ButtonWidth(ui, "Presets");
    const Rect presets{right - width, row.y, width, row.h};
    if (hud::Button(ui, hud_id::Presets(), presets, Icon::Preset, "Presets", true,
                    m_popup.kind == PopupKind::Presets)) {
      OpenPopupAt(PopupKind::Presets, presets);
    }
    ui.Tooltip(hud_id::Presets(), "Apply a named doctrine in one click, or save this one under a name.");
    Record(hud_id::Presets(), presets);
    right = presets.x - ui.S(8.0f);
  }

  float x = row.x;
  const auto text = [&](std::string_view value, float size, Color color, FontWeight weight = FontWeight::Regular) {
    const float width = std::min(ui.TextWidth(value, size, weight), std::max(0.0f, right - x));
    ui.Label({x, row.y, width + ui.S(2.0f), row.h}, value, {size, color, Align::Start, VAlign::Middle, weight});
    x += width + ui.S(8.0f);
  };
  if (m_subject.IsGroup()) {
    const battle::ViewGroup* group = FindGroup(view, m_subject.group);
    if (!group) {
      return;
    }
    if (const std::optional<int> key = HotkeyFor(group->id)) {
      const Rect badge{x, row.Center().y - ui.S(8.0f), ui.S(16.0f), ui.S(16.0f)};
      ui.Draw().FillRect(badge, theme.buttonFace);
      ui.Draw().StrokeRect(badge, theme.buttonBorder, ui.S(1.0f));
      ui.Label(badge, std::to_string(*key), {11.0f, theme.title, Align::Center, VAlign::Middle, FontWeight::Bold});
      x = badge.Right() + ui.S(6.0f);
    }
    text(GroupTitle(*group), 13.0f, theme.title, FontWeight::Bold);
    std::map<std::string, int> classes;
    for (UnitIndex unit : ActiveMembers(view, *group)) {
      ++classes[m_screen.Label(unit).className];
    }
    std::string composition;
    for (const auto& [name, count] : classes) {
      composition += (composition.empty() ? "" : ", ") + (name.empty() ? std::string("unknown class") : name)
                     + (count > 1 ? " x" + std::to_string(count) : "");
    }
    text(composition.empty() ? "no units on the field" : composition, 11.0f, theme.textDim);
    return;
  }
  if (!m_subject.units.empty()) {
    if (m_subject.units.size() == 1) {
      const UnitIndex unit = m_subject.units.front();
      text(m_screen.Label(unit).name, 13.0f, theme.title, FontWeight::Bold);
      std::string where = m_screen.Label(unit).className;
      if (const battle::ViewUnit* found = FindUnit(view, unit)) {
        if (const battle::ViewGroup* group = FindGroup(view, found->group)) {
          where += (where.empty() ? "in " : ", in ") + GroupTitle(*group);
        }
      }
      text(where, 11.0f, theme.textDim);
    } else {
      text(std::to_string(m_subject.units.size()) + (m_subject.flight ? " squadrons" : " ships"), 13.0f, theme.title,
           FontWeight::Bold);
      text("edits override each one's group doctrine", 11.0f, theme.textDim);
    }
    const int overrides = std::popcount(doctrine.overrides & battle::DoctrineOverrideMask{0xffff});
    if (overrides > 0 && x + ui.S(150.0f) < right) {
      DrawIcon(ui.Draw(), Icon::Override, {x, row.Center().y - ui.S(5.0f), ui.S(10.0f), ui.S(10.0f)}, kOverrideColor,
               ui.Scale());
      x += ui.S(14.0f);
      text(std::to_string(overrides) + (overrides == 1 ? " override" : " overrides"), 11.0f, kOverrideColor);
      const float width = hud::ButtonWidth(ui, "Clear", false);
      const Rect clear{x - ui.S(4.0f), row.y + ui.S(1.0f), width, row.h - ui.S(2.0f)};
      if (hud::Segment(ui, hud_id::ClearOverrides(), clear, "Clear", false)) {
        ClearOverrides(doctrine.overrides);
      }
      ui.Tooltip(hud_id::ClearOverrides(), "Drop these overrides so the ships follow their group's doctrine again.");
      Record(hud_id::ClearOverrides(), clear);
      x = clear.Right() + ui.S(8.0f);
    }
    if (doctrine.hasGroup) {
      const battle::ViewUnit* first = FindUnit(view, m_subject.units.front());
      const float width = hud::ButtonWidth(ui, "Edit group", false);
      const Rect edit{x - ui.S(4.0f), row.y + ui.S(1.0f), width, row.h - ui.S(2.0f)};
      if (first && edit.Right() < right && hud::Segment(ui, hud_id::EditGroup(), edit, "Edit group", false)) {
        FocusGroup(first->group);
      }
      if (edit.Right() < right) {
        ui.Tooltip(hud_id::EditGroup(), "Edit the doctrine of the whole group instead of this selection.");
        Record(hud_id::EditGroup(), edit);
      }
    }
    return;
  }
  const std::vector<UnitIndex>& selection = m_screen.GetSelection().Units();
  if (selection.size() == 1) {
    if (const battle::ViewUnit* unit = FindUnit(view, selection.front()); unit && unit->side != m_screen.PlayerSide()) {
      text(m_screen.Label(unit->unit).name, 13.0f, FactionColor(theme, unit->side), FontWeight::Bold);
      text(m_screen.Label(unit->unit).className + ", enemy: its doctrine is unknown", 11.0f, theme.textDim);
      return;
    }
  }
  text("No ships selected", 13.0f, theme.textDim, FontWeight::Bold);
  text("Click a group in the fleet list or a ship on the battle plane, or press 1-9.", 11.0f, theme.textDim);
}

void CommandHud::DrawChips(Ui& ui, const Rect& row, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  if (m_subject.Empty()) {
    return;
  }
  const std::span<const DoctrineField> fields = m_subject.flight ? FlightFields() : ShipFields();
  const float gap = ui.S(4.0f);
  const float icon = ui.S(16.0f);
  std::vector<std::string> values;
  std::vector<float> widths;
  float natural = 0.0f;
  for (DoctrineField field : fields) {
    const battle::DoctrineOverrideMask mask = FieldMask(field, m_subject.flight);
    std::string value
        = (doctrine.mixed & mask) ? "Mixed" : ChipValue(field, m_subject.flight, doctrine.ship, doctrine.flight);
    const battle::UnitIndex protectedUnit
        = field == DoctrineField::Role && doctrine.ship.role == battle::Role::Screen ? doctrine.ship.screenTarget
          : field == DoctrineField::Mission && doctrine.flight.mission == battle::FlightMission::Escort
              ? doctrine.flight.escortTarget
              : kNoUnit;
    if (protectedUnit != kNoUnit && !(doctrine.mixed & mask)) {
      value += ": " + m_screen.Label(protectedUnit).name;
    }
    const float text = std::max(ui.TextWidth(Describe(field).label, 9.0f, FontWeight::Bold),
                                ui.TextWidth(value, 12.0f, FontWeight::Bold));
    float width = icon + ui.S(20.0f) + text;
    if (field == DoctrineField::Priorities) {
      width = std::clamp(width, ui.S(130.0f), ui.S(250.0f));
    }
    values.push_back(std::move(value));
    widths.push_back(width);
    natural += width;
  }
  const float gaps = gap * static_cast<float>(fields.size() - 1);
  const float stretch = std::min(1.35f, (row.w - gaps) / natural);
  float x = row.x;
  for (size_t i = 0; i < fields.size(); ++i) {
    const DoctrineField field = fields[i];
    const battle::DoctrineOverrideMask mask = FieldMask(field, m_subject.flight);
    const WidgetId id = hud_id::Chip(field);
    const Rect chip{x, row.y, widths[i] * stretch, row.h};
    x = chip.Right() + gap;
    Record(id, chip);
    const bool open
        = (m_popup.kind == PopupKind::Field || m_popup.kind == PopupKind::Priorities) && m_popup.field == field;
    if (ui.Region(id, chip)) {
      OpenPopupAt(field == DoctrineField::Priorities ? PopupKind::Priorities : PopupKind::Field, chip, field);
    }
    const bool hovered = ui.Hovered(id);
    ui.Draw().FillRect(chip, open ? theme.buttonOn : hovered ? theme.buttonHover : theme.buttonFace);
    ui.Draw().StrokeRect(chip, open ? theme.accent : theme.buttonBorder, ui.S(1.0f));
    const bool small = chip.w < icon + ui.S(56.0f);
    float textX = chip.x + ui.S(6.0f);
    if (!small) {
      DrawIcon(ui.Draw(), FieldIcon(field), {textX, chip.Center().y - icon * 0.5f, icon, icon}, theme.title,
               ui.Scale());
      textX += icon + ui.S(6.0f);
    }
    const float textWidth = chip.Right() - textX - ui.S(4.0f);
    ui.Label({textX, chip.y + ui.S(3.0f), textWidth, ui.S(11.0f)}, Describe(field).label,
             {9.0f, theme.textDim, Align::Start, VAlign::Middle, FontWeight::Bold});
    const bool mixed = (doctrine.mixed & mask) != 0;
    ui.Label({textX, chip.y + ui.S(14.0f), textWidth, ui.S(15.0f)}, values[i],
             {12.0f, mixed ? theme.textDim : theme.text, Align::Start, VAlign::Middle, FontWeight::Bold});
    if (doctrine.overrides & mask) {
      DrawIcon(ui.Draw(), Icon::Override, {chip.Right() - ui.S(11.0f), chip.y + ui.S(3.0f), ui.S(8.0f), ui.S(8.0f)},
               kOverrideColor, ui.Scale());
    }
    std::string tip = std::string(Describe(field).label) + ": " + std::string(Describe(field).help);
    if (field != DoctrineField::Priorities && !mixed) {
      const OptionText& current = Option(field, GetOption(field, doctrine.ship, doctrine.flight));
      tip += " Now " + std::string(current.name) + ": " + std::string(current.help);
    }
    if (doctrine.overrides & mask) {
      tip += " Overridden for this selection; the group's setting no longer applies.";
    }
    ui.Tooltip(id, tip);
  }
}

void CommandHud::DrawOrders(Ui& ui, const Rect& row, const CardDoctrine& doctrine)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  const bool any = !m_subject.Empty() && !view.finished;
  std::vector<OrderSpec> orders;
  const auto add
      = [&](OrderButton order, Icon icon, std::string_view label, std::string_view help, bool enabled = true) {
          orders.push_back({order, icon, label, help, enabled && any});
        };
  add(OrderButton::Attack, Icon::Attack, "Attack",
      "Attack a target you pick, overriding the target priorities; Ctrl+click adds more targets in order.");
  add(OrderButton::Move, Icon::Move, "Move",
      "Move to a point you pick on the battle plane; right-click there does the same.");
  add(OrderButton::Hold, Icon::Hold, "Hold", "Stop and hold position, still firing at whatever comes in range (H).");
  add(OrderButton::Escort, Icon::Escort, "Escort",
      "Escort a friendly ship or squadron you pick, staying between it and the threat.");
  if (!m_subject.flight) {
    if (SubjectHas([](const UnitFacts& facts) {
          return facts.tractor;
        })) {
      add(OrderButton::Tractor, Icon::Tractor, "Tractor",
          "Lock a tractor beam on an enemy capital ship within 2 km; enough power stops it and prevents its jump.");
    }
    if (SubjectHas([](const UnitFacts& facts) {
          return facts.carrier;
        })) {
      add(OrderButton::Launch, Icon::Launch, "Launch", "Launch the squadrons docked aboard.");
    }
  } else {
    add(OrderButton::Strike, Icon::Strike, "Strike", "Attack an enemy capital ship you pick, then come back to rearm.");
    add(OrderButton::Launch, Icon::Launch, "Launch", "Launch the flight from its carrier.");
    add(OrderButton::Recall, Icon::Recall, "Recall", "Break off and fly back to the carrier, staying launched.");
    add(OrderButton::Recover, Icon::Recover, "Recover",
        "Dock with the carrier, or any ship with free hangar space, to rearm or to leave with it.");
  }
  add(OrderButton::Withdraw, Icon::Withdraw, "Withdraw",
      "Spool hyperdrives and jump out as soon as possible; these ships leave the battle.");
  const bool enemyDeathStar = std::ranges::any_of(view.units, [&](const battle::ViewUnit& unit) {
    return unit.kind == battle::ViewUnitKind::DeathStar && unit.side != m_screen.PlayerSide() && Selectable(unit);
  });
  if (m_subject.flight && enemyDeathStar) {
    add(OrderButton::DeathStarRun, Icon::DeathStar, "Attack Death Star",
        view.deathStarRunOpen ? "Send this flight down the trench at the exhaust port; one flight at a time."
                              : "Not possible now: the run needs a complete, unshielded Death Star and more of your "
                                "fighters in play than the Empire's.",
        view.deathStarRunOpen);
  }

  struct FleetSpec {
    WidgetId id;
    Icon icon;
    std::string_view label;
    std::string_view help;
    bool on;
  };
  const std::array fleet{
      FleetSpec{hud_id::WithdrawFleet(), Icon::Withdraw, "Withdraw fleet",
                "Withdraw every ship from the battle; asks for confirmation first.", false},
      FleetSpec{hud_id::Observe(), Icon::Eye, m_observing ? "Take command" : "Observe",
                m_observing ? "Take command of your fleet back from the AI."
                            : "Hand your fleet to the AI, which fights with your doctrines; any order takes it back.",
                m_observing},
      FleetSpec{hud_id::ImmediateResult(), Icon::FastForward, "Get Immediate Result",
                "Stop commanding: the battle is finished at once by the simulator, the AI commanding both sides.",
                false},
  };

  const float gap = ui.S(4.0f);
  const bool power = any && !m_subject.flight;
  const float powerWidth = power ? ui.S(48.0f) + 4.0f * ui.S(40.0f) : 0.0f;
  const auto widthOf = [&](std::string_view label, bool iconOnly) {
    return iconOnly ? hud::ButtonWidth(ui, "") : hud::ButtonWidth(ui, label);
  };
  const auto measure = [&](bool ordersIconOnly, bool fleetIconOnly) {
    float total = powerWidth + ui.S(24.0f);
    for (const OrderSpec& order : orders) {
      total += widthOf(order.label, ordersIconOnly) + gap;
    }
    for (const FleetSpec& spec : fleet) {
      total += widthOf(spec.label, fleetIconOnly) + gap;
    }
    return total;
  };
  bool fleetIconOnly = false;
  bool ordersIconOnly = false;
  if (measure(false, false) > row.w) {
    fleetIconOnly = true;
    ordersIconOnly = measure(false, true) > row.w;
  }

  float x = row.x;
  for (const OrderSpec& order : orders) {
    const WidgetId id = hud_id::Order(order.order);
    const Rect button{x, row.y, widthOf(order.label, ordersIconOnly), row.h};
    const bool picking = (order.order == OrderButton::Attack && m_pick.mode == PickMode::Attack)
                         || (order.order == OrderButton::Move && m_pick.mode == PickMode::Move)
                         || (order.order == OrderButton::Escort && m_pick.mode == PickMode::Escort)
                         || (order.order == OrderButton::Tractor && m_pick.mode == PickMode::Tractor)
                         || (order.order == OrderButton::Strike && m_pick.mode == PickMode::Strike);
    if (hud::Button(ui, id, button, order.icon, ordersIconOnly ? "" : order.label, order.enabled, picking)) {
      if (picking) {
        m_pick = {};
      } else {
        PressOrder(order.order);
      }
    }
    ui.Tooltip(id,
               ordersIconOnly ? std::string(order.label) + ": " + std::string(order.help) : std::string(order.help));
    Record(id, button);
    x = button.Right() + gap;
  }
  if (power) {
    x += ui.S(10.0f);
    hud::Caption(ui, {x, row.y, ui.S(44.0f), row.h}, "POWER");
    x += ui.S(46.0f);
    const bool mixed = (doctrine.mixed & battle::kShipPower) != 0;
    for (int i = 0; i < 4; ++i) {
      const auto routing = static_cast<battle::PowerRouting>(i);
      const WidgetId id = hud_id::Power(routing);
      const Rect segment{x, row.y, ui.S(40.0f), row.h};
      if (hud::Segment(ui, id, segment, kPowerLabels[i], !mixed && doctrine.ship.power == routing)) {
        battle::Command command;
        command.target = TargetOf(m_subject);
        command.order = battle::OrderSetPower{routing};
        Submit(std::move(command));
      }
      const OptionText& option = Option(DoctrineField::Power, i);
      ui.Tooltip(id, "Route power: " + std::string(option.name) + ". " + std::string(option.help)
                         + " Switching takes about five seconds.");
      Record(id, segment);
      x = segment.Right() - ui.S(1.0f);
    }
  }

  float right = row.Right();
  for (const FleetSpec& spec : fleet | std::views::reverse) {
    const float width = widthOf(spec.label, fleetIconOnly);
    const Rect button{right - width, row.y, width, row.h};
    if (button.x < x + gap) {
      break;
    }
    if (hud::Button(ui, spec.id, button, spec.icon, fleetIconOnly ? "" : spec.label, !view.finished, spec.on)) {
      if (spec.id == hud_id::WithdrawFleet()) {
        OpenPopupAt(PopupKind::ConfirmWithdraw, button);
      } else if (spec.id == hud_id::Observe()) {
        SetObserving(!m_observing);
      } else {
        m_pick = {};
        m_host.GetImmediateResult();
      }
    }
    ui.Tooltip(spec.id,
               fleetIconOnly ? std::string(spec.label) + ": " + std::string(spec.help) : std::string(spec.help));
    Record(spec.id, button);
    right = button.x - gap;
  }
  if (any) {
    const float divider = right - ui.S(6.0f);
    ui.Draw().FillRect({divider, row.y + ui.S(3.0f), ui.S(1.0f), row.h - ui.S(6.0f)}, theme.panelBorder.WithAlpha(90));
  }
}

void CommandHud::DrawIntent(Ui& ui, const Rect& row)
{
  const Theme& theme = ui.GetTheme();
  const battle::ViewModel& view = m_screen.View();
  UnitIndex unit = m_screen.Hovered();
  if (unit == kNoUnit) {
    for (UnitIndex candidate : SubjectUnits()) {
      if (const battle::ViewUnit* found = FindUnit(view, candidate); found && Selectable(*found)) {
        unit = candidate;
        break;
      }
    }
  }
  if (unit == kNoUnit && m_screen.GetSelection().Units().size() == 1) {
    unit = m_screen.GetSelection().Units().front();
  }
  const battle::ViewUnit* found = unit == kNoUnit ? nullptr : FindUnit(view, unit);
  hud::Caption(ui, {row.x, row.y, ui.S(50.0f), row.h}, "INTENT");
  float x = row.x + ui.S(52.0f);
  if (!found) {
    ui.Label({x, row.y, row.Right() - x, row.h}, "Hover over a ship to see what it is doing and why.",
             {11.0f, theme.textDim});
    return;
  }
  const std::string name = m_screen.Label(unit).name + ":";
  const float nameWidth = ui.TextWidth(name, 12.0f, FontWeight::Bold);
  ui.Label({x, row.y, nameWidth + ui.S(2.0f), row.h}, name,
           {12.0f, FactionColor(theme, found->side), Align::Start, VAlign::Middle, FontWeight::Bold});
  x += nameWidth + ui.S(6.0f);
  ui.Label({x, row.y, row.Right() - x, row.h}, found->intent.empty() ? "No orders" : found->intent,
           {12.0f, found->intent.empty() ? theme.textDim : theme.text});
}

} // namespace reb::battle_ui
