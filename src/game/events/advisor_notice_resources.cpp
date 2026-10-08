#include "core/checked_new.h"
#include "core/containers.h"
#include "platform/win32.h"
#include "strategy_ui/advisor_decl.h"
#include "ui/cool_window.h"

namespace {

constexpr UINT kAllianceNoticeList = 0x6000;
constexpr UINT kEmpireNoticeList = 0x6800;
constexpr int kEntryResourceStride = 3;

}  // namespace

// Reads the side's list of advisor notices out of the RCDATA resources: the first resource is a count, each of the
// following ones names a notice (id in the second word, kind in the low nibble of the first byte).
int __cdecl LoadAdvisorNotices(int side, IdValueTree* notices) {
    UINT first = side == 1 ? kAllianceNoticeList : kEmpireNoticeList;
    int loaded = 0;
    HMODULE module = FindResourceModule(2);
    HGLOBAL countResource = LoadResource(module, FindResourceA(module, MAKEINTRESOURCEA(first), RT_RCDATA));
    const uint16_t* countBytes = static_cast<const uint16_t*>(LockResource(countResource));
    unsigned count = countBytes ? *countBytes : 0;
    FreeResource(countResource);
    if (count != 0) {
        loaded = 1;
        int kindBase = 0x10;
        for (int index = 1; index <= static_cast<int>(count); ++index, kindBase += 0x10) {
            HGLOBAL resource = LoadResource(module, FindResourceA(module, MAKEINTRESOURCEA((first + index * kEntryResourceStride) & 0xffff), RT_RCDATA));
            if (!resource) continue;
            const uint8_t* bytes = static_cast<const uint8_t*>(LockResource(resource));
            // BUG(original): tests the pointer of the count resource, not the entry's.
            if (countBytes) {
                IdValueNode* node = NewOrNull<IdValueNode>(0);
                node->m_value = (bytes[0] & 0xf) + kindBase;
                node->SetId(*reinterpret_cast<const uint16_t*>(bytes + 2));
                notices->AddWithId(node);
            }
            FreeResource(resource);
        }
    }
    return loaded;
}
