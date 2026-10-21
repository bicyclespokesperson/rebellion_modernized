#include "smk_movie.h"

#include <cstring>

#include "../res/mapped_file.h"

namespace mwsmk
{

namespace
{

uint32_t Rd32(const uint8_t* p)
{
  return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Least-significant-bit-first bit reader; reads past the end give zeros.
class BitReader
{
  public:
  BitReader(const uint8_t* data, size_t size)
      : m_data(data)
      , m_size(size)
  {
  }
  int Bit()
  {
    size_t byte = m_pos >> 3;
    int bit = byte < m_size ? (m_data[byte] >> (m_pos & 7)) & 1 : 0;
    ++m_pos;
    return bit;
  }
  uint32_t Bits(int n)
  {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) {
      v |= static_cast<uint32_t>(Bit()) << i;
    }
    return v;
  }
  size_t BitPosition() const
  {
    return m_pos;
  }
  bool Overrun() const
  {
    return (m_pos >> 3) > m_size + 8;
  }

  private:
  const uint8_t* m_data;
  size_t m_size;
  size_t m_pos = 0;
};

// A binary tree whose leaves carry a value. Internal nodes are written as a 1 bit followed by the two subtrees; a leaf
// is a 0 bit followed by `leafBits` bits of value (small trees, 8-bit values) or by two codes from the small trees (the
// big trees).
struct Tree {
  struct Node {
    int child[2] = {-1, -1}; // -1 on a leaf
    int leaf = -1;           // leaf id on a leaf
  };
  std::vector<Node> nodes;
  std::vector<int32_t> leafValues; // by leaf id; the escape leaves of a big tree double as cache slots
  int slots[3] = {-1, -1, -1};
  bool present = false;

