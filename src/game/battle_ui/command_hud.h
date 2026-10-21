#pragma once
#include <array>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "battle_ui/auto_pause.h"
#include "battle_ui/battle_screen.h"
#include "battle_ui/doctrine_edit.h"
#include "battle_ui/doctrine_text.h"
#include "battle_ui/hud_icons.h"
#include "battle_ui/hud_settings.h"

// The player's command surface on the battle screen (tactical_combat.md §4, §5, §10.2): the roster of task forces and
// flights, the doctrine card with its target-priority editor and presets, the orders bar, commander signals, the Death
// Star controls, the event log, the briefing and deployment phase, pacing and accessibility options. Every action
// becomes a battle::Command submitted through the screen; the HUD never touches the simulation itself.
namespace reb::battle_ui
{

// What the HUD asks of whoever runs the battle: the campaign integration in the game, a stub in the demo.
class BattleHost
{
  public:
  virtual ~BattleHost() = default;
  // Stop commanding: the simulator finishes the battle headless from its current state, with the AI commanding both
  // sides (§5.4, §11.1).
  virtual void GetImmediateResult() = 0;
  // Hands the player's side to the battle AI, starting from the player's doctrines, or takes it back (§9).
  virtual void SetObserving(bool observing) = 0;
};

// Orders on the orders bar (§5.4).
enum class OrderButton : uint8_t {
  Attack,
  Move,
  Hold,
  Escort,
  Tractor,
  Launch,
  Recall,
  Strike,
  Recover,
  Withdraw,
  DeathStarRun,
};

// An order waiting for its target to be clicked on the battle view.
enum class PickMode : uint8_t {
  None,
  Attack,
  Move,
  Escort,
  Tractor,
  Strike,
  ConcentrateFire,
  Superlaser,
  Protect, // the ship a Screen group or an Escort flight protects (a doctrine field)
};

enum class PopupKind : uint8_t { None, Field, Priorities, Presets, Options, ConfirmWithdraw };

// Widget ids of the HUD's controls, for tests and review scripts (ControlRect).
namespace hud_id
{
WidgetId Chip(DoctrineField field);
WidgetId Order(OrderButton order);
WidgetId Power(battle::PowerRouting power);
WidgetId Signal(battle::SignalKind signal);
WidgetId FieldOption(int index);
WidgetId FollowGroup();
WidgetId PrioritySlot(int slot);
WidgetId PriorityRemove(int slot);
WidgetId PriorityTag(battle::TargetTag tag);
WidgetId FinishDamaged();
WidgetId RosterGroup(battle::GroupId group);
WidgetId RosterUnit(battle::UnitIndex unit);
WidgetId RosterNewGroup(bool flight);
WidgetId DeployHandle(battle::GroupId group);
WidgetId Engage();
WidgetId Presets();
WidgetId PresetRow(int index);
WidgetId SavePreset();
WidgetId Options();
WidgetId AutoPause(AutoPauseEvent event);
WidgetId CommandPhase(int seconds);
WidgetId Palette(bool colorBlind);
WidgetId HudScale(int index);
WidgetId WithdrawFleet();
WidgetId ConfirmWithdraw();
WidgetId Observe();
WidgetId ImmediateResult();
WidgetId FireSuperlaser();
WidgetId ClearOverrides();
WidgetId EditGroup();
WidgetId EventRow(int index);
WidgetId Battery(int index); // a planetary battery's row in the command panel
WidgetId CollapseRoster();
WidgetId CollapseCommand();
WidgetId CollapseEvents();
WidgetId CollapseCard();
} // namespace hud_id

class CommandHud : public HudPanel, public ScreenInputHook
{
  public:
  CommandHud(BattleScreen& screen, BattleHost& host, SettingsStore& store);

