#include "port/native/save_transaction.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <string>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "core/iostream_lib.h"
#include "port/minwin_impl/res/path.h"

#if defined(_WIN32)
// windows.h conflicts with the minwin type shims pulled in above, so declare the one Win32 call needed.
extern "C" __declspec(dllimport) int __stdcall MoveFileExA(const char* from, const char* to, unsigned long flags);
constexpr unsigned long kMoveReplaceExisting = 0x1;
constexpr unsigned long kMoveWriteThrough = 0x8;
#endif

namespace reb::native
{
namespace
{

class TemporarySave
{
  public:
  explicit TemporarySave(const std::string& path)
      : path_(path)
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
  if (!destination || !*destination) {
    return false;
  }
  try {
    const std::string target = mw::res::ResolvePath(destination);
    std::string temporary;
    int descriptor = -1;
#if defined(_WIN32)
    static std::atomic<unsigned> counter{0};
    temporary = target + ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "."
                + std::to_string(counter.fetch_add(1));
    {
      std::ofstream created(temporary, std::ios::binary);
      if (!created) {
        return false;
      }
    }
#else
    temporary = target + ".tmp.XXXXXX";
    descriptor = mkstemp(temporary.data());
    if (descriptor == -1) {
      return false;
    }
#endif
    TemporarySave cleanup(temporary);
#if !defined(_WIN32)
    if (close(descriptor) != 0) {
      return false;
    }
#endif

    {
      ScopedOfstream output;
      ofstream_open(output.get(), temporary.c_str(), 0x82, 0x180);
      if (iostream_access::State(output.get()) != 0) {
        return false;
      }
      const bool written = write(output.get()->Out());
      const int writeState = iostream_access::State(output.get());
      fstreambase_close(output.get()->Out());
      if (!written || writeState != 0 || iostream_access::State(output.get()) != 0) {
        return false;
      }
    }

#if defined(_WIN32)
    return MoveFileExA(temporary.c_str(), target.c_str(), kMoveReplaceExisting | kMoveWriteThrough) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    return !error;
#endif
  } catch (...) {
    return false;
  }
}

} // namespace reb::native
