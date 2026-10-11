#include <cstdint>
#include <new>
#include <string>

#include "ai/ai_common.h"
#include "app/app_dialogs.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "economy/side.h"
#include "events/game_clock_hooks.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "orders/mission_orders.h"
#include "orders/order_base.h"
#include "strategy_ui/battle_window.h"
#include "strategy_ui/encyclopedia_window.h"
#include "strategy_ui/fleet_finder_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/message_index_window.h"
#include "strategy_ui/mission_selection_window.h"
#include "strategy_ui/object_menu.h"
#include "strategy_ui/personnel_finder_window.h"
#include "strategy_ui/sector_window.h"
#include "strategy_ui/system_finder_window.h"
#include "strategy_ui/system_window.h"
#include "strategy_ui/troop_finder_window.h"
#include "strategy_ui/vote_window.h"
#include "tactical/comm2.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/custom_dialog_box.h"
#include "ui/object_list_item.h"
#include "ui/resource_dll.h"
#include "ui/sprite_stage.h"

extern const char* g_albriefDllName; // 0x6a86c4 "albrief.dll"
extern const char* g_embriefDllName; // 0x6a86b8 "embrief.dll"
extern const char* g_alspriteDllName; // 0x6a86e0 "alsprite.dll"
extern const char* g_emspriteDllName; // 0x6a86d0 "emsprite.dll"
extern const char* g_currentLatencyText; // 0x6a86f0 "Current Latency:"

extern int g_spriteDllSide; // 0x6b14d0 the side whose sprite DLL (slot 9) is loaded, 0 for none
extern int g_briefingDllSide; // 0x6b14d4 the side whose briefing DLL (slot 0xd) is loaded, 0 for none

// The other strategy windows' constructors (the finders, message index, mission selection, side windows) and helpers
// of the modules that own them.
void FUN_00457f30(SystemWindow* window, int slot, GameObjNotifier* manager, const ObjectKey* character);

namespace
{

constexpr uint32_t kSpriteSlot = 9;
constexpr uint32_t kBriefingSlot = 0xd;
constexpr uint32_t kStyleVisible = 0x10000000;
constexpr uint32_t kWindowTypeMask = 0x3f;
constexpr uint32_t kSectorWindowType = 1;
constexpr uint32_t kSystemWindowType = 9;
constexpr uint32_t kSectorWindowWidth = 0xeb;
constexpr uint32_t kSectorWindowHeight = 0x168;
constexpr int kFinderWidth = 0x1d6;
constexpr int kFinderHeight = 0x14a;
constexpr uint32_t kSystemFinderId = 0x14;
constexpr uint32_t kFleetFinderId = 0x15;
constexpr uint32_t kTroopFinderId = 0x16;
constexpr uint32_t kPersonnelFinderId = 0x17;
constexpr uint32_t kEncyclopediaId = 0x19;
constexpr uint32_t kMessageIndexId = 0xd;
constexpr uint32_t kMissionSelectionId = 0xc;
constexpr uint32_t kTypeSevenId = 7;
constexpr uint32_t kTypeEightId = 8;
constexpr uint32_t kType6Id = 6;
constexpr int kModeNormal = 1;
constexpr int kModePickTarget = 2;
constexpr int kResourceTypeRcData = 10;
constexpr uint16_t kDroidSlotAlliance = 9;
constexpr uint32_t kDroidIdLeft = 0xdead;
constexpr uint32_t kDroidIdRight = 0xbeef;

template <class Ctor>
void OpenAndShow(GalaxyWindow* parent, size_t size, Ctor construct, bool hidden)
{
  void* memory = ::operator new(size);
  CoolWindow* window = memory ? construct(memory) : nullptr;
  if (!window)
    return;
  if (hidden)
    window->m_style &= ~kStyleVisible;
  window->Create(0);
  parent->m_children.Insert(window);
  static_cast<CoolGameWindow*>(window)->ShowAndTrack(5);
}

void PostClosePopups(GalaxyWindow* galaxy)
{
  PostMessageA(galaxy->m_hwnd, kGalaxyClosePopups, 0, 0);
}

template <class Finder>
void OpenFinder(GalaxyWindow* galaxy, uint32_t childId)
{
  PostClosePopups(galaxy);
  if (galaxy->m_children.FindById(childId))
    return;
  OpenAndShow(
    galaxy, sizeof(Finder),
    [&](void* memory)
    {
      return static_cast<CoolWindow*>(new (memory)
                                        Finder(galaxy->m_instance, 0, 0, kFinderWidth, kFinderHeight, galaxy, childId));
    },
    true);
}

ObjectKey ParentKeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  GetObjectKey(object->m_parent, &key);
  return key;
}