  int Walk(BitReader& br) const
  { // returns the leaf id
    if (nodes.empty()) {
      return -1;
    }
    int n = 0;
    while (nodes[n].leaf < 0) {
      int next = nodes[n].child[br.Bit()];
      if (next < 0) {
        return -1;
      }
      n = next;
    }
    return nodes[n].leaf;
  }
  int Value(BitReader& br) const
  { // small tree
    int id = Walk(br);
    return id < 0 ? 0 : leafValues[id];
  }
};

constexpr int kMaxLeaves = 1 << 16;

// Small tree: 8-bit leaf values.
bool ReadSmallTree(BitReader& br, Tree* t, int depth = 0)
{
  int index = static_cast<int>(t->nodes.size());
  if (index > 2 * kMaxLeaves || depth > 64 || br.Overrun()) {
    return false;
  }
  t->nodes.emplace_back();
  if (!br.Bit()) {
    t->nodes[index].leaf = static_cast<int>(t->leafValues.size());
    t->leafValues.push_back(static_cast<int32_t>(br.Bits(8)));
    return true;
  }
  int left = static_cast<int>(t->nodes.size());
  if (!ReadSmallTree(br, t, depth + 1)) {
    return false;
  }
  int right = static_cast<int>(t->nodes.size());
  if (!ReadSmallTree(br, t, depth + 1)) {
    return false;
  }
  t->nodes[index].child[0] = left;
  t->nodes[index].child[1] = right;
  return true;
}

struct BigTreeContext {
  const Tree* low;
  const Tree* high;
  uint32_t escapes[3];
};

bool ReadBigNode(BitReader& br, Tree* t, const BigTreeContext& ctx, int depth = 0)
{
  int index = static_cast<int>(t->nodes.size());
  if (index > 2 * kMaxLeaves || depth > 64 || br.Overrun()) {
    return false;
  }
  t->nodes.emplace_back();
  if (!br.Bit()) {
    auto lo = static_cast<uint32_t>(ctx.low->Value(br));
    auto hi = static_cast<uint32_t>(ctx.high->Value(br));
    uint32_t value = lo | (hi << 8);
    int id = static_cast<int>(t->leafValues.size());
    t->nodes[index].leaf = id;
    for (int k = 0; k < 3; ++k) {
      if (value == ctx.escapes[k]) {
        t->slots[k] = id;
        value = 0;
        break;
      }
    }
    t->leafValues.push_back(static_cast<int32_t>(value));
    return true;
  }
  int left = static_cast<int>(t->nodes.size());
  if (!ReadBigNode(br, t, ctx, depth + 1)) {
    return false;
  }
  int right = static_cast<int>(t->nodes.size());
  if (!ReadBigNode(br, t, ctx, depth + 1)) {
    return false;
  }
  t->nodes[index].child[0] = left;
  t->nodes[index].child[1] = right;
  return true;
}

// One of MMAP / MCLR / FULL / TYPE.
bool ReadBigTree(BitReader& br, Tree* out)
{
  out->present = br.Bit() != 0;
  if (!out->present) {
    return true;
  }
  Tree low, high; // each is preceded by its own presence bit; an absent tree always yields 0
  if (br.Bit()) {
    if (!ReadSmallTree(br, &low)) {
      return false;
    }
    br.Bit();
  }
  if (br.Bit()) {
    if (!ReadSmallTree(br, &high)) {
      return false;
    }
    br.Bit();
  }
  BigTreeContext ctx;
  ctx.low = &low;
  ctx.high = &high;
  for (unsigned int& escape : ctx.escapes) {
    escape = br.Bits(16);
  }
  if (!ReadBigNode(br, out, ctx)) {
    return false;
  }
  br.Bit();
  return true;
}

// Decodes one value through a big tree and updates the three-entry cache held in its escape leaves.
int DecodeBig(BitReader& br, Tree* t)
{
  if (!t->present) {
    return 0;
  }
  int id = t->Walk(br);
  if (id < 0) {
    return 0;
  }
  int v = t->leafValues[id];
  if (t->slots[0] >= 0 && t->slots[1] >= 0 && t->slots[2] >= 0 && v != t->leafValues[t->slots[0]]) {
    t->leafValues[t->slots[2]] = t->leafValues[t->slots[1]];
    t->leafValues[t->slots[1]] = t->leafValues[t->slots[0]];
    t->leafValues[t->slots[0]] = v;
  }
  return v;
}

void ResetCache(Tree* t)
{
  if (!t->present) {
    return;
  }
  for (int s : t->slots) {
    if (s >= 0) {
      t->leafValues[s] = 0;
    }
  }
}

constexpr int kBlockRuns[64]
    = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,  17,  18,  19,   20,  21, 22,
       23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,  39,  40,  41,   42,  43, 44,
       45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048};

// 6-bit palette component to 8 bits.
uint8_t Expand6(uint32_t v)
{
  return static_cast<uint8_t>((v << 2) | (v >> 4));
}

} // namespace

struct Movie::Impl {
  Tree mmap, mclr, full, type;
  std::vector<uint8_t> treeData;
};

Movie::~Movie()
{
  delete m_impl;
  delete m_file;
}

Movie* Movie::Open(const std::string& path)
{
  mw::res::MappedFile* file = mw::res::MappedFile::Open(path);
  if (!file) {
    return nullptr;
  }
  auto* m = new Movie;
  m->m_file = file;
  m->m_data = file->data();
  m->m_size = file->size();
  if (!m->Parse()) {
    delete m;
    return nullptr;
  }
  return m;
}

Movie* Movie::FromMemory(const uint8_t* data, size_t size)
{
  auto* m = new Movie;
  m->m_data = data;
  m->m_size = size;
  if (!m->Parse()) {
    delete m;
    return nullptr;
  }
  return m;
}

