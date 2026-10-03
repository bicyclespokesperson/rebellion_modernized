#include "gameobj/game_root.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "data/probability_table.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_type_db.h"

// ---- helpers the game root shares with the rest of the setup code ----------------------------------------------

int FUN_0051c000();      // current setup stage (global 0x6b2bb4)
void FUN_0051c010(int);  // sets the setup stage; stage 0x1a also marks setup complete (global 0x6b90e0)
void FUN_0051c030();     // advances to the next setup stage
int FUN_0051c040();      // installed as g_gameHook2AEC by the stages that start a game

int __stdcall FUN_00524fb0(int, NotifyContext*);  // shared stub: returns 1
int FUN_0040f340(GameRoot*);                      // shared stub: returns 1

// The stage bodies and the game root's persistence (all thiscall on the game root).
int __thiscall FUN_00518dc0(GameRoot*);
int __thiscall FUN_00518dd0(GameRoot*, istream*);
int __thiscall FUN_00518ef0(GameRoot*, ostream*);
int __thiscall FUN_00519060(GameRoot*, Crc32*);
int __thiscall FUN_00519130(GameRoot*);
void __thiscall FUN_005197c0(GameRoot*);
int __thiscall FUN_00519800(GameRoot*, NotifyContext*);
int __thiscall FUN_00519910(GameRoot*, NotifyContext*);
int __thiscall FUN_00519c20(GameRoot*, int, NotifyContext*);
int __thiscall FUN_00519d00(GameRoot*, NotifyContext*);
int __thiscall FUN_0051a280(GameRoot*, int*, NotifyContext*);
int __thiscall FUN_0051a530(GameRoot*, NotifyContext*);
int __thiscall FUN_0051a550(GameRoot*, NotifyContext*);
int __thiscall FUN_0051a570(GameRoot*, int*, NotifyContext*);
int __thiscall FUN_0051a640(GameRoot*, NotifyContext*);
int __thiscall FUN_0051aa50(GameRoot*, NotifyContext*);
int __thiscall FUN_0051b6d0(GameRoot*, NotifyContext*);
int __thiscall FUN_0051b710(GameRoot*, NotifyContext*);
int __thiscall FUN_0051b750(GameRoot*, NotifyContext*);
int __thiscall FUN_0051b7f0(GameRoot*, NotifyContext*);
int __thiscall FUN_0051b940(GameRoot*, NotifyContext*);

extern int g_setupComplete;  // 0x6b90e0

namespace {

constexpr uint32_t kViewSideMask = 0x30;
constexpr int kSideViews = 3;
constexpr uint32_t kMasterSideRootId = 3;

// The setup stages that do something other than "call one helper with the setup context".
enum SetupStage {
    kStageInstallHook = 2,
    kStageCreateTypeDb = 3,
    kStageCreateTables = 4,
    kStageFirstHelper = 5,
    kStageCreateDatabase = 6,
    kStageCheckDatabaseEarly = 7,
    kStageSettings = 0xa,
    kStageFlagA = 0xc,
    kStageFlagB = 0xf,
    kStageCheckDatabaseLate = 0x16,
    kStageNoHelper = 0x19,
    kStageDone = 0x1a,
    kStageLoading = 0x1b,
};

void deleteView(GameRoot* root, int view) {
    if (GameObjNotifier* object = root->GetViewObject(view)) delete object;
}

GameObjNotifier* findMaster(uint32_t family, uint32_t id) {
    const ObjectKey key = ObjectKey::Make(family, id);
    return GetMasterObject(&key);
}

int allOf(int condition, int result) { return condition && result ? 1 : 0; }

}  // namespace

// ---- GameRoot ----------------------------------------------------------------------------------------------