void RemoveFromOpenWindows(GalaxyWindow* galaxy, CoolWindow* window)
{
  galaxy->m_openWindowIds.erase(window->m_childId);
  galaxy->DestroyChildById(window->m_childId);
}

} // namespace

// ---- Targeting, renaming and opening windows -------------------------------------------------------------

void GalaxyWindow::BeginTargeting(void* command)
{
  m_mouseMode = kModePickTarget;
  SetCaptureWindow(m_hwnd);
  m_currentCursor = m_targetCursor;
  m_pendingCommand = command;
}

void GalaxyWindow::BeginRename(TextMessage* request)
{
  ObjectKeyValueSet keys(*request->GetTeam());
  ObjectKey target;
  static_cast<ObjectKeyNode*>(keys.First())->GetKey(&target);
  for (const auto& ownedChild : m_children.Entries())
  {
    CustomDialogBox* window = static_cast<CustomDialogBox*>(ownedChild.get());
    if (window->m_objects.Find(&target) && (window->m_childId & kWindowTypeMask) == kSystemWindowType)
    {
      static_cast<SystemWindow*>(window)->BeginRenameRequest(request);
      return;
    }
    if (window->m_objects.Find(&target) && window->m_hwnd == m_activeChild)
    {
      window->BeginRename(request);
      return;
    }
  }
}

