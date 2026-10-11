#include "core/game_string.h"
#include "strategy_ui/advisor_notices.h"

#include <new>

#include "audio/sound_manager.h"
#include "events/game_clock_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/notif_payload.h"
#include "platform/win32.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_window.h"


namespace {

constexpr int kAdviceResourceModule = 9;
constexpr uint32_t kRcdata = 10;
constexpr int kAdviceSoundModule = 9;
constexpr uint32_t kStaleTicks = 300;
constexpr uint32_t kNoticeFlagUi = 0x10;
constexpr int kUiMessageForward = 0x302;
constexpr unsigned kRecordTypeMask = 0xfff;
constexpr unsigned kRecordTypeResourceBase = 0x1000;

}  // namespace

// ---- UiNotice -------------------------------------------------------------------------------------------------

UiNotice::UiNotice() : IdNode(), m_flags(0) {
    m_unk1c = 0;
    m_owner = 0;
    m_created = GameHookTickCount();
    m_unk40 = 1;
    m_unk28 = 0;
    m_picture[0] = 0;
    m_picture[1] = 0;
    m_sound[0] = 0;
    m_sound[1] = 0;
    m_categories = 0;
    m_flags.m_value |= kNoticeFlagUi;
}

UiNotice::UiNotice(int owner) : IdNode(), m_flags(0) {
    m_unk1c = 0;
    m_owner = owner;
    m_created = GameHookTickCount();
    m_unk28 = 0;
    m_unk40 = 1;
    m_picture[0] = 0;
    m_picture[1] = 0;
    m_sound[0] = 0;
    m_sound[1] = 0;
    m_categories = 0;
    m_flags.m_value |= kNoticeFlagUi;
}

UiNotice::~UiNotice() = default;

void UiNotice::Read(istream* in) {
    ReadValue32(in, &m_unk1c);
    ReadValue32(in, &m_owner);
    ReadValue32(in, &m_created);
    ReadValue16(in, &m_unk3c);
    ReadValue32(in, &m_unk40);
    ReadValue32(in, &m_unk28);
    ReadValue16(in, &m_picture[0]);
    ReadValue16(in, &m_picture[1]);
    ReadValue16(in, &m_sound[0]);
    ReadValue16(in, &m_sound[1]);
    ReadValue32(in, &m_categories);
    m_flags.Read(in);
}

void UiNotice::Write(ostream* out) {
    WriteValue32(out, &m_unk1c);
    WriteValue32(out, &m_owner);
    WriteValue32(out, &m_created);
    WriteValue16(out, &m_unk3c);
    WriteValue32(out, &m_unk40);
    WriteValue32(out, &m_unk28);
    WriteValue16(out, &m_picture[0]);
    WriteValue16(out, &m_picture[1]);
    WriteValue16(out, &m_sound[0]);
    WriteValue16(out, &m_sound[1]);
    WriteValue32(out, &m_categories);
    m_flags.Write(out);
}

int UiNotice::IsStale() { return m_created + kStaleTicks < static_cast<uint32_t>(GameHookTickCount()); }

void UiNotice::OnMessage(int message, intptr_t a, intptr_t b) {
    if (message == kUiMessageForward) UiSendNotificationToSide(m_owner, a);
}

// ---- UiTextNotice / UiKeyNotice --------------------------------------------------------------------------------

UiTextNotice::UiTextNotice() : UiNotice() { m_target.m_value = 0; }

UiTextNotice::UiTextNotice(int owner) : UiNotice(owner) { m_target.m_value = 0; }

UiTextNotice::~UiTextNotice() = default;

void UiTextNotice::Read(istream* in) {
    UiNotice::Read(in);
    ReadLegacyString(in, m_text0);
    ReadLegacyString(in, m_text1);
    m_key.Read(in);
    m_target.Read(in);
}

void UiTextNotice::Write(ostream* out) {
    UiNotice::Write(out);
    WriteLegacyString(out, m_text0);
    WriteLegacyString(out, m_text1);
    m_key.Write(out);
    m_target.Write(out);
}

void UiTextNotice::Virtual15() { Virtual13(); }

int ForwardPayloads(UiNotice* sink, const LinkNode* payloads) {
    if (!payloads) return 0;
    for (const LinkNode* node = payloads->m_next; node; node = node->m_next)
        static_cast<NotifPayload*>(const_cast<LinkNode*>(node))->DispatchToSink(sink);
    return 1;
}

