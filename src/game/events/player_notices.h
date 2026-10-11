#pragma once
class AiNoticeGroup;
// The concrete UiNotice classes (0x48aac0-0x49c6a0): one per kind of player notice. A notice is built by the
// controller of a side from the notification payloads that describe an event (the constructor taking the payload
// list hands each payload to the notice through slots 5-9, see NotifPayload::DispatchToSink); the notice keeps the
// resulting texts, pictures and sounds for the message index and the advisor. Names are numbered by GetKind().
#include <cstdint>

#include "core/containers.h"
#include "gameobj/object_key.h"
#include "orders/order_base.h"
#include "strategy_ui/advisor_notices.h"

// kind 44: three texts for the events 0x129-0x12b
class Notice44 : public UiTextNotice
{
public:
  Notice44(); // 0x48aac0
  Notice44(const AiNoticeGroup* payloads, int owner); // 0x48ab40
  ~Notice44() override; // 0x48abb0 (deleting 0x48ab20)

  int GetKind() override; // 0x48ad20
  int GetClass() override; // 0x48ad30
  int Virtual11() override; // 0x48c2a0
  UiNotice* Virtual10() override; // 0x48acc0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x49c0b0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48abc0
  void Read(istream* in) override; // 0x499dc0
  void Write(ostream* out) override; // 0x499dd0

  int m_f64; // +0x64
};

// kind 45: the texts for event 0x12c
class Notice45 : public UiTextNotice
{
public:
  Notice45(); // 0x48ad40
  Notice45(const AiNoticeGroup* payloads, int owner); // 0x48adc0
  ~Notice45() override; // 0x48ae30 (deleting 0x48ada0)

  int GetKind() override; // 0x48af40
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override; // 0x48af30
  UiNotice* Virtual10() override; // 0x48aed0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48ae40

  int m_f64; // +0x64
};

// kind 43: two objects of a transfer (event 0x306): their texts and the system above them
class Notice43 : public UiTextNotice
{
public:
  Notice43(); // 0x48af50
  Notice43(const AiNoticeGroup* payloads, int owner); // 0x48afd0
  ~Notice43() override; // 0x48b040 (deleting 0x48afb0)

  int GetKind() override; // 0x48b260
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48b200
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x48b050
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Read(istream* in) override; // 0x48b960
  void Write(ostream* out) override; // 0x48b980

  int m_f64; // +0x64
};

// kind 42: a record (research/production type) became available
class Notice42 : public UiTextNotice
{
public:
  Notice42(); // 0x48b270
  Notice42(int owner, const IdNode* record); // 0x48b2e0
  ~Notice42() override; // 0x48b3c0 (deleting 0x48b2c0)

  int GetKind() override; // 0x48b5d0
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override
  {
    return 512;
  } // shared 0x48b450
  UiNotice* Virtual10() override; // 0x48b3e0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int IsStale() override
  {
    return 0;
  } // shared 0x6158b0
  int Virtual16(const AiNoticeGroup* payloads) override
  {
    return 0;
  } // shared 0x48b440
};

// kind 41: event 0x231: the texts for a side that can name a general
class Notice41 : public UiTextNotice
{
public:
  Notice41(); // 0x48b5e0
  Notice41(const AiNoticeGroup* payloads, int owner); // 0x48b660
  ~Notice41() override; // 0x48b6d0 (deleting 0x48b640)

  int GetKind() override; // 0x48b850
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48b7f0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48b6e0

  int m_f64; // +0x64
};

// kind 34: event 0x305: a character met another one
class Notice34 : public UiTextNotice
{
public:
  Notice34(); // 0x48b860
  Notice34(const AiNoticeGroup* payloads, int owner); // 0x48b8e0
  ~Notice34() override; // 0x48b950 (deleting 0x48b8c0)

  int GetKind() override; // 0x48bc70
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override; // 0x48bc60
  UiNotice* Virtual10() override; // 0x48bb90
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x48b9a0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int Virtual16(const AiNoticeGroup* payloads) override; // 0x48bbf0
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
  } // shared 0x48b960
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
  } // shared 0x48b980

  int m_f64; // +0x64
};