CoolWindow* GalaxyWindow::OpenSectorWindow(const ObjectKey* sectorKey)
{
  PostClosePopups(this);
  Sector* sector = static_cast<Sector*>(GetObjectInFamilies_80_90(m_side, sectorKey));
  if (!sector)
    return nullptr;
  const uint32_t childId = (static_cast<uint16_t>(static_cast<int16_t>(sectorKey->m_value) << 6)) | kSectorWindowType;
  if (CoolWindow* existing = m_children.FindById(childId))
    return existing;
  UiHumanSetAdvisorMask(1);
  const bool empire = m_side != 1;
  int left = (empire ? 0x3c : 0) + 0x3c;
  int top = (empire ? 5 : 0) + 0x23;
  const int rightLeft = (empire ? 0x41 : 0) + 300;
  CoolWindow* sameLeft = nullptr;
  CoolWindow* other = nullptr;
  for (const auto& ownedNode : m_children.Entries())
  {
    auto* node = ownedNode.get();
    CoolWindow* window = static_cast<CoolWindow*>(node);
    CoolWindow* previousOther = other;
    if ((window->m_childId & kWindowTypeMask) == kSectorWindowType)
    {
      other = window;
      if (window->m_x == left)
      {
        sameLeft = window;
        other = previousOther;
      }
      if (sameLeft && other)
        break;
    }
  }
  CoolWindow* replaced = other;
  bool replace = false;
  if (!sameLeft)
  {
    if (!other)
    {
      uint32_t position;
      sector->GetPosition(&position);
      if (static_cast<int16_t>(position) > 0x1fe)
        left = rightLeft;
    }
    else
    {
      replace = false;
    }
  }
  else
  {
    left = rightLeft;
    if (other)
      replace = true;
  }
  if (replace)
  {
    uint32_t position;
    sector->GetPosition(&position);
    if (static_cast<int16_t>(position) < 0x1ff)
      replaced = sameLeft;
    left = replaced->m_x;
    top = replaced->m_y;
    RemoveFromOpenWindows(this, replaced);
  }
  void* memory = ::operator new(sizeof(SectorWindow));
  CoolWindow* window =
    memory ?
      static_cast<CoolWindow*>(new (memory) SectorWindow(m_instance, left, top, kSectorWindowWidth, kSectorWindowHeight,
                                                         this, GetBackground(), childId, sector)) :
      nullptr;
  if (!window)
    return nullptr;
  window->Create(0);
  TrackChildWindow(window);
  m_children.Insert(window);
  SetWindowPos(window->m_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetWindowPos(m_gidLegendIcon->m_hwnd, window->m_hwnd, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  return window;
}

void GalaxyWindow::OpenEncyclopedia(const ObjectKey* object, const NullableKey* type)
{
  PostClosePopups(this);
  if (m_children.FindById(kEncyclopediaId))
    return;
  EncyclopediaWindow* window =
    NewOrNull<EncyclopediaWindow>(m_instance, 0, 0, kFinderWidth, kFinderHeight, this, kEncyclopediaId, *object, *type);
  if (!window)
    return;
  window->m_style &= ~kStyleVisible;
  window->Create(0);
  m_children.Insert(window);
  window->ShowAndTrack(5);
}

void GalaxyWindow::OpenSystemFinder()
{
  OpenFinder<SystemFinderWindow>(this, kSystemFinderId);
}

void GalaxyWindow::OpenFleetFinder()
{
  OpenFinder<FleetFinderWindow>(this, kFleetFinderId);
}

void GalaxyWindow::OpenPersonnelFinder()
{
  OpenFinder<PersonnelFinderWindow>(this, kPersonnelFinderId);
}

void GalaxyWindow::OpenTroopFinder()
{
  OpenFinder<TroopFinderWindow>(this, kTroopFinderId);
}

void GalaxyWindow::OpenMessageIndex(int mode, int tab)
{
  PostClosePopups(this);
  if (m_children.FindById(kMessageIndexId))
    return;
  if (GameHookTickCount() <= 1)
    return;
  OpenAndShow(
    this, sizeof(MessageIndexWindow),
    [&](void* memory)
    {
      return static_cast<CoolWindow*>(new (memory) MessageIndexWindow(m_instance, this, kMessageIndexId, mode, tab));
    },
    false);
}

void GalaxyWindow::RefreshMessageIndex()
{
  CoolWindow* messageIndex = m_children.FindById(kMessageIndexId);
  if (messageIndex)
    static_cast<MessageIndexWindow*>(messageIndex)->Refresh();
}

void GalaxyWindow::IssueTargetedCommand(void* command)
{
  PostClosePopups(this);
  ObjectNameList candidates;
  static_cast<MissionOrder240*>(command)->CollectCandidatesB(&candidates);
  if (!candidates.First())
  {
    UiHumanReceiveMessage(static_cast<PlayerMessage*>(command), 0);
    return;
  }
  OpenAndShow(
    this, sizeof(MissionSelectionWindow),
    [&](void* memory)
    {
      return static_cast<CoolWindow*>(new (memory) MissionSelectionWindow(m_instance, this, kMissionSelectionId,
                                                                          static_cast<MissionOrder240*>(command)));
    },
    false);
}

void GalaxyWindow::OpenType1cWindow(LPARAM argument)
{
  PostClosePopups(this);
  PostMessageA(m_hwnd, kGalaxyOpenModal, 0x1c, argument);
}

void GalaxyWindow::OpenObjectStatus(const ObjectKey* key)
{
  PostClosePopups(this);
  ObjectKey* copy = NewOrNull<ObjectKey>(*key);
  PostMessageA(m_hwnd, kGalaxyOpenModal, 0x1a, reinterpret_cast<LPARAM>(copy));
}

void GalaxyWindow::OpenOrderConfirm(LPARAM argument)
{
  PostClosePopups(this);
  PostMessageA(m_hwnd, kGalaxyOpenModal, 0x10, argument);
}

void GalaxyWindow::OpenType6Window(LPARAM argument)
{
  PostClosePopups(this);
  if (m_mouseMode == kModePickTarget)
  {
    if (m_pendingCommand)
    {
      delete static_cast<PlayerMessage*>(m_pendingCommand);
      m_pendingCommand = nullptr;
    }
    HWND hwnd = m_hwnd;
    m_mouseMode = kModeNormal;
    if (GetCapture() == hwnd)
      ReleaseCapture();
  }
  PostMessageA(m_hwnd, kGalaxyOpenModal, 6, argument);
}

void GalaxyWindow::OpenType8Window(LPARAM argument)
{
  PostClosePopups(this);
  if (CoolWindow* previous = m_children.FindById(kTypeSevenId))
    DestroyChildById(previous->m_childId);
  void* memory = ::operator new(sizeof(BattleWindow));
  CoolWindow* window = memory ? static_cast<CoolWindow*>(new (memory) BattleWindow(
                                  m_instance, this, kTypeEightId, reinterpret_cast<BattleMessage*>(argument))) :
                                nullptr;
  window->Create(0);
  static_cast<CoolGameWindow*>(window)->ShowAndTrack(5);
  m_children.Insert(window);
  SetFocus(window->m_hwnd);
}

void GalaxyWindow::ForwardToType6Window(int a, int b)
{
  if (CoolWindow* window = m_children.FindById(kType6Id))
    static_cast<VoteWindow*>(window)->VoteWindow::ShowButtons(a, b);
}

// ---- Manufacturing manager changes shown in the system window ----------------------------------------

namespace
{

SystemWindow* SystemWindowOfManager(GalaxyWindow* galaxy, GameObjNotifier* manager)
{
  const int16_t systemId = static_cast<int16_t>(ParentKeyOf(manager).m_value);
  return static_cast<SystemWindow*>(galaxy->m_children.FindById((static_cast<int>(systemId) << 6) | kSystemWindowType));
}

} // namespace

void GalaxyWindow::OnManagerCharacterChanged(const ObjectKey* managerKey, const ObjectKey* character)
{
  GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_side, managerKey);
  SystemWindow* window = SystemWindowOfManager(this, manager);
  if (window)
  {
    const int slot = window->SlotOfManager(managerKey);
    window->SetSlotCharacter(slot, manager, character);
  }
}

void GalaxyWindow::OnManagerStatusTextChanged(const ObjectKey* managerKey, std::string* text)
{
  GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_side, managerKey);
  SystemWindow* window = SystemWindowOfManager(this, manager);
  if (window)
  {
    const int slot = window->SlotOfManager(managerKey);
    window->SetSlotStatusText(slot, manager, text);
  }
}

