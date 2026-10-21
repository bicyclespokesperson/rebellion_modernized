#include "battle_ui/command_hud.h"

#include <algorithm>
#include <functional>
#include <ranges>

#include "battle_ui/battle_hud.h"
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

constexpr double kToastSeconds = 3.5;
constexpr size_t kMaxNameLength = 32;

WidgetId Indexed(std::string_view name, int64_t index)
{
  return Id(Id(name), index);
}

void PopUtf8(std::string& text)
{
  while (!text.empty()) {
    const auto byte = static_cast<uint8_t>(text.back());
    text.pop_back();
    if ((byte & 0xc0) != 0x80) {
      break;
    }
  }
}

std::string_view PickPrompt(PickMode mode)
{
  switch (mode) {
    case PickMode::Attack:
      return "ATTACK: click an enemy to attack it. Ctrl+click adds further targets in order.";
    case PickMode::Move:
      return "MOVE: click a point on the battle plane.";
    case PickMode::Escort:
      return "ESCORT: click a friendly ship or squadron to escort.";
    case PickMode::Tractor:
      return "TRACTOR: click an enemy capital ship within 2 km to lock it.";
    case PickMode::Strike:
      return "STRIKE: click an enemy capital ship for the flight to attack.";
    case PickMode::ConcentrateFire:
      return "CONCENTRATE FIRE: click the enemy every ship in range should fire on.";
    case PickMode::Superlaser:
      return "SUPERLASER: click an enemy capital ship to destroy it.";
    case PickMode::Protect:
      return "PROTECT: click the friendly ship this group should screen or escort.";
    case PickMode::None:
      break;
  }
  return "";
}

std::string_view PickMistake(PickMode mode)
{
  switch (mode) {
    case PickMode::Escort:
    case PickMode::Protect:
      return "Pick one of your own ships or squadrons to protect";
    case PickMode::Tractor:
    case PickMode::Strike:
    case PickMode::Superlaser:
      return "Pick an enemy capital ship";
    default:
      return "Pick an enemy unit";
  }
}

} // namespace

namespace hud_id
{
WidgetId Chip(DoctrineField field)
{
  return Indexed("hud.chip", static_cast<int>(field));
}
WidgetId Order(OrderButton order)
{
  return Indexed("hud.order", static_cast<int>(order));
}
WidgetId Power(battle::PowerRouting power)
{
  return Indexed("hud.power", static_cast<int>(power));
}
WidgetId Signal(battle::SignalKind signal)
{
  return Indexed("hud.signal", static_cast<int>(signal));
}
WidgetId FieldOption(int index)
{
  return Indexed("hud.option", index);
}
WidgetId FollowGroup()
{
  return Id("hud.follow_group");
}
WidgetId PrioritySlot(int slot)
{
  return Indexed("hud.priority.slot", slot);
}
WidgetId PriorityRemove(int slot)
{
  return Indexed("hud.priority.remove", slot);
}
WidgetId PriorityTag(battle::TargetTag tag)
{
  return Indexed("hud.priority.tag", static_cast<int>(tag));
}
WidgetId FinishDamaged()
{
  return Id("hud.priority.finish");
}
WidgetId RosterGroup(battle::GroupId group)
{
  return Indexed("hud.roster.group", group);
}
WidgetId RosterUnit(UnitIndex unit)
{
  return Indexed("hud.roster.unit", unit);
}
WidgetId RosterNewGroup(bool flight)
{
  return Indexed("hud.roster.new", flight ? 1 : 0);
}
WidgetId DeployHandle(battle::GroupId group)
{
  return Indexed("hud.deploy", group);
}
WidgetId Engage()
{
  return Id("hud.engage");
}
WidgetId Presets()
{
  return Id("hud.presets");
}
WidgetId PresetRow(int index)
{
  return Indexed("hud.preset", index);
}
WidgetId SavePreset()
{
  return Id("hud.preset.save");
}
WidgetId Options()
{
  return Id("hud.options");
}
WidgetId AutoPause(AutoPauseEvent event)
{
  return Indexed("hud.autopause", static_cast<int>(event));
}
WidgetId CommandPhase(int seconds)
{
  return Indexed("hud.phase", seconds);
}
WidgetId Palette(bool colorBlind)
{
  return Indexed("hud.palette", colorBlind ? 1 : 0);
}
WidgetId HudScale(int index)
{
  return Indexed("hud.scale", index);
}
WidgetId WithdrawFleet()
{
  return Id("hud.withdraw_fleet");
}
WidgetId ConfirmWithdraw()
{
  return Id("hud.withdraw_fleet.confirm");
}
WidgetId Observe()
{
  return Id("hud.observe");
}
WidgetId ImmediateResult()
{
  return Id("hud.immediate");
}
WidgetId FireSuperlaser()
{
  return Id("hud.superlaser");
}
WidgetId ClearOverrides()
{
  return Id("hud.clear_overrides");
}
WidgetId Battery(int index)
{
  return Id(Id("hud.battery"), index);
}
WidgetId EditGroup()
{
  return Id("hud.edit_group");
}
WidgetId EventRow(int index)
{
  return Indexed("hud.event", index);
}
WidgetId CollapseRoster()
{
  return Id("hud.collapse.roster");
}
WidgetId CollapseCommand()
{
  return Id("hud.collapse.command");
}
WidgetId CollapseEvents()
{
  return Id("hud.collapse.events");
}
WidgetId CollapseCard()
{
  return Id("hud.collapse.card");
}
} // namespace hud_id

