#include "core/reb_abi.h"
#include "app/app_cdcheck_version.h"
#include "app/app_strings.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#include <winver.h>

extern "C" {
__declspec(dllimport) DWORD REB_STDCALL GetFileVersionInfoSizeA(LPCSTR fileName, LPDWORD handle);
__declspec(dllimport) BOOL REB_STDCALL GetFileVersionInfoA(LPCSTR fileName, DWORD handle, DWORD length, LPVOID data);
__declspec(dllimport) DWORD REB_STDCALL VerLanguageNameA(DWORD language, LPSTR buffer, DWORD size);
__declspec(dllimport) BOOL REB_STDCALL VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length);
}

extern char g_languageName[0x100];       // 0x6b06f8
extern char g_versionOlderText[0x200];   // 0x6b07f8
extern char g_versionNewerText[0x200];   // 0x6b09f8
extern char g_cdErrorTitle[0x200];       // 0x6b0bf8
extern int g_versionCheckResult;         // 0x6b0df8
extern char g_versionErrorTitle[0x200];  // 0x6b0e00
extern char g_cdErrorText[0x200];        // 0x6b1000

namespace {

constexpr UINT kErrorBoxStyle = MB_ICONHAND | MB_SETFOREGROUND | MB_TOPMOST;
constexpr unsigned char kTextXorKey = 0xdd;

struct ErrorTexts {
    char* cdErrorTitle;
    char* cdErrorText;
    char* versionErrorTitle;
    char* versionOlderText;
    char* versionNewerText;
};

void UseBuiltInTexts() {
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
void DecodeTexts(char* text, int size, ErrorTexts* out) {
    const bool encrypted = text[0] != '#';
    unsigned quotes = 0;
    int index = 0;
    char* cursor = text;
    // BUG(original): the first pointer starts out as the 0/1 'encrypted' flag, and the others are uninitialised, so
    // a file with fewer than five strings makes sprintf read through a bad pointer.
    *out = ErrorTexts{};
    out->cdErrorTitle = reinterpret_cast<char*>(static_cast<uintptr_t>(encrypted));
    while (index < size) {
        char* next = cursor + 1;
        if (encrypted) text[index] ^= kTextXorKey;
        if (text[index] == '"') {
            ++quotes;
            if (quotes == 1) out->cdErrorTitle = next;
            if (quotes == 3) out->cdErrorText = next;
            if (quotes == 5) out->versionErrorTitle = next;
            if (quotes == 7) out->versionOlderText = next;
            if (quotes == 9) out->versionNewerText = next;
            if ((quotes & 1) == 0) text[index] = '\0';
        }
        if (text[index] == '\\') {
            text[index] = ' ';
            *next = '\n';
            ++index;
            next = cursor + 2;
        }
        ++index;
        cursor = next;
    }
}

}  // namespace

void InitCdAndVersionCheck() {
    FILE* file = fopen(g_textsFileName, "rb");
    if (!file) {
        UseBuiltInTexts();
        g_versionCheckResult = 1;
        g_versionCheckResult = CheckTextstraVersion();
        return;
    }
    fseek(file, 0, SEEK_END);
    const int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* text = static_cast<char*>(malloc(size));
    fread(text, size, 1, file);
    fclose(file);

    ErrorTexts texts;
    DecodeTexts(text, size, &texts);
    sprintf(g_cdErrorText, texts.cdErrorText);
    sprintf(g_cdErrorTitle, texts.cdErrorTitle);
    sprintf(g_versionOlderText, texts.versionOlderText);
    sprintf(g_versionNewerText, texts.versionNewerText);
    sprintf(g_versionErrorTitle, texts.versionErrorTitle);
    free(text);
    g_versionCheckResult = CheckTextstraVersion();
}

int GetVersionCheckResult() { return g_versionCheckResult; }

void ShowCdNotFoundError() { MessageBoxA(nullptr, g_cdErrorText, g_cdErrorTitle, kErrorBoxStyle); }

void ShowGameVersionOlderError() { MessageBoxA(GetActiveWindow(), g_versionOlderText, g_versionErrorTitle, kErrorBoxStyle); }

void ShowOpponentVersionOlderError() {
    MessageBoxA(GetActiveWindow(), g_versionNewerText, g_versionErrorTitle, kErrorBoxStyle);
}

const char* GetLanguageName() { return g_languageName; }

int CheckTextstraVersion() {
    bool compatible = true;
    DWORD handle;
    const DWORD size = GetFileVersionInfoSizeA(g_textstraDllName, &handle);
    if (size != 0) {
        void* data = operator new(size);
        if (GetFileVersionInfoA(g_textstraDllName, 0, size, data)) {
            WORD* translation;
            UINT length;
            if (VerQueryValueA(data, g_translationQuery, reinterpret_cast<LPVOID*>(&translation), &length)) {
                VerLanguageNameA(translation[0], g_languageName, 0xff);
                char subBlock[256];
                wsprintfA(subBlock, g_fileVersionQueryFormat, translation[0], translation[1]);
                char* version;
                if (VerQueryValueA(data, subBlock, reinterpret_cast<LPVOID*>(&version), &length)) {
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
