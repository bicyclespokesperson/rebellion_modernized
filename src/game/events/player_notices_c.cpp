#include "events/player_notices.h"

#include <new>

#include "core/checked_new.h"
#include "ui/message_template.h"
#include "character/character.h"
#include "character/role.h"
#include "core/game_string.h"
#include "events/player_notice_util.h"
#include "events/ui_notif_helpers.h"
#include "events/ui_notifs_ext.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "story/story_characters.h"
#include "strategy_ui/game_ui_facade.h"

namespace {

const uint32_t kLeiaKey = 0x31000241;

bool dagobahDone(GameObjNotifier* luke) { return (static_cast<Luke*>(luke)->m_storyFlags & Luke::kDagobahCompleted) != 0; }

}  // namespace

// ---- Notice31 (kind 31) ----

Notice31::Notice31() : UiKeyNotice() {
    m_categories = Virtual11();
}

Notice31::Notice31(const LinkNode* payloads, int owner) : UiKeyNotice(owner) {
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual14(payloads);
}

Notice31::~Notice31() = default;

int Notice31::GetKind() { return 31; }

UiNotice* Notice31::Virtual10() { return NewOrNull<Notice31>(); }

void Notice31::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind == 0x128 && (b & 1) != 0) {
        m_unk40 = 1;
        m_unk28 = 0x11;
        ExpandMessageTemplate(&m_text0, 0x7188, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7189, nullptr, nullptr, nullptr, nullptr);
        m_value = 0x73;
    }
}

void Notice31::Read(istream* in) {
    UiKeyNotice::Read(in);
    ReadValue32(in, &m_f64);
}

void Notice31::Write(ostream* out) {
    UiKeyNotice::Write(out);
    WriteValue32(out, &m_f64);
}

// ---- Notice32 (kind 32) ----

Notice32::Notice32() : UiTextNotice(), m_state(0) {
    m_categories = Virtual11();
}

Notice32::Notice32(const LinkNode* payloads, int owner) : UiTextNotice(owner), m_state(0) {
    m_categories = Virtual11();
    Virtual16(payloads);
}

Notice32::~Notice32() = default;

int Notice32::GetKind() { return 32; }

UiNotice* Notice32::Virtual10() { return NewOrNull<Notice32>(); }

void Notice32::OnMessage(int kind, intptr_t a, intptr_t b) {
    if (kind != 0x302) {
        UiNotice::OnMessage(kind, a, b);
        return;
    }
    const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
    if (notice::inFamilies(*key, 0x20, 0x30)) {
        m_state.m_value |= 0x10;
        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
        if (object) ExpandMessageTemplate(&m_name, 0x719b, notice::nameOf(object), nullptr, nullptr, nullptr);
    }
}

void Notice32::Virtual08(int kind, intptr_t a, intptr_t b) {
    const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
    if (kind != 2) {
        if (kind == 0x154) {
            m_key = ObjectKey(*key);
            m_state.m_value |= 8;
            return;
        }
        if (kind != 0x155) return;
        m_key = ObjectKey(*key);
        m_state.m_value &= 0xfffffff7;
        return;
    }
    bool inSystemRange = m_key.Family() > 0x8f && m_key.Family() < 0x98;
    GameObjNotifier* system = nullptr;
    m_unk40 = 0;
    if (inSystemRange) system = GetObjectInFamilies_90_98(m_owner, &m_key);
    if (inSystemRange && system) {
        uint32_t controller = system->ControlSide();
        if (controller == 1) {
            if (m_owner == 1) m_unk40 = 1;
        } else if (controller == 2) {
            if (m_owner == 2) m_unk40 = 1;
        } else {
            m_unk40 = 0;
        }
    }
    if (m_unk40 == 0) return;
    if ((m_state.m_value & 8) == 0) {
        m_picture[1] = 0x3ea;
        m_sound[0] = 0x44d;
        uint32_t text;
        if ((m_state.m_value & 1) == 0) {
            ExpandMessageTemplate(&m_text0, 0x7196, notice::nameOf(system), nullptr, nullptr, nullptr);
            text = (m_state.m_value & 2) == 0 ? 0x7197 : 0x7198;
        } else {
            ExpandMessageTemplate(&m_text0, 0x7194, notice::nameOf(system), nullptr, nullptr, nullptr);
            text = (m_state.m_value & 2) == 0 ? 0x7199 : 0x719a;
        }
        ExpandMessageTemplate(&m_text1, text, notice::nameOf(system), nullptr, nullptr, nullptr);
    } else {
        m_sound[0] = 0x44e;
        ExpandMessageTemplate(&m_text0, 0x7190, notice::nameOf(system), nullptr, nullptr, nullptr);
        uint32_t state = m_state.m_value;
        int16_t text = 0;
        if ((state & 0x10) == 0) {
            if ((state & 6) == 0)
                m_unk40 = 0;
            else
                text = 0x7191;
        } else {
            text = ((state & 6) != 0) + 0x7192;
        }
        if (text != 0 && m_unk40 != 0) {
            ExpandMessageTemplate(&m_text1, text, notice::nameOf(system), nullptr, nullptr, nullptr);
            if ((m_state.m_value & 0x10) != 0) {
                String suffix(m_name.m_text);
                m_text1 += suffix;
            }
        }
        m_picture[1] = 0x3eb;
        m_unk28 = 0x1c;
    }
    m_unk28 = 0x1c;
}

