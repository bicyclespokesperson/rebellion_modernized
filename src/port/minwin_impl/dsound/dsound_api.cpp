// IDirectSound / IDirectSoundBuffer / IDirectSoundNotify / IDirectSound3DListener on the software mixer (mixer.h), with the
// argument checking and return codes of the real DirectSound for the calls the game makes (docs/platform_layer.md section 4).
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/dsound.h"

#include <cstring>

#include <atomic>
#include <memory>

#include "mixer.h"

using namespace mwds;

extern "C" {
const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID IID_IDirectSound = {0x279afa83, 0x4981, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
const GUID IID_IDirectSoundBuffer = {0x279afa85, 0x4981, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
#if !defined( \
  MW_IIDS_IN_GAME_DATA) // the standalone executable's data image already defines these two (and IID_IDirectDraw2)
const GUID IID_IDirectSound3DListener = {0x279afa84, 0x4981, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};
const GUID IID_IDirectSoundNotify = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};
#endif
}

namespace
{

constexpr HRESULT kDsOk = 0;
constexpr HRESULT kErrInvalidParam = static_cast<HRESULT>(0x80070057);
constexpr HRESULT kErrNoInterface = static_cast<HRESULT>(0x80004002);
constexpr HRESULT kErrUnsupported = static_cast<HRESULT>(0x80004001);
constexpr HRESULT kErrControlUnavailable = static_cast<HRESULT>(0x8878001e);
constexpr HRESULT kErrInvalidCall = static_cast<HRESULT>(0x88780032);
constexpr HRESULT kErrPriorityLevelNeeded = static_cast<HRESULT>(0x88780046);
constexpr HRESULT kErrBadFormat = static_cast<HRESULT>(0x88780064);
constexpr HRESULT kErrAlreadyInitialized = static_cast<HRESULT>(0x88780078);

constexpr DWORD kCapsPrimary = 0x1, kCapsCtrl3d = 0x10, kCapsCtrlFrequency = 0x20, kCapsCtrlPan = 0x40,
                kCapsCtrlVolume = 0x80, kCapsCtrlNotify = 0x100;
constexpr DWORD kLockFromWriteCursor = 1, kLockEntireBuffer = 2;
constexpr DWORD kStatusPlaying = 1, kStatusLooping = 4;
constexpr DWORD kPlayLooping = 1;
constexpr DWORD kSizeMin = 4, kSizeMax = 0x0fffffff;
constexpr int kMinVolume = -10000, kMaxPan = 10000;

bool SameGuid(const GUID& a, const GUID& b)
{
  return memcmp(&a, &b, sizeof(GUID)) == 0;
}

using VirtualClock = uint64_t (*)();
VirtualClock g_virtualClock = nullptr;

Mutex& VirtualSoundMutex()
{
  static Mutex mutex;
  return mutex;
}

// REB_VIRTUAL_SOUND (src/port/virtual_sound.h): the playing state a secondary buffer reports follows a virtual microsecond clock
// instead of the mixer's wall-clock playback. A one-shot buffer plays (bytes - position) / bytes-per-second of virtual time, a looping
// one until Stop. The mixer still renders as usual; only GetStatus changes.
struct VirtualPlayback
{
  uint32_t bytes = 0, bytesPerSecond = 0, position = 0;
  bool playing = false, looping = false;
  uint64_t startUs = 0;

