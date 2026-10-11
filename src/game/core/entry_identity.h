#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <set>
#include <unordered_map>

#include "core/game_string.h"
#include "core/stream_io.h"

// Payload identity and polymorphic lifetime support only; standard containers live in their domain owners.
class AvlNode
{
public:
  AvlNode();
  AvlNode(const AvlNode&);
  AvlNode& operator=(const AvlNode&) = delete;
  AvlNode(AvlNode&&) = delete;
  AvlNode& operator=(AvlNode&&) = delete;
  virtual ~AvlNode();
};

class IdNode : public AvlNode
{
public:
  IdNode();
  explicit IdNode(uint32_t id);
  ~IdNode() override;

  void SetId(uint32_t id);
  void ReadId(istream* in);
  void WriteId(ostream* out);

  uint32_t m_id; // +0x18
};

class LinkNode
{
public:
  LinkNode();
  LinkNode(const LinkNode&);
  LinkNode& operator=(const LinkNode&) = delete;
  LinkNode(LinkNode&&) = delete;
  LinkNode& operator=(LinkNode&&) = delete;
  virtual ~LinkNode();
  virtual void Read(istream* in);
  virtual void Write(ostream* out);
};

class IdLinkNode : public LinkNode
{
public:
  IdLinkNode();
  ~IdLinkNode() override;
  void Read(istream* in) override;
  void Write(ostream* out) override;

  uint32_t m_id; // +0x0c
};