// kind 33: event 0x160: a manufacturing manager of a system finished or is idle
class Notice33 : public UiTextNotice
{
public:
  Notice33(); // 0x48bc80
  Notice33(const AiNoticeGroup* payloads, int owner); // 0x48bd10
  ~Notice33() override; // 0x48bd90 (deleting 0x48bcf0)

  int GetKind() override; // 0x48c310
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 8;
  } // shared 0x48c2a0
  UiNotice* Virtual10() override; // 0x48c240
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48be60
  int IsStale() override; // 0x48c2b0
  void Read(istream* in) override; // 0x48bde0
  void Write(ostream* out) override; // 0x48be20

  int m_f64; // +0x64
  int m_f68; // +0x68
  ObjectKey m_hostKey; // +0x6c
};

// kind 40: event 0x200: a character leads a mission
class Notice40 : public UiTextNotice
{
public:
  Notice40(); // 0x48c320
  Notice40(const AiNoticeGroup* payloads, int owner); // 0x48c3a0
  ~Notice40() override; // 0x48c410 (deleting 0x48c380)

  int GetKind() override; // 0x48c8b0
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48c850
  void Virtual05(int kind, intptr_t payload) override; // 0x48c4c0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Read(istream* in) override; // 0x48c460
  void Write(ostream* out) override; // 0x48c490

  int m_f64; // +0x64
  ObjectKey m_subjectKey; // +0x68
};

// kind 39: event 0x361: two characters of different sides
class Notice39 : public UiTextNotice
{
public:
  Notice39(); // 0x48c8c0
  Notice39(const AiNoticeGroup* payloads, int owner); // 0x48c940
  ~Notice39() override; // 0x48c9b0 (deleting 0x48c920)

  int GetKind() override; // 0x48cd70
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48cd10
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x48c9c0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
};

// kind 38: event 0x230: the Emperor's side learns of the Death Star finale
class Notice38 : public UiTextNotice
{
public:
  Notice38(); // 0x48cd80
  Notice38(const AiNoticeGroup* payloads, int owner); // 0x48ce00
  ~Notice38() override; // 0x48ce70 (deleting 0x48cde0)

  int GetKind() override; // 0x48cfe0
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48cf80
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48ce80

  int m_f64; // +0x64
};

// kind 37: events 0x210 and 0x221: Luke on a mission
class Notice37 : public UiTextNotice
{
public:
  Notice37(); // 0x48cff0
  Notice37(const AiNoticeGroup* payloads, int owner); // 0x48d070
  ~Notice37() override; // 0x48d0e0 (deleting 0x48d050)

  int GetKind() override; // 0x48d360
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48d300
  void Virtual05(int kind, intptr_t payload) override; // 0x48d190
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48d0f0

  int m_f64; // +0x64
};

// kind 35: event 0x305 for a capital ship that meets a system object
class Notice35 : public UiKeyNotice
{
public:
  Notice35(); // 0x48d370
  Notice35(const AiNoticeGroup* payloads, int owner); // 0x48d3e0
  ~Notice35() override; // 0x48d440 (deleting 0x48d3c0)

  int GetKind() override; // 0x48d630
  int GetClass() override; // 0x48d640
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48d5d0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x48d470
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Read(istream* in) override; // 0x48d450
  void Write(ostream* out) override; // 0x48d460
};

// kind 36: the story characters of both sides meet (events 2, 0x211, 0x220, 0x360)
class Notice36 : public UiTextNotice
{
public:
  Notice36(); // 0x48d650
  Notice36(const AiNoticeGroup* payloads, int owner); // 0x48d6f0
  ~Notice36() override; // 0x48d780 (deleting 0x48d6d0)

  int GetKind() override; // 0x48e480
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48dad0
  void Virtual05(int kind, intptr_t payload) override; // 0x48d9a0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48d7f0
  void SetPriority(); // 0x48db30
  void FillFinalDuel(); // 0x48dc50
  void FillStoryPair(); // 0x48df20
  void FillStoryEncounter(); // 0x48e1d0
  void FillLukeAlone(); // 0x48e3c0

