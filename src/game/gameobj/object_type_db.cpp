#include "gameobj/object_type_db.h"

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/vptr_store.h"
#include "data/dat_record.h"

// Pieces owned by the data-table module (0x58e5a0-0x58eda0, 0x52d720, 0x53b860), used through their addresses.
void* __thiscall FUN_0058ec40(void* storage);
void* __thiscall FUN_0058eb60(void* storage);
int __thiscall FUN_0058ec20(void* entry, int a, int b);
void* __thiscall FUN_0058ec90(FamilyTable* table, uint32_t family);
void* __thiscall FUN_0058ecc0(void* storage);
int __thiscall FUN_0058eda0(void* node, DatRecord* record);
void __thiscall FUN_0052d720(void* storage);
void __thiscall FUN_0052d760(void* list);
int __thiscall FUN_0053b860(DatRecord* record);
ObjectKey* __thiscall FUN_0042ec10(void* node, ObjectKey* out);
int __thiscall FUN_004f4270(IdAvlTree* list, void* node);
uint16_t __thiscall FUN_004f4290(IdAvlTree* list);

namespace {

constexpr size_t kEntryOkOffset = 0x1c;
constexpr size_t kEntryLoaderOffset = 0x20;
constexpr size_t kEntryRecordsOffset = 0x30;
constexpr size_t kEntrySize = 0x3c;
constexpr size_t kNameNodeSize = 0x30;
constexpr uint32_t kFirstLevelKind = 0x28;
constexpr uint32_t kEndLevelKind = 0x2c;
constexpr uint32_t kUnitFamilyFirst = 0x10;
constexpr uint32_t kUnitFamilyEnd = 0x40;
constexpr uint32_t kFamilyResourceBase = 0x642;
constexpr int kFamilyNameResources[] = {0x6b4, 0x6a9, 0x6ac, 0x6a8, 0x6ab, 0x6b2, 0x6ad, 0x6a7, 0x6b5,
                                        0x6ae, 0x6b0, 0x6a6, 0x6a4, 0x6af, 0x6a5, 0x6aa, 0x6b3};

// Embedded at +0x20 of a family entry: owns the family range and loads its records.
class FamilyLoader {
public:
    virtual ~FamilyLoader();
    virtual void Virtual01() = 0;
    virtual void Virtual02() = 0;
    virtual void Virtual03() = 0;
    virtual int Load(int arg) = 0;
};

template <class T>
T* At(void* base, size_t offset) {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(base) + offset);
}

template <class T>
const T& Field(const void* base, size_t offset) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(base) + offset);
}

IdAvlTree* RecordsOf(void* entry) { return At<IdAvlTree>(entry, kEntryRecordsOffset); }

AvlNode* NextNode(AvlNode* node) { return node->m_successor; }

SdRecord* AsRecord(AvlNode* node) { return static_cast<SdRecord*>(static_cast<DatRecord*>(static_cast<IdNode*>(node))); }

const ResourceStringId* NameResource(const DatRecord* record) {
    return reinterpret_cast<const ResourceStringId*>(&record->m_small);
}

// A name list on the stack (0x52d720 constructs it, 0x52d760 destroys it).
class NameList {
public:
    NameList() { FUN_0052d720(storage); }
    ~NameList() { FUN_0052d760(storage); }
    IdAvlTree* tree() { return reinterpret_cast<IdAvlTree*>(storage); }

private:
    alignas(8) char storage[0x10];
};

bool Available(const SdRecord* record, int view) {
    return (view == 1 && record->m_field40 != 0) || (view == 2 && record->m_field44 != 0);
}

}  // namespace

// ---- ObjectTypeDb ------------------------------------------------------------------------------------------

ObjectTypeDb::ObjectTypeDb() : m_families(nullptr) { m_ok = BuildFamilies(); }

ObjectTypeDb::~ObjectTypeDb() {
    if (m_families) {
        delete m_families;
        m_families = nullptr;
    }
    REB_KEEP_VPTR_STORE();
}

int ObjectTypeDb::BuildFamilies() {
    if (m_families) {
        delete m_families;
        m_families = nullptr;
    }
    void* memory = ::operator new(sizeof(FamilyTable));
    m_families = memory ? static_cast<FamilyTable*>(FUN_0058ec40(memory)) : nullptr;
    return m_families ? m_families->m_ok : 0;
}

