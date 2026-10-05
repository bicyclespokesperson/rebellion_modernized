#include "strategy_ui/ui_handler_table.h"

#include <cstdint>

#include "core/containers.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

// The handler singletons are function-local statics of the original constructor: a guard byte per eight of them,
// constructed on first use with their destructor registered through atexit. They belong to the player-action modules
// (0x48aac0-0x49bda0), which are not reconstructed yet, so objects, constructors and the atexit stubs are declared by address.

extern uint8_t DAT_006b169c;
extern uint8_t DAT_006b1af8;
extern uint8_t DAT_006b1c2c;
extern uint8_t DAT_006b1ed4;
extern uint8_t DAT_006b22ec;
extern uint8_t DAT_006b24ec;

extern uint8_t DAT_006b24f0[];
extern uint8_t DAT_006b2358[];
extern uint8_t DAT_006b1b68[];
extern uint8_t DAT_006b16a0[];
extern uint8_t DAT_006b1bc8[];
extern uint8_t DAT_006b2060[];
extern uint8_t DAT_006b21b0[];
extern uint8_t DAT_006b2830[];
extern uint8_t DAT_006b2570[];
extern uint8_t DAT_006b1e70[];
extern uint8_t DAT_006b1878[];
extern uint8_t DAT_006b2218[];
extern uint8_t DAT_006b1770[];
extern uint8_t DAT_006b14e8[];
extern uint8_t DAT_006b2788[];
extern uint8_t DAT_006b1950[];
extern uint8_t DAT_006b1a20[];
extern uint8_t DAT_006b2130[];
extern uint8_t DAT_006b1d98[];
extern uint8_t DAT_006b1c30[];
extern uint8_t DAT_006b17e0[];
extern uint8_t DAT_006b2720[];
extern uint8_t DAT_006b1708[];
extern uint8_t DAT_006b2488[];
extern uint8_t DAT_006b26b8[];
extern uint8_t DAT_006b20c8[];
extern uint8_t DAT_006b1e08[];
extern uint8_t DAT_006b1b00[];
extern uint8_t DAT_006b25d8[];
extern uint8_t DAT_006b1a90[];
extern uint8_t DAT_006b18e0[];
extern uint8_t DAT_006b1d30[];
extern uint8_t DAT_006b2650[];
extern uint8_t DAT_006b19b8[];
extern uint8_t DAT_006b2288[];
extern uint8_t DAT_006b1630[];
extern uint8_t DAT_006b15c0[];
extern uint8_t DAT_006b1cc8[];
extern uint8_t DAT_006b2420[];
extern uint8_t DAT_006b22f0[];
extern uint8_t DAT_006b1ff8[];
extern uint8_t DAT_006b1ed8[];
extern uint8_t DAT_006b1558[];

void __thiscall FUN_0049bda0(void* self);
void __thiscall FUN_0049aad0(void* self);
void __thiscall FUN_0049a120(void* self);
void __thiscall FUN_00499ce0(void* self);
void __thiscall FUN_004999c0(void* self);
void __thiscall FUN_00499680(void* self);
void __thiscall FUN_00499360(void* self);
void __thiscall FUN_00499030(void* self);
void __thiscall FUN_00498e20(void* self);
void __thiscall FUN_00498bb0(void* self);
void __thiscall FUN_00498870(void* self);
void __thiscall FUN_004980c0(void* self);
void __thiscall FUN_00497810(void* self);
void __thiscall FUN_00497560(void* self);
void __thiscall FUN_00496380(void* self);
void __thiscall FUN_00496010(void* self);
void __thiscall FUN_00495d50(void* self);
void __thiscall FUN_004953c0(void* self);
void __thiscall FUN_00494ef0(void* self);
void __thiscall FUN_004923a0(void* self);
void __thiscall FUN_00491190(void* self);
void __thiscall FUN_00490f80(void* self);
void __thiscall FUN_00490250(void* self);
void __thiscall FUN_0048fd00(void* self);
void __thiscall FUN_0048f910(void* self);
void __thiscall FUN_0048f690(void* self);
void __thiscall FUN_0048f0e0(void* self);
void __thiscall FUN_0048ec80(void* self);
void __thiscall FUN_0048e690(void* self);
void __thiscall FUN_0048e490(void* self);
void __thiscall FUN_0048d650(void* self);
void __thiscall FUN_0048d370(void* self);
void __thiscall FUN_0048cff0(void* self);
void __thiscall FUN_0048cd80(void* self);
void __thiscall FUN_0048c8c0(void* self);
void __thiscall FUN_0048c320(void* self);
void __thiscall FUN_0048bc80(void* self);
void __thiscall FUN_0048b860(void* self);
void __thiscall FUN_0048b5e0(void* self);
void __thiscall FUN_0048b270(void* self);
void __thiscall FUN_0048af50(void* self);
void __thiscall FUN_0048ad40(void* self);
void __thiscall FUN_0048aac0(void* self);
int __thiscall FUN_0048c2a0(void* self);
int __thiscall FUN_00492380(void* self);

