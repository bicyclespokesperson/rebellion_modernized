#include "mapped_file.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>

namespace mw::res
{

#if defined(_WIN32)
MappedFile* MappedFile::Open(const std::string& path)
{
  HANDLE file =
    CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return nullptr;
  LARGE_INTEGER size;
  if (!GetFileSizeEx(file, &size) || size.QuadPart == 0)
  {
    CloseHandle(file);
    return nullptr;
  }
  HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  CloseHandle(file);
  if (!mapping)
    return nullptr;
  void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
  CloseHandle(mapping);
  if (!view)
    return nullptr;
  MappedFile* m = new MappedFile;
  m->m_data = static_cast<const uint8_t*>(view);
  m->m_size = static_cast<size_t>(size.QuadPart);
  return m;
}

MappedFile::~MappedFile()
{
  if (m_data)
    UnmapViewOfFile(m_data);
}
#else
MappedFile* MappedFile::Open(const std::string& path)
{
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
    return nullptr;
  struct stat st;
  if (fstat(fd, &st) != 0 || st.st_size <= 0)
  {
    close(fd);
    return nullptr;
  }
  void* view = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (view == MAP_FAILED)
    return nullptr;
  auto* m = new MappedFile;
  m->m_data = static_cast<const uint8_t*>(view);
  m->m_size = static_cast<size_t>(st.st_size);
  return m;
}

MappedFile::~MappedFile()
{
  if (m_data)
    munmap(const_cast<uint8_t*>(m_data), m_size);
}
#endif

} // namespace mw::res
