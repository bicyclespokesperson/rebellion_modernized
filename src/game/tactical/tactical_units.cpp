#include "tactical/tactical_units.h"

#include <new>

#include "tactical/tactical_externals.h"
#include "tactical/tactical_objmgr.h"

TacticalCharacterUnit::TacticalCharacterUnit(uint32_t family,
                                             uint32_t id,
                                             uint16_t spriteId,
                                             std::string name,
                                             int notAlliance,
                                             int isMajor,
                                             int isFirstCommander,
                                             int isSecondCommander,
                                             float scaleA,
                                             float scaleB,
                                             int shipHandle,
                                             int injured)
{
  m_channelA.m_data = nullptr;
  m_channelB.m_data = nullptr;
  m_objectFamily = family;
  m_objectId = id;
  m_spriteId = spriteId;
  (m_name = name);
  m_notAlliance = notAlliance;
  m_isMajor = isMajor;
  m_scaleA = scaleA;
  m_scaleB = scaleB;
  m_isFirstCommander = isFirstCommander;
  m_isSecondCommander = isSecondCommander;
  m_shipHandle = shipHandle;
  m_injured = injured;
}

TacticalCharacterUnit::~TacticalCharacterUnit() = default;

TacticalSpecialForceUnit::TacticalSpecialForceUnit(uint32_t family,
                                                   uint32_t id,
                                                   uint16_t spriteId,
                                                   std::string name,
                                                   int notAlliance,
                                                   int shipHandle)
{
  m_channelA.m_data = nullptr;
  m_channelB.m_data = nullptr;
  m_objectFamily = family;
  m_objectId = id;
  m_spriteId = spriteId;
  (m_name = name);
  m_notAlliance = notAlliance;
  m_shipHandle = shipHandle;
}

TacticalSpecialForceUnit::~TacticalSpecialForceUnit() = default;

// ---- streaming of the records (synchronisation and checksums of the 3D battle) -------------------------------------------

namespace
{

int bitsOf(float value)
{
  int bits;
  __builtin_memcpy(&bits, &value, sizeof bits);
  return bits;
}

int pointerWord(void* pointer)
{
  return static_cast<int>(reinterpret_cast<intptr_t>(pointer));
}

void* wordPointer(int word)
{
  return reinterpret_cast<void*>(static_cast<intptr_t>(word));
}

} // namespace

void TacticalCharacterUnit::Checksum(Crc32* crc)
{
  crc->AddInt(static_cast<int>(m_id));
  crc->AddInt(pointerWord(m_channelA.m_data));
  crc->AddInt(pointerWord(m_channelB.m_data));
  crc->AddInt(static_cast<int>(m_objectFamily));
  crc->AddInt(static_cast<int>(m_objectId));
  crc->AddShort(static_cast<int16_t>(m_spriteId));
  crc->AddInt(m_notAlliance);
  crc->AddInt(m_isMajor);
  crc->AddInt(m_isFirstCommander);
  crc->AddInt(m_isSecondCommander);
  crc->AddInt(bitsOf(m_scaleA));
  crc->AddInt(bitsOf(m_scaleB));
  crc->AddInt(m_shipHandle);
  crc->AddInt(m_injured);
}

void TacticalCharacterUnit::Read(istream* in)
{
  uint32_t id = 0;
  ReadValue32(in, &id);
  SetId(id);
  int word = 0;
  ReadValue32(in, &word);
  m_channelA.m_data = wordPointer(word);
  ReadValue32(in, &word);
  m_channelB.m_data = wordPointer(word);
  ReadValue32(in, &m_objectFamily);
  ReadValue32(in, &m_objectId);
  ReadValue16(in, &m_spriteId);
  ReadLegacyString(in, m_name);
  ReadValue32(in, &m_notAlliance);
  ReadValue32(in, &m_isMajor);
  ReadValue32(in, &m_isFirstCommander);
  ReadValue32(in, &m_isSecondCommander);
  ReadValue32(in, &m_scaleA);
  ReadValue32(in, &m_scaleB);
  ReadValue32(in, &m_shipHandle);
  ReadValue32(in, &m_injured);
}

void TacticalCharacterUnit::Write(ostream* out)
{
  int word = static_cast<int>(m_id);
  WriteValue32(out, &word);
  word = pointerWord(m_channelA.m_data);
  WriteValue32(out, &word);
  word = pointerWord(m_channelB.m_data);
  WriteValue32(out, &word);
  WriteValue32(out, &m_objectFamily);
  WriteValue32(out, &m_objectId);
  WriteValue16(out, &m_spriteId);
  WriteLegacyString(out, m_name);
  WriteValue32(out, &m_notAlliance);
  WriteValue32(out, &m_isMajor);
  WriteValue32(out, &m_isFirstCommander);
  WriteValue32(out, &m_isSecondCommander);
  WriteValue32(out, &m_scaleA);
  WriteValue32(out, &m_scaleB);
  WriteValue32(out, &m_shipHandle);
  WriteValue32(out, &m_injured);
}

