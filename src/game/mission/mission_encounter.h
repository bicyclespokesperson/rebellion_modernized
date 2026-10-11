#pragma once
#include <cstdint>

#include "gameobj/child_iterator.h"
#include "gameobj/object_key.h"

// The mission "encounter" resolution (0x587100-0x58ad80): when a mission's team meets enemy units or persons at the
// target (or travels with a fleet), every team member and decoy gets rolled against what is there: decoys against
// units (fighters, capital ships, troops), foils, force detection, betrayal ("not betray") and "identify traitor".
// The strings of the original's debug trace calls (0x6ab3cc "foil", 0x6ab3d4 "decoy", 0x6ab3dc "not betray",
// 0x6ab3e8 "identify traitor", 0x6ab3fc "force detect") survive only as arguments of 0x5897b0, a shared `return 1`
// stub (cdecl), so the reconstruction leaves those calls out.
//
//   EncounterRunner (0x5897c0, VT 0x66a888, 0x70 bytes)   one resolution run of a mission against its location
//     EncounterContext (0x587550, VT 0x66a830, 0x3c bytes) embedded at +8: location, mission, counters
//     4 ObjectKeyValueSets at +0x50..+0x68: selection state of the phases
//   Visitors (VT with a destructor and one visit slot), applied by the context's ForEach... walkers:
//     MemberVisitorA (0x5871b0)  MemberVisitor (0x587200)  Visit(person, &stop, arg)
//     UnitVisitor    (0x587250)                            Visit(unit, container, &stop, arg)
//
// Names are provisional.

class Detector;
class Fleet;
class NotifyContext;
class Role;
class System;
class EncounterContext;

// Iterator over the missions of the families 0x60-0x68 (the character-target missions: Rescue, Palace, Bounty),
// VT 0x66a7e0 (destructor only).
class CharacterTargetMissionIterator : public ChildIterator {
public:
    explicit CharacterTargetMissionIterator(GameObjNotifier* parent);  // 0x587100 (state filter 1)
    ~CharacterTargetMissionIterator() override;                           // 0x587180 (deleting), 0x5871a0
};

// ---- visitor bases -------------------------------------------------------------------------------------------------

class MemberVisitorA {
public:
    explicit MemberVisitorA(EncounterContext* context);  // 0x5871b0
    virtual ~MemberVisitorA();                            // 0x5871d0 (deleting), 0x5871f0
    virtual int Visit(Role* member, int* stop, NotifyContext* ctx) = 0;

    EncounterContext* m_context;  // +0x04
};

class MemberVisitor {
public:
    explicit MemberVisitor(EncounterContext* context);  // 0x587200
    virtual ~MemberVisitor();                            // 0x587220 (deleting), 0x587240
    virtual int Visit(Role* member, int* stop, NotifyContext* ctx) = 0;

    EncounterContext* m_context;  // +0x04
};

class UnitVisitor {
public:
    explicit UnitVisitor(EncounterContext* context);  // 0x587250
    virtual ~UnitVisitor();                            // 0x587270 (deleting), 0x587290
    virtual int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) = 0;

    EncounterContext* m_context;  // +0x04
};

// ---- concrete visitors ---------------------------------------------------------------------------------------------

// Counts the visited persons.
class MemberCounter : public MemberVisitor {
public:
    explicit MemberCounter(EncounterContext* context);  // 0x5872a0
    ~MemberCounter() override;                            // 0x5872c0 (deleting), 0x5872e0
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x5872f0

    int m_count;  // +0x08
};

// Counts the visited units (visit slot is the shared stub 0x58d1a0).
class UnitCounter : public UnitVisitor {
public:
    explicit UnitCounter(EncounterContext* context);  // 0x587310
    ~UnitCounter() override;                            // 0x587330 (deleting), 0x587350
    int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) override;  // 0x58d1a0

    int m_count;  // +0x08
};

// Stops at the person with index m_target and keeps it.
class MemberPicker : public MemberVisitor {
public:
    explicit MemberPicker(EncounterContext* context);  // 0x587360
    ~MemberPicker() override;                            // 0x587390 (deleting), 0x5873b0
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x5873c0

    int m_index;     // +0x08
    int m_target;    // +0x0c
    Role* m_result;  // +0x10
};