int ObjectTypeDb::LoadFamilies() {
    int ok = BuildFamilies();
    if (!ok) return 0;
    for (int nameResource : kFamilyNameResources) {
        ok = AddFamily(kFamilyResourceBase, nameResource);
        if (!ok) return 0;
    }
    int unique = 1;
    ok = AreLevelsUnique(1, 0x28, &unique);
    if (!ok) return 0;
    if (unique) ok = AreLevelsUnique(1, 0x29, &unique);
    if (!ok) return 0;
    if (unique) ok = AreLevelsUnique(1, 0x2a, &unique);
    if (!ok) return 0;
    if (unique) ok = AreLevelsUnique(2, 0x28, &unique);
    if (!ok) return 0;
    if (unique) ok = AreLevelsUnique(2, 0x29, &unique);
    if (!ok) return 0;
    if (unique) ok = AreLevelsUnique(2, 0x2a, &unique);
    return ok;
}

int ObjectTypeDb::AddFamily(int a, int b) {
    void* memory = ::operator new(kEntrySize);
    void* entry = memory ? FUN_0058eb60(memory) : nullptr;
    int ok = entry != nullptr;
    if (ok) {
        ok = Field<int>(entry, kEntryOkOffset);
        if (ok) ok = FUN_0058ec20(entry, a, b);
    }
    if (entry) {
        for (AvlNode* record = RecordsOf(entry)->First(); record && ok; record = NextNode(record)) {
            DatRecord* datRecord = AsRecord(record);
            datRecord->m_name.Assign(String(NameResource(datRecord)));
        }
    }
    if (ok) ok = static_cast<int>(m_families->AddWithId(static_cast<IdNode*>(entry)));
    return ok;
}

int ObjectTypeDb::ForEachFamily(int arg) {
    int ok = 1;
    for (AvlNode* entry = m_families->First(); entry && ok; entry = NextNode(entry)) {
        ok = At<FamilyLoader>(entry, kEntryLoaderOffset)->Load(arg);
    }
    return ok;
}

DatRecord* ObjectTypeDb::FindRecord(const SeedKey* key) {
    DatRecord* record = nullptr;
    if (void* entry = FUN_0058ec90(m_families, key->m_key >> 24)) {
        record = AsRecord(RecordsOf(entry)->FindById(key->m_key & 0xffffff));
    }
    return record;
}

DatRecord* ObjectTypeDb::FirstRecordOfFamily(uint32_t family) {
    DatRecord* record = nullptr;
    if (void* entry = FUN_0058ec90(m_families, family)) record = AsRecord(RecordsOf(entry)->First());
    return record;
}

int ObjectTypeDb::ListAllRecordKeys(ObjectRefList* out) {
    out->Clear();
    int ok = m_families != nullptr;
    if (!ok) return 0;
    for (AvlNode* entry = m_families->First(); entry && ok; entry = NextNode(entry)) {
        for (AvlNode* record = RecordsOf(entry)->First(); record && ok; record = NextNode(record)) {
            ObjectKey key;
            key = ObjectKey::Make(AsRecord(record)->m_third, AsRecord(record)->m_id);
            ok = out->AddKey(key) != nullptr;
        }
    }
    return ok;
}

int ObjectTypeDb::CollectRecords(int view, uint32_t kind, int level, int whenBelow, int whenEqual, int whenAbove,
                                 IdAvlTree* out, int* maxLevel) {
    *maxLevel = 0;
    bool valid = false;
    if (view >= 1 && view <= 2) valid = kind >= kFirstLevelKind && kind < kEndLevelKind;
    if (!valid) return 0;

    int ok = m_families != nullptr;
    for (AvlNode* entry = ok ? m_families->First() : nullptr; entry && ok; entry = NextNode(entry)) {
        for (AvlNode* node = RecordsOf(entry)->First(); node && ok; node = NextNode(node)) {
            SdRecord* record = AsRecord(node);
            IntPair range;
            GetRecordKindRange(record, &range);
            const bool kindMatches = kind >= range.m_first && kind < range.m_second;
            if (!(kindMatches && FUN_0053b860(record) && Available(record, view))) continue;

            const int recordLevel = static_cast<int>(record->m_field50);
            if (*maxLevel < recordLevel) *maxLevel = recordLevel;
            int wanted = whenBelow;
            if (level <= recordLevel) wanted = recordLevel == level ? whenEqual : whenAbove;
            if (!wanted) continue;

            void* memory = ::operator new(kNameNodeSize);
            void* nameNode = memory ? FUN_0058ecc0(memory) : nullptr;
            ok = nameNode != nullptr;
            if (nameNode) {
                ok = FUN_0058eda0(nameNode, record);
                if (!ok || !(ok = FUN_004f4270(out, nameNode))) delete static_cast<IdNode*>(nameNode);
            }
        }
    }
    return ok;
}

