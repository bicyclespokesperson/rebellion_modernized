#include "gameobj/game_obj.h"

// Notification debug log. The original guards every entry with a "logging enabled?" call that the linker folded into
// the shared `return 0` stub, so none of the formatting ever runs. The wrappers below keep that structure (and their
// argument conventions); the formatter itself, LogChange (0x4f8c60), is not reconstructed.

int icf_return_zero();
String* FUN_00553c80(ObjectKey* key, String* out);
void FUN_0051ccc0(ObjectKey* key, String* out);

namespace {

const char* sideName(uint32_t side) {
    switch (side) {
        case 0:
            return "None   ";
        case 1:
            return "Alliance";
        case 2:
            return "Empire  ";
        case 3:
            return "Neutral ";
        default:
            return "UNKNOWN!";
    }
}

}  // namespace

int GameObj::LogFlag(const char* tag, const char* property, int value, int unused, NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) result = LogChange(tag, property, value ? "TRUE " : "FALSE", "", reinterpret_cast<const char*>(unused), ctx);
    return result;
}

int GameObj::LogSide(const char* tag, const char* property, uint32_t newSide, uint32_t oldSide, int unused,
                     NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) {
        result = LogChange(tag, property, sideName(newSide), sideName(oldSide), reinterpret_cast<const char*>(unused), ctx);
    }
    return result;
}

int GameObj::LogInt(const char* tag, const char* property, int newValue, int oldValue, int unused, NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) {
        String newText;
        String oldText;
        newText.AppendSigned(newValue);
        oldText.AppendSigned(oldValue);
        result = LogChange(tag, property, newText.m_text, oldText.m_text, reinterpret_cast<const char*>(unused), ctx);
    }
    return result;
}

int GameObj::LogString(const char* tag, const char* property, String* newValue, String* oldValue, int unused,
                       NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) {
        result = LogChange(tag, property, newValue->m_text, oldValue->m_text, reinterpret_cast<const char*>(unused), ctx);
    }
    return result;
}

int GameObj::LogKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, int unused,
                    NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) {
        String newText;
        String oldText;
        FUN_00553c80(newKey, &newText);
        FUN_00553c80(oldKey, &oldText);
        result = LogChange(tag, property, newText.m_text, oldText.m_text, reinterpret_cast<const char*>(unused), ctx);
    }
    return result;
}

int GameObj::LogTypeKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, int unused,
                        NotifyContext* ctx) {
    int result = 1;
    if (icf_return_zero()) {
        String newText;
        String oldText;
        FUN_0051ccc0(newKey, &newText);
        FUN_0051ccc0(oldKey, &oldText);
        result = LogChange(tag, property, newText.m_text, oldText.m_text, reinterpret_cast<const char*>(unused), ctx);
    }
    return result;
}