// Stops at the unit with index m_target and keeps it with its container.
class UnitPicker : public UnitVisitor {
public:
    explicit UnitPicker(EncounterContext* context);  // 0x587410
    ~UnitPicker() override;                            // 0x587440 (deleting), 0x587460
    int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) override;  // 0x587470

    int m_index;            // +0x08
    int m_target;           // +0x0c
    Detector* m_result;     // +0x10
    Fleet* m_resultFleet;   // +0x14
};

// Sums the Espionage skill of the visited persons.
class MemberEspionageSum : public MemberVisitor {
public:
    explicit MemberEspionageSum(EncounterContext* context);  // 0x5874c0
    ~MemberEspionageSum() override;                            // 0x5874e0 (deleting), 0x587500
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x587510

    int m_sum;  // +0x08
};

// Ends the decoying of every visited person.
class ClearDecoyingVisitor : public MemberVisitor {
public:
    explicit ClearDecoyingVisitor(EncounterContext* context);  // 0x589000
    ~ClearDecoyingVisitor() override;                            // 0x589020 (deleting), 0x589040
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x589050
};

// Rolls "not betray" for every visited person (EncounterContext::RollNotBetray).
class BetrayalRollVisitor : public MemberVisitor {
public:
    explicit BetrayalRollVisitor(EncounterContext* context);  // 0x589080
    ~BetrayalRollVisitor() override;                            // 0x5890a0 (deleting), 0x5890c0
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x5890d0
};

// Clears IsDecoyed on every visited unit.
class ClearDecoyedVisitor : public UnitVisitor {
public:
    explicit ClearDecoyedVisitor(EncounterContext* context);  // 0x589110
    ~ClearDecoyedVisitor() override;                            // 0x589130 (deleting), 0x589150
    int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) override;  // 0x589160
};

// For a visited person: picks a random unit and resolves the failed decoy (EncounterContext::ResolveFailedDecoy).
class UnitAgainstMemberVisitor : public MemberVisitor {
public:
    explicit UnitAgainstMemberVisitor(EncounterContext* context);  // 0x589190
    ~UnitAgainstMemberVisitor() override;                            // 0x5891b0 (deleting), 0x5891d0
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x5891e0
};

// Fight between a person and the person it has encountered (key at Character +0x9c).
class EncounterFightVisitor : public MemberVisitorA {
public:
    explicit EncounterFightVisitor(EncounterContext* context);  // 0x589250
    ~EncounterFightVisitor() override;                            // 0x589270 (deleting), 0x589290
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x5892a0
};

// Persons without an encounter key meet a unit (UnitAgainstMemberVisitor), the others fight (EncounterFightVisitor).
class EncounterDispatchVisitor : public MemberVisitorA {
public:
    explicit EncounterDispatchVisitor(EncounterContext* context);  // 0x589310
    ~EncounterDispatchVisitor() override;                            // 0x589330 (deleting), 0x589350
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x589360
};

// "identify traitor" roll for every visited person (EncounterContext::RollIdentifyTraitor).
class IdentifyTraitorVisitor : public MemberVisitorA {
public:
    explicit IdentifyTraitorVisitor(EncounterContext* context);  // 0x589440
    ~IdentifyTraitorVisitor() override;                            // 0x589460 (deleting), 0x589480
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x589490
};

// Collects the keys of the persons that can still be betrayed / identified into m_keys.
class BetrayalCandidateCollector : public MemberVisitorA {
public:
    BetrayalCandidateCollector(EncounterContext* context, ObjectKeyValueSet* keys);  // 0x5894d0
    ~BetrayalCandidateCollector() override;                                            // 0x5894f0 (deleting), 0x589510
    int Visit(Role* member, int* stop, NotifyContext* ctx) override;  // 0x589520

    ObjectKeyValueSet* m_keys;  // +0x08
};

// For a unit: picks a random decoy and tries the decoy roll (EncounterContext::TryDecoy).
class UnitAgainstDecoyVisitor : public UnitVisitor {
public:
    explicit UnitAgainstDecoyVisitor(EncounterContext* context);  // 0x5895d0
    ~UnitAgainstDecoyVisitor() override;                            // 0x5895f0 (deleting), 0x589610
    int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) override;  // 0x589620
};

