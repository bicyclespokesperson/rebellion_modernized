#pragma once

// Access to the loaded rule tables (GData/*.DAT) through the table registry singleton (0x58b360), as used by
// the rules code. Table-type numbers are the ones the registry loader assigns (0x58b420): 0xd is RLEVADTB,
// 0x3c GNPRTB, 0x3d SDPRTB.

enum TableType {
    kTableRlevad = 0xd,
    kTableGnprtb = 0x3c,
    kTableSdprtb = 0x3d,
};

// Partial layout of a parameter/probability table record: the key it was loaded under and its first value.
// GNPRTB records hold 8 difficulty values from +0x24, SDPRTB records 8 x 2 (per side).
struct ParamTableRecord {
    unsigned char m_unk00[0x20];
    int m_id;          // +0x20
    int m_firstValue;  // +0x24
};

class Crc32;
class GDataTables;
using TableRegistry = GDataTables;

// The three 5-byte `jmp` thunks at 0x55bea0-0x55bec0 forward to the registry singleton's accessor, its
// creator and its destructor.
TableRegistry* GetTableRegistry();
void CreateTableRegistry();
void DestroyTableRegistry();

// Record `id` of table `tableType`, or null (also null when there is no registry).
ParamTableRecord* FindTableRecord(int tableType, int id);
// Adds every table to the checksum (the registry's per-table virtual 3), stopping at the first that fails.
void ChecksumTables(Crc32* crc);

// *value = first value of the record (0 if absent). Returns 1 if the record exists.
int LookupTableValue(int tableType, int id, int* value);
// Looks the record up and rolls PercentChance(first value): *success is the roll (1 when the record is
// missing, so a missing table never blocks an outcome). Returns 1 if the record exists.
int RollTableChanceWithValue(int tableType, int id, int* success, int* chance);
int RollTableChance(int tableType, int id, int* success);
// RLEVADTB (0xd) lookup keyed by the difference `a - b`; *success preset to 1.
int RollRlevadChance(int unused, int a, int b, int* success);
