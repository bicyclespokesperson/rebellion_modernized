#pragma once
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "ui/cool_list_item.h"

// A list row that stands for a game object: the strategy windows' object lists (finders, sector/system windows,
// fleet window, encyclopedia...). Rows whose m_visible is 0 are skipped by Next/Previous.
class ObjectListItem : public CoolListItem {
public:
    ObjectListItem(uint32_t id, const ObjectKey* key);  // 0x442130
    explicit ObjectListItem(const ObjectKey* key);      // 0x4421d0: id = the key's low 24 bits
    virtual ~ObjectListItem();                          // 0x442250 (scalar deleting destructor 0x4421b0)
    virtual void Read(istream* in) override;            // slot 1, 0x5ae1e0: does nothing
    virtual void Write(ostream* out) override;          // slot 2, 0x5ae1e0: does nothing
    // The next/previous row, skipping invisible ones (linker-folded with RecordListItem's, 0x4ad730/0x4ad750).
    virtual CoolListItem* Next() override;              // slot 3
    virtual CoolListItem* Previous() override;          // slot 4

    ObjectKey m_key;  // +0x68
    int m_visible;    // +0x6c 1
    int m_unk70;      // +0x70 0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectListItem, 0x74);
REB_ASSERT_OFFSET(ObjectListItem, m_key, 0x68);
REB_ASSERT_OFFSET(ObjectListItem, m_visible, 0x6c);
#pragma clang diagnostic pop
