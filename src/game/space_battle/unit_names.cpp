#include "space_battle/unit_names.h"

#include <algorithm>
#include <array>

namespace reb::battle
{

std::string_view ClassName(int32_t typeIndex)
{
  static constexpr std::array<std::string_view, 38> kNames{"Mon Calamari Cruiser",
                                                           "Bulk Cruiser",
                                                           "Assault Frigate",
                                                           "Nebulon-B Frigate",
                                                           "Alliance Escort Carrier",
                                                           "Corellian Corvette",
                                                           "Medium Transport",
                                                           "Bulk Transport",
                                                           "Corellian Gunship",
                                                           "Alliance Dreadnaught",
                                                           "CC-7700 Frigate",
                                                           "CC-9600 Frigate",
                                                           "Bulwark Battlecruiser",
                                                           "Liberator Cruiser",
                                                           "Dauntless Cruiser",
                                                           "Strike Cruiser",
                                                           "Lancer Frigate",
                                                           "Interdictor Cruiser",
                                                           "Carrack Light Cruiser",
                                                           "Victory Destroyer",
                                                           "Imperial Star Destroyer",
                                                           "Super Star Destroyer",
                                                           "Assault Transport",
                                                           "Galleon",
                                                           "Victory II Star Destroyer",
                                                           "Imperial II Star Destroyer",
                                                           "Star Galleon",
                                                           "Imperial Escort Carrier",
                                                           "Imperial Dreadnaught",
                                                           "X-wing",
                                                           "Y-wing",
                                                           "A-wing",
                                                           "B-wing",
                                                           "TIE Fighter",
                                                           "TIE Interceptor",
                                                           "TIE Bomber",
                                                           "TIE Defender",
                                                           "Death Star"};
  return typeIndex >= 0 && typeIndex < static_cast<int32_t>(kNames.size()) ? kNames[typeIndex] : std::string_view{};
}

namespace
{

std::string_view Trimmed(std::string_view text)
{
  const size_t first = text.find_first_not_of(' ');
  return first == std::string_view::npos ? std::string_view{}
                                         : text.substr(first, text.find_last_not_of(' ') - first + 1);
}

bool AllDigits(std::string_view text)
{
  return !text.empty() && std::ranges::all_of(text, [](char c) {
    return c >= '0' && c <= '9';
  });
}

} // namespace

UnitNaming NameUnit(const ViewUnit& unit, const ViewUnitDetail& detail, int ordinal)
{
  UnitNaming naming;
  const std::string_view known = ClassName(unit.typeIndex);
  naming.className = known.empty() ? detail.className : std::string(known);
  const std::string_view own = Trimmed(detail.name);
  // Native campaigns name ships by their number only (" 4"): the class part of the strategic name is missing.
  if (!own.empty() && !AllDigits(own)) {
    naming.name = own;
    naming.ownName = true;
    return naming;
  }
  const bool squadron = unit.kind == ViewUnitKind::Squadron;
  const std::string base = naming.className.empty() ? (squadron ? "Squadron" : "Ship") : naming.className;
  const std::string number = own.empty() ? std::to_string(ordinal) : std::string(own);
  naming.name = base + (squadron && !naming.className.empty() ? " Squadron " : " ") + number;
  return naming;
}

UnitNaming NameUnit(const BattleSnapshot& snapshot, UnitIndex unit,
                                            std::string_view fallbackClassName)
{
  const auto identityOf = [](const UnitEntry& entry) -> const UnitIdentity& {
    return std::visit([](const auto& value) -> const UnitIdentity& { return value.identity; }, entry);
  };
  const UnitEntry& entry = snapshot.units[unit];
  const UnitIdentity& identity = identityOf(entry);
  int ordinal = 1;
  for (UnitIndex previous = 0; previous < unit; ++previous) {
    const UnitEntry& previousEntry = snapshot.units[previous];
    if (!std::holds_alternative<CapitalShipEntry>(previousEntry) && !std::holds_alternative<SquadronEntry>(previousEntry)
        && !std::holds_alternative<DeathStarEntry>(previousEntry)) {
      continue;
    }
    const UnitIdentity& other = identityOf(previousEntry);
    if (other.side == identity.side && other.typeIndex == identity.typeIndex) {
      ++ordinal;
    }
  }
  ViewUnit view;
  view.typeIndex = identity.typeIndex;
  view.kind = std::holds_alternative<SquadronEntry>(entry) ? ViewUnitKind::Squadron : ViewUnitKind::CapitalShip;
  ViewUnitDetail detail;
  detail.name = identity.name;
  detail.className = fallbackClassName.empty() ? identity.className : std::string(fallbackClassName);
  return NameUnit(view, detail, ordinal);
}

} // namespace reb::battle
