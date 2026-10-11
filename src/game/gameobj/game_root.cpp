#include "gameobj/game_root.h"
#include "core/icf_stubs.h"

#include "core/checked_new.h"
#include "data/probability_table.h"
#include "mission/mission_forwarders.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_type_db.h"
#include "galaxy/game_setup_seed.h"

// ---- helpers the game root shares with the rest of the setup code ----------------------------------------------


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
      m_random(nullptr),
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
      m_key150(ObjectKey::Make(0, 0)),
      m_key154(),
      m_ctx15c(),
      m_ctx178(),
      m_ctx194() {
    SetId(4);
    m_parent = nullptr;
    SetFlag31(GetFlag31() != 0);
}

GameRoot::~GameRoot() {

    if ((m_flags & kViewSideMask) == 0) {
        deleteView(this, 1);
        deleteView(this, 2);
        DestroyObjectTypeDb();
        DestroyTableRegistry();
        DestroyRandomGenerator();
        DestroyObjectDatabase();
        DestroyUnresolvedKeySet();
        SetSetupStage(1);
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

int GameRoot::Read(istream* in) { return ReadBody(in); }

int GameRoot::Write(ostream* out) { return WriteBody(out); }

int GameRoot::Checksum(Crc32* crc) { return ChecksumBody(crc); }

int GameRoot::Validate() { return ValidateBody(); }

int GameRoot::ApplyAll(NotifyContext* ctx) {
    const int isSideView = IsSideView();
    if (isSideView) return GameObj::ApplyAll(ctx);
    return isSideView;
}

int GameRoot::CreateViews(int controlKind, NotifyContext* ctx) { return ReturnOneTwoArgs(controlKind, ctx); }

GameObj* GameRoot::CreateViewObject() { return NewOrNull<GameRoot>(); }

GameObjNotifier* CreateGameRoot() { return NewOrNull<GameRoot>(); }

GameSettings& GameRoot::SetSettings(const GameSettings& settings) { return m_settings = settings; }

int GameRoot::BeginSetup() {
    const GameSettings defaults;
    if (defaults == m_settings) m_settings.m_field04 = 1;
    SetSetupStage(kStageInstallHook);
    return 1;
}

int GameRoot::IsSetupComplete() { return g_setupComplete; }

int GameRoot::StepSetup(int* status) {
    int flag = 0;
    *status = 1;
    bool advance = true;
    switch (GetSetupStage()) {
        case kStageInstallHook: {
            g_gameHook2AEC = AllocateObjectId;
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
            BindDifficultyTables();
            break;
        case kStageCreateDatabase:
            CreateObjectDatabase();
            CreateUnresolvedKeySet();
            break;
        case kStageCheckDatabaseEarly:
        case kStageCheckDatabaseLate:
            *status = CheckObjectDatabase();
            break;
        case 8:
            *status = SetupRootObject(&m_ctx15c);
            break;
        case 9:
            *status = CreateRootChildren(&m_ctx15c);
            break;
        case kStageSettings:
            *status = CreateSectors(m_settings.m_field04, &m_ctx15c);
            break;
        case 0xb:
            *status = DistributeSystems(&m_ctx15c);
            break;
        case kStageFlagA:
            *status = PlaceNextPooledSystem(&flag, &m_ctx15c);
            advance = *status != 0 && flag != 0;
            break;
        case 0xd:
            *status = BuildRulesObject(&m_ctx15c);
            break;
        case 0xe:
            *status = BeginSystemWalk(&m_ctx15c);
            break;
        case kStageFlagB:
            *status = InitializeNextSystem(&flag, &m_ctx15c);
            advance = *status != 0 && flag != 0;
            break;
        case 0x10:
            *status = PlaceHeadquarters(&m_ctx15c);
            break;
        case 0x11:
        case 0x13:
            *status = SetupSideObjects(&m_ctx15c);
            break;
        case 0x12:
            *status = SeedInitialForces(&m_ctx15c);
            break;
        case 0x14:
            *status = SetupCharacterManagerA(&m_ctx15c);
            break;
        case 0x15:
            *status = SetupCharacterManagerB(&m_ctx15c);
            break;
        case 0x17:
            *status = InitializeAllObjects(&m_ctx15c);
            break;
        case 0x18:
            *status = FinishSideViews(&m_ctx15c);
            break;
        case kStageNoHelper:
            *status = ReturnOne();
            break;
        default:
            advance = false;
            break;
    }
    if (advance) AdvanceSetupStage();
    return IsSetupComplete();
}

int GameRoot::LoadGame(istream* in, GameRoot** out) {
    *out = nullptr;
    SetSetupStage(kStageLoading);
    g_gameHook2AEC = AllocateObjectId;
    CreateObjectTypeDb();
    CreateTableRegistry();
    CreateObjectDatabase();
    int ok = LoadUnresolvedKeySet(in);
    GameRoot* root = nullptr;
    for (int view = 0; view < kSideViews; ++view) {
        if (GameRoot* created = NewOrNull<GameRoot>()) {
            if (view == 0) {
                root = created;
                *out = root;
            }
            created->Read(in);
            ok = created->ReadTree(in);
        }
    }
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
        result = allOf(root->BuildRulesObject(&root->m_ctx178), result);
        result = allOf(root->BindDifficultyTables(), result);
    }
    SetSetupStage(kStageDone);
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
