// MultiPlayerWindow, part 3: the lobby protocol (settings, chat, slot table) and its helpers.
#include <cstdlib>
#include <cstring>
#include <new>

#include "app/app_control_cmds.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "multiplayer/comm_mgr.h"
#include "multiplayer/message_codec.h"
#include "multiplayer/multiplayer_window.h"
#include "orders/order_base.h"
#include "tactical/comm2.h"
#include "ui/cool_button.h"
#include "ui/cool_string_field.h"
#include "ui/text_scroll_field.h"

void ShowGameVersionOlderError();      // 0x406800
void ShowOpponentVersionOlderError();  // 0x406820

namespace {

constexpr int kLobbyMessageCategory = 1;
constexpr int kVoteMessageCategory = 4;
constexpr int kSettingsKind = 0x10;
constexpr int kChatKind = 0x11;
constexpr int kPlayerInfoKind = 0x12;
constexpr int kSlotTableKind = 0x14;
constexpr int kFlagsKind = 0x16;
constexpr int kSlotsInTable = 5;
constexpr uint32_t kStatusLinePrefix = 0x17;
constexpr char kMarkerWithVersion[] = "\x17" "0100";
constexpr char kVersionText[] = "0100";
constexpr char kStatusMarker[] = "\x17" "4";
constexpr char kLineEnd[] = "\n";
constexpr char kSideA[] = "\x17" "10";
constexpr char kSideB[] = "\x17" "11";
constexpr char kSize1[] = "\x17" "20";
constexpr char kSize2[] = "\x17" "21";
constexpr char kSize3[] = "\x17" "22";
constexpr char kType1[] = "\x17" "30";
constexpr char kType2[] = "\x17" "31";
constexpr char kCancelTrace[] = "MultiPlayerWindow::DoCancelMultiPlayer(): About to call PlayerQuitSession()!\n";
constexpr int kStateMainMenu = 3;
constexpr int kStateLobbyLaunched = 0xe;
constexpr int kStateStartGame = 7;
constexpr int kPlayerStateConnected = 7;
constexpr UINT kNoticeMessage = 0x46e;
constexpr DWORD kStartWaitMs = 7000;
constexpr DWORD kSlotPollIntervalMs = 1000;

void SendMessageNow(MessageBase* message) {
    MessageCodec codec;
    codec.Serialize(message);
    if (codec.m_valid) {
        CommSendMessage(codec.m_buffer, codec.m_size, kLobbyMessageCategory, 1);
        CommFlushAll();
    }
}

MessageBase* TakeLobbyMessage(int firstCategory) {
    MessageBase* result = nullptr;
    CommBuffer* buffer = CommTakeMessage(firstCategory);
    if (!buffer) {
        buffer = CommTakeMessage(kVoteMessageCategory);
        if (!buffer) return result;
    }
    MessageCodec codec;
    codec.SetData(buffer->m_data, buffer->m_size);
    codec.Deserialize(&result);
    if (buffer->m_data) ::operator delete(buffer->m_data);
    buffer->m_data = nullptr;
    ::operator delete(buffer);
    return result;
}

void AppendMarker(String* text, const char* marker) {
    const String piece(marker);
    *text += piece;
}

// Leaves the lobby for the game that was agreed on, after the host had every acknowledgement (or 7 seconds).
void LaunchGame(MultiPlayerWindow* window) {
    window->m_closing = 1;
    KillTimer(window->m_hwnd, kLobbyTimerSessions);
    AppSetGameParams(window->m_side, 2, window->m_galaxySize, window->m_gameSeed, window->m_gameType,
                     kPlayerStateConnected);
    AppSetUnknown138(window->m_isHost);
    CommCloseSessionToNewPlayers(window);
    if (window->m_isHost != 0) {
        window->SendSettings(1);
        const DWORD start = window->m_lastPollTime;
        DWORD elapsed = timeGetTime() - start;
        while (elapsed < kStartWaitMs && CommAllPacketsAcknowledged() == 0) elapsed = timeGetTime() - start;
    }
    KillTimer(window->m_hwnd, kLobbyTimerNetwork);
    window->m_closing = 1;
    AppSetState(kStateStartGame);
}

}  // namespace

// ---- helpers shared with the other modules ----------------------------------------------------------------------

String* __thiscall GetMessageText(TextMessage* message, String* out) {  // 0x411710
    new (out) String(message->m_text);
    return out;
}

uint32_t GetSlotSaveId(int slot) {
    uint32_t id = 0;
    if (slot < 7) {
        SavedGameInfo header = AppReadSaveHeader(slot, 1);
        id = static_cast<uint32_t>(header.m_unknown24);
    }
    return id;
}