// For a unit: rolls the foil table against the team (EncounterContext::RollFoil); average Espionage and count of the
// team are computed on the first visit.
class UnitFoilVisitor : public UnitVisitor {
public:
    explicit UnitFoilVisitor(EncounterContext* context);  // 0x589680
    ~UnitFoilVisitor() override;                            // 0x5896b0 (deleting), 0x5896d0
    int Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) override;  // 0x5896e0

    int m_foiled;        // +0x08
    int m_prepared;      // +0x0c
    int m_averageSkill;  // +0x10
    int m_teamCount;     // +0x14
};

// ---- the context ---------------------------------------------------------------------------------------------------

class EncounterContext {
public:
    EncounterContext();           // 0x587550
    virtual ~EncounterContext();  // 0x587590 (deleting), 0x5875b0

    // Units (squadrons, troops, capital ships of the fleets) at the location, see ForEachUnit.
    int VisitUnits(UnitVisitor* visitor, NotifyContext* ctx);              // 0x5875c0
    int VisitUndecoyedUnits(UnitVisitor* visitor, NotifyContext* ctx);     // 0x5875e0
    int VisitUndecoyedCapShips(UnitVisitor* visitor, NotifyContext* ctx);  // 0x587600
    int VisitUndecoyedSquadrons(UnitVisitor* visitor, NotifyContext* ctx); // 0x587620
    int ForEachUnit(UnitVisitor* visitor, int capShips, int squadrons, int troops, int skipDecoyed, NotifyContext* ctx);  // 0x587640

    // The mission's persons: characters / special forces, team / decoys.
    int VisitDecoys(MemberVisitor* visitor, NotifyContext* ctx);             // 0x587b30
    int VisitTeam(MemberVisitor* visitor, NotifyContext* ctx);               // 0x587b50
    int VisitTeamSpecialForces(MemberVisitor* visitor, NotifyContext* ctx);  // 0x587b70
    int VisitAllMembers(MemberVisitor* visitor, NotifyContext* ctx);         // 0x587b90
    int ForEachMember(MemberVisitor* visitor, int characters, int specialForces, int team, int decoys, int flag, NotifyContext* ctx);  // 0x587bb0

    // Same walks over the major / minor characters (the visitor class is MemberVisitorA).
    int VisitAllMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx);    // 0x587f60
    int VisitTeamMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx);   // 0x587f80
    int VisitDecoyMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx);  // 0x587fa0
    int VisitMajors(MemberVisitorA* visitor, NotifyContext* ctx);           // 0x587fc0
    int VisitMinors(MemberVisitorA* visitor, NotifyContext* ctx);           // 0x587fe0
    int ForEachMajorMinor(MemberVisitorA* visitor, int majors, int minors, int team, int decoys, int flag, NotifyContext* ctx);  // 0x588000

    int GetMemberValue(Role* member, int flag, int* out);  // 0x5883b0: m_valueA / m_valueB by the member's parent
    int CountUnits(int* out, NotifyContext* ctx);      // 0x588470 -> m_unitCount
    int CountTeam(int* out, NotifyContext* ctx);       // 0x5884f0 -> m_teamCount
    int CountDecoys(int* out, NotifyContext* ctx);     // 0x588570 -> m_decoyCount
    int CountAll(NotifyContext* ctx);                  // 0x5885f0
    int PickRandomUnit(Detector** unit, Fleet** container, NotifyContext* ctx);  // 0x588650
    int PickRandomDecoy(Role** decoy, NotifyContext* ctx);                       // 0x588700
    int AverageTeamEspionage(int* out, NotifyContext* ctx);                      // 0x5887a0
    int IsForeignDetector(Role* member, int* out);                    // 0x588840
    int IsBetrayalCandidate(Role* member, int* out);                  // 0x5888a0
    int ResolveFailedDecoy(Detector* unit, Fleet* container, Role* member, NotifyContext* ctx);  // 0x5888f0
    int RollFoil(Detector* unit, Fleet* container, int skill, int count, int* foiled, NotifyContext* ctx);  // 0x588a90
    int TryDecoy(Role* member, Detector* unit, Fleet* container, NotifyContext* ctx);  // 0x588b90
    int ResolveEncounterFight(Role* member, Role* other, NotifyContext* ctx);          // 0x588d30
    int RollNotBetray(Role* member, int* betrays, NotifyContext* ctx);                 // 0x588da0
    int RollIdentifyTraitor(Role* member, int* identified, NotifyContext* ctx);        // 0x588e80

    int m_unk04;                  // +0x04
    GameObjNotifier* m_location;  // +0x08, the system (or the fleet's system) where the mission is
    GameObjNotifier* m_mission;   // +0x0c
    int m_valueA;                 // +0x10, GetMemberValue result for members whose parent is the owner's
    int m_valueB;                 // +0x14, otherwise
    int m_flag18;                 // +0x18, units on the ground (squadrons / troops) take part
    int m_flag1c;                 // +0x1c, fleets take part
    int m_flag20;                 // +0x20, capital ships take part
    int m_flag24;                 // +0x24, fighter squadrons take part
    int m_flag28;                 // +0x28, troops take part
    int m_teamCount;              // +0x2c
    int m_decoyCount;             // +0x30
    int m_unitCount;              // +0x34
    ObjectKey m_betrayerKey;      // +0x38
};

