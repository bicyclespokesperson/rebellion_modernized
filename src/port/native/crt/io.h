#pragma once
#include <fcntl.h>
extern "C" {
int _setmode(int fd, int mode);
}
#ifndef _O_TEXT
#define _O_TEXT 0x4000
#endif
#ifndef _O_BINARY
#define _O_BINARY 0x8000
#endif