  PersistentInt m_state; // +0x64 bit 0x1: seen 0x211, 0x10: seen 0x220, 0x10000000: a character was not found
  ObjectKey m_firstKey; // +0x68 the character of the Alliance
  ObjectKey m_secondKey; // +0x6c the character of the Empire
};

// kind 31: event 0x128
class Notice31 : public UiKeyNotice
{
public:
  Notice31(); // 0x48e490
  Notice31(const AiNoticeGroup* payloads, int owner); // 0x48e500
  ~Notice31() override; // 0x48e570 (deleting 0x48e4e0)

  int GetKind() override; // 0x48e680
  int GetClass() override
  {
    return 6;
  } // shared 0x48d640
  int Virtual11() override
  {
    return 0x400;
  } // shared 0x48e670
  UiNotice* Virtual10() override; // 0x48e610
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48e5a0
  void Read(istream* in) override; // 0x48e580
  void Write(ostream* out) override; // 0x48f770

  int m_f64; // +0x64
};

// kind 32: events 2, 0x141, 0x143, 0x154, 0x155, 0x302: a system being besieged or blockaded
class Notice32 : public UiTextNotice
{
public:
  Notice32(); // 0x48e690
  Notice32(const AiNoticeGroup* payloads, int owner); // 0x48e720
  ~Notice32() override; // 0x48e7a0 (deleting 0x48e700)

  int GetKind() override; // 0x48ec70
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 4;
  } // shared 0x4047d0
  UiNotice* Virtual10() override; // 0x48ec10
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x48e890
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48e940
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x48e840
  void Read(istream* in) override; // 0x48e800
  void Write(ostream* out) override; // 0x48e820

  PersistentInt m_state; // +0x64 bit 1: attackers outnumber, 2: blockade, 4: siege, 8: cleared, 0x10: unit given
  std::string m_name; // +0x68
};

// kind 29: event 0x1e1: a character gained the Force
class Notice29 : public UiTextNotice
{
public:
  Notice29(); // 0x48ec80
  Notice29(const AiNoticeGroup* payloads, int owner); // 0x48ed00
  ~Notice29() override; // 0x48ed70 (deleting 0x48ece0)

  int GetKind() override; // 0x48f0d0
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48f070
  void Virtual05(int kind, intptr_t payload) override; // 0x48ed80
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
};

// kind 28: event 0x362: two characters meet (one can train the other)
class Notice28 : public UiTextNotice
{
public:
  Notice28(); // 0x48f0e0
  Notice28(const AiNoticeGroup* payloads, int owner); // 0x48f160
  ~Notice28() override; // 0x48f1d0 (deleting 0x48f140)

  int GetKind() override; // 0x48f660
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48f600
  void Virtual05(int kind, intptr_t payload) override; // 0x48f1e0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int IsStale() override
  {
    return 0;
  } // shared 0x6158b0
};

// kind 30: event 0x128 for the first system of the Alliance
class Notice30 : public UiKeyNotice
{
public:
  Notice30(); // 0x48f690
  Notice30(const AiNoticeGroup* payloads, int owner); // 0x48f700
  ~Notice30() override; // 0x48f760 (deleting 0x48f6e0)

  int GetKind() override; // 0x48f900
  int GetClass() override
  {
    return 6;
  } // shared 0x48d640
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x48f8a0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48f790
  void Read(istream* in) override
  {
    UiKeyNotice::Read(in);
    ReadValue32(in, &m_f64);
  } // shared 0x48e580
  void Write(ostream* out) override
  {
    UiKeyNotice::Write(out);
    WriteValue32(out, &m_f64);
  } // shared 0x48f770

  int m_f64; // +0x64
};

// kind 26: event 0x212 (shares its kind with Notice26b)
class Notice26a : public UiTextNotice
{
public:
  Notice26a(); // 0x48f910
  Notice26a(const AiNoticeGroup* payloads, int owner); // 0x48f990
  ~Notice26a() override; // 0x48f9f0 (deleting 0x48f970)

