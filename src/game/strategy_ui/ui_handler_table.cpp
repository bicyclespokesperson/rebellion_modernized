#include "core/reb_abi.h"
#include "strategy_ui/ui_handler_table.h"

#include <cstdint>
#include <new>

#include "core/containers.h"
#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "events/player_notices.h"
#include "strategy_ui/manufacture_order_window.h"

extern uint8_t g_noticeHandlerGuardB;  // 0x006b169c
extern uint8_t g_noticeHandlerGuardC;  // 0x006b1af8
extern uint8_t g_noticeHandlerGuardD;  // 0x006b1c2c
extern uint8_t g_noticeHandlerGuardF;  // 0x006b1ed4
extern uint8_t g_noticeHandlerGuardE;  // 0x006b22ec
extern uint8_t g_noticeHandlerGuardA;  // 0x006b24ec

extern Notice2 g_notice2Handler;  // 0x006b24f0
extern Notice1 g_notice1Handler;  // 0x006b2358
extern Notice8 g_notice8Handler;  // 0x006b1b68
extern Notice6 g_notice6Handler;  // 0x006b16a0
extern Notice5 g_notice5Handler;  // 0x006b1bc8
extern Notice4 g_notice4Handler;  // 0x006b2060
extern Notice3 g_notice3Handler;  // 0x006b21b0
extern Notice9 g_notice9Handler;  // 0x006b2830
extern Notice10 g_notice10Handler;  // 0x006b2570
extern Notice11 g_notice11Handler;  // 0x006b1e70
extern Notice12 g_notice12Handler;  // 0x006b1878
extern Notice13 g_notice13Handler;  // 0x006b2218
extern Notice14 g_notice14Handler;  // 0x006b1770
extern Notice16 g_notice16Handler;  // 0x006b14e8
extern Notice15 g_notice15Handler;  // 0x006b2788
extern Notice7 g_notice7Handler;  // 0x006b1950
extern Notice19 g_notice19Handler;  // 0x006b1a20
extern Notice20 g_notice20Handler;  // 0x006b2130
extern Notice21 g_notice21Handler;  // 0x006b1d98
extern Notice22 g_notice22Handler;  // 0x006b1c30
extern Notice23 g_notice23Handler;  // 0x006b17e0
extern Notice24 g_notice24Handler;  // 0x006b2720
extern Notice25 g_notice25Handler;  // 0x006b1708
extern Notice26b g_notice26bHandler;  // 0x006b2488
extern Notice26a g_notice26aHandler;  // 0x006b26b8
extern Notice30 g_notice30Handler;  // 0x006b20c8
extern Notice28 g_notice28Handler;  // 0x006b1e08
extern Notice29 g_notice29Handler;  // 0x006b1b00
extern Notice32 g_notice32Handler;  // 0x006b25d8
extern Notice31 g_notice31Handler;  // 0x006b1a90
extern Notice36 g_notice36Handler;  // 0x006b18e0
extern Notice35 g_notice35Handler;  // 0x006b1d30
extern Notice37 g_notice37Handler;  // 0x006b2650
extern Notice38 g_notice38Handler;  // 0x006b19b8
extern Notice39 g_notice39Handler;  // 0x006b2288
extern Notice40 g_notice40Handler;  // 0x006b1630
extern Notice33 g_notice33Handler;  // 0x006b15c0
extern Notice34 g_notice34Handler;  // 0x006b1cc8
extern Notice41 g_notice41Handler;  // 0x006b2420
extern Notice42 g_notice42Handler;  // 0x006b22f0
extern Notice43 g_notice43Handler;  // 0x006b1ff8
extern Notice45 g_notice45Handler;  // 0x006b1ed8
extern Notice44 g_notice44Handler;  // 0x006b1558


void DestroyNotice2Handler();
void DestroyNotice1Handler();
void DestroyNotice8Handler();
void DestroyNotice6Handler();
void DestroyNotice5Handler();
void DestroyNotice4Handler();
void DestroyNotice3Handler();
void DestroyNotice9Handler();
void DestroyNotice10Handler();
void DestroyNotice11Handler();
void DestroyNotice12Handler();
void DestroyNotice13Handler();
void DestroyNotice14Handler();
void DestroyNotice16Handler();
void DestroyNotice15Handler();
void DestroyNotice7Handler();
void DestroyNotice19Handler();
void DestroyNotice20Handler();
void DestroyNotice21Handler();
void DestroyNotice22Handler();
void DestroyNotice23Handler();
void DestroyNotice24Handler();
void DestroyNotice25Handler();
void DestroyNotice26bHandler();
void DestroyNotice26aHandler();
void DestroyNotice30Handler();
void DestroyNotice28Handler();
void DestroyNotice29Handler();
void DestroyNotice32Handler();
void DestroyNotice31Handler();
void DestroyNotice36Handler();
void DestroyNotice35Handler();
void DestroyNotice37Handler();
void DestroyNotice38Handler();
void DestroyNotice39Handler();
void DestroyNotice40Handler();
void DestroyNotice33Handler();
void DestroyNotice34Handler();
void DestroyNotice41Handler();
void DestroyNotice42Handler();
void DestroyNotice43Handler();
void DestroyNotice45Handler();
void DestroyNotice44Handler();

