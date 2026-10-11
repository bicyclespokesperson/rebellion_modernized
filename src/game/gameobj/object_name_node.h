#pragma once
#include "core/containers.h"
#include "core/game_string.h"
#include "gameobj/object_key.h"

class DatRecord;

// A named reference to a game object: the object's key plus the display name of its data record (VT 0x66a88c, only the
// destructor is virtual). The mission manager (0x54bac0) fills one per mission object and adds it to an ObjectRefList.
class ObjectNameNode : public IdNode {
public:
    ObjectNameNode();           // 0x58adb0
    ~ObjectNameNode() override;  // 0x58ae30 (scalar deleting 0x58ae10)

    // Takes the key (record type byte + record id) and the name (the record's resource string id) from `record`.
    // Returns whether the name could be allocated.
    int Load(DatRecord* record);  // 0x58ae90
    // Constructs a copy of the name in *out (0x439290).
    std::string* GetName(std::string* out);  // 0x439290

    int m_nameValid;     // +0x1c, m_name.m_valid at construction time
    NullableKey m_key;   // +0x20
    std::string m_name;       // +0x24
};