  void Draw(Ui& ui, BattleScreen& screen) override;
  std::string SaveUiState() const override;
  bool RestoreUiState(std::string_view state) override;
  bool ViewClick(battle::UnitIndex unit, battle::Vec2 world, uint8_t modifiers) override;
  bool Cancel() override;
  bool Keyboard(const InputEvent& event) override;
  void Submitted(const battle::Command& command) override;

  // ---- state and actions (the panels use these; so do tests) ----
  const DoctrineSubject& Subject() const
  {
    return m_subject;
  }
  PickMode Picking() const
  {
    return m_pick.mode;
  }
  PopupKind OpenPopup() const
  {
    return m_popup.kind;
  }
  bool InBriefing() const
  {
    return m_briefing;
  }
  bool Observing() const
  {
    return m_observing;
  }
  const HudSettings& Settings() const
  {
    return m_settings;
  }
  void ApplySettings(HudSettings settings);
  void Engage();
  void SetObserving(bool observing);
  void StartPick(PickMode mode);
  // Selects the group's active members; the card then edits the group's doctrine.
  void FocusGroup(battle::GroupId group);
  // Folds a group in the roster to its header line, or unfolds it.
  void FoldGroup(battle::GroupId group, bool folded);
  bool Submit(battle::Command command);
  // Where a control was drawn in the last frame.
  std::optional<Rect> ControlRect(WidgetId id) const;
  const std::vector<std::string>& Toasts() const
  {
    return m_toastTexts;
  }

  private:
  struct Pick {
    PickMode mode = PickMode::None;
    battle::CommandTarget who;
    int picked = 0;
    DoctrineSubject subject; // Protect: whose doctrine names the protected ship
  };
  struct Popup {
    PopupKind kind = PopupKind::None;
    DoctrineField field = DoctrineField::Role;
    DoctrineSubject subject; // a doctrine popup edits the subject it was opened for, and closes when that changes
    Rect anchor;
    Rect rect; // as drawn last frame
  };
  struct Toast {
    std::string text;
    double time = 0.0;
  };
  // The doctrine shown on the card: the subject's, with which fields are overridden or differ between units.
  struct CardDoctrine {
    battle::ShipDoctrine ship;
    battle::FlightDoctrine flight;
    battle::DoctrineOverrideMask overrides = 0;
    battle::DoctrineOverrideMask mixed = 0;
    // The group's doctrine, for "follow group".
    battle::ShipDoctrine groupShip;
    battle::FlightDoctrine groupFlight;
    bool hasGroup = false;
  };
  struct RosterRow {
    enum class Kind : uint8_t { Section, Group, Unit, NewGroup } kind = Kind::Section;
    battle::GroupId group = battle::kNoGroup;
    battle::UnitIndex unit = battle::kNoUnit;
    bool flight = false;
    float height = 0.0f;
  };

  // per frame
  void UpdateState(BattleScreen& screen);
  void UpdateFocus();
  void UpdateHotkeys();
  void CheckAutoPause();
  CardDoctrine ReadCardDoctrine() const;
  const UnitFacts& Facts(battle::UnitIndex unit) const;
  void Record(WidgetId id, const Rect& rect);
  void AddToast(std::string text);
  battle::Side Enemy() const;
  std::string GroupTitle(const battle::ViewGroup& group) const;
  std::optional<int> HotkeyFor(battle::GroupId group) const;

