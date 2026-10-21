#pragma once
#include <stddef.h>
#include <stdint.h>

#include <string>

namespace mw::res
{

// A read-only memory mapping of a whole file.
class MappedFile
{
  public:
  static MappedFile* Open(const std::string& nativePath);
  ~MappedFile();
  const uint8_t* data() const
  {
    return m_data;
  }
  size_t size() const
  {
    return m_size;
  }

  private:
  MappedFile() = default;
  const uint8_t* m_data = nullptr;
  size_t m_size = 0;
};

} // namespace mw::res