bool Movie::Parse()
{
  if (m_size < 0x68 || m_data[0] != 'S' || m_data[1] != 'M' || m_data[2] != 'K' || m_data[3] != '2') {
    return false;
  }
  m_signature = Rd32(m_data);
  m_width = Rd32(m_data + 4);
  m_height = Rd32(m_data + 8);
  m_frames = Rd32(m_data + 12);
  auto rate = static_cast<int32_t>(Rd32(m_data + 16));
  m_flags = Rd32(m_data + 20);
  if (m_width == 0 || m_height == 0 || m_width > 4096 || m_height > 4096 || m_frames == 0 || m_frames > 0xffffff) {
    return false;
  }
  if (rate > 0) {
    m_msPerFrame = rate;
  } else if (rate < 0) {
    m_msPerFrame = -rate / 100.0;
  } else {
    m_msPerFrame = 100.0;
  }
  if (m_flags & 1) {
    ++m_frames; // the ring frame is stored as an extra frame
  }
  for (int i = 0; i < kAudioTracks; ++i) {
    uint32_t rateFlags = Rd32(m_data + 0x48 + 4 * i);
    AudioTrackInfo& t = m_tracks[i];
    t.maxBytes = Rd32(m_data + 0x18 + 4 * i);
    t.sampleRate = rateFlags & 0xffffff;
    t.packed = (rateFlags & 0x80000000u) != 0;
    t.present = (rateFlags & 0x40000000u) != 0 && t.sampleRate != 0;
    t.bits16 = (rateFlags & 0x20000000u) != 0;
    t.stereo = (rateFlags & 0x10000000u) != 0;
  }
  size_t pos = 0x68;
  if (pos + 5ull * m_frames > m_size) {
    return false;
  }
  m_frameSizes.resize(m_frames);
  m_frameTypes.resize(m_frames);
  for (uint32_t i = 0; i < m_frames; ++i) {
    m_frameSizes[i] = Rd32(m_data + pos + 4 * i);
  }
  pos += 4ull * m_frames;
  memcpy(m_frameTypes.data(), m_data + pos, m_frames);
  pos += m_frames;
  uint32_t treesSize = Rd32(m_data + 0x34);
  if (pos + treesSize > m_size) {
    return false;
  }
  m_impl = new Impl;
  m_impl->treeData.assign(m_data + pos, m_data + pos + treesSize);
  pos += treesSize;
  m_frameOffsets.resize(m_frames + 1);
  m_frameOffsets[0] = pos;
  for (uint32_t i = 0; i < m_frames; ++i) {
    m_frameOffsets[i + 1] = m_frameOffsets[i] + (m_frameSizes[i] & ~3u);
  }
  if (m_frameOffsets[m_frames] > m_size) {
    return false;
  }
  if (!ReadTrees()) {
    return false;
  }
  m_picture.assign(static_cast<size_t>(m_width) * m_height, 0);
  return true;
}

bool Movie::ReadTrees()
{
  BitReader br(m_impl->treeData.data(), m_impl->treeData.size());
  return ReadBigTree(br, &m_impl->mmap) && ReadBigTree(br, &m_impl->mclr) && ReadBigTree(br, &m_impl->full)
         && ReadBigTree(br, &m_impl->type);
}

const uint8_t* Movie::FrameData(uint32_t frame, size_t* size) const
{
  *size = m_frameOffsets[frame + 1] - m_frameOffsets[frame];
  return m_data + m_frameOffsets[frame];
}

void Movie::DecodePaletteChunk(const uint8_t* p, size_t size)
{
  // The first byte is the chunk length in four-byte units, itself included; the update follows.
  const uint8_t* end = p + size;
  const uint8_t* q = p + 1;
  Palette old = m_palette;
  int i = 0;
  while (i < 256 && q < end) {
    uint8_t t = *q++;
    if (t & 0x80) {
      i += (t & 0x7f) + 1;
    } else if (t & 0x40) {
      int count = (t & 0x3f) + 1;
      int from = q < end ? *q++ : 0;
      for (int k = 0; k < count && i < 256; ++k, ++i, ++from) {
        if (from < 256) {
          memcpy(&m_palette.rgb[i * 3], &old.rgb[from * 3], 3);
        }
      }
    } else {
      if (q + 2 > end) {
        break;
      }
      m_palette.rgb[i * 3] = Expand6(t & 0x3f);
      m_palette.rgb[i * 3 + 1] = Expand6(*q++ & 0x3f);
      m_palette.rgb[i * 3 + 2] = Expand6(*q++ & 0x3f);
      ++i;
    }
  }
}

