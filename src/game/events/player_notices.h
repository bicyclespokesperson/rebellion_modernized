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
#pragma clang diagnostic pop
