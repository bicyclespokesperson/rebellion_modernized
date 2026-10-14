#pragma once
#include <cstdint>
#include <set>
#include <string>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "strategy_ui/panel_drag_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_timer.h"
#include "ui/game_window_object.h"

class CoolGameWindow;
class CoolListItem;
class CoolStrobeButton;
class CoolFrameButton;
class DibBitmap;
class ObjectListItem;
class SpriteStage;
class istream;
class ostream;
class GameObjNotifier;
class TextMessage;
class NullableKey;

// Child-window ids of the galaxy view's controls and the WM_COMMAND ids they send.
enum GalaxyCommand : uint32_t
{
  kGalaxySystemFinder = 0x12d, // finder buttons (also F2..F5)
  kGalaxyFleetFinder = 0x12e,
  kGalaxyPersonnelFinder = 0x12f,
  kGalaxyTroopFinder = 0x130,
  kGalaxyEncyclopedia = 0x131, // also F7
  kGalaxyGidButton = 0x132, // opens the Galactic Information Display menu
  kGalaxyGameControls = 0x133, // the options monitor (also F1); sets m_optionsRequested
  kGalaxyRestoreGidLegend = 0x134, // the minimised GID legend
  kGalaxyMessageButtonFirst = 0x136, // the nine message-category buttons 0x136..0x13e
  kGalaxyMessageButtonLast = 0x13e,
  kGalaxySetDisplayMode = 0x140, // HIWORD(wParam) is the GID display mode
  kGalaxyRawMaterials = 0x14c, // the three resource counters in the top bar (labels, not windows)
  kGalaxyRefinedMaterials = 0x14d,
  kGalaxyMaintenance = 0x14e,
  kGalaxyHelp = 0x154,
  kGalaxyCancelTargeting = 0x15e,
  kGalaxyDevSwitchSides = 0xf001,
};

// Custom messages of the galaxy view.
enum GalaxyMessage : UINT
{
  kGalaxyMovieDone = 0x402, // the movie m_pendingMovie stands for has finished
  kGalaxyCloseChild = 0x405, // wParam: child id (kCloseChildMessage)
  kGalaxyClosePopups = 0x409,
  kGalaxyActivateChild = 0x467, // wParam / lParam: the child window to bring to the front
  kGalaxyOpenModal = 0x468, // wParam: window type (low 6 bits), lParam: argument
  kGalaxyPlayMovie = 0x482, // wParam 1: lParam is a movie record (see SetPendingMovie)
  kGalaxyAccelerator = 0x483, // LOWORD(wParam): accelerator command 0xbb9..0xbd7
};

// One slot of the bar of minimised windows: the window's icon, drawn at m_area.
struct MinimizedWindowSlot
{
  MinimizedWindowSlot(); // 0x42dbe0 (linker-folded "zero the first dword")

  DibBitmap* m_icon; // +0x00 owned, the area's background with the icon drawn over it
  uint32_t m_iconId; // +0x04 bitmap id of the icon m_icon shows, 0 if none
  RECT m_area; // +0x08 fixed per side (constructor)
  uint32_t m_childId; // +0x18 the minimised window, 0 if the slot is free
};

// Stand-in (no creator found: the galaxy view never sets m_tickTimer, so this stays null in practice): a CoolTimer
// whose tick the view resumes when shown and suspends when hidden.
class GalaxyTickTimer : public CoolTimer
{
public:
  int m_suspended; // +0x38
};

// Window state of a strategy child window as saved with the galaxy view (VT 0x65bcb4; ui_strategic 0x49ecb0+,
// declared here because only the galaxy view's save/load uses it).
class ChildWindowState
{
public:
  ChildWindowState(); // 0x49ecb0
  explicit ChildWindowState(CoolWindow* window); // 0x49ecf0: type, position, size and shown flag of `window`
  virtual ~ChildWindowState(); // 0x49ed50
  void Write(ostream* out); // 0x49ed60
  void Read(istream* in); // 0x49edc0

  uint32_t m_type; // +0x04 low 6 bits of the child id
  int m_x; // +0x08
  int m_y; // +0x0c
  int m_width; // +0x10
  int m_height; // +0x14
  int m_shown; // +0x18 CoolGameWindow +0x110
  int m_valid; // +0x1c
};

