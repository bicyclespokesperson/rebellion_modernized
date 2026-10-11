#pragma once
#include "data/table_entry.h"
#include "gameobj/object_key.h"

// The rows of the seed tables (fleet/reserve rows): the count at +0x24 and the list of objects to create at +0x28. Overlay
// for the callers that read a row; the concrete row classes derive from TableEntry directly.
class SeedTableEntry : public TableEntryWithValue
{
public:
  ObjectRefList m_units; // +0x28
};
