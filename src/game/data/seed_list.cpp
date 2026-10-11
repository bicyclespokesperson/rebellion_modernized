#include "data/seed_list.h"

#include "core/checked_new.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "data/int_pair_node.h"

extern int g_seedListClassRegistered;
const char* GetGameDataDirectory();

namespace
{

DatRecord* asRecord(AvlNode* node)
{
  return static_cast<DatRecord*>(static_cast<IdNode*>(node));
}

DatRecord* createRecord(const RecordFamilyRange& range)
{
  return static_cast<DatRecord*>(CreateRecordForKey(range.m_low, range.m_high));
}

} // namespace

// ---- SeedList ----------------------------------------------------------------------------------------------

SeedList::SeedList() : m_range{0, 0}
{
}

SeedList::~SeedList() = default;

DatRecord* SeedList::FindRecord(uint32_t id) const
{
  return m_records.FindById(id);
}

uint32_t SeedList::InsertRecord(DatRecord* record)
{
  return m_records.AddWithId(record);
}

void SeedList::AddRecord(DatRecord* record)
{
  if (record)
    InsertRecord(record);
}

int SeedList::Read(istream* in)
{
  m_records.Clear();
  int ok = ReadVersion(in);
  if (!ok || (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  uint32_t count = 0;
  ReadValue32(in, &count);
  ReadValue32(in, &m_range.m_low);
  ReadValue32(in, &m_range.m_high);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return 0;
  for (uint32_t index = 0; index < count; ++index)
  {
    DatRecord* record = createRecord(m_range);
    if (!record)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
    ok = record->Read(in);
    if (!ok || (iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || InsertRecord(record) == 0)
    {
      delete record;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return 0;
    }
  }
  return ok;
}

int SeedList::Write(ostream* out)
{
  int ok = WriteVersion(out);
  if (!ok)
    return 0;
  if (!WriteCount32(out, m_records.Count()))
    return 0;
  uint32_t low = m_range.m_low;
  WriteValue32(out, &low);
  uint32_t high = m_range.m_high;
  WriteValue32(out, &high);
  for (const auto& entry : Records())
  {
    auto* node = entry.get();
    ok = asRecord(node)->Write(out);
    if (!ok)
      break;
  }
  return ok;
}

int SeedList::Checksum(Crc32* crc)
{
  int ok = ChecksumVersion(crc);
  if (!ok)
    return ok;
  crc->AddInt(static_cast<int32_t>(m_records.Count()));
  crc->AddInt(m_range.m_low);
  crc->AddInt(m_range.m_high);
  for (const auto& entry : Records())
  {
    auto* node = entry.get();
    ok = asRecord(node)->Checksum(crc);
    if (!ok)
      break;
  }
  return ok;
}

int SeedList::ParseText(istream* in)
{
  ScopedStrstream token;
  int endOfLine = 0;
  RecordFamilyRange recordKey = {0, 0};
  uint16_t textGroup = 0;
  // The original first calls 0x51ebb0, a stub that always returns 1.
  const int ok = 1;

  ReadCsvField(in, token.get(), &endOfLine);
  ParseUnsignedField(in, token.get(), &endOfLine, &m_range.m_low);
  ParseUnsignedField(in, token.get(), &endOfLine, &m_range.m_high);
  SkipRestOfLine(in, &endOfLine);

  ReadCsvField(in, token.get(), &endOfLine);
  ParseUnsignedField(in, token.get(), &endOfLine, &recordKey.m_low);
  ParseUnsignedField(in, token.get(), &endOfLine, &recordKey.m_high);
  SkipRestOfLine(in, &endOfLine);

  ReadCsvField(in, token.get(), &endOfLine);
  ParseUnsignedShortField(in, token.get(), &endOfLine, &textGroup);
  SkipRestOfLine(in, &endOfLine);

  SkipTableHeader(in);
  if (iostream_access::State(in) != 0)
    return ok;
  do
  {
    DatRecord* record = createRecord(m_range);
    if (record)
    {
      record->m_first = recordKey.m_low;
      record->m_second = recordKey.m_high;
      record->m_small2 = textGroup;
      if (record->Parse(in))
        AddRecord(record);
      else
        delete record;
    }
  } while (iostream_access::State(in) == 0);
  return ok;
}

void* CreateSeedList()
{
  return NewOrNull<SeedList>();
}

void InitSeedListClassRegistration()
{
  g_seedListClassRegistered = RegisterTableClass("SeedList", CreateSeedList);
}

// ---- SeedListFile ------------------------------------------------------------------------------------------

SeedListFile::SeedListFile()
{
  m_version = m_list.m_version;
}

SeedListFile::~SeedListFile() = default;

int SeedListFile::Load(uint16_t firstId, uint16_t secondId)
{
  return m_list.Open(firstId, secondId, GetGameDataDirectory());
}

// ---- SeedListFileTree --------------------------------------------------------------------------------------

SeedListFileTree::SeedListFileTree() : m_ready(1)
{
}

SeedListFileTree::~SeedListFileTree() = default;

SeedListFile* SeedListFileTree::FindByFamily(uint32_t family)
{
  for (const auto& entry : Entries())
  {
    auto* node = entry.get();
    SeedListFile* file = static_cast<SeedListFile*>(static_cast<IdNode*>(node));
    if (family >= file->m_list.m_range.m_low && family < file->m_list.m_range.m_high)
      return file;
  }
  return nullptr;
}

// ---- RecordNameNode ----------------------------------------------------------------------------------------

RecordNameNode::RecordNameNode() : m_key(0)
{
  m_ready = 1;
}

RecordNameNode::~RecordNameNode() = default;

int RecordNameNode::SetFromRecord(DatRecord* record)
{
  m_key = (record->m_third << 24) | (record->m_id & 0xffffff);
  const ResourceStringId nameId = {static_cast<int16_t>(record->m_small), static_cast<int16_t>(record->m_small2)};
  const std::string name = LoadStringResource(&nameId);
  (m_name = name);
  return 1;
}

// ---- SeedKeyCountNode --------------------------------------------------------------------------------------

SeedKeyCountNode::SeedKeyCountNode() : m_count(0)
{
}

SeedKeyCountNode::SeedKeyCountNode(const SeedKey& key) : m_key(key), m_count(0)
{
}

SeedKeyCountNode::SeedKeyCountNode(const SeedKeyCountNode& other) : m_key(other.m_key), m_count(other.m_count)
{
}

SeedKeyCountNode::~SeedKeyCountNode() = default;

SeedKey* SeedKeyCountNode::GetKey(SeedKey* out) const
{
  new (out) SeedKey(m_key);
  return out;
}

SeedKeyCountNode* SeedKeyCountNode::Clone()
{
  return NewOrNull<SeedKeyCountNode>(*this);
}

void SeedKeyCountNode::Load(istream* in)
{
  Read(in);
}

void SeedKeyCountNode::Save(ostream* out)
{
  Write(out);
}

void SeedKeyCountNode::Read(istream* in)
{
  if (m_key.ReadValue(in))
    StreamRead32(in, &m_count);
}

void SeedKeyCountNode::Write(ostream* out)
{
  if (m_key.WriteValue(out))
    StreamWrite32(out, &m_count);
}

int SeedKeyCountNode::Checksum(Crc32* crc)
{
  const int ok = m_key.AddToChecksum(crc);
  if (ok)
    crc->AddInt(m_count);
  return ok;
}

int g_seedListClassRegistered{};
