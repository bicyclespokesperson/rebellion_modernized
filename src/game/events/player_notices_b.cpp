#include "events/player_notices.h"

#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "ui/message_template.h"
#include "character/character.h"
#include "character/role.h"
#include "events/player_notice_util.h"
#include "events/ui_notifs_ext.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "story/story_characters.h"
#include "strategy_ui/game_ui_facade.h"

namespace {

const uint32_t kLukeKey = 0x32000242;
const uint32_t kLeiaKey = 0x31000241;
const uint32_t kEmperorKey = 0x35000281;
const uint32_t kVaderKey = 0x34000280;

bool dagobahDone(GameObjNotifier* luke) { return (static_cast<Luke*>(luke)->m_storyFlags & Luke::kDagobahCompleted) != 0; }

}  // namespace

// ---- Notice40 (kind 40) ----

Notice40::Notice40() : UiTextNotice() {
    m_f64 = 0;
    m_unk40 = 0;
}

Notice40::Notice40(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice40::~Notice40() { REB_KEEP_VPTR_STORE(); }

int Notice40::GetKind() { return 40; }

UiNotice* Notice40::Virtual10() { return NewOrNull<Notice40>(); }

void Notice40::Virtual05(int kind, intptr_t payload) {
    if (kind != 0x200) return;
    const UiNotif200* notif = reinterpret_cast<const UiNotif200*>(payload);
    ObjectKey characterKey(notif->m_key);
    ObjectKey missionKey(notif->m_missionKey);
    GameObjNotifier* character = GetObjectInFamilies_01_ff(m_owner, &characterKey);
    if (m_f64 == 0) {
        m_f64 = 1;
        m_picture[1] = (m_owner != 1) + 0x412;
        m_sound[0] = (m_owner != 1) + 0x47c;
        m_subjectKey = ObjectKey(characterKey);
    }
    if (m_owner == 2) {
        m_unk40 = 0;
    } else if (notice::inFamilies(characterKey, 0x30, 0x3c)) {
        if (notif->m_valueB == 0)
            m_unk40 = 0;
        else if (missionKey.Family() == 100)
            m_unk40 = 1;
    } else {
        m_unk40 = 0;
    }
    if (m_unk40 != 0 && notif->m_leadsMission != 0) {
        m_subjectKey = ObjectKey(characterKey);
        m_key = ObjectKey(characterKey);
        if (notice::inFamilies(m_subjectKey, 0x30, 0x38)) {
            switch (m_subjectKey.m_value & 0xffffff) {
            case 0x240:
                m_unk28 = 0x17;
                break;
            case 0x241:
                m_unk28 = 0x18;
                m_sound[1] = AdvisorPickAdviceLine(0xe5);
                break;
            case 0x242:
                m_unk28 = 0x16;
                m_sound[1] = AdvisorPickAdviceLine(dagobahDone(character) ? 0xdb : 0xd1);
                break;
            case 0x243:
                m_unk28 = 0x19;
                break;
            case 0x280:
                m_unk28 = 0x1a;
                break;
            case 0x281:
                m_unk28 = 0x1b;
                break;
            }
        } else {
            m_unk28 = 0x15;
        }
        ExpandMessageTemplate(&m_text0, 0x71ea, notice::nameOf(character), nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71eb, notice::nameOf(character), nullptr, nullptr, nullptr);
        uint32_t family = character->GetFamily();
        if (family >= 0x30 && family < 0x3c) m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(character));
    }
}

void Notice40::Read(istream* in) {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
    m_subjectKey.Read(in);
}

void Notice40::Write(ostream* out) {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
    m_subjectKey.Write(out);
}

// ---- Notice39 (kind 39) ----

