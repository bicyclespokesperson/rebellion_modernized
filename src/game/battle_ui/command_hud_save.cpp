#include "battle_ui/command_hud.h"

#include <sstream>
#include <locale>

namespace reb::battle_ui
{
std::string CommandHud::SaveUiState() const
{
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "REB_HUD 1 " << m_briefing << ' ' << m_observing << ' ' << m_rosterCollapsed << ' '
      << m_commandCollapsed << ' ' << m_eventsCollapsed << ' ' << m_cardCollapsed << ' ' << m_focus << ' '
      << m_collapsedGroups.size();
  for (auto group : m_collapsedGroups) {
    out << ' ' << group;
  }
  return out.str();
}

bool CommandHud::RestoreUiState(std::string_view state)
{
  std::istringstream in{std::string(state)};
  in.imbue(std::locale::classic());
  std::string magic;
  int version = 0;
  bool briefing = false;
  bool observing = false;
  bool roster = false;
  bool command = false;
  bool events = false;
  bool card = false;
  battle::GroupId focus = battle::kNoGroup;
  size_t count = 0;
  if (!(in >> magic >> version >> briefing >> observing >> roster >> command >> events >> card >> focus >> count)
      || magic != "REB_HUD" || version != 1 || count > m_screen.View().groups.size()) {
    return false;
  }
  std::set<battle::GroupId> collapsed;
  for (size_t i = 0; i < count; ++i) {
    battle::GroupId group;
    if (!(in >> group)) {
      return false;
    }
    collapsed.insert(group);
  }
  in >> std::ws;
  if (!in.eof()) {
    return false;
  }
  m_briefing = briefing;
  m_observing = observing;
  m_rosterCollapsed = roster;
  m_commandCollapsed = command;
  m_eventsCollapsed = events;
  m_cardCollapsed = card;
  m_pendingFocus = focus;
  m_collapsedGroups = std::move(collapsed);
  m_autoFolded = true;
  return true;
}
} // namespace reb::battle_ui
