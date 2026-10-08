#include <new>

#include "app/app_control_cmds.h"
#include "app/player_controller_decl.h"
#include "core/checked_new.h"
#include "events/notif_event_payloads.h"
#include "events/player_notices.h"
#include "events/ui_notifs_ext.h"
#include "gameobj/game_obj.h"
#include "gameobj/notif_payload.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/game_ui_facade.h"

namespace {

bool inFamilies(const ObjectKey& key, uint32_t first, uint32_t last) { return key.Family() >= first && key.Family() < last; }

constexpr uint32_t kFlagDialogWaiting = 1;
constexpr uint32_t kFlagReportWaiting = 2;
constexpr uint32_t kFlagIdleWait = 8;
constexpr uint32_t kFlagHeld = 0x10000000;
constexpr uint32_t kFlagDialogShown = 0x20000000;

}  // namespace

// ---- PlayerDispatch -------------------------------------------------------------------------------------------

PlayerDispatch::PlayerDispatch() : m_flags(0), m_defaultState(0) {
    m_side = 0;
    m_unk50 = 0;
    m_unk40 = 0;
}

PlayerDispatch::~PlayerDispatch() { ClearRecords(); }

void PlayerDispatch::Read(istream* in) {
    ReadValue32(in, &m_side);
    ReadValuePersistentInt(in, &m_flags);
    m_trees[0].Read(in);
    m_trees[1].Read(in);
    m_states.Clear();
    m_states.Read(in);
    m_defaultState.Read(in);
    ReadValue32(in, &m_unk40);
    ReadValue32(in, &m_unk50);
    UiClearButtonHighlights(m_unk50);
    m_trees[3].Read(in);
}

void PlayerDispatch::Write(ostream* out) {
    WriteValue32(out, &m_side);
    WriteValuePersistentInt(out, &m_flags);
    m_trees[0].Write(out);
    m_trees[1].Write(out);
    m_states.Write(out);
    m_defaultState.Write(out);
    WriteValue32(out, &m_unk40);
    WriteValue32(out, &m_unk50);
    m_trees[3].Write(out);
}

void PlayerDispatch::ClearRecords() {
    m_trees[0].Clear();
    m_trees[1].Clear();
}

void PlayerDispatch::HandleNotifications(LinkNode* payloads) {
    if (!payloads) return;
    int kind = ClassifyNotifications(payloads);
    if (kind != 0) {
        UiNotice* notice = CreateNotice(kind, payloads);
        AddNotice(notice);
        return;
    }
    DiscardPayloads(payloads);
}

void PlayerDispatch::Update() {
    if (m_trees[2].First()) {
        if ((m_flags.m_value & kFlagDialogShown) == 0) {
            AvlNode* dialog = m_trees[2].First();
            if (dialog) {
                UiSetPendingMovie(reinterpret_cast<LPARAM>(dialog));
                m_flags.m_value |= kFlagDialogShown;
            }
        }
        m_flags.m_value &= ~kFlagIdleWait;
        return;
    }
    int* flags = &m_flags.m_value;
    if (m_flags.Get() == 0) return;
    uint32_t value = *flags;
    if ((value & kFlagDialogWaiting) == 0) {
        if ((value & kFlagReportWaiting) == 0) {
            if ((value & 4) != 0) *flags &= ~4u;
        } else if (m_unk40 != 0) {
            if (UiIsGalaxyViewShown()) {
                UiRecord* record = static_cast<UiRecord*>(m_trees[1].First());
                if (record && record->GetKind() == 2) {
                    UiOpenType6Window(reinterpret_cast<LPARAM>(record));
                    m_unk40 = 0;
                    *flags &= ~kFlagReportWaiting;
                    return;
                }
            }
        }
    } else if (m_unk40 != 0) {
        if (UiIsGalaxyViewShown()) {
            UiRecord* record = static_cast<UiRecord*>(m_trees[1].First());
            if (record && record->GetKind() == 1) {
                UiOpenType8Window(reinterpret_cast<LPARAM>(record));
                m_unk40 = 0;
                *flags &= ~kFlagDialogWaiting;
                return;
            }
        }
    }
}