Notice39::Notice39() : UiTextNotice() {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice39::Notice39(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice39::~Notice39() { REB_KEEP_VPTR_STORE(); }

int Notice39::GetKind() { return 39; }

UiNotice* Notice39::Virtual10() { return NewOrNull<Notice39>(); }

void Notice39::Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) {
    if (kind != 0x361) return;
    const ObjectKey* keyA = reinterpret_cast<const ObjectKey*>(a);
    const ObjectKey* keyC = reinterpret_cast<const ObjectKey*>(c);
    GameObjNotifier* first = nullptr;
    GameObjNotifier* second = nullptr;
    GameObjNotifier* otherSide = GetSideObject(m_owner, 2 - (m_owner != 1));
    bool characters = notice::inFamilies(*keyA, 0x30, 0x3c) && notice::inFamilies(*keyC, 0x30, 0x3c);
    if (characters) {
        m_key = ObjectKey(*keyA);
        first = GetObjectInFamilies_30_3c(m_owner, keyA);
        second = GetObjectInFamilies_30_3c(m_owner, keyC);
        if (first && second && otherSide) m_unk40 = 1;
        bool keep;
        if (m_owner == 1)
            keep = static_cast<Character*>(first)->GetRecordSide() != 2;
        else
            keep = m_owner == 2 && static_cast<Character*>(first)->GetRecordSide() != 1;
        if (!keep) m_unk40 = 0;
    }
    if (m_unk40 != 0) {
        m_picture[1] = (m_owner != 1) + 0x412;
        m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(first));
        m_sound[0] = (m_owner != 1) + 0x47c;
        ExpandMessageTemplate(&m_text0, 0x71e8, notice::nameOf(first), notice::nameOf(second), nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71e9, notice::nameOf(first), notice::nameOf(second), notice::nameOf(otherSide), nullptr);
        if (notice::inFamilies(*keyA, 0x30, 0x38)) {
            switch (keyA->m_value & 0xffffff) {
            case 0x240:
                m_unk28 = 0x17;
                break;
            case 0x241:
                m_unk28 = 0x18;
                m_sound[1] = AdvisorPickAdviceLine(0x127);
                break;
            case 0x242:
                m_unk28 = 0x16;
                m_sound[1] = AdvisorPickAdviceLine(dagobahDone(first) ? 0x126 : 0x125);
                break;
            case 0x243:
                m_unk28 = 0x19;
                break;
            case 0x280:
                m_unk28 = 0x1a;
                m_sound[1] = AdvisorPickAdviceLine(0x129);
                break;
            case 0x281:
                m_unk28 = 0x1b;
                m_sound[1] = AdvisorPickAdviceLine(0x128);
                break;
            }
        } else {
            m_unk28 = 0x15;
        }
    }
}

// ---- Notice38 (kind 38) ----

Notice38::Notice38() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
    m_unk40 = 0;
}

Notice38::Notice38(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice38::~Notice38() { REB_KEEP_VPTR_STORE(); }

int Notice38::GetKind() { return 38; }

UiNotice* Notice38::Virtual10() { return NewOrNull<Notice38>(); }

void Notice38::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind == 0x230 && m_f64 == 0 && b != 0) {
        m_f64 = 1;
        if (m_owner == 2) m_unk40 = 1;
        if (m_unk40 != 0) {
            m_key.m_value = kVaderKey;
            m_picture[1] = 0x42c;
            m_unk28 = 0x1a;
            m_sound[0] = 0x47b;
            m_sound[1] = AdvisorPickAdviceLine(0x130);
            ExpandMessageTemplate(&m_text0, 0x71c8, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x71c9, nullptr, nullptr, nullptr, nullptr);
        }
    }
}

// ---- Notice37 (kind 37) ----

Notice37::Notice37() : UiTextNotice() {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice37::Notice37(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice37::~Notice37() { REB_KEEP_VPTR_STORE(); }

int Notice37::GetKind() { return 37; }

UiNotice* Notice37::Virtual10() { return NewOrNull<Notice37>(); }

void Notice37::Virtual05(int kind, intptr_t payload) {
    if (kind == 0x221 && m_f64 == 0) {
        m_f64 = 1;
        if (m_owner == 1) {
            ObjectKey missionKey(static_cast<Role*>(GetObject_32000242(1))->m_missionKey);
            if (missionKey.Family() == 0x71) m_unk40 = 1;
            if (reinterpret_cast<const IntPairPayload*>(payload)->m_valueB == 0) m_unk40 = 0;
        }
        if (m_unk40 != 0) {
            m_picture[1] = 0x421;
            m_unk28 = 0x16;
            m_sound[0] = 0x46c;
            m_sound[1] = AdvisorPickAdviceLine(0x11d);
            ExpandMessageTemplate(&m_text0, 0x71b8, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x71b9, nullptr, nullptr, nullptr, nullptr);
        }
    }
}

void Notice37::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind == 0x210 && b != 0 && m_owner == 1) {
        m_f64 = 1;
        GameObjNotifier* luke = GetObject_32000242(1);
        if (luke) {
            m_picture[1] = 0x412;
            m_sound[0] = (m_owner != 1) + 0x47c;
            m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(luke));
            m_unk28 = 0x16;
            ExpandMessageTemplate(&m_text0, 0x71ba, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x71bb, nullptr, nullptr, nullptr, nullptr);
            m_unk40 = 1;
        }
    }
}

