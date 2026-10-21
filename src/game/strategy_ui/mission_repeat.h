#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gameobj/object_key.h"

class HumanPlayer;
class OrderBase;
class istream;
class ostream;

// What it takes to send a finished mission's team out again.
struct MissionRepeatPlan {
  ObjectKey mission;     // the finished mission; ties together the reports it produced
  ObjectKey missionType; // the mission record (the mission order's mission key)
  ObjectKey target;      // the finished mission's objective
  ObjectKey system;      // where it took place
  std::vector<ObjectKey> agents;
  std::vector<ObjectKey> decoys;
  std::string name; // the mission's name, for the reports ("Sabotage")
};

// "Repeat mission" for the human player's mission reports. A report on a finished mission of the player's own team
// keeps a plan to send the surviving agents and decoys out again: against the same target while it is still there,
// otherwise against the next one of its kind at that system (sabotage, abduction, assassination, rescue), or to the
// same system (recruitment and Jedi training, and espionage and reconnaissance after a failure). Missions that carry
// on by themselves (diplomacy, research, uprisings) have no plan. A requested plan waits until the whole team is back
// and free, which the arrival report mentions, and then goes out as an ordinary mission order.
class MissionRepeats
{
  public:
  enum class Result { Unavailable, Sent, Queued };

  // Once per frame, after the frame's notifications became notices.
  void Update(HumanPlayer& player);
  bool HasOffer(uint32_t noticeId) const;
  const MissionRepeatPlan* Offer(uint32_t noticeId) const;
  Result Request(HumanPlayer& player, uint32_t noticeId);
  // The player gave members of an offered or queued repeat other orders, so it no longer applies.
  void OnPlayerOrder(OrderBase* order);

  // Saved state: the offers of the reports still listed and the queued repeats.
  bool HasSavedState() const
  {
    return !m_offers.empty() || !m_queued.empty();
  }
  void Write(ostream* out) const;
  bool Read(istream* in);
  // After a load: the reports already listed get no new offers (theirs were saved).
  void ForgetListedReports(HumanPlayer& player);

  const std::vector<MissionRepeatPlan>& Queued() const
  {
    return m_queued;
  }

  private:
  struct Team {
    std::vector<ObjectKey> agents;
    std::vector<ObjectKey> decoys;
  };

  void RecordTeams(int side);
  void ExamineNewNotices(HumanPlayer& player);
  bool MakeOffer(int side, const ObjectKey& missionKey, MissionRepeatPlan* plan) const;
  void AnnounceArrival(int side, uint32_t noticeId, HumanPlayer& player);
  void SendReady(HumanPlayer& player);
  void Announce(const MissionRepeatPlan& plan, std::string line, uint32_t tick);
  void DropQueuedIf(const std::function<bool(const MissionRepeatPlan&)>& drop);
  bool Send(HumanPlayer& player, const MissionRepeatPlan& plan);

  std::map<uint32_t, Team> m_teams; // mission key -> its team while it was still out
  uint32_t m_teamsTick = 0;
  std::set<uint32_t> m_examined; // notice ids
  // What the arrival report of a repeat's team adds; a repeat often goes out a few frames before that report is made.
  struct Announcement {
    std::vector<ObjectKey> members;
    std::string line;
    uint32_t tick = 0;
  };

  std::map<uint32_t, MissionRepeatPlan> m_offers; // notice id -> plan
  std::vector<MissionRepeatPlan> m_queued;
  std::vector<Announcement> m_announcements;
  std::set<uint32_t> m_announcedEarly; // first agents of queued repeats whose arrival report came before they set out
  bool m_submitting = false;
};
