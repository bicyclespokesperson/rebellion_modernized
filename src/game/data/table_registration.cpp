#include "data/table_registration.h"
#include "data/dat_record.h"
#include "data/param_table_entries.h"
#include "data/sd_records.h"
#include "data/seed_table_entries.h"
#include "data/table_entry.h"

extern int g_tableEntryClassesRegistered;
extern int g_recordFactoriesRegistered;

namespace {

void registerRecordFactory(int low, int high, void* (*factory)()) {
    const int key[2] = {low, high};
    RegisterRecordFactory(key, factory);
}

}  // namespace

void RegisterTableEntryClasses() {
    RegisterTableEntryClass("IntTableEntry", CreateIntTableEntry);
    RegisterTableEntryClass("SideParamTableEntry", CreateSideParamTableEntry);
    RegisterTableEntryClass("SeedFamilyTableEntry", CreateSeedFamilyTableEntry);
    RegisterTableEntryClass("SeedTableEntry", CreateSeedTableEntry);
    RegisterTableEntryClass("GeneralParamTableEntry", CreateGeneralParamTableEntry);
}

int EnsureTableEntryClassesRegistered() {
    if (g_tableEntryClassesRegistered == 0) {
        RegisterTableEntryClasses();
        g_tableEntryClassesRegistered = 1;
    }
    return 1;
}

void RegisterRecordFactories() {
    registerRecordFactory(0x20, 0x22, CreateAllFacilitySD);
    registerRecordFactory(0x14, 0x1c, CreateCapitalShipSD);
    registerRecordFactory(0x22, 0x28, CreateDefenseFacilitySD);
    registerRecordFactory(0x1c, 0x20, CreateFighterSD);
    registerRecordFactory(0x30, 0x38, CreateMajorCharacterSD);
    registerRecordFactory(0x28, 0x2c, CreateManufacturingFacilitySD);
    registerRecordFactory(0x38, 0x3c, CreateMinorCharacterSD);
    registerRecordFactory(0x40, 0x80, CreateMissionSD);
    registerRecordFactory(0x2c, 0x30, CreateProductionFacilitySD);
    registerRecordFactory(0x80, 0x90, CreateSectorSD);
    registerRecordFactory(0x3c, 0x40, CreateSpecialForceSD);
    registerRecordFactory(0x90, 0x98, CreateSystemSD);
    registerRecordFactory(0x10, 0x14, CreateTroopSD);
    registerRecordFactory(8, 0x10, CreateBasicSD);
    registerRecordFactory(0xf0, 0xff, CreateBasicSD);
    registerRecordFactory(0x98, 0xa0, CreateBasicSD);
    registerRecordFactory(0xa0, 0xb0, CreateBasicSD);
}

int EnsureRecordFactoriesRegistered() {
    if (g_recordFactoriesRegistered == 0) {
        RegisterRecordFactories();
        g_recordFactoriesRegistered = 1;
    }
    return 1;
}

int g_recordFactoriesRegistered{};
int g_tableEntryClassesRegistered{};
