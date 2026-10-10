#include "formats.h"

#include <stdlib.h>
#include <string.h>

namespace mw::res {

namespace {

uint16_t Rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t Rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
int32_t RdS32(const uint8_t* p) { return static_cast<int32_t>(Rd32(p)); }

}  // namespace

bool LoadStringText(const Module& module, uint32_t id, std::string* out) {
    Entry e;
    if (!module.Find(ResId::Int(kString), ResId::Int(static_cast<uint16_t>(id / 16 + 1)), &e)) return false;
    const uint8_t* p = e.data;
    const uint8_t* end = e.data + e.size;
    for (uint32_t i = 0; i < 16; ++i) {
        if (p + 2 > end) return false;
        uint32_t n = Rd16(p);
        p += 2;
        if (p + n * 2 > end) return false;
        if (i == id % 16) {
            out->clear();
            for (uint32_t k = 0; k < n; ++k) {
                uint16_t c = Rd16(p + k * 2);
                out->push_back(c < 0x100 ? static_cast<char>(c) : '?');
            }
            return n != 0;
        }
        p += n * 2;
    }
    return false;
}

// Rows of a DIB are stored bottom-up (positive height) and padded to 4 bytes.
bool DecodeDibToRgba(const uint8_t* dib, size_t size, bool hasMask, Image* out) {
    if (size < 40 || Rd32(dib) < 40) return false;
    uint32_t headerSize = Rd32(dib);
    int width = RdS32(dib + 4), rawHeight = RdS32(dib + 8);
    int bits = Rd16(dib + 14);
    uint32_t compression = Rd32(dib + 16);
    uint32_t colorsUsed = Rd32(dib + 32);
    if (compression != 0 && !(compression == 3 && (bits == 16 || bits == 32))) return false;
    bool topDown = rawHeight < 0;
    int height = (topDown ? -rawHeight : rawHeight);
    if (hasMask) height /= 2;
    if (width <= 0 || width > 4096 || height <= 0 || height > 4096) return false;

    uint32_t paletteCount = bits <= 8 ? (colorsUsed ? colorsUsed : (1u << bits)) : 0;
    size_t paletteOffset = headerSize + (compression == 3 && headerSize == 40 ? 12 : 0);
    size_t pixelOffset = paletteOffset + paletteCount * 4;
    size_t stride = ((static_cast<size_t>(width) * bits + 31) / 32) * 4;
    size_t maskStride = ((static_cast<size_t>(width) + 31) / 32) * 4;
    size_t maskOffset = pixelOffset + stride * height;
    if (pixelOffset + stride * height > size) return false;
    bool haveMask = hasMask && maskOffset + maskStride * height <= size;

    out->width = width;
    out->height = height;
    out->rgba.assign(static_cast<size_t>(width) * height * 4, 0);
    bool anyAlpha = false;
    for (int y = 0; y < height; ++y) {
        int srcRow = topDown ? y : height - 1 - y;
        const uint8_t* row = dib + pixelOffset + stride * srcRow;
        uint8_t* dst = out->rgba.data() + static_cast<size_t>(y) * width * 4;
        for (int x = 0; x < width; ++x, dst += 4) {
            uint8_t r = 0, g = 0, b = 0, a = 255;
            if (bits <= 8) {
                int idx;
                if (bits == 8) idx = row[x];
                else if (bits == 4) idx = (row[x / 2] >> ((x & 1) ? 0 : 4)) & 15;
                else idx = (row[x / 8] >> (7 - (x & 7))) & 1;
                if (static_cast<uint32_t>(idx) < paletteCount && paletteOffset + idx * 4 + 3 < size) {
                    const uint8_t* c = dib + paletteOffset + idx * 4;
                    b = c[0];
                    g = c[1];
                    r = c[2];
                }
            } else if (bits == 24) {
                b = row[x * 3];
                g = row[x * 3 + 1];
                r = row[x * 3 + 2];
            } else if (bits == 32) {
                b = row[x * 4];
                g = row[x * 4 + 1];
                r = row[x * 4 + 2];
                a = row[x * 4 + 3];
                if (a) anyAlpha = true;
            } else if (bits == 16) {
                uint16_t v = Rd16(row + x * 2);
                r = static_cast<uint8_t>(((v >> 10) & 31) * 255 / 31);
                g = static_cast<uint8_t>(((v >> 5) & 31) * 255 / 31);
                b = static_cast<uint8_t>((v & 31) * 255 / 31);
            } else {
                return false;
            }
            if (haveMask) {
                const uint8_t* mrow = dib + maskOffset + maskStride * (topDown ? y : height - 1 - y);
                bool masked = (mrow[x / 8] >> (7 - (x & 7))) & 1;
                if (masked) a = (bits == 1 && r) ? 255 : 0;
            }
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;
            dst[3] = a;
        }
    }
    if (bits == 32 && !anyAlpha && !haveMask) {
        for (size_t i = 3; i < out->rgba.size(); i += 4) out->rgba[i] = 255;
    } else if (bits == 32 && !anyAlpha && haveMask) {
        for (int y = 0; y < height; ++y) {
            const uint8_t* mrow = dib + maskOffset + maskStride * (topDown ? y : height - 1 - y);
            for (int x = 0; x < width; ++x)
                out->rgba[(static_cast<size_t>(y) * width + x) * 4 + 3] = ((mrow[x / 8] >> (7 - (x & 7))) & 1) ? 0 : 255;
        }
    }
    return true;
}

namespace {

struct GroupEntry {
    int width, height, bits;
    uint16_t id;
};

bool ReadGroup(const Module& module, uint16_t groupType, const ResId& name, bool isCursor, std::vector<GroupEntry>* out) {
    Entry e;
    if (!module.Find(ResId::Int(groupType), name, &e) || e.size < 6) return false;
    uint32_t count = Rd16(e.data + 4);
    if (6 + count * 14 > e.size) return false;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* d = e.data + 6 + i * 14;
        GroupEntry g;
        if (isCursor) {
            g.width = Rd16(d);
            g.height = Rd16(d + 2) / 2;
        } else {
            g.width = d[0] ? d[0] : 256;
            g.height = d[1] ? d[1] : 256;
        }
        g.bits = Rd16(d + 6);
        g.id = Rd16(d + 12);
        out->push_back(g);
    }
    return !out->empty();
}

bool LoadFromGroup(const Module& module, uint16_t groupType, uint16_t imageType, const ResId& name, bool isCursor, Image* out,
                   int preferredSize) {
    std::vector<GroupEntry> group;
    if (!ReadGroup(module, groupType, name, isCursor, &group)) return false;
    const GroupEntry* best = &group[0];
    for (const GroupEntry& g : group) {
        bool better = preferredSize
                          ? (abs(g.width - preferredSize) < abs(best->width - preferredSize) ||
                             (g.width == best->width && g.bits > best->bits))
                          : (g.width > best->width || (g.width == best->width && g.bits > best->bits));
        if (better) best = &g;
    }
    Entry img;
    if (!module.Find(ResId::Int(imageType), ResId::Int(best->id), &img)) return false;
    if (isCursor) {
        if (img.size < 4) return false;
        out->hotX = Rd16(img.data);
        out->hotY = Rd16(img.data + 2);
        return DecodeDibToRgba(img.data + 4, img.size - 4, true, out);
    }
    return DecodeDibToRgba(img.data, img.size, true, out);
}

}  // namespace

