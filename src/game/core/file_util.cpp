#include "core/file_util.h"

#include "platform/win32.h"

#include "core/game_string.h"
#include "iostream/iostream.h"

extern int g_fileError;
extern HMODULE g_resourceModule;
extern char g_resourceString[256];
extern int g_filebufOpenprot;

#pragma clang diagnostic ignored "-Winconsistent-dllimport"
extern "C" char* strncpy(char* destination, const char* source, size_t count);

namespace
{

constexpr int kOpenBinaryInput = 0xa1;
constexpr int kOpenBinaryOutput = 0x82;
constexpr int kInvalidDescriptor = -1;

template <typename Stream>
int FilebufDescriptor(Stream* stream)
{
  const auto* buffer = static_cast<const filebuf*>(iostream_access::IosOf(stream)->m_bp);
  return buffer ? buffer->m_fd : kInvalidDescriptor;
}

} // namespace

void SetResourceModule(void* module)
{
  if (!module)
    module = GetModuleHandleA(nullptr);
  g_resourceModule = static_cast<HMODULE>(module);
  g_fileError = kFileOk;
}

int GetFileError()
{
  return g_fileError;
}

char* LoadRcdataString(unsigned id)
{
  HRSRC resource = FindResourceA(g_resourceModule, MAKEINTRESOURCEA(id & 0xffff), RT_RCDATA);
  if (resource)
  {
    HGLOBAL loaded = LoadResource(g_resourceModule, resource);
    const char* text = static_cast<const char*>(LockResource(loaded));
    strncpy(g_resourceString, text, 0xff);
    FreeResource(loaded);
    g_resourceString[0xff] = 0;
    return g_resourceString;
  }
  g_fileError = kFileResourceMissing;
  g_resourceString[0] = 0;
  return g_resourceString;
}

void OpenInputFile(ifstream* stream, const char* fileName)
{
  g_fileError = kFileOk;
  std::string name = StringFromText(fileName);
  if (name.empty())
  {
    g_fileError = kFileNoName;
  }
  else
  {
    ifstream_open(stream, name.c_str(), kOpenBinaryInput, g_filebufOpenprot);
    if (FilebufDescriptor(stream) == kInvalidDescriptor)
      g_fileError = kFileOpenFailed;
  }
}

void OpenOutputFile(ofstream* stream, const char* fileName)
{
  g_fileError = kFileOk;
  std::string name = StringFromText(fileName);
  if (name.empty())
  {
    g_fileError = kFileNoName;
  }
  else
  {
    ofstream_open(stream, name.c_str(), kOpenBinaryOutput, g_filebufOpenprot);
    if (FilebufDescriptor(stream) == kInvalidDescriptor)
      g_fileError = kFileOpenFailed;
  }
}

void CloseFileStream(ifstream* stream)
{
  fstreambase_close(stream->In());
}
void CloseFileStream(ofstream* stream)
{
  fstreambase_close(stream->Out());
}

int DeleteGameFile(const char* fileName)
{
  int deleted = 0;
  std::string name = StringFromText(fileName);
  if (!name.empty())
    deleted = DeleteFileA(name.c_str());
  return deleted;
}

HMODULE g_resourceModule{};
char g_resourceString[256]{};
int g_fileError = 8;
