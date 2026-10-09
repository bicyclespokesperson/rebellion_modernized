#include "character/character_mgr.h"
#include "gameobj/factory_registries.h"

#include "character/character38.h"
#include "character/major_character.h"
#include "character/special_forces.h"
#include "economy/defense_facility.h"
#include "economy/manu_mgr_leaves.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "economy/side.h"
#include "events/command_classes.h"
#include "events/command_classes2.h"
#include "events/incident_timers.h"
#include "events/object_timers.h"
#include "events/person_events.h"
#include "events/plain_events.h"
#include "events/request_orders.h"
#include "events/source_events.h"
#include "events/system_events.h"
#include "fleet/capship_subclasses.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/troop_reg.h"
#include "galaxy/mission_type42.h"
#include "galaxy/role_container.h"
#include "galaxy/sector.h"
#include "galaxy/system_type90.h"
#include "galaxy/system_type92.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/game_root.h"
#include "gameobj/side_root.h"
#include "mission/mission_abduction.h"
#include "mission/mission_adrift.h"
#include "mission/mission_assassination.h"
#include "mission/mission_autoroute.h"
#include "mission/mission_diplomacy.h"
#include "mission/mission_dssabotage.h"
#include "mission/mission_espionage.h"
#include "mission/mission_events.h"
#include "mission/mission_incite.h"
#include "mission/mission_jedi_training.h"
#include "mission/mission_recon.h"
#include "mission/mission_recruit.h"
#include "mission/mission_rescue.h"
#include "mission/mission_research.h"
#include "mission/mission_sabotage.h"
#include "mission/mission_subdue.h"
#include "orders/mission_orders.h"
#include "orders/order_base.h"
#include "orders/orders_210_216.h"
#include "story/mission_mgr.h"
#include "story/story_characters.h"
#include "story/story_missions.h"
#include "rules/rules_mgr.h"

// The factories themselves live in the modules of the classes they build; only their addresses are needed here.
// Taking the address of a function declared by its original name yields the original address.
GameObjNotifier* FUN_0056ebd0();


void* CreateMoveOrder();
void* CreateConfirmedMoveOrder();
void* CreateObjectMoveOrder();
void* CreateReadyForDeleteOrder();
void* CreateRenameOrder();
void* CreateRenameOrder215();
void* CreateScrapOrder();
void* FUN_00628e60();

constexpr int kMaxKeyedFactories = 0x800;

struct KeyedFactoryEntry {
    int key;
    void* (*create)();
};

extern int g_keyedFactoryCount;                 // 0x6b6fd8
extern KeyedFactoryEntry g_keyedFactories[];    // 0x6b2fd8, kMaxKeyedFactories entries

