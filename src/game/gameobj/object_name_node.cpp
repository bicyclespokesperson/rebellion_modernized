#include "gameobj/object_name_node.h"

#include "data/dat_record.h"

namespace {

// The key of a data record: its type byte (+0x2c) over its id (+0x18).
uint32_t RecordKeyValue(const DatRecord* record) {
    return (record->m_third << 24) | (record->m_id & 0xffffff);
}

}  // namespace

ObjectNameNode::ObjectNameNode() : IdNode(), m_key(), m_name() { m_nameValid = m_name.m_valid; }

ObjectNameNode::~ObjectNameNode() {  }

int ObjectNameNode::Load(DatRecord* record) {
    m_key.m_value = RecordKeyValue(record);
    const ResourceStringId nameId = {static_cast<int16_t>(record->m_small), static_cast<int16_t>(record->m_small2)};
    const String name(&nameId);
    m_name.Assign(name);
    return m_name.m_valid;
}
