#include "character/character.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "events/notif_event_payloads.h"
#include "events/player_notice_util.h"
#include "events/player_notices.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "mission/mission_recruit.h"
#include "mission/mission_research.h"
#include "orders/order_base.h"
#include "story/story_characters.h"
#include "strategy_ui/advisor_notices.h"
#include "ui/message_template.h"

namespace {

constexpr uint32_t kStateDestroyed = 0x8;
constexpr uint32_t kStateFlag200 = 0x200;
constexpr uint32_t kCapturedFlag = 0x1;
constexpr uint32_t kStoryFlagSpecial = 0x8;

uint32_t stateFlags(GameObjNotifier* object) { return static_cast<GameObj*>(object)->m_stateFlags; }
bool isCaptured(GameObjNotifier* character) { return (static_cast<Character*>(character)->m_flagsAc & kCapturedFlag) != 0; }
uint16_t sideVariant(int owner, uint16_t base) { return static_cast<uint16_t>((owner != 1) + base); }

void say(String* text, uint32_t id, GameObjNotifier* a = nullptr, GameObjNotifier* b = nullptr, GameObjNotifier* c = nullptr) {
    ExpandMessageTemplate(text, id, notice::nameOf(a), notice::nameOf(b), notice::nameOf(c), nullptr);
}

enum MissionFamily : uint32_t {
    kFamily51 = 0x51,
    kFamily52 = 0x52,
    kResearchFamily = 0x53,
    kFamily54 = 0x54,
    kFamily55 = 0x55,
    kFamily56 = 0x56,
    kFamily57 = 0x57,
    kFamily58 = 0x58,
    kFamily61 = 0x61,
    kFamily62 = 0x62,
    kFamily63 = 0x63,
    kFamily69 = 0x69,
    kFamily6a = 0x6a,
};

GameObjNotifier* findTeamLocation(int owner, ObjectKeyValueSet& members) {
    GameObjNotifier* found = nullptr;
    bool keepGoing = true;
    for (ObjectKeyNode* node = members.First(); node && keepGoing; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey nodeKey(node->m_key);
        found = GetObjectInFamilies_01_ff(owner, &nodeKey);
        if (!found) continue;
        if ((stateFlags(found) & kStateDestroyed) != 0)
            found = nullptr;
        else if (notice::inFamilies(nodeKey, 0x30, 0x3c) && isCaptured(found))
            found = nullptr;
        while (found && keepGoing) {
            ObjectKey key;
            found->GetKey(&key);
            if (notice::inFamilies(key, 0x90, 0x98)) {
                keepGoing = false;
                continue;
            }
            if (notice::inFamilies(key, 8, 0x10)) {
                keepGoing = false;
                break;
            }
            found = found->m_parent;
        }
    }
    return found;
}

// The facts the per-mission texts are built from.
struct Started {
    Notice22* notice;
    GameObjNotifier* mission;
    GameObjNotifier* leader;
    GameObjNotifier* target;
    GameObjNotifier* found;
    ObjectKeyValueSet* stash;
    int outcome;
    bool story;