  int GetKind() override; // 0x48fcf0
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x48fc90
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48fa00
};

// kind 26: event 0x106: a character was killed or captured
class Notice26b : public UiTextNotice
{
public:
  Notice26b(); // 0x48fd00
  Notice26b(const AiNoticeGroup* payloads, int owner); // 0x48fd80
  ~Notice26b() override; // 0x48fde0 (deleting 0x48fd60)

  int GetKind() override
  {
    return 26;
  } // shared 0x48fcf0
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x4901f0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x48fdf0
};

// kind 25: event 0x1e0: where a character with the Force ended up
class Notice25 : public UiTextNotice
{
public:
  Notice25(); // 0x490250
  Notice25(const AiNoticeGroup* payloads, int owner); // 0x4902d0
  ~Notice25() override; // 0x490330 (deleting 0x4902b0)

  int GetKind() override; // 0x490f30
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x490ed0
  void Virtual05(int kind, intptr_t payload) override; // 0x490340
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0

  int m_f64; // +0x64 which of the five texts
};

// kind 24: event 0x153: a system changed hands
class Notice24 : public UiTextNotice
{
public:
  Notice24(); // 0x490f80
  Notice24(const AiNoticeGroup* payloads, int owner); // 0x490ff0
  ~Notice24() override; // 0x491050 (deleting 0x490fd0)

  int GetKind() override; // 0x491180
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x491120
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x491060
};

// kind 21: events 2 and 0x182 / 0x14f: a fleet with a Death Star arrives
class Notice21 : public UiKeyNotice
{
public:
  Notice21(); // 0x494ef0
  Notice21(const AiNoticeGroup* payloads, int owner); // 0x494f80
  ~Notice21() override; // 0x495000 (deleting 0x494f60)

  int GetKind() override; // 0x4953b0
  int GetClass() override; // 0x4953a0
  int Virtual11() override
  {
    return 0x400;
  } // shared 0x48e670
  UiNotice* Virtual10() override; // 0x495340
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x4950d0
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x4952a0
  void Read(istream* in) override; // 0x495050
  void Write(ostream* out) override; // 0x495090

  int m_f64; // +0x64 which side's object is named (1 or 2)
  ObjectKey m_subject; // +0x68
  int m_f6c; // +0x6c
};

// kind 20: event 0x200: a character leads a mission to a system
class Notice20 : public UiTextNotice
{
public:
  Notice20(); // 0x4953c0
  Notice20(const AiNoticeGroup* payloads, int owner); // 0x495460
  ~Notice20() override; // 0x4954f0 (deleting 0x495440)

  int GetKind() override; // 0x495d40
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x495b90
  void Virtual05(int kind, intptr_t payload) override; // 0x4955b0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int Virtual16(const AiNoticeGroup* payloads) override; // 0x495bf0
  int CountUnscheduled(const AiNoticeGroup* payloads); // 0x495d00
  void Read(istream* in) override; // 0x495550
  void Write(ostream* out) override; // 0x495580

  int m_f64; // +0x64
  std::string m_name; // +0x68
  ObjectKey m_unitKey; // +0x74
  int m_f78; // +0x78 number of 0x200 payloads
};

// kind 19: event 0x340: an object arrived in a system
class Notice19 : public UiTextNotice
{
public:
  Notice19(); // 0x495d50
  Notice19(const AiNoticeGroup* payloads, int owner); // 0x495dd0
  ~Notice19() override; // 0x495e40 (deleting 0x495db0)

  int GetKind() override; // 0x496000
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 256;
  } // shared 0x48bc60
  UiNotice* Virtual10() override; // 0x495fa0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x495e90
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Read(istream* in) override; // 0x495e50
  void Write(ostream* out) override; // 0x495e70

  int m_unused64; // +0x64, never set
  int m_f68; // +0x68
};

