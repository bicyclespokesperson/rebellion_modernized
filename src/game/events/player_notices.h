#pragma once
// The concrete UiNotice classes (0x48aac0-0x49c6a0): one per kind of player notice, the "UI handlers" that
// UiRecordTree (strategy_ui/ui_handler_table.cpp) keeps one prototype of per kind. A notice is built by the
// controller of a side from the notification payloads that describe an event (the constructor taking the payload
// list hands each payload to the notice through slots 5-9, see NotifPayload::DispatchToSink); the notice keeps the
// resulting texts, pictures and sounds for the message index and the advisor. Names are numbered by GetKind().
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/object_key.h"
#include "strategy_ui/advisor_notices.h"

// kind 44: three texts for the events 0x129-0x12b
class Notice44 : public UiTextNotice {
public:
    Notice44();  // 0x48aac0
    Notice44(const LinkNode* payloads, int owner);  // 0x48ab40
    virtual ~Notice44();  // 0x48abb0 (deleting 0x48ab20)

    virtual int GetKind() override;  // 0x48ad20
    virtual int GetClass() override;  // 0x48ad30
    virtual int Virtual11() override;  // 0x48c2a0
    virtual UiNotice* Virtual10() override;  // 0x48acc0
    virtual void Virtual06(int kind, int a, int b, int c) override;  // 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x49c0b0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48abc0
    virtual void Read(istream* in) override;  // 0x499dc0
    virtual void Write(ostream* out) override;  // 0x499dd0

    int m_f64;  // +0x64
};

// kind 45: the texts for event 0x12c
class Notice45 : public UiTextNotice {
public:
    Notice45();  // 0x48ad40
    Notice45(const LinkNode* payloads, int owner);  // 0x48adc0
    virtual ~Notice45();  // 0x48ae30 (deleting 0x48ada0)

    virtual int GetKind() override;  // 0x48af40
    virtual int GetClass() override { return 8; }  // shared 0x48ad30
    virtual int Virtual11() override;  // 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48aed0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48ae40

    int m_f64;  // +0x64
};

// kind 43: two objects of a transfer (event 0x306): their texts and the system above them
class Notice43 : public UiTextNotice {
public:
    Notice43();  // 0x48af50
    Notice43(const LinkNode* payloads, int owner);  // 0x48afd0
    virtual ~Notice43();  // 0x48b040 (deleting 0x48afb0)

    virtual int GetKind() override;  // 0x48b260
    virtual int GetClass() override { return 8; }  // shared 0x48ad30
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48b200
    virtual void Virtual06(int kind, int a, int b, int c) override {}  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x48b050
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Read(istream* in) override;  // 0x48b960
    virtual void Write(ostream* out) override;  // 0x48b980

    int m_f64;  // +0x64
};

// kind 42: a record (research/production type) became available
class Notice42 : public UiTextNotice {
public:
    Notice42();  // 0x48b270
    Notice42(int owner, const IdNode* record);  // 0x48b2e0
    virtual ~Notice42();  // 0x48b3c0 (deleting 0x48b2c0)

    virtual int GetKind() override;  // 0x48b5d0
    virtual int GetClass() override { return 8; }  // shared 0x48ad30
    virtual int Virtual11() override { return 512; }  // shared 0x48b450
    virtual UiNotice* Virtual10() override;  // 0x48b3e0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual int IsStale() override { return 0; }  // shared 0x6158b0
    virtual int Virtual16(const LinkNode* payloads) override { return 0; }  // shared 0x48b440

};

// kind 41: event 0x231: the texts for a side that can name a general
class Notice41 : public UiTextNotice {
public:
    Notice41();  // 0x48b5e0
    Notice41(const LinkNode* payloads, int owner);  // 0x48b660
    virtual ~Notice41();  // 0x48b6d0 (deleting 0x48b640)

    virtual int GetKind() override;  // 0x48b850
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48b7f0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48b6e0

    int m_f64;  // +0x64
};

// kind 34: event 0x305: a character met another one
class Notice34 : public UiTextNotice {
public:
    Notice34();  // 0x48b860
    Notice34(const LinkNode* payloads, int owner);  // 0x48b8e0
    virtual ~Notice34();  // 0x48b950 (deleting 0x48b8c0)

    virtual int GetKind() override;  // 0x48bc70
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override;  // 0x48bc60
    virtual UiNotice* Virtual10() override;  // 0x48bb90
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x48b9a0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual int Virtual16(const LinkNode* payloads) override;  // 0x48bbf0
    virtual void Read(istream* in) override { UiTextNotice::Read(in);
    ReadValue32(in, &m_f64); }  // shared 0x48b960
    virtual void Write(ostream* out) override { UiTextNotice::Write(out);
    WriteValue32(out, &m_f64); }  // shared 0x48b980