// The galaxy view: the main window of the strategic game (child 7 or 8 of the application window, by side). It
// draws the galaxy map, the top bar (date, speed, resource counters), the console buttons (finders,
// encyclopedia, GID, message categories, options), the advisor droids, the Galactic Information Display overlay
// and the bar of minimised windows, and it owns every strategy child window (sector/system/fleet windows, finders,
// dialogs) in m_children. Child windows are opened through the routes in 0x429320-0x42dc10 and the modal ones
// through kGalaxyOpenModal, whose handler runs a nested message loop that keeps stepping the game until the
// modal popup closes.
class GalaxyWindow : public GameWindowObject
{
public:
  // `restoring` is set when the view is built for a loaded game.
  GalaxyWindow(HINSTANCE instance,
               int x,
               int y,
               int width,
               int height,
               CoolWindow* parent,
               int side,
               int restoring); // 0x421c70
  ~GalaxyWindow() override; // 0x422270 (scalar deleting destructor 0x422250)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x422ce0
  // Issues command `a` with no selection (game_ui_facade 0x41cdf0).
  void OnMenuCommand(int a, int b, int c) override; // slot 8, 0x4414a0
  // Advances the droid animations (the idle slot).
  void TickIdleAnimations() override; // slot 11, 0x42d020
  // Saves the display modes and the state of every child window.
  virtual void SaveState(ostream* out); // slot 15, 0x440ea0
  // Restores what SaveState wrote and re-opens the child windows.
  virtual void LoadState(istream* in); // slot 16, 0x440f80
  // Tells every open child window to refresh (their slot 35).
  virtual void RefreshChildWindows(); // slot 17, 0x42cfe0

  // ---- strategy_window_core (0x421c70-0x429320) ----
  // Rebuilds the top bar counters and the encyclopedia list (m_encyclopediaItems) for a new or loaded game.
  void InitializeGameDisplay(); // 0x422620
  // Starts the in-game music once (m_musicPending).
  void StartMusic(); // 0x422cb0
  // Sets m_alternateDisplayMode from the GID flags of an event.
  void SetAlternateDisplayMode(uint32_t flags); // 0x425c90
  // Remembers the movie record and posts kGalaxyPlayMovie for it.
  void SetPendingMovie(LPARAM movie); // 0x425ce0
  // Switches the Galactic Information Display to `mode` (0x80: off); `selection` (modes 0x92/0x93 only; the ids the
  // display was showing) becomes the selected ids.
  void SetDisplayMode(int mode, const std::set<uint32_t>* selection, bool resizing = false); // 0x425d00
  // Opens (or, for mode 0x80, hides) the GID legend panel for `mode`.
  void ShowDisplayLegend(int mode); // 0x426d00
  // Icon id for a value against thresholds: m_gidIconIds[kind - 1][...].
  int GidIconId(int value, int kind, int high, int midHigh, int midLow, int lowHigh, int lowLow); // 0x426e70
  void LoadGidIcons(HMODULE module); // 0x426ee0
  // Builds the background (galaxy map + frame, with or without the GID tint); returns the map picture.
  DibBitmap* BuildBackground(int gidActive); // 0x427010
  void CreateConsoleButtons(HMODULE module); // 0x427270
  void CreateConsoleButtonWindows(HMODULE stringModule); // 0x4286b0
  // Minimises `window` into the bar; returns the slot number (1-based).
  int MinimizeChildWindow(CoolWindow* window); // 0x428b40
  // Redraws the icon of `window` in its slot of the bar.
  void UpdateMinimizedIcon(CoolWindow* window); // 0x428d70
  // Restores the window of slot `slot` (1-based).
  void RestoreMinimizedWindow(int slot); // 0x429020
  // The slot (1-based) child `childId` is minimised in, 0 if none.
  int FindMinimizedSlot(uint32_t childId); // 0x4291d0
  // Shows `value` in resource counter `counter` (kGalaxyRawMaterials...); `unused` is ignored.
  void SetResourceCounter(int counter, int value, int unused); // 0x429200