// kind 7: event 0x14e: a system is under attack
class Notice7 : public UiTextNotice
{
public:
  Notice7(); // 0x496010
  Notice7(const AiNoticeGroup* payloads, int owner); // 0x496080
  ~Notice7() override; // 0x4960e0 (deleting 0x496060)

  int GetKind() override
  {
    return 7;
  } // shared 0x4e7790
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 256;
  } // shared 0x48bc60
  UiNotice* Virtual10() override; // 0x496320
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x4960f0
};

// kind 16: event 0x303: a facility or ship arrived at a system
class Notice16 : public UiTextNotice
{
public:
  Notice16(); // 0x497560
  Notice16(const AiNoticeGroup* payloads, int owner); // 0x4975e0
  ~Notice16() override; // 0x497650 (deleting 0x4975c0)

  int GetKind() override
  {
    return 16;
  } // shared 0x5a5980
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 8;
  } // shared 0x48c2a0
  UiNotice* Virtual10() override; // 0x4977b0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x497690
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Read(istream* in) override; // 0x497660
  void Write(ostream* out) override; // 0x497910

  int m_f64; // +0x64
  int m_f68; // +0x68
};

// kind 14: event 0x107: an object was destroyed
class Notice14 : public UiTextNotice
{
public:
  Notice14(); // 0x497810
  Notice14(const AiNoticeGroup* payloads, int owner); // 0x497890
  ~Notice14() override; // 0x497900 (deleting 0x497870)

  int GetKind() override
  {
    return 14;
  } // shared 0x5a5920
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 8;
  } // shared 0x48c2a0
  UiNotice* Virtual10() override; // 0x498060
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x497940
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
    ReadValue32(in, &m_f68);
  } // shared 0x497660
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
    WriteValue32(out, &m_f68);
  } // shared 0x497910

  int m_f64; // +0x64
  int m_f68; // +0x68
};

// kind 13: event 0x105: an object was created
class Notice13 : public UiTextNotice
{
public:
  Notice13(); // 0x4980c0
  Notice13(const AiNoticeGroup* payloads, int owner); // 0x498140
  ~Notice13() override; // 0x4981b0 (deleting 0x498120)

  int GetKind() override
  {
    return 13;
  } // shared 0x5a5600
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 128;
  } // shared 0x526670
  UiNotice* Virtual10() override; // 0x498810
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x4981c0
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
    ReadValue32(in, &m_f68);
  } // shared 0x497660
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
    WriteValue32(out, &m_f68);
  } // shared 0x497910

  int m_f64; // +0x64
  int m_f68; // +0x68
};

// kind 12: event 0x304: two objects met in a system
class Notice12 : public UiTextNotice
{
public:
  Notice12(); // 0x498870
  Notice12(const AiNoticeGroup* payloads, int owner); // 0x4988f0
  ~Notice12() override; // 0x498960 (deleting 0x4988d0)

  int GetKind() override; // 0x498ba0
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 8;
  } // shared 0x48c2a0
  UiNotice* Virtual10() override; // 0x498b40
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x498970
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int Virtual16(const AiNoticeGroup* payloads) override; // shared 0x48bbf0
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
  } // shared 0x48b960
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
  } // shared 0x48b980

  int m_f64; // +0x64
};

// kind 11: event 0x148: a system became garrisoned
class Notice11 : public UiTextNotice
{
public:
  Notice11(); // 0x498bb0
  Notice11(const AiNoticeGroup* payloads, int owner); // 0x498c30
  ~Notice11() override; // 0x498ca0 (deleting 0x498c10)

  int GetKind() override; // 0x498e10
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x498db0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x498cb0
};

// kind 10: a text for the side opposite the owner (no payloads)
class Notice10 : public UiTextNotice
{
public:
  Notice10(); // 0x498e20
  Notice10(const char* text, int owner, int showFirst); // 0x498e90
  ~Notice10() override; // 0x498fa0 (deleting 0x498e70)