void RegisterGameObjFactories() {
    GameObjRoot::RegisterFactory(0x98, RoleContainer::Create);
    GameObjRoot::RegisterFactory(0x62, MissionAbduction::Create);
    GameObjRoot::RegisterFactory(0x44, MissionAdrift::Create);
    GameObjRoot::RegisterFactory(0x20, CreateAllFacility);
    GameObjRoot::RegisterFactory(0x63, MissionAssassination::Create);
    GameObjRoot::RegisterFactory(0x43, MissionAutoroute::Create);
    GameObjRoot::RegisterFactory(0x65, MissionBounty::Create);
    GameObjRoot::RegisterFactory(0x2a, CreateManufacturingFacility2A);
    GameObjRoot::RegisterFactory(0xa0, CreateConstructionYardManuMgr);
    GameObjRoot::RegisterFactory(0x90, SystemType90::Create);
    GameObjRoot::RegisterFactory(0x35, MajorChar35::Create);
    GameObjRoot::RegisterFactory(0x71, DagobahMission::Create);
    GameObjRoot::RegisterFactory(0x18, CreateDeathStar);
    GameObjRoot::RegisterFactory(0x6a, MissionDSSabotage::Create);
    GameObjRoot::RegisterFactory(0x25, CreateDefenseFacility25);
    GameObjRoot::RegisterFactory(0x51, MissionDiplomacy::Create);
    GameObjRoot::RegisterFactory(0x34, MajorChar34::Create);
    GameObjRoot::RegisterFactory(0x52, MissionEspionage::Create);
    GameObjRoot::RegisterFactory(0x1c, CreateFightSquad);
    GameObjRoot::RegisterFactory(0x8, CreateFleet);
    GameObjRoot::RegisterFactory(0xf1, CreateGameRoot);
    GameObjRoot::RegisterFactory(0x33, Han::Create);
    GameObjRoot::RegisterFactory(0x72, MissionVacation::Create);
    GameObjRoot::RegisterFactory(0x56, MissionIncite::Create);
    GameObjRoot::RegisterFactory(0x22, CreateDefenseFacility22);
    GameObjRoot::RegisterFactory(0x58, MissionJediTraining::Create);
    GameObjRoot::RegisterFactory(0x31, Leia::Create);
    GameObjRoot::RegisterFactory(0x32, Luke::Create);
    GameObjRoot::RegisterFactory(0x2c, CreateProductionFacilityType2C);
    GameObjRoot::RegisterFactory(0x38, Character38::Create);
    GameObjRoot::RegisterFactory(0xf8, MissionMgr::Create);
    GameObjRoot::RegisterFactory(0x30, MajorChar30::Create);
    GameObjRoot::RegisterFactory(0x41, FUN_0056ebd0);
    GameObjRoot::RegisterFactory(0x64, MissionPalace::Create);
    GameObjRoot::RegisterFactory(0x73, MissionPickup::Create);
    GameObjRoot::RegisterFactory(0x24, CreateDefenseFacility24);
    GameObjRoot::RegisterFactory(0x53, MissionResearch::Create);
    GameObjRoot::RegisterFactory(0x54, MissionRecon::Create);
    GameObjRoot::RegisterFactory(0x55, MissionRecruit::Create);
    GameObjRoot::RegisterFactory(0x2d, CreateProductionFacilityType2D);
    GameObjRoot::RegisterFactory(0x61, MissionRescue::Create);
    GameObjRoot::RegisterFactory(0x42, Mission42::Create);
    GameObjRoot::RegisterFactory(0x92, SystemType92::Create);
    GameObjRoot::RegisterFactory(0x69, MissionSabotage::Create);
    GameObjRoot::RegisterFactory(0x80, Sector::Create);
    GameObjRoot::RegisterFactory(0x28, CreateManufacturingFacility28);
    GameObjRoot::RegisterFactory(0xa2, CreateShipyardManuMgr);
    GameObjRoot::RegisterFactory(0xf3, CreateSide);
    GameObjRoot::RegisterFactory(0x3c, SpecialForces::Create);
    GameObjRoot::RegisterFactory(0x14, CreateCapitalShip);
    GameObjRoot::RegisterFactory(0x57, MissionSubdue::Create);
    GameObjRoot::RegisterFactory(0xf9, CreateRulesMgr);
    GameObjRoot::RegisterFactory(0xfa, CharacterMgr::Create);
    GameObjRoot::RegisterFactory(0x29, CreateManufacturingFacility29);
    GameObjRoot::RegisterFactory(0xa4, CreateTrainingFacilManuMgr);
    GameObjRoot::RegisterFactory(0x10, CreateTroopReg);
    GameObjRoot::RegisterFactory(0x23, CreateDefenseFacility23);
    GameObjRoot::RegisterFactory(0xf2, CreateSideRoot);
}

