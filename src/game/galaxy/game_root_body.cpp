#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "data/random.h"
#include "economy/manu_mgr.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/game_root.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "gameobj/object_type_db.h"
#include "galaxy/game_setup_seed.h"

// The persistence and consistency code of the game root that sits in the setup-stage range (0x518dc0-0x519800): the
// bodies behind GameRoot's Read/Write/Checksum/Validate slots (the slots themselves are 5-byte thunks at 0x513f00),
// the gameplay random generator the root owns, and the checks that every object's lock count and builder are
// accounted for.

// Loads the rule tables for the game's difficulty (0x513f70).
int BindRuleTables(int difficulty);

namespace {

constexpr uint32_t kViewSideMask = 0x30;
constexpr int kGameSettingsChecksumSlot = 3;
constexpr uint32_t kCreatedFlag = 0x2;
constexpr uint32_t kUnfinishedMask = 0x1c;
constexpr size_t kMissionKeyOffset = 0x70;
constexpr size_t kCharacterMissionKeyOffset = 0x68;
constexpr FamilyRange kUnitFamilies(0x10, 0x40);
constexpr FamilyRange kMissionFamilies(0x40, 0x80);
constexpr FamilyRange kCharacterFamilies(0x30, 0x40);
constexpr FamilyRange kAllFamilies(1, 0xff);

template <typename R, typename... A>
R callVirtual(void* object, int slot, A... args) {
    using Function = R(__thiscall*)(void*, A...);
    return reinterpret_cast<Function**>(object)[0][slot](object, args...);
}

GameObj* asGameObj(GameObjNotifier* object) { return static_cast<GameObj*>(object); }

// The object key stored at `offset` of a mission or character (their classes are declared elsewhere).
ObjectKey* keyAt(GameObjNotifier* object, size_t offset) {
    return reinterpret_cast<ObjectKey*>(reinterpret_cast<uint8_t*>(object) + offset);
}

// Adds one to the counter of `key` in `counts`, creating the counter first. Returns 0 if it could not be created.
int countKey(ObjectKeyValueSet* counts, ObjectKey* key, int ok) {
    if (key->OrNull() == nullptr) return ok;
    ObjectKeyNode* node = counts->Find(key);
    if (node == nullptr) {
        ok = counts->AddKeyValue(key, 0);
        if (ok == 0) return 0;
        node = counts->Find(key);
    }
    if (ok == 0) return 0;
    ok = node != nullptr;
    if (ok) static_cast<ObjectKeyValueNode*>(node)->m_value += 1;
    return ok;
}

}  // namespace

int GameRoot::BindDifficultyTables() { return BindRuleTables(m_settings.m_field0c); }

int GameRoot::ReadBody(istream* in) {
    int ok = GameObj::Read(in);
    if (ok != 0 && (m_flags & kViewSideMask) == 0) {
        ok = DestroyRandomGenerator();
        if (ok != 0) {
            ok = CreateRandomGenerator();
            if (ok != 0) {
                ok = HasRandomGenerator();
                if (ok != 0) {
                    ok = m_settings.Read(in);
                    if (ok != 0) {
                        ReadValue32(in, &m_unk6c);
                        ReadValue32(in, &m_unk70);
                        ReadValue32(in, &m_unk74);
                        ReadValue32(in, &m_unk78[0]);
                        ReadValue32(in, &m_unk78[1]);
                        ReadValue32(in, &m_unk78[2]);
                        ReadValue32(in, &m_unk78[3]);
                        ReadValue32(in, &m_unk78[4]);
                        ok = m_key8c.Read(in);
                        if (ok != 0) {
                            ok = m_ctx90.Read(in);
                            if (ok != 0) m_random->Read(in);
                        }
                    }
                }
            }
        }
    }
    return ok;
}

int GameRoot::WriteBody(ostream* out) {
    int ok = GameObj::Write(out);
    if (ok != 0 && (m_flags & kViewSideMask) == 0) {
        ok = HasRandomGenerator();
        if (ok != 0) {
            ok = m_settings.Write(out);
            if (ok != 0) {
                WriteValue32(out, &m_unk6c);
                WriteValue32(out, &m_unk70);
                WriteValue32(out, &m_unk74);
                WriteValue32(out, &m_unk78[0]);
                WriteValue32(out, &m_unk78[1]);
                WriteValue32(out, &m_unk78[2]);
                WriteValue32(out, &m_unk78[3]);
                WriteValue32(out, &m_unk78[4]);
                ok = m_key8c.Write(out);
                if (ok != 0) {
                    ok = m_ctx90.Write(out);
                    if (ok != 0) m_random->Write(out);
                }
            }
        }
    }
    return ok;
}

int GameRoot::ChecksumBody(Crc32* crc) {
    int ok = GameObj::Checksum(crc);
    if (ok != 0 && (m_flags & kViewSideMask) == 0) {
        ok = HasRandomGenerator();
        if (ok != 0) {
            ok = callVirtual<int>(&m_settings, kGameSettingsChecksumSlot, crc);
            if (ok != 0) {
                crc->AddInt(m_unk6c);
                crc->AddInt(m_unk70);
                crc->AddInt(m_unk74);
                crc->AddInt(m_unk78[0]);
                crc->AddInt(m_unk78[1]);
                crc->AddInt(m_unk78[2]);
                crc->AddInt(m_unk78[3]);
                crc->AddInt(m_unk78[4]);
                ok = m_key8c.Checksum(crc);
                if (ok != 0) ok = m_ctx90.Checksum(crc);
            }
        }
    }
    return ok;
}

