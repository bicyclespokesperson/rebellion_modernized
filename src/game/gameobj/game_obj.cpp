#include "gameobj/game_obj.h"

#include <new>

#include "core/vptr_store.h"
#include "data/dat_record.h"
#include "data/gnprtb.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));
void DestroyEmptyName();

extern uint32_t g_gameObjStaticInitFlags;  // 0x6b2968, bit 0: g_emptyName constructed
extern String g_emptyName;                 // 0x6b2ac8
extern const char g_emptyText[];           // 0x6b120c

namespace {

constexpr uint32_t kViewSideMask = 0x30;
constexpr uint32_t kExistingFlag = 0x40;
constexpr uint32_t kConstructedFlag = 0x10000;
constexpr uint32_t kCreatedFlag = 0x2;
constexpr uint32_t kDestroyedFlag = 0x8;
constexpr uint32_t kCompletedFlag = 0x4;
constexpr uint32_t kEnrouteFlag = 0x10;
constexpr uint32_t kRecordTypeMask = 0xfff;
constexpr int16_t kIndefiniteArticleA = 0x4da1;
constexpr int16_t kIndefiniteArticleAn = 0x4da2;
constexpr int16_t kArticleGroup = 2;
constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusDifferentControl = 0x24;
constexpr int kStatusNotSupported = 0x17;
constexpr int kStatusNotSupported25 = 0x25;
constexpr int kStatusNotSupported27 = 0x27;

void setStatus(StatusCode* out, int code) { *out = StatusCode(kStatusCategory, code); }

bool isSideRootFamily(uint32_t value) { return value >= family::kSideRoot && value < family::kSide; }

void appendArticle(String* out, const String& name) {
    const ResourceStringId article = {name.StartsWithNonVowel() == 0 ? kIndefiniteArticleAn : kIndefiniteArticleA,
                                      kArticleGroup};
    const String articleText(&article);
    *out += articleText;
}

}  // namespace

GameObj::GameObj()
    : GameObjNotifier(),
      m_name(nullptr),
      m_packed(0),
      m_eta(0),
      m_unk48(0),
      m_unk4c(0),
      m_stateFlags(0),
      m_data(nullptr) {
    SetFlag31(GetFlag31());
}

GameObj::~GameObj() {
    REB_KEEP_VPTR_STORE();
    if (m_name) {
        delete m_name;
        m_name = nullptr;
    }
    if ((m_flags & kViewSideMask) == 0) DestroyData();
}

int GameObj::InstallData() {
    m_data = CreateData();
    return m_data != nullptr;
}

int GameObj::DestroyData() {
    if (m_data) {
        delete m_data;
        m_data = nullptr;
    }
    return 1;
}

GameObjNotifier* GameObj::FindNextSibling(const FamilyRange* range, int resolve) {
    return FindNextObjectInRange(this, range, resolve);
}

void GameObj::WriteName(String* out, int kind) {
    const String name(*GetName());
    switch (kind) {
        case 1:
        case 2:
        case 4:
            *out += *GetName();
            break;
        case 3:
            if (m_record) *out += static_cast<DatRecord*>(m_record)->m_name;
            break;
        case 0x20:
        case 0x21:
            appendArticle(out, name);
            *out += *GetName();
            break;
        case 0x22:
            if (m_record) {
                const String recordName(static_cast<DatRecord*>(m_record)->m_name);
                appendArticle(out, recordName);
                *out += recordName;
            }
            break;
        default:
            break;
    }
}

// The secondary vtable slot is entered with `this` pointing at the ObjectBase subobject.
void __thiscall AppendGameObjName(ObjectBase* base, String* out, int kind) {
    static_cast<GameObj*>(base)->GameObj::WriteName(out, kind);
}

String* GameObj::GetNameCopy(String* out) {
    if (m_name) {
        new (out) String(*m_name);
        return out;
    }
    if (m_record) {
        new (out) String(static_cast<DatRecord*>(m_record)->m_name);
        return out;
    }
    new (out) String(g_emptyText);
    return out;
}

String* GameObj::GetName() {
    if ((g_gameObjStaticInitFlags & 1) == 0) {
        g_gameObjStaticInitFlags |= 1;
        new (&g_emptyName) String(g_emptyText);
        atexit(DestroyEmptyName);
    }
    if (m_name) return m_name;
    if (m_record) return &static_cast<DatRecord*>(m_record)->m_name;
    return &g_emptyName;
}