void Movie::DecodeVideo(const uint8_t* p, size_t size)
{
  BitReader br(p, size);
  Impl& d = *m_impl;
  ResetCache(&d.mmap);
  ResetCache(&d.mclr);
  ResetCache(&d.full);
  ResetCache(&d.type);
  const int bw = static_cast<int>(m_width / 4), bh = static_cast<int>(m_height / 4);
  const int blocks = bw * bh;
  const int stride = static_cast<int>(m_width);
  uint8_t* pic = m_picture.data();
  int minBx = bw, minBy = bh, maxBx = -1, maxBy = -1;
  m_changedBlocks.assign(static_cast<size_t>(blocks), 0);
  auto touch = [&](int blk) {
    m_changedBlocks[static_cast<size_t>(blk)] = 1;
    int bx = blk % bw, by = blk / bw;
    if (bx < minBx) {
      minBx = bx;
    }
    if (bx > maxBx) {
      maxBx = bx;
    }
    if (by < minBy) {
      minBy = by;
    }
    if (by > maxBy) {
      maxBy = by;
    }
  };
  int blk = 0;
  while (blk < blocks) {
    int type = DecodeBig(br, &d.type);
    int run = kBlockRuns[(type >> 2) & 0x3f];
    switch (type & 3) {
      case 0: // two colours and a 16-bit mask
        while (run-- && blk < blocks) {
          int colours = DecodeBig(br, &d.mclr);
          int map = DecodeBig(br, &d.mmap);
          uint8_t* out = pic + (blk / bw) * stride * 4 + (blk % bw) * 4;
          int hi = (colours >> 8) & 255, lo = colours & 255;
          for (int y = 0; y < 4; ++y, out += stride) {
            for (int x = 0; x < 4; ++x) {
              out[x] = static_cast<uint8_t>((map >> x) & 1 ? hi : lo);
            }
            map >>= 4;
          }
          touch(blk);
          ++blk;
        }
        break;
      case 1: // sixteen pixels coded individually, two at a time (the right pair of a row first)
        while (run-- && blk < blocks) {
          uint8_t* out = pic + (blk / bw) * stride * 4 + (blk % bw) * 4;
          for (int y = 0; y < 4; ++y, out += stride) {
            int right = DecodeBig(br, &d.full);
            out[2] = static_cast<uint8_t>(right & 255);
            out[3] = static_cast<uint8_t>(right >> 8);
            int left = DecodeBig(br, &d.full);
            out[0] = static_cast<uint8_t>(left & 255);
            out[1] = static_cast<uint8_t>(left >> 8);
          }
          touch(blk);
          ++blk;
        }
        break;
      case 2: // unchanged
        blk += run;
        break;
      case 3:
        { // solid colour
          int colour = type >> 8;
          while (run-- && blk < blocks) {
            uint8_t* out = pic + (blk / bw) * stride * 4 + (blk % bw) * 4;
            for (int y = 0; y < 4; ++y, out += stride) {
              memset(out, colour, 4);
            }
            touch(blk);
            ++blk;
          }
          break;
        }
    }
    if (br.Overrun()) {
      break;
    }
  }
  if (maxBx < 0) {
    m_changed = Rect();
  } else {
    m_changed.x = minBx * 4;
    m_changed.y = minBy * 4;
    m_changed.w = (maxBx - minBx + 1) * 4;
    m_changed.h = (maxBy - minBy + 1) * 4;
  }
}

