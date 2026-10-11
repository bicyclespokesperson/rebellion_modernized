#include "path.h"

#include <sys/stat.h>

#if !defined(_WIN32)
#include <dirent.h>
#endif

namespace mw::res
{

bool FileExists(const std::string& p)
{
  struct stat st;
  return stat(p.c_str(), &st) == 0;
}

std::string JoinPath(const std::string& dir, const std::string& name)
{
  if (dir.empty())
    return name;
  char last = dir.back();
  if (last == '/' || last == '\\')
    return dir + name;
#if defined(_WIN32)
  return dir + "\\" + name;
#else
  return dir + "/" + name;
#endif
}

#if defined(_WIN32)
std::string ResolvePath(const std::string& windowsPath)
{
  return windowsPath;
}
#else
namespace
{

char Lower(char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
}

bool EqualNoCase(const char* a, const std::string& b)
{
  size_t i = 0;
  for (; a[i] && i < b.size(); ++i)
    if (Lower(a[i]) != Lower(b[i]))
      return false;
  return !a[i] && i == b.size();
}

bool FindInDirectory(const std::string& dir, const std::string& name, std::string* actual)
{
  DIR* d = opendir(dir.empty() ? "." : dir.c_str());
  if (!d)
    return false;
  bool found = false;
  while (dirent* e = readdir(d))
  {
    if (EqualNoCase(e->d_name, name))
    {
      *actual = e->d_name;
      found = true;
      break;
    }
  }
  closedir(d);
  return found;
}

} // namespace

std::string ResolvePath(const std::string& windowsPath)
{
  std::string p = windowsPath;
  for (char& c : p)
    if (c == '\\')
      c = '/';
  if (p.size() >= 2 && p[1] == ':' && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')))
    p.erase(0, 2);
  std::string result;
  size_t pos = 0;
  if (!p.empty() && p[0] == '/')
  {
    result = "/";
    pos = 1;
  }
  bool translating = true;
  while (pos <= p.size())
  {
    size_t next = p.find('/', pos);
    if (next == std::string::npos)
      next = p.size();
    std::string part = p.substr(pos, next - pos);
    pos = next + 1;
    if (part.empty())
      continue;
    std::string candidate = result + part;
    if (translating && part != "." && part != ".." && !FileExists(candidate))
    {
      std::string actual;
      if (FindInDirectory(result, part, &actual))
        candidate = result + actual;
      else
        translating = false;
    }
    result = candidate;
    if (next < p.size())
      result += "/";
  }
  return result;
}
#endif

} // namespace mw::res