int IsSlotAssigned(int slot) {
    bool assigned = false;
    const uint32_t saved = GetSlotSaveId(slot);
    uint32_t announced = 0;
    if (slot < 7) announced = g_lobbySlotTable[slot];
    if (saved != 0 && announced != 0) assigned = saved == announced;
    return assigned;
}

void SetLobbyFlags(uint32_t flags) { g_lobbyFlags = flags; }

void SetSlotValue(int slot, uint32_t value) {
    if (slot < 7) g_lobbySlotTable[slot] = value;
}

void BroadcastSlotTable() {
    MpSlotTableMsg* message = NewOrNull<MpSlotTableMsg>();
    if (!message) return;
    for (int slot = 1; slot < 6; ++slot) {
        uint32_t value = 0;
        if (slot < 7) {
            SavedGameInfo header = AppReadSaveHeader(slot, 1);
            value = static_cast<uint32_t>(header.m_unknown24);
        }
        message->m_values[slot - 1] = static_cast<int>(value);
    }
    SendMessageNow(message);
    delete message;
}

MessageBase* ReceiveLobbyMessage() { return TakeLobbyMessage(kLobbyMessageCategory); }

int ParseLobbyText(const String* text, String* out, int stringGroup) {
    int result = 1;
    const char* source = text->m_text;
    out->Clear();
    if (*source != '\x17') return result;
    char versionText[5];
    strncpy(versionText, source + 1, 4);
    versionText[4] = 0;
    const int theirVersion = atoi(versionText);
    const int myVersion = atoi(kVersionText);
    if (theirVersion != myVersion) return myVersion < theirVersion ? 2 : 1;
    uint32_t index = 5;
    for (char c = source[5]; c != 0; c = source[index & 0xffff]) {
        const char* position = source + (index & 0xffff);
        if (*position == '\x17') {
            String piece;
            uint32_t next = index + 1;
            ResourceStringId id = {0, static_cast<int16_t>(stringGroup)};
            switch (source[next & 0xffff]) {
            case '1':
                next = index + 2;
                if (source[next & 0xffff] == '0') {
                    id.m_id = 0x1150;
                } else if (source[next & 0xffff] == '1') {
                    id.m_id = 0x1151;
                }
                break;
            case '2':
                next = index + 2;
                if (source[next & 0xffff] == '0') {
                    id.m_id = 0x1152;
                } else if (source[next & 0xffff] == '1') {
                    id.m_id = 0x1153;
                } else if (source[next & 0xffff] == '2') {
                    id.m_id = 0x1154;
                }
                break;
            case '3':
                next = index + 2;
                if (source[next & 0xffff] == '0') {
                    id.m_id = 0x1155;
                } else if (source[next & 0xffff] == '1') {
                    id.m_id = 0x1156;
                }
                break;
            case '4':
                id.m_id = 0x1158;
                result = 3;
                break;
            }
            if (id.m_id != 0) piece.SetFromResource(&id);
            *out += piece;
            index = next;
        } else {
            out->AppendChar(*position);
        }
        ++index;
    }
    if (result == 1) result = 0;
    return result;
}

// ---- the lobby protocol -----------------------------------------------------------------------------------------

void MultiPlayerWindow::SendToPeer(MessageBase* message) {
    if (!message) return;
    SendMessageNow(message);
    delete message;
}

void MultiPlayerWindow::SendSettings(int startGame) {
    auto* message = NewOrNull<MpLobbySettingsMsg>(static_cast<int>(m_gameSeed), 2 - (m_side != 1), m_galaxySize,
                                                  m_gameType, startGame);
    if (!message) return;
    SendToPeer(message);
}

void MultiPlayerWindow::AnnounceSettings() {
    String text;
    if (m_side == 1) {
        AppendMarker(&text, kSideA);
    } else if (m_side == 2) {
        AppendMarker(&text, kSideB);
    }
    if (m_galaxySize == 1) {
        AppendMarker(&text, kSize1);
    } else if (m_galaxySize == 2) {
        AppendMarker(&text, kSize2);
    } else if (m_galaxySize == 3) {
        AppendMarker(&text, kSize3);
    }
    if (m_gameType == 1) {
        AppendMarker(&text, kType1);
    } else if (m_gameType == 2) {
        AppendMarker(&text, kType2);
    }
    PrintChatLine(&text);
}