  int GetKind() override; // 0x499020
  int GetClass() override
  {
    return 8;
  } // shared 0x48ad30
  int Virtual11() override
  {
    return 32;
  } // shared 0x526bd0
  UiNotice* Virtual10() override; // 0x498fb0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int IsStale() override
  {
    return 0;
  } // shared 0x6158b0
  int Virtual16(const AiNoticeGroup* payloads) override
  {
    return 0;
  } // shared 0x48b440
  void SetSecondText(const char* text); // 0x499010
};

// kind 9: event 0x145: a system gained or lost a population/ruler
class Notice9 : public UiTextNotice
{
public:
  Notice9(); // 0x499030
  Notice9(const AiNoticeGroup* payloads, int owner); // 0x4990b0
  ~Notice9() override; // 0x499120 (deleting 0x499090)

  int GetKind() override; // 0x499350
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x4992f0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x499130
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
    ReadValue32(in, &m_f68);
  } // shared 0x497660
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
    WriteValue32(out, &m_f68);
  } // shared 0x497910

  int m_f64; // +0x64
  int m_f68; // +0x68
};

// kind 3: event 0x14b: a system was occupied or liberated
class Notice3 : public UiTextNotice
{
public:
  Notice3(); // 0x499360
  Notice3(const AiNoticeGroup* payloads, int owner); // 0x4993e0
  ~Notice3() override; // 0x499450 (deleting 0x4993c0)

  int GetKind() override
  {
    return 3;
  } // shared 0x5a6730
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x499620
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x499460
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
  } // shared 0x48b960
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
  } // shared 0x48b980

  int m_f64; // +0x64
};

// kind 4: event 0x100: a system changed hands
class Notice4 : public UiTextNotice
{
public:
  Notice4(); // 0x499680
  Notice4(const AiNoticeGroup* payloads, int owner); // 0x4996f0
  ~Notice4() override; // 0x499750 (deleting 0x4996d0)

  int GetKind() override
  {
    return 4;
  } // shared 0x5a6700
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 1;
  } // shared 0x40f340
  UiNotice* Virtual10() override; // 0x499960
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x499760
  void Read(istream* in) override
  {
    UiTextNotice::Read(in);
    ReadValue32(in, &m_f64);
  } // shared 0x48b960
  void Write(ostream* out) override
  {
    UiTextNotice::Write(out);
    WriteValue32(out, &m_f64);
  } // shared 0x48b980

  int m_f64; // +0x64
};

// kind 5: events 0x125-0x127: a tier record of a side changed
class Notice5 : public UiTextNotice
{
public:
  Notice5(); // 0x4999c0
  Notice5(const AiNoticeGroup* payloads, int owner); // 0x499a30
  ~Notice5() override; // 0x499a90 (deleting 0x499a10)

  int GetKind() override; // 0x499cd0
  int GetClass() override
  {
    return 4;
  } // shared 0x5a6700
  int Virtual11() override
  {
    return 8;
  } // shared 0x48c2a0
  UiNotice* Virtual10() override; // 0x499c70
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x499aa0
};

// kind 6: event 0x106: a capital ship or squadron was lost
class Notice6 : public UiTextNotice
{
public:
  Notice6(); // 0x499ce0
  Notice6(const AiNoticeGroup* payloads, int owner); // 0x499d50
  ~Notice6() override; // 0x499db0 (deleting 0x499d30)

  int GetKind() override
  {
    return 6;
  } // shared 0x48d640
  int GetClass() override
  {
    return 3;
  } // shared 0x5a6730
  int Virtual11() override
  {
    return 128;
  } // shared 0x526670
  UiNotice* Virtual10() override; // 0x49a0c0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x499de0
};

// kind 8: the dialog that confirms an order
class Notice8 : public UiNotice
{
public:
  Notice8(); // 0x49a120
  Notice8(int owner, MessageBase* subject); // 0x49a1b0
  ~Notice8() override; // 0x49a230 (deleting 0x49a190)