Theme HudTheme(bool colorBlind)
{
  Theme theme;
  if (colorBlind) {
    // Orange against sky blue stays distinct under red-green and blue-yellow colour blindness.
    theme.alliance = Rgb(0xf5a43a);
    theme.empire = Rgb(0x5cc6f2);
    // Bolts follow the sides' colours; ion fire, otherwise blue like the Empire's, is told apart by brightness.
    theme.allianceBolt = Rgb(0xff9a1e);
    theme.empireBolt = Rgb(0x3cc8ff);
    theme.ionBolt = Rgb(0xf0ecff);
  }
  return theme;
}

CommandHud& InstallCommandHud(BattleScreen& screen, BattleHost& host, SettingsStore& store)
{
  auto hud = std::make_unique<CommandHud>(screen, host, store);
  CommandHud& installed = *hud;
  screen.AddPanel(std::move(hud));
  screen.SetInputHook(&installed);
  screen.SetInfoBoxVisible(false); // the roster and the card show the selection
  return installed;
}

CommandHud::CommandHud(BattleScreen& screen, BattleHost& host, SettingsStore& store)
    : m_screen(screen)
    , m_host(host)
    , m_store(store)
    , m_settings(HudSettings::Load(store))
{
  m_hotkeyGroup.fill(kNoGroup);
  m_briefing = screen.View().tick == 0 && screen.Paused() && !screen.View().finished;
  screen.SetTheme(HudTheme(m_settings.colorBlindPalette));
  screen.SetHudScale(m_settings.hudScale);
}

void CommandHud::ApplySettings(HudSettings settings)
{
  m_settings = std::move(settings);
  m_settings.Save(m_store);
  m_screen.SetTheme(HudTheme(m_settings.colorBlindPalette));
  m_screen.SetHudScale(m_settings.hudScale);
}

void CommandHud::Draw(Ui& ui, BattleScreen&)
{
  m_ui = &ui;
  m_rects.clear();
  if (ui.ActiveId() == 0) {
    m_pressAbandoned = false;
  }
  UpdateState(m_screen);
  if (m_briefing) {
    DrawDeployment(ui);
  }
  DrawPickOverlay(ui);
  DrawObserving(ui);
  DrawRoster(ui);
  DrawRightColumn(ui);
  DrawCard(ui);
  DrawPopup(ui);
  DrawDragGhost(ui);
  DrawToasts(ui);
}

// ---- per-frame state ----------------------------------------------------------------------------------------------

void CommandHud::UpdateState(BattleScreen& screen)
{
  const battle::ViewModel& view = screen.View();
  if (m_briefing && (view.tick > 0 || view.finished)) {
    m_briefing = false;
  }
  UpdateHotkeys();
  UpdateFocus();
  m_subject
      = ResolveSubject(view, screen.PlayerSide(), screen.GetSelection().Orderable(view, screen.PlayerSide()), m_focus);
  if (!m_briefing) {
    CheckAutoPause();
  }
  std::erase_if(m_toasts, [&](const Toast& toast) {
    return screen.Seconds() - toast.time > kToastSeconds;
  });
}

