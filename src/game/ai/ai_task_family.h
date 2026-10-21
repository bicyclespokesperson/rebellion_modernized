#pragma once
#include <cstdint>

#include "ai/ai_selector.h"
#include "gameobj/object_key.h"

// The task types 5, 6, 7, 10 and 11 (and, with other constants, 1, 2 and 3) manage one kind of facility of the side:
// they keep a short list of systems (the targets), claim systems for it, size what the side lacks, and order the
// facility managers to produce it or the old facilities to be scrapped. The original compiled the same source once per
// task type; the code here is that source once, with the per-type numbers in FamilyParams and the per-type fields
// reached through FamilyFields.

class AiSideController;
class AiSystemEval;
class GameObjNotifier;
class OrderBase;

namespace aitask
{

// The facility families the scrap order walks.
enum class FacilityKind : int { kManufacturing28, kManufacturing29, kManufacturing2A };

struct FamilyFields {
  AiSideController* owner;
  int side;  // the task's view
  int ratio; // AiTaskBase::m_x24, the share of the side's budget (percent)
  AiRankedSelector* selector;
  ObjectKeySet* targets; // the systems the task works on
  ObjectKeySet* dropped; // targets that were given up (RefreshTargets)
  ObjectKey* system;     // the system picked
  ObjectKey* manager;    // the manufacturing manager picked
  ObjectKey* classKey;   // the class of facility chosen (0 for none)
  int* count;            // how many to produce
  int* cap;              // upper bound for count
  int* limit;            // the picked system's own limit
};

struct FamilyParams {
  int needPercent;         // Need: percent of the budget the task may use
  int usedStat;            // Need: the side figure that is subtracted
  int targetPercent;       // NeedForTargets
  int supplyStat;          // AiSystemEval stat summed over the targets
  int fitStat;             // AiSystemEval stat AddTargetSystem compares with the need
  int shortStat;           // AiSystemEval stat: how many facilities the system lacks
  uint32_t sectorRequire;  // SelectTarget / AddTargetSystem: required sector flags
  int sectorStat;          // ... and the figure sectors are ranked by
  uint32_t systemForbid;   // forbidden system flags
  int systemStat;          // figure systems are ranked by
  uint32_t claimBit;       // AiSystemEval::m_flags28 bit that marks a system as claimed by the task
  int sectorShortStat;     // AiSectorEval stat that is positive when the sector has systems to work on
  uint32_t classFlags;     // ChooseClass: required class flags
  uint32_t ownerBit;       // AiSideController::m_bitsA bit AddTargetSystem sets when nothing is left to do
  int countStepCode;       // ComputeCount: the step to go on at
  int lackStepCode;        // ComputeCount: the step to go on at when the side is over its budget
  FacilityKind scrapFirst; // BuildScrapOrder: the two kinds of facility that may be scrapped, with their claim masks
  uint32_t scrapFirstMask;
  FacilityKind scrapSecond;
  uint32_t scrapSecondMask;
  bool targetCountGuard; // AddTargetSystem does nothing when the list already has two systems
  bool supplyResetsCap;  // PickSupplySystemNear clears *cap first
  bool supplyClaims;     // PickSupplySystemNear adds the target system to the list and claims it
  bool supplyByDistance; // PickSupplySystemNear ranks the supply systems by their distance to the target
};

int Need(const FamilyFields& f, const FamilyParams& p);
int NeedForTargets(const FamilyFields& f, const FamilyParams& p);
int ComputeCount(const FamilyFields& f, const FamilyParams& p);
OrderBase* BuildScrapOrder(const FamilyFields& f, const FamilyParams& p);
OrderBase* BuildCapitalOrder(const FamilyFields& f, const FamilyParams& p, FacilityKind kind);
int SelectTarget(const FamilyFields& f, const FamilyParams& p);
void AddTargetSystem(const FamilyFields& f, const FamilyParams& p);
void RefreshTargets(const FamilyFields& f, const FamilyParams& p);
// The order that makes the manager produce `count` of the class (kinds 0x210-0x212): null unless the manager's family
// is [familyLow, familyLow + 2).
OrderBase* BuildManufactureOrder(const FamilyFields& f, int kind, uint32_t familyLow);
// Order 0x214: the units the manager builds go to *target, which must be in the families [targetLow, targetHigh).
OrderBase* BuildDestinationOrder(const FamilyFields& f, ObjectKey* target, uint32_t familyLow,
                                 uint32_t targetLow = 0x90, uint32_t targetHigh = 0x98);
// The sector nearest to the target system (f.system) that has a supply system, the manager of that system and how much
// it can produce (*f.cap); the target is added to the list of targets and claimed. 1 if a manager was picked.
int PickSupplySystemNear(const FamilyFields& f, const FamilyParams& p);

} // namespace aitask