bool LoadCursorImage(const Module& module, const ResId& name, Image* out, int preferredSize) {
    return LoadFromGroup(module, kGroupCursor, kCursor, name, true, out, preferredSize);
}

bool LoadIconImage(const Module& module, const ResId& name, Image* out, int preferredSize) {
    return LoadFromGroup(module, kGroupIcon, kIcon, name, false, out, preferredSize);
}

bool LoadAcceleratorTable(const Module& module, const ResId& name, std::vector<Accelerator>* out) {
    Entry e;
    if (!module.Find(ResId::Int(kAccelerator), name, &e)) return false;
    out->clear();
    for (uint32_t off = 0; off + 8 <= e.size; off += 8) {
        uint8_t flags = e.data[off];
        out->push_back({static_cast<uint8_t>(flags & 0x7f), Rd16(e.data + off + 2), Rd16(e.data + off + 4)});
        if (flags & 0x80) break;
    }
    return true;
}

namespace {

size_t Align4(size_t v) { return (v + 3) & ~static_cast<size_t>(3); }

// Parses one VS_VERSIONINFO-style node starting at `p`; returns its total byte length (0 on error).
size_t ParseNode(const uint8_t* p, const uint8_t* limit, VersionBlock* node) {
    if (p + 6 > limit) return 0;
    size_t length = Rd16(p);
    size_t valueLength = Rd16(p + 2);
    bool text = Rd16(p + 4) == 1;
    if (length < 6 || p + length > limit) return 0;
    const uint8_t* end = p + length;
    const uint8_t* q = p + 6;
    node->key.clear();
    while (q + 2 <= end && Rd16(q) != 0) {
        uint16_t c = Rd16(q);
        node->key.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        q += 2;
    }
    q += 2;
    q = p + Align4(static_cast<size_t>(q - p));
    node->isText = text;
    node->valueLength = static_cast<uint32_t>(valueLength);
    size_t valueBytes = text ? valueLength * 2 : valueLength;
    node->value = valueLength ? q : nullptr;
    if (q + valueBytes > end) return 0;
    q += valueBytes;
    q = p + Align4(static_cast<size_t>(q - p));
    while (q + 6 <= end) {
        VersionBlock child;
        size_t n = ParseNode(q, end, &child);
        if (n == 0) break;
        node->children.push_back(child);
        q += Align4(n);
    }
    return length;
}

bool EqualNoCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y + 32);
        if (x != y) return false;
    }
    return true;
}

}  // namespace