void CommandHud::UpdateFocus()
{
  const battle::ViewModel& view = m_screen.View();
  const battle::Side side = m_screen.PlayerSide();
  const std::vector<UnitIndex> selection = m_screen.GetSelection().Orderable(view, side);
  if (m_pendingFocus) {
    const battle::ViewGroup* group = FindGroup(view, *m_pendingFocus);
    m_pendingFocus.reset();
    if (group && group->side == side) {
      std::vector<UnitIndex> members = ActiveMembers(view, *group);
      std::ranges::sort(members);
      if (members == selection) {
        m_focus = group->id;
        m_focusSelection = members;
      }
    }
  }
  if (m_focus == kNoGroup) {
    return;
  }
  const battle::ViewGroup* group = FindGroup(view, m_focus);
  if (!group || group->side != side) {
    m_focus = kNoGroup;
    return;
  }
  std::vector<UnitIndex> members = ActiveMembers(view, *group);
  std::ranges::sort(members);
  if (members == selection) {
    m_focusSelection = std::move(members);
  } else if (selection == m_focusSelection) {
    // The group changed under an untouched selection (squadrons launched, a ship joined): follow the group.
    m_screen.GetSelection().Set(members);
    m_focusSelection = std::move(members);
  } else {
    m_focus = kNoGroup;
  }
}

void CommandHud::UpdateHotkeys()
{
  const battle::ViewModel& view = m_screen.View();
  const battle::Side side = m_screen.PlayerSide();
  HotkeyGroups& hotkeys = m_screen.Hotkeys();
  const auto own = [&](const battle::ViewGroup& group) {
    return group.side == side && !group.members.empty();
  };
  if (!m_hotkeysBound) {
    m_hotkeysBound = true;
    int key = 1;
    for (const bool flights : {false, true}) {
      for (const battle::ViewGroup& group : view.groups) {
        if (!own(group) || group.flight != flights || key >= HotkeyGroups::kCount) {
          continue;
        }
        if (hotkeys.Get(key) == group.members) {
          m_hotkeyGroup[key] = group.id;
          m_hotkeyWritten[key] = group.members;
        }
        ++key;
      }
    }
  }
  for (int key = 1; key < HotkeyGroups::kCount; ++key) {
    if (m_hotkeyGroup[key] == kNoGroup) {
      continue;
    }
    const battle::ViewGroup* group = FindGroup(view, m_hotkeyGroup[key]);
    if (hotkeys.Get(key) != m_hotkeyWritten[key] || !group) {
      m_hotkeyGroup[key] = kNoGroup; // the player stored something else under this key
      continue;
    }
    if (group->members != m_hotkeyWritten[key]) {
      hotkeys.Store(key, group->members);
      m_hotkeyWritten[key] = group->members;
    }
  }
  for (const battle::ViewGroup& group : view.groups) {
    if (!own(group) || HotkeyFor(group.id)) {
      continue;
    }
    for (int key = 1; key < HotkeyGroups::kCount; ++key) {
      if (m_hotkeyGroup[key] == kNoGroup && hotkeys.Get(key).empty()) {
        hotkeys.Store(key, group.members);
        m_hotkeyGroup[key] = group.id;
        m_hotkeyWritten[key] = group.members;
        break;
      }
    }
  }
}

std::optional<int> CommandHud::HotkeyFor(battle::GroupId group) const
{
  for (int key = 1; key < HotkeyGroups::kCount; ++key) {
    if (m_hotkeyGroup[key] == group) {
      return key;
    }
  }
  return std::nullopt;
}

void CommandHud::CheckAutoPause()
{
  const battle::ViewModel& view = m_screen.View();
  const std::vector<AutoPauseHit> hits = m_watcher.Update(
      view, m_screen.PlayerSide(),
      [this](UnitIndex unit) {
        return std::cref(Facts(unit));
      },
      [this](UnitIndex unit) {
        return m_screen.Detail(unit).shipDoctrine.withdraw;
      });
  std::string reason;
  for (const AutoPauseHit& hit : hits) {
    if (m_settings.AutoPause(hit.event) && reason.empty()) {
      reason = hit.text;
    }
  }
  if (m_watcher.PhaseDue(view.battleSeconds, m_settings.commandPhaseSeconds) && reason.empty()) {
    reason = "Command phase at " + FormatClock(view.battleSeconds);
  }
  if (!reason.empty() && !m_screen.Paused() && !view.finished) {
    m_screen.SetPaused(true);
    AddToast("Paused: " + reason);
  }
}

