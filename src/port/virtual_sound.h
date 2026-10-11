// Virtual-time sound status for the determinism shims (src/hybrid/determinism.cpp, src/standalone/determinism.cpp), REB_VIRTUAL_SOUND=1.
//
// The game times parts of its UI by its sounds: a droid's talking animation step (AnimationStep04/13) lasts until
// IDirectSoundBuffer::GetStatus stops reporting DSBSTATUS_PLAYING, and DirectSound plays in wall-clock time. The advisor's
// later draws from the session random generator (the one the AI breaks ties with) then land a wall-clock-dependent number of
// game ticks later. Here the playing state the game sees follows the virtual clock instead: a buffer started with Play plays for
// (buffer bytes - start position) / nAvgBytesPerSec of virtual time, a looping one until Stop. The real buffers still play (or
// not) as DirectSound decides; only what GetStatus reports changes. The game never runs without a DirectSound device
// (SoundManager::SetPrimaryBufferFormat dereferences it unchecked), so failing DirectSoundCreate is not an option.
//
// The DirectSound and buffer vtables of the sound driver are patched once; buffers this file did not see created pass through.
// Header-only so both shims share it; never calls game code.
#pragma once

#include <windows.h>

#include <stdint.h>

namespace virtual_sound
{

using NowUs = uint64_t (*)();
using Log = void (*)(const char* format, ...);

enum : int
{
  kDirectSoundCreateSoundBuffer = 3,
  kDirectSoundDuplicateSoundBuffer = 5,
  kBufferRelease = 2,
  kBufferGetStatus = 9,
  kBufferPlay = 12,
  kBufferSetCurrentPosition = 13,
  kBufferStop = 18,
};

constexpr DWORD kStatusPlaying = 0x1;
constexpr DWORD kStatusLooping = 0x4;
constexpr DWORD kPlayLooping = 0x1;
constexpr DWORD kCapsPrimaryBuffer = 0x1;
constexpr int kMaxBuffers = 512;

struct WaveFormat
{ // WAVEFORMATEX, the part read here
  WORD formatTag;
  WORD channels;
  DWORD samplesPerSecond;
  DWORD averageBytesPerSecond;
};

struct BufferDescription
{ // DSBUFFERDESC1
  DWORD size;
  DWORD flags;
  DWORD bufferBytes;
  DWORD reserved;
  WaveFormat* format;
};

struct BufferState
{
  void* buffer;
  uint32_t bytes;
  uint32_t bytesPerSecond;
  uint32_t position;
  bool playing;
  bool looping;
  uint64_t startUs;
};

using CreateSoundBufferFn = HRESULT(WINAPI*)(void*, const BufferDescription*, void**, void*);
using DuplicateSoundBufferFn = HRESULT(WINAPI*)(void*, void*, void**);
using ReleaseFn = ULONG(WINAPI*)(void*);
using GetStatusFn = HRESULT(WINAPI*)(void*, DWORD*);
using PlayFn = HRESULT(WINAPI*)(void*, DWORD, DWORD, DWORD);
using SetCurrentPositionFn = HRESULT(WINAPI*)(void*, DWORD);
using StopFn = HRESULT(WINAPI*)(void*);

struct State
{
  NowUs now = nullptr;
  Log log = nullptr;
  bool trace = false;
  volatile LONG lock = 0;
  BufferState buffers[kMaxBuffers] = {};
  void** soundVtable = nullptr;
  void** bufferVtable = nullptr;
  CreateSoundBufferFn createSoundBuffer = nullptr;
  DuplicateSoundBufferFn duplicateSoundBuffer = nullptr;
  ReleaseFn release = nullptr;
  GetStatusFn getStatus = nullptr;
  PlayFn play = nullptr;
  SetCurrentPositionFn setCurrentPosition = nullptr;
  StopFn stop = nullptr;
};

inline State g_state;

class Guard
{
public:
  Guard()
  {
    while (InterlockedExchange(&g_state.lock, 1))
    {
    }
  }
  ~Guard()
  {
    InterlockedExchange(&g_state.lock, 0);
  }
};

inline void** VtableOf(void* object)
{
  return *static_cast<void***>(object);
}

inline BufferState* Find(void* buffer)
{
  for (BufferState& state : g_state.buffers)
    if (state.buffer == buffer)
      return &state;
  return nullptr;
}

inline void Track(void* buffer, uint32_t bytes, uint32_t bytesPerSecond)
{
  BufferState* state = Find(buffer);
  if (!state)
    state = Find(nullptr);
  if (!state)
  {
    g_state.log("virtual sound: more than %d buffers, %p is not tracked", kMaxBuffers, buffer);
    return;
  }
  *state = BufferState{buffer, bytes, bytesPerSecond, 0, false, false, 0};
}

inline uint64_t DurationUs(const BufferState& state)
{
  if (state.bytesPerSecond == 0 || state.position >= state.bytes)
    return 0;
  return static_cast<uint64_t>(state.bytes - state.position) * 1000000u / state.bytesPerSecond;
}

// A one-shot buffer whose virtual play time has run out has stopped, rewound like DirectSound's.
inline void Expire(BufferState& state)
{
  if (!state.playing || state.looping)
    return;
  if (g_state.now() - state.startUs < DurationUs(state))
    return;
  state.playing = false;
  state.position = 0;
}

inline ULONG WINAPI Release(void* buffer);
inline HRESULT WINAPI GetStatus(void* buffer, DWORD* status);
inline HRESULT WINAPI Play(void* buffer, DWORD reserved1, DWORD priority, DWORD flags);
inline HRESULT WINAPI SetCurrentPosition(void* buffer, DWORD position);
inline HRESULT WINAPI Stop(void* buffer);

inline void PatchSlot(void** vtable, int slot, void* replacement)
{
  DWORD old;
  if (!VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &old))
  {
    g_state.log("virtual sound: VirtualProtect failed at vtable %p slot %d", vtable, slot);
    return;
  }
  vtable[slot] = replacement;
  VirtualProtect(&vtable[slot], sizeof(void*), old, &old);
}