void FUN_00437660();
void FUN_00437650();
void FUN_00437640();
void FUN_00437630();
void FUN_00437620();
void FUN_00437610();
void FUN_00437600();
void FUN_004375f0();
void FUN_004375e0();
void FUN_004375d0();
void FUN_004375c0();
void FUN_004375b0();
void FUN_004375a0();
void FUN_00437590();
void FUN_00437580();
void FUN_00437570();
void FUN_00437560();
void FUN_00437550();
void FUN_00437540();
void FUN_00437530();
void FUN_00437520();
void FUN_00437510();
void FUN_00437500();
void FUN_004374f0();
void FUN_004374e0();
void FUN_004374d0();
void FUN_004374c0();
void FUN_004374b0();
void FUN_004374a0();
void FUN_00437490();
void FUN_00437480();
void FUN_00437470();
void FUN_00437460();
void FUN_00437450();
void FUN_00437440();
void FUN_00437430();
void FUN_00437420();
void FUN_00437410();
void FUN_00437400();
void FUN_004373f0();
void FUN_004373e0();
void FUN_004373d0();
void FUN_004373c0();

extern int g_uiHandlerTableNeedsInit;  // 0x6a8704
extern UiHandler* g_uiHandlers[0x2e];  // 0x6b1f40

namespace {

using Constructor = void(__thiscall*)(void*);
using KindOf = int(__thiscall*)(void*);

struct HandlerInit {
    uint8_t* m_guard;
    uint8_t m_guardBit;
    Constructor m_construct;
    uint8_t* m_object;
    void(__cdecl* m_destroyAtExit)();
    int m_slot;       // -1: ask m_kindOf
    KindOf m_kindOf;
};

const HandlerInit kHandlerInits[] = {
    {&DAT_006b24ec, 0x01, FUN_0049bda0, DAT_006b24f0, FUN_00437660, 2, nullptr},
    {&DAT_006b24ec, 0x02, FUN_0049aad0, DAT_006b2358, FUN_00437650, 1, nullptr},
    {&DAT_006b24ec, 0x04, FUN_0049a120, DAT_006b1b68, FUN_00437640, -1, FUN_0048c2a0},
    {&DAT_006b24ec, 0x08, FUN_00499ce0, DAT_006b16a0, FUN_00437630, 6, nullptr},
    {&DAT_006b24ec, 0x10, FUN_004999c0, DAT_006b1bc8, FUN_00437620, 5, nullptr},
    {&DAT_006b24ec, 0x20, FUN_00499680, DAT_006b2060, FUN_00437610, 4, nullptr},
    {&DAT_006b24ec, 0x40, FUN_00499360, DAT_006b21b0, FUN_00437600, 3, nullptr},
    {&DAT_006b24ec, 0x80, FUN_00499030, DAT_006b2830, FUN_004375f0, 9, nullptr},
    {&DAT_006b169c, 0x01, FUN_00498e20, DAT_006b2570, FUN_004375e0, 10, nullptr},
    {&DAT_006b169c, 0x02, FUN_00498bb0, DAT_006b1e70, FUN_004375d0, 11, nullptr},
    {&DAT_006b169c, 0x04, FUN_00498870, DAT_006b1878, FUN_004375c0, 12, nullptr},
    {&DAT_006b169c, 0x08, FUN_004980c0, DAT_006b2218, FUN_004375b0, 13, nullptr},
    {&DAT_006b169c, 0x10, FUN_00497810, DAT_006b1770, FUN_004375a0, 14, nullptr},
    {&DAT_006b169c, 0x20, FUN_00497560, DAT_006b14e8, FUN_00437590, 16, nullptr},
    {&DAT_006b169c, 0x40, FUN_00496380, DAT_006b2788, FUN_00437580, 15, nullptr},
    {&DAT_006b169c, 0x80, FUN_00496010, DAT_006b1950, FUN_00437570, 7, nullptr},
    {&DAT_006b1af8, 0x01, FUN_00495d50, DAT_006b1a20, FUN_00437560, 19, nullptr},
    {&DAT_006b1af8, 0x02, FUN_004953c0, DAT_006b2130, FUN_00437550, 20, nullptr},
    {&DAT_006b1af8, 0x04, FUN_00494ef0, DAT_006b1d98, FUN_00437540, 21, nullptr},
    {&DAT_006b1af8, 0x08, FUN_004923a0, DAT_006b1c30, FUN_00437530, -1, FUN_00492380},
    {&DAT_006b1af8, 0x10, FUN_00491190, DAT_006b17e0, FUN_00437520, -1, FUN_00492380},
    {&DAT_006b1af8, 0x20, FUN_00490f80, DAT_006b2720, FUN_00437510, 24, nullptr},
    {&DAT_006b1af8, 0x40, FUN_00490250, DAT_006b1708, FUN_00437500, 25, nullptr},
    {&DAT_006b1af8, 0x80, FUN_0048fd00, DAT_006b2488, FUN_004374f0, 26, nullptr},
    {&DAT_006b1c2c, 0x01, FUN_0048f910, DAT_006b26b8, FUN_004374e0, 26, nullptr},
    {&DAT_006b1c2c, 0x02, FUN_0048f690, DAT_006b20c8, FUN_004374d0, 30, nullptr},
    {&DAT_006b1c2c, 0x04, FUN_0048f0e0, DAT_006b1e08, FUN_004374c0, 28, nullptr},
    {&DAT_006b1c2c, 0x08, FUN_0048ec80, DAT_006b1b00, FUN_004374b0, 29, nullptr},
    {&DAT_006b1c2c, 0x10, FUN_0048e690, DAT_006b25d8, FUN_004374a0, 32, nullptr},
    {&DAT_006b1c2c, 0x20, FUN_0048e490, DAT_006b1a90, FUN_00437490, 31, nullptr},
    {&DAT_006b1c2c, 0x40, FUN_0048d650, DAT_006b18e0, FUN_00437480, 36, nullptr},
    {&DAT_006b1c2c, 0x80, FUN_0048d370, DAT_006b1d30, FUN_00437470, 35, nullptr},
    {&DAT_006b22ec, 0x01, FUN_0048cff0, DAT_006b2650, FUN_00437460, 37, nullptr},
    {&DAT_006b22ec, 0x02, FUN_0048cd80, DAT_006b19b8, FUN_00437450, 38, nullptr},
    {&DAT_006b22ec, 0x04, FUN_0048c8c0, DAT_006b2288, FUN_00437440, 39, nullptr},
    {&DAT_006b22ec, 0x08, FUN_0048c320, DAT_006b1630, FUN_00437430, 40, nullptr},
    {&DAT_006b22ec, 0x10, FUN_0048bc80, DAT_006b15c0, FUN_00437420, 33, nullptr},
    {&DAT_006b22ec, 0x20, FUN_0048b860, DAT_006b1cc8, FUN_00437410, 34, nullptr},
    {&DAT_006b22ec, 0x40, FUN_0048b5e0, DAT_006b2420, FUN_00437400, 41, nullptr},
    {&DAT_006b22ec, 0x80, FUN_0048b270, DAT_006b22f0, FUN_004373f0, 42, nullptr},
    {&DAT_006b1ed4, 0x01, FUN_0048af50, DAT_006b1ff8, FUN_004373e0, 43, nullptr},
    {&DAT_006b1ed4, 0x02, FUN_0048ad40, DAT_006b1ed8, FUN_004373d0, 45, nullptr},
    {&DAT_006b1ed4, 0x04, FUN_0048aac0, DAT_006b1558, FUN_004373c0, 44, nullptr},
};

void InitializeHandler(const HandlerInit& init) {
    if ((*init.m_guard & init.m_guardBit) == 0) {
        *init.m_guard |= init.m_guardBit;
        init.m_construct(init.m_object);
        atexit(init.m_destroyAtExit);
    }
    const int slot = init.m_kindOf ? init.m_kindOf(init.m_object) : init.m_slot;
    g_uiHandlers[slot] = reinterpret_cast<UiHandler*>(init.m_object);
}

}  // namespace

