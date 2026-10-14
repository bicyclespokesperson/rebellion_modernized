#include "pe_module.h"

#include <cstring>

#include "mapped_file.h"

namespace mw::res
{

namespace
{

uint16_t Rd16(const uint8_t* p)
{
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t Rd32(const uint8_t* p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

char Lower(char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

bool EqualNoCase(const std::string& a, const std::string& b)
{
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (Lower(a[i]) != Lower(b[i]))
      return false;
  return true;
}

constexpr uint32_t kMaxDirEntries = 70000; // STRATEGY.DLL has ~1,000 bitmaps; a bound against corrupt files

} // namespace

ResId ResId::FromPointer(const char* p)
{
  auto v = reinterpret_cast<uintptr_t>(p);
  if (v < 0x10000)
    return Int(static_cast<uint16_t>(v));
  if (p[0] == '#')
  {
    int n = 0;
    for (const char* q = p + 1; *q >= '0' && *q <= '9'; ++q)
      n = n * 10 + (*q - '0');
    return Int(static_cast<uint16_t>(n));
  }
  return Str(p);
}

Module::~Module()
{
  delete m_file;
}

Module* Module::Open(const std::string& nativePath)
{
  MappedFile* file = MappedFile::Open(nativePath);
  if (!file)
    return nullptr;
  auto* m = new Module;
  m->m_file = file;
  m->m_base = file->data();
  m->m_size = file->size();
  m->m_path = nativePath;
  if (!m->Parse())
  {
    delete m;
    return nullptr;
  }
  return m;
}

Module* Module::FromMemory(const uint8_t* data, size_t size)
{
  auto* m = new Module;
  m->m_base = data;
  m->m_size = size;
  if (!m->Parse())
  {
    delete m;
    return nullptr;
  }
  return m;
}

bool Module::Parse()
{
  if (m_size < 0x40 || m_base[0] != 'M' || m_base[1] != 'Z')
    return false;
  uint32_t pe = Rd32(m_base + 0x3c);
  if (pe + 0x18 > m_size || memcmp(m_base + pe, "PE\0\0", 4) != 0)
    return false;
  uint16_t sectionCount = Rd16(m_base + pe + 6);
  uint16_t optionalSize = Rd16(m_base + pe + 20);
  const uint8_t* opt = m_base + pe + 24;
  if (pe + 24 + optionalSize > m_size)
    return false;
  uint16_t magic = Rd16(opt);
  m_is64 = magic == 0x20b;
  if (magic != 0x10b && magic != 0x20b)
    return false;
  uint32_t dirCountOffset = m_is64 ? 108 : 92;
  uint32_t dirBase = m_is64 ? 112 : 96;
  if (optionalSize >= dirBase + 8 * 3 && Rd32(opt + dirCountOffset) > 2)
  {
    m_resRva = Rd32(opt + dirBase + 16);
    m_resSize = Rd32(opt + dirBase + 20);
  }
  const uint8_t* sec = opt + optionalSize;
  if (static_cast<size_t>(sec - m_base) + sectionCount * 40u > m_size)
    return false;
  for (int i = 0; i < sectionCount; ++i, sec += 40)
    m_sections.push_back({Rd32(sec + 12), Rd32(sec + 8), Rd32(sec + 20), Rd32(sec + 16)});
  if (m_resRva == 0)
    return true; // a PE without resources: every Find fails
  m_resOffset = RvaToOffset(m_resRva, 16);
  return m_resOffset != 0;
}

uint32_t Module::RvaToOffset(uint32_t rva, uint32_t size) const
{
  for (const Section& s : m_sections)
  {
    uint32_t extent = s.vsize > s.rawSize ? s.vsize : s.rawSize;
    if (rva >= s.rva && rva - s.rva < extent)
    {
      uint64_t off = static_cast<uint64_t>(s.rawOffset) + (rva - s.rva);
      if (off + size > m_size)
        return 0;
      return static_cast<uint32_t>(off);
    }
  }
  return 0;
}

bool Module::ReadDir(uint32_t offset, std::vector<DirEntry>* out) const
{
  out->clear();
  if (offset + 16 > m_resSize)
    return false;
  const uint8_t* d = m_base + m_resOffset + offset;
  uint32_t named = Rd16(d + 12), ids = Rd16(d + 14);
  if (named + ids > kMaxDirEntries || offset + 16 + (named + ids) * 8 > m_resSize)
    return false;
  const uint8_t* e = d + 16;
  for (uint32_t i = 0; i < named + ids; ++i, e += 8)
  {
    uint32_t nameField = Rd32(e), target = Rd32(e + 4);
    DirEntry entry;
    entry.named = (nameField & 0x80000000u) != 0;
    entry.id = entry.named ? (nameField & 0x7fffffffu) : (nameField & 0xffff);
    entry.isDir = (target & 0x80000000u) != 0;
    entry.offset = target & 0x7fffffffu;
    out->push_back(entry);
  }
  return true;
}

std::string Module::NameOf(const DirEntry& e) const
{
  std::string s;
  if (!e.named || e.id + 2 > m_resSize)
    return s;
  const uint8_t* p = m_base + m_resOffset + e.id;
  uint32_t n = Rd16(p);
  if (e.id + 2 + n * 2 > m_resSize)
    return s;
  for (uint32_t i = 0; i < n; ++i)
  {
    uint16_t c = Rd16(p + 2 + i * 2);
    s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
  }
  return s;
}

bool Module::Matches(const DirEntry& e, const ResId& id) const
{
  if (id.isString)
    return e.named && EqualNoCase(NameOf(e), id.name);
  return !e.named && e.id == id.id;
}

const Module::DirEntry* Module::PickLanguage(const std::vector<DirEntry>& langs, uint16_t language) const
{
  if (langs.empty())
    return nullptr;
  for (const DirEntry& e : langs)
    if (!e.named && e.id == language)
      return &e;
  return &langs.front();
}

bool Module::ReadEntry(const DirEntry& e, uint16_t, Entry* out) const
{
  if (e.isDir || e.offset + 16 > m_resSize)
    return false;
  const uint8_t* de = m_base + m_resOffset + e.offset;
  uint32_t rva = Rd32(de), size = Rd32(de + 4);
  uint32_t off = RvaToOffset(rva, size);
  if (off == 0 && size != 0)
    return false;
  out->data = m_base + off;
  out->size = size;
  out->codepage = Rd32(de + 8);
  out->language = e.named ? 0 : static_cast<uint16_t>(e.id);
  return true;
}

bool Module::Find(const ResId& type, const ResId& name, Entry* out, uint16_t language) const
{
  if (m_resOffset == 0)
    return false;
  std::vector<DirEntry> level;
  if (!ReadDir(0, &level))
    return false;
  for (const DirEntry& t : level)
  {
    if (!t.isDir || !Matches(t, type))
      continue;
    std::vector<DirEntry> names;
    if (!ReadDir(t.offset, &names))
      return false;
    for (const DirEntry& n : names)
    {
      if (!n.isDir || !Matches(n, name))
        continue;
      std::vector<DirEntry> langs;
      if (!ReadDir(n.offset, &langs))
        return false;
      const DirEntry* pick = PickLanguage(langs, language);
      if (!pick)
        return false;
      DirEntry chosen = *pick;
      if (chosen.isDir)
      { // tolerate a deeper tree: take the first leaf
        std::vector<DirEntry> deeper;
        if (!ReadDir(chosen.offset, &deeper) || deeper.empty())
          return false;
        chosen = deeper.front();
      }
      return ReadEntry(chosen, language, out);
    }
    return false;
  }
  return false;
}

std::vector<ResId> Module::ListTypes() const
{
  std::vector<ResId> result;
  std::vector<DirEntry> level;
  if (m_resOffset == 0 || !ReadDir(0, &level))
    return result;
  for (const DirEntry& t : level)
    result.push_back(t.named ? ResId::Str(NameOf(t)) : ResId::Int(static_cast<uint16_t>(t.id)));
  return result;
}

std::vector<ResId> Module::ListNames(const ResId& type) const
{
  std::vector<ResId> result;
  std::vector<DirEntry> level;
  if (m_resOffset == 0 || !ReadDir(0, &level))
    return result;
  for (const DirEntry& t : level)
  {
    if (!t.isDir || !Matches(t, type))
      continue;
    std::vector<DirEntry> names;
    if (!ReadDir(t.offset, &names))
      break;
    for (const DirEntry& n : names)
      result.push_back(n.named ? ResId::Str(NameOf(n)) : ResId::Int(static_cast<uint16_t>(n.id)));
    break;
  }
  return result;
}

} // namespace mw::res
