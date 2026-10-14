// Stand-ins for the 3D tactical code that the reconstructed code still references (presentation: the tactical window, the
// 3D view, sound, the 3D object manager and its lockstep streaming, the 3D movement and weapon-hit code of the ships,
// the groups', formations' and 3D side's behaviour). None of it runs in an auto-resolved battle (count-1 captures of every
// one of these over space_battle_autoresolve and _retry reach only the functions reconstructed elsewhere), so each stub does
// nothing and returns 0.
//
// The stubs carry the original names, so they are hooks: they replace the original code in the hybrid. This file is therefore
// compiled into the hybrid only with -DREB_NO_TACTICAL_3D=ON (see CMakeLists.txt), together with the battle flow resolving
// a "tactical" vote as auto-resolve; the default hybrid still runs the original 3D code. It is compiled either way.
#include "app/app_externals.h"
#include "tactical/tactical_externals.h"
#include "tactical/tactical_externals_2b.h"
#include "tactical/tactical_formation.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_lists.h"
#include "tactical/tactical_moving_object.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_ship.h"
#include "tactical/tactical_side.h"
#include "tactical/tactical_simulator.h"
#include "tactical/tactical_units.h"
#include "tactical/tactical_weapon_profile.h"

void* FUN_0059a8d0(void* memory,
                   HINSTANCE instance,
                   int x,
                   int y,
                   int width,
                   int height,
                   CoolWindow* parent,
                   void* directDrawHost)
{
  return nullptr;
} // 0x59a8d0
void FUN_0059b100(CoolWindow* window, LPARAM drawItem)
{
} // 0x59b100
#ifndef REB_NO_TACTICAL_3D
int FUN_0059cb60(TacticalWindow* window, void* stream)
{
  return 0;
} // 0x59cb60
int FUN_0059cb80(TacticalWindow* window, void* stream)
{
  return 0;
} // 0x59cb80
#endif
void FUN_0059e840(TacticalHub* hub)
{
} // 0x59e840
TacticalHub* FUN_0059ed70()
{
  return nullptr;
} // 0x59ed70
void FUN_0059ee10(TacticalHub* hub)
{
} // 0x59ee10
void FUN_0059eec0(TacticalHub* hub, int a)
{
} // 0x59eec0
void FUN_0059eef0(TacticalHub* hub, int a)
{
} // 0x59eef0
void FUN_0059f4d0(TacticalHub* hub, TacticalShipBase* ship)
{
} // 0x59f4d0
int FUN_0059f640(TacticalHub* hub, TacticalShipBase* ship)
{
  return 0;
} // 0x59f640
int FUN_0059f6e0(TacticalHub* hub)
{
  return 0;
} // 0x59f6e0
void FUN_005a0240()
{
} // 0x5a0240
void FUN_005a0320(TacticalHub* hub, int groupId)
{
} // 0x5a0320
void FUN_005a0a50(TacticalHub* hub, int kind, int arg)
{
} // 0x5a0a50
void FUN_005a0ad0(TacticalHub* hub, int arg)
{
} // 0x5a0ad0
void FUN_005a0b50(TacticalHub* hub, int a, int b)
{
} // 0x5a0b50
void FUN_005a0bf0(TacticalHub* hub, uint32_t* out)
{
} // 0x5a0bf0
void FUN_005a0c20(TacticalHub* hub, uint32_t* out)
{
} // 0x5a0c20
void FUN_005a5cf0(TacticalHub* hub, int attacker, int target)
{
} // 0x5a5cf0
void FUN_005a5fd0(TacticalHub* hub, int arg)
{
} // 0x5a5fd0
TacticalVector* FUN_005a7490(TacticalObjectManager* manager, TacticalVector* out, int side)
{
  return nullptr;
} // 0x5a7490
int FUN_005a8a70(TacticalObjectManager* manager, int low, int high)
{
  return 0;
} // 0x5a8a70
void FUN_005a9070(TacticalObjectManager* manager, TacticalShipBase* ship)
{
} // 0x5a9070
void FUN_005a90c0(TacticalObjectManager* manager, TacticalShipBase* ship)
{
} // 0x5a90c0
void FUN_005a9130(TacticalObjectManager* manager, TacticalShipBase* ship)
{
} // 0x5a9130
void FUN_005a9170(TacticalObjectManager* manager, TacticalShipBase* ship)
{
} // 0x5a9170
int TacticalObjectManager::AddDeathStar(int family,
                                        int id,
                                        int notCompleted,
                                        int hasFacility,
                                        float hull,
                                        float hullNow,
                                        int hangarFree)
{
  return 0;
} // 0x5a9220
TacticalUnit* FUN_005a9330(TacticalObjectManager* manager)
{
  return nullptr;
} // 0x5a9330
int TacticalObjectManager::AddShipToWorld(TacticalShipBase* ship)
{
  return 0;
} // 0x5a9340
int TacticalObjectManager::AddCharacterToWorld(TacticalUnit* character)
{
  return 0;
} // 0x5a93e0
TacticalCharacterUnit* FUN_005a9430(TacticalObjectManager* manager, int handle)
{
  return nullptr;
} // 0x5a9430
int TacticalObjectManager::AddSpecialForceToWorld(TacticalUnit* force)
{
  return 0;
} // 0x5a9450
int TacticalObjectManager::AddLaunchedFighterToWorld(TacticalShipBase* fighter)
{
  return 0;
} // 0x5a9470
unsigned TacticalObjectManager::Step(float milliseconds)
{
  return 0;
} // 0x5aacb0
void TacticalObjectManager::ForwardEvent(int a, int b, int c)
{
} // 0x5aafc0
void TacticalObjectManager::ResetView()
{
} // 0x5ab0e0
double FUN_005ad750(TacticalObjectManager* manager, int side)
{
  return 0;
} // 0x5ad750
double FUN_005ad7e0(TacticalObjectManager* manager, int side)
{
  return 0;
} // 0x5ad7e0
int FUN_005ad870(TacticalObjectManager* manager, TacticalShipBase* ship)
{
  return 0;
} // 0x5ad870
int TacticalObjectManager::WorldCheck()
{
  return 0;
} // 0x5ad930
int FUN_005ae1d0(TacticalObjectManager* manager, int side)
{
  return 0;
} // 0x5ae1d0
void TacticalCapitalShip::PlayHitSoundA()
{
} // 0x5b00a0
void TacticalCapitalShip::PlayHitSoundB()
{
} // 0x5b00c0
void TacticalCapitalShip::PlayHitSoundC()
{
} // 0x5b00e0
void TacticalCapitalShip::PlayDestructionSound()
{
} // 0x5b0170
void TacticalShipBase::Advance(float milliseconds)
{
} // 0x5b2f30
void TacticalShipBase::Unreconstructed3DShipSlot34()
{
} // 0x5b3a40
void TacticalShipBase::Unreconstructed3DShipSlot31(TacticalVector* out)
{
} // 0x5b4960
int TacticalShipBase::Unreconstructed3DShipSlot32(float a)
{
  return 0;
} // 0x5b4d40
void TacticalShipBase::HandleHitEvent54(float damage, int attacker, int target)
{
} // 0x5b5100
void TacticalShipBase::HandleHitEvent53(float damage, int attacker, int target)
{
} // 0x5b5750
void TacticalShipBase::HandleHitEvent55(float damage, int attacker, int target)
{
} // 0x5b5b60
void TacticalShipBase::HandleHitEvent56(float damage, int attacker, int target)
{
} // 0x5b5f50
void TacticalShipBase::Unreconstructed3DShipSlot42(int a)
{
} // 0x5b6530
void BehaviorLink::UnreconstructedBehaviorLinkSlot01(int a)
{
} // 0x5b7dc0
void BehaviorLink::UnreconstructedBehaviorLinkSlot02()
{
} // 0x5b7e30
void BehaviorLink::UnreconstructedBehaviorLinkSlot03()
{
} // 0x5b7e40
void TacticalFighter::Advance(float milliseconds)
{
} // 0x5b8f70
double TacticalFighter::Turn(float step, float rate)
{
  return 0;
} // 0x5b97a0
int TacticalFighter::Unreconstructed3DShipSlot32(float a)
{
  return 0;
} // 0x5b9a20
void TacticalFighter::PlayHitSoundC()
{
} // 0x5ba0b0
void TacticalFighter::PlayDestructionSound()
{
} // 0x5ba0d0
void TacticalFighter::PlayHitSoundA()
{
} // 0x5ba0f0
void TacticalDeathStar::GetLaunchPoint(TacticalVector* out)
{
} // 0x5ba480
TacticalSoundManager* FUN_005ba8b0()
{
  return nullptr;
} // 0x5ba8b0
void FUN_005ba980(TacticalSoundManager* manager, int sound, int flag)
{
} // 0x5ba980
void FUN_005bab10(TacticalSoundManager* manager, int soundHandle)
{
} // 0x5bab10
void FUN_005bbd90(void* syncObject, int handle)
{
} // 0x5bbd90
void FUN_005bbe20(TacticalTurnSync* sync)
{
} // 0x5bbe20
void* FUN_005bbe70(int handle)
{
  return nullptr;
} // 0x5bbe70
void FUN_005be0f0()
{
} // 0x5be0f0
void FUN_005bf320()
{
} // 0x5bf320
void FUN_005c0f80(int paused)
{
} // 0x5c0f80
void FUN_005c1020(int a)
{
} // 0x5c1020
void FUN_005c1200(int unit)
{
} // 0x5c1200
void FUN_005c12d0(int target, int source, int kind, float volume, int level)
{
} // 0x5c12d0
void FUN_005c1320(int unit, int on)
{
} // 0x5c1320
void FUN_005c1360(int unit, int on)
{
} // 0x5c1360
void FUN_005c13d0(int unit, int kind, int sound)
{
} // 0x5c13d0
void FUN_005c1470(float seconds)
{
} // 0x5c1470
int FUN_005c1490()
{
  return 0;
} // 0x5c1490
void UnitRefTree::Read(istream* in)
{
} // 0x5c5de0
void UnitRefTree::Write(ostream* out)
{
} // 0x5c5e50
void UnitRefTree::Checksum(Crc32* crc)
{
} // 0x5c5ed0
void CapitalShipGroup::Unreconstructed3DGroupSlot27(int a)
{
} // 0x5c6230
void CapitalShipGroup::Unreconstructed3DGroupSlot26(int a)
{
} // 0x5c6270
void ShipGroup::Unreconstructed3DGroupSlot04(int a)
{
} // 0x5c65e0
void CapitalShipGroup::Unreconstructed3DGroupSlot05(int a)
{
} // 0x5c6610
void CapitalShipGroup::Unreconstructed3DGroupSlot06(int a, int b)
{
} // 0x5c7060
int FUN_005c7150(ShipGroup* group)
{
  return 0;
} // 0x5c7150
void FormationBase::Unreconstructed3DFormationSlot14(int a)
{
} // 0x5c7310
void FormationBase::Unreconstructed3DFormationSlot15()
{
} // 0x5c7350
int TacticalSideBase::HasNoShipsAtAll()
{
  return 0;
} // 0x5c7e80
int TacticalSideBase::Refresh(int a)
{
  return 0;
} // 0x5c7f70
int TacticalSideBase::UnreconstructedTacticalSideSlot06()
{
  return 0;
} // 0x5c8300
int TacticalSideBase::SplitIntoGroups(TargetPriorityTree* targets, int least, int most)
{
  return 0;
} // 0x5c83c0
int TacticalSideBase::UnreconstructedTacticalSideSlot08(int a, int b, int c)
{
  return 0;
} // 0x5c8660
int TacticalSideBase::ResetEvaluators()
{
  return 0;
} // 0x5c8d20
TargetPriorityTree* TacticalSideBase::BuildTargetList()
{
  return nullptr;
} // 0x5c8e10
void ShipGroup::Unreconstructed3DGroupSlot27(int a)
{
} // 0x5c9780
void ShipGroup::Unreconstructed3DGroupSlot26(int a)
{
} // 0x5c9940
void ShipGroup::Unreconstructed3DGroupSlot28(int a)
{
} // 0x5c9ae0
void ShipGroup::Unreconstructed3DGroupSlot03(int a)
{
} // 0x5c9bd0
void ShipGroup::Unreconstructed3DGroupSlot25(int a)
{
} // 0x5ca530
void FUN_005caef0(ShipGroup* group, int handle)
{
} // 0x5caef0
TacticalVector* ShipGroup::GetPosition(TacticalVector* out)
{
  return nullptr;
} // 0x5cafa0
void ShipGroup::Unreconstructed3DGroupSlot06(int a, int b)
{
} // 0x5cb050
void FUN_005cb1a0(ShipGroup* group, int a)
{
} // 0x5cb1a0
void ShipGroup::Unreconstructed3DGroupSlot08(int a)
{
} // 0x5cb430
void ShipGroup::Unreconstructed3DGroupSlot11(int a)
{
} // 0x5cb460
void ShipGroup::Unreconstructed3DGroupSlot09(int a)
{
} // 0x5cb490
void ShipGroup::Unreconstructed3DGroupSlot13(int a)
{
} // 0x5cb4d0
void ShipGroup::Unreconstructed3DGroupSlot14(int a)
{
} // 0x5cb5b0
void ShipGroup::Unreconstructed3DGroupSlot07(int a)
{
} // 0x5cb600
void ShipGroup::Unreconstructed3DGroupSlot19(int a)
{
} // 0x5cb770
void ShipGroup::Unreconstructed3DGroupSlot20(int a)
{
} // 0x5cb7b0
void ShipGroup::Unreconstructed3DGroupSlot23(int a)
{
} // 0x5cb7e0
void ShipGroup::Unreconstructed3DGroupSlot24(int a)
{
} // 0x5cb810
void ShipGroup::Unreconstructed3DGroupSlot22(int a)
{
} // 0x5cb840
void ShipGroup::BeforeMemberRemoved(int handle)
{
} // 0x5cb870
void ShipGroup::Unreconstructed3DGroupSlot15(int a)
{
} // 0x5cb8e0
int FUN_005cb980(ShipGroup* group)
{
  return 0;
} // 0x5cb980
void ShipGroup::Unreconstructed3DGroupSlot17()
{
} // 0x5cb990
void FighterGroup::Unreconstructed3DGroupSlot05(int a)
{
} // 0x5cc300
void MovingObject::Read(istream* in)
{
} // 0x5cd000
void MovingObject::Write(ostream* out)
{
} // 0x5cd0f0
void MovingObject::Checksum(Crc32* crc)
{
} // 0x5cd1e0
double MovingObject::Turn(float step, float rate)
{
  return 0;
} // 0x5cd460
void MovingObject::Advance(float milliseconds)
{
} // 0x5cd640
void* FUN_005ce500(void* memory, float time, int kind, int a, int b, int c)
{
  return nullptr;
} // 0x5ce500
GroupOrder* FUN_005d0820(void* memory, ShipGroup* group)
{
  return nullptr;
} // 0x5d0820
int TacticalAiSide::UnreconstructedTacticalSideSlot08(int a, int b, int c)
{
  return 0;
} // 0x5d1080
int TacticalAiSide::UnreconstructedTacticalSideSlot06()
{
  return 0;
} // 0x5d1410
void WeaponProfile::Read(istream* in)
{
} // 0x5d23b0
void WeaponProfile::Write(ostream* out)
{
} // 0x5d2400
void WeaponProfile::Checksum(Crc32* crc)
{
} // 0x5d2450
void FormationBase::Read(istream* in)
{
} // 0x5e9e00
void FormationBase::Write(ostream* out)
{
} // 0x5e9ec0
void FormationBase::Checksum(Crc32* crc)
{
} // 0x5e9f80
float FormationBase::GetSpeed()
{
  return 0;
} // 0x5ea4e0
void FormationBase::Unreconstructed3DFormationSlot11(int a, int b)
{
} // 0x5ea500
void FormationBase::Unreconstructed3DFormationSlot16()
{
} // 0x5ea740
double FormationBase::Turn(float step, float rate)
{
  return 0;
} // 0x5eae40
void FormationBase::Unreconstructed3DFormationSlot09(int a, int b)
{
} // 0x5eb040
void FormationBase::Unreconstructed3DFormationSlot17(int a)
{
} // 0x5eb190
int FighterFormation::Unreconstructed3DMovementSlot03(int a, int b, int c)
{
  return 0;
} // 0x5eb1a0
int FormationBase::Unreconstructed3DMovementSlot03(int a, int b, int c)
{
  return 0;
} // 0x5eb540
void FormationBase::Unreconstructed3DFormationSlot10(int a, int b)
{
} // 0x5ebdf0
void CapitalShipFormation::Unreconstructed3DFormationSlot09(int a, int b)
{
} // 0x5ebe30
float CapitalShipFormation::GetSpeed()
{
  return 0;
} // 0x5ec0b0
int CapitalShipFormation::Unreconstructed3DMovementSlot03(int a, int b, int c)
{
  return 0;
} // 0x5ec0d0
float FighterFormation::GetSpeed()
{
  return 0;
} // 0x5ec3c0
void CapitalShipFormation::Write(ostream* out)
{
} // 0x5ec4e0
void CapitalShipFormation::Read(istream* in)
{
} // 0x5ec510
void CapitalShipFormation::Checksum(Crc32* crc)
{
} // 0x5ec540
int FormationEvaluatorBase::Assign(TacticalSideBase* side,
                                   GroupRefTree* groups,
                                   TargetPriorityTree* targets,
                                   float limit)
{
  return 0;
} // 0x5ec6d0
void GroupBehavior::Unreconstructed3DBehaviorSlot05(int a)
{
} // 0x5edd50
void GroupBehavior::Unreconstructed3DBehaviorSlot06(int a, int b)
{
} // 0x5eddf0
void GroupBehavior::Unreconstructed3DBehaviorSlot07(int a)
{
} // 0x5ede90
void ShipGroupBehavior::Unreconstructed3DBehaviorSlot01(int a)
{
} // 0x5edf80
void ShipGroupBehavior::Unreconstructed3DBehaviorSlot02(int a)
{
} // 0x5ee250
void ShipGroupBehavior::Unreconstructed3DBehaviorSlot03()
{
} // 0x5ee280
void ShipGroupBehavior::Unreconstructed3DBehaviorSlot04(int a)
{
} // 0x5ee290