// ---- Notice35 (kind 35) ----

Notice35::Notice35() : UiKeyNotice() {
    m_categories = Virtual11();
}

Notice35::Notice35(const LinkNode* payloads, int owner) : UiKeyNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual14(payloads);
}

Notice35::~Notice35() { REB_KEEP_VPTR_STORE(); }

int Notice35::GetKind() { return 35; }

int Notice35::GetClass() { return 6; }

UiNotice* Notice35::Virtual10() { return NewOrNull<Notice35>(); }

void Notice35::OnMessage(int kind, intptr_t a, intptr_t b) {
    if (kind != 0x305) {
        UiNotice::OnMessage(kind, a, b);
        return;
    }
    GameObjNotifier* second = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(b));
    GameObjNotifier* parent = second->m_parent;
    GameObjNotifier* system = nullptr;
    if (parent) {
        ObjectKey parentKey;
        parent->GetKey(&parentKey);
        if (parentKey.Family() > 0x8f && parentKey.Family() < 0x98) system = parent;
    }
    if (notice::inFamilies(*reinterpret_cast<const ObjectKey*>(a), 0x18, 0x1c) && system) {
        m_unk40 = 1;
        m_unk28 = 0x15;
        ExpandMessageTemplate(&m_text0, 0x71bd, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71be, notice::nameOf(system), nullptr, nullptr, nullptr);
        m_value = 0x72;
    }
}

void Notice35::Read(istream* in) {
    UiKeyNotice::Read(in);
}

void Notice35::Write(ostream* out) {
    UiKeyNotice::Write(out);
}

// ---- Notice36 (kind 36) ----

Notice36::Notice36() : UiTextNotice(), m_state(0) {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice36::Notice36(const LinkNode* payloads, int owner) : UiTextNotice(owner), m_state(0) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual16(payloads);
}

Notice36::~Notice36() { REB_KEEP_VPTR_STORE(); }

int Notice36::GetKind() { return 36; }

UiNotice* Notice36::Virtual10() { return NewOrNull<Notice36>(); }

void Notice36::Virtual05(int kind, intptr_t payload) {
    if (kind != 0x360) return;
    ObjectKey characterKey(reinterpret_cast<const KeyedNotifPayload*>(payload)->m_key);
    GameObjNotifier* character = GetObjectInFamilies_01_ff(m_owner, &characterKey);
    if (character) {
        if (notice::inFamilies(characterKey, 0x30, 0x3c)) {
            static_cast<Character*>(character)->GetRecordSide();
            int side = static_cast<Character*>(character)->GetRecordSide();
            if (side == 1) {
                m_firstKey = ObjectKey(characterKey);
                if (m_owner == 1) m_key = ObjectKey(characterKey);
                return;
            }
            if (side == 2) {
                m_secondKey = ObjectKey(characterKey);
                if (m_owner == 2) m_key = ObjectKey(characterKey);
                return;
            }
        } else {
            m_unk40 = 0;
        }
    }
    m_state.m_value |= 0x10000000;
}

void Notice36::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind != 2) {
        if (kind == 0x211) {
            m_state.m_value |= 1;
        } else if (kind == 0x220) {
            m_state.m_value |= 0x10;
        }
        return;
    }
    if ((m_state.m_value & 0x10000000) != 0) {
        m_unk40 = 0;
        return;
    }
    m_unk40 = 1;
    if ((m_state.m_value & 0x10) != 0) {
        m_key.m_value = kLukeKey;
        FillLukeAlone();
    } else {
        bool storyPair = (m_firstKey.m_value == kLukeKey || m_firstKey.m_value == kLeiaKey) &&
                         (m_secondKey.m_value == kEmperorKey || m_secondKey.m_value == kVaderKey);
        if (storyPair) {
            if ((m_state.m_value & 1) == 0)
                FillStoryPair();
            else
                FillFinalDuel();
        } else {
            FillStoryEncounter();
        }
    }
    SetPriority();
}