void RegisterEventFactories() {
    RegisterEventFactory(0x3f0, CreatePayloadEvent3f0);
    RegisterEventFactory(0x30f, CreateSystemEvent30f);
    RegisterEventFactory(0x307, CreateEvent307);
    RegisterEventFactory(0x309, CreateSystemEvent309);
    RegisterEventFactory(0x304, CreateSystemEvent304);
    RegisterEventFactory(0x305, CreateSystemEvent305);
    RegisterEventFactory(0x311, CreateSystemEvent311);
    RegisterEventFactory(0x310, CreateSystemEvent310);
    RegisterEventFactory(0x317, CreateEvent317);
    RegisterEventFactory(0x30a, CreateEvent30a);
    RegisterEventFactory(0x308, CreateEvent308);
    RegisterEventFactory(0x302, CreateEvent302);
    RegisterEventFactory(0x318, CreateEvent318);
    RegisterEventFactory(0x300, CreateEvent300);
    RegisterEventFactory(0x31d, CreateEvent31d);
    RegisterEventFactory(0x306, CreateEvent306);
    RegisterEventFactory(0x316, CreateEvent316);
    RegisterEventFactory(0x313, CreateEvent313);
    RegisterEventFactory(0x30c, CreateMissionMgrEvent30c);
    RegisterEventFactory(0x30d, CreateMissionMgrEvent30d);
    RegisterEventFactory(0x315, CreateEvent315);
    RegisterEventFactory(0x320, CreateEvent320);
    RegisterEventFactory(0x31a, CreateEvent31a);
    RegisterEventFactory(0x30e, CreateRoleStateEvent);
    RegisterEventFactory(0x312, CreatePersonRemovedEvent);
    RegisterEventFactory(0x321, CreateLockTransferEvent);
    RegisterEventFactory(0x30b, CreateRemoveRequestEvent);
    RegisterEventFactory(0x314, CreateResignRequestEvent);
    RegisterEventFactory(0x31b, CreateSystemEvent31b);
    RegisterEventFactory(0x319, CreateSystemEvent319);
    RegisterEventFactory(0x301, CreateEvent301);
    RegisterEventFactory(0x303, CreateSystemEvent303);
    RegisterEventFactory(0x221, CreateFleetCommand221);
    RegisterEventFactory(0x220, CreateFleetCommand220);
    RegisterEventFactory(0x214, CreateDestinationCommand);
    RegisterEventFactory(0x215, CreateManagerRenameCommand);
    RegisterEventFactory(0x216, CreateManagerCommand216);
    RegisterEventFactory(0x210, CreateManufactureCommand);
    RegisterEventFactory(0x213, CreateManagerCommand213);
    RegisterEventFactory(0x240, CreateRootCommand240);
    RegisterEventFactory(0x242, CreateRootCommand242);
    RegisterEventFactory(0x372, CreateReparentToRootEvent);
    RegisterEventFactory(0x250, CreateMissionOrderCommand);
    RegisterEventFactory(0x201, CreateMoveCommand);
    RegisterEventFactory(0x370, CreateRefineryQueueEvent);
    RegisterEventFactory(0x371, CreateManufacturerQueueEvent);
    RegisterEventFactory(0x205, CreateReadyForDeleteCommand);
    RegisterEventFactory(0x202, CreateRenameCommand);
    RegisterEventFactory(0x230, CreateRoleActivityCommand);
    RegisterEventFactory(0x200, CreateScrapCommand);
    RegisterEventFactory(0x225, CreateSystemCommand225);
    RegisterEventFactory(0x224, CreateSystemCommand224);
    RegisterEventFactory(0x373, CreateLoyaltyEvent373);
    RegisterEventFactory(0x388, CreateCharacterInjuryTimer);
    RegisterEventFactory(0x389, CreateCharacterTimer389);
    RegisterEventFactory(0x38a, CreateCharacterEscapeTimer);
    RegisterEventFactory(0x380, CreateFleetSlot138Timer);
    RegisterEventFactory(0x387, CreateHyperdriveTimer);
    RegisterEventFactory(0x38c, CreateMissionStatBTimer);
    RegisterEventFactory(0x392, CreateMissionMgrStatATimer);
    RegisterEventFactory(0x393, CreateMissionMgrStatBTimer);
    RegisterEventFactory(0x38b, CreateMissionStatATimer);
    RegisterEventFactory(0x394, CreateProcFacilTimer);
    RegisterEventFactory(0x382, CreateSideScrapTimer);
    RegisterEventFactory(0x381, CreateSideResearchTimer);
    RegisterEventFactory(0x38e, CreateInformantIncidentTimer);
    RegisterEventFactory(0x386, CreateBlockadeLoyaltyTimer);
    RegisterEventFactory(0x385, CreateAssaultLoyaltyTimer);
    RegisterEventFactory(0x384, CreateUprisingLoyaltyTimer);
    RegisterEventFactory(0x38f, CreateSideStatATimer);
    RegisterEventFactory(0x390, CreateSideStatBTimer);
    RegisterEventFactory(0x383, CreateUprisingEndTimer);
    RegisterEventFactory(0x38d, CreateUprisingPulseTimer);
}