  // ---- window message handlers (parts of WindowProc, 0x422ce0) ----
  LRESULT OnCreate(); // WM_CREATE
  LRESULT OnPaint(); // WM_PAINT
  void ResizeCanvas(int width, int height);
  LRESULT OnKeyDown(WPARAM key, LPARAM lParam); // WM_KEYDOWN: F1-F7 shortcuts
  LRESULT OnShowWindow(WPARAM shown, LPARAM lParam); // WM_SHOWWINDOW
  LRESULT OnMouseMove(WPARAM keys, LPARAM position); // WM_MOUSEMOVE: sector hover
  LRESULT OnCommandMessage(WPARAM wParam, LPARAM lParam); // WM_COMMAND: console buttons and child notifications
  void OnChildDropped(CoolWindow* source, LPARAM cursor); // a child's selection was dropped at the cursor
  LRESULT OnMovieDone(); // kGalaxyMovieDone
  LRESULT OnLeftButtonUp(WPARAM keys, LPARAM position); // WM_LBUTTONUP
  void CompleteTargeting(WPARAM keys, LPARAM position); // ... in targeting mode
  LRESULT OnLeftDoubleClick(WPARAM keys, LPARAM position); // WM_LBUTTONDBLCLK
  LRESULT OnRightButtonUp(WPARAM keys, LPARAM position); // WM_RBUTTONUP
  void OnCloseChild(WPARAM childId); // kGalaxyCloseChild
  void OnActivateChild(WPARAM window); // kGalaxyActivateChild
  LRESULT OnModalRequest(UINT message, WPARAM wParam, LPARAM lParam); // kGalaxyOpenModal / kGalaxyPlayMovie
  CoolWindow* CreateModalWindow(WPARAM wParam, LPARAM lParam);
  void OnAccelerator(WPARAM command); // kGalaxyAccelerator
  void ToggleGidMenu(); // the GID button
  // Accelerators 0xbd3 / 0xbd4: brings the next (previous) open non-sector window of m_openWindowIds after `current`
  // to the front.
  void ActivateNextOpenWindow(uint32_t currentId, bool backwards);

  // ---- strategy_window_ext (0x440ea0-0x441a60) ----
  HWND LastChildWindow(); // 0x440f20: the bottom of the z-order
  CoolWindow* FindChildByHwnd(HWND hwnd); // 0x440f50
  static void SaveChildWindow(ostream* out, CoolWindow* window); // 0x4410a0
  CoolWindow* LoadChildWindow(istream* in); // 0x441120
  // Creates the child window `state` describes (sector/system/fleet... windows).
  CoolWindow* OpenChildWindow(istream* in, ChildWindowState* state); // 0x441190
  // Builds the sector hot spots (m_sectorItems) of the galaxy map.
  void BuildSectorItems(HMODULE module); // 0x441520