    String* text0() const { return &notice->m_text0; }
    String* text1() const { return &notice->m_text1; }
    void giveUp() const { notice->m_unk40 = 0; }
    void foundLocation() const {
        if (found) say(text1(), 0x70e6, found);
    }
    void offerOrder() const {
        MessageBase* message = static_cast<MessageBase*>(CreateMessageByKind(0x250));
        notice->m_order = message;
        if (!message) return;
        OrderBase* order = static_cast<OrderBase*>(message);
        order->m_side = notice->m_owner;
        ObjectKey missionKey;
        mission->GetKey(&missionKey);
        stash->AddKeyValue(&missionKey, 0);
        order->SetTeam(stash);
        ObjectKey unset;
        order->SetTarget(&unset);
    }
};

// ---- the case for every kind of mission ----

void recruitStarted(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 5;
    s.offerOrder();
    if (s.story) {
        say(s.text0(), 0x70d0, s.leader, s.target);
        if (s.outcome == 1)
            say(s.text1(), 0x70d3, s.leader, s.target);
        else if (s.outcome == 2)
            say(s.text1(), 0x70d2, s.leader, s.target);
        else if (s.outcome == 3)
            say(s.text1(), 0x70d1, s.leader, s.target);
        else
            s.giveUp();
    } else {
        say(s.text0(), 0x70d4, s.leader, s.target);
        if (s.outcome == 1)
            say(s.text1(), 0x70d7, s.target);
        else if (s.outcome == 2)
            say(s.text1(), 0x70d6, s.target);
        else if (s.outcome == 3)
            say(s.text1(), 0x70d5, s.target);
        else
            s.giveUp();
    }
    const uint32_t loyalty = static_cast<System*>(s.target)->m_controlData.Loyalty();
    if (loyalty == 1)
        say(s.text1(), 0x70d9);
    else if (loyalty == 2)
        say(s.text1(), 0x70d8);
    else
        say(s.text1(), 0x70da);
    say(s.text1(), 0x70db);
}

void family52Started(Started& s) {
    Notice22* n = s.notice;
    n->m_unk40 = 1;
    n->m_class = 3;
    n->m_key = ObjectKey(static_cast<Mission*>(s.mission)->m_targetKey);
    if (s.story) {
        say(s.text0(), 0x7108, s.leader, s.target);
        if (s.outcome == 2) {
            say(s.text1(), 0x710a, s.leader, s.target);
        } else if (s.outcome == 3) {
            say(s.text1(), 0x7109, s.leader, s.target);
            n->ListSecondKeys();
        } else {
            s.giveUp();
        }
    } else {
        say(s.text0(), 0x710b, s.target);
        if (s.outcome == 2) {
            say(s.text1(), 0x710d, s.target);
        } else if (s.outcome == 3) {
            say(s.text1(), 0x710c, s.target);
            n->ListSecondKeys();
        } else {
            s.giveUp();
        }
    }
    s.foundLocation();
}

void researchStarted(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 5;
    s.offerOrder();
    const int orders = static_cast<MissionResearch*>(s.mission)->m_rdOrders;
    if (s.story) {
        say(s.text0(), 0x7120, s.leader, s.target, s.mission);
        if (s.outcome == 2)
            say(s.text1(), 0x7123, s.leader, s.target, s.mission);
        else if (s.outcome == 3)
            say(s.text1(), orders == 0 ? 0x7121 : 0x7122, s.leader, s.target, s.mission);
        else
            s.giveUp();
    } else {
        say(s.text0(), 0x7124, s.target, s.mission);
        if (s.outcome == 2) {
            say(s.text1(), 0x7127, s.target, s.mission);
        } else if (s.outcome == 3) {
            say(s.text1(), orders == 0 ? 0x7125 : 0x7126, s.target, s.mission);
        } else {
            s.giveUp();
        }
    }
    say(s.text1(), 0x70db);
}

void family54Started(Started& s) {
    Notice22* n = s.notice;
    if (!s.leader) return;
    say(s.text0(), 0x70e8);
    if (s.outcome == 2)
        say(s.text1(), 0x70ea, s.target);
    else if (s.outcome == 3)
        say(s.text1(), 0x70e9, s.target);
    else
        s.giveUp();
    s.foundLocation();
    n->m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(s.leader));
}

void recruitCharacterStarted(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 3;
    if (!s.story) return;
    if (s.outcome == 2) {
        say(s.text0(), 0x7118, s.leader, s.target);
        say(s.text1(), 0x711a, s.leader, s.target);
    } else if (s.outcome == 3) {
        GameObjNotifier* recruit = GetObjectInFamilies_01_ff(n->m_owner, &static_cast<MissionRecruit*>(s.mission)->m_recruitedCharacter);
        say(s.text0(), 0x711b, s.leader, s.target, recruit);
        say(s.text1(), 0x7119, s.leader, s.target, recruit);
    } else {
        s.giveUp();
    }
}

void family56Started(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 5;
    s.offerOrder();
    const bool uprising = (static_cast<System*>(s.target)->m_flags & system_flag::kUprising) != 0;
    if (s.story) {
        say(s.text0(), 0x7100, s.leader, s.target);
        if (s.outcome == 2) {
            say(s.text1(), uprising ? 0x7107 : 0x7102, s.leader, s.target);
            say(s.text1(), 0x70db);
        } else if (s.outcome == 3) {
            n->m_picture[1] = 0x3ed;
            say(s.text1(), 0x7101, s.leader, s.target);
            n->m_class = 3;
        } else {
            s.giveUp();
        }
    } else {
        say(s.text0(), 0x7103, s.target);
        if (s.outcome == 2) {
            say(s.text1(), uprising ? 0x7106 : 0x7105, s.target);
            say(s.text1(), 0x70db);
        } else if (s.outcome == 3) {
            n->m_picture[1] = 0x3ed;
            say(s.text1(), 0x7104, s.target);
            n->m_class = 3;
        } else {
            s.giveUp();
        }
    }
}