void PlayerDispatch::AddMessage(UiNotice* message) {
    if (!message) return;
    int messageClass = message->GetClass();
    if (messageClass == 1) {
        m_trees[1].DetachById(message->m_id);
        delete message;
    } else if (messageClass == 2) {
        m_trees[1].DetachById(message->m_id);
        m_trees[3].AddWithId(message);
    } else if (messageClass == 6) {
        m_trees[2].DetachById(message->m_id);
        m_flags.m_value &= 0xdfffffff;
        delete message;
    }
}

void PlayerDispatch::AddArrivalNotice(const ObjectKey* key, int attack) {
    if (!inFamilies(*key, 0x90, 0x98)) return;
    UiNotice* notice = NewOrNull<Notice2>(key, m_side);
    if (notice) static_cast<Notice2*>(notice)->m_attack = attack;
    AddNotice(notice);
}

void PlayerDispatch::DiscardPayloads(LinkNode* payloads) {
    for (LinkNode* node = payloads->m_next; node; node = node->m_next) static_cast<NotifPayload*>(node)->DispatchToSink(nullptr);
}

void PlayerDispatch::AddNotice(UiNotice* notice) {
    if (!notice) return;
    switch (notice->GetClass()) {
    case 1:
        m_trees[1].AddWithId(notice);
        m_flags.m_value |= kFlagDialogWaiting;
        return;
    case 2:
        m_trees[1].AddWithId(notice);
        m_flags.m_value |= kFlagReportWaiting;
        return;
    case 3:
    case 4:
    case 5:
    case 8: {
        UiTextNotice* text = static_cast<UiTextNotice*>(notice);
        if (!text->m_text0.m_text) {
            delete notice;
        } else {
            unsigned previous = GetStateValue(static_cast<int>(notice->m_categories));
            m_trees[0].AddWithId(notice);
            m_unk50 |= notice->m_categories;
            UiClearButtonHighlights(notice->m_categories);
            if (notice->m_unk40 != 0 && (previous & 4) == 0) {
                static_cast<AdvisorBase*>(m_advisor)->ScheduleCue(notice->m_unk40);
                RefreshRecords();
                UiRefreshMessageIndex();
                return;
            }
        }
        RefreshRecords();
        UiRefreshMessageIndex();
        return;
    }
    case 6:
        m_trees[2].AddWithId(notice);
        return;
    case 7:
        m_trees[0].AddWithId(notice);
        UiOpenType1cWindow(reinterpret_cast<LPARAM>(notice));
        return;
    default:
        delete notice;
    }
}

void PlayerDispatch::RemoveRecord(int id) {
    if (m_trees[0].FindById(id)) {
        IdNode* node = m_trees[0].DetachById(id);
        if (node) delete node;
    }
}

unsigned PlayerDispatch::GetStateValue(int key) {
    IdValueNode* node = static_cast<IdValueNode*>(m_states.FindById(key));
    if (node) return node->m_value;
    return m_defaultState.Get();
}

void PlayerDispatch::SetDefaultState(int state) {
    m_states.Clear();
    m_defaultState.m_value = state;
}

void PlayerDispatch::SetStateValue(int key, int value) {
    IdValueNode* node = static_cast<IdValueNode*>(m_states.FindById(key));
    if (!node) {
        node = NewOrNull<IdValueNode>(0);
        node->SetId(key);
        m_states.AddWithId(node);
    }
    node->m_value = value;
}

void PlayerDispatch::RefreshRecords() {
    if ((m_flags.m_value & kFlagHeld) != 0) return;
    AvlNode* next = m_trees[0].First();
    m_unk50 = 0;
    while (next) {
        UiNotice* notice = static_cast<UiNotice*>(next);
        next = notice->m_successor;
        if (!notice->IsStale()) {
            if ((notice->m_flags.m_value & 0x10) != 0) m_unk50 |= notice->m_categories;
        } else {
            m_trees[0].DetachById(notice->m_id);
            if ((notice->m_flags.m_value & 1) == 0) notice->Virtual13();
            delete notice;
        }
    }
    if (~m_unk50 != 0) UiSetButtonHighlights(~m_unk50);
    UiClearButtonHighlights(m_unk50);
}