extern int g_uiHandlerTableNeedsInit;  // 0x6a8704
extern UiHandler* g_uiHandlers[0x2e];  // 0x6b1f40

namespace {

using Constructor = void (*)(void*);
using KindFn = int (*)(void*);

template <class T>
void Construct(void* memory) {
    new (memory) T();
}

template <class T>
int KindOf(void* object) {
    return static_cast<T*>(object)->GetKind();
}


struct HandlerInit {
    uint8_t* m_guard;
    uint8_t m_guardBit;
    Constructor m_construct;
    void* m_object;
    void(REB_CDECL* m_destroyAtExit)();
    int m_slot;       // -1: ask m_kindOf
    KindFn m_kindOf;
};

const HandlerInit kHandlerInits[] = {
    {&g_noticeHandlerGuardA, 0x01, Construct<Notice2>, &g_notice2Handler, DestroyNotice2Handler, 2, nullptr},
    {&g_noticeHandlerGuardA, 0x02, Construct<Notice1>, &g_notice1Handler, DestroyNotice1Handler, 1, nullptr},
    {&g_noticeHandlerGuardA, 0x04, Construct<Notice8>, &g_notice8Handler, DestroyNotice8Handler, -1, KindOf<Notice8>},
    {&g_noticeHandlerGuardA, 0x08, Construct<Notice6>, &g_notice6Handler, DestroyNotice6Handler, 6, nullptr},
    {&g_noticeHandlerGuardA, 0x10, Construct<Notice5>, &g_notice5Handler, DestroyNotice5Handler, 5, nullptr},
    {&g_noticeHandlerGuardA, 0x20, Construct<Notice4>, &g_notice4Handler, DestroyNotice4Handler, 4, nullptr},
    {&g_noticeHandlerGuardA, 0x40, Construct<Notice3>, &g_notice3Handler, DestroyNotice3Handler, 3, nullptr},
    {&g_noticeHandlerGuardA, 0x80, Construct<Notice9>, &g_notice9Handler, DestroyNotice9Handler, 9, nullptr},
    {&g_noticeHandlerGuardB, 0x01, Construct<Notice10>, &g_notice10Handler, DestroyNotice10Handler, 10, nullptr},
    {&g_noticeHandlerGuardB, 0x02, Construct<Notice11>, &g_notice11Handler, DestroyNotice11Handler, 11, nullptr},
    {&g_noticeHandlerGuardB, 0x04, Construct<Notice12>, &g_notice12Handler, DestroyNotice12Handler, 12, nullptr},
    {&g_noticeHandlerGuardB, 0x08, Construct<Notice13>, &g_notice13Handler, DestroyNotice13Handler, 13, nullptr},
    {&g_noticeHandlerGuardB, 0x10, Construct<Notice14>, &g_notice14Handler, DestroyNotice14Handler, 14, nullptr},
    {&g_noticeHandlerGuardB, 0x20, Construct<Notice16>, &g_notice16Handler, DestroyNotice16Handler, 16, nullptr},
    {&g_noticeHandlerGuardB, 0x40, Construct<Notice15>, &g_notice15Handler, DestroyNotice15Handler, 15, nullptr},
    {&g_noticeHandlerGuardB, 0x80, Construct<Notice7>, &g_notice7Handler, DestroyNotice7Handler, 7, nullptr},
    {&g_noticeHandlerGuardC, 0x01, Construct<Notice19>, &g_notice19Handler, DestroyNotice19Handler, 19, nullptr},
    {&g_noticeHandlerGuardC, 0x02, Construct<Notice20>, &g_notice20Handler, DestroyNotice20Handler, 20, nullptr},
    {&g_noticeHandlerGuardC, 0x04, Construct<Notice21>, &g_notice21Handler, DestroyNotice21Handler, 21, nullptr},
    {&g_noticeHandlerGuardC, 0x08, Construct<Notice22>, &g_notice22Handler, DestroyNotice22Handler, -1, KindOf<Notice22>},
    {&g_noticeHandlerGuardC, 0x10, Construct<Notice23>, &g_notice23Handler, DestroyNotice23Handler, -1, KindOf<Notice23>},
    {&g_noticeHandlerGuardC, 0x20, Construct<Notice24>, &g_notice24Handler, DestroyNotice24Handler, 24, nullptr},
    {&g_noticeHandlerGuardC, 0x40, Construct<Notice25>, &g_notice25Handler, DestroyNotice25Handler, 25, nullptr},
    {&g_noticeHandlerGuardC, 0x80, Construct<Notice26b>, &g_notice26bHandler, DestroyNotice26bHandler, 26, nullptr},
    {&g_noticeHandlerGuardD, 0x01, Construct<Notice26a>, &g_notice26aHandler, DestroyNotice26aHandler, 26, nullptr},
    {&g_noticeHandlerGuardD, 0x02, Construct<Notice30>, &g_notice30Handler, DestroyNotice30Handler, 30, nullptr},
    {&g_noticeHandlerGuardD, 0x04, Construct<Notice28>, &g_notice28Handler, DestroyNotice28Handler, 28, nullptr},
    {&g_noticeHandlerGuardD, 0x08, Construct<Notice29>, &g_notice29Handler, DestroyNotice29Handler, 29, nullptr},
    {&g_noticeHandlerGuardD, 0x10, Construct<Notice32>, &g_notice32Handler, DestroyNotice32Handler, 32, nullptr},
    {&g_noticeHandlerGuardD, 0x20, Construct<Notice31>, &g_notice31Handler, DestroyNotice31Handler, 31, nullptr},
    {&g_noticeHandlerGuardD, 0x40, Construct<Notice36>, &g_notice36Handler, DestroyNotice36Handler, 36, nullptr},
    {&g_noticeHandlerGuardD, 0x80, Construct<Notice35>, &g_notice35Handler, DestroyNotice35Handler, 35, nullptr},
    {&g_noticeHandlerGuardE, 0x01, Construct<Notice37>, &g_notice37Handler, DestroyNotice37Handler, 37, nullptr},
    {&g_noticeHandlerGuardE, 0x02, Construct<Notice38>, &g_notice38Handler, DestroyNotice38Handler, 38, nullptr},
    {&g_noticeHandlerGuardE, 0x04, Construct<Notice39>, &g_notice39Handler, DestroyNotice39Handler, 39, nullptr},
    {&g_noticeHandlerGuardE, 0x08, Construct<Notice40>, &g_notice40Handler, DestroyNotice40Handler, 40, nullptr},
    {&g_noticeHandlerGuardE, 0x10, Construct<Notice33>, &g_notice33Handler, DestroyNotice33Handler, 33, nullptr},
    {&g_noticeHandlerGuardE, 0x20, Construct<Notice34>, &g_notice34Handler, DestroyNotice34Handler, 34, nullptr},
    {&g_noticeHandlerGuardE, 0x40, Construct<Notice41>, &g_notice41Handler, DestroyNotice41Handler, 41, nullptr},
    {&g_noticeHandlerGuardE, 0x80, Construct<Notice42>, &g_notice42Handler, DestroyNotice42Handler, 42, nullptr},
    {&g_noticeHandlerGuardF, 0x01, Construct<Notice43>, &g_notice43Handler, DestroyNotice43Handler, 43, nullptr},
    {&g_noticeHandlerGuardF, 0x02, Construct<Notice45>, &g_notice45Handler, DestroyNotice45Handler, 45, nullptr},
    {&g_noticeHandlerGuardF, 0x04, Construct<Notice44>, &g_notice44Handler, DestroyNotice44Handler, 44, nullptr},
};

void InitializeHandler(const HandlerInit& init) {
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


void InitGlobalA() { g_lastShipType.m_value = 0; }

void InitGlobalB() { g_lastTroopType.m_value = 0; }

void InitGlobalC() { g_lastFacilityType.m_value = 0; }

namespace {

template <class T>
void DestroyHandler(T& object) { object.~T(); }

}  // namespace

void DestroyNotice44Handler() { DestroyHandler(g_notice44Handler); }
void DestroyNotice45Handler() { DestroyHandler(g_notice45Handler); }
void DestroyNotice43Handler() { DestroyHandler(g_notice43Handler); }
void DestroyNotice42Handler() { DestroyHandler(g_notice42Handler); }
void DestroyNotice41Handler() { DestroyHandler(g_notice41Handler); }
void DestroyNotice34Handler() { DestroyHandler(g_notice34Handler); }
void DestroyNotice33Handler() { DestroyHandler(g_notice33Handler); }
void DestroyNotice40Handler() { DestroyHandler(g_notice40Handler); }
void DestroyNotice39Handler() { DestroyHandler(g_notice39Handler); }
void DestroyNotice38Handler() { DestroyHandler(g_notice38Handler); }
void DestroyNotice37Handler() { DestroyHandler(g_notice37Handler); }
void DestroyNotice35Handler() { DestroyHandler(g_notice35Handler); }
void DestroyNotice36Handler() { DestroyHandler(g_notice36Handler); }
void DestroyNotice31Handler() { DestroyHandler(g_notice31Handler); }
void DestroyNotice32Handler() { DestroyHandler(g_notice32Handler); }
void DestroyNotice29Handler() { DestroyHandler(g_notice29Handler); }
void DestroyNotice28Handler() { DestroyHandler(g_notice28Handler); }
void DestroyNotice30Handler() { DestroyHandler(g_notice30Handler); }
void DestroyNotice26aHandler() { DestroyHandler(g_notice26aHandler); }
void DestroyNotice26bHandler() { DestroyHandler(g_notice26bHandler); }
void DestroyNotice25Handler() { DestroyHandler(g_notice25Handler); }
void DestroyNotice24Handler() { DestroyHandler(g_notice24Handler); }
void DestroyNotice23Handler() { DestroyHandler(g_notice23Handler); }
void DestroyNotice22Handler() { DestroyHandler(g_notice22Handler); }
void DestroyNotice21Handler() { DestroyHandler(g_notice21Handler); }
void DestroyNotice20Handler() { DestroyHandler(g_notice20Handler); }
void DestroyNotice19Handler() { DestroyHandler(g_notice19Handler); }
void DestroyNotice7Handler() { DestroyHandler(g_notice7Handler); }
void DestroyNotice15Handler() { DestroyHandler(g_notice15Handler); }
void DestroyNotice16Handler() { DestroyHandler(g_notice16Handler); }
void DestroyNotice14Handler() { DestroyHandler(g_notice14Handler); }
void DestroyNotice13Handler() { DestroyHandler(g_notice13Handler); }
void DestroyNotice12Handler() { DestroyHandler(g_notice12Handler); }
void DestroyNotice11Handler() { DestroyHandler(g_notice11Handler); }
void DestroyNotice10Handler() { DestroyHandler(g_notice10Handler); }
void DestroyNotice9Handler() { DestroyHandler(g_notice9Handler); }
void DestroyNotice3Handler() { DestroyHandler(g_notice3Handler); }
void DestroyNotice4Handler() { DestroyHandler(g_notice4Handler); }
void DestroyNotice5Handler() { DestroyHandler(g_notice5Handler); }
void DestroyNotice6Handler() { DestroyHandler(g_notice6Handler); }
void DestroyNotice8Handler() { DestroyHandler(g_notice8Handler); }
void DestroyNotice1Handler() { DestroyHandler(g_notice1Handler); }
void DestroyNotice2Handler() { DestroyHandler(g_notice2Handler); }

Notice1 g_notice1Handler{};
Notice10 g_notice10Handler{};
Notice11 g_notice11Handler{};
Notice12 g_notice12Handler{};
Notice13 g_notice13Handler{};
Notice14 g_notice14Handler{};
Notice15 g_notice15Handler{};
Notice16 g_notice16Handler{};
Notice19 g_notice19Handler{};
Notice2 g_notice2Handler{};
Notice20 g_notice20Handler{};
Notice21 g_notice21Handler{};
Notice22 g_notice22Handler{};
Notice23 g_notice23Handler{};
Notice24 g_notice24Handler{};
Notice25 g_notice25Handler{};
Notice26a g_notice26aHandler{};
Notice26b g_notice26bHandler{};
Notice28 g_notice28Handler{};
Notice29 g_notice29Handler{};
Notice3 g_notice3Handler{};
Notice30 g_notice30Handler{};
Notice31 g_notice31Handler{};
Notice32 g_notice32Handler{};
Notice33 g_notice33Handler{};
Notice34 g_notice34Handler{};
Notice35 g_notice35Handler{};
Notice36 g_notice36Handler{};
Notice37 g_notice37Handler{};
Notice38 g_notice38Handler{};
Notice39 g_notice39Handler{};
Notice4 g_notice4Handler{};
Notice40 g_notice40Handler{};
Notice41 g_notice41Handler{};
Notice42 g_notice42Handler{};
Notice43 g_notice43Handler{};
Notice44 g_notice44Handler{};
Notice45 g_notice45Handler{};
Notice5 g_notice5Handler{};
Notice6 g_notice6Handler{};
Notice7 g_notice7Handler{};
Notice8 g_notice8Handler{};
Notice9 g_notice9Handler{};
int g_uiHandlerTableNeedsInit = 1;
uint8_t g_noticeHandlerGuardA{};
uint8_t g_noticeHandlerGuardB{};
uint8_t g_noticeHandlerGuardC{};
uint8_t g_noticeHandlerGuardD{};
uint8_t g_noticeHandlerGuardE{};
uint8_t g_noticeHandlerGuardF{};
