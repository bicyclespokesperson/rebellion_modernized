#pragma once
// MultiPlayerWindow (0x40d7a0-0x4119e0, VT 0x658788, 0x194 bytes): the multiplayer lobby, shown from the front end's
// "Play Head to Head" console (application states 3, 0xe...) and, for games started from a DirectPlay lobby, directly on
// its last page. It walks the player through four pages (m_page, the id of the page's overlay bitmap):
//
//   0x2774  provider    pick a DirectPlay service provider (CommMgr::EnumerateServiceProviders), then "Connect To
//                       Game" (join) or "Setup Game" (host)
//   0x2775  join        the sessions of the provider (refreshed by a 5 s timer) and the player's name
//   0x2776  host        the player's name and the name of the new session
//   0x2777  setup       chat, the host's choices (side, galaxy size, game type) and the Start button
//
// Network traffic of the page 0x2777 is MessageBase objects of the Mp* kinds (app/app_control_cmds.h) coded with
// MessageCodec and sent on category 1 (settings, slot table, chat, player info) and read on categories 1 and 4;
// ProcessNetwork (0x4104b0) drains them every 400 ms timer tick and on every window message.
#include <cstdint>

#include "core/game_string.h"
#include "multiplayer/comm_packets.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolDragList;
class CoolStringField;
class CoolStrobeButton;
class MessageBase;
class TextMessage;
class TextScrollField;
class SavedGameInfo;

// Pages of the lobby (also the overlay bitmap ids and the string group of their texts).
enum MultiPlayerPage : int
{
  kLobbyPageProvider = 0x2774,
  kLobbyPageJoin = 0x2775,
  kLobbyPageHost = 0x2776,
  kLobbyPageSetup = 0x2777,
};

// Child ids of the buttons.
enum MultiPlayerButton : uint32_t
{
  kLobbyButtonBack = 100,
  kLobbyButtonNext = 0x65,
  kLobbyButtonStart = 0x66,
  kLobbyButtonCancel = 0x67,
  kLobbyButtonJoinMode = 0x68,
  kLobbyButtonHostMode = 0x69,
  kLobbyButtonSideA = 0x6a,
  kLobbyButtonSideB = 0x6b,
  kLobbyButtonSize1 = 0x6c,
  kLobbyButtonSize2 = 0x6d,
  kLobbyButtonSize3 = 0x6e,
  kLobbyButtonLoad = 0x6f,
  kLobbyButtonType1 = 0x70,
  kLobbyButtonType2 = 0x71,
};

// Child ids of the lists and fields.
enum MultiPlayerChild : uint32_t
{
  kLobbyChildProviderList = 0x12c,
  kLobbyChildSessionList = 0x12d,
};

// Custom messages: 0x407 is the "return pressed" message of a string field, 0x40c the CommMgr notification.
constexpr uint32_t kLobbyTimerSessions = 0x191; // refreshes the session list of the join page
constexpr uint32_t kLobbyTimerNetwork = 400; // drains the network on the setup page

class MultiPlayerWindow : public CoolGameWindow
{
public:
  MultiPlayerWindow(HINSTANCE instance,
                    int x,
                    int y,
                    int width,
                    int height,
                    CoolWindow* parent,
                    uint32_t stringSlot,
                    uint32_t bitmapSlot,
                    uint32_t backgroundId,
                    uint32_t childId,
                    uint32_t style,
                    uint32_t flags,
                    int side,
                    int galaxySize); // 0x40d7a0
  ~MultiPlayerWindow() override; // 0x40d980 (scalar deleting destructor 0x40d960)

  LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override; // slot 5, 0x40ec60
  // Starts the DirectPlay layer for this window and chooses the first page.
  void OnCreate1() override; // slot 14, 0x40db20
  void OnCreate2() override; // slot 15, 0x40dc30: the texts of the four pages
  void OnCreate3() override; // slot 16, 0x40dfd0: lists, fields, buttons
  void OnCommand(int a, int b, int c) override; // slot 18, 0x40efb0 (a is the low word: the child id)
  int Virtual14() override
  {
    return 1;
  } // slot 20, shared 0x40f340