const UnitFacts& CommandHud::Facts(UnitIndex unit) const
{
  static const UnitFacts kNone;
  if (unit < 0) {
    return kNone;
  }
  if (static_cast<size_t>(unit) >= m_facts.size()) {
    m_facts.resize(static_cast<size_t>(unit) + 1);
  }
  std::optional<UnitFacts>& facts = m_facts[unit];
  if (!facts) {
    const battle::ViewUnitDetail detail = m_screen.Detail(unit);
    facts = UnitFacts{m_screen.Label(unit).name, detail.hangarCapacity > 0, detail.tractorRange > 0.0f};
  }
  facts->name = m_screen.Label(unit).name;
  return *facts;
}

void CommandHud::Record(WidgetId id, const Rect& rect)
{
  m_rects[id] = rect;
}

std::optional<Rect> CommandHud::ControlRect(WidgetId id) const
{
  const auto it = m_rects.find(id);
  return it == m_rects.end() ? std::nullopt : std::optional<Rect>(it->second);
}

void CommandHud::AddToast(std::string text)
{
  m_toastTexts.push_back(text);
  m_toasts.push_back({std::move(text), m_screen.Seconds()});
  if (m_toasts.size() > 3) {
    m_toasts.erase(m_toasts.begin());
  }
}

battle::Side CommandHud::Enemy() const
{
  return m_screen.PlayerSide() == battle::Side::Alliance ? battle::Side::Empire : battle::Side::Alliance;
}

std::string CommandHud::GroupTitle(const battle::ViewGroup& group) const
{
  if (!group.name.empty()) {
    return group.name;
  }
  return (group.flight ? "Flight " : "Task Force ") + std::to_string(group.id + 1);
}

CommandHud::CardDoctrine CommandHud::ReadCardDoctrine() const
{
  CardDoctrine card;
  const battle::ViewModel& view = m_screen.View();
  const battle::ViewGroup* group = nullptr;
  if (m_subject.IsGroup()) {
    group = FindGroup(view, m_subject.group);
    if (group) {
      card.ship = group->shipDoctrine;
      card.flight = group->flightDoctrine;
    }
  } else if (!m_subject.units.empty()) {
    const battle::ViewUnitDetail first = m_screen.Detail(m_subject.units.front());
    card.ship = first.shipDoctrine;
    card.flight = first.flightDoctrine;
    card.overrides = first.overrides;
    for (size_t i = 1; i < m_subject.units.size(); ++i) {
      const battle::ViewUnitDetail other = m_screen.Detail(m_subject.units[i]);
      card.overrides |= other.overrides;
      for (DoctrineField field : m_subject.flight ? FlightFields() : ShipFields()) {
        if (FieldDiffers(field, m_subject.flight, card.ship, card.flight, other.shipDoctrine, other.flightDoctrine)) {
          card.mixed |= FieldMask(field, m_subject.flight);
        }
      }
    }
    if (const battle::ViewUnit* unit = FindUnit(view, m_subject.units.front())) {
      group = FindGroup(view, unit->group);
    }
  }
  if (group) {
    card.groupShip = group->shipDoctrine;
    card.groupFlight = group->flightDoctrine;
    card.hasGroup = true;
  }
  return card;
}

std::vector<UnitIndex> CommandHud::SubjectUnits() const
{
  if (!m_subject.IsGroup()) {
    return m_subject.units;
  }
  const battle::ViewGroup* group = FindGroup(m_screen.View(), m_subject.group);
  return group ? group->members : std::vector<UnitIndex>{};
}

bool CommandHud::SubjectHas(bool (*test)(const UnitFacts&)) const
{
  return std::ranges::any_of(SubjectUnits(), [&](UnitIndex unit) {
    return test(Facts(unit));
  });
}

// ---- actions ------------------------------------------------------------------------------------------------------

void CommandHud::Submitted(const battle::Command&)
{
  // Any order takes command back from the AI, a right-click on the battle view included.
  if (m_observing) {
    SetObserving(false);
  }
}