void PlayerDispatch::QueueConfirmation(MessageBase* message) {
    Notice8* notice = NewOrNull<Notice8>(m_side, message);
    UiOpenOrderConfirm(reinterpret_cast<LPARAM>(notice));
}

void PlayerDispatch::OnChatMessage(MessageBase* message) {
    TextMessage* chat = nullptr;
    if (message && message->GetKind() == 0x11) chat = static_cast<TextMessage*>(message);
    if (chat->m_side == m_side) return;
    Notice10* notice = NewOrNull<Notice10>(chat->m_text.m_text, m_side, 1);
    if (notice->m_unk40 != 0) {
        notice->SetId(UiNextId());
        AddNotice(notice);
    }
}

void PlayerDispatch::QueueChat(String* text, int) {
    MpChatMsg* message = NewOrNull<MpChatMsg>();
    message->m_text.Assign(*text);
    message->m_side = m_side;
    UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(message), 0);
}

int PlayerDispatch::IsIdle() {
    if ((m_flags.m_value & kFlagDialogShown) == 0) {
        if (!m_trees[2].First()) return 1;
        m_flags.m_value |= kFlagIdleWait;
    }
    return 0;
}

void PlayerDispatch::ClearAlerts(unsigned mask) {
    if ((m_unk50 & mask) == 0) {
        if (mask == 0) {
            m_unk50 = 0;
            UiSetButtonHighlights(mask);
        }
    } else {
        m_unk50 = ~mask & m_unk50;
        UiSetButtonHighlights(mask);
    }
    for (AvlNode* node = m_trees[0].First(); node; node = node->m_successor) {
        UiNotice* notice = static_cast<UiNotice*>(node);
        if ((notice->m_categories & mask) != 0 || mask == 0) notice->m_flags.m_value &= 0xffffffef;
    }
}

void PlayerDispatch::AddCharacterNotice(const IdNode* record) {
    Notice42* notice = NewOrNull<Notice42>(m_side, record);
    if (notice->m_unk40 != 0) {
        notice->SetId(UiNextId());
        AddNotice(notice);
    }
}

void PlayerDispatch::SetHold(int on) {
    if (on != 0) {
        m_flags.m_value |= kFlagHeld;
        return;
    }
    m_flags.m_value &= ~kFlagHeld;
    RefreshRecords();
}