void family57Started(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 5;
    s.offerOrder();
    if (s.story) {
        say(s.text0(), 0x7110, s.leader, s.target);
        if (s.outcome == 2) {
            say(s.text1(), 0x7112, s.leader, s.target);
            say(s.text1(), 0x70db);
            n->m_picture[1] = 0x3f2;
            n->m_sound[0] = 0x454;
        } else if (s.outcome == 3) {
            n->m_sound[0] = 0x455;
            n->m_picture[1] = sideVariant(n->m_owner, 0x3f3);
            say(s.text1(), 0x7111, s.leader, s.target);
            n->m_class = 3;
        } else {
            s.giveUp();
        }
    } else {
        say(s.text0(), 0x7113, s.target);
        if (s.outcome == 2) {
            say(s.text1(), 0x7115, s.target);
            n->m_picture[1] = 0x3f2;
            n->m_sound[0] = 0x454;
            say(s.text1(), 0x70db);
        } else if (s.outcome == 3) {
            n->m_sound[0] = 0x455;
            n->m_picture[1] = sideVariant(n->m_owner, 0x3f3);
            say(s.text1(), 0x7114, s.target);
            n->m_class = 3;
        } else {
            s.giveUp();
        }
    }
}

void family58Started(Started& s) {
    Notice22* n = s.notice;
    Mission* mission = static_cast<Mission*>(s.mission);
    n->m_class = 3;
    GameObjNotifier* second = GetObjectInFamilies_01_ff(n->m_owner, &mission->m_objectiveKey);
    n->m_key = ObjectKey(mission->m_targetLocationKey);
    if (s.story) {
        say(s.text0(), 0x71c0, s.leader, s.target, second);
        if (s.outcome == 2)
            say(s.text1(), 0x71c2, s.leader, s.target, second);
        else if (s.outcome == 3)
            say(s.text1(), 0x71c1, s.leader, s.target, second);
        else
            s.giveUp();
    } else {
        say(s.text0(), 0x71c3, s.target, second);
        if (s.outcome == 2)
            say(s.text1(), 0x71c5, s.target, second);
        else if (s.outcome == 3)
            say(s.text1(), 0x71c4, s.target, second);
        else
            s.giveUp();
    }
}

void family61Started(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 3;
    GameObjNotifier* second = GetObjectInFamilies_01_ff(n->m_owner, &static_cast<Mission*>(s.mission)->m_objectiveKey);
    if (s.story) {
        say(s.text0(), 0x7130, s.leader, s.target, second);
        if (s.outcome == 2)
            say(s.text1(), 0x7132, s.leader, s.target, second);
        else if (s.outcome == 3)
            say(s.text1(), 0x7131, s.leader, s.target, second);
        else {
            s.giveUp();
            s.foundLocation();
            return;
        }
    } else {
        say(s.text0(), 0x7133, s.target, second);
        if (s.outcome == 2)
            say(s.text1(), 0x7135, s.target, second);
        else if (s.outcome == 3)
            say(s.text1(), 0x7134, s.target, second);
        else {
            s.giveUp();
            s.foundLocation();
            return;
        }
    }
    s.foundLocation();
}

void family62Started(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 3;
    GameObjNotifier* second = GetObjectInFamilies_01_ff(n->m_owner, &static_cast<Mission*>(s.mission)->m_objectiveKey);
    if (s.story) {
        say(s.text0(), 0x70f0, s.leader, s.target, second);
        if (s.outcome == 2) {
            say(s.text1(), 0x70f2, s.leader, s.target, second);
        } else if (s.outcome == 3) {
            say(s.text1(), (stateFlags(second) & kStateDestroyed) == 0 ? 0x70f1 : 0x70f6, s.leader, s.target, second);
        } else {
            s.giveUp();
        }
    } else {
        say(s.text0(), 0x70f3, s.target, second);
        if (s.outcome == 2) {
            say(s.text1(), 0x70f5, s.target, second);
        } else if (s.outcome == 3) {
            say(s.text1(), (stateFlags(second) & kStateDestroyed) == 0 ? 0x70f4 : 0x70f7, s.target, second);
        } else {
            s.giveUp();
            s.foundLocation();
            return;
        }
    }
    s.foundLocation();
}