bool CommandHud::Submit(battle::Command command)
{
  const bool accepted = m_screen.Submit(std::move(command));
  if (!accepted) {
    AddToast("The order was not accepted");
  }
  return accepted;
}

void CommandHud::SetObserving(bool observing)
{
  if (observing == m_observing) {
    return;
  }
  m_observing = observing;
  m_host.SetObserving(observing);
  if (observing) {
    m_pick = {};
    AddToast("Observing: the AI commands your fleet with your doctrines");
  } else {
    AddToast("You have command");
  }
}

void CommandHud::Engage()
{
  m_briefing = false;
  m_deployDrag = kNoGroup;
  if (m_popup.kind == PopupKind::ConfirmWithdraw) {
    ClosePopup();
  }
  m_screen.SetPaused(false);
}

void CommandHud::FocusGroup(battle::GroupId group)
{
  const battle::ViewGroup* found = FindGroup(m_screen.View(), group);
  if (!found || found->side != m_screen.PlayerSide()) {
    return;
  }
  std::vector<UnitIndex> members = ActiveMembers(m_screen.View(), *found);
  m_screen.GetSelection().Set(members);
  std::ranges::sort(members);
  m_focusSelection = std::move(members);
  m_focus = group;
  m_subject = ResolveSubject(m_screen.View(), m_screen.PlayerSide(),
                             m_screen.GetSelection().Orderable(m_screen.View(), m_screen.PlayerSide()), m_focus);
}

void CommandHud::FoldGroup(battle::GroupId group, bool folded)
{
  m_autoFolded = true;
  if (folded) {
    m_collapsedGroups.insert(group);
  } else {
    m_collapsedGroups.erase(group);
  }
}

void CommandHud::StartPick(PickMode mode)
{
  ClosePopup();
  Pick pick{mode, {}, 0};
  if (mode == PickMode::Superlaser) {
    for (const battle::ViewUnit& unit : m_screen.View().units) {
      if (unit.kind == battle::ViewUnitKind::DeathStar && unit.side == m_screen.PlayerSide()) {
        pick.who.units = {unit.unit};
      }
    }
  } else if (mode != PickMode::ConcentrateFire) {
    pick.who = TargetOf(m_subject);
    pick.subject = m_subject;
    if (m_subject.Empty()) {
      AddToast("Select ships or a group first");
      return;
    }
  }
  m_pick = std::move(pick);
}

void CommandHud::PressOrder(OrderButton order)
{
  if (m_subject.Empty()) {
    AddToast("Select ships or a group first");
    return;
  }
  battle::Command command;
  command.target = TargetOf(m_subject);
  switch (order) {
    case OrderButton::Attack:
      StartPick(PickMode::Attack);
      return;
    case OrderButton::Move:
      StartPick(PickMode::Move);
      return;
    case OrderButton::Escort:
      StartPick(PickMode::Escort);
      return;
    case OrderButton::Tractor:
      StartPick(PickMode::Tractor);
      return;
    case OrderButton::Strike:
      StartPick(PickMode::Strike);
      return;
    case OrderButton::Hold:
      command.order = battle::OrderHold{};
      break;
    case OrderButton::Launch:
      command.order = battle::OrderLaunch{};
      break;
    case OrderButton::Recall:
      command.order = battle::OrderRecall{};
      break;
    case OrderButton::Recover:
      command.order = battle::OrderRecover{};
      break;
    case OrderButton::Withdraw:
      command.order = battle::OrderWithdraw{};
      break;
    case OrderButton::DeathStarRun:
      command.order = battle::OrderDeathStarRun{};
      break;
  }
  Submit(std::move(command));
}

void CommandHud::SubmitDoctrine(const CardDoctrine& doctrine, battle::DoctrineOverrideMask fields)
{
  Submit(SetDoctrineCommand(m_subject, doctrine.ship, doctrine.flight, fields));
}

void CommandHud::SubmitPriorities(const CardDoctrine& doctrine, const battle::TargetPriorities& priorities)
{
  CardDoctrine edited = doctrine;
  (m_subject.flight ? edited.flight.priorities : edited.ship.priorities) = priorities;
  SubmitDoctrine(edited, FieldMask(DoctrineField::Priorities, m_subject.flight));
}