    int m_f64;  // +0x64
};

// kind 33: event 0x160: a manufacturing manager of a system finished or is idle
class Notice33 : public UiTextNotice {
public:
    Notice33();  // 0x48bc80
    Notice33(const LinkNode* payloads, int owner);  // 0x48bd10
    virtual ~Notice33();  // 0x48bd90 (deleting 0x48bcf0)

    virtual int GetKind() override;  // 0x48c310
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 8; }  // shared 0x48c2a0
    virtual UiNotice* Virtual10() override;  // 0x48c240
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x48be60
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48be60
    virtual int IsStale() override;  // 0x48c2b0
    virtual void Read(istream* in) override;  // 0x48bde0
    virtual void Write(ostream* out) override;  // 0x48be20

    int m_f64;  // +0x64
    int m_f68;  // +0x68
    ObjectKey m_hostKey;  // +0x6c
};

// kind 40: event 0x200: a character leads a mission
class Notice40 : public UiTextNotice {
public:
    Notice40();  // 0x48c320
    Notice40(const LinkNode* payloads, int owner);  // 0x48c3a0
    virtual ~Notice40();  // 0x48c410 (deleting 0x48c380)

    virtual int GetKind() override;  // 0x48c8b0
    virtual int GetClass() override { return 8; }  // shared 0x48ad30
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48c850
    virtual void Virtual05(int kind, int payload) override;  // 0x48c4c0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Read(istream* in) override;  // 0x48c460
    virtual void Write(ostream* out) override;  // 0x48c490

    int m_f64;  // +0x64
    ObjectKey m_subjectKey;  // +0x68
};

// kind 39: event 0x361: two characters of different sides
class Notice39 : public UiTextNotice {
public:
    Notice39();  // 0x48c8c0
    Notice39(const LinkNode* payloads, int owner);  // 0x48c940
    virtual ~Notice39();  // 0x48c9b0 (deleting 0x48c920)

    virtual int GetKind() override;  // 0x48cd70
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48cd10
    virtual void Virtual06(int kind, int a, int b, int c) override;  // 0x48c9c0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0

};

// kind 38: event 0x230: the Emperor's side learns of the Death Star finale
class Notice38 : public UiTextNotice {
public:
    Notice38();  // 0x48cd80
    Notice38(const LinkNode* payloads, int owner);  // 0x48ce00
    virtual ~Notice38();  // 0x48ce70 (deleting 0x48cde0)

    virtual int GetKind() override;  // 0x48cfe0
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48cf80
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48ce80

    int m_f64;  // +0x64
};

// kind 37: events 0x210 and 0x221: Luke on a mission
class Notice37 : public UiTextNotice {
public:
    Notice37();  // 0x48cff0
    Notice37(const LinkNode* payloads, int owner);  // 0x48d070
    virtual ~Notice37();  // 0x48d0e0 (deleting 0x48d050)

    virtual int GetKind() override;  // 0x48d360
    virtual int GetClass() override { return 8; }  // shared 0x48ad30
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48d300
    virtual void Virtual05(int kind, int payload) override;  // 0x48d190
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48d0f0

    int m_f64;  // +0x64
};

// kind 35: event 0x305 for a capital ship that meets a system object
class Notice35 : public UiKeyNotice {
public:
    Notice35();  // 0x48d370
    Notice35(const LinkNode* payloads, int owner);  // 0x48d3e0
    virtual ~Notice35();  // 0x48d440 (deleting 0x48d3c0)

    virtual int GetKind() override;  // 0x48d630
    virtual int GetClass() override;  // 0x48d640
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48d5d0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x48d470
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Read(istream* in) override;  // 0x48d450
    virtual void Write(ostream* out) override;  // 0x48d460

};

// kind 36: the story characters of both sides meet (events 2, 0x211, 0x220, 0x360)
class Notice36 : public UiTextNotice {
public:
    Notice36();  // 0x48d650
    Notice36(const LinkNode* payloads, int owner);  // 0x48d6f0
    virtual ~Notice36();  // 0x48d780 (deleting 0x48d6d0)

    virtual int GetKind() override;  // 0x48e480
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48dad0
    virtual void Virtual05(int kind, int payload) override;  // 0x48d9a0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48d7f0
    void SetPriority();  // 0x48db30
    void FillFinalDuel();  // 0x48dc50
    void FillStoryPair();  // 0x48df20
    void FillStoryEncounter();  // 0x48e1d0
    void FillLukeAlone();  // 0x48e3c0

