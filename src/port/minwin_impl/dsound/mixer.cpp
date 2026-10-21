#include "mixer.h"

#include <cmath>
#include <cstring>

#include <algorithm>

namespace mwds
{

namespace
{
EventSignaler g_signaler = nullptr;

constexpr int kWriteLeadMs = 15; // how far ahead of the play cursor the write cursor is reported

int32_t PanGainQ15(int pan, bool left)
{
  // pan > 0 attenuates the left channel, pan < 0 the right one
  int attenuation = left ? (pan > 0 ? pan : 0) : (pan < 0 ? -pan : 0);
  return GainQ15(-attenuation);
}

int Sample(const uint8_t* frame, int channel, int bits)
{
  if (bits == 8) {
    return (static_cast<int>(frame[channel]) - 128) << 8;
  }
  auto v = static_cast<int16_t>(frame[channel * 2] | (frame[channel * 2 + 1] << 8));
  return v;
}

} // namespace

void SetEventSignaler(EventSignaler signaler)
{
  g_signaler = signaler;
}

namespace
{
DeviceOpener g_deviceOpener = nullptr;
}
void SetDeviceOpener(DeviceOpener opener)
{
  g_deviceOpener = opener;
}
void OpenDevice()
{
  if (g_deviceOpener) {
    g_deviceOpener();
  }
}

int32_t GainQ15(int hundredthsDb)
{
  if (hundredthsDb <= -10000) {
    return 0;
  }
  if (hundredthsDb >= 0) {
    return 32768;
  }
  return static_cast<int32_t>(floor(pow(10.0, hundredthsDb / 2000.0) * 32768.0 + 0.5));
}

Mixer& Mixer::Global()
{
  static Mixer mixer;
  return mixer;
}

void Mixer::SetOutputRate(int rate)
{
  auto lock = Lock();
  m_outputRate = rate;
}

void Mixer::SetMasterVolume(int hundredthsDb)
{
  auto lock = Lock();
  m_masterVolume = hundredthsDb;
}

uint64_t Mixer::FramesRendered() const
{
  auto lock = Lock();
  return m_frames;
}

void Mixer::Add(Voice* v)
{
  auto lock = Lock();
  m_voices.push_back(v);
}
void Mixer::Remove(Voice* v)
{
  auto lock = Lock();
  m_voices.erase(std::remove(m_voices.begin(), m_voices.end(), v), m_voices.end());
}
void Mixer::Add(Stream* s)
{
  auto lock = Lock();
  m_streams.push_back(s);
}
void Mixer::Remove(Stream* s)
{
  auto lock = Lock();
  m_streams.erase(std::remove(m_streams.begin(), m_streams.end(), s), m_streams.end());
}

// ---- Voice

Voice::Voice(Mixer* mixer, const Format& format, uint32_t sizeBytes)
    : m_mixer(mixer)
    , m_format(format)
    , m_size(sizeBytes)
    , m_data(std::make_shared<std::vector<uint8_t>>(sizeBytes, format.bitsPerSample == 8 ? 128 : 0))
{
  m_mixer->Add(this);
}

Voice::Voice(Mixer* mixer, const Voice& original)
    : m_mixer(mixer)
    , m_format(original.m_format)
    , m_size(original.m_size)
    , m_data(original.m_data)
    , m_volume(original.m_volume)
    , m_pan(original.m_pan)
    , m_frequency(original.m_frequency)
{
  m_mixer->Add(this);
}

Voice::~Voice()
{
  m_mixer->Remove(this);
}

void Voice::Play(bool loop)
{
  auto lock = m_mixer->Lock();
  m_playing = true;
  m_looping = loop;
}

void Voice::Stop()
{
  std::vector<void*> events;
  {
    auto lock = m_mixer->Lock();
    if (m_playing) {
      for (const Notification& n : m_notifications) {
        if (n.offset == kOffsetStop) {
          events.push_back(n.event);
        }
      }
    }
    m_playing = false;
  }
  if (g_signaler) {
    for (void* e : events) {
      g_signaler(e);
    }
  }
}

bool Voice::IsPlaying() const
{
  auto lock = m_mixer->Lock();
  return m_playing;
}

bool Voice::IsLooping() const
{
  auto lock = m_mixer->Lock();
  return m_playing && m_looping;
}

void Voice::GetCursors(uint32_t* play, uint32_t* write) const
{
  auto lock = m_mixer->Lock();
  auto block = static_cast<uint32_t>(m_format.BlockAlign());
  auto p = static_cast<uint32_t>((m_pos >> 32) * block);
  if (p >= m_size) {
    p = 0;
  }
  uint32_t w = p;
  if (m_playing) {
    auto lead = static_cast<uint32_t>(static_cast<uint64_t>(m_format.sampleRate) * block * kWriteLeadMs / 1000);
    lead -= lead % block;
    w = (p + lead) % (m_size ? m_size : 1);
  }
  if (play) {
    *play = p;
  }
  if (write) {
    *write = w;
  }
}

bool Voice::SetPlayCursor(uint32_t bytes)
{
  auto lock = m_mixer->Lock();
  if (bytes >= m_size) {
    return false;
  }
  m_pos = static_cast<uint64_t>(bytes / m_format.BlockAlign()) << 32;
  return true;
}

void Voice::SetVolume(int hundredthsDb)
{
  auto lock = m_mixer->Lock();
  m_volume = hundredthsDb;
}

void Voice::SetPan(int pan)
{
  auto lock = m_mixer->Lock();
  m_pan = pan;
}

void Voice::SetFrequency(uint32_t hz)
{
  auto lock = m_mixer->Lock();
  m_frequency = hz == static_cast<uint32_t>(m_format.sampleRate) ? 0 : hz;
}

void Voice::SetNotifications(const Notification* list, uint32_t count)
{
  auto lock = m_mixer->Lock();
  m_notifications.assign(list, list + count);
}

Voice::Regions Voice::Lock(uint32_t offset, uint32_t bytes)
{
  Regions r = {Data() + offset, bytes, nullptr, 0};
  if (offset + bytes > m_size) {
    r.firstBytes = m_size - offset;
    r.second = Data();
    r.secondBytes = bytes - r.firstBytes;
  }
  return r;
}

// ---- Stream

Stream::Stream(Mixer* mixer, const Format& format)
    : m_mixer(mixer)
    , m_format(format)
{
  m_mixer->Add(this);
}
Stream::~Stream()
{
  m_mixer->Remove(this);
}

void Stream::Push(const uint8_t* pcm, uint32_t bytes)
{
  auto lock = m_mixer->Lock();
  m_queue.insert(m_queue.end(), pcm, pcm + bytes);
}

uint32_t Stream::BufferedBytes() const
{
  auto lock = m_mixer->Lock();
  auto block = static_cast<uint32_t>(m_format.BlockAlign());
  uint64_t consumed = (m_pos >> 32) * block;
  return consumed >= m_queue.size() ? 0 : static_cast<uint32_t>(m_queue.size() - consumed);
}

void Stream::Clear()
{
  auto lock = m_mixer->Lock();
  m_queue.clear();
  m_pos = 0;
}

void Stream::SetVolume(int hundredthsDb)
{
  auto lock = m_mixer->Lock();
  m_volume = hundredthsDb;
}

void Stream::SetPan(int pan)
{
  auto lock = m_mixer->Lock();
  m_pan = pan;
}

// ---- Mixer rendering

void Mixer::RenderVoice(Voice* v, int32_t* acc, int frames, std::vector<void*>* events)
{
  if (!v->m_playing) {
    return;
  }
  const Format& f = v->m_format;
  const auto block = static_cast<uint32_t>(f.BlockAlign());
  const uint64_t sizeFrames = v->m_size / block;
  if (sizeFrames == 0) {
    return;
  }
  const uint64_t step = (static_cast<uint64_t>(v->Frequency()) << 32) / static_cast<uint64_t>(m_outputRate);
  const int32_t volume = GainQ15(v->m_volume);
  const auto gainL = static_cast<int32_t>((static_cast<int64_t>(volume) * PanGainQ15(v->m_pan, true)) >> 15);
  const auto gainR = static_cast<int32_t>((static_cast<int64_t>(volume) * PanGainQ15(v->m_pan, false)) >> 15);
  const uint8_t* data = v->m_data->data();
  uint64_t pos = v->m_pos;
  for (int i = 0; i < frames; ++i) {
    uint64_t frame = pos >> 32;
    if (frame >= sizeFrames) {
      if (!v->m_looping) {
        v->m_playing = false;
        pos = 0;
        for (const Notification& n : v->m_notifications) {
          if (n.offset == kOffsetStop) {
            events->push_back(n.event);
          }
        }
        break;
      }
      pos -= sizeFrames << 32;
      frame = pos >> 32;
    }
    uint64_t next = frame + 1;
    if (next >= sizeFrames) {
      next = v->m_looping ? 0 : frame;
    }
    const uint8_t* a = data + frame * block;
    const uint8_t* b = data + next * block;
    auto frac = static_cast<int32_t>((pos >> 16) & 0xffff);
    int32_t s[2];
    for (int c = 0; c < 2; ++c) {
      int ch = f.channels == 1 ? 0 : c;
      int sa = Sample(a, ch, f.bitsPerSample), sb = Sample(b, ch, f.bitsPerSample);
      s[c] = sa + static_cast<int32_t>((static_cast<int64_t>(sb - sa) * frac) >> 16);
    }
    acc[i * 2] += static_cast<int32_t>((static_cast<int64_t>(s[0]) * gainL) >> 15);
    acc[i * 2 + 1] += static_cast<int32_t>((static_cast<int64_t>(s[1]) * gainR) >> 15);
    uint64_t before = frame;
    pos += step;
    uint64_t after = pos >> 32;
    if (!v->m_notifications.empty() && after != before) {
      for (const Notification& n : v->m_notifications) {
        if (n.offset == kOffsetStop) {
          continue;
        }
        uint64_t target = n.offset / block;
        // crossing target when moving from frame `before` to `after` (possibly wrapping)
        uint64_t lo = before, hi = after;
        bool hit = false;
        if (hi < sizeFrames) {
          hit = target > lo && target <= hi;
        } else {
          hit = target > lo; // up to the end of the buffer
          if (v->m_looping) {
            hit = hit || target <= (hi - sizeFrames);
          }
        }
        if (hit) {
          events->push_back(n.event);
        }
      }
    }
  }
  v->m_pos = pos;
}

void Mixer::RenderStream(Stream* s, int32_t* acc, int frames)
{
  const Format& f = s->m_format;
  const auto block = static_cast<uint32_t>(f.BlockAlign());
  const uint64_t availFrames = s->m_queue.size() / block;
  if (availFrames == 0) {
    return;
  }
  const uint64_t step = (static_cast<uint64_t>(f.sampleRate) << 32) / static_cast<uint64_t>(m_outputRate);
  const int32_t volume = GainQ15(s->m_volume);
  const auto gainL = static_cast<int32_t>((static_cast<int64_t>(volume) * PanGainQ15(s->m_pan, true)) >> 15);
  const auto gainR = static_cast<int32_t>((static_cast<int64_t>(volume) * PanGainQ15(s->m_pan, false)) >> 15);
  const uint8_t* data = s->m_queue.data();
  uint64_t pos = s->m_pos;
  for (int i = 0; i < frames; ++i) {
    uint64_t frame = pos >> 32;
    if (frame >= availFrames) {
      break;
    }
    uint64_t next = frame + 1 < availFrames ? frame + 1 : frame;
    const uint8_t* a = data + frame * block;
    const uint8_t* b = data + next * block;
    auto frac = static_cast<int32_t>((pos >> 16) & 0xffff);
    int32_t v[2];
    for (int c = 0; c < 2; ++c) {
      int ch = f.channels == 1 ? 0 : c;
      int sa = Sample(a, ch, f.bitsPerSample), sb = Sample(b, ch, f.bitsPerSample);
      v[c] = sa + static_cast<int32_t>((static_cast<int64_t>(sb - sa) * frac) >> 16);
    }
    acc[i * 2] += static_cast<int32_t>((static_cast<int64_t>(v[0]) * gainL) >> 15);
    acc[i * 2 + 1] += static_cast<int32_t>((static_cast<int64_t>(v[1]) * gainR) >> 15);
    pos += step;
  }
  // drop what has been played to keep the queue short
  uint64_t consumedFrames = pos >> 32;
  if (consumedFrames > availFrames) {
    consumedFrames = availFrames;
  }
  if (consumedFrames > 4096) {
    s->m_queue.erase(s->m_queue.begin(), s->m_queue.begin() + consumedFrames * block);
    pos -= consumedFrames << 32;
  }
  s->m_pos = pos;
}

void Mixer::Render(int16_t* out, int frames)
{
  std::vector<int32_t> acc(static_cast<size_t>(frames) * 2, 0);
  std::vector<void*> events;
  {
    auto lock = Lock();
    for (Voice* v : m_voices) {
      RenderVoice(v, acc.data(), frames, &events);
    }
    for (Stream* s : m_streams) {
      RenderStream(s, acc.data(), frames);
    }
    const int32_t master = GainQ15(m_masterVolume);
    for (int i = 0; i < frames * 2; ++i) {
      auto x = static_cast<int32_t>((static_cast<int64_t>(acc[i]) * master) >> 15);
      out[i] = static_cast<int16_t>(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
    }
    m_frames += static_cast<uint64_t>(frames);
  }
  if (g_signaler) {
    for (void* e : events) {
      g_signaler(e);
    }
  }
}

} // namespace mwds