GameRoot::GameRoot()
    : GameObj(),
      m_settings(),
      m_unk6c(0x500),
      m_unk70(0),
      m_unk74(0),
      m_unk78{0, 0, 0, 0, 0},
      m_key8c(),
      m_ctx90(),
      m_unkac(0),
      m_master_f2000003(nullptr),
      m_master_20000005(nullptr),
      m_master_f8000006(nullptr),
      m_master_f9000007(nullptr),
      m_master_fa000008(nullptr),
      m_master_f300000f(nullptr),
      m_master_f3000010(nullptr),
      m_master_98000440(nullptr),
      m_master_98000441(nullptr),
      m_master_98000442(nullptr),
      m_master_98000480(nullptr),
      m_master_98000481(nullptr),
      m_refLists(),
      m_unk150(0),
      m_key154(),
      m_ctx15c(),
      m_ctx178(),
      m_ctx194() {
    SetId(4);
    m_parent = nullptr;
    SetFlag31(GetFlag31() != 0);
}

GameRoot::~GameRoot() {
    REB_KEEP_VPTR_STORE();
    if ((m_flags & kViewSideMask) == 0) {
        deleteView(this, 1);
        deleteView(this, 2);
        DestroyObjectTypeDb();
        DestroyTableRegistry();
        FUN_005197c0(this);
        DestroyObjectDatabase();
        DestroyUnresolvedKeySet();
        g_gameRoot = nullptr;
        FUN_0051c010(1);
        m_master_f2000003 = nullptr;
        m_master_20000005 = nullptr;
        m_master_f8000006 = nullptr;
        m_master_f9000007 = nullptr;
        m_master_fa000008 = nullptr;
        m_master_f300000f = nullptr;
        m_master_f3000010 = nullptr;
        m_master_98000440 = nullptr;
        m_master_98000441 = nullptr;
        m_master_98000442 = nullptr;
        m_master_98000480 = nullptr;
        m_master_98000481 = nullptr;
    }
}

uint32_t GameRoot::GetFamily() { return family::kGameRoot; }

int GameRoot::Read(istream* in) { return FUN_00518dd0(this, in); }

int GameRoot::Write(ostream* out) { return FUN_00518ef0(this, out); }

int GameRoot::Checksum(Crc32* crc) { return FUN_00519060(this, crc); }

int GameRoot::Validate() { return FUN_00519130(this); }

int GameRoot::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return GameObj::ApplyAll(ctx);
    return isSideView;
}

int GameRoot::CreateViews(int controlKind, NotifyContext* ctx) { return FUN_00524fb0(controlKind, ctx); }

GameObj* GameRoot::CreateViewObject() { return NewOrNull<GameRoot>(); }

GameObjNotifier* CreateGameRoot() { return NewOrNull<GameRoot>(); }

GameSettings& GameRoot::SetSettings(const GameSettings& settings) { return m_settings = settings; }

int GameRoot::BeginSetup() {
    const GameSettings defaults;
    if (defaults == m_settings) m_settings.m_field04 = 1;
    FUN_0051c010(kStageInstallHook);
    return 1;
}

int GameRoot::IsSetupComplete() { return g_setupComplete; }

