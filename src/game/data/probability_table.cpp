#include "data/probability_table.h"

#include "data/random.h"

TableRegistry* __cdecl FUN_0058b360();
void __cdecl FUN_0058b370();
void __cdecl FUN_0058b400();
ParamTableRecord* __thiscall FUN_0058b7e0(TableRegistry* registry, int tableType, int id);  // find record
int __thiscall FUN_0058b7a0(TableRegistry* registry, int argument);                        // notify tables

TableRegistry* GetTableRegistry() { return FUN_0058b360(); }

void CreateTableRegistry() { FUN_0058b370(); }

void DestroyTableRegistry() { FUN_0058b400(); }

ParamTableRecord* FindTableRecord(int tableType, int id) {
    TableRegistry* registry = GetTableRegistry();
    if (!registry) return nullptr;
    return FUN_0058b7e0(registry, tableType, id);
}

void NotifyTables(int argument) {
    TableRegistry* registry = GetTableRegistry();
    if (registry) FUN_0058b7a0(registry, argument);
}

int LookupTableValue(int tableType, int id, int* value) {
    *value = 0;
    const ParamTableRecord* record = FindTableRecord(tableType, id);
    const int found = record != nullptr;
    if (found) *value = record->m_firstValue;
    return found;
}

int RollTableChanceWithValue(int tableType, int id, int* success, int* chance) {
    *success = 1;
    *chance = 0;
    const int found = LookupTableValue(tableType, id, chance);
    if (found) *success = PercentChance(*chance);
    return found;
}

int RollTableChance(int tableType, int id, int* success) {
    int chance = 0;
    return RollTableChanceWithValue(tableType, id, success, &chance);
}

int RollRlevadChance(int unused, int a, int b, int* success) {
    *success = 1;
    return RollTableChance(kTableRlevad, a - b, success);
}
