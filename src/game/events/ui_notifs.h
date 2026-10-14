#pragma once
// The UI notification handlers 0x43a2d0-0x440ea0: one class per notification kind, derived from the leaf payloads of
// notif_event_payloads.h (they add no data). Slot 6 (Dispatch) tells the PlayerController of the side about the
// notification through one of its virtual hooks (PlayerController slot = byte offset / 4); slot 7 (DispatchToSink)
// tells a UiNotice (strategy_ui/advisor_notices.h) the kind and the payload's data through the notice slot that
// fits the payload's shape. Slot 4 is the delivery mode (1 unless overridden).
#include <cstdint>

#include "events/notif_event_payloads.h"
#include "events/ui_notif_helpers.h"
#include "gameobj/status_code.h"
#include "strategy_ui/advisor_notices.h"

class PlayerController;

// kind 0x340
class UiNotif340 : public KeyPairPayload
{
public:
  UiNotif340(); // 0x43a2d0
  ~UiNotif340() override; // 0x43a310 (deleting 0x43a2f0)
  void Read(istream* in) override; // 0x43cc50
  void Write(ostream* out) override; // 0x43cc60
  int GetKind() override; // 0x43a390
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43a3a0
};

void* CreateUiNotif340(); // 0x43a320

// kind 0x14f
class UiNotif14f : public IntPairPayload
{
public:
  UiNotif14f(); // 0x43a3d0
  ~UiNotif14f() override; // 0x43a410 (deleting 0x43a3f0)
  void Read(istream* in) override; // 0x43a950
  void Write(ostream* out) override; // 0x43a760
  int GetKind() override; // 0x43a490
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43a4a0
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif14f(); // 0x43a420

// kind 0x150
class UiNotif150 : public IntPairPayload
{
public:
  UiNotif150(); // 0x43a4c0
  ~UiNotif150() override; // 0x43a500 (deleting 0x43a4e0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x150;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43a580
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif150(); // 0x43a510

// kind 0x14d
class UiNotif14d : public IntPairPayload
{
public:
  UiNotif14d(); // 0x43a5a0
  ~UiNotif14d() override; // 0x43a5e0 (deleting 0x43a5c0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43a660
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43a670
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif14d(); // 0x43a5f0

// kind 0x14e
class UiNotif14e : public IntPairPayload
{
public:
  UiNotif14e(); // 0x43a690
  ~UiNotif14e() override; // 0x43a6d0 (deleting 0x43a6b0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43a750
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43a770
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif14e(); // 0x43a6e0

// kind 0x142
class UiNotif142 : public IntPairPayload
{
public:
  UiNotif142(); // 0x43a790
  ~UiNotif142() override; // 0x43a7d0 (deleting 0x43a7b0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43a850
  void Dispatch(PlayerController* controller) override; // 0x43a860
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif142(); // 0x43a7e0

// kind 0x141
class UiNotif141 : public IntPairPayload
{
public:
  UiNotif141(); // 0x43a880
  ~UiNotif141() override; // 0x43a8c0 (deleting 0x43a8a0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43a940
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43a960
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif141(); // 0x43a8d0

// kind 0x14a
class UiNotif14a : public IntPairPayload
{
public:
  UiNotif14a(); // 0x43a980
  ~UiNotif14a() override; // 0x43a9c0 (deleting 0x43a9a0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43aa40
  void Dispatch(PlayerController* controller) override; // 0x43aa50
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif14a(); // 0x43a9d0

// kind 0x140
class UiNotif140 : public IntPairPayload
{
public:
  UiNotif140(); // 0x43aa70
  ~UiNotif140() override; // 0x43aab0 (deleting 0x43aa90)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ab30
  void Dispatch(PlayerController* controller) override; // 0x43ab40
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif140(); // 0x43aac0

// kind 0x14c
class UiNotif14c : public IntPairPayload
{
public:
  UiNotif14c(); // 0x43ab60
  ~UiNotif14c() override; // 0x43aba0 (deleting 0x43ab80)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ac20
  void Dispatch(PlayerController* controller) override; // 0x43ac30
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif14c(); // 0x43abb0

// kind 0x149
class UiNotif149 : public IntPairPayload
{
public:
  UiNotif149(); // 0x43ac50
  ~UiNotif149() override; // 0x43ac90 (deleting 0x43ac70)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ad10
  void Dispatch(PlayerController* controller) override; // 0x43ad20
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif149(); // 0x43aca0

// kind 0x146
class UiNotif146 : public IntPairPayload
{
public:
  UiNotif146(); // 0x43ad40
  ~UiNotif146() override; // 0x43ad80 (deleting 0x43ad60)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ae00
  void Dispatch(PlayerController* controller) override; // 0x43ae10
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif146(); // 0x43ad90

// kind 0x144
class UiNotif144 : public IntPairPayload
{
public:
  UiNotif144(); // 0x43ae30
  ~UiNotif144() override; // 0x43ae70 (deleting 0x43ae50)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43aef0
  void Dispatch(PlayerController* controller) override; // 0x43af00
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif144(); // 0x43ae80

// kind 0x143
class UiNotif143 : public IntPairPayload
{
public:
  UiNotif143(); // 0x43af20
  ~UiNotif143() override; // 0x43af60 (deleting 0x43af40)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43afe0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43aff0
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif143(); // 0x43af70

// kind 0x145
class UiNotif145 : public IntPairPayload
{
public:
  UiNotif145(); // 0x43b010
  ~UiNotif145() override; // 0x43b050 (deleting 0x43b030)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b0d0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b0e0
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif145(); // 0x43b060

// kind 0x14b
class UiNotif14b : public IntPairPayload
{
public:
  UiNotif14b(); // 0x43b100
  ~UiNotif14b() override; // 0x43b140 (deleting 0x43b120)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b1c0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b1d0
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif14b(); // 0x43b150

// kind 0x147
class UiNotif147 : public IntPairPayload
{
public:
  UiNotif147(); // 0x43b1f0
  ~UiNotif147() override; // 0x43b230 (deleting 0x43b210)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b2b0
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif147(); // 0x43b240

// kind 0x148
class UiNotif148 : public IntPairPayload
{
public:
  UiNotif148(); // 0x43b2c0
  ~UiNotif148() override; // 0x43b300 (deleting 0x43b2e0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b380
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif148(); // 0x43b310

// kind 0x156
class UiNotif156 : public IntPairPayload
{
public:
  UiNotif156(); // 0x43b390
  ~UiNotif156() override; // 0x43b3d0 (deleting 0x43b3b0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x156;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b450
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif156(); // 0x43b3e0

// kind 0x151
class UiNotif151 : public IntPairPayload
{
public:
  UiNotif151(); // 0x43b470
  ~UiNotif151() override; // 0x43b4b0 (deleting 0x43b490)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b530
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b540
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif151(); // 0x43b4c0

// kind 0x152
class UiNotif152 : public IntPairPayload
{
public:
  UiNotif152(); // 0x43b560
  ~UiNotif152() override; // 0x43b5a0 (deleting 0x43b580)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x152;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b620
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif152(); // 0x43b5b0

// kind 0x153
class UiNotif153 : public IntPairPayload
{
public:
  UiNotif153(); // 0x43b640
  ~UiNotif153() override; // 0x43b680 (deleting 0x43b660)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b700
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b710
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif153(); // 0x43b690

// kind 0x154
class UiNotif154 : public IntPairPayload
{
public:
  UiNotif154(); // 0x43b730
  ~UiNotif154() override; // 0x43b770 (deleting 0x43b750)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b7f0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b800
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif154(); // 0x43b780

// kind 0x155
class UiNotif155 : public IntPairPayload
{
public:
  UiNotif155(); // 0x43b820
  ~UiNotif155() override; // 0x43b860 (deleting 0x43b840)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b8e0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43b8f0
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif155(); // 0x43b870

// kind 0x121
class UiNotif121 : public IntPairPayload
{
public:
  UiNotif121(); // 0x43b910
  ~UiNotif121() override; // 0x43b950 (deleting 0x43b930)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43b9d0
  void Dispatch(PlayerController* controller) override; // 0x43b9e0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif121(); // 0x43b960

// kind 0x123
class UiNotif123 : public IntPairPayload
{
public:
  UiNotif123(); // 0x43ba00
  ~UiNotif123() override; // 0x43ba40 (deleting 0x43ba20)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43bac0
  void Dispatch(PlayerController* controller) override; // 0x43bad0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif123(); // 0x43ba50

// kind 0x122
class UiNotif122 : public IntPairPayload
{
public:
  UiNotif122(); // 0x43baf0
  ~UiNotif122() override; // 0x43bb30 (deleting 0x43bb10)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43bbb0
  void Dispatch(PlayerController* controller) override; // 0x43bbc0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif122(); // 0x43bb40

// kind 0x124
class UiNotif124 : public IntPairPayload
{
public:
  UiNotif124(); // 0x43bbe0
  ~UiNotif124() override; // 0x43bc20 (deleting 0x43bc00)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43bca0
  void Dispatch(PlayerController* controller) override; // 0x43bcb0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif124(); // 0x43bc30

// kind 0x600
class UiNotif600 : public IntQuadPayload
{
public:
  UiNotif600(); // 0x43bcd0
  ~UiNotif600() override; // 0x43bd10 (deleting 0x43bcf0)
  void Read(istream* in) override; // 0x43bda0
  void Write(ostream* out) override; // 0x43bdb0
  int GetKind() override; // 0x43bd90
  void Dispatch(PlayerController* controller) override; // 0x43bdc0
  void DispatchToSink(UiNotice*) override
  {
  }
  IntPair* GetRange(IntPair* out); // 0x43c0f0
};

void* CreateUiNotif600(); // 0x43bd20

// kind 0x120
class UiNotif120 : public IntPairPayload
{
public:
  UiNotif120(); // 0x43be20
  ~UiNotif120() override; // 0x43be60 (deleting 0x43be40)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43bee0
  void Dispatch(PlayerController* controller) override; // 0x43bef0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif120(); // 0x43be70

// kind 0x125
class UiNotif125 : public IntPairPayload
{
public:
  UiNotif125(); // 0x43bf10
  ~UiNotif125() override; // 0x43bf50 (deleting 0x43bf30)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43bfd0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43bfe0
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif125(); // 0x43bf60

// kind 0x126
class UiNotif126 : public IntPairPayload
{
public:
  UiNotif126(); // 0x43c000
  ~UiNotif126() override; // 0x43c040 (deleting 0x43c020)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c0c0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43c0d0
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif126(); // 0x43c050

// kind 0x127
class UiNotif127 : public IntPairPayload
{
public:
  UiNotif127(); // 0x43c120
  ~UiNotif127() override; // 0x43c160 (deleting 0x43c140)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c1e0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43c200
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif127(); // 0x43c170

// kind 0x129
class UiNotif129 : public IntPairPayload
{
public:
  UiNotif129(); // 0x43c220
  ~UiNotif129() override; // 0x43c260 (deleting 0x43c240)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c2e0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif129(); // 0x43c270

// kind 0x12a
class UiNotif12a : public IntPairPayload
{
public:
  UiNotif12a(); // 0x43c2f0
  ~UiNotif12a() override; // 0x43c330 (deleting 0x43c310)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c3b0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif12a(); // 0x43c340

// kind 0x12b
class UiNotif12b : public IntPairPayload
{
public:
  UiNotif12b(); // 0x43c3c0
  ~UiNotif12b() override; // 0x43c400 (deleting 0x43c3e0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c480
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif12b(); // 0x43c410

// kind 0x12c
class UiNotif12c : public IntPairPayload
{
public:
  UiNotif12c(); // 0x43c490
  ~UiNotif12c() override; // 0x43c4d0 (deleting 0x43c4b0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c550
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif12c(); // 0x43c4e0

// kind 0x128
class UiNotif128 : public IntPairPayload
{
public:
  UiNotif128(); // 0x43c560
  ~UiNotif128() override; // 0x43c5a0 (deleting 0x43c580)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c620
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43c630
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif128(); // 0x43c5b0

// kind 0x380
class UiNotif380 : public KeyPairPayload
{
public:
  UiNotif380(); // 0x43c8b0
  ~UiNotif380() override; // 0x43c8f0 (deleting 0x43c8d0)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x380;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43c980
  void DispatchToSink(UiNotice* sink) override; // 0x43e690
};

void* CreateUiNotif380(); // 0x43c900

// kind 0x1f0
class UiNotif1f0 : public IntPairPayload
{
public:
  UiNotif1f0(); // 0x43c9a0
  ~UiNotif1f0() override; // 0x43c9e0 (deleting 0x43c9c0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ca60
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43ca70
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif1f0(); // 0x43c9f0

// kind 0x1f1
class UiNotif1f1 : public IntPairPayload
{
public:
  UiNotif1f1(); // 0x43ca90
  ~UiNotif1f1() override; // 0x43cad0 (deleting 0x43cab0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43cb50
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43cb60
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif1f1(); // 0x43cae0

// kind 0x370
class UiNotif370 : public KeyPairPayload
{
public:
  UiNotif370(); // 0x43cb80
  ~UiNotif370() override; // 0x43cbc0 (deleting 0x43cba0)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x370;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43cc70
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif370(); // 0x43cbd0

// kind 0x221
class UiNotif221 : public IntPairPayload
{
public:
  UiNotif221(); // 0x43cc90
  ~UiNotif221() override; // 0x43ccd0 (deleting 0x43ccb0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x221;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif221(); // 0x43cce0

// kind 0x220
class UiNotif220 : public IntPairPayload
{
public:
  UiNotif220(); // 0x43cd50
  ~UiNotif220() override; // 0x43cd90 (deleting 0x43cd70)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x220;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif220(); // 0x43cda0

// kind 0x161
class UiNotif161 : public IntPairPayload
{
public:
  UiNotif161(); // 0x43ce10
  ~UiNotif161() override; // 0x43ce50 (deleting 0x43ce30)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ced0
  void Dispatch(PlayerController* controller) override; // 0x43cee0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif161(); // 0x43ce60

// kind 0x321
class UiNotif321 : public KeyPairPayload
{
public:
  UiNotif321(); // 0x43cf00
  ~UiNotif321() override; // 0x43cf40 (deleting 0x43cf20)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x321;
  }
  void Dispatch(PlayerController* controller) override; // 0x43cfc0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif321(); // 0x43cf50

// kind 0x320
class UiNotif320 : public KeyPairPayload
{
public:
  UiNotif320(); // 0x43cfe0
  ~UiNotif320() override; // 0x43d020 (deleting 0x43d000)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x320;
  }
  void Dispatch(PlayerController* controller) override; // 0x43d0a0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif320(); // 0x43d030

// kind 0x520
class UiNotif520 : public StringPairPayload
{
public:
  UiNotif520(); // 0x43d0c0
  ~UiNotif520() override; // 0x43d100 (deleting 0x43d0e0)
  void Read(istream* in) override; // 0x43d190
  void Write(ostream* out) override; // 0x43d1a0
  int GetKind() override; // 0x43d180
  void Dispatch(PlayerController* controller) override; // 0x43d1b0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif520(); // 0x43d110

// kind 0x160
class UiNotif160 : public IntPairPayload
{
public:
  UiNotif160(); // 0x43d240
  ~UiNotif160() override; // 0x43d280 (deleting 0x43d260)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43d300
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43d310
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif160(); // 0x43d290

// kind 0x162
class UiNotif162 : public IntPairPayload
{
public:
  UiNotif162(); // 0x43d330
  ~UiNotif162() override; // 0x43d370 (deleting 0x43d350)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43d3f0
  void Dispatch(PlayerController* controller) override; // 0x43d400
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif162(); // 0x43d380

// kind 0x400
class UiNotif400 : public NullableKeyPairPayload
{
public:
  UiNotif400(); // 0x43d420
  ~UiNotif400() override; // 0x43d460 (deleting 0x43d440)
  void Read(istream* in) override; // 0x43d4e0
  void Write(ostream* out) override; // 0x43d4f0
  int GetKind() override
  {
    return 0x400;
  }
  void Dispatch(PlayerController* controller) override; // 0x43d500
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif400(); // 0x43d470

// kind 0x163
class UiNotif163 : public IntPairPayload
{
public:
  UiNotif163(); // 0x43d520
  ~UiNotif163() override; // 0x43d560 (deleting 0x43d540)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43d5e0
  void Dispatch(PlayerController* controller) override; // 0x43d5f0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif163(); // 0x43d570

// kind 0x360
class UiNotif360 : public KeyPairPayload
{
public:
  UiNotif360(); // 0x43d610
  ~UiNotif360() override; // 0x43d650 (deleting 0x43d630)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override; // 0x43d6d0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif360(); // 0x43d660

// kind 0x361
class UiNotif361 : public KeyPairPayload
{
public:
  UiNotif361(); // 0x43d6e0
  ~UiNotif361() override; // 0x43d720 (deleting 0x43d700)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override; // 0x43d7a0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e690
};

void* CreateUiNotif361(); // 0x43d730

// kind 0x362
class UiNotif362 : public KeyPairPayload
{
public:
  UiNotif362(); // 0x43d7b0
  ~UiNotif362() override; // 0x43d7f0 (deleting 0x43d7d0)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override; // 0x43d870
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif362(); // 0x43d800

// kind 0x1e1
class UiNotif1e1 : public IntPairPayload
{
public:
  UiNotif1e1(); // 0x43d880
  ~UiNotif1e1() override; // 0x43d8c0 (deleting 0x43d8a0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43d940
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43d950
  void DispatchToSink(UiNotice* sink) override; // 0x43df10
};

void* CreateUiNotif1e1(); // 0x43d8d0

// kind 0x1e5
class UiNotif1e5 : public IntPairPayload
{
public:
  UiNotif1e5(); // 0x43d970
  ~UiNotif1e5() override; // 0x43d9b0 (deleting 0x43d990)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43da30
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1e5(); // 0x43d9c0

// kind 0x1e4
class UiNotif1e4 : public IntPairPayload
{
public:
  UiNotif1e4(); // 0x43da40
  ~UiNotif1e4() override; // 0x43da80 (deleting 0x43da60)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43db00
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1e4(); // 0x43da90

// kind 0x1e2
class UiNotif1e2 : public IntPairPayload
{
public:
  UiNotif1e2(); // 0x43df30
  ~UiNotif1e2() override; // 0x43df70 (deleting 0x43df50)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43dff0
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43e000
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1e2(); // 0x43df80

// kind 0x1e3
class UiNotif1e3 : public IntPairPayload
{
public:
  UiNotif1e3(); // 0x43e020
  ~UiNotif1e3() override; // 0x43e060 (deleting 0x43e040)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43e0e0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1e3(); // 0x43e070

// kind 0x210
class UiNotif210 : public IntPairPayload
{
public:
  UiNotif210(); // 0x43e0f0
  ~UiNotif210() override; // 0x43e130 (deleting 0x43e110)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x210;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif210(); // 0x43e140

// kind 0x211
class UiNotif211 : public IntPairPayload
{
public:
  UiNotif211(); // 0x43e1b0
  ~UiNotif211() override; // 0x43e1f0 (deleting 0x43e1d0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x211;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif211(); // 0x43e200

// kind 0x212
class UiNotif212 : public IntPairPayload
{
public:
  UiNotif212(); // 0x43e270
  ~UiNotif212() override; // 0x43e2b0 (deleting 0x43e290)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x212;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif212(); // 0x43e2c0

// kind 0x230
class UiNotif230 : public IntPairPayload
{
public:
  UiNotif230(); // 0x43e330
  ~UiNotif230() override; // 0x43e370 (deleting 0x43e350)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x230;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif230(); // 0x43e380

// kind 0x231
class UiNotif231 : public IntPairPayload
{
public:
  UiNotif231(); // 0x43e3f0
  ~UiNotif231() override; // 0x43e430 (deleting 0x43e410)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43e4b0
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif231(); // 0x43e440

// kind 0x300
class UiNotif300 : public KeyPairPayload
{
public:
  UiNotif300(); // 0x43e4f0
  ~UiNotif300() override; // 0x43e530 (deleting 0x43e510)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x300;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43e5b0
  void DispatchToSink(UiNotice* sink) override; // 0x43e690
};

void* CreateUiNotif300(); // 0x43e540

// kind 0x301
class UiNotif301 : public KeyPairPayload
{
public:
  UiNotif301(); // 0x43e5d0
  ~UiNotif301() override; // 0x43e610 (deleting 0x43e5f0)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x301;
  }
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43e690
};

void* CreateUiNotif301(); // 0x43e620

// kind 0x103
class UiNotif103 : public IntPairPayload
{
public:
  UiNotif103(); // 0x43e6c0
  ~UiNotif103() override; // 0x43e700 (deleting 0x43e6e0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43e780
  void Dispatch(PlayerController* controller) override; // 0x43e790
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif103(); // 0x43e710

// kind 0x107
class UiNotif107 : public IntPairPayload
{
public:
  UiNotif107(); // 0x43e7b0
  ~UiNotif107() override; // 0x43e7f0 (deleting 0x43e7d0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43e870
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController* controller) override; // 0x43e880
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif107(); // 0x43e800

// kind 0x100
class UiNotif100 : public IntPairPayload
{
public:
  UiNotif100(); // 0x43e8a0
  ~UiNotif100() override; // 0x43e8e0 (deleting 0x43e8c0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x100;
  }
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43e960
  void DispatchToSink(UiNotice* sink) override; // 0x43e980
};

void* CreateUiNotif100(); // 0x43e8f0

// kind 0x102
class UiNotif102 : public IntPairPayload
{
public:
  UiNotif102(); // 0x43e9b0
  ~UiNotif102() override; // 0x43e9f0 (deleting 0x43e9d0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ea70
  void Dispatch(PlayerController* controller) override; // 0x43e790
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif102(); // 0x43ea00

// kind 0x302
class UiNotif302 : public KeyPairPayload
{
public:
  UiNotif302(); // 0x43ea80
  ~UiNotif302() override; // 0x43eac0 (deleting 0x43eaa0)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x302;
  }
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43eb40
  void DispatchToSink(UiNotice* sink) override; // 0x43ebc0
};

void* CreateUiNotif302(); // 0x43ead0

// kind 0x303
class UiNotif303 : public KeyPairPayload
{
public:
  UiNotif303(); // 0x43ec50
  ~UiNotif303() override; // 0x43ec90 (deleting 0x43ec70)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x303;
  }
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif303(); // 0x43eca0

// kind 0x304
class UiNotif304 : public KeyPairPayload
{
public:
  UiNotif304(); // 0x43ed10
  ~UiNotif304() override; // 0x43ed50 (deleting 0x43ed30)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x304;
  }
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43edd0
};

void* CreateUiNotif304(); // 0x43ed60

// kind 0x305
class UiNotif305 : public KeyPairPayload
{
public:
  UiNotif305(); // 0x43ee40
  ~UiNotif305() override; // 0x43ee80 (deleting 0x43ee60)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x305;
  }
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController* controller) override; // 0x43ef10
  void DispatchToSink(UiNotice* sink) override; // 0x43ef70
};

void* CreateUiNotif305(); // 0x43ee90

// kind 0x306
class UiNotif306 : public KeyPairPayload
{
public:
  UiNotif306(); // 0x43efe0
  ~UiNotif306() override; // 0x43f020 (deleting 0x43f000)
  void Read(istream* in) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    reinterpret_cast<NullableKeyPairPayload*>(this)->WritePayload(out);
  }
  int GetKind() override
  {
    return 0x306;
  }
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43f0a0
};

void* CreateUiNotif306(); // 0x43f030

// kind 0x105
class UiNotif105 : public IntPairPayload
{
public:
  UiNotif105(); // 0x43f130
  ~UiNotif105() override; // 0x43f170 (deleting 0x43f150)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f1f0
  int GetDeliveryMode() override
  {
    return 3;
  }
  void Dispatch(PlayerController* controller) override; // 0x43f200
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif105(); // 0x43f180

// kind 0x104
class UiNotif104 : public IntPairPayload
{
public:
  UiNotif104(); // 0x43f220
  ~UiNotif104() override; // 0x43f260 (deleting 0x43f240)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f2e0
  void Dispatch(PlayerController* controller) override; // 0x43f2f0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif104(); // 0x43f270

// kind 0x500
class UiNotif500 : public StringPairPayload
{
public:
  UiNotif500(); // 0x43f310
  ~UiNotif500() override; // 0x43f350 (deleting 0x43f330)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f3d0
  void Dispatch(PlayerController* controller) override; // 0x43f3e0
  void DispatchToSink(UiNotice*) override
  {
  }
  ObjectKey* GetKeyPointer(); // 0x43f440
};

void* CreateUiNotif500(); // 0x43f360

// kind 0x101
class UiNotif101 : public IntPairPayload
{
public:
  UiNotif101(); // 0x43f490
  ~UiNotif101() override; // 0x43f4d0 (deleting 0x43f4b0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f550
  void Dispatch(PlayerController* controller) override; // 0x43f560
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif101(); // 0x43f4e0

// kind 0x106
class UiNotif106 : public IntPairPayload
{
public:
  UiNotif106(); // 0x43f580
  ~UiNotif106() override; // 0x43f5c0 (deleting 0x43f5a0)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f640
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43f650
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif106(); // 0x43f5d0

// kind 0x183
class UiNotif183 : public IntPairPayload
{
public:
  UiNotif183(); // 0x43f670
  ~UiNotif183() override; // 0x43f6b0 (deleting 0x43f690)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f730
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43f740
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif183(); // 0x43f6c0

// kind 0x180
class UiNotif180 : public IntPairPayload
{
public:
  UiNotif180(); // 0x43f760
  ~UiNotif180() override; // 0x43f7a0 (deleting 0x43f780)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f820
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43f830
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif180(); // 0x43f7b0

// kind 0x181
class UiNotif181 : public IntPairPayload
{
public:
  UiNotif181(); // 0x43f850
  ~UiNotif181() override; // 0x43f890 (deleting 0x43f870)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43f910
  int GetDeliveryMode() override
  {
    return 2;
  }
  void Dispatch(PlayerController* controller) override; // 0x43f920
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif181(); // 0x43f8a0

// kind 0x182
class UiNotif182 : public IntPairPayload
{
public:
  UiNotif182(); // 0x43f940
  ~UiNotif182() override; // 0x43f980 (deleting 0x43f960)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43fa00
  int GetDeliveryMode() override
  {
    return 4;
  }
  void Dispatch(PlayerController* controller) override; // 0x43fa10
  void DispatchToSink(UiNotice* sink) override; // 0x43e4c0
};

void* CreateUiNotif182(); // 0x43f990

// kind 0x1a0
class UiNotif1a0 : public IntPairPayload
{
public:
  UiNotif1a0(); // 0x43fa30
  ~UiNotif1a0() override; // 0x43fa70 (deleting 0x43fa50)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43faf0
  void Dispatch(PlayerController* controller) override; // 0x43fb00
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1a0(); // 0x43fa80

// kind 0x2
class UiNotif2 : public IntQuadPayload
{
public:
  UiNotif2(); // 0x43fb20
  ~UiNotif2() override; // 0x43fb60 (deleting 0x43fb40)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43c1f0
  void Dispatch(PlayerController*) override
  {
  }
  void DispatchToSink(UiNotice* sink) override; // 0x43fbe0
};

void* CreateUiNotif2(); // 0x43fb70

// kind 0x1c0
class UiNotif1c0 : public IntPairPayload
{
public:
  UiNotif1c0(); // 0x43fc50
  ~UiNotif1c0() override; // 0x43fc90 (deleting 0x43fc70)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43fd10
  void Dispatch(PlayerController* controller) override; // 0x43fd20
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c0(); // 0x43fca0

// kind 0x1c1
class UiNotif1c1 : public IntPairPayload
{
public:
  UiNotif1c1(); // 0x43fd40
  ~UiNotif1c1() override; // 0x43fd80 (deleting 0x43fd60)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43fe00
  void Dispatch(PlayerController* controller) override; // 0x43fe10
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c1(); // 0x43fd90

// kind 0x1c2
class UiNotif1c2 : public IntPairPayload
{
public:
  UiNotif1c2(); // 0x43fe30
  ~UiNotif1c2() override; // 0x43fe70 (deleting 0x43fe50)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43fef0
  void Dispatch(PlayerController* controller) override; // 0x43ff00
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c2(); // 0x43fe80

// kind 0x1c3
class UiNotif1c3 : public IntPairPayload
{
public:
  UiNotif1c3(); // 0x43ff20
  ~UiNotif1c3() override; // 0x43ff60 (deleting 0x43ff40)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x43ffe0
  void Dispatch(PlayerController* controller) override; // 0x43fff0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c3(); // 0x43ff70

// kind 0x1c4
class UiNotif1c4 : public IntPairPayload
{
public:
  UiNotif1c4(); // 0x440010
  ~UiNotif1c4() override; // 0x440050 (deleting 0x440030)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x4400d0
  void Dispatch(PlayerController* controller) override; // 0x4400e0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c4(); // 0x440060

// kind 0x1c5
class UiNotif1c5 : public IntPairPayload
{
public:
  UiNotif1c5(); // 0x440100
  ~UiNotif1c5() override; // 0x440140 (deleting 0x440120)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x4401c0
  void Dispatch(PlayerController* controller) override; // 0x4401d0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c5(); // 0x440150

// kind 0x1c6
class UiNotif1c6 : public IntPairPayload
{
public:
  UiNotif1c6(); // 0x4401f0
  ~UiNotif1c6() override; // 0x440230 (deleting 0x440210)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override; // 0x4402b0
  void Dispatch(PlayerController* controller) override; // 0x4402c0
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1c6(); // 0x440240

// kind 0x1
class UiNotif1 : public IntQuadPayload
{
public:
  UiNotif1(); // 0x4402e0
  ~UiNotif1() override; // 0x440320 (deleting 0x440300)
  void Read(istream* in) override
  {
    ReadPayload(in);
  }
  void Write(ostream* out) override
  {
    WritePayload(out);
  }
  int GetKind() override
  {
    return 0x1;
  }
  void Dispatch(PlayerController* controller) override; // 0x440330
  void DispatchToSink(UiNotice*) override
  {
  }
};

void* CreateUiNotif1(); // 0x440350
