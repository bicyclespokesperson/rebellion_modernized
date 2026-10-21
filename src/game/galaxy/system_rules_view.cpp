#include "galaxy/system_rules.h"
#include "galaxy/system_rules_1e.h"
#include "galaxy/system_rules_decl.h"

using namespace system_rules;

namespace
{

constexpr int kRangeSlots = 4;

struct ViewSyncPlan {
  FamilyRange systemChildren[kRangeSlots];
  FamilyRange fleetChildren[1];
  FamilyRange shipChildren[1];
  bool applyAll = false;
  bool applyVisibility = false;
  bool applyControl = false;
  bool applyUprising = false;
  bool applyResources = false;
  bool applyExtra = false;
  bool valid = true;
};

bool isEmpty(const FamilyRange& range)
{
  const FamilyRange none;
  return none.m_first == range.m_first && none.m_second == range.m_second;
}

ViewSyncPlan planFor(int mode)
{
  ViewSyncPlan plan;
  switch (mode) {
    case 0:
      break;
    case 1:
      plan.applyVisibility = true;
      plan.applyControl = true;
      break;
    case 2:
      plan.applyVisibility = true;
      plan.applyResources = true;
      break;
    case 3:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(0x10, 0x20);
      plan.systemChildren[1] = FamilyRange(0x22, 0x28);
      break;
    case 4:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(0xa0, 0xb0);
      plan.systemChildren[1] = FamilyRange(0x28, 0x30);
      break;
    case 5:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(8, 0x10);
      plan.fleetChildren[0] = FamilyRange(1, 0xff);
      break;
    case 6:
      plan.applyVisibility = true;
      plan.applyControl = true;
      plan.applyUprising = true;
      plan.systemChildren[0] = FamilyRange(8, 0x10);
      plan.fleetChildren[0] = FamilyRange(1, 0xff);
      plan.applyExtra = true;
      break;
    case 7:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(8, 0x10);
      plan.fleetChildren[0] = FamilyRange(1, 0xff);
      plan.shipChildren[0] = FamilyRange(1, 0xff);
      break;
    case 8:
      plan.systemChildren[0] = FamilyRange(0x20, 0x30);
      plan.systemChildren[1] = FamilyRange(0x10, 0x20);
      plan.systemChildren[2] = FamilyRange(0xa0, 0xb0);
      plan.systemChildren[3] = FamilyRange(8, 0x10);
      plan.fleetChildren[0] = FamilyRange(1, 0xff);
      plan.shipChildren[0] = FamilyRange(0x10, 0x20);
      plan.applyAll = true;
      break;
    case 9:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(0x30, 0x40);
      break;
    case 10:
      plan.applyAll = true;
      plan.systemChildren[0] = FamilyRange(1, 0xff);
      plan.fleetChildren[0] = FamilyRange(1, 0xff);
      plan.shipChildren[0] = FamilyRange(1, 0xff);
      break;
    case 11:
      plan.applyAll = true;
      plan.systemChildren[0] = FamilyRange(0x10, 0x40);
      plan.systemChildren[1] = FamilyRange(0xa0, 0xb0);
      break;
    case 12:
      plan.applyVisibility = true;
      plan.systemChildren[0] = FamilyRange(0x20, 0x30);
      break;
    default:
      plan.valid = false;
  }
  return plan;
}

} // namespace

// Pushes the master's state onto the side view `view` (1 or 2) for an uprising event of kind `mode`: the properties and
// the child ranges that kind touches, with the context's phase set to 1.
int System::SyncViewForEvent(int mode, int view, NotifyContext* ctx)
{
  int ok = 1;
  System* target = nullptr;
  if (view == 1 || view == 2) {
    target = static_cast<System*>(GetViewObject(view));
  }
  if (!target) {
    return ok;
  }

  ViewSyncPlan plan = planFor(mode);
  ok = plan.valid;
  NotifyContext phase(*ctx);
  phase.m_flag04 = 1;
  phase.m_flag08 = 3;

  if (ok) {
    if (plan.applyAll) {
      ok = target->ApplyAll(&phase) != 0;
    } else {
      if (plan.applyVisibility) {
        const int explored = target->ApplyExplored(&phase);
        const int populated = target->ApplyPopulated(&phase);
        ok = populated && explored;
        const int controlKind = target->ApplyControlKind(&phase);
        ok = controlKind && ok;
      }
      if (ok) {
        if (plan.applyControl) {
          const int causedKind = target->ApplyLoyaltyCausedCurrentControlKind(&phase);
          const int controlData = target->ApplyControlData(&phase);
          ok = controlData && causedKind;
          const int loyalty = target->ApplyLoyalty(&phase);
          ok = loyalty && ok;
        }
        if (ok) {
          if (plan.applyUprising) {
            const int uprising = target->ApplyUprising(&phase);
            const int kind = target->ApplyControlKindUprising(&phase);
            ok = kind && uprising;
          }
          if (ok && plan.applyResources) {
            const int energy = target->ApplyEnergy(&phase);
            const int rawMaterial = target->ApplyRawMaterial(&phase);
            ok = rawMaterial && energy;
          }
        }
      }
    }
  }

  for (auto& i : plan.systemChildren) {
    if (isEmpty(i)) {
      break;
    }
    ok = target->UpdateChildren(&i, &phase) && ok;
  }
  if (plan.applyExtra) {
    ok = ReapplySystemCharacterViews(static_cast<GameObj*>(target), &phase) && ok;
  }

  if (ok) {
    ChildIterator fleets(target, &family::kFleets, kStateAny);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
      if (ok && !isEmpty(plan.fleetChildren[0])) {
        ok = static_cast<GameObj*>(fleets.Current())->UpdateChildren(&plan.fleetChildren[0], &phase) && ok;
      }
      ChildIterator ships(fleets.Current(), &family::kCapitalShips, kStateAny);
      for (ships.Begin(); ships.Current(); ships.Advance()) {
        if (!isEmpty(plan.shipChildren[0])) {
          ok = static_cast<GameObj*>(ships.Current())->UpdateChildren(&plan.shipChildren[0], &phase) && ok;
        }
        if (plan.applyExtra) {
          ok = ReapplySystemCharacterViews(static_cast<GameObj*>(ships.Current()), &phase) && ok;
        }
      }
    }
  }
  return ok;
}