void MultiPlayerWindow::PrintChatLine(const String* text) {
    String marker(kMarkerWithVersion);
    String line;
    line = kLineEnd;
    if (!text) {
        const String* typed = m_chatInput ? &m_chatInput->m_text : nullptr;
        line.Insert(*typed, 0);
        if (m_chatInput) m_chatInput->Clear();
    } else {
        line.Insert(*text, 0);
    }
    ResourceStringId separatorId = {0x1157, static_cast<int16_t>(m_stringSlot)};
    String separator(&separatorId);
    line.Insert(separator, 0);
    const String* playerName = nullptr;
    switch (m_isHost == 0) {
    case false:
        playerName = m_hostNameField ? &m_hostNameField->m_text : nullptr;
        break;
    case true:
        playerName = m_joinNameField ? &m_joinNameField->m_text : nullptr;
    }
    line.Insert(playerName ? *playerName : *text, 0);
    auto* message = NewOrNull<MpChatMsg>();
    if (!m_chatLog) return;
    String parsed;
    line.Insert(marker, 0);
    ParseLobbyText(&line, &parsed, static_cast<int>(m_stringSlot));
    m_chatLog->InsertText(parsed, 0);
    message->m_text.Assign(line);
    if (message) SendToPeer(message);
}

void MultiPlayerWindow::HandleChat(TextMessage* message) {
    if (!message) return;
    String text;
    GetMessageText(message, &text);
    String parsed;
    const int result = ParseLobbyText(&text, &parsed, static_cast<int>(m_stringSlot));
    bool skipInsert = false;
    if (result != 0) {
        if (result != 3) {
            UpdateStartButtons();
            CommClearLobbyLaunch();
            CommCleanUp();
            OnCreate1();
            if (result == 1) ShowOpponentVersionOlderError();
            if (result == 2) ShowGameVersionOlderError();
            if (CommLaunchedFromLobby() == 0) {
                ReturnToProviderPage();
            } else {
                CommTrace(kCancelTrace);
                CommSendPlayerQuit();
                m_closing = 1;
                KillTimer(m_hwnd, kLobbyTimerSessions);
                KillTimer(m_hwnd, kLobbyTimerNetwork);
                CommCleanUp();
                g_isNetworkGame = 0;
                const int launched = CommLaunchedFromLobby();
                m_closing = 1;
                AppSetState(launched == 0 ? kStateMainMenu : kStateLobbyLaunched);
            }
        } else if ((g_lobbyFlags & 1) != 0) {
            skipInsert = true;
        }
    }
    if (skipInsert) return;
    if (m_chatLog) m_chatLog->InsertText(parsed, 0);
    if (result == 3) g_lobbyFlags |= 1;
}

void MultiPlayerWindow::PrintStatusMarker() {
    String marker(kStatusMarker);
    PrintChatLine(&marker);
}

void MultiPlayerWindow::ProcessNetwork() {
    const uint32_t playerCount = CommGetPlayerCount();
    if (m_closing != 0 || m_ready != 1) return;
    m_lastPlayerCount = playerCount;
    MessageBase* message = ReceiveLobbyMessage();
    if (!message) return;
    switch (message->GetKind()) {
    case kSettingsKind: {
        m_lastPollTime = timeGetTime();
        if (m_isHost == 0) {
            auto* settings = static_cast<MpLobbySettingsMsg*>(message);
            SetGameType(settings->m_setting128, 0, 0);
            SetSide(settings->m_setting120, 0, 0);
            SetGalaxySize(settings->m_setting124, 0, 0);
            m_gameSeed = static_cast<uint32_t>(settings->m_setting130);
            if (settings->m_startGame != 0) {
                LaunchGame(this);
                delete message;
                return;
            }
        }
        break;
    }
    case kPlayerInfoKind:
        m_closing = 1;
        static_cast<MpPlayerInfoMsg*>(message)->Apply();
        delete message;
        return;
    case kChatKind:
        HandleChat(static_cast<TextMessage*>(message));
        delete message;
        return;
    case kSlotTableKind:
        if (m_isHost == 0) {
            PrintStatusMarker();
            BroadcastSlotTable();
            delete message;
            return;
        }
        static_cast<MpSlotTableMsg*>(message)->Apply();
        UpdateStartButtons();
        if ((g_lobbyFlags & 2) == 0) AnnounceSettings();
        g_lobbySlotPollTime = 0xffffffff;
        g_lobbyFlags |= 2;
        delete message;
        return;
    case kFlagsKind:
        static_cast<MpFlagsMsg*>(message)->Apply();
        break;
    }
    delete message;
}

void MultiPlayerWindow::PollSlotTable() {
    if (!m_loadButton || !m_startButton) return;
    const DWORD now = timeGetTime();
    if (g_lobbySlotPollTime > now - kSlotPollIntervalMs) return;
    const uint32_t playerCount = CommGetPlayerCount();
    if (m_isHost == 0) return;
    if (playerCount == 2) {
        if (m_startButton->HasStateFlags(kButtonDisabled) == 0) {
            g_lobbySlotPollTime = 0xffffffff;
            return;
        }
        MpSlotTableMsg* message = NewOrNull<MpSlotTableMsg>();
        if (message) {
            SendMessageNow(message);
            delete message;
        }
    }
    g_lobbySlotPollTime = timeGetTime();
}