void GalaxyWindow::OnManagerProductionChanged(const ObjectKey* managerKey, int unused)
{
  GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_side, managerKey);
  SystemWindow* window = SystemWindowOfManager(this, manager);
  if (window)
  {
    const int slot = window->SlotOfManager(managerKey);
    window->SetSlotProduction(slot, manager);
  }
  BroadcastObjectUpdated(managerKey, nullptr);
}

void GalaxyWindow::OnManagerProgressChanged(const ObjectKey* managerKey, int position)
{
  GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_side, managerKey);
  SystemWindow* window = SystemWindowOfManager(this, manager);
  if (window)
  {
    const int slot = window->SlotOfManager(managerKey);
    window->SetSlotProgress(slot, manager, position, 0);
  }
}

// ---- Broadcasts to the open windows --------------------------------------------------------------------------

namespace
{

bool IsContainerFamily(uint32_t family)
{
  return (family >= 0x90 && family < 0x98) || (family >= 8 && family < 0x10) || (family >= 0x14 && family < 0x1c) ||
         (family >= 0x40 && family < 0x80);
}

} // namespace

void GalaxyWindow::BroadcastObjectChanged(const ObjectKey* key, GameObjNotifier* object)
{
  auto openWindowPosition = m_openWindowIds.begin();
  UpdateSystemMarker(key, object, 1);
  if (!object)
    object = GetObjectInFamilies_01_ff(m_side, key);
  if (!object)
    return;
  if (!object->m_parent)
    return;
  ObjectKey parentKey;
  GetObjectKey(object->m_parent, &parentKey);
  if (!IsContainerFamily(parentKey.Family()) || openWindowPosition == m_openWindowIds.end())
    return;
  while (openWindowPosition != m_openWindowIds.end())
  {
    auto current = openWindowPosition++;
    const uint32_t openWindowId = *current;
    CustomDialogBox* window = static_cast<CustomDialogBox*>(m_children.FindById(openWindowId));
    if (window && window->m_objects.Find(&parentKey))
    {
      window->OnChildAdded(key, object);
      if (FindMinimizedSlot(window->m_childId))
        UpdateMinimizedIcon(window);
    }
  }
}

void GalaxyWindow::BroadcastObjectMoved(const ObjectKey* key,
                                        GameObjNotifier* object,
                                        const ObjectKey* otherKey,
                                        GameObjNotifier* otherObject)
{
  if (!object)
    object = GetObjectInFamilies_01_ff(m_side, key);
  auto openWindowPosition = m_openWindowIds.begin();
  UpdateSystemMarker(otherKey, otherObject, 1);
  while (openWindowPosition != m_openWindowIds.end())
  {
    auto current = openWindowPosition++;
    const uint32_t openWindowId = *current;
    CustomDialogBox* window = static_cast<CustomDialogBox*>(m_children.FindById(openWindowId));
    if (!window)
      continue;
    if (window->m_objects.Find(key) || (otherKey && window->m_objects.Find(otherKey)))
    {
      window->OnChildMoved(key, object, otherKey, otherObject);
      if (FindMinimizedSlot(window->m_childId))
        UpdateMinimizedIcon(window);
    }
  }
}