int UiTextNotice::Virtual16(const LinkNode* payloads) { return ForwardPayloads(this, payloads); }

int UiKeyNotice::Virtual14(const LinkNode* payloads) { return ForwardPayloads(this, payloads); }

UiKeyNotice::UiKeyNotice() : UiNotice() {}

UiKeyNotice::UiKeyNotice(int owner) : UiNotice(owner) {}

UiKeyNotice::~UiKeyNotice() = default;

void UiKeyNotice::Read(istream* in) {
    UiNotice::Read(in);
    ReadLegacyString(in, m_text0);
    ReadLegacyString(in, m_text1);
    ReadValue32(in, &m_value);
    m_key.Read(in);
}

void UiKeyNotice::Write(ostream* out) {
    UiNotice::Write(out);
    WriteLegacyString(out, m_text0);
    WriteLegacyString(out, m_text1);
    WriteValue32(out, &m_value);
    m_key.Write(out);
}

// ---- DroidScript ----------------------------------------------------------------------------------------------

DroidScript::DroidScript() : IdNode() {
    m_unk1c = 0;
    m_entryCount = 0;
    m_cursor = 0;
    m_entries = nullptr;
}

DroidScript::~DroidScript() {
    if (m_entries) ::operator delete(m_entries);

}

void DroidScript::Read(istream* in) {
    ReadId(in);
    ReadValue32(in, &m_unk1c);
    ReadValue32(in, &m_entryCount);
    ReadValue32(in, &m_cursor);
    m_entries = static_cast<DroidScriptEntry*>(::operator new(m_entryCount * sizeof(DroidScriptEntry)));
    for (uint32_t i = 0; i < m_entryCount; ++i) {
        ReadValue16(in, &m_entries[i].m_id);
        ReadValue32(in, &m_entries[i].m_flags);
        ReadValue32(in, &m_entries[i].m_x);
        ReadValue32(in, &m_entries[i].m_y);
    }
}

void DroidScript::Write(ostream* out) {
    WriteId(out);
    WriteValue32(out, &m_unk1c);
    WriteValue32(out, &m_entryCount);
    WriteValue32(out, &m_cursor);
    for (uint32_t i = 0; i < m_entryCount; ++i) {
        WriteValue16(out, &m_entries[i].m_id);
        WriteValue32(out, &m_entries[i].m_flags);
        WriteValue32(out, &m_entries[i].m_x);
        WriteValue32(out, &m_entries[i].m_y);
    }
}

DroidScriptEntry* DroidScript::NextEntry() {
    if (m_unk1c != 0 && m_cursor < m_entryCount) return &m_entries[m_cursor++];
    m_cursor = 0;
    return nullptr;
}

// ---- Free functions -------------------------------------------------------------------------------------------

uint16_t AdvisorPickAdviceLine(uint32_t adviceId) {
    const uint16_t* table = nullptr;
    uint16_t line = 0;
    HMODULE module = FindResourceModule(kAdviceResourceModule);
    HRSRC info = FindResourceA(module, MAKEINTRESOURCEA(adviceId & 0xffff), MAKEINTRESOURCEA(kRcdata));
    HGLOBAL resource = reinterpret_cast<HGLOBAL>(static_cast<uintptr_t>(adviceId));
    if (info && (resource = LoadResource(module, info)) != nullptr) table = static_cast<const uint16_t*>(LockResource(resource));
    if (!table) return line;
    const uint16_t count = table[0];
    if (count != 0) {
        if (count > 1) {
            line = table[UiSessionRandom(count) + 1];
            FreeResource(resource);
            return line;
        }
        line = table[1];
    }
    FreeResource(resource);
    return line;
}

SoundRequest* AdvisorPlayAdvice(SoundRequest* result, uint32_t adviceId) {
    SoundRequest request(0, 0, 0, 0, 0, 0);
    request.m_kind = 1;
    request.m_soundId = adviceId & 0xffff;
    request.m_moduleIndex = kAdviceSoundModule;
    SoundManager::Play(&request);
    return new (result) SoundRequest(request);
}

uint16_t AdvisorRecordTypeResource(GameObj* object) {
    if (!object) return 0;
    return static_cast<uint16_t>((object->GetRecordTypeBits() & kRecordTypeMask) + kRecordTypeResourceBase);
}