void Notice32::Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) {
    if (kind == 0x141) {
        if (c < b) m_state.m_value |= 1;
        m_state.m_value |= 4;
    } else if (kind == 0x143) {
        if (c < b) m_state.m_value |= 1;
        m_state.m_value |= 2;
    }
}

void Notice32::Read(istream* in) {
    UiTextNotice::Read(in);
    m_state.Read(in);
}

void Notice32::Write(ostream* out) {
    UiTextNotice::Write(out);
    m_state.Write(out);
}

// ---- Notice29 (kind 29) ----

Notice29::Notice29() : UiTextNotice() {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice29::Notice29(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice29::~Notice29() = default;

int Notice29::GetKind() { return 29; }

UiNotice* Notice29::Virtual10() { return NewOrNull<Notice29>(); }

void Notice29::Virtual05(int kind, intptr_t payload) {
    if (kind != 0x1e1) return;
    const IntPairPayload* notif = reinterpret_cast<const IntPairPayload*>(payload);
    ObjectKey characterKey(notif->m_key);
    m_key = ObjectKey(characterKey);
    GameObjNotifier* character = GetObjectInFamilies_01_ff(m_owner, &characterKey);
    m_picture[1] = (m_owner != 1) + 0x412;
    m_sound[0] = (m_owner != 1) + 0x47c;
    if (character) {
        bool valid = notice::inFamilies(characterKey, 0x30, 0x3c) && notif->m_valueA != 0 && notif->m_valueB != 0;
        if (valid) {
            m_unk40 = 1;
            if (m_owner == 1) {
                if (static_cast<Character*>(character)->GetRecordSide() == 2) m_unk40 = 0;
            } else if (m_owner != 2 || static_cast<Character*>(character)->GetRecordSide() == 1) {
                m_unk40 = 0;
            }
        }
    }
    if (m_unk40 != 0) {
        m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(character));
        ExpandMessageTemplate(&m_text0, 0x7180, notice::nameOf(character), nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7181, notice::nameOf(character), nullptr, nullptr, nullptr);
        if (notice::inFamilies(characterKey, 0x30, 0x38)) {
            switch (characterKey.m_value & 0xffffff) {
            case 0x240:
                m_unk28 = 0x17;
                break;
            case 0x241:
                m_unk28 = 0x18;
                m_sound[1] = AdvisorPickAdviceLine(0x134);
                break;
            case 0x242:
                m_unk28 = 0x16;
                m_sound[1] = AdvisorPickAdviceLine(dagobahDone(character) ? 0x133 : 0x132);
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
}

// ---- Notice28 (kind 28) ----

Notice28::Notice28() : UiTextNotice() {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice28::Notice28(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice28::~Notice28() = default;

int Notice28::GetKind() { return 28; }

UiNotice* Notice28::Virtual10() { return NewOrNull<Notice28>(); }

void Notice28::Virtual05(int kind, intptr_t payload) {
    if (kind != 0x362) return;
    KeyPairPayload* notif = reinterpret_cast<KeyPairPayload*>(payload);
    ObjectKey nameKey(notif->m_key);
    ObjectKey scratch;
    ObjectKey secondKey(*copySecondKeyOf(notif, &scratch));
    GameObjNotifier* first = GetObjectInFamilies_01_ff(m_owner, &nameKey);
    GameObjNotifier* second = GetObjectInFamilies_01_ff(m_owner, &secondKey);
    m_picture[1] = (m_owner != 1) + 0x412;
    m_sound[0] = (m_owner != 1) + 0x47c;
    if (first && second) {
        m_key = ObjectKey(secondKey);
        if (notice::inFamilies(nameKey, 0x30, 0x3c) && notice::inFamilies(secondKey, 0x30, 0x3c)) m_unk40 = 1;
        if (m_owner == 1) {
            if (static_cast<Character*>(first)->GetRecordSide() != 1) m_unk40 = 0;
        } else if (m_owner == 2 && static_cast<Character*>(first)->GetRecordSide() != 2) {
            m_unk40 = 0;
        }
    }
    if (m_unk40 != 0) {
        m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(first));
        if (secondKey.m_value == kLeiaKey) {
            nameKey = ObjectKey(secondKey);
            m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(second));
            ExpandMessageTemplate(&m_text0, 0x71d8, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x71d9, nullptr, nullptr, nullptr, nullptr);
        } else {
            ExpandMessageTemplate(&m_text0, 0x7178, notice::nameOf(second), notice::nameOf(first), nullptr, nullptr);
            int canTrain;
            static_cast<Character*>(first)->CanTrainForce(&canTrain);
            ExpandMessageTemplate(&m_text1, canTrain == 0 ? 0x717a : 0x7179, notice::nameOf(second), notice::nameOf(first), nullptr, nullptr);
        }
        if (notice::inFamilies(nameKey, 0x30, 0x38)) {
            switch (nameKey.m_value & 0xffffff) {
            case 0x240:
                m_unk28 = 0x17;
                break;
            case 0x241:
                m_unk28 = 0x18;
                m_sound[1] = AdvisorPickAdviceLine(0x124);
                break;
            case 0x242:
                m_unk28 = 0x16;
                m_sound[1] = AdvisorPickAdviceLine(0x135);
                break;
            case 0x243:
                m_unk28 = 0x19;
                break;
            case 0x280:
                m_unk28 = 0x1a;
                break;
            case 0x281:
                m_unk28 = 0x1b;
                m_sound[1] = AdvisorPickAdviceLine(0x136);
                break;
            }
        } else {
            m_unk28 = 0x15;
        }
    }
}

// ---- Notice30 (kind 30) ----

Notice30::Notice30() : UiKeyNotice() {
    m_categories = Virtual11();
}

Notice30::Notice30(const LinkNode* payloads, int owner) : UiKeyNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual14(payloads);
}

Notice30::~Notice30() = default;

int Notice30::GetKind() { return 30; }

UiNotice* Notice30::Virtual10() { return NewOrNull<Notice30>(); }

void Notice30::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind == 0x128) {
        ObjectKey systemKey;
        systemKey.m_value = 0x90000109;
        GameObjNotifier* system = GetObjectInFamilies_90_98(m_owner, &systemKey);
        m_key = ObjectKey(systemKey);
        int show = 0;
        if (system && system->ControlSide() == 1) show = 1;
        m_unk40 = show;
        if (show != 0) {
            m_f64 = 1;
            m_unk28 = (m_owner != 1) + 1;
            ExpandMessageTemplate(&m_text0, 0x7170, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x7171, nullptr, nullptr, nullptr, nullptr);
            m_value = 0x74;
        }
    }
}

// ---- Notice26a (kind 26) ----

Notice26a::Notice26a() : UiTextNotice() {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice26a::Notice26a(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual16(payloads);
}

Notice26a::~Notice26a() = default;

int Notice26a::GetKind() { return 26; }

UiNotice* Notice26a::Virtual10() { return NewOrNull<Notice26a>(); }

void Notice26a::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind == 0x212 && b == 0) {
        ObjectKey characterKey;
        characterKey.m_value = 0x33000243;
        Character* character = static_cast<Character*>(GetObjectInFamilies_30_3c(m_owner, &characterKey));
        m_unk40 = 1;
        if ((character->m_flagsAc & 1) == 0) {
            if (m_owner == 1) {
                m_picture[1] = 0x41c;
                m_sound[0] = 0x475;
                m_unk28 = 0x19;
                m_sound[1] = AdvisorPickAdviceLine(0x120);
                ExpandMessageTemplate(&m_text0, 0x7168, nullptr, nullptr, nullptr, nullptr);
                ExpandMessageTemplate(&m_text1, 0x7169, nullptr, nullptr, nullptr, nullptr);
                m_key.m_value = 0x33000243;
            } else {
                ObjectKey systemKey;
                GameObjNotifier* system = notice::containingSystem(character->m_parent, &systemKey);
                if (system) {
                    m_picture[1] = 0x41e;
                    m_sound[0] = 0x477;
                    m_unk28 = 0x2d;
                    ExpandMessageTemplate(&m_text0, 0x716a, nullptr, nullptr, nullptr, nullptr);
                    ExpandMessageTemplate(&m_text1, 0x716b, notice::nameOf(system), nullptr, nullptr, nullptr);
                    m_key.m_value = 0x33000243;
                    return;
                }
                m_unk40 = 0;
            }
        } else {
            m_picture[1] = 0x41d;
            m_sound[0] = 0x476;
            m_unk28 = 0x21;
            m_sound[1] = AdvisorPickAdviceLine(0x11f);
            ExpandMessageTemplate(&m_text0, 0x716c, nullptr, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x716d, nullptr, nullptr, nullptr, nullptr);
        }
    }
}