void GalaxyWindow::BroadcastObjectUpdated(const ObjectKey* key, GameObjNotifier* object)
{
  if (!object)
    object = GetObjectInFamilies_01_ff(m_side, key);
  if (!object)
    return;
  UpdateSystemMarker(key, object, 1);
  for (uint32_t openWindowId : m_openWindowIds)
  {
    CustomDialogBox* window = static_cast<CustomDialogBox*>(m_children.FindById(openWindowId));
    if (window && window->m_objects.Find(key))
    {
      window->OnObjectUpdated(key, object);
      if (FindMinimizedSlot(window->m_childId))
        UpdateMinimizedIcon(window);
    }
  }
}

int GalaxyWindow::TrackChildWindow(CoolWindow* window)
{
  const uint32_t id = window->m_childId;
  return m_openWindowIds.insert(id).second || m_openWindowIds.find(id) != m_openWindowIds.end();
}

// ---- Droid animations ----------------------------------------------------------------------------------

namespace
{

void StartScript(SpriteStage* stage, uint32_t droidId, uint32_t resourceId, uint32_t x, uint32_t y, uint16_t slot)
{
  const uint16_t* script = nullptr;
  HGLOBAL data = nullptr;
  HMODULE module = FindResourceModule(slot);
  HRSRC info = FindResourceA(module, MAKEINTRESOURCEA(resourceId & 0xffff), MAKEINTRESOURCEA(kResourceTypeRcData));
  if (info)
  {
    data = LoadResource(module, info);
    if (data)
      script = static_cast<const uint16_t*>(LockResource(data));
  }
  if (!script || !stage)
    return;
  if (x == 0)
    x = script[2];
  if (y == 0)
    y = script[3];
  uint32_t sequence = (static_cast<uint32_t>(slot) << 16) | script[1];
  stage->StartAnimator(droidId, &sequence, script[0], x, y);
  FreeResource(data);
}

} // namespace

void GalaxyWindow::PlayDroidScript(uint32_t resourceId, uint32_t x, uint32_t y, int alternateSet)
{
  const uint16_t slot = (alternateSet ? 4 : 0) + kDroidSlotAlliance;
  StartScript(m_droidStage, kDroidIdLeft, resourceId, x, y, slot);
}

void GalaxyWindow::PlaySecondDroidScript(uint32_t resourceId, uint32_t x, uint32_t y)
{
  StartScript(m_droidStage2, kDroidIdRight, resourceId, x, y, kDroidSlotAlliance);
}

// ---- Markers, controls, date ------------------------------------------------------------------------------

void GalaxyWindow::ShowSpeedMenu(int x, int y)
{
  constexpr uint32_t kMenuKind = 7;
  constexpr uint32_t kBaseColorAlliance = 0x20000ff;
  constexpr uint32_t kBaseColorEmpire = 0x200ff00;
  constexpr uint32_t kShadowColor = 0x2ffffff;
  constexpr uint32_t kDisabledColor = 0x2808080;
  if (m_side == 1)
  {
    for (int command = 0x20; command < 0x25; ++command)
      AddMenuEntry(command, kMenuKind, 0, x, y, this, kBaseColorAlliance, kShadowColor, kDisabledColor);
  }
  else
  {
    for (int command = 0x25; command < 0x2a; ++command)
      AddMenuEntry(command, kMenuKind, 0, x, y, this, kBaseColorEmpire, kShadowColor, kDisabledColor);
  }
  ShowMenu(x, y, CoolAppGetResourceModule(7), this, this, GetBackground());
}

void GalaxyWindow::ShowObjectMenu(int x, int y, uint32_t kind)
{
  if (kind == 0)
    return;
  const uint32_t baseColor = (AppGetSide() != 1 ? 0xfe01 : 0) + 0x20000ff;
  EventKindSet commands;
  if (kind == 0xbeef)
  {
    UiHumanRefreshOrderMenus(&commands);
  }
  else if (kind == 0xdead)
  {
    UiHumanRefreshMenus(&commands, 1);
  }
  else
  {
    return;
  }
  for (const auto& [kind, node] : commands.Entries())
  {
    uint32_t flags = node.m_value == 0 ? 1 : 0;
    if (node.m_unknown != 0)
      flags |= 2;
    AddMenuEntry(static_cast<uint32_t>(kind), 7, flags, x, y, this, baseColor, 0x2ffffff, 0x2808080);
  }
  ShowMenu(x, y, CoolAppGetResourceModule(7), this, this, GetBackground());
}

