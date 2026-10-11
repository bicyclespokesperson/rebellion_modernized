#include "ui/message_template.h"

#include <cstring>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/utility_classes.h"

namespace {

constexpr uint8_t kIndirectFieldFlag = 0x80;
constexpr int kFieldBytes = 6;

char ReadMarkerByte(HMODULE module, UINT id) {
    HGLOBAL resource = LoadResource(module, FindResourceA(module, MAKEINTRESOURCEA(id & 0xffff), RT_RCDATA));
    const char* bytes = static_cast<const char*>(LockResource(resource));
    const char marker = bytes ? *bytes : 0;
    FreeResource(resource);
    return marker;
}

int32_t ReadFieldArgument(const char* at) {
    int32_t value;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

}  // namespace

MessageTemplate::MessageTemplate(UINT endMarkerId, UINT fieldMarkerId, HMODULE module) {
    m_endMarker = ReadMarkerByte(module, endMarkerId);
    m_fieldMarker = ReadMarkerByte(module, fieldMarkerId);
    m_module = module;
    m_noField = nullptr;
}

MessageTemplate::~MessageTemplate() = default;

void MessageTemplate::Expand(String* out, UINT templateId, ObjectBase* field1, ObjectBase* field2,
                             ObjectBase* field3, ObjectBase* field4) {
    const char* text = nullptr;
    HGLOBAL resource = LoadResource(m_module, FindResourceA(m_module, MAKEINTRESOURCEA(templateId & 0xffff), RT_RCDATA));
    if (resource) text = static_cast<const char*>(LockResource(resource));
    if (text) {
        bool done = false;
        do {
            if (std::strncmp(text, &m_endMarker, 1) == 0) {
                done = true;
            } else if (std::strncmp(text, &m_fieldMarker, 1) == 0) {
                const uint8_t code = static_cast<uint8_t>(text[1]);
                ObjectBase* field;
                if (code & kIndirectFieldFlag) {
                    // BUG(original): indirect codes index the template object itself as a table of fields: 0 is
                    // the null m_noField, 1 and 2 reinterpret the marker bytes and the module handle.
                    field = reinterpret_cast<ObjectBase* const*>(this)[code & 0x7f];
                } else {
                    switch (code) {
                        case 1:
                            field = field1;
                            break;
                        case 2:
                            field = field2;
                            break;
                        case 3:
                            field = field3;
                            break;
                        case 4:
                            field = field4;
                            break;
                        default:
                            field = nullptr;
                            break;
                    }
                }
                if (field) field->WriteName(out, ReadFieldArgument(text + 2));
                text += kFieldBytes;
            } else {
                out->AppendChar(*text);
                ++text;
            }
        } while (!done);
    }
    FreeResource(resource);
}

void SetMessageTemplateSource(UINT endMarkerId, UINT fieldMarkerId, HMODULE module) {
    if (MessageTemplate* previous = g_messageTemplate) {
        delete previous;
        g_messageTemplate = nullptr;
    }
    if (module) g_messageTemplate = NewOrNull<MessageTemplate>(endMarkerId, fieldMarkerId, module);
}

void ExpandMessageTemplate(String* out, UINT templateId, ObjectBase* field1, ObjectBase* field2, ObjectBase* field3,
                           ObjectBase* field4) {
    if (g_messageTemplate) g_messageTemplate->Expand(out, templateId, field1, field2, field3, field4);
}
