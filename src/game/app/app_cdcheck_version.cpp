#include "app/app_cdcheck_version.h"
#include "app/app_strings.h"
#include "core/runtime_environment.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#include "platform/win32.h"

extern char g_languageName[0x100]; // 0x6b06f8
extern char g_versionOlderText[0x200]; // 0x6b07f8
extern char g_versionNewerText[0x200]; // 0x6b09f8
extern char g_cdErrorTitle[0x200]; // 0x6b0bf8
extern int g_versionCheckResult; // 0x6b0df8
extern char g_versionErrorTitle[0x200]; // 0x6b0e00
extern char g_cdErrorText[0x200]; // 0x6b1000

namespace
{

constexpr UINT kErrorBoxStyle = MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST;
constexpr unsigned char kTextXorKey = 0xdd;

struct ErrorTexts
{
  char* cdErrorTitle;
  char* cdErrorText;
  char* versionErrorTitle;
  char* versionOlderText;
  char* versionNewerText;
};

void UseBuiltInTexts()
{
  sprintf(g_cdErrorTitle, "Error: CD Not Found");
  sprintf(g_cdErrorText,
          "The CD-ROM could not be found, please check the drive.\nIf the problem persists, please reinstall from "
          "the original CD.");
  sprintf(g_versionErrorTitle, "Error: Versions Incompatible");
  sprintf(g_versionOlderText,
          "Your version of the game is older than your opponent's version. Upgrade information is available at "
          "http://www.lucasarts.com/products/rebellion.");
  sprintf(g_versionNewerText,
          "Your opponent's version of the game is older than your version. Upgrade information is available at "
          "http://www.lucasarts.com/products/rebellion.");
}

// Decodes the file in place and returns where its quoted strings start. Every second quote ends a string (and is
// overwritten with a NUL); a backslash becomes a space followed by a newline.
bool DecodeTexts(char* text, size_t size, ErrorTexts* out)
{
  if (!text || size == 0 || !out)
    return false;
  const bool encrypted = text[0] != '#';
  unsigned quotes = 0;
  *out = ErrorTexts{};
  for (size_t index = 0; index < size; ++index)
  {
    if (encrypted)
      text[index] ^= kTextXorKey;
    if (text[index] == '"')
    {
      ++quotes;
      if (quotes == 1)
        out->cdErrorTitle = text + index + 1;
      if (quotes == 3)
        out->cdErrorText = text + index + 1;
      if (quotes == 5)
        out->versionErrorTitle = text + index + 1;
      if (quotes == 7)
        out->versionOlderText = text + index + 1;
      if (quotes == 9)
        out->versionNewerText = text + index + 1;
      if ((quotes & 1) == 0)
        text[index] = '\0';
    }
    if (text[index] == '\\')
    {
      text[index] = ' ';
      if (index + 1 < size)
      {
        text[index + 1] = '\n';
        ++index;
      }
    }
  }
  return out->cdErrorTitle && out->cdErrorText && out->versionErrorTitle && out->versionOlderText &&
         out->versionNewerText && (quotes >= 10) && (quotes % 2 == 0);
}

} // namespace