inline void PatchBufferVtable(void* buffer)
{
  void** vtable = VtableOf(buffer);
  if (g_state.bufferVtable == vtable)
    return;
  if (g_state.bufferVtable)
  {
    g_state.log("virtual sound: second buffer vtable %p (first %p) not patched", vtable, g_state.bufferVtable);
    return;
  }
  g_state.bufferVtable = vtable;
  g_state.release = reinterpret_cast<ReleaseFn>(vtable[kBufferRelease]);
  g_state.getStatus = reinterpret_cast<GetStatusFn>(vtable[kBufferGetStatus]);
  g_state.play = reinterpret_cast<PlayFn>(vtable[kBufferPlay]);
  g_state.setCurrentPosition = reinterpret_cast<SetCurrentPositionFn>(vtable[kBufferSetCurrentPosition]);
  g_state.stop = reinterpret_cast<StopFn>(vtable[kBufferStop]);
  PatchSlot(vtable, kBufferRelease, reinterpret_cast<void*>(&Release));
  PatchSlot(vtable, kBufferGetStatus, reinterpret_cast<void*>(&GetStatus));
  PatchSlot(vtable, kBufferPlay, reinterpret_cast<void*>(&Play));
  PatchSlot(vtable, kBufferSetCurrentPosition, reinterpret_cast<void*>(&SetCurrentPosition));
  PatchSlot(vtable, kBufferStop, reinterpret_cast<void*>(&Stop));
}

inline HRESULT WINAPI CreateSoundBuffer(void* sound, const BufferDescription* description, void** buffer, void* outer)
{
  const HRESULT result = g_state.createSoundBuffer(sound, description, buffer, outer);
  if (result < 0 || !buffer || !*buffer || !description || (description->flags & kCapsPrimaryBuffer))
    return result;
  Guard guard;
  PatchBufferVtable(*buffer);
  Track(*buffer, description->bufferBytes, description->format ? description->format->averageBytesPerSecond : 0);
  return result;
}

