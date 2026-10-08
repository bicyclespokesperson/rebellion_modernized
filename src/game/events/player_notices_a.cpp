#include "events/player_notices.h"

#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "ui/message_template.h"
#include "economy/manu_mgr.h"
#include "economy/manufacturing_facility.h"
#include "events/player_notice_util.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "strategy_ui/game_ui_facade.h"


// ---- Notice44 (kind 44) ----

Notice44::Notice44() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
    m_unk40 = 0;
}

Notice44::Notice44(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice44::~Notice44() { REB_KEEP_VPTR_STORE(); }

int Notice44::GetKind() { return 44; }

int Notice44::GetClass() { return 8; }

int Notice44::Virtual11() { return 8; }

UiNotice* Notice44::Virtual10() { return NewOrNull<Notice44>(); }

void Notice44::Virtual06(int kind, int a, int b, int c) {

}

void Notice44::OnMessage(int kind, int a, int b) {
    UiNotice::OnMessage(kind, a, b);
}

void Notice44::Virtual08(int kind, int a, int b) {
    m_unk28 = 4;
    m_sound[0] = 0x459;
    m_picture[1] = 0x3f9;
    if (kind == 0x129) {
        m_unk40 = 1;
        ExpandMessageTemplate(&m_text0, 0x7218, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7219, nullptr, nullptr, nullptr, nullptr);
    } else if (kind == 0x12a) {
        m_unk40 = 1;
        ExpandMessageTemplate(&m_text1, 0x721a, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text0, 0x7218, nullptr, nullptr, nullptr, nullptr);
    } else if (kind == 0x12b) {
        m_unk40 = 1;
        ExpandMessageTemplate(&m_text0, 0x7218, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x721b, nullptr, nullptr, nullptr, nullptr);
    }
}

void Notice44::Read(istream* in) {
    UiTextNotice::Read(in);
}

void Notice44::Write(ostream* out) {
    UiTextNotice::Write(out);
}

// ---- Notice45 (kind 45) ----

Notice45::Notice45() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
    m_unk40 = 0;
}

Notice45::Notice45(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice45::~Notice45() { REB_KEEP_VPTR_STORE(); }

int Notice45::GetKind() { return 45; }

int Notice45::Virtual11() { return 16; }

UiNotice* Notice45::Virtual10() { return NewOrNull<Notice45>(); }

void Notice45::Virtual08(int kind, int a, int b) {
    if (kind == 300 && m_f64 == 0 && b != 0) {
        m_picture[1] = (m_owner != 1) + 0x412;
        m_f64 = 1;
        m_unk40 = 1;
        m_sound[0] = (m_owner != 1) + 0x47c;
        ExpandMessageTemplate(&m_text0, 0x7210, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7211, nullptr, nullptr, nullptr, nullptr);
    }
}

// ---- Notice43 (kind 43) ----

Notice43::Notice43() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
}

Notice43::Notice43(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice43::~Notice43() { REB_KEEP_VPTR_STORE(); }

int Notice43::GetKind() { return 43; }

UiNotice* Notice43::Virtual10() { return NewOrNull<Notice43>(); }

void Notice43::OnMessage(int kind, int a, int b) {
    if (kind != 0x306) return;
    GameObjNotifier* first = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(a));
    GameObjNotifier* second = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(b));
    ObjectKey systemKey;
    GameObjNotifier* system = notice::containingSystem(second, &systemKey);
    if (system) {
        system->GetKey(&systemKey);
        m_key = ObjectKey(systemKey);
    }
    if (first && second) {
        if (m_f64 == 0) {
            m_f64 = 1;
            ExpandMessageTemplate(&m_text0, 0x7208, notice::nameOf(first), notice::nameOf(second), notice::nameOf(system), nullptr);
            ExpandMessageTemplate(&m_text1, 0x7209, notice::nameOf(first), notice::nameOf(second), notice::nameOf(system), nullptr);
            m_unk28 = 0x15;
            m_sound[0] = 0x474;
            m_picture[1] = 0x42e;
        }
        m_unk40 = m_owner == 1 ? 1 : 0;
    }
}

void Notice43::Read(istream* in) {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
}

void Notice43::Write(ostream* out) {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
}

// ---- Notice42 (kind 42) ----

Notice42::Notice42() : UiTextNotice() {
    m_categories = Virtual11();
}

