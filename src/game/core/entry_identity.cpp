#include "core/entry_identity.h"

// ---- AvlNode ---------------------------------------------------------------------------------------------

AvlNode::AvlNode() = default;

AvlNode::AvlNode(const AvlNode&) = default;

AvlNode::~AvlNode() = default;

// ---- IdNode -----------------------------------------------------------------------------------------------

IdNode::IdNode()
    : m_id(0)
{
}

IdNode::IdNode(uint32_t id)
    : m_id(id)
{
}

IdNode::~IdNode() = default;

void IdNode::SetId(uint32_t id)
{
  m_id = id;
}

void IdNode::ReadId(istream* in)
{
  ReadValue32(in, &m_id);
}

void IdNode::WriteId(ostream* out)
{
  WriteValue32(out, &m_id);
}

// ---- LinkNode / IdLinkNode ---------------------------------------------------------------------------------

LinkNode::LinkNode() = default;

LinkNode::LinkNode(const LinkNode&) = default;

LinkNode::~LinkNode() = default;

void LinkNode::Read(istream*) {}

void LinkNode::Write(ostream*) {}

IdLinkNode::IdLinkNode()
    : LinkNode()
    , m_id(0)
{
}

IdLinkNode::~IdLinkNode() = default;

void IdLinkNode::Read(istream* in)
{
  ReadValue32(in, &m_id);
}

void IdLinkNode::Write(ostream* out)
{
  WriteValue32(out, &m_id);
}