// ---- Window services -----------------------------------------------------------------------------------

void GalaxyWindow::RefreshChildWindows()
{
  for (uint32_t openWindowId : m_openWindowIds)
  {
    CoolWindow* window = m_children.FindById(openWindowId);
    if (window)
      static_cast<CustomDialogBox*>(window)->Virtual35();
  }
}

void GalaxyWindow::Virtual0B()
{
  if (m_droidStage)
    m_droidStage->Tick();
  if (m_droidStage2)
    m_droidStage2->Tick();
}

void GalaxyWindow::StopDroidScripts()
{
  m_droidStage->StopAnimator(kDroidIdLeft);
  m_droidStage2->StopAnimator(kDroidIdRight);
}

int GalaxyWindow::IsShownAndIdle()
{
  const uint32_t dialogMessage = g_appDialogMessage;
  if (m_hwnd && m_created)
  {
    if (IsWindowVisible(m_hwnd) && dialogMessage == 0)
      return 1;
  }
  return 0;
}

void GalaxyWindow::EnableControls()
{
  m_currentCursor = m_normalCursor;
  SetCursor(m_normalCursor);
  for (CoolStrobeButton* button : m_consoleButtons)
  {
    if (button)
      button->Enable();
  }
  if (m_unknownButton150)
    m_unknownButton150->Enable();
  if (m_gidLegendIcon)
    m_gidLegendIcon->Enable();
}

void GalaxyWindow::DisableControls()
{
  m_currentCursor = LoadCursorA(nullptr, IDC_WAIT);
  SetCursor(m_currentCursor);
  for (CoolStrobeButton* button : m_consoleButtons)
  {
    if (button)
      button->Disable();
  }
  if (m_unknownButton150)
    m_unknownButton150->Disable();
  if (m_gidLegendIcon)
    m_gidLegendIcon->Disable();
}

DibBitmap* GalaxyWindow::GetMarkerImage(GameObjNotifier* system)
{
  ObjectKey key;
  GetObjectKey(system, &key);
  DibBitmap* image = nullptr;
  if (CoolListItem* marker = m_systemItems.FindById(key.Id()))
    image = marker->m_image;
  return image;
}

namespace
{

struct ButtonHighlight
{
  uint32_t m_bit;
  int m_index;
};

constexpr ButtonHighlight kButtonHighlights[] = {{0x1, 5},  {0x4, 8},  {0x8, 6},   {0x10, 7}, {0x20, 1},
                                                 {0x40, 3}, {0x80, 4}, {0x100, 2}, {0x200, 0}};

} // namespace

void GalaxyWindow::ClearButtonHighlights(uint32_t mask)
{
  for (const ButtonHighlight& entry : kButtonHighlights)
  {
    if ((mask & entry.m_bit) == 0)
      continue;
    CoolFrameButton* button = m_messageButtons[entry.m_index];
    if (button->GetState() & kButtonHighlight)
      button->ClearStateFlags(kButtonHighlight);
  }
}

void GalaxyWindow::SetButtonHighlights(uint32_t mask)
{
  for (const ButtonHighlight& entry : kButtonHighlights)
  {
    if (mask != 0 && (mask & entry.m_bit) == 0)
      continue;
    CoolFrameButton* button = m_messageButtons[entry.m_index];
    if ((button->GetState() & kButtonHighlight) == 0)
      button->SetStateFlags(kButtonHighlight);
  }
}

void GalaxyWindow::ResumePendingRequest()
{
  if (m_alternateDisplayMode == 0)
  {
    if (m_optionsRequested != 0 && m_unknownAc == 0)
      PostMessageA(m_hwnd, WM_COMMAND, kGalaxyCancelTargeting, 0);
  }
  else if (m_inModalLoop == 0)
  {
    PostMessageA(m_hwnd, kGalaxyPlayMovie, 0, m_alternateDisplayMode);
    m_optionsRequested = 0;
    return;
  }
  m_optionsRequested = 0;
}

