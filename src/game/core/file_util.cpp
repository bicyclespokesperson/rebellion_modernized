#include "core/file_util.h"

#include <windows.h>

#include "core/game_string.h"

extern int g_fileError;
extern HMODULE g_resourceModule;
extern char g_resourceString[256];
extern int g_filebufOpenprot;

extern "C" char* __cdecl strncpy(char* destination, const char* source, size_t count);

void __thiscall ifstream_open(ifstream* stream, const char* fileName, int mode, int protection);
void __thiscall ofstream_open(ofstream* stream, const char* fileName, int mode, int protection);
void __thiscall fstreambase_close(void* stream);

namespace {

constexpr int kOpenBinaryInput = 0xa1;
constexpr int kOpenBinaryOutput = 0x82;
constexpr int kInvalidDescriptor = -1;

// A stream derived from ios (virtual base): vbtable[1] is the offset of the ios part, whose streambuf pointer
// (+4) is the filebuf holding the file descriptor (+0x4c).
int FilebufDescriptor(void* fstream) {
    char* object = static_cast<char*>(fstream);
    const int* vbtable = *reinterpret_cast<int**>(object);
    char* ios = object + vbtable[1];
    char* filebuf = *reinterpret_cast<char**>(ios + 4);
    return *reinterpret_cast<int*>(filebuf + 0x4c);
}

}  // namespace

void SetResourceModule(void* module) {
    if (!module) module = GetModuleHandleA(nullptr);
    g_resourceModule = static_cast<HMODULE>(module);
    g_fileError = kFileOk;
}

int GetFileError() { return g_fileError; }

char* LoadRcdataString(unsigned id) {
    HRSRC resource = FindResourceA(g_resourceModule, MAKEINTRESOURCEA(id & 0xffff), RT_RCDATA);
    if (resource) {
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

void OpenInputFile(ifstream* stream, const char* fileName) {
    g_fileError = kFileOk;
    String name(fileName);
    if (!name.m_text) {
        g_fileError = kFileNoName;
    } else {
        ifstream_open(stream, name.m_text, kOpenBinaryInput, g_filebufOpenprot);
        if (FilebufDescriptor(stream) == kInvalidDescriptor) g_fileError = kFileOpenFailed;
    }
}

void OpenOutputFile(ofstream* stream, const char* fileName) {
    g_fileError = kFileOk;
    String name(fileName);
    if (!name.m_text) {
        g_fileError = kFileNoName;
    } else {
        ofstream_open(stream, name.m_text, kOpenBinaryOutput, g_filebufOpenprot);
        if (FilebufDescriptor(stream) == kInvalidDescriptor) g_fileError = kFileOpenFailed;
    }
}

void CloseFileStream(void* fstream) { fstreambase_close(fstream); }

int DeleteGameFile(const char* fileName) {
    int deleted = 0;
    String name(fileName);
    if (name.m_text) deleted = DeleteFileA(name.m_text);
    return deleted;
}
