#pragma once
#include <cstdint>

#include "core/layout.h"
#include "core/stream_io.h"
#include "gameobj/game_hooks.h"
#include "gameobj/game_obj.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_key.h"

// The game root (family 0xf1, VT 0x65e938, secondary 0x65e930): the single top object of the game-object tree. It
// owns the one-time setup of a new game (an object-type database, table registry, object database and unresolved-key
// set, then a long sequence of "stages" that each fill in one part of the universe), loads and saves the three views
// of the whole tree, and caches the master objects of the singleton managers that the rest of the game asks for by key.
//
// The setup stage number lives in a global (0x6b2bb4); the stages and what each one calls are listed in StepSetup.

class Crc32;

class GameRoot : public GameObj {
public:
    GameRoot();
    virtual ~GameRoot();

    virtual uint32_t GetFamily() override;
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;
    virtual int Checksum(Crc32* crc) override;
    virtual int Validate() override;
    // Only side views take part in ApplyAll; the master answers 0.
    virtual int ApplyAll(NotifyContext* ctx) override;
    virtual int Virtual16() override { return 1; }
    virtual int CreateViews(int controlKind, NotifyContext* ctx) override;
    virtual GameObj* CreateViewObject() override;

    // Copies the settings in; returns the stored copy.
    GameSettings& SetSettings(const GameSettings& settings);  // 0x513640
    // Flags m_settings.m_field04 if the settings are still the defaults, then enters setup stage 2. Returns 1.
    int BeginSetup();  // 0x513650
    // Runs one setup stage (the global stage number selects it) and advances to the next one when it is done.
    // *status receives the stage's result (1 if the stage does not produce one). Returns whether setup is complete.
    int StepSetup(int* status);  // 0x5136d0
    int IsSetupComplete();       // 0x513960

    // Loads a whole game: the unresolved-key set, then the root and its tree once per view. *out receives the master
    // root (null if none could be built). Returns 1 on success.
    static int LoadGame(istream* in, GameRoot** out);  // 0x513a50
    // Saves / checksums the three views of `root`, one after the other.
    static int SaveGame(ostream* out, GameRoot* root);       // 0x513df0
    static int ChecksumGame(Crc32* crc, GameRoot* root);     // 0x513e80

    // ---- fields ----
    GameSettings m_settings;      // +0x58, m_settings.m_field04 is also read as the "defaults" flag
    int m_unk6c;                  // +0x6c, 0x500
    int m_unk70;                  // +0x70
    int m_unk74;                  // +0x74
    int m_unk78[5];               // +0x78
    ObjectKey m_key8c;            // +0x8c
    NotifyContext m_ctx90;        // +0x90
    int m_unkac;                  // +0xac
    // Master objects of singletons, looked up by key once the tree is loaded (0x513a50), cleared by the destructor.
    GameObjNotifier* m_master_f2000003;  // +0xb0
    GameObjNotifier* m_master_20000005;  // +0xb4
    GameObjNotifier* m_master_f8000006;  // +0xb8
    GameObjNotifier* m_master_f9000007;  // +0xbc, the rules object
    GameObjNotifier* m_master_fa000008;  // +0xc0
    GameObjNotifier* m_master_f300000f;  // +0xc4
    GameObjNotifier* m_master_f3000010;  // +0xc8
    GameObjNotifier* m_master_98000440;  // +0xcc
    GameObjNotifier* m_master_98000441;  // +0xd0
    GameObjNotifier* m_master_98000442;  // +0xd4
    GameObjNotifier* m_master_98000480;  // +0xd8
    GameObjNotifier* m_master_98000481;  // +0xdc
    ObjectRefList m_refLists[7];  // +0xe0
    int m_unk150;                 // +0x150
    ObjectKey m_key154;           // +0x154
    uint8_t m_unk158[4];          // +0x158, written by setup stage 0xe
    NotifyContext m_ctx15c;       // +0x15c, the context the setup stages run with
    NotifyContext m_ctx178;       // +0x178
    NotifyContext m_ctx194;       // +0x194
};

// The single game root (0x6b2bb0), null before setup stage 6 and after the root is destroyed.
extern GameRoot* g_gameRoot;

// The factory registered for family 0xf1 (0x513970).
GameObjNotifier* CreateGameRoot();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameRoot, 0x1b0);
REB_ASSERT_OFFSET(GameRoot, m_settings, 0x58);
REB_ASSERT_OFFSET(GameRoot, m_unk6c, 0x6c);
REB_ASSERT_OFFSET(GameRoot, m_unk78, 0x78);
REB_ASSERT_OFFSET(GameRoot, m_key8c, 0x8c);
REB_ASSERT_OFFSET(GameRoot, m_ctx90, 0x90);
REB_ASSERT_OFFSET(GameRoot, m_unkac, 0xac);
REB_ASSERT_OFFSET(GameRoot, m_master_f2000003, 0xb0);
REB_ASSERT_OFFSET(GameRoot, m_master_20000005, 0xb4);
REB_ASSERT_OFFSET(GameRoot, m_master_f8000006, 0xb8);
REB_ASSERT_OFFSET(GameRoot, m_master_f9000007, 0xbc);
REB_ASSERT_OFFSET(GameRoot, m_master_fa000008, 0xc0);
REB_ASSERT_OFFSET(GameRoot, m_master_f300000f, 0xc4);
REB_ASSERT_OFFSET(GameRoot, m_master_f3000010, 0xc8);
REB_ASSERT_OFFSET(GameRoot, m_master_98000440, 0xcc);
REB_ASSERT_OFFSET(GameRoot, m_master_98000441, 0xd0);
REB_ASSERT_OFFSET(GameRoot, m_master_98000442, 0xd4);
REB_ASSERT_OFFSET(GameRoot, m_master_98000480, 0xd8);
REB_ASSERT_OFFSET(GameRoot, m_master_98000481, 0xdc);
REB_ASSERT_OFFSET(GameRoot, m_refLists, 0xe0);
REB_ASSERT_OFFSET(GameRoot, m_unk150, 0x150);
REB_ASSERT_OFFSET(GameRoot, m_key154, 0x154);
REB_ASSERT_OFFSET(GameRoot, m_unk158, 0x158);
REB_ASSERT_OFFSET(GameRoot, m_ctx15c, 0x15c);
REB_ASSERT_OFFSET(GameRoot, m_ctx178, 0x178);
REB_ASSERT_OFFSET(GameRoot, m_ctx194, 0x194);
#pragma clang diagnostic pop
