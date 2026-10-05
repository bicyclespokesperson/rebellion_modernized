#include "ai/ai_common.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_task_types.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"

using ai::KeyInFamilies;

namespace {

constexpr int kAdvisorOrderKind = 0x203;
constexpr int kMessagesPerPass = 10;
constexpr int16_t kStringGroup = 2;

constexpr uint32_t kShipEnroute = 0x10;
constexpr uint32_t kShipDamaged = 0x20;
constexpr uint32_t kShipOther = 0x40;
constexpr uint32_t kShipReportable = 0x4000;
constexpr uint32_t kShipUnreportable = 0x8002800;

struct TextTable {
    int16_t firstId;
    int16_t count;
};

struct SideTexts {
    TextTable enroute;
    TextTable damaged;
    TextTable other;
};

constexpr SideTexts kAllianceTexts = {{0x5200, 0x14}, {0x5260, 0x23}, {0x52c0, 0x10}};
constexpr SideTexts kEmpireTexts = {{0x5100, 0x27}, {0x5160, 0x22}, {0x51c0, 0x20}};

void UseText(String* text, const TextTable& table, int16_t* next) {
    const ResourceStringId id{static_cast<int16_t>(*next + table.firstId), kStringGroup};
    text->SetFromResource(&id);
    ++*next;
}

}  // namespace

AiTask14::AiTask14(int view) : AiTaskBase(14, view), m_resume() {
    m_count = 0;
    m_cursor = nullptr;
    m_enrouteTexts = 0;
    m_damagedTexts = 0;
    m_otherTexts = 0;
}

AiTask14::~AiTask14() {
    m_cursor = nullptr;
    REB_KEEP_VPTR_STORE();
}

void AiTask14::Read(istream* in) {
    AiTaskBase::Read(in);
    ReadValue32(in, &m_count);
    m_resume.Read(in);
    ReadValue16(in, &m_enrouteTexts);
    ReadValue16(in, &m_damagedTexts);
    ReadValue16(in, &m_otherTexts);
}

void AiTask14::Write(ostream* out) {
    AiTaskBase::Write(out);
    WriteValue32(out, &m_count);
    m_resume.Write(out);
    WriteValue16(out, &m_enrouteTexts);
    WriteValue16(out, &m_damagedTexts);
    WriteValue16(out, &m_otherTexts);
}

void AiTask14::Evaluate() {
    if (m_flags.Get() != 1 && m_flags.Get() != 2 && 0 < m_owner->Stat(0x264)) {
        m_flags.m_value = 1;
        m_finished = 0;
    }
}

OrderBase* AiTask14::Step() {
    OrderBase* order = nullptr;
    if (m_flags.Get() != 1) {
        m_count = 0;
        m_finished = 1;
        m_cursor = nullptr;
        return order;
    }
    if (m_count >= kMessagesPerPass) {
        m_count = 0;
        m_finished = 1;
        ObjectKey key;
        m_cursor->GetKey(&key);
        m_resume = key;
        m_cursor = nullptr;
        return order;
    }
    if (!m_cursor) {
        if (KeyInFamilies(m_resume, 0x14, 0x1c)) m_cursor = static_cast<AiShipRecord*>(m_owner->m_ships.Find(&m_resume));
        if (!m_cursor) m_cursor = static_cast<AiShipRecord*>(m_owner->m_ships.First());
        return order;
    }
    const uint32_t flags = m_cursor->m_flags.m_value;
    if ((flags & kShipReportable) != 0 && (flags & kShipUnreportable) == 0) {
        order = BuildMessage(m_cursor);
        ++m_count;
    }
    m_cursor = static_cast<AiShipRecord*>(m_cursor->m_successor);
    if (!m_cursor) {
        m_count = 0;
        m_finished = 1;
        m_resume.Reset();
    }
    return order;
}

void AiTask14::Refused(int*, OrderBase* order) {
    if (order) delete order;
    m_priority = 0;
}

OrderBase* AiTask14::BuildMessage(AiShipRecord* record) {
    String text;
    ObjectKeyValueSet team;
    const SideTexts& table = m_view == 1 ? kAllianceTexts : kEmpireTexts;
    const uint32_t flags = record->m_flags.m_value;
    bool chosen = false;
    auto tryTable = [&](const TextTable& entries, int16_t* next, bool allowed) {
        if (chosen || !allowed) return;
        UseText(&text, entries, next);
        chosen = true;
    };
    auto fresh = [](const TextTable& entries, int16_t next) { return next < entries.count; };

    if (flags & kShipEnroute) {
        tryTable(table.enroute, &m_enrouteTexts, fresh(table.enroute, m_enrouteTexts));
    } else if (flags & kShipDamaged) {
        tryTable(table.damaged, &m_damagedTexts, fresh(table.damaged, m_damagedTexts));
        tryTable(table.enroute, &m_enrouteTexts, fresh(table.enroute, m_enrouteTexts));
    } else if (flags & kShipOther) {
        tryTable(table.other, &m_otherTexts, fresh(table.other, m_otherTexts));
        tryTable(table.damaged, &m_damagedTexts, fresh(table.damaged, m_damagedTexts));
        tryTable(table.enroute, &m_enrouteTexts, fresh(table.enroute, m_enrouteTexts));
    }
    if (!chosen) {
        ObjectKey key;
        record->GetKey(&key);
        GameObj* ship = static_cast<GameObj*>(GetObjectInFamilies_14_1c(m_view, &key));
        if (ship) {
            String name;
            ship->GetNameCopy(&name);
            text.Assign(name);
        }
    }
    TextMessage* order = static_cast<TextMessage*>(CreateMessageByKind(kAdvisorOrderKind));
    if (order) {
        ObjectKey key;
        record->GetKey(&key);
        ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(key, 0);
        if (node) {
            team.Add(node);
            order->m_side = m_view;
            order->SetTeam(&team);
            order->m_text.Assign(text);
        }
    }
    return order;
}
