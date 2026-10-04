#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "ui/cool_list_item.h"

// Stand-in declaration (drop at merge for src/game/ui/object_list_item.h, owned by ui-misc): a list row naming a
// game object (VT 0x659ba0).
class ObjectListItem : public CoolListItem {
public:
    ObjectListItem(uint32_t id, const ObjectKey* key);  // 0x442130

    ObjectKey m_key;     // +0x68
    int m_unknown6c;     // +0x6c 1
    int m_unknown70;     // +0x70 0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectListItem, 0x74);
REB_ASSERT_OFFSET(ObjectListItem, m_key, 0x68);
#pragma clang diagnostic pop