  // ---- strategy_window_routes (0x429320-0x42dc10) ----
  // Starts picking a target for `command` (mode 2, the target cursor, mouse captured).
  void BeginTargeting(void* command); // 0x429320
  // Starts renaming the first object of `request` in the window (system window or the active child) showing it.
  void BeginRename(TextMessage* request); // 0x429350
  // Opens the sector or system window that shows `object` and selects it; 1 if a window was found.
  int OpenWindowForObject(const ObjectKey* object); // 0x429440
  // Opens (or returns the open) sector window of the sector `sector`: at most two stand side by side.
  CoolWindow* OpenSectorWindow(const ObjectKey* sector); // 0x429ce0
  void OpenEncyclopedia(const ObjectKey* object, const NullableKey* type); // 0x429f30
  void OpenSystemFinder(); // 0x42a000
  void OpenFleetFinder(); // 0x42a0c0
  void OpenPersonnelFinder(); // 0x42a180
  void OpenMessageIndex(int mode, int tab); // 0x42a240
  // Closes the popups, shows the message index (child 0xd) tab refresh of the window's tab control (0x469d20).
  void RefreshMessageIndex(); // 0x42a300
  // Asks for the characters that can take the mission command; opens the selection window (type 0xc) if there are some.
  void IssueTargetedCommand(void* command); // 0x42a320
  void OpenTroopFinder(); // 0x42a4d0
  // Posts kGalaxyOpenModal for window type 0x1c / 0x1a (object status of `key`) / 0x10 (order confirmation) /
  // 6 (after leaving targeting mode); every one closes the popups first.
  void OpenType1cWindow(LPARAM argument); // 0x42a410
  void OpenObjectStatus(const ObjectKey* key); // 0x42a440
  void OpenOrderConfirm(LPARAM argument); // 0x42a590
  void OpenType6Window(LPARAM argument); // 0x42a5c0
  // Opens the type 8 window (replacing a type 7 one) and focuses it.
  void OpenType8Window(LPARAM argument); // 0x42a640
  void ForwardToType6Window(int a, int b); // 0x42a700
  // The next four update the panel of the manufacturing manager's slot in the system window of its system.
  void OnManagerCharacterChanged(const ObjectKey* manager, const ObjectKey* character); // 0x42a730
  void OnManagerStatusTextChanged(const ObjectKey* manager, std::string* text); // 0x42a7a0
  void OnManagerProductionChanged(const ObjectKey* manager, int unused); // 0x42a810
  void OnManagerProgressChanged(const ObjectKey* manager, int position); // 0x42a890
  // Tell every open window that shows the parent of `object` (key `key`) about a change.
  void BroadcastObjectChanged(const ObjectKey* key, GameObjNotifier* object); // 0x42a900
  void BroadcastObjectMoved(const ObjectKey* key,
                            GameObjNotifier* object,
                            const ObjectKey* otherKey,
                            GameObjNotifier* otherObject); // 0x42aaf0
  void BroadcastObjectUpdated(const ObjectKey* key, GameObjNotifier* object); // 0x42abd0
  int TrackChildWindow(CoolWindow* window); // 0x42ac70: adds its id to m_openWindowIds
  // The system `system` changed (kind 1 or 2): updates its marker and its sector window.
  void OnSystemChanged(const ObjectKey* system, int kind, int a, int b); // 0x42ad10
  void CreateDroidAnimations(); // 0x42adb0
  void PlayDroidScript(uint32_t resourceId, uint32_t x, uint32_t y, int alternateSet); // 0x42b1d0
  void PlaySecondDroidScript(uint32_t resourceId, uint32_t x, uint32_t y); // 0x42b290
  // Brings the GID marker of the system `key` (family 0x90) up to date; with `redraw` also its sector's markers.
  void UpdateSystemMarker(const ObjectKey* key, GameObjNotifier* object, int redraw); // 0x42b330
  void RedrawSectorMarkers(GameObjNotifier* sector); // 0x42cde0
  void ShowSpeedMenu(int x, int y); // 0x42d190
  void ShowDeveloperMenu();
  // Shows day `day` (0: unchanged) and speed `speed` (redrawn if changed or `force`).
  void SetDateAndSpeed(int day, int speed, int force); // 0x42d230
  void EnableControls(); // 0x42d4d0
  void DisableControls(); // 0x42d570
  // Stops the scripts of both droid animations.
  void StopDroidScripts(); // 0x42d620
  // Shows the object menu of kind `kind` (0xdead: the object commands, 0xbeef: the order commands) at (x, y).
  void ShowObjectMenu(int x, int y, uint32_t kind); // 0x42d050
  void ShowLatency(); // 0x42d780
  int MapOffsetX() const; // horizontal offset of the original 640-pixel galaxy scene on a wider canvas
  int SectorSlotX(bool right) const;
  // The GID marker image of the system `system`, null if it has no marker.
  DibBitmap* GetMarkerImage(GameObjNotifier* system); // 0x42d860
  void ClearButtonHighlights(uint32_t mask); // 0x42d8d0
  void SetButtonHighlights(uint32_t mask); // 0x42da10
  void ResumePendingRequest(); // 0x42db70
  int IsShownAndIdle(); // 0x42d490

