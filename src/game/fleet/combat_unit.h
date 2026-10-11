#pragma once
#include "fleet/detector.h"

// CombatUnit (0x557410, VT 0x662318, secondary ObjectBase VT 0x662314, 0x60 bytes): the common base of capital ships and
// fighter squadrons. Over Detector it adds the repair flags (UnderRepair, FastRepair), a tracked repair level in its data
// block (CombatUnitData), the shipboard record fields of an SdRecordGrid (+0x60..+0xbc: the grid of counts and its
// totals) and 31 more vtable slots (112 and up are shared with Detector: 112-117).
//
// UnderRepair follows (Damaged and Usable); FastRepair is set by the leaf classes. The repair level (slot 118, pure here)
// is a GNPRTB value that depends on FastRepair.
//
// Slots 5-9 and 37 stay pure and have bodies (Read, Write, Checksum, Validate, ApplyAll, CreateViews) that the leaf
// classes call first.
class CombatUnit : public Detector {
public:
    static constexpr uint32_t kUnderRepairFlag = 0x1;
    static constexpr uint32_t kFastRepairFlag = 0x2;

    CombatUnit();           // 0x557410
    ~CombatUnit() override;  // 0x557450 (scalar deleting destructor 0x557430)

    int GetEventKinds(int unused, EventKindSet* out) override;                          // 15, 0x557510
    int CheckOrder(int a, int orderKind, StatusCode* out) override;                     // 22, 0x557530
    int CheckRequestImpl(const ActivityRequest* request, StatusCode* out) override;     // 23, 0x5575f0
    int CheckUnknownImpl(const ActivityRequest* request, StatusCode* out) override;     // 27, 0x5576a0
    GameObjData* CreateData() override;                                                 // 58, 0x557da0
    int OnParentChanged(GameObjNotifier* oldParent, GameObjNotifier* newParent, NotifyContext* ctx) override;  // 61, 0x557b40
    int OnUsableChanged(int newValue, NotifyContext* ctx) override;                     // 68, 0x557b60
    int OnDestroyedChanged(int newValue, NotifyContext* ctx) override;                  // 71, 0x557b90
    int OnExistingChanged(int newValue, NotifyContext* ctx) override;                   // 74, 0x557bb0
    int OnDamagedChanged(int newValue, NotifyContext* ctx) override;                    // 77, 0x557bd0

    int CreateViews(int controlKind, NotifyContext* ctx) override = 0;  // 37, body 0x557900
    int Read(istream* in) override = 0;               // body 0x557940
    int Write(ostream* out) override = 0;             // body 0x557970
    int Checksum(Crc32* crc) override = 0;            // body 0x5579e0
    int Validate() override = 0;                      // body 0x557a10
    int ApplyAll(NotifyContext* ctx) override = 0;    // body 0x5579a0

    // The value the repair level is set to (GNPRTB, depending on FastRepair).
    virtual int GetRepairLevel() = 0;                            // 118
    virtual int GetDefenseLeft() { return 0; }                    // 119, the defense that is left (maximum minus lost); 0 here
    // Record fields +0x60..+0x70 (0 without a record).
    virtual uint32_t GetField60();                         // 120, 0x557750 (jumps to the shared body 0x520b80)
    virtual uint32_t GetField64();                         // 121, 0x557760 (jumps to 0x520b90)
    virtual uint32_t GetField68();                         // 122, 0x557770 (jumps to 0x520ba0)
    virtual uint32_t GetField6c();                         // 123, 0x557780 (jumps to 0x557470)
    virtual uint32_t GetField70();                         // 124, 0x557790 (jumps to 0x557480)
    virtual uint32_t GetGridTotal();                             // 125, 0x5577e0 (jumps to 0x5574f0)
    virtual uint32_t GetGridColumnTotal(int column);             // 126, 0x5577d0
    virtual uint32_t GetGridValue(int row, int column);          // 127, 0x5577a0
    virtual uint32_t GetGridFieldA4(int column);                 // 128, 0x5577c0
    virtual uint32_t GetDamageField60() { return 0; }              // 129..137: the "lost" counterparts of 120-128 (0 here; the leaf classes scale)
    virtual uint32_t GetDamageField64() { return 0; }                      // 130
    virtual uint32_t GetDamageField68() { return 0; }                      // 131
    virtual uint32_t GetDamageField6c() { return 0; }                      // 132
    virtual uint32_t GetDamageField70() { return 0; }                      // 133
    virtual uint32_t GetDamageTotal() { return 0; }                        // 134
    virtual uint32_t GetDamageColumnTotal(int column) { return 0; }                // 135, shared "return 0" stub (1 argument)
    virtual uint32_t GetDamageGridValue(int row, int column) { return 0; }          // 136, shared "return 0" stub (2 arguments)
    virtual uint32_t GetDamageFieldA4(int column) { return 0; }                // 137, shared "return 0" stub (1 argument)
    virtual int RepairStep(NotifyContext* ctx) = 0;              // 138: repairs one unit (a squad member, hull points)
    virtual int OnUnderRepairChanged(int newValue, NotifyContext* ctx);   // 139, 0x557c00: sets the repair stat's flag
    virtual int OnFastRepairChanged(int newValue, NotifyContext* ctx);    // 140, 0x557c30: sets the repair stat's level
    virtual int OnUnderRepairNotified(int value, NotifyContext* ctx);     // 141, 0x557c60
    virtual int OnFastRepairNotified(int value, NotifyContext* ctx);      // 142, 0x557ca0

    // Non-virtual record readers the grid slots use (0 without a record).
    uint32_t GetRecordGridValue(int row, int column);  // 0x557490
    uint32_t GetRecordFieldA4(int column);             // 0x5574b0
    uint32_t GetRecordColumnTotal(int column);         // 0x5574d0
    uint32_t GetRecordTotal();                         // 0x5574f0

    // Master view only. Return IsMasterView().
    int SetUnderRepair(int on, NotifyContext* ctx);  // 0x5577f0
    int SetFastRepair(int on, NotifyContext* ctx);   // 0x557860
    // UnderRepair = Damaged and Usable.
    int UpdateUnderRepair(NotifyContext* ctx);       // 0x5578d0
    int ApplyUnderRepair(NotifyContext* ctx);        // 0x557a20
    int ApplyFastRepair(NotifyContext* ctx);         // 0x557ab0

    // Builds the static kind set on first use (flag at 0x6bb358): Detector's plus 0x200, 0x201, 0x202, 0x204.
    static int EnsureEventKinds();                              // 0x557ce0
    static int InitEventKinds(int unused, EventKindSet* out);   // 0x557d80

    uint32_t m_combatFlags;  // +0x5c, bit 0 UnderRepair, bit 1 FastRepair
};