  // Fills the provider list with what DirectPlay offers.
  void FillProviderList(); // 0x40f350
  // Refreshes the session list (join page); returns the CommMgr status, 0x11 without lists.
  int RefreshSessionList(); // 0x40f450
  // True if the sessions of `sessions` are exactly the ones the session list shows.
  int SessionListUpToDate(SessionList* sessions); // 0x40f5d0
  // Remember (as copies) the selected provider / session; the old copies are deleted.
  void RememberSelectedProvider(); // 0x40f6d0
  void RememberSelectedSession(); // 0x40f790
  // The page changed: hide every control, then the page shows what it needs.
  void HideControls(); // 0x40f850
  void SetGameType(int type, int force, int apply); // 0x40f9f0 buttons 0x70 / 0x71
  void SetSide(int side, int force, int apply); // 0x40fa70 buttons 0x6a / 0x6b
  void SetGalaxySize(int size, int force, int apply); // 0x40faf0 buttons 0x6c / 0x6d / 0x6e
  // Shows or hides the host's choice buttons.
  void ShowSetupButtons(int show); // 0x40fbb0
  // Sends the host's choices (and the start flag) to the other player.
  void SendSettings(int startGame); // 0x40fd20
  // Writes the host's choices into the chat window as a status line.
  void AnnounceSettings(); // 0x40fe10
  // Prints `text` (or the name of the player) in the chat window, preceded by the player's name.
  void PrintChatLine(const std::string* text); // 0x410240
  // Prints the "player joined" marker line (the "\x17" "4" code) in the chat window.
  void PrintStatusMarker(); // 0x4101e0
  // One round of network handling for the setup page.
  void ProcessNetwork(); // 0x4104b0
  // A chat line arrived.
  void HandleChat(TextMessage* message); // 0x410850
  // Enables the load button and the start button according to the number of players and the saved games.
  void UpdateStartButtons(); // 0x4109e0
  // Back to the provider page after closing the session.
  void ReturnToProviderPage(); // 0x410b10
  void ShowJoinPage(); // 0x410c70
  void ShowHostPage(); // 0x410dd0
  void ShowSetupPage(); // 0x410e90
  // Default names: the account name and the computer name, preceded by the string resource of the page.
  std::string* GetDefaultPlayerName(std::string* out); // 0x411270
  std::string* GetDefaultGameName(std::string* out); // 0x411340
  // Broadcasts the settings now and then so a late joiner picks them up.
  void PollSlotTable(); // 0x4115c0
  // Sends a message to the other player on category 1 and deletes it.
  void SendToPeer(MessageBase* message); // 0x4106f0

  int m_ready; // +0x114 the controls exist
  int m_closing; // +0x118 set when the window leaves; stops the timers' work
  int m_isHost; // +0x11c the player creates the session
  int m_side; // +0x120 1 or 2: the faction the host plays
  int m_galaxySize; // +0x124 1, 2 or 3
  int m_gameType; // +0x128 1 or 2
  int m_page; // +0x12c MultiPlayerPage
  uint32_t m_gameSeed; // +0x130 the seed the host chose for the galaxy
  uint32_t m_lastPollTime; // +0x134 timeGetTime() of the last sign of the other player
  uint32_t m_lastPlayerCount; // +0x138 CommMgr player count at the last ProcessNetwork
  CoolStrobeButton* m_backButton; // +0x13c child 100
  CoolStrobeButton* m_nextButton; // +0x140 child 0x65
  CoolStrobeButton* m_startButton; // +0x144 child 0x66
  CoolStrobeButton* m_joinModeButton; // +0x148 child 0x68
  CoolStrobeButton* m_hostModeButton; // +0x14c child 0x69
  CoolStrobeButton* m_sideButtonA; // +0x150 child 0x6a
  CoolStrobeButton* m_sideButtonB; // +0x154 child 0x6b
  CoolStrobeButton* m_sizeButton1; // +0x158 child 0x6c
  CoolStrobeButton* m_sizeButton2; // +0x15c child 0x6d
  CoolStrobeButton* m_sizeButton3; // +0x160 child 0x6e
  CoolStrobeButton* m_loadButton; // +0x164 child 0x6f
  CoolStrobeButton* m_typeButton1; // +0x168 child 0x70
  CoolStrobeButton* m_typeButton2; // +0x16c child 0x71
  CoolStringField* m_gameNameField; // +0x170 host page: name of the session
  CoolStringField* m_hostNameField; // +0x174 host page: the player's name
  CoolStringField* m_joinNameField; // +0x178 join page: the player's name
  CoolStringField* m_chatInput; // +0x17c setup page
  TextScrollField* m_chatLog; // +0x180 setup page
  CoolDragList* m_providerList; // +0x184 items own a ServiceProviderInfo copy
  CoolDragList* m_sessionList; // +0x188 items own a SessionInfo copy
  ServiceProviderInfo* m_selectedProvider; // +0x18c owned copy
  SessionInfo* m_selectedSession; // +0x190 owned copy
};

// Globals of the lobby.
// The session list rectangle (0x658778); the battle flow reads its left and right edges as the flow range.
extern const RECT g_rectSessionList; // 0x658778

extern uint32_t g_lobbySlotTable[7]; // 0x6b1220, indexed 1..6: the saved game ids the players announced
extern uint32_t g_lobbyFlags; // 0x6b123c bit 0: version message shown, bit 1: slot table received
extern uint32_t g_lobbySlotPollTime; // 0x6b1248 timeGetTime() of the last slot table broadcast (-1: send now)

// Free helpers of the lobby, in this module's address range.
int ParseLobbyText(const std::string* text,
                   std::string* out,
                   int stringGroup); // 0x40ffd0: expands the \x17 escape codes
MessageBase* ReceiveLobbyMessage(); // 0x410790
std::string* GetMessageText(TextMessage* message, std::string* out); // 0x411710: a copy of the text of a chat message
int IsSlotAssigned(int slot); // 0x411410
uint32_t GetSlotSaveId(int slot); // 0x411450
void SetLobbyFlags(uint32_t flags); // 0x411490
void SetSlotValue(int slot, uint32_t value); // 0x4114a0
void BroadcastSlotTable(); // 0x4114c0