int ObjectTypeDb::FindRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out) {
    *out = nullptr;
    NameList names;
    int maxLevel = 0;
    int ok = CollectRecords(view, kind, level, 0, 1, 0, names.tree(), &maxLevel);
    if (ok) {
        if (AvlNode* first = names.tree()->First()) {
            ObjectKey key;
            FUN_0042ec10(first, &key);
            const uint32_t family = key.Family();
            ok = 0;
            if (family >= kUnitFamilyFirst && family < kUnitFamilyEnd) {
                SeedKey seed;
                seed.m_key = key.m_value;
                *out = FindRecord(&seed);
                ok = *out != nullptr;
            }
        }
    }
    return ok;
}

int ObjectTypeDb::FindNextLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue) {
    *found = 0;
    *foundLevel = 0;
    *extraValue = 0;
    NameList names;
    int maxLevel = 0;
    DatRecord* record = nullptr;
    int ok = CollectRecords(view, static_cast<uint32_t>(kind), 0, 0, 0, 0, names.tree(), &maxLevel);
    for (int candidate = level + 1; ok && candidate <= maxLevel && *found == 0; ++candidate) {
        ok = FindRecordAtLevel(view, static_cast<uint32_t>(kind), candidate, &record);
        if (ok) {
            *found = record != nullptr;
            if (*found) {
                *foundLevel = candidate;
                *extraValue = static_cast<int>(static_cast<SdRecord*>(record)->m_field54);
            }
        }
    }
    return ok;
}

int ObjectTypeDb::AreLevelsUnique(int view, uint32_t kind, int* unique) {
    *unique = 1;
    NameList names;
    int maxLevel = 0;
    int ok = CollectRecords(view, kind, 0, 0, 0, 0, names.tree(), &maxLevel);
    for (int level = 1; ok && level <= maxLevel; ++level) {
        NameList atLevel;
        ok = CollectRecords(view, kind, level, 0, 1, 0, atLevel.tree(), &maxLevel);
        if (ok) {
            const int count = FUN_004f4290(atLevel.tree());
            if (count != 0 && count != 1) *unique = 0;
        }
    }
    return ok;
}

void ObjectTypeDb::DescribeKey(const SeedKey* key, String* out) {
    out->Assign(String(""));
    if (key->m_key == 0) {
        out->Assign(String("<empty SeedKey>"));
        return;
    }
    const uint32_t id = key->m_key & 0xffffff;
    if (DatRecord* record = FindRecord(key)) {
        *out += String(NameResource(record));
        *out += String(" (ID=");
        out->AppendUnsigned(id);
        *out += String(")");
        return;
    }
    *out += String("<can't find name, type = ");
    out->AppendUnsigned(key->m_key >> 24);
    *out += String("> ");
    *out += String(" (ID=");
    out->AppendUnsigned(id);
    *out += String(")");
}

// ---- singleton ---------------------------------------------------------------------------------------------

ObjectTypeDb* GetObjectTypeDb() { return g_objectTypeDb; }

void CreateObjectTypeDb() {
    DestroyObjectTypeDb();
    g_objectTypeDb = NewOrNull<ObjectTypeDb>();
    if (g_objectTypeDb && g_objectTypeDb->m_ok && g_objectTypeDb->LoadFamilies()) return;
    DestroyObjectTypeDb();
}

void DestroyObjectTypeDb() {
    if (g_objectTypeDb) {
        delete g_objectTypeDb;
        g_objectTypeDb = nullptr;
    }
}

IntPair* __thiscall GetRecordKindRange(DatRecord* record, IntPair* out) {
    *out = IntPair(record->m_first, record->m_second);
    return out;
}