inline HRESULT WINAPI DuplicateSoundBuffer(void* sound, void* original, void** duplicate)
{
  const HRESULT result = g_state.duplicateSoundBuffer(sound, original, duplicate);
  if (result < 0 || !duplicate || !*duplicate)
    return result;
  Guard guard;
  if (const BufferState* source = Find(original))
  {
    const BufferState copy = *source;
    Track(*duplicate, copy.bytes, copy.bytesPerSecond);
  }
  return result;
}

inline ULONG WINAPI Release(void* buffer)
{
  const ULONG count = g_state.release(buffer);
  if (count == 0)
  {
    Guard guard;
    if (BufferState* state = Find(buffer))
      state->buffer = nullptr;
  }
  return count;
}

inline HRESULT WINAPI GetStatus(void* buffer, DWORD* status)
{
  const HRESULT result = g_state.getStatus(buffer, status);
  if (result < 0 || !status)
    return result;
  Guard guard;
  if (BufferState* state = Find(buffer))
  {
    Expire(*state);
    *status &= ~(kStatusPlaying | kStatusLooping);
    if (state->playing)
      *status |= kStatusPlaying | (state->looping ? kStatusLooping : 0);
  }
  return result;
}

inline HRESULT WINAPI Play(void* buffer, DWORD reserved1, DWORD priority, DWORD flags)
{
  const HRESULT result = g_state.play(buffer, reserved1, priority, flags);
  if (result < 0)
    return result;
  Guard guard;
  if (BufferState* state = Find(buffer))
  {
    Expire(*state);
    if (state->playing)
      state->position += static_cast<uint32_t>((g_state.now() - state->startUs) * state->bytesPerSecond / 1000000u);
    state->playing = true;
    state->looping = (flags & kPlayLooping) != 0;
    state->startUs = g_state.now();
    if (g_state.trace)
      g_state.log("virtual sound: play %p at %u us, %u bytes from %u at %u/s, %s", buffer,
                  static_cast<unsigned>(state->startUs), state->bytes, state->position, state->bytesPerSecond,
                  state->looping ? "looping" : "once");
  }
  return result;
}

inline HRESULT WINAPI SetCurrentPosition(void* buffer, DWORD position)
{
  const HRESULT result = g_state.setCurrentPosition(buffer, position);
  if (result < 0)
    return result;
  Guard guard;
  if (BufferState* state = Find(buffer))
  {
    state->position = position;
    state->startUs = g_state.now();
  }
  return result;
}

inline HRESULT WINAPI Stop(void* buffer)
{
  const HRESULT result = g_state.stop(buffer);
  if (result < 0)
    return result;
  Guard guard;
  if (BufferState* state = Find(buffer))
  {
    Expire(*state);
    if (state->playing && !state->looping)
      state->position += static_cast<uint32_t>((g_state.now() - state->startUs) * state->bytesPerSecond / 1000000u);
    state->playing = false;
  }
  return result;
}

// Call with the result of the real DirectSoundCreate.
// `trace` logs every Play of a tracked buffer.
inline void Attach(HRESULT result, void** directSound, NowUs now, Log log, bool trace)
{
  if (result < 0 || !directSound || !*directSound)
    return;
  Guard guard;
  g_state.now = now;
  g_state.log = log;
  g_state.trace = trace;
  void** vtable = VtableOf(*directSound);
  if (g_state.soundVtable == vtable)
    return;
  g_state.soundVtable = vtable;
  g_state.createSoundBuffer = reinterpret_cast<CreateSoundBufferFn>(vtable[kDirectSoundCreateSoundBuffer]);
  g_state.duplicateSoundBuffer = reinterpret_cast<DuplicateSoundBufferFn>(vtable[kDirectSoundDuplicateSoundBuffer]);
  PatchSlot(vtable, kDirectSoundCreateSoundBuffer, reinterpret_cast<void*>(&CreateSoundBuffer));
  PatchSlot(vtable, kDirectSoundDuplicateSoundBuffer, reinterpret_cast<void*>(&DuplicateSoundBuffer));
  log("virtual sound: DirectSound %p patched, buffer status follows the virtual clock", *directSound);
}

} // namespace virtual_sound