  // ---- data ----
  int m_side; // +0x9c 1 = Alliance, 2 = Empire (the object view of the player)
  int m_canvasLayoutWidth; // last width applied to the widescreen galaxy layout
  int m_optionsRequested; // +0xa0 set by kGalaxyGameControls / F1, cleared by the input hooks
  int m_inModalLoop; // +0xa4 1 while kGalaxyOpenModal pumps messages
  int m_alternateDisplayMode; // +0xa8 GID mode saved with the game next to m_displayMode
  int m_unknownAc; // +0xac set to 1 by game_ui_facade 0x41d810
  int m_restoring; // +0xb0 constructor argument
  int m_musicPending; // +0xb4 1 until StartMusic
  HWND m_activeChild; // +0xb8 the child window in front
  HWND m_previousActiveChild; // +0xbc
  uint32_t m_mouseMode; // +0xc0 1 normal, 2 picking a target for m_pendingCommand; bit 4: restoring
  void* m_pendingCommand; // +0xc4 owned (polymorphic), the command waiting for its target
  int m_displayMode; // +0xc8 GID display mode, 0x80 = off
  RECT m_panelArea; // +0xcc area child panels are centred in
  int m_created; // +0xdc set at the end of WM_CREATE
  CoolLabel m_dateLabel; // +0xe0
  int16_t m_speed; // +0x114 game speed 0..4 (-1 until shown)
  int16_t m_unknown116; // +0x116
  CoolGameWindow* m_gidMenu; // +0x118 owned, the GID menu (0x451130)
  int m_legendX; // +0x11c where the GID legend panel opens
  int m_legendY; // +0x120
  CoolGameWindow* m_gidLegend; // +0x124 owned, the GID legend panel (0x452090)
  SpriteStage* m_droidStage; // +0x128 owned, the advisor droid animation
  SpriteStage* m_droidStage2; // +0x12c owned, the second droid animation
  GalaxyTickTimer* m_tickTimer; // +0x130 owned, never set (see GalaxyTickTimer)
  CoolStrobeButton* m_consoleButtons[7]; // +0x134 commands 0x12d..0x133
  CoolStrobeButton* m_unknownButton150; // +0x150 owned, never created by the view itself
  CoolFrameButton* m_messageButtons
    [9]; // +0x154 message-category buttons (commands 0x13d, 0x13e, 0x13c, 0x13b, 0x137, 0x136, 0x13a, 0x138, 0x139)
  CoolLabel m_resourceLabels[3]; // +0x178 kGalaxyRawMaterials, kGalaxyRefinedMaterials, kGalaxyMaintenance
  CoolLabel m_latencyLabel; // +0x214 multiplayer only (command-line flag 0x100)
  PanelDragButton* m_gidLegendIcon; // +0x248 owned, the minimised GID legend (0x452ab0, command 0x134)
  CoolListItemList m_sectorItems; // +0x24c ObjectListItems of the sectors (hover rectangles at +0x40)
  ObjectListItem* m_hoveredSector; // +0x25c
  CoolLabel m_sectorLabel; // +0x260 name of m_hoveredSector, drawn in WM_PAINT
  int m_gidIconIds[3][4]; // +0x294 per kind: the four threshold icons
  DibBitmap* m_gidIcons; // +0x2c4 owned
  DibBitmap* m_gidIcons2; // +0x2c8 owned
  std::set<uint32_t> m_selectedIds; // +0x2cc
  MinimizedWindowSlot m_minimized[12]; // +0x2d8
  uint32_t m_minimizedOrder[12]; // +0x428 child ids in the order they were minimised
  int m_minimizedCount; // +0x458
  DibBitmap* m_offscreen; // +0x45c owned, the composed view
  HDC m_offscreenDc; // +0x460 owned
  HCURSOR m_normalCursor; // +0x464
  HCURSOR m_targetCursor; // +0x468 resource 0x3ea
  HCURSOR m_currentCursor; // +0x46c
  uint32_t m_unknown470; // +0x470
  CoolListItemList* m_encyclopediaItems; // +0x474 every encyclopedia entry (ObjectListItems, id 0x1000 + record id)
  std::set<uint32_t> m_openWindowIds; // +0x478 ids of the open strategy child windows
  CoolListItemList m_systemItems; // +0x484 GID markers of the systems
  ResourceStringId m_titleStringId; // +0x494 title of the next GID mode switch (id 0: by mode), then reset
  LPARAM m_pendingMovie; // +0x498 owned (game_ui_facade 0x41d330 deletes it)
  ResourceStringId m_activeTitleStringId; // title override currently painted into the GID background
};

// Stores `value` at +0x30 of `object` (linker-folded setter shared by several classes).
void SetDword30(void* object, int value); // 0x441a50

// The picture of an object (key `key`, `object` if already known) for the lists and windows of the strategic game: its
// bitmap from `module` over a frame, with the badges of its state (enroute, has orders, not completed, captured) when
// `decorate`; `alternate` selects the highlighted set. Null if the object or its bitmap is missing.
DibBitmap* CreateObjectPicture(HMODULE module,
                               const ObjectKey* key,
                               GameObjNotifier* object,
                               int alternate,
                               int decorate); // 0x42c3b0

// The galaxy view, null outside the strategic game.
GalaxyWindow* GetGalaxyWindow(); // 0x422ca0

// Animates a rectangle (XOR outline) growing from `from` to `to` over four 50 ms steps.
void AnimateZoomRect(HWND hwnd, const RECT* to, const RECT* from); // 0x428940

extern GalaxyWindow* g_galaxyWindow; // 0x6b14cc
