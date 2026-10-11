#include "port/native/save_transaction.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

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
    std::string temporary = target + ".tmp.XXXXXX";
    const int descriptor = mkstemp(temporary.data());
    if (descriptor == -1)
      return false;
    TemporarySave cleanup(temporary);
    if (close(descriptor) != 0)
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

    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    return !error;
  }
  catch (...)
  {
    return false;
  }
}

} // namespace reb::native