bool ParseVersionInfo(const uint8_t* data, size_t size, VersionBlock* root) { return ParseNode(data, data + size, root) != 0; }

const VersionBlock* QueryVersionBlock(const VersionBlock& root, const std::string& path) {
    if (path.empty() || path == "\\") return &root;
    const VersionBlock* node = &root;
    size_t pos = 0;
    while (pos < path.size()) {
        while (pos < path.size() && path[pos] == '\\') ++pos;
        size_t next = path.find('\\', pos);
        std::string part = path.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        pos = next == std::string::npos ? path.size() : next;
        if (part.empty()) break;
        const VersionBlock* found = nullptr;
        for (const VersionBlock& c : node->children)
            if (EqualNoCase(c.key, part)) {
                found = &c;
                break;
            }
        if (!found) return nullptr;
        node = found;
    }
    return node;
}

std::string LanguageName(uint32_t id) {
    static const struct {
        uint16_t id;
        const char* name;
    } kNames[] = {
        {0x0000, "Language Neutral"},       {0x0400, "Process Default Language"}, {0x0401, "Arabic"},
        {0x0404, "Chinese (Traditional)"},  {0x0405, "Czech"},                    {0x0406, "Danish"},
        {0x0407, "German (Standard)"},      {0x0408, "Greek"},                    {0x0409, "English (United States)"},
        {0x040a, "Spanish (Traditional Sort)"}, {0x040b, "Finnish"},              {0x040c, "French (Standard)"},
        {0x040d, "Hebrew"},                 {0x040e, "Hungarian"},                {0x0410, "Italian (Standard)"},
        {0x0411, "Japanese"},               {0x0412, "Korean"},                   {0x0413, "Dutch (Standard)"},
        {0x0414, "Norwegian (Bokmal)"},     {0x0415, "Polish"},                   {0x0416, "Portuguese (Brazilian)"},
        {0x0419, "Russian"},                {0x041d, "Swedish"},                  {0x041f, "Turkish"},
        {0x0804, "Chinese (Simplified)"},   {0x0807, "German (Swiss)"},           {0x0809, "English (United Kingdom)"},
        {0x080c, "French (Belgian)"},       {0x0816, "Portuguese (Standard)"},    {0x0c07, "German (Austrian)"},
        {0x0c09, "English (Australian)"},   {0x0c0a, "Spanish (Modern Sort)"},    {0x0c0c, "French (Canadian)"},
        {0x1009, "English (Canadian)"},
    };
    for (const auto& n : kNames)
        if (n.id == id) return n.name;
    return "Unknown";
}

}  // namespace mw::res
