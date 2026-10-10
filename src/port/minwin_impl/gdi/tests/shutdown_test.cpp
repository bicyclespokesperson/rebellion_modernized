#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/gdi32.h"

#include <stdio.h>
#include <stdlib.h>

namespace {
struct GlobalGraphicsOwner {
    HBRUSH brush = nullptr;

    ~GlobalGraphicsOwner() {
        if (!DeleteObject(brush)) {
            fprintf(stderr, "Failed to delete a graphics object during static cleanup\n");
            _Exit(1);
        }
    }
};

GlobalGraphicsOwner owner;
}

int main() {
    owner.brush = CreateSolidBrush(0x123456);
    return owner.brush ? 0 : 1;
}