void RegisterOrderFactories() {
    RegisterOrderFactory(0x234, CreateOrder234);
    RegisterOrderFactory(0x220, CreateOrder220);
    RegisterOrderFactory(0x221, CreateOrder221);
    RegisterOrderFactory(0x222, CreateOrder222);
    RegisterOrderFactory(0x223, CreateOrder223);
    RegisterOrderFactory(0x214, CreateDestinationOrder);
    RegisterOrderFactory(0x215, CreateRenameOrder215);
    RegisterOrderFactory(0x216, CreateOrder216);
    RegisterOrderFactory(0x211, CreateManufactureOrder211);
    RegisterOrderFactory(0x212, CreateManufactureOrder212);
    RegisterOrderFactory(0x210, CreateManufactureOrder210);
    RegisterOrderFactory(0x213, CreateOrder213);
    RegisterOrderFactory(0x260, CreateRootOrder260);
    RegisterOrderFactory(0x261, CreateRootOrder261);
    RegisterOrderFactory(0x262, CreateRootOrder262);
    RegisterOrderFactory(0x263, CreateRootOrder263);
    RegisterOrderFactory(0x268, CreateOrder268);
    RegisterOrderFactory(0x270, CreateOrder270);
    RegisterOrderFactory(0x3, CreatePayloadMessage);
    RegisterOrderFactory(0x250, CreateOrder250);
    RegisterOrderFactory(0x201, CreateMoveOrder);
    RegisterOrderFactory(0x202, CreateConfirmedMoveOrder);
    RegisterOrderFactory(0x204, CreateObjectMoveOrder);
    RegisterOrderFactory(0x205, CreateReadyForDeleteOrder);
    RegisterOrderFactory(0x203, CreateRenameOrder);
    RegisterOrderFactory(0x240, CreateMissionOrder240);
    RegisterOrderFactory(0x241, CreateMissionOrder241);
    RegisterOrderFactory(0x242, CreateOrder242);
    RegisterOrderFactory(0x200, CreateScrapOrder);
    RegisterOrderFactory(0x280, FUN_00628e60);
}

void* CreateKeyedObject(int key) {
    void* (*create)() = nullptr;
    for (int i = 0; i < g_keyedFactoryCount && !create; ++i) {
        if (g_keyedFactories[i].key == key) create = g_keyedFactories[i].create;
    }
    if (create) return create();
    return nullptr;
}

int RegisterKeyedFactory(int key, void* (*factory)()) {
    bool absent = true;
    for (int i = 0; i < g_keyedFactoryCount && absent; ++i) absent = g_keyedFactories[i].key != key;
    if (absent && static_cast<uint32_t>(g_keyedFactoryCount) + 1U < kMaxKeyedFactories) {
        g_keyedFactories[g_keyedFactoryCount].key = key;
        g_keyedFactories[g_keyedFactoryCount].create = factory;
        ++g_keyedFactoryCount;
    }
    return 0;
}