void CommandHud::ClearOverrides(battle::DoctrineOverrideMask fields)
{
  if (!m_subject.IsGroup() && !m_subject.Empty()) {
    Submit(ClearOverridesCommand(m_subject, fields));
  }
}

void CommandHud::WithdrawFleet()
{
  battle::Command command;
  for (const battle::ViewGroup& group : m_screen.View().groups) {
    if (group.side == m_screen.PlayerSide() && !group.members.empty()) {
      command.target.groups.push_back(group.id);
    }
  }
  for (const battle::ViewUnit& unit : m_screen.View().units) {
    if (unit.side == m_screen.PlayerSide() && unit.group == kNoGroup && Selectable(unit)
        && unit.kind != battle::ViewUnitKind::DeathStar) {
      command.target.units.push_back(unit.unit);
    }
  }
  command.order = battle::OrderWithdraw{};
  if (Submit(std::move(command))) {
    AddToast("Fleet withdrawing: ships jump as soon as they can");
  }
}

void CommandHud::OpenPopupAt(PopupKind kind, const Rect& anchor, DoctrineField field)
{
  if (m_popup.kind == kind && m_popup.field == field && kind != PopupKind::None) {
    ClosePopup();
    return;
  }
  m_pick = {};
  m_popup = {kind, field, m_subject, anchor, {}};
  m_prioritySlot = -1;
  m_priorityDrag = -1;
  m_naming = false;
}

void CommandHud::ClosePopup()
{
  m_popup = {};
  m_naming = false;
  m_prioritySlot = -1;
  m_priorityDrag = -1;
}

bool CommandHud::PickValid(UnitIndex unit) const
{
  const battle::ViewUnit* found = unit == kNoUnit ? nullptr : FindUnit(m_screen.View(), unit);
  if (!found || !Selectable(*found)) {
    return false;
  }
  const bool enemy = found->side != m_screen.PlayerSide();
  const bool capital = found->kind == battle::ViewUnitKind::CapitalShip;
  switch (m_pick.mode) {
    case PickMode::Attack:
    case PickMode::ConcentrateFire:
      return enemy;
    case PickMode::Escort:
    case PickMode::Protect:
      return !enemy && std::ranges::find(m_pick.who.units, unit) == m_pick.who.units.end()
             && std::ranges::find(m_pick.who.groups, found->group) == m_pick.who.groups.end();
    case PickMode::Tractor:
    case PickMode::Strike:
    case PickMode::Superlaser:
      return enemy && capital;
    case PickMode::Move:
    case PickMode::None:
      break;
  }
  return false;
}

// ---- input hook ---------------------------------------------------------------------------------------------------

bool CommandHud::ViewClick(UnitIndex unit, battle::Vec2 world, uint8_t modifiers)
{
  if (m_popup.kind != PopupKind::None) {
    ClosePopup();
    return true;
  }
  if (m_pick.mode == PickMode::None) {
    if (unit == kNoUnit) {
      m_focus = kNoGroup;
    }
    return false;
  }
  battle::Command command;
  command.target = m_pick.who;
  command.queue = (modifiers & kModShift) != 0;
  if (m_pick.mode == PickMode::Move) {
    command.order = battle::OrderMove{world, std::nullopt};
    Submit(std::move(command));
    m_pick = {};
    return true;
  }
  if (!PickValid(unit)) {
    AddToast(std::string(PickMistake(m_pick.mode)));
    return true;
  }
  bool keepPicking = false;
  switch (m_pick.mode) {
    case PickMode::Attack:
      command.order = battle::OrderAttack{{unit}};
      command.queue = command.queue || m_pick.picked > 0;
      keepPicking = (modifiers & kModCtrl) != 0;
      break;
    case PickMode::Escort:
      command.order = battle::OrderEscort{unit};
      break;
    case PickMode::Tractor:
      command.order = battle::OrderTractor{unit};
      break;
    case PickMode::Strike:
      command.order = battle::OrderStrike{unit};
      break;
    case PickMode::ConcentrateFire:
      command.target = {};
      command.order = battle::OrderSignal{battle::SignalKind::ConcentrateFire, unit};
      break;
    case PickMode::Superlaser:
      command.order = battle::OrderFireSuperlaser{unit};
      break;
    case PickMode::Protect:
      {
        battle::ShipDoctrine ship;
        battle::FlightDoctrine flight;
        ship.screenTarget = unit;
        flight.escortTarget = unit;
        command = SetDoctrineCommand(m_pick.subject, ship, flight,
                                     m_pick.subject.flight ? battle::DoctrineOverrideMask{battle::kFlightEscortTarget}
                                                           : battle::DoctrineOverrideMask{battle::kShipScreenTarget});
        break;
      }
    case PickMode::Move:
    case PickMode::None:
      break;
  }
  if (Submit(std::move(command))) {
    ++m_pick.picked;
  }
  if (!keepPicking) {
    m_pick = {};
  }
  return true;
}

