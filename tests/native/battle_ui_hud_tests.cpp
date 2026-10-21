// The command HUD (src/game/battle_ui/command_hud.h) driven through the battle screen with synthetic input, against
// the fabricated battle: doctrine edits and their masks, overrides, priority editing, order targeting, roster
// regrouping, deployment, settings and auto-pause.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string_view>

#include "battle_ui/auto_pause.h"
#include "battle_ui/battle_screen.h"
#include "battle_ui/command_hud.h"
#include "battle_ui/doctrine_edit.h"
#include "battle_ui/fake_battle.h"
#include "battle_ui/hud_settings.h"

namespace
{

using namespace reb;
using namespace reb::battle_ui;
using battle::Side;
using battle::TargetTag;
using battle::UnitIndex;

#define REQUIRE(condition)                                                                                             \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::fprintf(stderr, "%s:%d: requirement failed: %s\n", __FILE__, __LINE__, #condition);                         \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

// Fabricated battle unit indices (fake_battle.cpp).
constexpr UnitIndex kLiberty = 0;
constexpr UnitIndex kDefiance = 1;
constexpr UnitIndex kSalvation = 3;
constexpr UnitIndex kTantive = 4;
constexpr UnitIndex kRedSquadron = 7;
constexpr UnitIndex kAvenger = 10;
constexpr UnitIndex kHarrow = 12;
constexpr battle::GroupId kTaskForce1 = 0;
constexpr battle::GroupId kTaskForce2 = 1;

class RecordingHost : public BattleHost
{
  public:
  void GetImmediateResult() override
  {
    ++immediate;
  }
  void SetObserving(bool value) override
  {
    observing = value;
  }
  int immediate = 0;
  bool observing = false;
};

class KnowledgeBattle : public FakeBattle
{
  public:
  battle::ViewUnitDetail Detail(UnitIndex unit) const override
  {
    auto detail = FakeBattle::Detail(unit);
    if (hideEnemy && unit >= kAvenger) {
      detail.briefingKnown = {false, true};
    }
    return detail;
  }
  void BuildViewModel(float alpha, battle::ViewModel& out) const override
  {
    FakeBattle::BuildViewModel(alpha, out);
    if (friendlyProjector) {
      out.wells.push_back({kLiberty, {-6000.0f, 0.0f}, 22500.0f, Side::Alliance, true});
    }
    if (enemyBatteries) {
      out.coverRadius = out.planetRadius + 15000.0f;
      battle::ViewDefense battery;
      battery.side = Side::Empire;
      battery.weapon = battle::WeaponKind::Turbolaser;
      battery.name = "LNR Series II";
      battery.site = {out.planetCenter.x + out.planetRadius, out.planetCenter.y};
      battery.dps = 24.7f;
      battery.briefingKnown = {batteriesKnown, true};
      battery.target = batteriesFiring ? kLiberty : battle::kNoUnit;
      battery.sinceFired = batteriesFiring ? 0.5f : -1.0f;
      out.defenses.push_back(battery);
      battery.weapon = battle::WeaponKind::Ion;
      battery.name = "KDY-150";
      out.defenses.push_back(battery);
    }
  }
  bool hideEnemy = false;
  bool friendlyProjector = false;
  bool enemyBatteries = false;
  bool batteriesKnown = true;
  bool batteriesFiring = false;
};

class Harness
{
  public:
  explicit Harness(bool briefing = true, Side player = Side::Alliance, bool deathStar = false,
                   ScreenInfo info = {1920, 1080, 1.0f, 1.0f})
  {
    if (deathStar) {
      source.AddDeathStar({9800.0f, -3800.0f});
    }
    BattleScreenOptions options;
    options.playerSide = player;
    options.startPaused = briefing;
    screen.emplace(source, text, sprites, options);
    hud = &InstallCommandHud(*screen, host, store);
    screen->HandleInput(ScreenChanged{info});
    Frame();
  }