// ---- the runner ----------------------------------------------------------------------------------------------------

class EncounterRunner {
public:
    EncounterRunner();           // 0x5897c0
    virtual ~EncounterRunner();  // 0x589850 (deleting), 0x589870

    // Runs the encounter in both directions (m_secondPass 1 then 0); 0 if the mission is already over.
    int Run(NotifyContext* ctx);                                               // 0x5898f0
    int RunPass(NotifyContext* ctx);                                           // 0x589970
    int Prepare(NotifyContext* ctx);                                           // 0x589a40
    int RunDetection(NotifyContext* ctx);                                      // 0x589e40
    int RunEncounters(NotifyContext* ctx);                                     // 0x589f10
    int RunMembers(NotifyContext* ctx);                                        // 0x58a020
    int RunPromotion(NotifyContext* ctx);                                      // 0x58a130
    int RunFinalMembers(NotifyContext* ctx);                                   // 0x58a1c0
    int RunForceDetectDispatch(ObjectKey* chosen, NotifyContext* ctx);         // 0x58a2c0
    int ClearSelection();                                                      // 0x58a3b0
    int ForceDetect(ObjectKey* picked, ObjectKey* chosen, NotifyContext* ctx);  // 0x58a3f0
    int MarkSelected(ObjectKey* picked, ObjectKey* chosen);                    // 0x58a530
    int PrepareSelections(NotifyContext* ctx);                                 // 0x58a5b0
    int PrepareMemberSelections(NotifyContext* ctx);                           // 0x58a5f0
    int PrepareUnitSelections(NotifyContext* ctx);                             // 0x58a6c0
    int VisitLocationPersons(GameObjNotifier* parent);                         // 0x58a7e0
    int ChooseDetection(ObjectKey* chosen);                // 0x58a9e0
    int ChooseMemberDetection(ObjectKey* chosen);          // 0x58ab00
    // Selection helpers over the key sets; flag +0x1c of the nodes is the "taken" state (1) / "marked" bit (2). They
    // are members that do not use `this`.
    int MarkKeyTaken(ObjectKey* key, ObjectKeySet* set);                       // 0x58ac20 (bit 2)
    int TakeKey(ObjectKey* key, ObjectKeySet* set, ObjectKey* chosen);         // 0x58ac50
    int TakeRandomFreeKey(ObjectKeySet* set, ObjectKey* chosen);               // 0x58aca0
    int ClearTakenFlags(ObjectKeySet* set);                                    // 0x58ad80

    int m_unk04;                 // +0x04, 1
    EncounterContext m_context;  // +0x08 (0x3c bytes)
    int m_secondPass;            // +0x44
    int m_unk48;                 // +0x48, nothing to resolve
    int m_unk4c;                 // +0x4c, the encounter is decided
    ObjectKeyValueSet m_setA;    // +0x50
    ObjectKeyValueSet m_setB;    // +0x58
    ObjectKeyValueSet m_setC;    // +0x60
    ObjectKeyValueSet m_setD;    // +0x68
};