int GameRoot::StepSetup(int* status) {
    int flag = 0;
    *status = 1;
    bool advance = true;
    switch (FUN_0051c000()) {
        case kStageInstallHook: {
            g_gameHook2AEC = FUN_0051c040;
            const NotifyContext fresh;
            m_ctx15c = fresh;
            break;
        }
        case kStageCreateTypeDb:
            CreateObjectTypeDb();
            break;
        case kStageCreateTables:
            CreateTableRegistry();
            break;
        case kStageFirstHelper:
            FUN_00518dc0(this);
            break;
        case kStageCreateDatabase:
            CreateObjectDatabase();
            CreateUnresolvedKeySet();
            g_gameRoot = this;
            break;
        case kStageCheckDatabaseEarly:
        case kStageCheckDatabaseLate:
            *status = CheckObjectDatabase();
            break;
        case 8:
            *status = FUN_00519800(this, &m_ctx15c);
            break;
        case 9:
            *status = FUN_00519910(this, &m_ctx15c);
            break;
        case kStageSettings:
            *status = FUN_00519c20(this, m_settings.m_field04, &m_ctx15c);
            break;
        case 0xb:
            *status = FUN_00519d00(this, &m_ctx15c);
            break;
        case kStageFlagA:
            *status = FUN_0051a280(this, &flag, &m_ctx15c);
            advance = *status != 0 && flag != 0;
            break;
        case 0xd:
            *status = FUN_0051a530(this, &m_ctx15c);
            break;
        case 0xe:
            *status = FUN_0051a550(this, &m_ctx15c);
            break;
        case kStageFlagB:
            *status = FUN_0051a570(this, &flag, &m_ctx15c);
            advance = *status != 0 && flag != 0;
            break;
        case 0x10:
            *status = FUN_0051a640(this, &m_ctx15c);
            break;
        case 0x11:
        case 0x13:
            *status = FUN_0051b750(this, &m_ctx15c);
            break;
        case 0x12:
            *status = FUN_0051aa50(this, &m_ctx15c);
            break;
        case 0x14:
            *status = FUN_0051b6d0(this, &m_ctx15c);
            break;
        case 0x15:
            *status = FUN_0051b710(this, &m_ctx15c);
            break;
        case 0x17:
            *status = FUN_0051b7f0(this, &m_ctx15c);
            break;
        case 0x18:
            *status = FUN_0051b940(this, &m_ctx15c);
            break;
        case kStageNoHelper:
            *status = FUN_0040f340(this);
            break;
        default:
            advance = false;
            break;
    }
    if (advance) FUN_0051c030();
    return IsSetupComplete();
}

int GameRoot::LoadGame(istream* in, GameRoot** out) {
    *out = nullptr;
    FUN_0051c010(kStageLoading);
    g_gameHook2AEC = FUN_0051c040;
    CreateObjectTypeDb();
    CreateTableRegistry();
    CreateObjectDatabase();
    int ok = LoadUnresolvedKeySet(in);
    g_gameRoot = nullptr;
    for (int view = 0; view < kSideViews; ++view) {
        if (GameRoot* created = NewOrNull<GameRoot>()) {
            if (view == 0) g_gameRoot = created;
            created->Read(in);
            ok = created->ReadTree(in);
        }
    }
    GameRoot* root = g_gameRoot;
    *out = root;
    int result = allOf(root != nullptr, ok);
    if (root) {
        root->m_master_f2000003 = findMaster(family::kSideRoot, kMasterSideRootId);
        root->m_master_20000005 = findMaster(0x20, 5);
        root->m_master_f8000006 = findMaster(family::kMissionManager, 6);
        root->m_master_f9000007 = findMaster(0xf9, 7);
        root->m_master_fa000008 = findMaster(family::kCharacterManager, 8);
        root->m_master_f300000f = findMaster(family::kSide, 0xf);
        root->m_master_f3000010 = findMaster(family::kSide, 0x10);
        root->m_master_98000440 = findMaster(0x98, 0x440);
        root->m_master_98000441 = findMaster(0x98, 0x441);
        root->m_master_98000442 = findMaster(0x98, 0x442);
        root->m_master_98000480 = findMaster(0x98, 0x480);
        root->m_master_98000481 = findMaster(0x98, 0x481);
        result = allOf(FUN_0051a530(root, &root->m_ctx178), result);
        result = allOf(FUN_00518dc0(root), result);
    }
    FUN_0051c010(kStageDone);
    return result;
}

int GameRoot::SaveGame(ostream* out, GameRoot* root) {
    int ok = SaveUnresolvedKeySet(out) != 0;
    for (int view = 0; view < kSideViews; ++view) {
        GameObjNotifier* object = view == 0 ? root : root->GetViewObject(view);
        ok = allOf(object != nullptr, ok);
        if (object) {
            ok = allOf(object->Write(out), ok);
            ok = allOf(object->WriteTree(out), ok);
        }
    }
    return ok;
}

int GameRoot::ChecksumGame(Crc32* crc, GameRoot* root) {
    int result = 1;
    for (int view = 0; view < kSideViews && result; ++view) {
        GameObjNotifier* object = view == 0 ? root : root->GetViewObject(view);
        result = object != nullptr;
        if (result) {
            result = object->Checksum(crc);
            if (result) result = object->ChecksumTree(crc);
        }
    }
    return result;
}
