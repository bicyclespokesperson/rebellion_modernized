#include "core/stream_io.h"

#include "core/game_string.h"

// String::ReadFrom / String::WriteTo (String module).
void __thiscall FUN_005f3530(String* text, istream* in);
void __thiscall FUN_005f3560(String* text, ostream* out);

void StreamRead16(istream* in, void* value) { istream_read(in, static_cast<char*>(value), 2); }
void StreamWrite16(ostream* out, const void* value) { ostream_write(out, value, 2); }
void StreamRead8(istream* in, void* value) { istream_read(in, static_cast<char*>(value), 1); }
void StreamWrite8(ostream* out, const void* value) { ostream_write(out, value, 1); }
void StreamRead32(istream* in, void* value) { istream_read(in, static_cast<char*>(value), 4); }
void StreamWrite32(ostream* out, const void* value) { ostream_write(out, value, 4); }

void ReadValue32(istream* in, void* value) { StreamRead32(in, value); }
void WriteValue32(ostream* out, const void* value) { StreamWrite32(out, value); }
void ReadValue16(istream* in, void* value) { StreamRead16(in, value); }
void WriteValue16(ostream* out, const void* value) { StreamWrite16(out, value); }
void ReadValue8(istream* in, void* value) { StreamRead8(in, value); }
void WriteValue8(ostream* out, const void* value) { StreamWrite8(out, value); }
void ReadString(istream* in, String* value) { FUN_005f3530(value, in); }
void WriteString(ostream* out, String* value) { FUN_005f3560(value, out); }
void ReadValueString(istream* in, String* value) { ReadString(in, value); }
void WriteValueString(ostream* out, String* value) { WriteString(out, value); }

int CheckPositionMarker(istream* in) {
    const long position = istream_tellg(in);
    int stored;
    StreamRead32(in, &stored);
    return position == stored;
}

void WritePositionMarker(ostream* out) {
    int position = ostream_tellp(out);
    StreamWrite32(out, &position);
}