Notice42::Notice42(int owner, const IdNode* record) : UiTextNotice(owner) {
    m_categories = Virtual11();
    m_unk28 = 0x2a;
    m_unk40 = 1;
    int group = static_cast<int>(record->m_id) >> 4;
    int firstText;
    int secondText;
    if (m_owner == 1) {
        m_picture[1] = 0x42f;
        m_sound[0] = 0x461;
        firstText = group * 3 + 0x6001;
        secondText = group * 3 + 0x6002;
    } else {
        m_picture[1] = 0x430;
        m_sound[0] = 0x462;
        secondText = group * 3 + 0x6802;
        firstText = (group + 0x22ab) * 3;
    }
    ExpandMessageTemplate(&m_text0, firstText, nullptr, nullptr, nullptr, nullptr);
    ExpandMessageTemplate(&m_text1, secondText, nullptr, nullptr, nullptr, nullptr);
}

Notice42::~Notice42() { REB_KEEP_VPTR_STORE(); }

int Notice42::GetKind() { return 42; }

UiNotice* Notice42::Virtual10() { return NewOrNull<Notice42>(); }

// ---- Notice41 (kind 41) ----

Notice41::Notice41() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
    m_unk40 = 0;
}

Notice41::Notice41(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice41::~Notice41() { REB_KEEP_VPTR_STORE(); }

int Notice41::GetKind() { return 41; }

UiNotice* Notice41::Virtual10() { return NewOrNull<Notice41>(); }

void Notice41::Virtual08(int kind, int a, int b) {
    if (kind == 0x231 && m_f64 == 0 && b != 0) {
        m_f64 = 1;
        m_unk40 = 1;
        const uint8_t* character = reinterpret_cast<const uint8_t*>(GetObject_32000242(m_owner));
        uint32_t advice;
        if ((character[0xb0] & 8) == 0) {
            m_picture[1] = 0x42a;
            m_sound[0] = 0x479;
            advice = 0x123;
        } else {
            m_picture[1] = 0x42b;
            m_sound[0] = 0x47a;
            advice = 0x139;
        }
        m_sound[1] = AdvisorPickAdviceLine(advice);
        m_unk28 = 0x1b;
        m_key.m_value = 0x35000281;
        ExpandMessageTemplate(&m_text0, 0x7200, nullptr, nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x7201, nullptr, nullptr, nullptr, nullptr);
    }
}

// ---- Notice34 (kind 34) ----

Notice34::Notice34() : UiTextNotice() {
    m_categories = Virtual11();
    m_f64 = 0;
}

Notice34::Notice34(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_f64 = 0;
    m_categories = Virtual11();
    m_unk40 = 0;
    Virtual16(payloads);
}

Notice34::~Notice34() { REB_KEEP_VPTR_STORE(); }

int Notice34::GetKind() { return 34; }

int Notice34::Virtual11() { return 256; }

UiNotice* Notice34::Virtual10() { return NewOrNull<Notice34>(); }

void Notice34::OnMessage(int kind, int a, int b) {
    if (kind != 0x305) return;
    GameObjNotifier* first = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(a));
    GameObjNotifier* second = GetObjectInFamilies_01_ff(m_owner, reinterpret_cast<const ObjectKey*>(b));
    ObjectKey systemKey;
    GameObjNotifier* system = notice::containingSystem(second, &systemKey);
    if (system) {
        system->GetKey(&systemKey);
        m_key = ObjectKey(systemKey);
    }
    if (first && second) {
        if (m_f64 == 0) {
            m_f64 = 1;
            ExpandMessageTemplate(&m_text0, 0x7083, notice::nameOf(first), notice::nameOf(second), notice::nameOf(system), nullptr);
            ExpandMessageTemplate(&m_text1, 0x7084, notice::nameOf(first), notice::nameOf(second), notice::nameOf(system), nullptr);
            m_unk28 = 0xc;
            m_sound[0] = 0x456;
            m_categories = 0x100;
        }
        ExpandMessageTemplate(&m_text1, 0x7085, notice::nameOf(first), notice::nameOf(second), nullptr, nullptr);
        m_unk40 = 1;
        uint32_t controller = first->ControlSide();
        if (controller == 1) {
            if (m_owner != 2) return;
        } else if (controller == 2 && m_owner != 1) {
            return;
        }
        m_unk40 = 0;
    }
}

int Notice34::Virtual16(const LinkNode* payloads) {
    int result = ForwardPayloads(this, payloads);
    if (m_unk40 != 0) {
        m_unk28 = 0xc;
        m_sound[0] = 0x456;
        if (m_owner == 1) {
            m_picture[1] = 0x3f5;
            return result;
        }
        m_picture[1] = 0x3f6;
    }
    return result;
}

