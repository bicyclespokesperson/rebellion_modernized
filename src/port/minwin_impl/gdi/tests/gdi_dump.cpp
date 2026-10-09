// gdi_dump <out.txt> [filter]: runs the GDI scenarios and writes the resulting surfaces (see gdi_ops.h).
#include <stdio.h>

#include "gdi_ops.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: gdi_dump out.txt [filter]\n");
        return 2;
    }
    std::vector<Dump> dumps;
    RunGdiScenarios(argc > 2 ? argv[2] : nullptr, &dumps);
    FILE* f = fopen(argv[1], "wb");
    if (!f) return 1;
    WriteDumps(dumps, f);
    fclose(f);
    printf("%zu scenarios\n", dumps.size());
    return 0;
}
