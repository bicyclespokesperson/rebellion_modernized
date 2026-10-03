#pragma once
#include <cstdint>

class istream;
class ostream;
class ifstream;
class ofstream;

// Statically linked iostream entry points (LIBCI), called by address.
istream* __thiscall istream_read(istream* in, void* buffer, int count);
ostream* __thiscall ostream_write(ostream* out, const void* buffer, int count);
long __thiscall istream_tellg(istream* in);
long __thiscall ostream_tellp(ostream* out);

// Raw binary I/O of 1, 2 and 4 bytes. Every persistent object in the game saves and loads itself with these.
void StreamRead8(istream* in, void* value);
void StreamWrite8(ostream* out, const void* value);
void StreamRead16(istream* in, void* value);
void StreamWrite16(ostream* out, const void* value);
void StreamRead32(istream* in, void* value);
void StreamWrite32(ostream* out, const void* value);

// The same operations under the names the game's field-by-field Read/Write code calls (one per field width;
// the original compiler instantiated a template per type and left one thin wrapper per width).
void ReadValue8(istream* in, void* value);
void WriteValue8(ostream* out, const void* value);
void ReadValue16(istream* in, void* value);
void WriteValue16(ostream* out, const void* value);
void ReadValue32(istream* in, void* value);
void WriteValue32(ostream* out, const void* value);

class String;
void ReadString(istream* in, String* value);
void WriteString(ostream* out, String* value);
void ReadValueString(istream* in, String* value);
void WriteValueString(ostream* out, String* value);

// Save files interleave position markers with the data: the writer stores its own stream position, the reader
// checks that the stored position matches where it is, to detect a desynchronised file.
int CheckPositionMarker(istream* in);
void WritePositionMarker(ostream* out);
