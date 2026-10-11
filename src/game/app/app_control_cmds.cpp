#include "app/app_control_cmds.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "multiplayer/multiplayer_window.h"

extern const int g_rectA[4];  // 0x658b08 {left, top, right, bottom}
extern int g_rectAWidth;      // 0x6b14c4
extern const int g_rectB[4];  // 0x658b90
extern int g_rectBWidth;      // 0x6b14c8

namespace {

constexpr int kSlotCount = 6;
constexpr int kRectLeft = 0;
constexpr int kRectRight = 2;
constexpr int kRectMargin = 5;
constexpr int kActionSetLocalPlayer = 1;
constexpr int kActionSetPeerId = 2;

int rectWidth(const int (&rect)[4]) { return rect[kRectRight] - rect[kRectLeft] - kRectMargin; }

}  // namespace

// ---- MpSlotTableMsg (kind 0x14) ------------------------------------------------------------------------------

MpSlotTableMsg::MpSlotTableMsg() {
    for (int& value : m_values) value = 0;
}

MpSlotTableMsg::~MpSlotTableMsg() { REB_KEEP_VPTR_STORE(); }

int MpSlotTableMsg::Read(istream* in) {
    int result = MessageBase::Read(in);
    // BUG(original): the values are read into the function's own argument slot, which starts out holding the
    // stream pointer and keeps the last value read; a failed read therefore stores that stale value.
    int staged = static_cast<int>(reinterpret_cast<uintptr_t>(in));
    for (int& value : m_values) {
        StreamRead32(in, &staged);
        value = staged;
    }
    return result;
}

int MpSlotTableMsg::Write(ostream* out) {
    int result = MessageBase::Write(out);
    for (const int& value : m_values) StreamWrite32(out, &value);
    return result;
}

int MpSlotTableMsg::GetKind() { return 0x14; }

int MpSlotTableMsg::Apply() {
    for (int slot = 1; slot <= kSlotCount; ++slot) SetSlotValue(slot, m_values[slot - 1]);
    return 0;
}

MessageBase* CreateMpSlotTableMsg() { return NewOrNull<MpSlotTableMsg>(); }

void ComputeRectAWidth() { g_rectAWidth = rectWidth(g_rectA); }
void StaticInitRectAWidth() { ComputeRectAWidth(); }

// ---- MpLobbySettingsMsg (kind 0x10) --------------------------------------------------------------------------

MpLobbySettingsMsg::MpLobbySettingsMsg(int setting130, int setting120, int setting124, int setting128, int startGame)
    : m_setting130(setting130),
      m_setting120(setting120),
      m_setting124(setting124),
      m_setting128(setting128),
      m_startGame(startGame) {}

MpLobbySettingsMsg::~MpLobbySettingsMsg() { REB_KEEP_VPTR_STORE(); }

int MpLobbySettingsMsg::Read(istream* in) {
    MessageBase::Read(in);
    StreamRead32(in, &m_setting130);
    StreamRead32(in, &m_setting120);
    StreamRead32(in, &m_setting124);
    StreamRead32(in, &m_setting128);
    StreamRead32(in, &m_startGame);
    return 1;
}

int MpLobbySettingsMsg::Write(ostream* out) {
    MessageBase::Write(out);
    StreamWrite32(out, &m_setting130);
    StreamWrite32(out, &m_setting120);
    StreamWrite32(out, &m_setting124);
    StreamWrite32(out, &m_setting128);
    StreamWrite32(out, &m_startGame);
    return 1;
}

int MpLobbySettingsMsg::GetKind() { return 0x10; }

MessageBase* CreateMpLobbySettingsMsg() { return NewOrNull<MpLobbySettingsMsg>(0, 0, 0, 0, 0); }

// ---- MpChatMsg (kind 0x11) -----------------------------------------------------------------------------------

MpChatMsg::MpChatMsg() {}

MpChatMsg::~MpChatMsg() { REB_KEEP_VPTR_STORE(); }

int MpChatMsg::Read(istream* in) {
    TextMessage::Read(in);
    return 1;
}

int MpChatMsg::Write(ostream* out) {
    TextMessage::Write(out);
    return 1;
}

int MpChatMsg::GetKind() { return 0x11; }

int MpChatMsg::Execute(StatusCode* status) {
    StatusCode success(1, 0xffffffff);
    new (status) StatusCode(success);
    return 1;
}

MessageBase* CreateMpChatMsg() { return NewOrNull<MpChatMsg>(); }

// ---- MpFlagsMsg (kind 0x16) ----------------------------------------------------------------------------------

int MpFlagsMsg::Apply() {
    SetLobbyFlags(m_flags);
    return 0;
}

void ComputeRectBWidth() { g_rectBWidth = rectWidth(g_rectB); }
void StaticInitRectBWidth() { ComputeRectBWidth(); }

// ---- MpPlayerInfoMsg (kind 0x12) -----------------------------------------------------------------------------

MpPlayerInfoMsg::MpPlayerInfoMsg() : m_action(0), m_playerId(0), m_extra(0) {}

MpPlayerInfoMsg::~MpPlayerInfoMsg() { REB_KEEP_VPTR_STORE(); }

int MpPlayerInfoMsg::Read(istream* in) {
    int result = MessageBase::Read(in);
    StreamRead32(in, &m_action);
    StreamRead32(in, &m_playerId);
    ReadString(in, &m_playerName);
    StreamRead32(in, &m_extra);
    return result;
}

int MpPlayerInfoMsg::Write(ostream* out) {
    int result = MessageBase::Write(out);
    StreamWrite32(out, &m_action);
    StreamWrite32(out, &m_playerId);
    WriteString(out, &m_playerName);
    StreamWrite32(out, &m_extra);
    return result;
}

int MpPlayerInfoMsg::GetKind() { return 0x12; }

int MpPlayerInfoMsg::Apply() {
    if (m_action == kActionSetLocalPlayer) {
        AppSetAutosaveTick(m_extra);
        AppRequestSave(m_playerId, String(m_playerName));
        return 1;
    }
    if (m_action == kActionSetPeerId) {
        AppRequestLoad(m_playerId);
        return 1;
    }
    return 0;
}

MessageBase* CreateMpPlayerInfoMsg() { return NewOrNull<MpPlayerInfoMsg>(); }

HCURSOR g_defaultCursor{};
HHOOK g_mouseHook{};
HHOOK g_tooltipHook{};
HINSTANCE g_stringModule{};
HPALETTE g_displayPalette{};
HPALETTE g_palette{};
HPALETTE g_savedPalette{};
HWND g_tooltipWindow{};
HwndNode * g_captureStack{};
IdAvlTree g_windowRegistry{};
NodeStack g_popupStack{};
const RECT g_rectSessionList = {124, 277, 473, 352};
const char *g_tooltipClassName = nullptr;
const int g_rectA[4] = {144, 125, 500, 222};
const int g_rectB[4] = {144, 125, 500, 222};
int g_isNetworkGame{};
int g_musicVolume{};
int g_rectAWidth{};
int g_rectBWidth{};
uint32_t g_appDialogMessage{};
uint32_t g_lobbyFlags{};
uint32_t g_lobbySlotPollTime{};
uint32_t g_lobbySlotTable[7]{};