    PersistentInt m_state;  // +0x64 bit 0x1: seen 0x211, 0x10: seen 0x220, 0x10000000: a character was not found
    ObjectKey m_firstKey;  // +0x68 the character of the Alliance
    ObjectKey m_secondKey;  // +0x6c the character of the Empire
};

// kind 31: event 0x128
class Notice31 : public UiKeyNotice {
public:
    Notice31();  // 0x48e490
    Notice31(const LinkNode* payloads, int owner);  // 0x48e500
    virtual ~Notice31();  // 0x48e570 (deleting 0x48e4e0)

    virtual int GetKind() override;  // 0x48e680
    virtual int GetClass() override { return 6; }  // shared 0x48d640
    virtual int Virtual11() override { return 0x400; }  // shared 0x48e670
    virtual UiNotice* Virtual10() override;  // 0x48e610
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48e5a0
    virtual void Read(istream* in) override;  // 0x48e580
    virtual void Write(ostream* out) override;  // 0x48f770

    int m_f64;  // +0x64
};

// kind 32: events 2, 0x141, 0x143, 0x154, 0x155, 0x302: a system being besieged or blockaded
class Notice32 : public UiTextNotice {
public:
    Notice32();  // 0x48e690
    Notice32(const LinkNode* payloads, int owner);  // 0x48e720
    virtual ~Notice32();  // 0x48e7a0 (deleting 0x48e700)

    virtual int GetKind() override;  // 0x48ec70
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 4; }  // shared 0x4047d0
    virtual UiNotice* Virtual10() override;  // 0x48ec10
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override;  // 0x48e890
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48e940
    virtual void Virtual09(int kind, int a, int b, int c) override;  // 0x48e840
    virtual void Read(istream* in) override;  // 0x48e800
    virtual void Write(ostream* out) override;  // 0x48e820

    PersistentInt m_state;  // +0x64 bit 1: attackers outnumber, 2: blockade, 4: siege, 8: cleared, 0x10: unit given
    String m_name;  // +0x68
};

// kind 29: event 0x1e1: a character gained the Force
class Notice29 : public UiTextNotice {
public:
    Notice29();  // 0x48ec80
    Notice29(const LinkNode* payloads, int owner);  // 0x48ed00
    virtual ~Notice29();  // 0x48ed70 (deleting 0x48ece0)

    virtual int GetKind() override;  // 0x48f0d0
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48f070
    virtual void Virtual05(int kind, int payload) override;  // 0x48ed80
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0

};

// kind 28: event 0x362: two characters meet (one can train the other)
class Notice28 : public UiTextNotice {
public:
    Notice28();  // 0x48f0e0
    Notice28(const LinkNode* payloads, int owner);  // 0x48f160
    virtual ~Notice28();  // 0x48f1d0 (deleting 0x48f140)

    virtual int GetKind() override;  // 0x48f660
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48f600
    virtual void Virtual05(int kind, int payload) override;  // 0x48f1e0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual int IsStale() override { return 0; }  // shared 0x6158b0

};

// kind 30: event 0x128 for the first system of the Alliance
class Notice30 : public UiKeyNotice {
public:
    Notice30();  // 0x48f690
    Notice30(const LinkNode* payloads, int owner);  // 0x48f700
    virtual ~Notice30();  // 0x48f760 (deleting 0x48f6e0)

    virtual int GetKind() override;  // 0x48f900
    virtual int GetClass() override { return 6; }  // shared 0x48d640
    virtual int Virtual11() override { return 1; }  // shared 0x40f340
    virtual UiNotice* Virtual10() override;  // 0x48f8a0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48f790

    int m_f64;  // +0x64
};

// kind 26: event 0x212 (shares its kind with Notice26b)
class Notice26a : public UiTextNotice {
public:
    Notice26a();  // 0x48f910
    Notice26a(const LinkNode* payloads, int owner);  // 0x48f990
    virtual ~Notice26a();  // 0x48f9f0 (deleting 0x48f970)

    virtual int GetKind() override;  // 0x48fcf0
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x48fc90
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48fa00

};

// kind 26: event 0x106: a character was killed or captured
class Notice26b : public UiTextNotice {
public:
    Notice26b();  // 0x48fd00
    Notice26b(const LinkNode* payloads, int owner);  // 0x48fd80
    virtual ~Notice26b();  // 0x48fde0 (deleting 0x48fd60)

    virtual int GetKind() override { return 26; }  // shared 0x48fcf0
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x4901f0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x48fdf0

};

// kind 25: event 0x1e0: where a character with the Force ended up
class Notice25 : public UiTextNotice {
public:
    Notice25();  // 0x490250
    Notice25(const LinkNode* payloads, int owner);  // 0x4902d0
    virtual ~Notice25();  // 0x490330 (deleting 0x4902b0)

