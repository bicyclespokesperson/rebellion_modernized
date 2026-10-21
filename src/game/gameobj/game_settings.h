#pragma once
#include <cstdint>

#include "core/stream_io.h"

// The new-game setup choices, saved with the game root (GameRoot::m_settings, +0x58). The fields are only known by
// how they are used: the setup screens fill a default-constructed GameSettings (0x4fce50), the game root compares
// it with its own copy, and m_field04 is what the setup stage machine hands to its stage 0xa.
class Crc32;

class GameSettings
{
  public:
  GameSettings();
  virtual ~GameSettings();
  virtual int Read(istream* in);
  virtual int Write(ostream* out);
  virtual int Checksum(Crc32* crc); // 0x51c8c0

  GameSettings& operator=(const GameSettings& other);
  int operator==(const GameSettings& other) const;

  int m_field04; // +0x04
  int m_field08; // +0x08
  int m_field0c; // +0x0c, defaults to 7
  int m_field10; // +0x10
};