void family63Started(Started& s) {
    Notice22* n = s.notice;
    n->m_class = 3;
    GameObjNotifier* second = GetObjectInFamilies_01_ff(n->m_owner, &static_cast<Mission*>(s.mission)->m_objectiveKey);
    if (s.story) {
        say(s.text0(), 0x70f8, s.leader, s.target, second);
        if (s.outcome == 2) {
            say(s.text1(), (stateFlags(second) & kStateFlag200) == 0 ? 0x70fa : 0x70ff, s.leader, s.target, second);
        } else if (s.outcome == 3) {
            if ((stateFlags(second) & kStateDestroyed) != 0) say(s.text1(), 0x70f9, s.leader, s.target, second);
        } else {
            s.giveUp();
        }
    } else {
        say(s.text0(), 0x70fb, s.target, second);
        if (s.outcome == 2) {
            say(s.text1(), 0x70fd, s.target, second);
        } else if (s.outcome == 3) {
            say(s.text1(), (stateFlags(second) & kStateDestroyed) == 0 ? 0x70fe : 0x70fc, s.target, second);
        } else {
            s.giveUp();
            s.foundLocation();
            return;
        }
    }
    s.foundLocation();
}

void family69Started(Started& s) {
    Notice22* n = s.notice;
    Mission* mission = static_cast<Mission*>(s.mission);
    n->m_unk40 = 1;
    n->m_class = 3;
    GameObjNotifier* other = GetObjectInFamilies_01_ff(n->m_owner, &mission->m_objectiveKey);
    n->m_key = ObjectKey(mission->m_targetKey);
    if (s.story) {
        say(s.text0(), 0x70e0, s.leader, s.target);
        say(s.text1(), 0x70e3, s.leader, s.target, other);
    } else {
        say(s.text0(), 0x70e1, s.target);
        say(s.text1(), 0x70e2, s.target, other);
    }
    if (s.outcome == 2)
        say(s.text1(), 0x70e5, other);
    else if (s.outcome == 3)
        say(s.text1(), 0x70e4, other);
    else
        s.giveUp();
    s.foundLocation();
}

// What the leader's identity adds to every notice of a mission start: advice line, picture and the notice's priority.
void finishLeader(Notice22* n, GameObjNotifier* leader, bool outcomeIsThree) {
    const ObjectKey leaderKey(n->m_key74);
    if (notice::inFamilies(leaderKey, 0x30, 0x38)) {
        switch (leaderKey.Id()) {
        case 0x240:
            n->m_unk28 = 0x17;
            n->m_sound[1] = AdvisorPickAdviceLine(outcomeIsThree ? 0xf2 : 0xf3);
            break;
        case 0x241:
            n->m_unk28 = 0x18;
            n->m_sound[1] = AdvisorPickAdviceLine(outcomeIsThree ? 0xde : 0xdf);
            break;
        case 0x242: {
            n->m_unk28 = 0x16;
            uint32_t notSpecial = 0;
            if (leader) notSpecial = ~static_cast<StoryCharacter*>(leader)->m_storyFlags >> 3 & 1;
            const uint32_t base = outcomeIsThree ? 0xd4 : 0xd5;
            n->m_sound[1] = AdvisorPickAdviceLine(notSpecial != 0 ? base - 10 : base);
            break;
        }
        case 0x243:
            n->m_unk28 = 0x19;
            n->m_sound[1] = AdvisorPickAdviceLine(outcomeIsThree ? 0xe8 : 0xe9);
            break;
        case 0x280:
            n->m_unk28 = 0x1a;
            n->m_sound[1] = AdvisorPickAdviceLine(outcomeIsThree ? 0x106 : 0x107);
            break;
        case 0x281:
            n->m_unk28 = 0x1b;
            n->m_sound[1] = AdvisorPickAdviceLine(outcomeIsThree ? 0xfc : 0xfd);
            break;
        default:
            break;
        }
    } else {
        n->m_unk28 = 0x15;
    }
    if (notice::inFamilies(n->m_key74, 0x30, 0x3c)) {
        GameObjNotifier* character = GetObjectInFamilies_01_ff(n->m_owner, &n->m_key74);
        if (character) n->m_picture[0] = AdvisorRecordTypeResource(static_cast<GameObj*>(character));
    } else {
        GameObjNotifier* object = GetObjectInFamilies_01_ff(n->m_owner, &n->m_key74);
        if (object) n->m_picture[1] = AdvisorRecordTypeResource(static_cast<GameObj*>(object));
    }
    n->m_key = ObjectKey(n->m_key74);
}

}  // namespace