// ---- Notice26b (kind 26) ----

Notice26b::Notice26b() : UiTextNotice() {
    m_categories = Virtual11();
    m_unk40 = 0;
}

Notice26b::Notice26b(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_unk40 = 0;
    m_categories = Virtual11();
    Virtual16(payloads);
}

Notice26b::~Notice26b() = default;

UiNotice* Notice26b::Virtual10() { return NewOrNull<Notice26b>(); }

void Notice26b::Virtual08(int kind, intptr_t a, intptr_t b) {
    if (kind != 0x106) return;
    const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
    GameObj* object = static_cast<GameObj*>(GetObjectInFamilies_01_ff(m_owner, key));
    GameObj* character = nullptr;
    if (notice::inFamilies(*key, 0x30, 0x3c)) {
        m_unk40 = 1;
        if (!object)
            m_unk40 = 0;
        else
            m_key = ObjectKey(*key);
        character = object;
        if ((object->m_stateFlags & 8) != 0 && (object->m_packed & 0xff) == 7) m_unk40 = 0;
    }
    if (m_unk40 == 0) return;
    m_sound[0] = 0x474;
    if ((character->m_stateFlags & 8) == 0) {
        if (b == 0) {
            m_picture[1] = 0x41b;
            m_picture[0] = AdvisorRecordTypeResource(object);
            ExpandMessageTemplate(&m_text0, 0x7162, character, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x7163, character, nullptr, nullptr, nullptr);
            if (notice::inFamilies(*key, 0x30, 0x38)) {
                uint32_t advice = 0;
                switch (key->m_value & 0xffffff) {
                case 0x240:
                    advice = 0xf7;
                    break;
                case 0x241:
                    advice = 0xe3;
                    break;
                case 0x242:
                    advice = dagobahDone(object) ? 0xd9 : 0xcf;
                    break;
                case 0x243:
                    advice = 0xed;
                    break;
                case 0x280:
                    advice = 0x10b;
                    break;
                case 0x281:
                    advice = 0x101;
                    break;
                default:
                    goto setPriority;
                }
                m_sound[1] = AdvisorPickAdviceLine(advice);
            }
        } else {
            m_picture[1] = 0x41a;
            ExpandMessageTemplate(&m_text0, 0x7160, character, nullptr, nullptr, nullptr);
            ExpandMessageTemplate(&m_text1, 0x7161, character, nullptr, nullptr, nullptr);
        }
    } else {
        m_picture[1] = (static_cast<Character*>(character)->GetRecordSide() != 1 ? 5 : 0) + 0x42e;
        ExpandMessageTemplate(&m_text0, 0x7164, character, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7365, character, nullptr, nullptr, nullptr);
    }
setPriority:
    if (notice::inFamilies(*key, 0x30, 0x38)) {
        switch (key->m_value & 0xffffff) {
        case 0x240:
            m_unk28 = 0x17;
            return;
        case 0x241:
            m_unk28 = 0x18;
            return;
        case 0x242:
            m_unk28 = 0x16;
            return;
        case 0x243:
            m_unk28 = 0x19;
            return;
        case 0x280:
            m_unk28 = 0x1a;
            return;
        case 0x281:
            m_unk28 = 0x1b;
            return;
        }
    } else {
        m_unk28 = 0x15;
    }
}
