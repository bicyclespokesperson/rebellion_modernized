#pragma once
// The leaf payload classes of the notifications (0x539290-0x539980, VT 0x661300-0x661380): KeyedNotifPayload plus two
// words of data. They are abstract (slots 3, 6 and 7 stay pure; the UI notification handlers 0x43a2d0-0x440ea0 derive
// from them and provide those), so the classes only own their data and the stream functions the handlers call. Read
// and Write are plain members, not virtual. PostPayload* (event_post.h) fills the data of a freshly built payload.
#include <cstdint>

#include "core/game_string.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/notif_payload.h"
#include "gameobj/object_key.h"

// VT 0x661300: two object keys. No Read/Write of its own.
class KeyPairPayload : public KeyedNotifPayload
{
public:
  KeyPairPayload(); // 0x539290
  ~KeyPairPayload() override; // 0x5392f0 (deleting), 0x539310

  ObjectKey m_keyA; // +0x24
  ObjectKey m_keyB; // +0x28
};

// VT 0x661320: two ints.
class IntPairPayload : public KeyedNotifPayload
{
public:
  IntPairPayload(); // 0x539370
  ~IntPairPayload() override; // 0x539390 (deleting), 0x5393b0

  int ReadPayload(istream* in); // 0x5393c0
  int WritePayload(ostream* out); // 0x539400

  int m_valueA; // +0x24
  int m_valueB; // +0x28
};

// VT 0x661340: four ints.
class IntQuadPayload : public KeyedNotifPayload
{
public:
  IntQuadPayload(); // 0x539440
  ~IntQuadPayload() override; // 0x539460 (deleting), 0x539480

  int ReadPayload(istream* in); // 0x539490
  int WritePayload(ostream* out); // 0x5394f0

  int m_valueA; // +0x24
  int m_valueB; // +0x28
  int m_valueC; // +0x2c
  int m_valueD; // +0x30
};

// VT 0x661360: two strings.
class StringPairPayload : public KeyedNotifPayload
{
public:
  StringPairPayload(); // 0x539550
  ~StringPairPayload() override; // 0x5395b0 (deleting), 0x5395d0

  int ReadPayload(istream* in); // 0x539630
  int WritePayload(ostream* out); // 0x539660

  std::string m_textA; // +0x24
  std::string m_textB; // +0x30
};

// VT 0x661380: two 4-byte keys that start out as 0 and are saved the way an ObjectKey is.
class NullableKeyPairPayload : public KeyedNotifPayload
{
public:
  NullableKeyPairPayload(); // 0x539690
  ~NullableKeyPairPayload() override; // 0x5396f0 (deleting), 0x539710

  int ReadPayload(istream* in); // 0x539770
  int WritePayload(ostream* out); // 0x5397a0

  NullableKey m_keyA; // +0x24
  NullableKey m_keyB; // +0x28
};

// 0x1c bytes, no vtable: three ints and two running checksums. Built, copied and saved by the game manager
// (0x51cd20-0x51d874); Load/Save have void-returning forwarders (Read 0x539960, Write 0x539970) that it calls.
class ChecksumRecord
{
public:
  ChecksumRecord(); // 0x5397d0
  ~ChecksumRecord(); // 0x539830
  ChecksumRecord& operator=(const ChecksumRecord& other); // 0x539880

  int Load(istream* in); // 0x5398c0
  int Save(ostream* out); // 0x539910
  int Read(istream* in); // 0x539960
  int Write(ostream* out); // 0x539970

  int m_value00; // +0x00
  int m_value04; // +0x04
  int m_value08; // +0x08
  Crc32 m_crcA; // +0x0c
  Crc32 m_crcB; // +0x14
};