UiRecordTree::UiRecordTree() {
    if (g_uiHandlerTableNeedsInit == 0) return;
    for (UiHandler*& handler : g_uiHandlers) handler = nullptr;
    for (const HandlerInit& init : kHandlerInits) InitializeHandler(init);
    g_uiHandlerTableNeedsInit = 0;
}

UiRecordTree::~UiRecordTree() { REB_KEEP_VPTR_STORE(); }

void UiRecordTree::Read(istream* in) {
    int16_t count;
    ReadValue16(in, &count);
    while (count != 0) {
        --count;
        uint32_t kind;
        ReadValue32(in, &kind);
        UiRecord* record = CreateUiRecord(kind);
        record->Read(in);
        AddWithId(record);
    }
}

void UiRecordTree::Write(ostream* out) {
    int16_t count = Count();
    WriteValue16(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        UiRecord* record = static_cast<UiRecord*>(node);
        int kind = record->GetKind();
        WriteValue32(out, &kind);
        record->Write(out);
    }
}

UiRecord* CreateUiRecord(int kind) { return g_uiHandlers[kind]->CreateRecord(); }

// ---- Static initializers of three UI globals (a zeroed pointer each, destroyed at exit) -----------------------

void __thiscall FUN_0042dbe0(void* self);
extern uint8_t DAT_006b28a4[];
extern uint8_t DAT_006b28a0[];
extern uint8_t DAT_006b289c[];
void FUN_004377c0();
void FUN_00437810();
void FUN_00437860();

void StaticInitGlobalA() {
    InitGlobalA();
    RegisterGlobalADestructor();
}
void InitGlobalA() { FUN_0042dbe0(DAT_006b28a4); }
void RegisterGlobalADestructor() { atexit(FUN_004377c0); }
void StaticInitGlobalB() {
    InitGlobalB();
    RegisterGlobalBDestructor();
}
void InitGlobalB() { FUN_0042dbe0(DAT_006b28a0); }
void RegisterGlobalBDestructor() { atexit(FUN_00437810); }
void StaticInitGlobalC() {
    InitGlobalC();
    RegisterGlobalCDestructor();
}
void InitGlobalC() { FUN_0042dbe0(DAT_006b289c); }
void RegisterGlobalCDestructor() { atexit(FUN_00437860); }
