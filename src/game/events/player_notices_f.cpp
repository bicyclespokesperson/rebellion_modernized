#include "events/player_notices.h"

#include <new>

#include "core/checked_new.h"
#include "ui/message_template.h"
#include "character/character.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notifs_ext.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/game_ui_facade.h"


// ---- Notice19 (kind 19) ----

Notice19::Notice19() : UiTextNotice() {
    m_categories = Virtual11();
    m_f68 = 0;
    m_unk40 = 0;
}

Notice19::Notice19(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    m_f68 = 0;
    Virtual16(payloads);
}

Notice19::~Notice19() = default;

int Notice19::GetKind() { return 19; }

UiNotice* Notice19::Virtual10() { return NewOrNull<Notice19>(); }

void Notice19::OnMessage(int kind, intptr_t a, intptr_t b) {
    if (kind != 0x340) {
        UiNotice::OnMessage(kind, a, b);
        return;
    }
    GameObjNotifier* first = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(a));
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, reinterpret_cast<const ObjectKey*>(b));
    m_key = ObjectKey(*reinterpret_cast<const ObjectKey*>(b));
    if (first && system) {
        if (m_f68 == 0) {
            m_unk40 = 1;
            m_f68 = 1;
            ExpandMessageTemplate(&m_text0, 0x70a8, notice::nameOf(first), notice::nameOf(system), nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x70a9, notice::nameOf(first), notice::nameOf(system), nullptr, nullptr);
            m_unk28 = 0x10;
            if (m_owner == 1) {
                m_picture[1] = 0x407;
                m_sound[0] = 0x460;
            } else {
                m_picture[1] = 0x406;
                m_sound[0] = 0x45f;
            }
        }
        if (m_unk40 != 0)
            ExpandMessageTemplate(&m_text1, 0x70aa, notice::nameOf(first), notice::nameOf(system), nullptr, nullptr);
    }
}

void Notice19::Read(istream* in) {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f68);
}

void Notice19::Write(ostream* out) {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f68);
}

// ---- Notice7 (kind 7) ----

Notice7::Notice7() : UiTextNotice() {
    m_categories = Virtual11();
}

Notice7::Notice7(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual16(payloads);
}

Notice7::~Notice7() = default;

UiNotice* Notice7::Virtual10() { return NewOrNull<Notice7>(); }

void Notice7::Virtual08(int kind, intptr_t a, intptr_t b) {
    bool sideMatch = false;
    if (kind != 0x14e) return;
    const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, key);
    GameObjNotifier* other = nullptr;
    GameObjNotifier* fleet = nullptr;
    m_key = ObjectKey(*key);
    if (!system || b == 0) {
        m_unk40 = 0;
    } else {
        FleetIterator it(system);
        it.Begin();
        fleet = it.Current();
        if (!fleet) m_unk40 = 0;
        uint32_t controller = system->ControlSide();
        if (controller == 1) {
            other = GetSideObject(m_owner, 2);
            sideMatch = m_owner == 1;
            m_picture[1] = 0x404;
        } else if (controller == 2) {
            other = GetSideObject(m_owner, 1);
            sideMatch = m_owner == 2;
            m_picture[1] = 0x403;
        } else {
            sideMatch = false;
            other = nullptr;
            m_unk40 = 0;
        }
        if ((static_cast<System*>(system)->m_flags & 0x4000) == 0)
            m_unk40 = 1;
        else
            m_unk40 = 0;
    }
    if (!other) m_unk40 = 0;
    if (m_unk40 != 0) {
        m_sound[0] = 0x45e;
        if (sideMatch) {
            ExpandMessageTemplate(&m_text0, 0x70a2, notice::nameOf(system), nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x70a3, notice::nameOf(other), notice::nameOf(system), notice::nameOf(fleet), nullptr);
            m_unk28 = 0xe;
        } else {
            ExpandMessageTemplate(&m_text0, 0x70a0, notice::nameOf(system), nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x70a1, notice::nameOf(other), notice::nameOf(system), notice::nameOf(fleet), nullptr);
            m_unk28 = 0xd;
        }
    }
}
