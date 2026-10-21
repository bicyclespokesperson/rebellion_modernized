// minwin_impl/res: a portable PE resource reader. A Module is an original EXE or DLL (REBEXE.EXE, STRATEGY.DLL,
// TEXTCOMM.DLL, ...) mapped read-only from the user's installation; only the resource tree is parsed. No Win32 loader
// is involved, so the same code reads the files on Linux, macOS and Windows, on 32- and 64-bit hosts.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace mw::res
{

// Resource type / name / language identifiers: a 16-bit integer or a (case-insensitive) string.
struct ResId {
  bool isString = false;
  uint16_t id = 0;
  std::string name;
  static ResId Int(uint16_t v)
  {
    ResId r;
    r.id = v;
    return r;
  }
  static ResId Str(const std::string& s)
  {
    ResId r;
    r.isString = true;
    r.name = s;
    return r;
  }
  // The LPCSTR convention of FindResourceA: a pointer value below 0x10000 is an integer id, "#123" also means 123.
  static ResId FromPointer(const char* p);
};

struct Entry {
  const uint8_t* data = nullptr; // points into the mapping; valid until the Module is destroyed
  uint32_t size = 0;
  uint32_t codepage = 0;
  uint16_t language = 0;
};

// Standard resource types (RT_*).
enum : uint16_t {
  kCursor = 1,
  kBitmap = 2,
  kIcon = 3,
  kMenu = 4,
  kDialog = 5,
  kString = 6,
  kFontDir = 7,
  kFont = 8,
  kAccelerator = 9,
  kRcData = 10,
  kGroupCursor = 12,
  kGroupIcon = 14,
  kVersion = 16
};

class MappedFile;

class Module
{
  public:
  ~Module();
  // Maps `nativePath` and parses its PE headers and resource directory. Returns nullptr when the file is missing or is
  // not a PE image.
  static Module* Open(const std::string& nativePath);
  // Parse from memory (tests, embedded images); the buffer must outlive the Module.
  static Module* FromMemory(const uint8_t* data, size_t size);

  // FindResource semantics: language 0 asks for the neutral entry, else the first one (the game's DLLs are
  // single-language).
  bool Find(const ResId& type, const ResId& name, Entry* out, uint16_t language = 0) const;
  std::vector<ResId> ListTypes() const;
  std::vector<ResId> ListNames(const ResId& type) const;

  const std::string& Path() const
  {
    return m_path;
  }
  bool Is64() const
  {
    return m_is64;
  }
  int refs = 1;

  private:
  Module() = default;
  bool Parse();
  struct DirEntry {
    bool named;
    bool isDir;
    uint32_t id;     // integer id, or offset of the name string (relative to the resource section) when named
    uint32_t offset; // subdirectory or data entry, relative to the resource section
  };
  bool ReadDir(uint32_t offset, std::vector<DirEntry>* out) const;
  std::string NameOf(const DirEntry& e) const;
  bool Matches(const DirEntry& e, const ResId& id) const;
  bool ReadEntry(const DirEntry& e, uint16_t language, Entry* out) const;
  const DirEntry* PickLanguage(const std::vector<DirEntry>& langs, uint16_t language) const;
  uint32_t RvaToOffset(uint32_t rva, uint32_t size) const;

  MappedFile* m_file = nullptr;
  const uint8_t* m_base = nullptr;
  size_t m_size = 0;
  std::string m_path;
  bool m_is64 = false;
  uint32_t m_resRva = 0, m_resSize = 0, m_resOffset = 0;
  struct Section {
    uint32_t rva, vsize, rawOffset, rawSize;
  };
  std::vector<Section> m_sections;
};

} // namespace mw::res