UiNotice* PlayerDispatch::CreateNotice(int kind, LinkNode* payloads) {
    UiNotice* notice = nullptr;
    switch (kind) {
    case 1: {
        IdNode* vote = static_cast<IdNode*>(m_trees[3].First());
        if (vote) {
            notice = NewOrNull<Notice1>(payloads, m_side, static_cast<MessageBase*>(vote));
            m_trees[3].DetachById(vote->m_id);
            delete vote;
        }
        break;
    }
    case 0x03: notice = NewOrNull<Notice3>(payloads, m_side); break;
    case 0x04: notice = NewOrNull<Notice4>(payloads, m_side); break;
    case 0x05: notice = NewOrNull<Notice5>(payloads, m_side); break;
    case 0x06: notice = NewOrNull<Notice6>(payloads, m_side); break;
    case 0x07: notice = NewOrNull<Notice7>(payloads, m_side); break;
    case 0x09: notice = NewOrNull<Notice9>(payloads, m_side); break;
    case 0x0b: notice = NewOrNull<Notice11>(payloads, m_side); break;
    case 0x0c: notice = NewOrNull<Notice12>(payloads, m_side); break;
    case 0x0d: notice = NewOrNull<Notice13>(payloads, m_side); break;
    case 0x0e: notice = NewOrNull<Notice14>(payloads, m_side); break;
    case 0x0f: notice = NewOrNull<Notice15>(payloads, m_side); break;
    case 0x10: notice = NewOrNull<Notice16>(payloads, m_side); break;
    case 0x13: notice = NewOrNull<Notice19>(payloads, m_side); break;
    case 0x14: notice = NewOrNull<Notice20>(payloads, m_side); break;
    case 0x15: notice = NewOrNull<Notice21>(payloads, m_side); break;
    case 0x16: notice = NewOrNull<Notice22>(payloads, m_side); break;
    case 0x17: notice = NewOrNull<Notice23>(payloads, m_side); break;
    case 0x18: notice = NewOrNull<Notice24>(payloads, m_side); break;
    case 0x19: notice = NewOrNull<Notice25>(payloads, m_side); break;
    case 0x1a: notice = NewOrNull<Notice26b>(payloads, m_side); break;
    case 0x1b: notice = NewOrNull<Notice26a>(payloads, m_side); break;
    case 0x1c: notice = NewOrNull<Notice28>(payloads, m_side); break;
    case 0x1d: notice = NewOrNull<Notice29>(payloads, m_side); break;
    case 0x1e: notice = NewOrNull<Notice30>(payloads, m_side); break;
    case 0x1f: notice = NewOrNull<Notice31>(payloads, m_side); break;
    case 0x20: notice = NewOrNull<Notice32>(payloads, m_side); break;
    case 0x21: notice = NewOrNull<Notice33>(payloads, m_side); break;
    case 0x22: notice = NewOrNull<Notice34>(payloads, m_side); break;
    case 0x23: notice = NewOrNull<Notice35>(payloads, m_side); break;
    case 0x24: notice = NewOrNull<Notice36>(payloads, m_side); break;
    case 0x25: notice = NewOrNull<Notice37>(payloads, m_side); break;
    case 0x26: notice = NewOrNull<Notice38>(payloads, m_side); break;
    case 0x27: notice = NewOrNull<Notice39>(payloads, m_side); break;
    case 0x28: notice = NewOrNull<Notice40>(payloads, m_side); break;
    case 0x29: notice = NewOrNull<Notice41>(payloads, m_side); break;
    case 0x2b: notice = NewOrNull<Notice43>(payloads, m_side); break;
    case 0x2c: notice = NewOrNull<Notice44>(payloads, m_side); break;
    case 0x2d: notice = NewOrNull<Notice45>(payloads, m_side); break;
    default: break;
    }
    if (notice) {
        if (notice->m_unk40 == 0) {
            delete notice;
            notice = nullptr;
        } else {
            notice->SetId(UiNextId());
        }
    }
    return notice;
}