bool CommandHud::Cancel()
{
  if (m_pick.mode != PickMode::None) {
    m_pick = {};
    return true;
  }
  if (m_popup.kind != PopupKind::None) {
    ClosePopup();
    return true;
  }
  if (m_rosterDragging || m_deployDrag != kNoGroup || m_priorityDrag >= 0) {
    m_rosterDragging = false;
    m_deployDrag = kNoGroup;
    m_priorityDrag = -1;
    m_pressAbandoned = true;
    return true;
  }
  return false;
}

bool CommandHud::Keyboard(const InputEvent& event)
{
  if (const auto* text = std::get_if<TextInput>(&event)) {
    if (m_naming && m_presetName.size() + text->text.size() <= kMaxNameLength) {
      m_presetName += text->text;
    }
    return m_naming;
  }
  const auto* key = std::get_if<KeyEvent>(&event);
  if (!key) {
    return false;
  }
  if (m_naming) {
    if (!key->down) {
      return false;
    }
    if (key->key == Key::Backspace) {
      PopUtf8(m_presetName);
    } else if (key->key == Key::Escape) {
      m_naming = false;
    } else if (key->key == Key::Return) {
      m_naming = false;
      if (!m_presetName.empty()) {
        const CardDoctrine doctrine = ReadCardDoctrine();
        DoctrinePreset preset{
            m_presetName, m_subject.flight, doctrine.ship, doctrine.flight, PresetFields(m_subject.flight), false};
        HudSettings settings = m_settings;
        std::erase_if(settings.presets, [&](const DoctrinePreset& existing) {
          return existing.name == preset.name && existing.flight == preset.flight;
        });
        if (static_cast<int>(settings.presets.size()) >= HudSettings::kMaxPresets) {
          AddToast("The preset list is full: delete one first");
          return true;
        }
        settings.presets.push_back(std::move(preset));
        ApplySettings(std::move(settings));
        AddToast("Saved preset \"" + m_presetName + "\"");
      }
    }
    return true;
  }
  if (!key->down || key->repeat) {
    return false;
  }
  if (const int digit = KeyDigit(key->key); digit > 0 && !(key->modifiers & kModCtrl)) {
    if (m_hotkeyGroup[digit] != kNoGroup) {
      m_pendingFocus = m_hotkeyGroup[digit];
    }
    return false;
  }
  if (key->key == Key::Return && m_briefing) {
    Engage();
    return true;
  }
  return false;
}

// ---- overlays -----------------------------------------------------------------------------------------------------

void CommandHud::DrawPickOverlay(Ui& ui)
{
  if (m_pick.mode == PickMode::None) {
    return;
  }
  const Theme& theme = ui.GetTheme();
  const Rect view = m_screen.Layout().view;
  const UnitIndex hovered = m_screen.Hovered();
  if (hovered != kNoUnit) {
    if (const battle::ViewUnit* unit = FindUnit(m_screen.View(), hovered)) {
      const Camera& camera = m_screen.GetCamera();
      const Point center = camera.WorldToScreen(unit->position);
      const float r = std::max(camera.PixelsFor(unit->radius), ui.S(10.0f)) + ui.S(8.0f);
      const bool valid = PickValid(hovered);
      const Color color = valid ? Rgb(0xffd25a) : theme.disabledText;
      ui.Draw().PushClip(view);
      ui.Draw().Circle(center, r, color, Stroke{ui.S(2.0f), valid ? 0.0f : ui.S(4.0f), ui.S(4.0f)});
      if (valid) {
        for (int i = 0; i < 4; ++i) {
          const Point dir = ScreenDirection(static_cast<float>(i) * kPi * 0.5f);
          ui.Draw().Line(center + dir * (r - ui.S(5.0f)), center + dir * (r + ui.S(6.0f)), color, Stroke{ui.S(2.0f)});
        }
      }
      ui.Draw().PopClip();
    }
  }
  const std::string_view prompt = PickPrompt(m_pick.mode);
  const std::string hint = "Right-click or Esc cancels";
  const float size = 13.0f;
  const float width = ui.TextWidth(prompt, size, FontWeight::Bold) + ui.TextWidth(hint, 11.0f) + ui.S(44.0f);
  const Rect box{view.Center().x - width * 0.5f, view.y + ui.S(42.0f), width, ui.S(28.0f)};
  ui.Draw().FillRect(box, theme.tooltip);
  ui.Draw().StrokeRect(box, Rgb(0xffd25a, 200), ui.S(1.0f));
  const Rect text = box.Inset(ui.S(12.0f), 0.0f);
  ui.Label(text, prompt, {size, Rgb(0xffe6a0), Align::Start, VAlign::Middle, FontWeight::Bold});
  ui.Label(text, hint, {11.0f, theme.textDim, Align::End});
}

