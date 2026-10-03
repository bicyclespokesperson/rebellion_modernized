#include "data/table_registration.h"
#include "data/dat_record.h"
#include "data/param_table_entries.h"

// Entry class factories and the registry live with the seedlist / SD record classes (0x58e570-0x5923a0).
void* FUN_0058fb90();  // IntTableEntry
void* FUN_0058f460();  // SeedFamilyTableEntry
void* FUN_0058f1a0();  // SeedTableEntry
// The registry stores these two factories by address; referring to them through the original addresses keeps the
// stored function pointers identical to the original's while the reconstruction is hooked into the game.
void* FUN_00585640();  // CreateGeneralParamTableEntry
void* FUN_005858c0();  // CreateSideParamTableEntry
int FUN_0058f870(const char* className, void* (*factory)());

// SD record factories, one per range of record families.
void* FUN_00592260();
void* FUN_00591d30();
void* FUN_00591aa0();
void* FUN_005917b0();
void* FUN_005915f0();
void* FUN_005913f0();
void* FUN_005912b0();
void* FUN_00590b40();
void* FUN_00590980();
void* FUN_005906d0();
void* FUN_005904c0();
void* FUN_00590180();
void* FUN_0058ff10();
void* FUN_0058fdb0();

extern int g_tableEntryClassesRegistered;
extern int g_recordFactoriesRegistered;

namespace {

void registerRecordFactory(int low, int high, void* (*factory)()) {
    const int key[2] = {low, high};
    RegisterRecordFactory(key, factory);
}

}  // namespace

void RegisterTableEntryClasses() {
    FUN_0058f870("IntTableEntry", FUN_0058fb90);
    FUN_0058f870("SideParamTableEntry", FUN_005858c0);
    FUN_0058f870("SeedFamilyTableEntry", FUN_0058f460);
    FUN_0058f870("SeedTableEntry", FUN_0058f1a0);
    FUN_0058f870("GeneralParamTableEntry", FUN_00585640);
}

int EnsureTableEntryClassesRegistered() {
    if (g_tableEntryClassesRegistered == 0) {
        RegisterTableEntryClasses();
        g_tableEntryClassesRegistered = 1;
    }
    return 1;
}

void RegisterRecordFactories() {
    registerRecordFactory(0x20, 0x22, FUN_00592260);
    registerRecordFactory(0x14, 0x1c, FUN_00591d30);
    registerRecordFactory(0x22, 0x28, FUN_00591aa0);
    registerRecordFactory(0x1c, 0x20, FUN_005917b0);
    registerRecordFactory(0x30, 0x38, FUN_005915f0);
    registerRecordFactory(0x28, 0x2c, FUN_005913f0);
    registerRecordFactory(0x38, 0x3c, FUN_005912b0);
    registerRecordFactory(0x40, 0x80, FUN_00590b40);
    registerRecordFactory(0x2c, 0x30, FUN_00590980);
    registerRecordFactory(0x80, 0x90, FUN_005906d0);
    registerRecordFactory(0x3c, 0x40, FUN_005904c0);
    registerRecordFactory(0x90, 0x98, FUN_00590180);
    registerRecordFactory(0x10, 0x14, FUN_0058ff10);
    registerRecordFactory(8, 0x10, FUN_0058fdb0);
    registerRecordFactory(0xf0, 0xff, FUN_0058fdb0);
    registerRecordFactory(0x98, 0xa0, FUN_0058fdb0);
    registerRecordFactory(0xa0, 0xb0, FUN_0058fdb0);
}

int EnsureRecordFactoriesRegistered() {
    if (g_recordFactoriesRegistered == 0) {
        RegisterRecordFactories();
        g_recordFactoriesRegistered = 1;
    }
    return 1;
}