void Notice36::SetPriority() {
    if (notice::inFamilies(m_key, 0x30, 0x38)) {
        switch (m_key.m_value & 0xffffff) {
        case 0x240:
            m_unk28 = 0x17;
            break;
        case 0x241:
            m_unk28 = 0x18;
            break;
        case 0x242:
            m_unk28 = 0x16;
            break;
        case 0x243:
            m_unk28 = 0x19;
            break;
        case 0x280:
            m_unk28 = 0x1a;
            break;
        case 0x281:
            m_unk28 = 0x1b;
            break;
        }
    } else {
        m_unk28 = 0x15;
    }
}

void Notice36::FillFinalDuel() {
    Character* luke = static_cast<Character*>(GetObject_32000242(m_owner));
    Character* emperor = static_cast<Character*>(GetObject_35000281(m_owner));
    Character* enemyLuke = static_cast<Character*>(GetObject_32000242(2));
    Character* enemyEmperor = static_cast<Character*>(GetObject_35000281(2));
    bool lukeAndEmperor = m_firstKey.m_value == kLukeKey && m_secondKey.m_value == kEmperorKey && luke && emperor;
    if (!lukeAndEmperor) {
        m_unk40 = 0;
        return;
    }
    ExpandMessageTemplate(&m_text0, 0x71a0, nullptr, nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x71a1, nullptr, nullptr, nullptr, nullptr);
    uint32_t text;
    bool setText = false;
    if ((luke->m_flagsAc & 1) == 0) {
        if (luke->m_injury != 0) {
            text = 0x71a4;
            setText = true;
        } else if ((emperor->m_flagsAc & 1) == 0) {
            text = 0x71a5;
            setText = true;
        }
    } else {
        bool sameMission = false;
        if (enemyLuke && enemyEmperor) {
            ObjectKey emperorMission(static_cast<Role*>(enemyEmperor)->m_missionKey);
            ObjectKey lukeMission(static_cast<Role*>(enemyLuke)->m_missionKey);
            if (lukeMission.m_value == emperorMission.m_value) {
                GameObjNotifier* vader = GetObject_34000280(2);
                Mission* mission = static_cast<Mission*>(GetObjectInFamilies_40_80(2, &lukeMission));
                if (mission && vader) {
                    ObjectKey vaderParent;
                    vader->m_parent->GetKey(&vaderParent);
                    if (mission->m_targetKey.m_value == vaderParent.m_value) sameMission = true;
                }
            }
        }
        text = sameMission ? 0x71a2 : 0x71a3;
        setText = true;
    }
    if (setText) ExpandMessageTemplate(&m_text1, text, nullptr, nullptr, nullptr, nullptr);
    bool emperorText = true;
    if ((emperor->m_flagsAc & 1) == 0) {
        if (emperor->m_injury == 0)
            emperorText = false;
        else
            text = 0x71a7;
    } else {
        text = 0x71a6;
    }
    if (emperorText) ExpandMessageTemplate(&m_text1, text, nullptr, nullptr, nullptr, nullptr);
    m_picture[1] = 0x422;
    m_sound[0] = 0x46d;
    m_sound[1] = AdvisorPickAdviceLine(0x11e);
}

