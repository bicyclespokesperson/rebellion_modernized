#pragma once
#include <cstdint>

#include "core/stream_io.h"

// Error code of the last file/resource helper call below (g_fileError).
enum FileError : int {
  kFileOk = 0,
  kFileNoName = 1,
  kFileOpenFailed = 2,
  kFileResourceMissing = 4,
};

// Module whose resources LoadRcdataString reads (the game's own module if `module` is null); clears the error.
void SetResourceModule(void* module);
int GetFileError();

// RT_RCDATA resource `id` (16 bits) as a NUL-terminated string in a shared 256-byte buffer that is overwritten by
// the next call (truncated to 255 characters). Empty string and kFileResourceMissing if absent.
char* LoadRcdataString(unsigned id);

// Opens `fileName` on an ifstream / ofstream (binary, default protection). Sets the file error to kFileNoName
// if there is no name and kFileOpenFailed if the open failed.
void OpenInputFile(ifstream* stream, const char* fileName);
void OpenOutputFile(ofstream* stream, const char* fileName);
void CloseFileStream(ifstream* stream);
void CloseFileStream(ofstream* stream);

// DeleteFile on the name; 0 if there is no name.
int DeleteGameFile(const char* fileName);