void GalaxyWindow::ShowLatency()
{
  std::string text;
  text += StringFromText(g_currentLatencyText);
  text.append(std::to_string(static_cast<unsigned long>(CommGetLastLatency())));
  m_latencyLabel.SetText(text.c_str());
  RECT area = m_latencyLabel.m_rect;
  InvalidateRect(m_hwnd, &area, FALSE);
}

void GalaxyWindow::RedrawSectorMarkers(GameObjNotifier* sector)
{
  RECT dirty;
  SetRectEmpty(&dirty);
  SystemIterator systems(sector, 4);
  for (systems.Begin(); systems.Current(); systems.Advance())
  {
    ObjectKey key;
    GetObjectKey(systems.Current(), &key);
    CoolListItem* marker = m_systemItems.FindById(key.Id());
    if (!marker)
      continue;
    marker->m_selectedImage->Blit(GetBackground(), marker->m_x, marker->m_y, 0, 0, 0, 0);
    UnionRect(&dirty, &dirty, &marker->m_rect);
  }
  for (systems.Begin(); systems.Current(); systems.Advance())
  {
    ObjectKey key;
    GetObjectKey(systems.Current(), &key);
    CoolListItem* marker = m_systemItems.FindById(key.Id());
    if (!marker)
      continue;
    DibBitmap* image;
    if ((marker->m_state.m_value & 0x80) == 0)
    {
      if (marker->m_state.m_value & 8)
        m_gidIcons2->DrawKeyed(GetBackground(), marker->m_x, marker->m_y);
      image = marker->m_image;
      if (!image)
        continue;
    }
    else
    {
      image = m_gidIcons;
    }
    image->DrawKeyed(GetBackground(), marker->m_x, marker->m_y);
  }
  if (!IsRectEmpty(&dirty))
    InvalidateRect(m_hwnd, &dirty, FALSE);
}

void GalaxyWindow::SetDateAndSpeed(int day, int speed, int force)
{
  std::string text;
  if (day != 0)
  {
    text.append(std::to_string(static_cast<unsigned long>(day)));
    m_dateLabel.SetText(text.c_str());
    m_dateLabel.Measure(m_offscreenDc);
    RECT area = m_dateLabel.m_rect;
    InvalidateRect(m_hwnd, &area, FALSE);
  }
  HMODULE module = CoolAppGetResourceModule(7);
  int16_t bitmapId = 0;
  if (m_speed != static_cast<int16_t>(speed) || force != 0)
  {
    m_speed = static_cast<int16_t>(speed);
    const int16_t empire = m_side != 1 ? 1 : 0;
    switch (speed)
    {
      case 0:
        bitmapId = (empire ? 4 : 0) + 0x2d3c;
        break;
      case 1:
        bitmapId = (empire ? 4 : 0) + 0x2d3d;
        break;
      case 2:
        bitmapId = (empire ? 4 : 0) + 0x2d3e;
        break;
      case 3:
        bitmapId = (empire ? 4 : 0) + 0x2d3f;
        break;
      case 4:
        bitmapId = empire + 0x2d44;
        break;
    }
    if (bitmapId != 0)
    {
      DibBitmap* image = NewOrNull<DibBitmap>(module, static_cast<uint32_t>(bitmapId), 2u);
      int bottom;
      int right;
      int left;
      if (m_side == 1)
      {
        bottom = image->Height() + 0x14;
        right = image->Width() + 0xa7;
        left = 0xa7;
      }
      else
      {
        bottom = image->Height() + 0x14;
        right = image->Width() + 0x231;
        left = 0x231;
      }
      RECT area;
      SetRect(&area, left, 0x14, right, bottom);
      image->Blit(GetBackground(), area.left, area.top, 0, 0, 0, 0);
      if (image)
      {
        image->~DibBitmap();
        operator delete(image);
      }
      InvalidateRect(m_hwnd, &area, FALSE);
    }
  }
  if (AppGetCommandLineFlags() & 0x100)
    ShowLatency();
}

ObjectKey* ObjectListItem::GetKey(ObjectKey* out)
{
  out->m_value = m_key.m_value;
  return out;
}

const char* g_albriefDllName = nullptr;
const char* g_alspriteDllName = nullptr;
const char* g_currentLatencyText = nullptr;
const char* g_embriefDllName = nullptr;
const char* g_emspriteDllName = nullptr;
int g_briefingDllSide{};
int g_spriteDllSide{};
