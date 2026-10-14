#include "platform/win32.h"
#include "strategy_ui/advisor_decl.h"
#include "ui/cool_window.h"
#include <cstring>

namespace
{

constexpr UINT kAllianceNoticeList = 0x6000;
constexpr UINT kEmpireNoticeList = 0x6800;
constexpr int kEntryResourceStride = 3;

} // namespace

// Reads the side's list of advisor notices out of the RCDATA resources: the first resource is a count, each of the
// following ones names a notice (id in the second word, kind in the low nibble of the first byte).
int LoadAdvisorNotices(int side, std::map<uint32_t, int>* notices)
{
  UINT first = side == 1 ? kAllianceNoticeList : kEmpireNoticeList;
  int loaded = 0;
  HMODULE module = FindResourceModule(2);
  HRSRC countInfo = FindResourceA(module, MAKEINTRESOURCEA(first), RT_RCDATA);
  HGLOBAL countResource = countInfo ? LoadResource(module, countInfo) : nullptr;
  const uint16_t* countBytes = countResource ? static_cast<const uint16_t*>(LockResource(countResource)) : nullptr;
  unsigned count = countBytes && SizeofResource(module, countInfo) >= sizeof(uint16_t) ? *countBytes : 0;
  if (countResource)
    FreeResource(countResource);
  if (count != 0)
  {
    loaded = 1;
    int kindBase = 0x10;
    for (int index = 1; index <= static_cast<int>(count); ++index, kindBase += 0x10)
    {
      HRSRC entryInfo =
        FindResourceA(module, MAKEINTRESOURCEA((first + index * kEntryResourceStride) & 0xffff), RT_RCDATA);
      HGLOBAL resource = entryInfo ? LoadResource(module, entryInfo) : nullptr;
      if (!resource)
        continue;
      const auto* bytes = static_cast<const uint8_t*>(LockResource(resource));
      if (bytes && SizeofResource(module, entryInfo) >= 4)
      {
        uint16_t id;
        std::memcpy(&id, bytes + 2, sizeof(id));
        notices->emplace(id, (bytes[0] & 0xf) + kindBase);
      }
      FreeResource(resource);
    }
  }
  return loaded;
}