void Movie::DecodeAudio(int track, const uint8_t* p, size_t size)
{
  std::vector<uint8_t>& out = m_audio[track];
  out.clear();
  const AudioTrackInfo& info = m_tracks[track];
  if (size < 4) {
    return;
  }
  uint32_t unpacked = Rd32(p);
  p += 4;
  size -= 4;
  if (!info.packed) {
    out.assign(p, p + size);
    return;
  }
  if (unpacked > (64u << 20)) {
    return;
  }
  BitReader br(p, size);
  if (!br.Bit()) {
    return; // no sound in this chunk
  }
  bool stereo = br.Bit() != 0;
  bool bits16 = br.Bit() != 0;
  int treeCount = 1 << (bits16 + stereo);
  Tree trees[4];
  for (int i = 0; i < treeCount; ++i) {
    if (!br.Bit()) {
      continue; // an absent tree always yields 0
    }
    if (!ReadSmallTree(br, &trees[i])) {
      return;
    }
    br.Bit();
  }
  out.resize(unpacked);
  int32_t pred[2] = {0, 0};
  if (bits16) {
    for (int i = stereo; i >= 0; --i) {
      uint32_t v = br.Bits(16);
      pred[i] = static_cast<int16_t>(((v & 0xff) << 8) | (v >> 8)); // stored big endian
    }
    uint32_t total = unpacked / 2;
    uint32_t i = 0;
    auto put = [&](int32_t s) {
      if (i * 2 + 1 < out.size()) {
        out[i * 2] = static_cast<uint8_t>(s);
        out[i * 2 + 1] = static_cast<uint8_t>(s >> 8);
      }
      ++i;
    };
    for (int c = 0; c <= stereo; ++c) {
      put(pred[c]);
    }
    for (; i < total;) {
      int ch = (i & stereo) ? 1 : 0;
      int base = ch * 2;
      int lo = trees[base].Value(br), hi = trees[base + 1].Value(br);
      auto delta = static_cast<int16_t>((lo & 0xff) | ((hi & 0xff) << 8));
      pred[ch] = static_cast<int16_t>(pred[ch] + delta);
      put(pred[ch]);
    }
  } else {
    for (int i = stereo; i >= 0; --i) {
      pred[i] = static_cast<int32_t>(br.Bits(8));
    }
    uint32_t i = 0;
    for (int c = 0; c <= stereo; ++c) {
      out[i++] = static_cast<uint8_t>(pred[c]);
    }
    for (; i < unpacked;) {
      int ch = (i & stereo) ? 1 : 0;
      int delta = static_cast<int8_t>(trees[ch].Value(br));
      pred[ch] = (pred[ch] + delta) & 0xff;
      out[i++] = static_cast<uint8_t>(pred[ch]);
    }
  }
}

bool Movie::PreparePalette(uint32_t frame)
{
  if (frame >= m_frames) {
    return false;
  }
  size_t size;
  const uint8_t* p = FrameData(frame, &size);
  m_paletteChanged = false;
  if ((m_frameTypes[frame] & 1) && size > 0) {
    size_t chunk = static_cast<size_t>(p[0]) * 4;
    if (chunk > size) {
      chunk = size;
    }
    DecodePaletteChunk(p, chunk);
    m_paletteChanged = true;
  }
  m_palettePrepared = frame;
  return true;
}

bool Movie::DecodeFrame(uint32_t frame)
{
  if (frame >= m_frames) {
    return false;
  }
  size_t size;
  const uint8_t* p = FrameData(frame, &size);
  const uint8_t* end = p + size;
  uint8_t type = m_frameTypes[frame];
  if (type & 1) {
    if (p >= end) {
      return false;
    }
    size_t chunk = static_cast<size_t>(p[0]) * 4;
    if (chunk > static_cast<size_t>(end - p)) {
      chunk = static_cast<size_t>(end - p);
    }
    if (m_palettePrepared != static_cast<int64_t>(frame)) {
      DecodePaletteChunk(p, chunk);
      m_paletteChanged = true;
    }
    p += chunk;
  } else {
    m_paletteChanged = false;
  }
  m_palettePrepared = -1;
  for (int t = 0; t < kAudioTracks; ++t) {
    m_audio[t].clear();
    if (!(type & (2 << t))) {
      continue;
    }
    if (end - p < 4) {
      return false;
    }
    size_t chunk = Rd32(p);
    if (chunk < 4 || chunk > static_cast<size_t>(end - p)) {
      return false;
    }
    DecodeAudio(t, p + 4, chunk - 4);
    p += chunk;
  }
  DecodeVideo(p, static_cast<size_t>(end - p));
  m_lastDecoded = frame;
  return true;
}

bool Movie::Seek(uint32_t frame)
{
  if (frame >= m_frames) {
    return false;
  }
  if (m_lastDecoded + 1 == static_cast<int64_t>(frame)) {
    return true; // already in sequence: the caller decodes it
  }
  uint32_t start = frame;
  while (start > 0 && !IsKeyFrame(start)) {
    --start;
  }
  if (start >= frame) {
    m_lastDecoded = static_cast<int64_t>(frame) - 1;
    return true;
  }
  for (uint32_t f = start; f < frame; ++f) {
    if (!DecodeFrame(f)) {
      return false;
    }
  }
  return true;
}

} // namespace mwsmk