void CommandHud::DrawObserving(Ui& ui)
{
  if (!m_observing) {
    return;
  }
  const Theme& theme = ui.GetTheme();
  const Rect view = m_screen.Layout().view;
  const std::string_view text = "OBSERVING: the AI commands your fleet with your doctrines";
  const float buttonWidth = hud::ButtonWidth(ui, "Take command");
  const float width = ui.TextWidth(text, 13.0f, FontWeight::Bold) + buttonWidth + ui.S(36.0f);
  const Rect box{view.Center().x - width * 0.5f, view.y + ui.S(42.0f), width, ui.S(30.0f)};
  ui.Block(box);
  ui.Draw().FillRect(box, theme.tooltip);
  ui.Draw().StrokeRect(box, theme.accent, ui.S(1.0f));
  ui.Label(box.Inset(ui.S(12.0f), 0.0f), text, {13.0f, theme.title, Align::Start, VAlign::Middle, FontWeight::Bold});
  const Rect button{box.Right() - buttonWidth - ui.S(4.0f), box.y + ui.S(3.0f), buttonWidth, box.h - ui.S(6.0f)};
  const WidgetId id = Id("hud.observing.take");
  if (hud::Button(ui, id, button, Icon::Eye, "Take command")) {
    SetObserving(false);
  }
  ui.Tooltip(id, "Take command of your fleet back from the AI; any order does the same.");
}

void CommandHud::DrawToasts(Ui& ui)
{
  const Theme& theme = ui.GetTheme();
  const Rect view = m_screen.Layout().view;
  float y = view.y + ui.S(m_pick.mode == PickMode::None && !m_observing ? 42.0f : 76.0f);
  for (const Toast& toast : m_toasts | std::views::reverse) {
    const float age = static_cast<float>(m_screen.Seconds() - toast.time);
    const float fade = std::clamp((static_cast<float>(kToastSeconds) - age) / 0.6f, 0.0f, 1.0f);
    const float width = ui.TextWidth(toast.text, 12.0f) + ui.S(24.0f);
    const Rect box{view.Center().x - width * 0.5f, y, width, ui.S(24.0f)};
    ui.Draw().FillRect(box, theme.tooltip.Faded(fade));
    ui.Draw().StrokeRect(box, theme.panelBorder.Faded(fade), ui.S(1.0f));
    ui.Label(box, toast.text, {12.0f, theme.text.Faded(fade), Align::Center});
    y += box.h + ui.S(4.0f);
  }
}

Rect CommandHud::PlacePopup(Ui& ui, float width, float height) const
{
  const Rect screen = m_screen.Layout().screen;
  const Rect& anchor = m_popup.anchor;
  const float gap = ui.S(4.0f);
  Rect rect{anchor.x, anchor.y - gap - height, width, height};
  if (rect.y < m_screen.Layout().topBar.Bottom()) {
    rect.y = anchor.Bottom() + gap;
  }
  rect.x = std::clamp(rect.x, ui.S(4.0f), std::max(ui.S(4.0f), screen.Right() - width - ui.S(4.0f)));
  const float top = m_screen.Layout().topBar.Bottom();
  rect.y = std::clamp(rect.y, top, std::max(top, screen.Bottom() - height));
  return rect;
}

} // namespace reb::battle_ui