int PlayerDispatch::ClassifyNotifications(LinkNode* payloads) {
    int kind = 0;
    int fallback = 0;
    if (!payloads) return kind;
    for (LinkNode* node = payloads->m_next; node; node = node->m_next) {
        if (kind != 0) return kind;
        KeyedNotifPayload* keyed = static_cast<KeyedNotifPayload*>(node);
        IntPairPayload* ints = static_cast<IntPairPayload*>(keyed);
        switch (keyed->GetKind()) {
        case 0x100:
            if (inFamilies(keyed->m_key, 0x90, 0x98)) kind = 4;
            break;
        case 0x105: {
            ObjectKey unset;
            if (ints->m_valueB == 0) {
                GameObjNotifier* object = GetObjectInFamilies_01_ff(m_side, &keyed->m_key);
                if (object) {
                    kind = 0xd;
                    if ((static_cast<GameObj*>(object)->m_packed & 0xff00) == 0x100) {
                        ObjectKey objectKey;
                        object->GetKey(&objectKey);
                        if (objectKey.m_value != unset.m_value) kind = 0;
                    }
                }
            }
            break;
        }
        case 0x106:
            if (inFamilies(keyed->m_key, 0x30, 0x3c))
                kind = 0x1a;
            else if (ints->m_valueB == 0)
                kind = 6;
            break;
        case 0x107: kind = 0xe; break;
        case 0x125:
        case 0x126:
        case 0x127: kind = 5; break;
        case 0x128:
            if ((ints->m_valueB & 1) == 0 || (ints->m_valueA & 1) != 0) {
                if ((ints->m_valueB & 0x100) != 0 && (ints->m_valueA & 0x100) == 0) kind = 0x1e;
            } else {
                kind = 0x1f;
            }
            break;
        case 0x129:
        case 0x12a:
        case 0x12b: kind = 0x2c; break;
        case 0x12c: kind = 0x2d; break;
        case 0x145: kind = 9; break;
        case 0x148: kind = 0xb; break;
        case 0x14b: kind = 3; break;
        case 0x14d: kind = 1; break;
        case 0x14e: kind = 7; break;
        case 0x14f:
            if (ints->m_valueB == 0) {
                ObjectKey subject(keyed->m_key);
                GameObjNotifier* system = GetObjectInFamilies_90_98(m_side, &subject);
                kind = 0xf;
                if (system && (static_cast<GameObj*>(system)->m_stateFlags & 8) != 0) kind = 0;
            }
            break;
        case 0x150: {
            kind = 0xf;
            if (keyed->m_key.m_value == 0x90000109 && ints->m_valueB != 0 && ints->m_valueA == 2) {
                ObjectKey system;
                system.m_value = 0x90000109;
                GameObjNotifier* object = GetObjectInFamilies_90_98(m_side, &system);
                if (object->ControlSide() == 1) kind = 0;
            }
            break;
        }
        case 0x153: kind = 0x18; break;
        case 0x154:
        case 0x155: kind = 0x20; break;
        case 0x160:
            if (ints->m_valueB == 0) kind = 0x21;
            break;
        case 0x1e0: kind = 0x19; break;
        case 0x1e1: kind = 0x1d; break;
        case 0x1f0: {
            GameObjNotifier* object = GetObjectInFamilies_40_80(m_side, &keyed->m_key);
            if (object && static_cast<Mission*>(object)->GetRecordHidden() == 0) {
                if (!inFamilies(keyed->m_key, 0x40, 0x50) && ints->m_valueB != 0) kind = 0x16;
                if (keyed->m_key.Family() == 0x6a && ints->m_valueB == 3) kind = 0;
                if (m_side == 1) {
                    if (object->ControlSide() != 1) kind = 0;
                } else if (m_side == 2 && object->ControlSide() != 2) {
                    kind = 0;
                }
            }
            break;
        }
        case 0x1f1:
            if (ints->m_valueB != 0 && !inFamilies(keyed->m_key, 0x40, 0x50)) {
                GameObjNotifier* object = GetObjectInFamilies_40_80(m_side, &keyed->m_key);
                if (object && static_cast<Mission*>(object)->GetRecordHidden() == 0 &&
                    static_cast<Mission*>(object)->m_taskStatus == 0)
                    kind = 0x17;
            }
            break;
        case 0x200: {
            UiNotif200* mission = static_cast<UiNotif200*>(keyed);
            if (inFamilies(mission->m_missionKey, 0x40, 0x50)) {
                if (ints->m_valueB == 0 && fallback == 0) {
                    ObjectKey subject(keyed->m_key);
                    if (!inFamilies(subject, 0x3c, 0x40))
                        fallback = 0x14;
                    else
                        GetObjectInFamilies_3c_40(m_side, &subject);
                }
            } else if (mission->m_missionKey.Family() == 100 && ints->m_valueB != 0) {
                kind = 0x28;
            }
            break;
        }
        case 0x210:
        case 0x221: kind = 0x25; break;
        case 0x212: kind = 0x1b; break;
        case 0x220:
        case 0x360: kind = 0x24; break;
        case 0x230: kind = 0x26; break;
        case 0x231: kind = 0x29; break;
        case 0x302:
            if (inFamilies(keyed->m_key, 0x90, 0x98)) kind = 0x15;
            break;
        case 0x303: kind = 0x10; break;
        case 0x304: kind = 0xc; break;
        case 0x305:
            if (inFamilies(keyed->m_key, 0x18, 0x1c))
                kind = 0x23;
            else
                fallback = 0x22;
            break;
        case 0x306: kind = 0x2b; break;
        case 0x340: kind = 0x13; break;
        case 0x361: kind = 0x27; break;
        case 0x362: kind = 0x1c; break;
        default: break;
        }
    }
    return kind != 0 ? kind : fallback;
}