void Notice36::FillStoryPair() {
    GameObjNotifier* first = GetObjectInFamilies_30_3c(m_owner, &m_firstKey);
    GameObjNotifier* second = GetObjectInFamilies_30_3c(m_owner, &m_secondKey);
    bool anyText = false;
    if (!first || !second) {
        m_unk40 = 0;
        return;
    }
    Character* firstCharacter = static_cast<Character*>(first);
    Character* secondCharacter = static_cast<Character*>(second);
    ExpandMessageTemplate(&m_text0, 0x71a8, notice::nameOf(first), notice::nameOf(second), nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x71a9, notice::nameOf(first), notice::nameOf(second), nullptr, nullptr);
    if ((firstCharacter->m_flagsAc & 1) == 0) {
        if (firstCharacter->m_injury != 0) {
            anyText = true;
            ExpandMessageTemplate(&m_text1, 0x71ab, notice::nameOf(first), nullptr, nullptr, nullptr);
        }
    } else {
        anyText = true;
        ExpandMessageTemplate(&m_text1, 0x71aa, notice::nameOf(first), nullptr, nullptr, nullptr);
    }
    if ((secondCharacter->m_flagsAc & 1) == 0) {
        if (secondCharacter->m_injury != 0) {
            anyText = true;
            ExpandMessageTemplate(&m_text1, 0x71ad, notice::nameOf(second), nullptr, nullptr, nullptr);
        }
    } else {
        anyText = true;
        ExpandMessageTemplate(&m_text1, 0x71ac, notice::nameOf(second), nullptr, nullptr, nullptr);
    }
    if (!anyText) ExpandMessageTemplate(&m_text1, 0x71ae, nullptr, nullptr, nullptr, nullptr);
    if (m_firstKey.m_value != kLukeKey) {
        if (m_secondKey.m_value == kEmperorKey) {
            m_picture[1] = 0x41f;
            m_sound[0] = 0x46a;
            m_sound[1] = AdvisorPickAdviceLine(0x137);
            return;
        }
        m_picture[1] = 0x420;
        m_sound[0] = 0x46b;
        m_sound[1] = AdvisorPickAdviceLine(0x138);
        return;
    }
    if (!dagobahDone(first)) {
        if (m_secondKey.m_value == kEmperorKey) {
            m_picture[1] = 0x423;
            m_sound[0] = 0x46e;
            m_sound[1] = AdvisorPickAdviceLine(0x118);
            return;
        }
        m_picture[1] = 0x425;
        m_sound[0] = 0x470;
        m_sound[1] = AdvisorPickAdviceLine(0x119);
        return;
    }
    if (m_secondKey.m_value == kEmperorKey) {
        m_picture[1] = 0x424;
        m_sound[0] = 0x46f;
        m_sound[1] = AdvisorPickAdviceLine(0x11a);
        return;
    }
    m_picture[1] = 0x426;
    m_sound[0] = 0x471;
    m_sound[1] = AdvisorPickAdviceLine(0x11b);
}

void Notice36::FillStoryEncounter() {
    GameObjNotifier* first = GetObjectInFamilies_30_3c(m_owner, &m_firstKey);
    GameObjNotifier* second = GetObjectInFamilies_30_3c(m_owner, &m_secondKey);
    GameObjNotifier* subject = m_owner == 1 ? first : second;
    GameObjNotifier* partner = m_owner == 1 ? second : first;
    if (!subject || !partner) {
        m_unk40 = 0;
        return;
    }
    m_picture[1] = (m_owner != 1) + 0x412;
    m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(subject));
    m_sound[0] = (m_owner != 1) + 0x47c;
    ExpandMessageTemplate(&m_text0, 0x71b0, notice::nameOf(subject), notice::nameOf(partner), nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, 0x71b1, notice::nameOf(subject), notice::nameOf(partner), nullptr, nullptr);
    ObjectKey subjectKey;
    subject->GetKey(&subjectKey);
    if (subjectKey.m_value == kLukeKey) {
        m_sound[1] = AdvisorPickAdviceLine(dagobahDone(subject) ? 0xda : 0xd0);
        return;
    }
    subject->GetKey(&subjectKey);
    if (subjectKey.m_value == kLeiaKey) m_sound[1] = AdvisorPickAdviceLine(0xe4);
}

void Notice36::FillLukeAlone() {
    Character* luke = static_cast<Character*>(GetObject_32000242(m_owner));
    if (!luke) {
        m_unk40 = 0;
        return;
    }
    ExpandMessageTemplate(&m_text0, 0x71e0, nullptr, nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, (luke->m_flagsAc & 1) == 0 ? 0x71e1 : 0x71e2, nullptr, nullptr, nullptr, nullptr);
    if (dagobahDone(luke)) {
        m_picture[1] = 0x428;
        m_sound[1] = AdvisorPickAdviceLine(0x12f);
        m_sound[0] = 0x473;
        return;
    }
    m_picture[1] = 0x427;
    m_sound[1] = AdvisorPickAdviceLine(0x11c);
    m_sound[0] = 0x472;
}
