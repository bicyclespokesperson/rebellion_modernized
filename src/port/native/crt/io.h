#pragma once
#include <fcntl.h>
extern "C" {
int _setmode(int fd, int mode);
}
#define _O_TEXT 0x4000
#define _O_BINARY 0x8000