int GameObj::IsUnderSideRoot() {
    GameObjNotifier* ancestor = m_parent;
    while (ancestor) {
        if (isSideRootFamily(ancestor->GetFamily())) break;
        ancestor = ancestor->m_parent;
    }
    return ancestor != nullptr;
}

uint32_t GameObj::GetRecordTypeBits() {
    if (!m_record) return 0;
    return static_cast<DatRecord*>(m_record)->m_small & kRecordTypeMask;
}

int GameObj::GetTransitSpeed(int arg) {
    if ((m_stateFlags & kExistingFlag) && arg != 0) return g_gnprtbDefault;
    return 0;
}

int GameObj::IsConstructed() { return (m_stateFlags >> 16) & 1; }

int GameObj::GetEventKinds(int unused, EventKindSet* out) {
    EventKindSet kinds;
    *out = kinds;
    return 1;
}

int GameObj::Virtual17() { return !Virtual16(); }

int GameObj::CheckSameControl(GameObj* other, StatusCode* out) {
    if (((other->m_flags ^ m_flags) & 0xc0) == 0) {
        setStatus(out, kStatusOk);
    } else {
        setStatus(out, kStatusDifferentControl);
    }
    return 1;
}

int GameObj::CheckOrder(int a, int orderKind, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    return 1;
}

int GameObj::CheckRequestImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    return 1;
}

int GameObj::CheckRequest(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    return CheckRequestImpl(request, out);
}

int GameObj::CheckRenameImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    return 1;
}

int GameObj::CheckRename(const ActivityRequest* request, int name, StatusCode* out) {
    setStatus(out, kStatusOk);
    return CheckRenameImpl(request, out);
}

int GameObj::CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusNotSupported);
    return 1;
}

int GameObj::CheckUnknown(const ActivityRequest* request, StatusCode* out) {
    setStatus(out, kStatusOk);
    return CheckUnknownImpl(request, out);
}

int GameObj::Virtual29(int a, int b, int c, int d, StatusCode* out) {
    setStatus(out, kStatusNotSupported25);
    return 1;
}

int GameObj::Virtual30(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) {
    *o1 = 0;
    *o2 = 0;
    *o3 = 0;
    setStatus(out, kStatusNotSupported27);
    return 1;
}

int GameObj::Virtual31(int a, int b, int c, int d, StatusCode* out) {
    setStatus(out, kStatusOk);
    return 1;
}

int GameObj::Virtual32(const FamilyRange* range, int b, int* o1, int* o2, int* o3, StatusCode* out) {
    *o1 = 1;
    *o2 = 1;
    *o3 = 0;
    setStatus(out, kStatusOk);
    return 1;
}

int GameObj::GetOwnerKey(ObjectKey* out) {
    int ok = 1;
    if (m_parent) {
        ok = m_parent->GetOwnerKey(out);
    } else {
        out->Reset();
    }
    if (ok && !out->OrNull()) {
        if ((m_stateFlags & kExistingFlag) && (m_flags & kViewSideMask) == 0) {
            if (!isSideRootFamily(GetFamily()) && IsNotGameRoot()) return 0;
        }
        ok = 1;
    }
    return ok;
}

int GameObj::GetParent(GameObjNotifier** out) {
    *out = m_parent;
    return m_parent != nullptr;
}

int GameObj::GetValidParentKey(ObjectKey* out) {
    int result = GetParentKey(out);
    if (result) result = out->IsSet();
    return result;
}

uint32_t GameObj::MatchesStateFilter(uint32_t mode) {
    if (static_cast<int>(mode) < 1 || 4 < static_cast<int>(mode)) return mode;
    switch (mode) {
        case 1:
            return m_stateFlags & 1;
        case 2:
            return (m_stateFlags >> 4) & 1;
        case 3:
            return (m_stateFlags >> 6) & 1;
        case 4:
            return 1;
        default:
            return 0;
    }
}

GameObjData* GameObj::CreateData() { return new GameObjData(); }

// ---- destructors registered with atexit ----------------------------------------------------

void DestroyEmptyName() { g_emptyName.~String(); }
