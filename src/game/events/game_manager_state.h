#pragma once
// The parts of the game manager's saved state that are not owned by a game object (0x568160-0x568fe0): the legacy
// option blocks and text slots that every save carries but nothing else uses, the queues of posted events and the
// tick table that hands out the per-key tick values. GameManager::Read/Write (game_manager.h) call them.
#include <cstdint>
#include <deque>
#include <map>
#include <memory>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "core/stream_io.h"
#include "data/seed_key.h"
#include "data/seed_list.h"
#include "gameobj/object_key.h"

class Crc32;

// ---- legacy option blocks -----------------------------------------------------------------------------------
// Four plain int blocks that are only read from and written to saves: g_legacyBlockA (20 ints) and
// g_legacyBlockB[3] (10 ints each). Nothing else in the executable touches them.

struct LegacyBlockA
{
  void Reset(); // 0x568160
  int Read(istream* in); // 0x5681b0
  int Write(ostream* out); // 0x5682d0

  int32_t m_value[20]; // +0x00
};

struct LegacyBlockB
{
  LegacyBlockB(); // 0x5683f0, every value -1
  int Read(istream* in); // 0x568420
  int Write(ostream* out); // 0x5684c0

  int32_t m_value[10]; // +0x00
};

extern LegacyBlockA g_legacyBlockA; // 0x6bb828
extern LegacyBlockB g_legacyBlockB[3]; // 0x6bb7a8

int ReadLegacyBlocks(istream* in); // 0x568560
int WriteLegacyBlocks(ostream* out); // 0x5685a0

// ---- legacy text slots --------------------------------------------------------------------------------------
// Ten strings with a count and one more int; saved as count, int, then the first `count` strings.

extern std::string g_legacyTexts[10]; // 0x6bb880
extern int g_legacyTextCount; // 0x6bb8f8
extern int g_legacyTextExtra; // 0x6bb8fc

void ResetLegacyTexts(); // 0x568a00
void ReadLegacyTexts(istream* in); // 0x568a30
void WriteLegacyTexts(ostream* out); // 0x568a80

// A table of `m_count` text slots of `m_width` bytes each, as old saves carry it (VT 0x663988). It is only ever read:
// the slots are allocated and emptied, then the saved bytes replace them when the save has any.
class LegacyTextTable
{
public:
  LegacyTextTable(); // 0x5686a0, 100 slots of 200 bytes
  virtual ~LegacyTextTable(); // 0x5686e0 (scalar deleting destructor 0x5686c0)

  // Frees the buffer; then reads count, width and the extra int, and the slot bytes if the save has any.
  int ReadFrom(istream* in); // 0x5686f0
  int Read(istream* in); // 0x568770, forwards
  void SetDefaults(); // 0x568780
  void Reset(); // 0x5687a0
  void AllocateSlots(); // 0x5687d0

  int m_count; // +0x04
  int m_width; // +0x08
  int m_unk0c; // +0x0c
  char* m_slots; // +0x10
};

// ---- posted-event queues ------------------------------------------------------------------------------------
// The two key-value queues at GameManager +0xb4/+0xb8: a FIFO of notification payloads saved as a flag, a count
// and, per node, its factory key followed by its own data.
class NotifPayload;

class PostedEventQueue
{
public:
  PostedEventQueue(); // 0x568840
  ~PostedEventQueue(); // 0x568880 (scalar deleting destructor 0x568860)

  int Read(istream* in); // 0x5688d0
  int Write(ostream* out); // 0x568980

  void Enqueue(std::unique_ptr<NotifPayload> payload);
  std::unique_ptr<NotifPayload> Dequeue();
  bool Empty() const
  {
    return m_payloads.empty();
  }
  void Clear();
  PostedEventQueue(const PostedEventQueue&) = delete;
  PostedEventQueue& operator=(const PostedEventQueue&) = delete;

  int m_flag;

private:
  std::deque<std::unique_ptr<NotifPayload>> m_payloads;
};

// ---- tick table ---------------------------------------------------------------------------------------------
// The table at GameManager +0xbc (VT 0x663990): the per-seed-key tick values, an AVL tree of SeedKeyCountNodes
// ordered by key. The saved form is a flag, a 32-bit count and the nodes.
class TickTable
{
public:
  TickTable(); // 0x568b40
  ~TickTable(); // 0x568b80 (scalar deleting destructor 0x568b60)

  virtual int Read(istream* in);
  virtual int ReadNodes(istream* in);
  virtual int Write(ostream* out);
  virtual int WriteNodes(ostream* out);
  virtual int Checksum(Crc32* crc);

  void Clear();
  int Insert(SeedKeyCountNode* node);
  size_t Count() const
  {
    return m_nodes.size();
  }
  const auto& Entries() const
  {
    return m_nodes;
  }

  // The node of `key`, or null.
  SeedKeyCountNode* Find(SeedKey* key); // 0x568de0
  // Adds a node for `key` (not checking that there is none already). Null if it could not be allocated.
  SeedKeyCountNode* Create(SeedKey* key); // 0x568d70
  void AddNode(SeedKeyCountNode* node); // 0x568d50, ignores null

  int m_ready; // +0x08, 1 once built

private:
  std::multimap<SeedKey, std::unique_ptr<SeedKeyCountNode>> m_nodes;
  bool m_clearing = false;
};
