#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "iostream/iostream.h"

// Binary I/O of 1, 2 and 4 bytes. Every persistent object in the game saves and loads itself with these.
//
// The `void*` functions are the raw primitives (the hooked originals). Use them only when the bytes are not one typed
// field, with an explicit cast to `void*`. A call with a typed pointer selects the checking templates further down, which
// require the field to have the width of the file format: a pointer, a `long` or a 64-bit field in a saved structure is a
// compile error on a 64-bit target instead of a silently different save layout.
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

template <class T>
inline void ReadValue8(istream* in, T* value) {
    static_assert(sizeof(T) == 1, "ReadValue8 on a field that is not 1 byte wide");
    ReadValue8(in, static_cast<void*>(value));
}
template <class T>
inline void WriteValue8(ostream* out, const T* value) {
    static_assert(sizeof(T) == 1, "WriteValue8 on a field that is not 1 byte wide");
    WriteValue8(out, static_cast<const void*>(value));
}
template <class T>
inline void ReadValue16(istream* in, T* value) {
    static_assert(sizeof(T) == 2, "ReadValue16 on a field that is not 2 bytes wide");
    ReadValue16(in, static_cast<void*>(value));
}
template <class T>
inline void WriteValue16(ostream* out, const T* value) {
    static_assert(sizeof(T) == 2, "WriteValue16 on a field that is not 2 bytes wide");
    WriteValue16(out, static_cast<const void*>(value));
}
template <class T>
inline void ReadValue32(istream* in, T* value) {
    static_assert(sizeof(T) == 4, "ReadValue32 on a field that is not 4 bytes wide");
    ReadValue32(in, static_cast<void*>(value));
}
template <class T>
inline void WriteValue32(ostream* out, const T* value) {
    static_assert(sizeof(T) == 4, "WriteValue32 on a field that is not 4 bytes wide");
    WriteValue32(out, static_cast<const void*>(value));
}
template <class T>
inline void StreamRead16(istream* in, T* value) {
    static_assert(sizeof(T) == 2, "StreamRead16 on a field that is not 2 bytes wide");
    StreamRead16(in, static_cast<void*>(value));
}
template <class T>
inline void StreamWrite16(ostream* out, const T* value) {
    static_assert(sizeof(T) == 2, "StreamWrite16 on a field that is not 2 bytes wide");
    StreamWrite16(out, static_cast<const void*>(value));
}
template <class T>
inline void StreamRead32(istream* in, T* value) {
    static_assert(sizeof(T) == 4, "StreamRead32 on a field that is not 4 bytes wide");
    StreamRead32(in, static_cast<void*>(value));
}
template <class T>
inline void StreamWrite32(ostream* out, const T* value) {
    static_assert(sizeof(T) == 4, "StreamWrite32 on a field that is not 4 bytes wide");
    StreamWrite32(out, static_cast<const void*>(value));
}

void ReadString(istream* in, std::string* value);
void WriteString(ostream* out, std::string* value);
void ReadValueString(istream* in, std::string* value);
void WriteValueString(ostream* out, std::string* value);

// Save files interleave position markers with the data: the writer stores its own stream position, the reader
// checks that the stored position matches where it is, to detect a desynchronised file.
int CheckPositionMarker(istream* in);
void WritePositionMarker(ostream* out);