void InitCdAndVersionCheck()
{
  FILE* file = OpenGameFile(g_textsFileName, "rb");
  if (!file)
  {
    UseBuiltInTexts();
    g_versionCheckResult = 1;
    g_versionCheckResult = CheckTextstraVersion();
    return;
  }
  if (fseek(file, 0, SEEK_END) != 0)
  {
    fclose(file);
    UseBuiltInTexts();
    g_versionCheckResult = CheckTextstraVersion();
    return;
  }
  const long fileSize = ftell(file);
  if (fileSize <= 0 || static_cast<unsigned long>(fileSize) > 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0)
  {
    fclose(file);
    UseBuiltInTexts();
    g_versionCheckResult = CheckTextstraVersion();
    return;
  }
  const auto size = static_cast<size_t>(fileSize);
  char* text = static_cast<char*>(malloc(size + 1));
  if (!text)
  {
    fclose(file);
    UseBuiltInTexts();
    g_versionCheckResult = CheckTextstraVersion();
    return;
  }
  const size_t bytesRead = fread(text, 1, size, file);
  fclose(file);
  text[size] = '\0';

  ErrorTexts texts;
  if (bytesRead != size || !DecodeTexts(text, size, &texts))
  {
    free(text);
    UseBuiltInTexts();
    g_versionCheckResult = CheckTextstraVersion();
    return;
  }
  std::snprintf(g_cdErrorText, sizeof(g_cdErrorText), "%s", texts.cdErrorText);
  std::snprintf(g_cdErrorTitle, sizeof(g_cdErrorTitle), "%s", texts.cdErrorTitle);
  std::snprintf(g_versionOlderText, sizeof(g_versionOlderText), "%s", texts.versionOlderText);
  std::snprintf(g_versionNewerText, sizeof(g_versionNewerText), "%s", texts.versionNewerText);
  std::snprintf(g_versionErrorTitle, sizeof(g_versionErrorTitle), "%s", texts.versionErrorTitle);
  free(text);
  g_versionCheckResult = CheckTextstraVersion();
}

int GetVersionCheckResult()
{
  return g_versionCheckResult;
}

void ShowCdNotFoundError()
{
  MessageBoxA(nullptr, g_cdErrorText, g_cdErrorTitle, kErrorBoxStyle);
}

void ShowGameVersionOlderError()
{
  MessageBoxA(GetActiveWindow(), g_versionOlderText, g_versionErrorTitle, kErrorBoxStyle);
}

void ShowOpponentVersionOlderError()
{
  MessageBoxA(GetActiveWindow(), g_versionNewerText, g_versionErrorTitle, kErrorBoxStyle);
}

const char* GetLanguageName()
{
  return g_languageName;
}

int CheckTextstraVersion()
{
  bool compatible = true;
  DWORD handle;
  const DWORD size = GetFileVersionInfoSizeA(g_textstraDllName, &handle);
  if (size != 0)
  {
    void* data = operator new(size);
    if (GetFileVersionInfoA(g_textstraDllName, 0, size, data))
    {
      WORD* translation;
      UINT length;
      if (VerQueryValueA(data, g_translationQuery, reinterpret_cast<LPVOID*>(&translation), &length))
      {
        VerLanguageNameA(translation[0], g_languageName, 0xff);
        char subBlock[256];
        wsprintfA(subBlock, g_fileVersionQueryFormat, translation[0], translation[1]);
        char* version;
        if (VerQueryValueA(data, subBlock, reinterpret_cast<LPVOID*>(&version), &length))
        {
          version[1] = '\0';
          version[4] = '\0';
          const int major = atoi(version + 2);
          const int minor = atoi(version + 5);
          compatible = minor == 0 && major == 0;
        }
      }
    }
    operator delete(data);
  }
  return compatible;
}

char g_cdErrorText[512]{};
char g_cdErrorTitle[512]{};
char g_languageName[256]{};
char g_versionErrorTitle[512]{};
char g_versionNewerText[512]{};
char g_versionOlderText[512]{};
const char* g_appClassName = nullptr;
const char* g_commonDllName = nullptr;
const char* g_displayOptsName = nullptr;
const char* g_fileVersionQueryFormat = nullptr;
const char* g_installedLocationName = nullptr;
const char* g_introMusicFailedText = nullptr;
const char* g_mismatchOptsName = nullptr;
const char* g_rebdlogDllName = nullptr;
const char* g_registryKeyPath = nullptr;
const char* g_soundManagerCaption = nullptr;
const char* g_textcommDllName = nullptr;
const char* g_textsFileName = nullptr;
const char* g_textstraDllName = nullptr;
const char* g_tooBigLocationName = nullptr;
const char* g_translationQuery = nullptr;
const char* g_volumeName = nullptr;
int g_versionCheckResult{};
