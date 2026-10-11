#pragma once
#include <cstdint>

#include "economy/procfacil.h"

// ProductionFacility (ctor 0x55a600 over ProcFacil, VT 0x662778, secondary ObjectBase VT 0x662774): the base of the
// production facilities (types 0x2c-0x2d, PROFAC records; mines and refineries), 0x70 bytes. Vtable: 135 slots, 0-128 are
// ProcFacil's, 129-134 are new. The class processes a batch of work: m_work is (total, done) and the processing time
// (ProcFacilData::m_duration) is derived from it, the record's rate and m_productionModifier by slot 134.
//
// Properties (SetX -> ApplyX -> OnXChanged / OnXNotified as usual):
//   Work (+0x64, IntPair {first = total, second = done})      slots 129 / 131
//   ProductionModifier (+0x6c, percent, default 100)          slots 130 / 132 ("Invalid ProductionModifier value!")
class ProductionFacility : public ProcFacil {
public:
    ProductionFacility();           // 0x55a600
    virtual ~ProductionFacility();  // 0x55a680 (scalar deleting destructor 0x55a660)

    virtual int OnControlKindChanged(uint32_t oldKind, uint32_t newKind, NotifyContext* ctx) override;  // 60, 0x55b010
    virtual int OnUsableChanged(int newValue, NotifyContext* ctx) override;                             // 68, 0x55b0e0
    virtual int OnCreatedChanged(int newValue, NotifyContext* ctx) override;                            // 69, 0x55b0c0
    virtual int OnExistingChanged(int newValue, NotifyContext* ctx) override;                           // 74, 0x55b140
    virtual int OnStateChanged(int oldState, int newState, NotifyContext* ctx) override;                // 115, 0x55b160
    virtual int OnSuspendedChanged(int value, NotifyContext* ctx) override;                             // 117, 0x55b110
    virtual int OnStartupCycleChanged(int value, NotifyContext* ctx) override;                          // 121, 0x55b190

    virtual int OnWorkChanged(const IntPair* oldWork, const IntPair* newWork, NotifyContext* ctx);                        // 129, 0x55b1c0
    virtual int OnProductionModifierChanged(int oldValue, int newValue, NotifyContext* ctx);                                // 130, 0x55b1e0
    virtual int OnWorkNotified(const IntPair* oldWork, const IntPair* newWork, NotifyContext* ctx);                        // 131, 0x55b1f0
    virtual int OnProductionModifierNotified(int oldValue, int newValue, NotifyContext* ctx);                               // 132, 0x55b200
    // Whether a facility holding `work` can process anything.
    virtual int CanProcess(const IntPair* work) = 0;                                                                        // 133
    // Processing time for `work` at `modifier` percent (see the leaf classes); false if it cannot be computed.
    virtual int ComputeDuration(const IntPair* work, int modifier, int notStartupCycle, int* duration) = 0;                // 134

    // Copies m_work into *out and returns out.
    IntPair* GetWork(IntPair* out);  // 0x52e740 (in the Side range of the code map)

    int SetWork(const IntPair* value, NotifyContext* ctx);          // 0x55a6e0
    int SetProductionModifier(int value, NotifyContext* ctx);       // 0x55a7a0
    // Adds the work implied by `ratio` {divisor, numerator}: how much of the batch should be done by now, capped by
    // `limit` and the room left; *added gets the amount. Always 1.
    int AdvanceWork(int limit, const int* ratio, int* added, NotifyContext* ctx);  // 0x55a820
    // The reverse of AdvanceWork. Always 1.
    int RewindWork(int limit, const int* ratio, int* removed, NotifyContext* ctx);  // 0x55a960
    // Idle <-> waiting according to CanProcess. Returns 1 unless it restarted or reset the state.
    int UpdateEligibility(NotifyContext* ctx);                      // 0x55aa80
    // Clears the work done when the facility is not usable, suspended or controlled by side 3. Always 1.
    int ResetWorkIfUnavailable(NotifyContext* ctx);                 // 0x55ab60
    // Stores ComputeDuration's result (0 if it fails) in the data block. Always 1.
    int UpdateDuration(NotifyContext* ctx);                         // 0x55abf0
    int ApplyWork(NotifyContext* ctx);                              // 0x55ae60
    int ApplyProductionModifier(NotifyContext* ctx);                // 0x55af90

    int ReadImpl(istream* in);             // 0x55acb0
    int WriteImpl(ostream* out);           // 0x55acf0
    int ChecksumImpl(Crc32* crc);          // 0x55ad70
    int ValidateImpl();                    // 0x55adb0
    int ApplyAllImpl(NotifyContext* ctx);  // 0x55ad30

    IntPair m_work;               // +0x64
    int m_productionModifier;     // +0x6c
};

// Iterator over families [0x2c,0x30) (VT 0x662994, destructor only).
class ProductionFacilityIterator : public ChildIterator {
public:
    ProductionFacilityIterator(GameObjNotifier* parent, uint32_t stateFilter);  // 0x55b260
    virtual ~ProductionFacilityIterator();                                      // 0x55b2e0 (scalar deleting destructor 0x55b240)
};