// Event 0x1f0: a mission of the side's character has started; each kind of mission has its own set of texts.
void Notice22::Virtual05(int kind, intptr_t payload) {
    ObjectKeyValueSet stash;
    ObjectKey missionKey;
    bool outcomeIsThree = false;
    if (kind != 0x1f0) return;
    const IntPairPayload* started = reinterpret_cast<const IntPairPayload*>(payload);
    missionKey = ObjectKey(started->m_key);
    m_key78 = ObjectKey(started->m_key);
    GameObjNotifier* leader = nullptr;
    GameObjNotifier* mission = GetObjectInFamilies_40_80(m_owner, &missionKey);
    if (m_f64 != 0) return;
    m_f64 = 1;
    m_unk40 = 1;
    m_picture[1] = sideVariant(m_owner, 0x412);
    m_sound[0] = sideVariant(m_owner, 0x47c);
    switch (missionKey.Family()) {
    case kFamily52:
        m_picture[1] = 0x415;
        m_f80 = kFamily52;
        m_sound[0] = 0x480;
        break;
    case kFamily51:
        m_f80 = kFamily51;
        m_picture[1] = 0x414;
        m_sound[0] = 0x47e;
        break;
    case kResearchFamily:
        m_f80 = kResearchFamily;
        m_sound[0] = 0x47f;
        m_picture[1] = 0x3f9;
        break;
    case kFamily54:
    case kFamily55:
    case kFamily56:
    case kFamily58:
    case kFamily61:
    case kFamily62:
    case kFamily63:
    case kFamily69:
        m_f80 = missionKey.Family();
        break;
    case kFamily57:
        m_f80 = kFamily57;
        m_picture[1] = 0x3f2;
        break;
    case kFamily6a:
        m_f80 = kFamily6a;
        if (started->m_valueB == 3) m_unk40 = 0;
        break;
    default:
        m_unk40 = 0;
        break;
    }

    GameObjNotifier* found = nullptr;
    if (mission) {
        if (m_unk40 != 0) {
            m_key = ObjectKey(static_cast<Mission*>(mission)->m_targetLocationKey);
            if (!notice::inFamilies(m_key, 0x90, 0x98)) m_unk40 = 0;
        }
        if (m_unk40 != 0) {
            found = findTeamLocation(m_owner, static_cast<Mission*>(mission)->m_finishedMembers);
            m_key74 = ObjectKey(static_cast<Mission*>(mission)->m_leaderKey);
            leader = GetObjectInFamilies_01_ff(m_owner, &m_key74);
            outcomeIsThree = started->m_valueB == 3;
            GameObjNotifier* target = GetObjectInFamilies_90_98(m_owner, &m_key);
            Started s{this, mission, leader, target, found, &stash, started->m_valueB, notice::inFamilies(m_key74, 0x30, 0x38)};
            switch (m_f80) {
            case kFamily51: recruitStarted(s); break;
            case kFamily52: family52Started(s); break;
            case kResearchFamily: researchStarted(s); break;
            case kFamily54: family54Started(s); break;
            case kFamily55: recruitCharacterStarted(s); break;
            case kFamily56: family56Started(s); break;
            case kFamily57: family57Started(s); break;
            case kFamily58: family58Started(s); break;
            case kFamily61: family61Started(s); break;
            case kFamily62: family62Started(s); break;
            case kFamily63: family63Started(s); break;
            case kFamily69: family69Started(s); break;
            default: m_unk40 = 0; break;
            }
        }
    }
    finishLeader(this, leader, outcomeIsThree);
}