  uint64_t DurationUs() const
  {
    if (bytesPerSecond == 0 || position >= bytes)
      return 0;
    return static_cast<uint64_t>(bytes - position) * 1000000u / bytesPerSecond;
  }
  void Expire()
  {
    if (!playing || looping || g_virtualClock() - startUs < DurationUs())
      return;
    playing = false;
    position = 0;
  }
  uint32_t Elapsed() const
  {
    return static_cast<uint32_t>((g_virtualClock() - startUs) * bytesPerSecond / 1000000u);
  }
  void Play(bool loop)
  {
    Expire();
    if (playing)
      position += Elapsed();
    playing = true;
    looping = loop;
    startUs = g_virtualClock();
  }
  void Stop()
  {
    Expire();
    if (playing && !looping)
      position += Elapsed();
    playing = false;
  }
  void SetPosition(uint32_t where)
  {
    position = where;
    startUs = g_virtualClock();
  }
};

struct BufferCaps
{ // DSBCAPS
  DWORD dwSize, dwFlags, dwBufferBytes, dwUnlockTransferRate, dwPlayCpuOverhead;
};

class DirectSoundImpl;
class BufferImpl;

class NotifyImpl final : public IDirectSoundNotify
{
public:
  explicit NotifyImpl(BufferImpl* owner) : m_owner(owner)
  {
  }
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override;
  ULONG REB_STDCALL AddRef() override;
  ULONG REB_STDCALL Release() override;
  HRESULT REB_STDCALL SetNotificationPositions(DWORD count, LPCDSBPOSITIONNOTIFY positions) override;

private:
  BufferImpl* m_owner;
};

class ListenerImpl final : public IDirectSound3DListener
{
public:
  explicit ListenerImpl(BufferImpl* owner) : m_owner(owner)
  {
  }
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override;
  ULONG REB_STDCALL AddRef() override;
  ULONG REB_STDCALL Release() override;
  HRESULT REB_STDCALL GetAllParameters(LPDS3DLISTENER) override
  {
    return kErrUnsupported;
  }
  HRESULT REB_STDCALL GetDistanceFactor(D3DVALUE* v) override
  {
    *v = 1.0f;
    return kDsOk;
  }
  HRESULT REB_STDCALL GetDopplerFactor(D3DVALUE* v) override
  {
    *v = 1.0f;
    return kDsOk;
  }
  HRESULT REB_STDCALL GetOrientation(D3DVECTOR*, D3DVECTOR*) override
  {
    return kErrUnsupported;
  }
  HRESULT REB_STDCALL GetPosition(D3DVECTOR*) override
  {
    return kErrUnsupported;
  }
  HRESULT REB_STDCALL GetRolloffFactor(D3DVALUE* v) override
  {
    *v = 1.0f;
    return kDsOk;
  }
  HRESULT REB_STDCALL GetVelocity(D3DVECTOR*) override
  {
    return kErrUnsupported;
  }
  HRESULT REB_STDCALL SetAllParameters(LPCDS3DLISTENER, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetDistanceFactor(D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetDopplerFactor(D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetOrientation(D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetPosition(D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetRolloffFactor(D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL SetVelocity(D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL CommitDeferredSettings() override
  {
    return kDsOk;
  }

private:
  BufferImpl* m_owner;
};

class BufferImpl final : public IDirectSoundBuffer
{
public:
  BufferImpl(DirectSoundImpl* device, DWORD flags, std::unique_ptr<Voice> voice, const Format& format);
  ~BufferImpl();

  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override;
  ULONG REB_STDCALL AddRef() override
  {
    return ++m_refs;
  }
  ULONG REB_STDCALL Release() override;
  HRESULT REB_STDCALL GetCaps(LPDSBCAPS caps) override;
  HRESULT REB_STDCALL GetCurrentPosition(LPDWORD play, LPDWORD write) override;
  HRESULT REB_STDCALL GetFormat(LPWAVEFORMATEX out, DWORD allocated, LPDWORD written) override;
  HRESULT REB_STDCALL GetVolume(LPLONG v) override;
  HRESULT REB_STDCALL GetPan(LPLONG v) override;
  HRESULT REB_STDCALL GetFrequency(LPDWORD v) override;
  HRESULT REB_STDCALL GetStatus(LPDWORD status) override;
  HRESULT REB_STDCALL Initialize(LPDIRECTSOUND, LPCDSBUFFERDESC) override
  {
    return kErrAlreadyInitialized;
  }
  HRESULT REB_STDCALL
  Lock(DWORD offset, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD flags) override;
  HRESULT REB_STDCALL Play(DWORD reserved1, DWORD priority, DWORD flags) override;
  HRESULT REB_STDCALL SetCurrentPosition(DWORD position) override;
  HRESULT REB_STDCALL SetFormat(LPCWAVEFORMATEX format) override;
  HRESULT REB_STDCALL SetVolume(LONG volume) override;
  HRESULT REB_STDCALL SetPan(LONG pan) override;
  HRESULT REB_STDCALL SetFrequency(DWORD hz) override;
  HRESULT REB_STDCALL Stop() override;
  HRESULT REB_STDCALL Unlock(LPVOID p1, DWORD b1, LPVOID p2, DWORD b2) override;
  HRESULT REB_STDCALL Restore() override
  {
    return kDsOk;
  }

  bool IsPrimary() const
  {
    return (m_flags & kCapsPrimary) != 0;
  }
  DWORD Flags() const
  {
    return m_flags;
  }
  Voice* GetVoice()
  {
    return m_voice.get();
  }
  void InitVirtualPlayback(uint32_t bytes, uint32_t bytesPerSecond)
  {
    m_virtual.bytes = bytes;
    m_virtual.bytesPerSecond = bytesPerSecond;
  }
  const VirtualPlayback& Virtual() const
  {
    return m_virtual;
  }
  void SetPrimaryFormat(const Format& f)
  {
    m_primaryFormat = f;
  }

  std::atomic<int> m_refs{1};
  NotifyImpl m_notify;
  ListenerImpl m_listener;

private:
  DirectSoundImpl* m_device;
  DWORD m_flags;
  std::unique_ptr<Voice> m_voice;
  Format m_primaryFormat;
  int m_primaryVolume = 0;
  int m_primaryPan = 0;
  bool m_primaryPlaying = false, m_primaryLooping = false;
  VirtualPlayback m_virtual;
};

class DirectSoundImpl final : public IDirectSound
{
public:
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override
  {
    if (!ppv)
      return kErrInvalidParam;
    if (SameGuid(riid, IID_IUnknown) || SameGuid(riid, IID_IDirectSound))
    {
      *ppv = static_cast<IDirectSound*>(this);
      AddRef();
      return kDsOk;
    }
    *ppv = nullptr;
    return kErrNoInterface;
  }
  ULONG REB_STDCALL AddRef() override
  {
    return ++m_refs;
  }
  ULONG REB_STDCALL Release() override
  {
    int n = --m_refs;
    if (n == 0)
      delete this;
    return static_cast<ULONG>(n);
  }
  HRESULT REB_STDCALL CreateSoundBuffer(LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER* out, LPUNKNOWN outer) override;
  HRESULT REB_STDCALL GetCaps(LPDSCAPS) override
  {
    return kErrUnsupported;
  }
  HRESULT REB_STDCALL DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER original, LPDIRECTSOUNDBUFFER* out) override;
  HRESULT REB_STDCALL SetCooperativeLevel(HWND, DWORD level) override
  {
    if (level < 1 || level > 4)
      return kErrInvalidParam;
    m_level = level;
    return kDsOk;
  }
  HRESULT REB_STDCALL Compact() override
  {
    return m_level < 2 ? kErrPriorityLevelNeeded : kDsOk;
  }
  HRESULT REB_STDCALL GetSpeakerConfig(LPDWORD config) override
  {
    if (!config)
      return kErrInvalidParam;
    *config = 3; // DSSPEAKER_STEREO
    return kDsOk;
  }
  HRESULT REB_STDCALL SetSpeakerConfig(DWORD) override
  {
    return kDsOk;
  }
  HRESULT REB_STDCALL Initialize(LPCGUID) override
  {
    return kErrAlreadyInitialized;
  }

  DWORD CooperativeLevel() const
  {
    return m_level;
  }
  BufferImpl* m_primary = nullptr;

private:
  std::atomic<int> m_refs{1};
  DWORD m_level = 1;
};

bool ReadFormat(const WAVEFORMATEX* wf, Format* out)
{
  if (!wf || wf->wFormatTag != 1 || wf->nChannels < 1 || wf->nChannels > 2 ||
      (wf->wBitsPerSample != 8 && wf->wBitsPerSample != 16) || wf->nSamplesPerSec < 100 || wf->nSamplesPerSec > 200000)
    return false;
  out->channels = wf->nChannels;
  out->bitsPerSample = wf->wBitsPerSample;
  out->sampleRate = static_cast<int>(wf->nSamplesPerSec);
  return true;
}

// ---- BufferImpl

BufferImpl::BufferImpl(DirectSoundImpl* device, DWORD flags, std::unique_ptr<Voice> voice, const Format& format)
: m_notify(this),
  m_listener(this),
  m_device(device),
  m_flags(flags),
  m_voice(std::move(voice)),
  m_primaryFormat(format)
{
  device->AddRef();
}

BufferImpl::~BufferImpl()
{
  if (m_device->m_primary == this)
    m_device->m_primary = nullptr;
  m_device->Release();
}

ULONG BufferImpl::Release()
{
  int n = --m_refs;
  if (n == 0)
    delete this;
  return static_cast<ULONG>(n);
}

HRESULT BufferImpl::QueryInterface(const IID& riid, LPVOID* ppv)
{
  if (!ppv)
    return kErrInvalidParam;
  *ppv = nullptr;
  if (SameGuid(riid, IID_IUnknown) || SameGuid(riid, IID_IDirectSoundBuffer))
  {
    *ppv = static_cast<IDirectSoundBuffer*>(this);
  }
  else if (SameGuid(riid, IID_IDirectSoundNotify))
  {
    if (IsPrimary() || !(m_flags & kCapsCtrlNotify))
      return kErrNoInterface;
    *ppv = static_cast<IDirectSoundNotify*>(&m_notify);
  }
  else if (SameGuid(riid, IID_IDirectSound3DListener))
  {
    if (!IsPrimary() || !(m_flags & kCapsCtrl3d))
      return kErrNoInterface;
    *ppv = static_cast<IDirectSound3DListener*>(&m_listener);
  }
  else
  {
    return kErrNoInterface;
  }
  AddRef();
  return kDsOk;
}

HRESULT BufferImpl::GetCaps(LPDSBCAPS caps)
{
  auto* c = reinterpret_cast<BufferCaps*>(caps);
  if (!c || c->dwSize < sizeof(BufferCaps))
    return kErrInvalidParam;
  c->dwFlags = m_flags;
  c->dwBufferBytes = IsPrimary() ? 0 : m_voice->SizeBytes();
  c->dwUnlockTransferRate = 0;
  c->dwPlayCpuOverhead = 0;
  return kDsOk;
}

HRESULT BufferImpl::GetCurrentPosition(LPDWORD play, LPDWORD write)
{
  if (IsPrimary())
  {
    if (play)
      *play = 0;
    if (write)
      *write = 0;
    return kDsOk;
  }
  uint32_t p, w;
  m_voice->GetCursors(&p, &w);
  if (play)
    *play = p;
  if (write)
    *write = w;
  return kDsOk;
}

HRESULT BufferImpl::GetFormat(LPWAVEFORMATEX out, DWORD allocated, LPDWORD written)
{
  const Format& f = IsPrimary() ? m_primaryFormat : m_voice->GetFormat();
  if (!out && !written)
    return kErrInvalidParam;
  if (written)
    *written = sizeof(WAVEFORMATEX);
  if (!out)
    return kDsOk;
  if (allocated < sizeof(WAVEFORMATEX) - 2)
    return kErrInvalidParam;
  WAVEFORMATEX wf = {};
  wf.wFormatTag = 1;
  wf.nChannels = static_cast<WORD>(f.channels);
  wf.nSamplesPerSec = static_cast<DWORD>(f.sampleRate);
  wf.wBitsPerSample = static_cast<WORD>(f.bitsPerSample);
  wf.nBlockAlign = static_cast<WORD>(f.BlockAlign());
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
  memcpy(out, &wf, allocated < sizeof wf ? allocated : sizeof wf);
  return kDsOk;
}

HRESULT BufferImpl::GetVolume(LPLONG v)
{
  if (!v)
    return kErrInvalidParam;
  if (!(m_flags & kCapsCtrlVolume))
    return kErrControlUnavailable;
  *v = IsPrimary() ? m_primaryVolume : m_voice->Volume();
  return kDsOk;
}

HRESULT BufferImpl::GetPan(LPLONG v)
{
  if (!v)
    return kErrInvalidParam;
  if (!(m_flags & kCapsCtrlPan))
    return kErrControlUnavailable;
  *v = IsPrimary() ? m_primaryPan : m_voice->Pan();
  return kDsOk;
}

HRESULT BufferImpl::GetFrequency(LPDWORD v)
{
  if (!v)
    return kErrInvalidParam;
  if (!(m_flags & kCapsCtrlFrequency))
    return kErrControlUnavailable;
  *v = IsPrimary() ? static_cast<DWORD>(m_primaryFormat.sampleRate) : m_voice->Frequency();
  return kDsOk;
}

HRESULT BufferImpl::GetStatus(LPDWORD status)
{
  if (!status)
    return kErrInvalidParam;
  DWORD s = 0;
  if (IsPrimary())
  {
    if (m_primaryPlaying)
      s |= kStatusPlaying | (m_primaryLooping ? kStatusLooping : 0);
  }
  else if (g_virtualClock)
  {
    Guard guard(VirtualSoundMutex());
    m_virtual.Expire();
    if (m_virtual.playing)
      s |= kStatusPlaying | (m_virtual.looping ? kStatusLooping : 0);
  }
  else if (m_voice->IsPlaying())
  {
    s |= kStatusPlaying | (m_voice->IsLooping() ? kStatusLooping : 0);
  }
  *status = s;
  return kDsOk;
}

HRESULT BufferImpl::Lock(DWORD offset, DWORD bytes, LPVOID* p1, LPDWORD b1, LPVOID* p2, LPDWORD b2, DWORD flags)
{
  if (!p1 || !b1)
    return kErrInvalidParam;
  *p1 = nullptr;
  *b1 = 0;
  if (p2)
    *p2 = nullptr;
  if (b2)
    *b2 = 0;
  if (IsPrimary())
    return kErrInvalidCall; // the primary buffer cannot be locked below WRITEPRIMARY priority
  uint32_t size = m_voice->SizeBytes();
  if (flags & kLockFromWriteCursor)
  {
    uint32_t play, write;
    m_voice->GetCursors(&play, &write);
    offset = write;
  }
  if (flags & kLockEntireBuffer)
    bytes = size;
  if (offset >= size || bytes > size || bytes == 0)
    return kErrInvalidParam;
  if (p2 && !b2)
    return kErrInvalidParam;
  Voice::Regions r = m_voice->Lock(offset, bytes);
  *p1 = r.first;
  *b1 = r.firstBytes;
  if (p2 && b2)
  {
    *p2 = r.second;
    *b2 = r.secondBytes;
  }
  return kDsOk;
}

HRESULT BufferImpl::Play(DWORD reserved1, DWORD, DWORD flags)
{
  if (reserved1 != 0)
    return kErrInvalidParam;
  if (IsPrimary())
  {
    m_primaryPlaying = true;
    m_primaryLooping = (flags & kPlayLooping) != 0;
    if (!m_primaryLooping)
      return kErrInvalidParam; // the primary buffer must loop
    return kDsOk;
  }
  m_voice->Play((flags & kPlayLooping) != 0);
  if (g_virtualClock)
  {
    Guard guard(VirtualSoundMutex());
    m_virtual.Play((flags & kPlayLooping) != 0);
  }
  return kDsOk;
}

HRESULT BufferImpl::SetCurrentPosition(DWORD position)
{
  if (IsPrimary())
    return kErrInvalidCall;
  if (!m_voice->SetPlayCursor(position))
    return kErrInvalidParam;
  if (g_virtualClock)
  {
    Guard guard(VirtualSoundMutex());
    m_virtual.SetPosition(position);
  }
  return kDsOk;
}

HRESULT BufferImpl::SetFormat(LPCWAVEFORMATEX wf)
{
  if (!IsPrimary())
    return kErrInvalidCall;
  if (m_device->CooperativeLevel() < 2)
    return kErrPriorityLevelNeeded;
  Format f;
  if (!ReadFormat(wf, &f))
    return kErrBadFormat;
  m_primaryFormat = f;
  return kDsOk;
}

HRESULT BufferImpl::SetVolume(LONG volume)
{
  if (volume < kMinVolume || volume > 0)
    return kErrInvalidParam;
  if (!(m_flags & kCapsCtrlVolume))
    return kErrControlUnavailable;
  if (IsPrimary())
  {
    m_primaryVolume = volume;
    Mixer::Global().SetMasterVolume(volume);
  }
  else
  {
    m_voice->SetVolume(volume);
  }
  return kDsOk;
}

HRESULT BufferImpl::SetPan(LONG pan)
{
  if (pan < -kMaxPan || pan > kMaxPan)
    return kErrInvalidParam;
  if (!(m_flags & kCapsCtrlPan))
    return kErrControlUnavailable;
  if (IsPrimary())
    m_primaryPan = pan;
  else
    m_voice->SetPan(pan);
  return kDsOk;
}

HRESULT BufferImpl::SetFrequency(DWORD hz)
{
  if (!(m_flags & kCapsCtrlFrequency))
    return kErrControlUnavailable;
  if (IsPrimary())
    return kErrInvalidCall;
  if (hz != 0 && (hz < 100 || hz > 100000))
    return kErrInvalidParam;
  m_voice->SetFrequency(hz);
  return kDsOk;
}

HRESULT BufferImpl::Stop()
{
  if (IsPrimary())
  {
    m_primaryPlaying = false;
    return kDsOk;
  }
  m_voice->Stop();
  if (g_virtualClock)
  {
    Guard guard(VirtualSoundMutex());
    m_virtual.Stop();
  }
  return kDsOk;
}

HRESULT BufferImpl::Unlock(LPVOID p1, DWORD b1, LPVOID p2, DWORD b2)
{
  if (IsPrimary())
    return kErrInvalidCall;
  uint8_t* base = m_voice->Data();
  uint32_t size = m_voice->SizeBytes();
  auto inside = [&](LPVOID p, DWORD n)
  {
    return !p || (static_cast<uint8_t*>(p) >= base && static_cast<uint8_t*>(p) + n <= base + size);
  };
  if (!p1 && !p2)
    return kErrInvalidParam;
  return inside(p1, b1) && inside(p2, b2) ? kDsOk : kErrInvalidParam;
}

// ---- notify, listener (delegate lifetime to the buffer)

HRESULT NotifyImpl::QueryInterface(const IID& riid, LPVOID* ppv)
{
  return m_owner->QueryInterface(riid, ppv);
}
ULONG NotifyImpl::AddRef()
{
  return m_owner->AddRef();
}
ULONG NotifyImpl::Release()
{
  return m_owner->Release();
}

HRESULT NotifyImpl::SetNotificationPositions(DWORD count, LPCDSBPOSITIONNOTIFY positions)
{
  if (count == 0)
  {
    m_owner->GetVoice()->SetNotifications(nullptr, 0);
    return kDsOk;
  }
  if (!positions || count > 1024)
    return kErrInvalidParam;
  if (m_owner->GetVoice()->IsPlaying())
    return kErrInvalidCall; // positions can only be set on a stopped buffer
  std::vector<Notification> list;
  for (DWORD i = 0; i < count; ++i)
  {
    if (positions[i].dwOffset != kOffsetStop && positions[i].dwOffset >= m_owner->GetVoice()->SizeBytes())
      return kErrInvalidParam;
    list.push_back({positions[i].dwOffset, positions[i].hEventNotify});
  }
  m_owner->GetVoice()->SetNotifications(list.data(), count);
  return kDsOk;
}

HRESULT ListenerImpl::QueryInterface(const IID& riid, LPVOID* ppv)
{
  return m_owner->QueryInterface(riid, ppv);
}
ULONG ListenerImpl::AddRef()
{
  return m_owner->AddRef();
}
ULONG ListenerImpl::Release()
{
  return m_owner->Release();
}

// ---- device

HRESULT DirectSoundImpl::CreateSoundBuffer(LPCDSBUFFERDESC pDesc, LPDIRECTSOUNDBUFFER* out, LPUNKNOWN outer)
{
  if (!out)
    return kErrInvalidParam;
  *out = nullptr;
  if (!pDesc || outer)
    return kErrInvalidParam;
  const auto* desc = reinterpret_cast<const DSBUFFERDESC1*>(pDesc);
  if (desc->dwSize < sizeof(DSBUFFERDESC1))
    return kErrInvalidParam;
  DWORD flags = desc->dwFlags;
  if (flags & kCapsPrimary)
  {
    if (desc->dwBufferBytes != 0 || desc->lpwfxFormat)
      return kErrInvalidParam;
    if (!m_primary)
    {
      Format f;
      m_primary = new BufferImpl(this, flags, nullptr, f);
    }
    else
    {
      m_primary->AddRef();
    }
    *out = m_primary;
    return kDsOk;
  }
  Format f;
  if (!ReadFormat(desc->lpwfxFormat, &f))
    return desc->lpwfxFormat ? kErrBadFormat : kErrInvalidParam;
  if (desc->dwBufferBytes < kSizeMin || desc->dwBufferBytes > kSizeMax)
    return kErrInvalidParam;
  auto voice = std::make_unique<Voice>(&Mixer::Global(), f, desc->dwBufferBytes);
  auto* buffer = new BufferImpl(this, flags, std::move(voice), f);
  buffer->InitVirtualPlayback(desc->dwBufferBytes, desc->lpwfxFormat->nAvgBytesPerSec);
  *out = buffer;
  return kDsOk;
}

HRESULT DirectSoundImpl::DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER original, LPDIRECTSOUNDBUFFER* out)
{
  if (!out)
    return kErrInvalidParam;
  *out = nullptr;
  auto* src = static_cast<BufferImpl*>(original);
  if (!src || src->IsPrimary())
    return kErrInvalidParam;
  auto voice = std::make_unique<Voice>(&Mixer::Global(), *src->GetVoice());
  auto* buffer = new BufferImpl(this, src->Flags(), std::move(voice), src->GetVoice()->GetFormat());
  buffer->InitVirtualPlayback(src->Virtual().bytes, src->Virtual().bytesPerSecond);
  *out = buffer;
  return kDsOk;
}

} // namespace

extern "C" HRESULT REB_STDCALL DirectSoundCreate(LPCGUID, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
{
  if (!ppDS)
    return kErrInvalidParam;
  *ppDS = nullptr;
  if (pUnkOuter)
    return kErrInvalidParam;
  OpenDevice();
  *ppDS = new DirectSoundImpl;
  return kDsOk;
}

// The determinism shim (src/standalone/determinism.cpp) calls this with its virtual clock when REB_VIRTUAL_SOUND is set.
extern "C" void MinwinDirectSoundSetVirtualClock(uint64_t (*nowUs)())
{
  g_virtualClock = nowUs;
}
