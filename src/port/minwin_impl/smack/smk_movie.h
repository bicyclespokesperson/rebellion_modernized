// minwin_impl/smack: a Smacker (SMK2) decoder written from the published description of the format, with no code taken
// from any existing decoder. It reproduces what RAD's SMACKW32.DLL delivers: the 8-bit picture of every frame, the
// palette and the PCM audio. Verified frame by frame against the DLL under Wine (smack/tests).
//
// File layout (little endian): header (signature, size, frame count, frame rate, flags, per-track audio sizes and
// rates, the sizes of the four Huffman tables), per-frame sizes and type bytes, the packed Huffman trees, then the
// frames. A frame holds an optional palette update, an optional chunk per audio track and the video data.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace mw::res
{
class MappedFile;
}

namespace mwsmk
{

struct AudioTrackInfo {
  bool present = false;
  bool packed = false; // Huffman-coded DPCM
  bool stereo = false;
  bool bits16 = false;
  uint32_t sampleRate = 0;
  uint32_t maxBytes = 0; // the largest decompressed chunk
};

constexpr int kAudioTracks = 7;

struct Palette {
  uint8_t rgb[256 * 3] = {};
};

class Movie
{
  public:
  ~Movie();
  static Movie* Open(const std::string& nativePath);
  // From memory (tests); the buffer must outlive the Movie.
  static Movie* FromMemory(const uint8_t* data, size_t size);

  uint32_t Version() const
  {
    return m_signature;
  }
  uint32_t Width() const
  {
    return m_width;
  }
  uint32_t Height() const
  {
    return m_height;
  }
  uint32_t FrameCount() const
  {
    return m_frames;
  }
  double MsPerFrame() const
  {
    return m_msPerFrame;
  }
  uint32_t Flags() const
  {
    return m_flags;
  }
  const AudioTrackInfo& Track(int i) const
  {
    return m_tracks[i];
  }
  bool IsKeyFrame(uint32_t frame) const
  {
    return frame < m_frames && (m_frameSizes[frame] & 1) != 0;
  }

  // The picture all frames are decoded into (width * height, one index per pixel), the current palette, and the frame
  // most recently decoded. Frames must be decoded in order from a key frame; Seek() does that.
  const uint8_t* Picture() const
  {
    return m_picture.data();
  }
  const Palette& CurrentPalette() const
  {
    return m_palette;
  }
  bool PaletteChanged() const
  {
    return m_paletteChanged;
  }
  // Bounding box (in pixels) of the blocks the last video decode changed; empty width means nothing changed.
  struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
  };
  Rect ChangedRect() const
  {
    return m_changed;
  }

  // The first 0x68 bytes of the file: RAD's Smack structure starts with a copy of them.
  const uint8_t* RawHeader() const
  {
    return m_data;
  }
  // Applies the palette update of `frame` ahead of decoding it (a player shows the new palette before the frame's
  // pixels). DecodeFrame() then does not apply it a second time.
  bool PreparePalette(uint32_t frame);
  // One byte per 4x4 block (row-major), non-zero for the blocks the last video decode wrote.
  const std::vector<uint8_t>& ChangedBlocks() const
  {
    return m_changedBlocks;
  }
  int BlocksPerRow() const
  {
    return static_cast<int>(m_width / 4);
  }

  // Decodes the palette part and the video of `frame` (which must be the frame after the last decoded one, or a key
  // frame). The audio chunks of that frame become available through TakeAudio().
  bool DecodeFrame(uint32_t frame);
  // The palette update of `frame` only (without touching the picture): what the player applies before the frame is
  // drawn. Rewinds to `frame` after decoding forward from the previous key frame.
  bool Seek(uint32_t frame);
  int64_t LastDecoded() const
  {
    return m_lastDecoded;
  }

  // PCM of the last decoded frame for a track (empty if the frame has none): 8-bit unsigned or 16-bit signed little
  // endian, interleaved when stereo.
  const std::vector<uint8_t>& TakeAudio(int track) const
  {
    return m_audio[track];
  }

  private:
  Movie() = default;
  bool Parse();
  bool ReadTrees();
  void DecodePaletteChunk(const uint8_t* p, size_t size);
  void DecodeVideo(const uint8_t* p, size_t size);
  void DecodeAudio(int track, const uint8_t* p, size_t size);
  const uint8_t* FrameData(uint32_t frame, size_t* size) const;

  struct Impl;
  Impl* m_impl = nullptr;
  mw::res::MappedFile* m_file = nullptr;
  const uint8_t* m_data = nullptr;
  size_t m_size = 0;

  uint32_t m_signature = 0, m_width = 0, m_height = 0, m_frames = 0, m_flags = 0;
  double m_msPerFrame = 100.0;
  AudioTrackInfo m_tracks[kAudioTracks];
  std::vector<uint32_t> m_frameSizes;
  std::vector<uint8_t> m_frameTypes;
  std::vector<size_t> m_frameOffsets;
  std::vector<uint8_t> m_picture;
  Palette m_palette;
  bool m_paletteChanged = false;
  int64_t m_palettePrepared = -1;
  std::vector<uint8_t> m_changedBlocks;
  Rect m_changed;
  int64_t m_lastDecoded = -1;
  mutable std::vector<uint8_t> m_audio[kAudioTracks];
};

} // namespace mwsmk
