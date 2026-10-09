#pragma once
#include "core/layout.h"
#include "data/table_entry.h"
#include "gameobj/object_key.h"

// The rows of the seed tables (fleet/reserve rows): the count at +0x24 and the list of objects to create at +0x28. Overlay
// for the callers that read a row; the concrete row classes derive from TableEntry directly.
class SeedTableEntry : public TableEntryWithValue {
public:
    ObjectRefList m_units;  // +0x28
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_OFFSET(SeedTableEntry, m_units, 0x28);
#pragma clang diagnostic pop
