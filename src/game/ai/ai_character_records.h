#pragma once
#include <cstdint>

#include "ai/ai_ranked_list.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"

// The AI's view of the characters of one side (0x401c70-0x403dd0).
//
//   AiCharacterRecord     what the AI knows about one character, VT 0x6584a0 (0x68 bytes): an ObjectKeyNode (the key is the
//                         character's) with the side that sees it, flag words and the eight skills
//   AiCharacterRecordSet  the records of one side, VT 0x6584c0 (0x1c bytes), built incrementally by BuildStep()
//
// m_flags (AiCharacterRecord::m_flags.m_value) bits:
//   0x1 not completed, 0x2 usable by the side, 0x4 usable by the other side, 0x8 story character,
//   0x10 injured, 0x20 en route, 0x40 captured, 0x80 on a mission, 0x100 weak force, 0x200 strong force, 0x400 Darth Vader,
//   0x400000 on a mandatory mission, 0x800000 always eligible, 0x8000000 not suited for the job (set by Refresh),
//   0x10000000 special force, 0x20000000 character, 0x40000000 story key, 0x80000000 needs a Refresh()
// m_capabilities bits: 0x1 diplomacy ... 0x800/0x1000/0x2000 research (see MissionCapability), 0x1000000/0x2000000/0x4000000
//   can be admiral/commander/general, 0x10000000.. the four special-force types.

class AiSideController;
class GameObjNotifier;
class DatRecord;

namespace ai_character_flag
{
constexpr uint32_t kNotCompleted = 0x1;
constexpr uint32_t kOwnSide = 0x2;
constexpr uint32_t kOtherSide = 0x4;
constexpr uint32_t kStoryCharacter = 0x8;
constexpr uint32_t kInjured = 0x10;
constexpr uint32_t kEnroute = 0x20;
constexpr uint32_t kCaptured = 0x40;
constexpr uint32_t kOnMission = 0x80;
constexpr uint32_t kWeakForce = 0x100;
constexpr uint32_t kStrongForce = 0x200;
constexpr uint32_t kDarthVader = 0x400;
constexpr uint32_t kOnMandatoryMission = 0x400000;
constexpr uint32_t kAlwaysEligible = 0x800000;
constexpr uint32_t kUnsuited = 0x8000000;
constexpr uint32_t kSpecialForce = 0x10000000;
constexpr uint32_t kCharacter = 0x20000000;
constexpr uint32_t kStoryKey = 0x40000000;
constexpr uint32_t kDirty = 0x80000000;
} // namespace ai_character_flag

class AiCharacterRecord : public ObjectKeyNode
{
public:
  static constexpr int kSkillCount = 8;
  static constexpr int kValueCount = 10;

  AiCharacterRecord(); // 0x401c70
  AiCharacterRecord(GameObjNotifier* character, int side, AiSideController* owner); // 0x401d20
  ~AiCharacterRecord() override; // 0: 0x401d00 (deleting), 0x401ff0
  void Save(ostream* out) override; // 4: 0x402120

  void ReadRecord(istream* in, AiSideController* owner); // 0x402070
  void MarkChanged(); // 0x4021b0: flags |= Dirty and tells the owner
  void SetLocation(const ObjectKey* location, const ObjectKey* previous); // 0x402210 (the previous one is ignored)
  int Refresh(); // 0x402230: re-reads the character; 1 if it was dirty
  void FindMissionCapabilities(); // 0x402720: asks a mission order which missions it can do
  void SetSpecialForceCapability(const ObjectKey* recordKey); // 0x402e80
  ObjectKey* GetRecordKey(ObjectKey* out); // 0x403040: the key of the character

  int m_side; // +0x1c the side whose view this is (1 or 2)
  uint32_t m_controlSide; // +0x20 the character's own control side
  int m_x24; // +0x24 zeroed whenever an agent takes or gives up the character
  int m_agentId; // +0x28 id of the AiAgent the character is assigned to (0: none)
  int m_eligibilityRank; // +0x2c compared by AiCharacterRecordSet::RankEligible; saved
  PersistentInt m_flags; // +0x30
  PersistentInt m_capabilities; // +0x34
  ObjectKey m_location; // +0x38 the character's location (a system or a ship)
  AiSideController* m_owner; // +0x3c
  // +0x40: eight skills (Diplomacy, Espionage, ShipyardRD, TrainingFacilRD, ConstructionYardRD, Combat, Leadership,
  // Loyalty), the Force at index 8 and the record's field 4c at index 9.
  int m_values[kValueCount];
};

// The set also uses its first int to say which view it sees (1 Alliance, 2 Empire).
class AiCharacterRecordSet : public ObjectKeySet
{
public:
  AiCharacterRecordSet(); // 0x403060
  ~AiCharacterRecordSet() override; // 0: 0x403090 (deleting), 0x4030b0
  void Load(istream* in) override; // 4: 0x4030d0
  void Save(ostream* out) override; // 6: 0x403200

  // One step of the incremental build from the game objects of the side (called every tick until it returns 1).
  int BuildStep(); // 0x4032c0
  // Records with all of `requireFlags` and none of `forbidFlags` (same for the capability word) whose value `valueIndex` lies
  // in [minValue, maxValue], ranked by that value.
  AiRankedList* RankByValue(uint32_t requireFlags,
                            uint32_t requireCaps,
                            uint32_t forbidFlags,
                            uint32_t forbidCaps,
                            uint32_t valueIndex,
                            int minValue,
                            int maxValue,
                            int descending); // 0x403460
  // Like RankByValue, but the record must be eligible beyond `rank` (m_eligibilityRank) or always eligible.
  AiRankedList* RankEligible(uint32_t rank,
                             uint32_t requireFlags,
                             uint32_t requireCaps,
                             uint32_t forbidFlags,
                             uint32_t forbidCaps,
                             uint32_t valueIndex,
                             int minValue,
                             int maxValue,
                             int descending); // 0x4035d0
  // Records matching the flags, ranked by their distance from a target system (the value of the system set's record).
  AiRankedList* RankByDistance(const ObjectKey* target,
                               uint32_t requireFlags,
                               uint32_t requireCaps,
                               uint32_t forbidFlags,
                               uint32_t forbidCaps,
                               int descending); // 0x403750
  // Creates the record of a character that appeared after the build, or refreshes the existing one.
  void AddOrRefresh(const ObjectKey* key); // 0x403a30
  void SetRecordLocation(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to); // 0x403bc0
  void RemoveCharacter(const ObjectKey* key); // 0x403bf0
  // Follows the parents of `key` up to the system (family 0x90-0x97) or an object without a parent; returns that key.
  ObjectKey* FindContainingSystem(ObjectKey* out, const ObjectKey* key); // 0x403c20
  AiCharacterRecord* FindRecord(uint32_t key); // 0x403d30
  void RemoveRecord(uint32_t key); // 0x403d80

  int m_view; // +0x08 1 Alliance, 2 Empire
  int m_built; // +0x0c 1 once BuildStep() has seen every character
  AiSideController* m_owner; // +0x10
  GameObjNotifier* m_cursor; // +0x14 the next character of the build, null before and after it
  int m_usableCount; // +0x18 decremented for each usable character record added by BuildStep
};