  // drawing (each in its own file)
  void DrawRoster(Ui& ui);
  void DrawRosterRow(Ui& ui, const RosterRow& row, const Rect& rect, bool hovered, bool dropTarget);
  void FinishRosterDrag(const RosterRow* dropRow);
  void DrawDragGhost(Ui& ui);
  void DrawRightColumn(Ui& ui);
  float DrawCommandPanel(Ui& ui, const Rect& column);
  void DrawEvents(Ui& ui, const Rect& rect);
  void DrawBriefing(Ui& ui, const Rect& column);
  void DrawDeployment(Ui& ui);
  void DrawCard(Ui& ui);
  void DrawCardHeader(Ui& ui, const Rect& row, const CardDoctrine& doctrine);
  void DrawChips(Ui& ui, const Rect& row, const CardDoctrine& doctrine);
  void DrawOrders(Ui& ui, const Rect& row, const CardDoctrine& doctrine);
  void DrawIntent(Ui& ui, const Rect& row);
  void DrawPopup(Ui& ui);
  void DrawFieldPopup(Ui& ui, const CardDoctrine& doctrine);
  void DrawPriorityPopup(Ui& ui, const CardDoctrine& doctrine);
  void DrawPresetPopup(Ui& ui, const CardDoctrine& doctrine);
  void DrawOptionsPopup(Ui& ui);
  void DrawConfirmWithdraw(Ui& ui);
  void DrawPickOverlay(Ui& ui);
  void DrawObserving(Ui& ui);
  void DrawToasts(Ui& ui);
  Rect PlacePopup(Ui& ui, float width, float height) const;

  // actions
  void PressOrder(OrderButton order);
  void SubmitDoctrine(const CardDoctrine& doctrine, battle::DoctrineOverrideMask fields);
  void SubmitPriorities(const CardDoctrine& doctrine, const battle::TargetPriorities& priorities);
  void ClearOverrides(battle::DoctrineOverrideMask fields);
  void WithdrawFleet();
  void OpenPopupAt(PopupKind kind, const Rect& anchor, DoctrineField field = DoctrineField::Role);
  void ClosePopup();
  bool PickValid(battle::UnitIndex unit) const;
  std::vector<battle::UnitIndex> SubjectUnits() const;
  bool SubjectHas(bool (*test)(const UnitFacts&)) const;

  BattleScreen& m_screen;
  BattleHost& m_host;
  SettingsStore& m_store;
  HudSettings m_settings;
  Ui* m_ui = nullptr;

  DoctrineSubject m_subject;
  battle::GroupId m_focus = battle::kNoGroup;
  std::vector<battle::UnitIndex> m_focusSelection;
  std::optional<battle::GroupId> m_pendingFocus;
  std::array<battle::GroupId, HotkeyGroups::kCount> m_hotkeyGroup{};
  std::array<std::vector<battle::UnitIndex>, HotkeyGroups::kCount> m_hotkeyWritten;
  bool m_hotkeysBound = false;
  bool m_autoFolded = false; // folded the larger groups once, when the roster first overflowed

  Pick m_pick;
  Popup m_popup;
  int m_prioritySlot = -1; // selected slot in the priority editor
  int m_priorityDrag = -1; // slot being dragged
  bool m_naming = false;   // typing a preset name
  std::string m_presetName;

  // roster drag-to-regroup
  bool m_rosterDragging = false;
  std::vector<battle::UnitIndex> m_dragUnits;
  bool m_dragFlight = false;
  bool m_pressAbandoned = false; // Esc ended a drag while the button is still held: no new drag until it is up
  std::set<battle::GroupId> m_collapsedGroups;
  // briefing: dragging a group's handle
  battle::GroupId m_deployDrag = battle::kNoGroup;

  bool m_briefing = false;
  bool m_observing = false;
  bool m_rosterCollapsed = false;
  bool m_commandCollapsed = false;
  bool m_eventsCollapsed = false;
  bool m_cardCollapsed = false;

  AutoPauseWatcher m_watcher;
  mutable std::vector<std::optional<UnitFacts>> m_facts; // filled on first use
  std::vector<Toast> m_toasts;
  std::vector<std::string> m_toastTexts;
  std::unordered_map<WidgetId, Rect> m_rects;
  std::unordered_map<WidgetId, Rect> m_lastRects;
};

// Creates the HUD, adds it to the screen as a panel and its input hook, and applies the stored settings. The screen
// owns it; `host` and `store` must outlive the screen.
CommandHud& InstallCommandHud(BattleScreen& screen, BattleHost& host, SettingsStore& store);

// The theme for the settings' palette.
Theme HudTheme(bool colorBlind);

} // namespace reb::battle_ui