void TacticalSpecialForceUnit::Checksum(Crc32* crc)
{
  crc->AddInt(static_cast<int>(m_id));
  crc->AddInt(pointerWord(m_channelA.m_data));
  crc->AddInt(pointerWord(m_channelB.m_data));
  crc->AddInt(static_cast<int>(m_objectFamily));
  crc->AddInt(static_cast<int>(m_objectId));
  crc->AddShort(static_cast<int16_t>(m_spriteId));
  crc->AddInt(m_notAlliance);
  crc->AddInt(m_shipHandle);
}

void TacticalSpecialForceUnit::Read(istream* in)
{
  uint32_t id = 0;
  ReadValue32(in, &id);
  SetId(id);
  int word = 0;
  ReadValue32(in, &word);
  m_channelA.m_data = wordPointer(word);
  ReadValue32(in, &word);
  m_channelB.m_data = wordPointer(word);
  ReadValue32(in, &m_objectFamily);
  ReadValue32(in, &m_objectId);
  ReadValue16(in, &m_spriteId);
  ReadLegacyString(in, m_name);
  ReadValue32(in, &m_notAlliance);
  ReadValue32(in, &m_shipHandle);
}

void TacticalSpecialForceUnit::Write(ostream* out)
{
  int word = static_cast<int>(m_id);
  WriteValue32(out, &word);
  word = pointerWord(m_channelA.m_data);
  WriteValue32(out, &word);
  word = pointerWord(m_channelB.m_data);
  WriteValue32(out, &word);
  WriteValue32(out, &m_objectFamily);
  WriteValue32(out, &m_objectId);
  WriteValue16(out, &m_spriteId);
  WriteLegacyString(out, m_name);
  WriteValue32(out, &m_notAlliance);
  WriteValue32(out, &m_shipHandle);
}

int TacticalCharacterTree::CountItems()
{
  return static_cast<int>(m_entries.Count());
}

TacticalCharacterTree::~TacticalCharacterTree()
{
  Clear();
}

void TacticalCharacterTree::Clear()
{
  m_entries.Clear();
}

TacticalCharacterUnit* TacticalCharacterTree::FirstItem()
{
  return m_entries.First();
}

TacticalCharacterUnit* TacticalCharacterTree::FindById(uint32_t id) const
{
  return m_entries.FindById(id);
}

uint32_t TacticalCharacterTree::AddWithId(TacticalCharacterUnit* node)
{
  return m_entries.AddWithId(node);
}

TacticalSpecialForceTree::~TacticalSpecialForceTree()
{
  Clear();
}

void TacticalSpecialForceTree::Clear()
{
  m_entries.Clear();
}

TacticalSpecialForceUnit* TacticalSpecialForceTree::FirstItem()
{
  return m_entries.First();
}

TacticalSpecialForceUnit* TacticalSpecialForceTree::FindById(uint32_t id) const
{
  return m_entries.FindById(id);
}

uint32_t TacticalSpecialForceTree::AddWithId(TacticalSpecialForceUnit* node)
{
  return m_entries.AddWithId(node);
}

namespace
{

template <class Record, class Tree>
void readRecords(Tree* tree, istream* in)
{
  tree->Clear();
  int count = 0;
  ReadValue32(in, &count);
  while (count != 0)
  {
    --count;
    void* memory = ::operator new(sizeof(Record));
    Record* record = memory ? new (memory) Record() : nullptr;
    if (record)
    {
      record->Read(in);
      tree->AddWithId(record);
    }
  }
}

template <class Record, class Tree>
void writeRecords(Tree* tree, ostream* out)
{
  int count = tree->CountItems();
  WriteValue32(out, &count);
  for (const auto& entry : tree->Entries())
  {
    entry.second->Write(out);
  }
}

template <class Record, class Tree>
void checksumRecords(Tree* tree, Crc32* crc)
{
  crc->AddInt(tree->CountItems());
  for (const auto& entry : tree->Entries())
  {
    entry.second->Checksum(crc);
  }
}

} // namespace

void TacticalCharacterTree::Read(istream* in)
{
  readRecords<TacticalCharacterUnit>(this, in);
}

void TacticalCharacterTree::Write(ostream* out)
{
  writeRecords<TacticalCharacterUnit>(this, out);
}

void TacticalCharacterTree::Checksum(Crc32* crc)
{
  checksumRecords<TacticalCharacterUnit>(this, crc);
}

void TacticalSpecialForceTree::Read(istream* in)
{
  readRecords<TacticalSpecialForceUnit>(this, in);
}

void TacticalSpecialForceTree::Write(ostream* out)
{
  writeRecords<TacticalSpecialForceUnit>(this, out);
}

void TacticalSpecialForceTree::Checksum(Crc32* crc)
{
  checksumRecords<TacticalSpecialForceUnit>(this, crc);
}

void TacticalCharacterStatusEvent(TacticalEvent* event)
{
  if (event == nullptr)
    return;
  int* args = event->m_args + 2;
  TacticalCharacterUnit* character = FUN_005a9430(GetObjectManager(), args[0]);
  if (character == nullptr)
    return;
  character->m_shipHandle = args[4];
  if (args[6] != 0)
  {
    character->m_injured = 2;
    return;
  }
  character->m_injured = args[7] != 0 ? 1 : 0;
}

int TacticalCharacterUnit::Virtual01()
{
  return 7;
}

TurnSyncChannelA::~TurnSyncChannelA() = default;

TurnSyncChannelB::~TurnSyncChannelB() = default;
