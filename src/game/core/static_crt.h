#pragma once
// The game links the CRT statically: include this before any CRT header so that its functions are declared
// without __declspec(dllimport) and resolve to the game's own copies.
#ifndef _ACRTIMP
#define _ACRTIMP
#endif
#ifndef _CRTIMP
#define _CRTIMP
#endif
