#include <stdio.h>

#include "gdi_ops.h"

void WriteDumps(const std::vector<Dump>& dumps, FILE* f) {
    for (const Dump& d : dumps) {
        fprintf(f, "%s %d %d\n", d.name.c_str(), d.width, d.height);
        for (int y = 0; y < d.height; ++y) {
            for (int x = 0; x < d.width; ++x) fprintf(f, "%02x", d.pixels[static_cast<size_t>(y) * d.width + x]);
            fputc('\n', f);
        }
    }
}

bool ReadDumps(FILE* f, std::vector<Dump>* dumps) {
    char name[128];
    int w, h;
    while (fscanf(f, "%127s %d %d", name, &w, &h) == 3) {
        Dump d;
        d.name = name;
        d.width = w;
        d.height = h;
        d.pixels.resize(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y) {
            char line[1024];
            if (fscanf(f, "%1023s", line) != 1) return false;
            for (int x = 0; x < w; ++x) {
                unsigned v;
                sscanf(line + 2 * x, "%2x", &v);
                d.pixels[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(v);
            }
        }
        dumps->push_back(d);
    }
    return true;
}
