#include "strategy_ui/ui_handler_table.h"

#include "core/stream_io.h"
#include "events/player_notices.h"

extern UiNotice* g_uiHandlers[kUiHandlerCount];
int g_uiHandlerTableNeedsInit = 1;

Notice2 g_notice2Handler{};
Notice1 g_notice1Handler{};
Notice8 g_notice8Handler{};
Notice6 g_notice6Handler{};
Notice5 g_notice5Handler{};
Notice4 g_notice4Handler{};
Notice3 g_notice3Handler{};
Notice9 g_notice9Handler{};
Notice10 g_notice10Handler{};
Notice11 g_notice11Handler{};
Notice12 g_notice12Handler{};
Notice13 g_notice13Handler{};
Notice14 g_notice14Handler{};
Notice16 g_notice16Handler{};
Notice15 g_notice15Handler{};
Notice7 g_notice7Handler{};
Notice19 g_notice19Handler{};
Notice20 g_notice20Handler{};
Notice21 g_notice21Handler{};
Notice22 g_notice22Handler{};
Notice23 g_notice23Handler{};
Notice24 g_notice24Handler{};
Notice25 g_notice25Handler{};
Notice26b g_notice26bHandler{};
Notice26a g_notice26aHandler{};
Notice30 g_notice30Handler{};
Notice28 g_notice28Handler{};
Notice29 g_notice29Handler{};
Notice32 g_notice32Handler{};
Notice31 g_notice31Handler{};
Notice36 g_notice36Handler{};
Notice35 g_notice35Handler{};
Notice37 g_notice37Handler{};
Notice38 g_notice38Handler{};
Notice39 g_notice39Handler{};
Notice40 g_notice40Handler{};
Notice33 g_notice33Handler{};
Notice34 g_notice34Handler{};
Notice41 g_notice41Handler{};
Notice42 g_notice42Handler{};
Notice43 g_notice43Handler{};
Notice45 g_notice45Handler{};
Notice44 g_notice44Handler{};

extern Notice2 g_notice2Handler;
extern Notice1 g_notice1Handler;
extern Notice8 g_notice8Handler;
extern Notice6 g_notice6Handler;
extern Notice5 g_notice5Handler;
extern Notice4 g_notice4Handler;
extern Notice3 g_notice3Handler;
extern Notice9 g_notice9Handler;
extern Notice10 g_notice10Handler;
extern Notice11 g_notice11Handler;
extern Notice12 g_notice12Handler;
extern Notice13 g_notice13Handler;
extern Notice14 g_notice14Handler;
extern Notice16 g_notice16Handler;
extern Notice15 g_notice15Handler;
extern Notice7 g_notice7Handler;
extern Notice19 g_notice19Handler;
extern Notice20 g_notice20Handler;
extern Notice21 g_notice21Handler;
extern Notice22 g_notice22Handler;
extern Notice23 g_notice23Handler;
extern Notice24 g_notice24Handler;
extern Notice25 g_notice25Handler;
extern Notice26b g_notice26bHandler;
extern Notice26a g_notice26aHandler;
extern Notice30 g_notice30Handler;
extern Notice28 g_notice28Handler;
extern Notice29 g_notice29Handler;
extern Notice32 g_notice32Handler;
extern Notice31 g_notice31Handler;
extern Notice36 g_notice36Handler;
extern Notice35 g_notice35Handler;
extern Notice37 g_notice37Handler;
extern Notice38 g_notice38Handler;
extern Notice39 g_notice39Handler;
extern Notice40 g_notice40Handler;
extern Notice33 g_notice33Handler;
extern Notice34 g_notice34Handler;
extern Notice41 g_notice41Handler;
extern Notice42 g_notice42Handler;
extern Notice43 g_notice43Handler;
extern Notice45 g_notice45Handler;
extern Notice44 g_notice44Handler;

namespace {

UiNotice* const kNoticePrototypes[] = {
    &g_notice2Handler,  &g_notice1Handler,  &g_notice8Handler,  &g_notice6Handler,  &g_notice5Handler,
    &g_notice4Handler,  &g_notice3Handler,  &g_notice9Handler,  &g_notice10Handler, &g_notice11Handler,
    &g_notice12Handler, &g_notice13Handler, &g_notice14Handler, &g_notice16Handler, &g_notice15Handler,
    &g_notice7Handler,  &g_notice19Handler, &g_notice20Handler, &g_notice21Handler, &g_notice22Handler,
    &g_notice23Handler, &g_notice24Handler, &g_notice25Handler, &g_notice26bHandler, &g_notice26aHandler,
    &g_notice30Handler, &g_notice28Handler, &g_notice29Handler, &g_notice32Handler, &g_notice31Handler,
    &g_notice36Handler, &g_notice35Handler, &g_notice37Handler, &g_notice38Handler, &g_notice39Handler,
    &g_notice40Handler, &g_notice33Handler, &g_notice34Handler, &g_notice41Handler, &g_notice42Handler,
    &g_notice43Handler, &g_notice45Handler, &g_notice44Handler,
};

}  // namespace

UiRecordTree::UiRecordTree() {
    if (g_uiHandlerTableNeedsInit == 0) return;
    for (UiNotice*& notice : g_uiHandlers) notice = nullptr;
    for (UiNotice* prototype : kNoticePrototypes) g_uiHandlers[prototype->GetKind()] = prototype;
    g_uiHandlerTableNeedsInit = 0;
}

UiRecordTree::~UiRecordTree() = default;

void UiRecordTree::Read(istream* in) {
    int16_t count;
    ReadValue16(in, &count);
    while (count != 0) {
        --count;
        uint32_t kind;
        ReadValue32(in, &kind);
        UiNotice* notice = CreateUiRecord(static_cast<int>(kind));
        notice->Read(in);
        AddWithId(notice);
    }
}

void UiRecordTree::Write(ostream* out) {
    int16_t count = Count();
    WriteValue16(out, &count);
    for (AvlNode* node = First(); node; node = node->m_successor) {
        UiNotice* notice = static_cast<UiNotice*>(node);
        int kind = notice->GetKind();
        WriteValue32(out, &kind);
        notice->Write(out);
    }
}

UiNotice* CreateUiRecord(int kind) { return g_uiHandlers[kind]->Virtual10(); }
