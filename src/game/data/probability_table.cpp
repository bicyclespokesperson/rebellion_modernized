#include "data/probability_table.h"

#include "data/random.h"
#include "data/table.h"

TableRegistry* GetTableRegistry() { return GetGDataTables(); }

void CreateTableRegistry() { InitGDataTables(); }

void DestroyTableRegistry() { DestroyGDataTables(); }

TableEntry* FindTableRecord(int tableType, int id) {
    TableRegistry* registry = GetTableRegistry();
    if (!registry) return nullptr;
    return registry->Lookup(tableType, id);
}

int TableRecordFirstValue(const TableEntry* record) { return *reinterpret_cast<const int*>(record + 1); }

void ChecksumTables(Crc32* crc) {
    TableRegistry* registry = GetTableRegistry();
    if (registry) registry->ChecksumAll(crc);
}

int LookupTableValue(int tableType, int id, int* value) {
    *value = 0;
    const TableEntry* record = FindTableRecord(tableType, id);
    const int found = record != nullptr;
    if (found) *value = TableRecordFirstValue(record);
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