  void Frame()
  {
    DrawList list;
    screen->Update(0.0);
    screen->Build(list);
  }
  Point At(WidgetId id) const
  {
    const std::optional<Rect> rect = hud->ControlRect(id);
    REQUIRE(rect.has_value());
    return rect->Center();
  }
  Point OnScreen(UnitIndex unit) const
  {
    return screen->GetCamera().WorldToScreen(FindUnit(screen->View(), unit)->position);
  }
  void Move(Point to)
  {
    screen->HandleInput(MouseMove{to, m_buttons, 0});
  }
  void Button(Point at, bool down, uint8_t modifiers = 0)
  {
    m_buttons = down ? ButtonBit(MouseButton::Left) : 0;
    screen->HandleInput(MouseButtonEvent{at, MouseButton::Left, down, 1, modifiers});
  }
  // The HUD acts on a click in the next frame; the frame after that shows the result.
  void Click(Point at, uint8_t modifiers = 0)
  {
    Move(at);
    Button(at, true, modifiers);
    Button(at, false, modifiers);
    Frame();
    Frame();
  }
  void Click(WidgetId id, uint8_t modifiers = 0)
  {
    Click(At(id), modifiers);
  }
  // Presses, moves in steps with a frame after each, and releases: the way a hand drags.
  void Drag(Point from, Point to)
  {
    Move(from);
    Button(from, true);
    Frame();
    for (int step = 1; step <= 6; ++step) {
      const float t = static_cast<float>(step) / 6.0f;
      Move({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t});
      Frame();
    }
    Button(to, false);
    Frame();
    Frame();
  }
  void Key(battle_ui::Key key, uint8_t modifiers = 0)
  {
    screen->HandleInput(KeyEvent{key, true, false, modifiers});
    screen->HandleInput(KeyEvent{key, false, false, modifiers});
    Frame();
    Frame();
  }
  size_t CommandCount() const
  {
    return source.Commands().size();
  }
  const battle::Command& Last() const
  {
    REQUIRE(!source.Commands().empty());
    return source.Commands().back();
  }
  template <class T>
  const T& LastOrder() const
  {
    const T* order = std::get_if<T>(&Last().order);
    REQUIRE(order != nullptr);
    return *order;
  }
  const battle::ViewGroup& Group(battle::GroupId id) const
  {
    const battle::ViewGroup* group = FindGroup(screen->View(), id);
    REQUIRE(group != nullptr);
    return *group;
  }

  KnowledgeBattle source;
  FixedTextMeasurer text;
  NoSprites sprites;
  MemorySettingsStore store;
  RecordingHost host;
  std::optional<BattleScreen> screen;
  CommandHud* hud = nullptr;

