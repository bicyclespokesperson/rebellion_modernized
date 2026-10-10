// text_dump <samples.txt> <out-prefix> [font-filter]: writes <prefix>.lines.txt (metrics, extents, DrawText rectangles) and
// <prefix>.pixels.txt (rendered surfaces, gdi_ops format).
#include <stdio.h>

#include "text_ops.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: text_dump samples.txt out-prefix [font-filter]\n");
        return 2;
    }
    TextResult result;
    if (!RunTextScenarios(argv[1], argc > 3 ? argv[3] : nullptr, &result)) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    std::string prefix = argv[2];
    FILE* lines = fopen((prefix + ".lines.txt").c_str(), "wb");
    FILE* pixels = fopen((prefix + ".pixels.txt").c_str(), "wb");
    if (!lines || !pixels) return 1;
    for (const std::string& l : result.lines) fprintf(lines, "%s\n", l.c_str());
    WriteDumps(result.pixels, pixels);
    fclose(lines);
    fclose(pixels);
    printf("%zu lines, %zu surfaces\n", result.lines.size(), result.pixels.size());
    return 0;
}