    virtual int GetKind() override;  // 0x490f30
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x490ed0
    virtual void Virtual05(int kind, int payload) override;  // 0x490340
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0

    int m_f64;  // +0x64 which of the five texts
};

// kind 24: event 0x153: a system changed hands
class Notice24 : public UiTextNotice {
public:
    Notice24();  // 0x490f80
    Notice24(const LinkNode* payloads, int owner);  // 0x490ff0
    virtual ~Notice24();  // 0x491050 (deleting 0x490fd0)

    virtual int GetKind() override;  // 0x491180
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 1; }  // shared 0x40f340
    virtual UiNotice* Virtual10() override;  // 0x491120
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x491060

};

// kind 21: events 2 and 0x182 / 0x14f: a fleet with a Death Star arrives
class Notice21 : public UiKeyNotice {
public:
    Notice21();  // 0x494ef0
    Notice21(const LinkNode* payloads, int owner);  // 0x494f80
    virtual ~Notice21();  // 0x495000 (deleting 0x494f60)

    virtual int GetKind() override;  // 0x4953b0
    virtual int GetClass() override;  // 0x4953a0
    virtual int Virtual11() override { return 0x400; }  // shared 0x48e670
    virtual UiNotice* Virtual10() override;  // 0x495340
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void Virtual08(int kind, int a, int b) override;  // 0x4950d0
    virtual void Virtual09(int kind, int a, int b, int c) override;  // 0x4952a0
    virtual void Read(istream* in) override;  // 0x495050
    virtual void Write(ostream* out) override;  // 0x495090

    int m_f64;  // +0x64 which side's object is named (1 or 2)
    ObjectKey m_subject;  // +0x68
    int m_f6c;  // +0x6c
};

// kind 20: event 0x200: a character leads a mission to a system
class Notice20 : public UiTextNotice {
public:
    Notice20();  // 0x4953c0
    Notice20(const LinkNode* payloads, int owner);  // 0x495460
    virtual ~Notice20();  // 0x4954f0 (deleting 0x495440)

    virtual int GetKind() override;  // 0x495d40
    virtual int GetClass() override { return 3; }  // shared 0x5a6730
    virtual int Virtual11() override { return 16; }  // shared 0x48af30
    virtual UiNotice* Virtual10() override;  // 0x495b90
    virtual void Virtual05(int kind, int payload) override;  // 0x4955b0
    virtual void Virtual06(int kind, int a, int b, int c) override {  }  // shared 0x48acb0
    virtual void OnMessage(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual void Virtual08(int kind, int a, int b) override {  }  // shared 0x48b3d0
    virtual int Virtual16(const LinkNode* payloads) override;  // 0x495bf0
    int CountUnscheduled(const LinkNode* payloads);  // 0x495d00
    virtual void Read(istream* in) override;  // 0x495550
    virtual void Write(ostream* out) override;  // 0x495580

    int m_f64;  // +0x64
    String m_name;  // +0x68
    ObjectKey m_unitKey;  // +0x74
    int m_f78;  // +0x78 number of 0x200 payloads
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(Notice44, 0x68);
REB_ASSERT_SIZE(Notice45, 0x68);
REB_ASSERT_SIZE(Notice43, 0x68);
REB_ASSERT_SIZE(Notice42, 0x64);
REB_ASSERT_SIZE(Notice41, 0x68);
REB_ASSERT_SIZE(Notice34, 0x68);
REB_ASSERT_SIZE(Notice33, 0x70);
REB_ASSERT_SIZE(Notice40, 0x6c);
REB_ASSERT_SIZE(Notice39, 0x64);
REB_ASSERT_SIZE(Notice38, 0x68);
REB_ASSERT_SIZE(Notice37, 0x68);
REB_ASSERT_SIZE(Notice35, 0x64);
REB_ASSERT_SIZE(Notice36, 0x70);
REB_ASSERT_SIZE(Notice31, 0x68);
REB_ASSERT_SIZE(Notice32, 0x74);
REB_ASSERT_SIZE(Notice29, 0x64);
REB_ASSERT_SIZE(Notice28, 0x64);
REB_ASSERT_SIZE(Notice30, 0x68);
REB_ASSERT_SIZE(Notice26a, 0x64);
REB_ASSERT_SIZE(Notice26b, 0x64);
REB_ASSERT_SIZE(Notice25, 0x68);
REB_ASSERT_SIZE(Notice24, 0x64);
REB_ASSERT_SIZE(Notice21, 0x70);
REB_ASSERT_SIZE(Notice20, 0x7c);
#pragma clang diagnostic pop