  private:
  uint8_t m_buttons = 0;
};

void GroupDoctrineEditSetsOnlyThatField()
{
  Harness h;
  h.Key(DigitKey(1));
  REQUIRE(h.hud->Subject().IsGroup() && h.hud->Subject().group == kTaskForce1);
  h.Click(hud_id::Chip(DoctrineField::Range));
  REQUIRE(h.hud->OpenPopup() == PopupKind::Field);
  h.Click(hud_id::FieldOption(static_cast<int>(battle::EngagementRange::StandOff)));
  REQUIRE(h.hud->OpenPopup() == PopupKind::None);
  const auto& order = h.LastOrder<battle::OrderSetShipDoctrine>();
  REQUIRE(order.fields == battle::kShipRange);
  REQUIRE(order.clear == 0);
  REQUIRE(order.doctrine.range == battle::EngagementRange::StandOff);
  REQUIRE(h.Last().target.groups == std::vector<battle::GroupId>{kTaskForce1});
  REQUIRE(h.Last().target.units.empty());
  REQUIRE(h.Group(kTaskForce1).shipDoctrine.range == battle::EngagementRange::StandOff);
  REQUIRE(h.Group(kTaskForce2).shipDoctrine.range == battle::EngagementRange::Optimal);

  // A setting's options close when the card switches to another subject (here a flight).
  h.Click(hud_id::Chip(DoctrineField::Range));
  REQUIRE(h.hud->OpenPopup() == PopupKind::Field);
  h.Key(DigitKey(4));
  REQUIRE(h.hud->Subject().flight && h.hud->OpenPopup() == PopupKind::None);
  h.Key(DigitKey(1));

  // Choosing Screen asks for the ship to protect, which is a doctrine field of its own.
  h.Click(hud_id::Chip(DoctrineField::Role));
  h.Click(hud_id::FieldOption(static_cast<int>(battle::Role::Screen)));
  REQUIRE(h.LastOrder<battle::OrderSetShipDoctrine>().fields == battle::kShipRole);
  REQUIRE(h.hud->Picking() == PickMode::Protect);
  h.Click(h.OnScreen(kSalvation));
  const auto& screenTarget = h.LastOrder<battle::OrderSetShipDoctrine>();
  REQUIRE(screenTarget.fields == battle::kShipScreenTarget && screenTarget.doctrine.screenTarget == kSalvation);
  REQUIRE(h.Group(kTaskForce1).shipDoctrine.role == battle::Role::Screen);
  REQUIRE(h.Group(kTaskForce1).shipDoctrine.screenTarget == kSalvation);
  REQUIRE(h.hud->Picking() == PickMode::None);

  // Clicking the group's header in the roster picks the group too, and flights get the flight card.
  h.Click(hud_id::RosterGroup(3));
  REQUIRE(h.hud->Subject().IsGroup() && h.hud->Subject().group == 3 && h.hud->Subject().flight);
  h.Click(hud_id::Chip(DoctrineField::Mission));
  h.Click(hud_id::FieldOption(static_cast<int>(battle::FlightMission::Escort)));
  const auto& flight = h.LastOrder<battle::OrderSetFlightDoctrine>();
  REQUIRE(flight.fields == battle::kFlightMission);
  REQUIRE(flight.doctrine.mission == battle::FlightMission::Escort);
  REQUIRE(h.Group(3).flightDoctrine.mission == battle::FlightMission::Escort);
}

void SmallScreensFoldTheLargestGroups()
{
  Harness h(true, Side::Alliance, false, {960, 540, 1.0f, 1.0f});
  REQUIRE(h.hud->ControlRect(hud_id::RosterGroup(kTaskForce1)) && h.hud->ControlRect(hud_id::RosterGroup(3)));
  REQUIRE(!h.hud->ControlRect(hud_id::RosterUnit(kLiberty)));
  h.Click(h.At(hud_id::RosterGroup(kTaskForce1))
          - Point{h.hud->ControlRect(hud_id::RosterGroup(kTaskForce1))->w * 0.5f - 8.0f, 0.0f});
  REQUIRE(h.hud->ControlRect(hud_id::RosterUnit(kLiberty)));
}

void ShipEditsAreOverridesAndCanBeCleared()
{
  Harness h;
  h.Click(hud_id::RosterUnit(kDefiance));
  REQUIRE(!h.hud->Subject().IsGroup());
  REQUIRE(h.hud->Subject().units == std::vector<UnitIndex>{kDefiance});
  h.Click(hud_id::Chip(DoctrineField::Facing));
  h.Click(hud_id::FieldOption(static_cast<int>(battle::Facing::Broadside)));
  const auto& set = h.LastOrder<battle::OrderSetShipDoctrine>();
  REQUIRE(set.fields == battle::kShipFacing && set.clear == 0);
  REQUIRE(h.Last().target.units == std::vector<UnitIndex>{kDefiance} && h.Last().target.groups.empty());
  REQUIRE(h.screen->Detail(kDefiance).overrides == battle::kShipFacing);
  REQUIRE(h.screen->Detail(kDefiance).shipDoctrine.facing == battle::Facing::Broadside);
  REQUIRE(h.Group(kTaskForce1).shipDoctrine.facing == battle::Facing::Auto);

  h.Click(hud_id::Chip(DoctrineField::Power));
  h.Click(hud_id::FieldOption(static_cast<int>(battle::PowerRouting::Shields)));
  REQUIRE(h.screen->Detail(kDefiance).overrides == (battle::kShipFacing | battle::kShipPower));

  // "Follow group" in a setting's options drops just that override.
  h.Click(hud_id::Chip(DoctrineField::Facing));
  h.Click(hud_id::FollowGroup());
  const auto& follow = h.LastOrder<battle::OrderSetShipDoctrine>();
  REQUIRE(follow.fields == 0 && follow.clear == battle::kShipFacing);
  REQUIRE(h.screen->Detail(kDefiance).overrides == battle::kShipPower);
  REQUIRE(h.screen->Detail(kDefiance).shipDoctrine.facing == battle::Facing::Auto);

  // The header's Clear drops the rest.
  h.Click(hud_id::ClearOverrides());
  REQUIRE(h.LastOrder<battle::OrderSetShipDoctrine>().clear == battle::kShipPower);
  REQUIRE(h.screen->Detail(kDefiance).overrides == 0);

  // Edit group switches the card to the ship's group.
  h.Click(hud_id::EditGroup());
  REQUIRE(h.hud->Subject().IsGroup() && h.hud->Subject().group == kTaskForce1);
}

void PriorityEditsReorderAndMask()
{
  battle::TargetPriorities list;
  list = SetPriority(list, 0, TargetTag::Carrier);
  list = SetPriority(list, 1, TargetTag::Interdictor);
  list = SetPriority(list, 2, TargetTag::Damaged);
  REQUIRE(list.count == 3 && list.tags[0] == TargetTag::Carrier && list.tags[2] == TargetTag::Damaged);
  list = MovePriority(list, 2, 0);
  REQUIRE(list.tags[0] == TargetTag::Damaged && list.tags[1] == TargetTag::Carrier
          && list.tags[2] == TargetTag::Interdictor);
  list = MovePriority(list, 0, 2);
  REQUIRE(list.tags[0] == TargetTag::Carrier && list.tags[2] == TargetTag::Damaged);
  list = SetPriority(list, 3, TargetTag::Carrier); // already listed: moves instead of duplicating
  REQUIRE(list.count == 3 && list.tags[2] == TargetTag::Carrier);
  list = RemovePriority(list, 0);
  REQUIRE(list.count == 2 && list.tags[0] == TargetTag::Damaged && list.tags[1] == TargetTag::Carrier);

  Harness h;
  h.Key(DigitKey(2));
  h.Click(hud_id::Chip(DoctrineField::Priorities));
  REQUIRE(h.hud->OpenPopup() == PopupKind::Priorities);
  h.Click(hud_id::PriorityTag(TargetTag::Carrier));
  h.Click(hud_id::PriorityTag(TargetTag::Interdictor));
  h.Click(hud_id::PriorityTag(TargetTag::Damaged));
  const battle::TargetPriorities& added = h.Group(kTaskForce2).shipDoctrine.priorities;
  REQUIRE(added.count == 4 && added.tags[0] == TargetTag::Nearest && added.tags[3] == TargetTag::Damaged);
  REQUIRE(h.LastOrder<battle::OrderSetShipDoctrine>().fields == battle::kShipPriorities);

  // Drag the last slot to the top.
  const Point from = h.At(hud_id::PrioritySlot(3));
  const Point to = h.At(hud_id::PrioritySlot(0));
  h.Drag(from, {to.x, to.y - 4.0f});
  const battle::TargetPriorities& moved = h.Group(kTaskForce2).shipDoctrine.priorities;
  REQUIRE(moved.tags[0] == TargetTag::Damaged && moved.tags[1] == TargetTag::Nearest
          && moved.tags[3] == TargetTag::Interdictor);

  // Select a slot, then a tag replaces it; x removes; finish damaged toggles.
  h.Click(hud_id::PrioritySlot(1));
  h.Click(hud_id::PriorityTag(TargetTag::Bomber));
  REQUIRE(h.Group(kTaskForce2).shipDoctrine.priorities.tags[1] == TargetTag::Bomber);
  h.Click(hud_id::PriorityRemove(0));
  REQUIRE(h.Group(kTaskForce2).shipDoctrine.priorities.count == 3);
  REQUIRE(h.Group(kTaskForce2).shipDoctrine.priorities.tags[0] == TargetTag::Bomber);
  h.Click(hud_id::FinishDamaged());
  REQUIRE(h.Group(kTaskForce2).shipDoctrine.priorities.finishDamaged);
  REQUIRE(h.LastOrder<battle::OrderSetShipDoctrine>().fields == battle::kShipPriorities);
}

void OrdersPickTheirTargets()
{
  Harness h(false);
  h.Key(DigitKey(1));
  const size_t before = h.CommandCount();
  h.Click(hud_id::Order(OrderButton::Attack));
  REQUIRE(h.hud->Picking() == PickMode::Attack);
  h.Click(h.OnScreen(kSalvation)); // friendly: not a valid attack target
  REQUIRE(h.hud->Picking() == PickMode::Attack && h.CommandCount() == before);
  REQUIRE(h.hud->Subject().group == kTaskForce1); // and the selection did not change

  h.Click(h.OnScreen(kAvenger), kModCtrl);
  REQUIRE(h.LastOrder<battle::OrderAttack>().targets == std::vector<UnitIndex>{kAvenger});
  REQUIRE(!h.Last().queue);
  REQUIRE(h.Last().target.groups == std::vector<battle::GroupId>{kTaskForce1});
  REQUIRE(h.hud->Picking() == PickMode::Attack);
  h.Click(h.OnScreen(kHarrow));
  REQUIRE(h.LastOrder<battle::OrderAttack>().targets == std::vector<UnitIndex>{kHarrow});
  REQUIRE(h.Last().queue);
  REQUIRE(h.hud->Picking() == PickMode::None);

  h.Click(hud_id::Order(OrderButton::Escort));
  h.Key(Key::Escape);
  REQUIRE(h.hud->Picking() == PickMode::None && !h.screen->QuitRequested());

  h.Click(hud_id::Order(OrderButton::Move));
  const Point plane = h.OnScreen(kSalvation) + Point{0.0f, 120.0f};
  h.Click(plane);
  const auto& move = h.LastOrder<battle::OrderMove>();
  const battle::Vec2 expected = h.screen->GetCamera().ScreenToWorld(plane);
  REQUIRE(std::abs(move.position.x - expected.x) < 1.0f && std::abs(move.position.y - expected.y) < 1.0f);

  h.Click(hud_id::Order(OrderButton::Hold));
  REQUIRE(std::holds_alternative<battle::OrderHold>(h.Last().order));

  // Commander signal with a target: fleet-wide, so no units are named.
  h.Click(hud_id::Signal(battle::SignalKind::ConcentrateFire));
  REQUIRE(h.hud->Picking() == PickMode::ConcentrateFire);
  h.Click(h.OnScreen(kHarrow));
  const auto& signal = h.LastOrder<battle::OrderSignal>();
  REQUIRE(signal.signal == battle::SignalKind::ConcentrateFire && signal.target == kHarrow);
  REQUIRE(h.Last().target.units.empty() && h.Last().target.groups.empty());
  h.Click(hud_id::Signal(battle::SignalKind::Scramble));
  REQUIRE(h.LastOrder<battle::OrderSignal>().signal == battle::SignalKind::Scramble);

  // Power routing is a live order for the card's subject.
  h.Click(hud_id::Power(battle::PowerRouting::Engines));
  REQUIRE(h.LastOrder<battle::OrderSetPower>().power == battle::PowerRouting::Engines);

  // Fleet withdrawal asks first.
  const size_t beforeWithdraw = h.CommandCount();
  h.Click(hud_id::WithdrawFleet());
  REQUIRE(h.hud->OpenPopup() == PopupKind::ConfirmWithdraw && h.CommandCount() == beforeWithdraw);
  h.Click(hud_id::ConfirmWithdraw());
  REQUIRE(std::holds_alternative<battle::OrderWithdraw>(h.Last().order));
  REQUIRE(h.Last().target.groups.size() == 6);

  h.Click(hud_id::Observe());
  REQUIRE(h.hud->Observing() && h.host.observing);
  h.Click(hud_id::Order(OrderButton::Hold));
  REQUIRE(!h.hud->Observing() && !h.host.observing);
  // A right-click order on the battle view takes command back too.
  h.Click(hud_id::Observe());
  REQUIRE(h.hud->Observing());
  h.Key(DigitKey(1));
  const Point open = h.OnScreen(kSalvation) + Point{0.0f, 160.0f};
  h.Move(open);
  h.screen->HandleInput(MouseButtonEvent{open, MouseButton::Right, true, 1, 0});
  h.screen->HandleInput(MouseButtonEvent{open, MouseButton::Right, false, 1, 0});
  h.Frame();
  REQUIRE(std::holds_alternative<battle::OrderMove>(h.Last().order));
  REQUIRE(!h.hud->Observing() && !h.host.observing);
  h.Click(hud_id::ImmediateResult());
  REQUIRE(h.host.immediate == 1);
}

void SuperlaserNeedsACapitalShipTarget()
{
  Harness h(false, Side::Empire, true);
  h.Click(hud_id::FireSuperlaser());
  REQUIRE(h.hud->Picking() == PickMode::Superlaser);
  const size_t before = h.CommandCount();
  h.Click(h.OnScreen(kRedSquadron));
  REQUIRE(h.CommandCount() == before && h.hud->Picking() == PickMode::Superlaser);
  h.Click(h.OnScreen(kLiberty));
  const auto& fire = h.LastOrder<battle::OrderFireSuperlaser>();
  REQUIRE(fire.target == kLiberty);
  REQUIRE(h.Last().target.units.size() == 1);
  REQUIRE(h.screen->View().superlaserCharge < 1.0f);

  // The Alliance's run needs more fighters in play than the Empire (32 TIEs against 28 here): offered, but disabled.
  Harness alliance(false, Side::Alliance, true);
  alliance.Key(DigitKey(4));
  REQUIRE(!alliance.screen->View().deathStarRunOpen);
  const size_t commands = alliance.CommandCount();
  alliance.Click(hud_id::Order(OrderButton::DeathStarRun));
  REQUIRE(alliance.CommandCount() == commands);
}

void RosterDragRegroups()
{
  Harness h;
  h.Drag(h.At(hud_id::RosterUnit(kTantive)), h.At(hud_id::RosterGroup(kTaskForce1)));
  REQUIRE(h.LastOrder<battle::OrderAssignGroup>().group == kTaskForce1);
  REQUIRE(h.Last().target.units == std::vector<UnitIndex>{kTantive});
  REQUIRE(FindUnit(h.screen->View(), kTantive)->group == kTaskForce1);
  REQUIRE(h.Group(kTaskForce1).members.size() == 4);

  // A ship never lands in a flight, and dropping on its own group does nothing.
  const size_t before = h.CommandCount();
  h.Drag(h.At(hud_id::RosterUnit(kTantive)), h.At(hud_id::RosterGroup(3)));
  h.Drag(h.At(hud_id::RosterUnit(kTantive)), h.At(hud_id::RosterUnit(kLiberty)));
  REQUIRE(h.CommandCount() == before);

  // The new-group slot appears while dragging.
  const Point grab = h.At(hud_id::RosterUnit(kSalvation));
  h.Move(grab);
  h.Button(grab, true);
  h.Frame();
  h.Move({grab.x, grab.y + 30.0f});
  h.Frame();
  h.Frame();
  const Point slot = h.At(hud_id::RosterNewGroup(false));
  for (int step = 1; step <= 4; ++step) {
    h.Move({grab.x, grab.y + 30.0f + (slot.y - grab.y - 30.0f) * static_cast<float>(step) / 4.0f});
    h.Frame();
  }
  h.Button(slot, false);
  h.Frame();
  h.Frame();
  REQUIRE(h.LastOrder<battle::OrderAssignGroup>().group == battle::kNoGroup);
  const battle::GroupId formed = FindUnit(h.screen->View(), kSalvation)->group;
  REQUIRE(formed > kTaskForce2 && h.Group(formed).members == std::vector<UnitIndex>{kSalvation});

  // Esc abandons a drag even though the button is still down, and a lost focus never drops.
  const size_t beforeAbandon = h.CommandCount();
  for (const bool escape : {true, false}) {
    const Point from = h.At(hud_id::RosterUnit(kTantive));
    const Point onto = h.At(hud_id::RosterGroup(kTaskForce2));
    h.Move(from);
    h.Button(from, true);
    h.Frame();
    h.Move({from.x, (from.y + onto.y) * 0.5f});
    h.Frame();
    if (escape) {
      h.Key(Key::Escape);
      REQUIRE(!h.screen->QuitRequested());
    } else {
      h.screen->HandleInput(FocusLost{});
    }
    h.Move(onto);
    h.Frame();
    h.Button(onto, false);
    h.Frame();
    h.Frame();
  }
  REQUIRE(h.CommandCount() == beforeAbandon);
  REQUIRE(FindUnit(h.screen->View(), kTantive)->group == kTaskForce1);

  // A plain click still selects the ship.
  h.Click(hud_id::RosterUnit(kLiberty));
  REQUIRE(h.screen->GetSelection().Units() == std::vector<UnitIndex>{kLiberty});
}

void BriefingDeploysAndEngages()
{
  Harness h;
  REQUIRE(h.hud->InBriefing() && h.screen->Paused());
  const auto briefingText = [&] {
    DrawList frame;
    h.screen->Build(frame);
    std::string lines;
    for (const DrawCommand& command : frame.Commands()) {
      if (const auto* text = std::get_if<DrawText>(&command)) {
        lines += text->text + " ";
      }
    }
    return lines;
  };
  REQUIRE(briefingText().find("Your Liberty projects a gravity well") == std::string::npos);
  h.source.friendlyProjector = true;
  h.Frame();
  const std::string warnings = briefingText();
  REQUIRE(warnings.find("Your Liberty projects a gravity well: enemy ships inside it can't jump to hyperspace.")
          != std::string::npos);
  REQUIRE(warnings.find("Enemy Hammerhead projects a gravity well: your ships inside it can't jump to hyperspace "
                        "until the projector is destroyed or disabled by ion fire.")
          != std::string::npos);
  h.source.hideEnemy = true;
  h.Frame();
  REQUIRE(!FindUnit(h.screen->View(), kAvenger));
  REQUIRE(FindUnit(h.screen->View(), kLiberty));
  for (const auto& craft : h.screen->View().craft) {
    REQUIRE(FindUnit(h.screen->View(), craft.squadron));
  }
  DrawList briefing;
  h.screen->Build(briefing);
  bool unknownComposition = false;
  for (const auto& command : briefing.Commands()) {
    if (const auto* text = std::get_if<DrawText>(&command)) {
      unknownComposition |= text->text.starts_with("No enemy composition");
    }
  }
  REQUIRE(unknownComposition);
  const battle::Vec2 before = FindUnit(h.screen->View(), kLiberty)->position;
  const Point handle = h.At(hud_id::DeployHandle(kTaskForce1));
  h.Drag(handle, {handle.x - 40.0f, handle.y + 30.0f});
  const auto& deploy = h.LastOrder<battle::OrderDeploy>();
  REQUIRE(h.Last().target.groups == std::vector<battle::GroupId>{kTaskForce1});
  const battle::Vec2 after = FindUnit(h.screen->View(), kLiberty)->position;
  REQUIRE(after.x < before.x - 100.0f && after.y < before.y - 100.0f);
  (void)deploy;
  h.Click(hud_id::Engage());
  REQUIRE(!h.hud->InBriefing() && !h.screen->Paused());
  h.screen->Update(0.05);
  REQUIRE(FindUnit(h.screen->View(), kAvenger));
}

std::string DrawnText(Harness& h)
{
  DrawList frame;
  h.screen->Build(frame);
  std::string lines;
  for (const DrawCommand& command : frame.Commands()) {
    if (const auto* text = std::get_if<DrawText>(&command)) {
      lines += text->text + " ";
    }
  }
  return lines;
}

// Planetary batteries (§6.13) explain themselves in the briefing, label their ring, and report what they are firing at;
// ones the player has no intelligence on stay hidden until they open fire.
void PlanetaryDefensesExplainThemselves()
{
  Harness h;
  h.source.enemyBatteries = true;
  h.source.batteriesKnown = false;
  h.Frame();
  std::string text = DrawnText(h);
  REQUIRE(text.find("planetary") == std::string::npos && text.find("PLANETARY") == std::string::npos);
  h.source.batteriesKnown = true;
  h.Frame();
  text = DrawnText(h);
  REQUIRE(text.find("The enemy's 1 LNR Series II turbolaser battery and 1 KDY-150 ion cannon fire on your capital "
                    "ships inside the ring around the planet: ion fire drains shields, then knocks out engines.")
          != std::string::npos);
  REQUIRE(text.find("They cannot be destroyed in this battle") != std::string::npos);
  h.Click(hud_id::Engage());
  text = DrawnText(h);
  REQUIRE(text.find("ENEMY PLANETARY DEFENSES") != std::string::npos);
  REQUIRE(text.find("no target in range") != std::string::npos);
  REQUIRE(h.hud->ControlRect(hud_id::Battery(0)) && h.hud->ControlRect(hud_id::Battery(1)));
  h.source.batteriesKnown = false;
  h.source.batteriesFiring = true; // a battery that fires is revealed
  h.Frame();
  text = DrawnText(h);
  REQUIRE(text.find("firing at " + h.screen->Label(kLiberty).name) != std::string::npos);
  REQUIRE(text.find("ENEMY PLANETARY DEFENSES") != std::string::npos);
}

void SettingsAndPresetsPersist()
{
  MemorySettingsStore store;
  HudSettings settings;
  settings.autoPause[static_cast<int>(AutoPauseEvent::CarrierLost)] = false;
  settings.commandPhaseSeconds = 20;
  settings.colorBlindPalette = true;
  settings.hudScale = 1.3f;
  DoctrinePreset preset{"Wall; of = steel", false};
  preset.ship.range = battle::EngagementRange::StandOff;
  preset.ship.priorities = SetPriority(SetPriority({}, 0, TargetTag::Heavy), 1, TargetTag::Carrier);
  preset.ship.priorities.finishDamaged = true;
  preset.fields = PresetFields(false);
  settings.presets = {preset};
  settings.Save(store);
  const HudSettings loaded = HudSettings::Load(store);
  REQUIRE(!loaded.AutoPause(AutoPauseEvent::CarrierLost) && loaded.AutoPause(AutoPauseEvent::GravityWell));
  REQUIRE(loaded.commandPhaseSeconds == 20 && loaded.colorBlindPalette && loaded.hudScale == 1.3f);
  REQUIRE(loaded.presets.size() == 1);
  REQUIRE(SerializePreset(loaded.presets.front()) == SerializePreset(preset));
  REQUIRE(loaded.presets.front().name == "Wall; of = steel");
  REQUIRE(!ParsePreset("flight=0;options=9,9;name=x"));
  REQUIRE(!ParsePreset("garbage"));
  for (const DoctrinePreset& builtIn : BuiltInPresets()) {
    const std::optional<DoctrinePreset> round = ParsePreset(SerializePreset(builtIn));
    REQUIRE(round && SerializePreset(*round) == SerializePreset(builtIn));
  }
  settings.presets.clear();
  settings.Save(store);
  REQUIRE(!store.Get("battle.hud.preset.0"));

  // Applying a preset sends only its fields; saving one stores it in the settings.
  Harness h;
  h.Key(DigitKey(1));
  h.Click(hud_id::Presets());
  REQUIRE(h.hud->OpenPopup() == PopupKind::Presets);
  h.Click(hud_id::PresetRow(0));
  const auto& applied = h.LastOrder<battle::OrderSetShipDoctrine>();
  REQUIRE(applied.fields == BuiltInPresets().front().fields);
  REQUIRE(h.Group(kTaskForce1).shipDoctrine.allocation == battle::FireAllocation::Concentrate);
  h.Click(hud_id::Presets());
  h.Click(hud_id::SavePreset());
  h.screen->HandleInput(TextInput{"Mine"});
  h.Key(Key::W); // typing must not pan the camera or trigger shortcuts
  h.Key(Key::Return);
  REQUIRE(h.hud->Settings().presets.size() == 1 && h.hud->Settings().presets.front().name == "Mine");
  REQUIRE(HudSettings::Load(h.store).presets.size() == 1);
}

void AutoPauseWatchesTheView()
{
  battle::ViewModel view;
  view.tick = 10;
  battle::ViewUnit carrier;
  carrier.unit = 0;
  carrier.side = Side::Alliance;
  carrier.group = 0;
  battle::ViewUnit cruiser = carrier;
  cruiser.unit = 1;
  view.units = {carrier, cruiser};
  battle::ViewGroup group;
  group.id = 0;
  group.side = Side::Alliance;
  group.members = {0, 1};
  group.shipDoctrine.withdraw = battle::WithdrawRule::HullBelow50;
  view.groups = {group};
  view.superlaserCharge = 0.5f;
  UnitFacts carrierFacts{"Bright Hope", true, false};
  UnitFacts otherFacts{"Other", false, false};
  const UnitFactsLookup facts = [&](UnitIndex unit) -> const UnitFacts& {
    return unit == 0 ? carrierFacts : otherFacts;
  };
  // Unit 1 keeps its group's rule (hull below 50%); an override to Never on unit 3 must not pause.
  const WithdrawRuleLookup rules = [](UnitIndex unit) {
    return unit == 3 ? battle::WithdrawRule::Never : battle::WithdrawRule::HullBelow50;
  };
  battle::ViewUnit stubborn = cruiser;
  stubborn.unit = 3;
  view.units.push_back(stubborn);
  AutoPauseWatcher watcher;
  REQUIRE(watcher.Update(view, Side::Alliance, facts, rules).empty());
  view.units[1].hullFraction = 0.4f;
  view.units[2].hullFraction = 0.4f;
  view.units[0].flags |= battle::kViewDestroyed;
  battle::ViewUnit enemy = carrier;
  enemy.unit = 2;
  enemy.side = Side::Empire;
  view.units.push_back(enemy);
  view.wells.push_back({2, {}, 15000.0f, Side::Empire, true});
  view.superlaserCharge = 1.0f;
  const std::vector<AutoPauseHit> hits = watcher.Update(view, Side::Alliance, facts, rules);
  const auto has = [&](AutoPauseEvent event) {
    return std::ranges::any_of(hits, [&](const AutoPauseHit& hit) {
      return hit.event == event;
    });
  };
  REQUIRE(hits.size() == 5);
  REQUIRE(has(AutoPauseEvent::CarrierLost) && has(AutoPauseEvent::BelowWithdrawThreshold)
          && has(AutoPauseEvent::Reinforcements) && has(AutoPauseEvent::GravityWell)
          && has(AutoPauseEvent::SuperlaserCharged));
  REQUIRE(watcher.Update(view, Side::Alliance, facts, rules).empty());

  REQUIRE(!watcher.PhaseDue(3.0f, 10));
  REQUIRE(!watcher.PhaseDue(9.9f, 10));
  REQUIRE(watcher.PhaseDue(10.0f, 10));
  REQUIRE(!watcher.PhaseDue(10.05f, 10));
  REQUIRE(watcher.PhaseDue(20.1f, 10));

  // In the HUD, a command phase pauses the running battle.
  Harness h(false);
  HudSettings phases = h.hud->Settings();
  phases.commandPhaseSeconds = 10;
  h.hud->ApplySettings(phases);
  for (int frame = 0; frame < 12 * 30 && !h.screen->Paused(); ++frame) {
    DrawList list;
    h.screen->Update(1.0 / 30.0);
    h.screen->Build(list);
  }
  REQUIRE(h.screen->Paused());
  REQUIRE(std::abs(h.screen->View().battleSeconds - 10.0f) < 0.2f);
}

} // namespace

int main(int argc, char** argv)
{
  const std::string_view only = argc > 1 ? argv[1] : "";
  const auto run = [&](std::string_view name, void (*test)()) {
    if (only.empty() || only == name) {
      test();
      std::printf("ok %.*s\n", static_cast<int>(name.size()), name.data());
    }
  };
  run("doctrine", GroupDoctrineEditSetsOnlyThatField);
  run("overrides", ShipEditsAreOverridesAndCanBeCleared);
  run("folding", SmallScreensFoldTheLargestGroups);
  run("priorities", PriorityEditsReorderAndMask);
  run("targeting", OrdersPickTheirTargets);
  run("superlaser", SuperlaserNeedsACapitalShipTarget);
  run("roster", RosterDragRegroups);
  run("briefing", BriefingDeploysAndEngages);
  run("planetary", PlanetaryDefensesExplainThemselves);
  run("settings", SettingsAndPresetsPersist);
  run("autopause", AutoPauseWatchesTheView);
  return 0;
}