int GameRoot::CreateRandomGenerator() {
    void* memory = ::operator new(sizeof(RandomGenerator));
    RandomGenerator* generator = memory ? new (memory) RandomGenerator(m_settings.m_field10) : nullptr;
    m_random = generator;
    if (generator != nullptr) SetGameRandom(generator);
    return generator != nullptr;
}

int GameRoot::DestroyRandomGenerator() {
    if (m_random != nullptr) {
        delete m_random;
        m_random = nullptr;
        SetGameRandom(nullptr);
    }
    return 1;
}

int GameRoot::HasRandomGenerator() { return m_random != nullptr; }

int GameRoot::ValidateBody() {
    int ok = GameObj::Validate();
    if (ok != 0 && (m_flags & kViewSideMask) == 0) {
        ok = m_master_f2000003 && m_master_20000005 && m_master_f8000006 && m_master_f9000007 &&
             m_master_fa000008 && m_master_f300000f && m_master_f3000010 && m_master_98000440 &&
             m_master_98000441 && m_master_98000442 && m_master_98000480 && m_master_98000481;
    }
    if (ok == 0) return ok;
    ok = ValidateBuilders();
    if (ok == 0) return ok;
    ok = ValidateLocks();
    if (ok == 0) return ok;
    for (int view = 0; view < 3 && ok != 0; ++view) {
        const ObjectKey sideRootKey = ObjectKey::Make(family::kSideRoot, 3);
        GameObjNotifier* sideRoot = LookupObject(&sideRootKey, view);
        ok = sideRoot != nullptr;
        if (ok == 0) break;
        GameObjIterator children(sideRoot, 4);
        for (children.Begin(); children.Current() != nullptr && ok != 0; children.Advance()) {
            GameObj* child = asGameObj(children.Current());
            if (child->m_stateFlags & 0x40) {
                ok = view != 0;
                continue;
            }
            GameObjIterator grandchildren(child, 4);
            const int hasGrandchildren = grandchildren.HasCurrent();
            if (!hasGrandchildren) continue;
            if (!(asGameObj(children.Current())->m_stateFlags & kCreatedFlag)) ok = view != 0;
            if (asGameObj(children.Current())->m_stateFlags & 0x8) ok = 0;
        }
    }
    return ok;
}

int GameRoot::ValidateBuilders() {
    int ok = GameObj::Validate();
    GameObjNotifier* unit = GetFirstMasterInFamilies_10_40();
    while (ok != 0 && unit != nullptr) {
        GameObj* object = asGameObj(unit);
        if ((object->m_stateFlags & kCreatedFlag) && (object->m_stateFlags & kUnfinishedMask) == 0) {
            ObjectKey builder(object->m_builderKey);
            if (builder.OrNull() != nullptr) {
                GameObjNotifier* manager = GetMasterInFamilies_a0_b0(&builder);
                ok = 0;
                if (manager != nullptr) {
                    ObjectKey unitKey;
                    object->GetKey(&unitKey);
                    ok = static_cast<ManuMgr*>(manager)->ManuMgr::IsQueued(&unitKey);
                }
            }
        }
        unit = object->FindNextSibling(&kUnitFamilies, 1);
    }
    return ok;
}

int GameRoot::ValidateLocks() {
    int ok = GameObj::Validate();
    ObjectKeyValueSet counts;
    GameObjNotifier* mission = GetFirstMasterInFamilies_40_80();
    while (ok != 0 && mission != nullptr) {
        ObjectKey key(*keyAt(mission, kMissionKeyOffset));
        ok = countKey(&counts, &key, ok);
        mission = asGameObj(mission)->FindNextSibling(&kMissionFamilies, 1);
    }
    for (int view = 1; ok != 0 && view <= 2; ++view) {
        GameObjNotifier* character = GetFirstObjectInRangeForSide(view, &kCharacterFamilies, 0);
        while (character != nullptr && ok != 0) {
            ObjectKey key(*keyAt(character, kCharacterMissionKeyOffset));
            ok = countKey(&counts, &key, ok);
            character = asGameObj(character)->FindNextSibling(&kCharacterFamilies, 0);
        }
    }
    GameObjNotifier* object = FindObjectInRange(&kAllFamilies, 0, 0);
    while (ok != 0 && object != nullptr) {
        int locks = 0;
        ok = asGameObj(object)->GetLockCount(&locks);
        if (ok != 0) {
            int expected = 0;
            ObjectKey key;
            object->GetKey(&key);
            if (ObjectKeyNode* node = counts.Find(&key)) expected = static_cast<ObjectKeyValueNode*>(node)->m_value;
            ok = locks == expected;
        }
        object = asGameObj(object)->FindNextSibling(&kAllFamilies, 0);
    }
    return ok;
}
