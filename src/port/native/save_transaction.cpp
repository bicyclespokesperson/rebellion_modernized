#include "port/native/save_transaction.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "core/iostream_lib.h"
#include "port/minwin_impl/res/path.h"

namespace reb::native
{
namespace
{

class TemporarySave
{
public:
  explicit TemporarySave(const std::string& path) : path_(path)
  {
  }
  TemporarySave(const TemporarySave&) = delete;
  TemporarySave& operator=(const TemporarySave&) = delete;
  ~TemporarySave()
  {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

private:
  const std::string& path_;
};

} // namespace

bool WriteSaveAtomically(const char* destination, const std::function<bool(ostream*)>& write)
{
  if (!destination || !*destination)
    return false;
  try
  {
    const std::string target = mw::res::ResolvePath(destination);
    std::string temporary;
    int descriptor = -1;
#if defined(_WIN32)
    temporary = target + ".tmp.XXXXXX";
    if (_mktemp_s(temporary.data(), temporary.size() + 1) != 0 ||
        _sopen_s(&descriptor, temporary.c_str(), _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _SH_DENYNO,
                 _S_IREAD | _S_IWRITE) != 0)
      return false;
#else
    temporary = target + ".tmp.XXXXXX";
    descriptor = mkstemp(temporary.data());
    if (descriptor == -1)
      return false;
#endif
    TemporarySave cleanup(temporary);
#if defined(_WIN32)
    if (_close(descriptor) != 0)
#else
    if (close(descriptor) != 0)
#endif
      return false;

    {
      ScopedOfstream output;
      ofstream_open(output.get(), temporary.c_str(), 0x82, 0x180);
      if (iostream_access::State(output.get()) != 0)
        return false;
      const bool written = write(output.get()->Out());
      const int writeState = iostream_access::State(output.get());
      fstreambase_close(output.get()->Out());
      if (!written || writeState != 0 || iostream_access::State(output.get()) != 0)
        return false;
    }

#if defined(_WIN32)
    return MoveFileExA(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    return !error;
#endif
  }
  catch (...)
  {
    return false;
  }
}

} // namespace reb::native