// ---- Notice33 (kind 33) ----

Notice33::Notice33() : UiTextNotice() {
    m_f64 = 0;
    m_categories = Virtual11();
    m_f68 = 0;
    m_unk40 = 0;
}

Notice33::Notice33(const LinkNode* payloads, int owner) : UiTextNotice(owner) {
    m_unk40 = 0;
    m_f68 = 0;
    m_categories = Virtual11();
    m_f64 = 0;
    Virtual16(payloads);
}

Notice33::~Notice33() { REB_KEEP_VPTR_STORE(); }

int Notice33::GetKind() { return 33; }

UiNotice* Notice33::Virtual10() { return NewOrNull<Notice33>(); }

void Notice33::OnMessage(int kind, int a, int b) {
    if (kind != 0x160) return;
    const ObjectKey* key = reinterpret_cast<const ObjectKey*>(a);
    ManuMgr* manager = nullptr;
    GameObjNotifier* facility = nullptr;
    GameObjNotifier* host = nullptr;
    FamilyRange wanted(0x2a, 0x2b);
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_owner, key);
    if (object) {
        if (notice::inFamilies(*key, 0xa0, 0xb0)) {
            m_hostKey = ObjectKey(*key);
            manager = static_cast<ManuMgr*>(object);
        }
        host = object->m_parent;
    }
    m_key = ObjectKey(*key);
    if (!manager) return;
    m_unk40 = manager->m_remainingGameObjCount == 0 ? 1 : 0;
    if (m_owner == 1) {
        if (object->ControlSide() != 1) m_unk40 = 0;
    } else if (m_owner != 2 || object->ControlSide() != 2) {
        m_unk40 = 0;
    }
    if (b != 0) m_unk40 = 0;
    int facilityKind = manager->GetFacilityKind();
    if (facilityKind == 0) {
        m_f64 = 3;
        wanted = FamilyRange(0x28, 0x29);
    } else if (facilityKind == 1) {
        m_f64 = 1;
        wanted = FamilyRange(0x2a, 0x2b);
    } else if (facilityKind == 2) {
        m_f64 = 2;
        wanted = FamilyRange(0x29, 0x2a);
    } else {
        m_unk40 = 0;
    }
    if (m_unk40 != 0) {
        ManufacturingFacilityIterator it(host, 1);
        for (it.Begin(); it.Current(); it.Advance()) {
            if (facility) break;
            ObjectKey facilityKey;
            it.Current()->GetKey(&facilityKey);
            if (notice::inFamilies(facilityKey, wanted.m_first, wanted.m_second)) facility = it.Current();
        }
        if (!facility) m_unk40 = 0;
    }
    if (m_unk40 == 0) return;
    if (m_f64 == 1) {
        ExpandMessageTemplate(&m_text0, 0x71f8, notice::nameOf(host), nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71f9, notice::nameOf(host), nullptr, nullptr, nullptr);
    } else if (m_f64 == 2) {
        ExpandMessageTemplate(&m_text0, 0x71fa, notice::nameOf(host), nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71fb, notice::nameOf(host), nullptr, nullptr, nullptr);
    } else if (m_f64 == 3) {
        ExpandMessageTemplate(&m_text0, 0x71fc, notice::nameOf(host), nullptr, nullptr, nullptr);
        ExpandMessageTemplate(&m_text1, 0x71fd, notice::nameOf(host), nullptr, nullptr, nullptr);
    } else {
        m_unk40 = 0;
    }
    ObjectKey facilityKey;
    facility->GetKey(&facilityKey);
    m_key = ObjectKey(facilityKey);
    m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(facility));
    m_unk28 = 3;
    m_sound[0] = 0x44d;
}

int Notice33::IsStale() {
    int stale = 1;
    GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_owner, &m_hostKey);
    if (manager && static_cast<ManuMgr*>(manager)->m_remainingGameObjCount == 0) stale = 0;
    if (m_owner == 1) {
        if (manager->ControlSide() != 1) stale = 1;
    } else if (m_owner == 2 && manager->ControlSide() != 2) {
        return 1;
    }
    return stale;
}

void Notice33::Read(istream* in) {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
    ReadValue32(in, &m_f68);
    m_hostKey.Read(in);
}

void Notice33::Write(ostream* out) {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
    WriteValue32(out, &m_f68);
    m_hostKey.Write(out);
}
