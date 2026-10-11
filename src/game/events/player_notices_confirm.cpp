#include "core/game_string.h"
#include "core/strstream_io.h"
#include "strategy_ui/object_status_helpers.h"
#include "events/player_notice_util.h"
#include "events/player_notices.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/move_orders.h"
#include "ui/message_template.h"

extern const char *g_transitDaysSeparator;  // 0x6a8798 ":  "

namespace {

constexpr int kMoveOrderMessage = 0x201;
constexpr int kSecondMoveOrderMessage = 0x202;

uint16_t sideVariant(int owner, uint16_t base) { return static_cast<uint16_t>((owner != 1) + base); }

}  // namespace

// The confirmation dialog of an order about to be sent: the text and pictures depend on the kind of order.
void Notice8::BuildText() {
    m_text0 = g_emptyText;
    OrderBase* order = static_cast<OrderBase*>(m_subject);
    switch (m_subject->GetKind()) {
    case 0x200: {
        m_sound[0] = sideVariant(m_owner, 0x461);
        ExpandMessageTemplate(&m_text1, 0x7050, nullptr, nullptr, nullptr, nullptr);
        ObjectKeyValueSet dependencies;
        order->CollectTeamDependencies(&dependencies);
        for (ObjectKeyNode* node = dependencies.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key(node->m_key);
            GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, &key);
            if (object) AppendName(object);
        }
        m_picture[1] = sideVariant(m_owner, 0x408);
        return;
    }
    case kMoveOrderMessage:
    case kSecondMoveOrderMessage:
        break;
    default:
        m_text0 = g_emptyText;
        m_text1 = g_emptyText;
        return;
    case 0x213: {
        m_sound[0] = 0x465;
        ExpandMessageTemplate(&m_text1, 0x7053, nullptr, nullptr, nullptr, nullptr);
        ObjectKeyValueSet* team = order->GetTeam();
        for (ObjectKeyNode* node = team->First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key(node->m_key);
            GameObjNotifier* object = GetObjectInFamilies_a0_b0(m_owner, &key);
            ExpandMessageTemplate(&m_text1, 0x7055, notice::nameOf(object), nullptr, nullptr, nullptr);
        }
        m_picture[1] = sideVariant(m_owner, 0x40e);
        return;
    }
    case 0x242: {
        m_sound[0] = sideVariant(m_owner, 0x461);
        ExpandMessageTemplate(&m_text1, 0x7051, nullptr, nullptr, nullptr, nullptr);
        ObjectKeyValueSet* team = order->GetTeam();
        for (ObjectKeyNode* node = team->First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key(node->m_key);
            GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, &key);
            if (object) ExpandMessageTemplate(&m_text1, 0x7054, notice::nameOf(object), nullptr, nullptr, nullptr);
        }
        m_picture[1] = sideVariant(m_owner, 0x40a);
        return;
    }
    case 0x250:
        ExpandMessageTemplate(&m_text1, 0x7052, nullptr, nullptr, nullptr, nullptr);
        m_sound[0] = sideVariant(m_owner, 0x463);
        m_picture[1] = sideVariant(m_owner, 0x40c);
        return;
    }
    GameObjNotifier* first = nullptr;
    GameObjNotifier* system = nullptr;
    bool blockaded = false;
    ObjectKeyNode* firstNode = order->GetTeam()->First();
    if (firstNode) {
        ObjectKey key(firstNode->m_key);
        first = GetObjectInFamilies_01_ff(m_owner, &key);
    }
    if (first) {
        const uint32_t family = first->GetFamily();
        if (family < 8 || family >= 0x10) {
            GameObjNotifier* parent = first->m_parent;
            const uint32_t parentFamily = parent->GetFamily();
            if (parentFamily >= 0x90 && parentFamily < 0x98) system = parent;
        }
    }
    if (system && (static_cast<System*>(system)->m_flags & system_flag::kBlockade) != 0) blockaded = true;
    if (!blockaded) {
        ExpandMessageTemplate(&m_text1, 0x7057, nullptr, nullptr, nullptr, nullptr);
        m_picture[1] = sideVariant(m_owner, 0x3fa);
    } else {
        ExpandMessageTemplate(&m_text1, 0x7056, nullptr, nullptr, nullptr, nullptr);
        m_picture[1] = static_cast<uint16_t>(0x407 - (m_owner != 1));
    }
    AppendOrderLines();
}

// Adds one line per character of the move order: its name and the days it needs.
void Notice8::AppendOrderLines() {
    StrStream stream;
    strstream_ctor(&stream, 1);
    {
        ObjectKeyValueSet transitDays;
        StatusCode status;
        static_cast<MoveOrderBase*>(m_subject)->CollectTransitDays(&status, &transitDays);
        for (ObjectKeyNode* node = transitDays.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key(node->m_key);
            GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, &key);
            if (!object) continue;
            const int days = static_cast<ObjectKeyValueNode*>(node)->m_value;
            String name;
            static_cast<GameObj*>(object)->GetNameCopy(&name);
            ostream* out = ostream_operator_ls_char(stream.Out(), '\n');
            out = ostream_operator_ls_cstr(out, name.m_text);
            out = ostream_operator_ls_cstr(out, g_transitDaysSeparator);
            ostream_operator_ls_long(out, days);
        }
        ostream_operator_ls_char(stream.Out(), 0);
        {
            String text(strstreambuf_str(StreambufOf(&stream)));
            m_text1 += text;
        }
        strstreambuf_freeze(StreambufOf(&stream), 0);
        ClearIosState(&stream);
    }
    strstream_dtor(stream.Ios());
    ios_dtor(stream.Ios());
}

const char *g_noText = nullptr;
const char *g_spaceString = nullptr;
const char *g_statusSeparator = nullptr;
const char *g_transitDaysSeparator = nullptr;
const char *g_yesText = nullptr;
const int16_t g_stringGroup65d424 = 2;