  int GetKind() override
  {
    return 8;
  } // shared 0x48c2a0
  int GetClass() override
  {
    return 5;
  } // shared 0x404c20
  int Virtual11() override
  {
    return 0;
  } // shared 0x6158b0
  UiNotice* Virtual10() override; // 0x49a2f0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void BuildText(); // 0x49a350
  void AppendName(GameObjNotifier* object); // 0x49a880
  void AppendOrderLines(); // 0x49a8b0
  void Read(istream* in) override; // 0x49a290
  void Write(ostream* out) override; // 0x49a2c0

  std::string m_text0; // +0x44
  std::string m_text1; // +0x50
  MessageBase* m_subject; // +0x5c the order that is confirmed
};

class Notice2;

// kind 1: the dialog that reports a battle or a blockade of a system
class Notice1 : public UiNotice
{
public:
  Notice1(); // 0x49aad0
  Notice1(const AiNoticeGroup* payloads, int owner, Notice2* vote); // 0x49ac00
  ~Notice1() override; // 0x49ad50 (deleting 0x49abe0)

  int GetKind() override
  {
    return 1;
  } // shared 0x5a66f0
  int GetClass() override
  {
    return 1;
  } // shared 0x5a66f0
  int Virtual11() override
  {
    return 256;
  } // shared 0x48bc60
  UiNotice* Virtual10() override; // 0x49b6c0
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x49b060
  int DispatchAndFill(const AiNoticeGroup* payloads); // 0x49b0f0
  void FillNeutral(); // 0x49b150
  void FillOwnSide(); // 0x49b330
  void FillOtherSide(); // 0x49b4d0
  void ReadVote(Notice2* vote); // 0x49b720
  void Read(istream* in) override; // 0x49aee0
  void Write(ostream* out) override; // 0x49afa0

  ObjectKey m_systemKey; // +0x44
  ObjectKey m_keyB; // +0x48
  ObjectKey m_keyC; // +0x4c
  int m_f50; // +0x50
  int m_f54; // +0x54
  int m_f58; // +0x58 1 or 2 for the side that owns the notice
  int m_f5c; // +0x5c, never set
  ObjectKeySet m_set60; // +0x60
  ObjectKeySet m_set68; // +0x68
  ObjectKeySet m_set70; // +0x70
  ObjectKeySet m_set78; // +0x78
  ObjectKeySet m_set80; // +0x80
  std::string m_text88; // +0x88
  std::string m_text94; // +0x94
  std::string m_texta0; // +0xa0
  std::string m_textac; // +0xac
  std::string m_textb8; // +0xb8
};

// kind 2: the dialog that reports the arrival of a fleet at a system
class Notice2 : public UiNotice
{
public:
  Notice2(); // 0x49bda0
  Notice2(const ObjectKey* key, int owner); // 0x49be60
  ~Notice2() override; // 0x49bf40 (deleting 0x49be40)

  int GetKind() override
  {
    return 2;
  } // shared 0x4e8740
  int GetClass() override
  {
    return 2;
  } // shared 0x4e8740
  int Virtual11() override
  {
    return 256;
  } // shared 0x48bc60
  UiNotice* Virtual10() override; // 0x49c570
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override
  {
    UiNotice::OnMessage(kind, a, b);
  } // shared 0x49c0b0
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int FillText(); // 0x49c0d0
  void AddObjectKeys(GameObjNotifier* object); // 0x49c5d0
  void Read(istream* in) override; // 0x49bfd0
  void Write(ostream* out) override; // 0x49c040

  ObjectKey m_systemKey; // +0x44
  int m_side; // +0x48 1 or 2 for the side that owns the notice
  int m_f4c; // +0x4c
  int m_attack; // +0x50
  std::string m_text54; // +0x54
  std::string m_text60; // +0x60
  ObjectKeySet m_set6c; // +0x6c
  ObjectKeySet m_set74; // +0x74
};

// kind 15: event 0x182: a fleet arrived (with the objects that came along)
class Notice15 : public UiTextNotice
{
public:
  Notice15(); // 0x496380
  Notice15(const AiNoticeGroup* payloads, int owner); // 0x496460
  ~Notice15() override; // 0x496530 (deleting 0x496440)

  int GetKind() override
  {
    return 15;
  } // shared 0x5a56b0
  int GetClass() override; // 0x497530
  int Virtual11() override
  {
    return 256;
  } // shared 0x48bc60
  UiNotice* Virtual10() override; // 0x497490
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x497380
  void Virtual08(int kind, intptr_t a, intptr_t b) override; // 0x496770, not reconstructed yet
  void Virtual09(int kind, intptr_t a, intptr_t b, intptr_t c) override; // 0x496f50, not reconstructed yet
  int IsStale() override
  {
    return 0;
  } // shared 0x6158b0
  void Read(istream* in) override; // 0x496670
  void Write(ostream* out) override; // 0x4966f0

  int m_f64; // +0x64
  int m_f68; // +0x68
  int m_f6c; // +0x6c
  ObjectKeySet m_set70; // +0x70
  ObjectKeySet m_set78; // +0x78
  ObjectKeySet m_set80; // +0x80
  ObjectKeySet m_set88; // +0x88
  ObjectKeySet m_set90; // +0x90
  ObjectKeySet m_set98; // +0x98
  int m_fa0; // +0xa0
};

// kind 23: events 0x1f1 and 0x302: a mission outcome (the kind and class can change)
class Notice23 : public UiTextNotice
{
public:
  Notice23(); // 0x491190
  Notice23(const AiNoticeGroup* payloads, int owner); // 0x491260
  ~Notice23() override; // 0x491320 (deleting 0x491240)

  int GetKind() override; // 0x492380
  int GetClass() override; // 0x492390
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x4922d0
  void Virtual05(int kind, intptr_t payload) override; // 0x491500, not reconstructed yet
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x492250
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  int Virtual16(const AiNoticeGroup* payloads) override; // 0x492330
  void Read(istream* in) override; // 0x4913f0
  void Write(ostream* out) override; // 0x4914a0

  int m_f64; // +0x64
  std::string m_name; // +0x68
  ObjectKey m_key74; // +0x74
  ObjectKey m_key78; // +0x78
  int m_kind; // +0x7c what GetKind returns
  int m_f80; // +0x80
  int m_class; // +0x84 what GetClass returns
  ObjectKeySet m_keys; // +0x88
};

// kind 22: events 0x1f0 and 0x302: a mission outcome with an order (the kind and class can change)
class Notice22 : public UiTextNotice
{
public:
  Notice22(); // 0x4923a0
  Notice22(const AiNoticeGroup* payloads, int owner); // 0x492470
  ~Notice22() override; // 0x492530 (deleting 0x492450)

  int GetKind() override
  {
    return m_kind;
  } // shared 0x492380
  int GetClass() override
  {
    return m_class;
  } // shared 0x492390
  int Virtual11() override
  {
    return 16;
  } // shared 0x48af30
  UiNotice* Virtual10() override; // 0x494cc0
  void Virtual05(int kind, intptr_t payload) override; // 0x4927c0, not reconstructed yet
  void Virtual06(int kind, intptr_t a, intptr_t b, intptr_t c) override
  {
  } // shared 0x48acb0
  void OnMessage(int kind, intptr_t a, intptr_t b) override; // 0x494c40
  void Virtual08(int kind, intptr_t a, intptr_t b) override
  {
  } // shared 0x48b3d0
  void Virtual14() override; // 0x494d70
  void Virtual15() override; // 0x494d90
  int Virtual16(const AiNoticeGroup* payloads) override; // 0x494d20
  void ListSecondKeys(); // 0x494df0
  void Read(istream* in) override; // 0x492610
  void Write(ostream* out) override; // 0x492740

  int m_f64; // +0x64
  std::string m_name; // +0x68
  ObjectKey m_key74; // +0x74
  ObjectKey m_key78; // +0x78
  int m_kind; // +0x7c what GetKind returns
  int m_f80; // +0x80
  int m_class; // +0x84 what GetClass returns
  ObjectKeySet m_keys; // +0x88
  MessageBase* m_order; // +0x90 the order 0x250 the notice offers, owned
  const AiNoticeGroup* m_payloads; // +0x94
};
