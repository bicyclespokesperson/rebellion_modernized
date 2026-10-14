#include "ui/message_template.h"
#include <string>

#include <cstring>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/utility_classes.h"

namespace
{

constexpr uint8_t kIndirectFieldFlag = 0x80;
constexpr int kFieldBytes = 6;

char ReadMarkerByte(HMODULE module, UINT id)
{
  HRSRC resourceInfo = module ? FindResourceA(module, MAKEINTRESOURCEA(id & 0xffff), RT_RCDATA) : nullptr;
  HGLOBAL resource = resourceInfo ? LoadResource(module, resourceInfo) : nullptr;
  const char* bytes = resource ? static_cast<const char*>(LockResource(resource)) : nullptr;
  const char marker = bytes && SizeofResource(module, resourceInfo) != 0 ? *bytes : 0;
  if (resource)
    FreeResource(resource);
  return marker;
}

int32_t ReadFieldArgument(const char* at)
{
  int32_t value;
  std::memcpy(&value, at, sizeof(value));
  return value;
}

} // namespace

MessageTemplate::MessageTemplate(UINT endMarkerId, UINT fieldMarkerId, HMODULE module)
{
  m_endMarker = ReadMarkerByte(module, endMarkerId);
  m_fieldMarker = ReadMarkerByte(module, fieldMarkerId);
  m_module = module;
  m_noField = nullptr;
}

MessageTemplate::~MessageTemplate() = default;

void MessageTemplate::Expand(std::string* out,
                             UINT templateId,
                             ObjectBase* field1,
                             ObjectBase* field2,
                             ObjectBase* field3,
                             ObjectBase* field4)
{
  if (!out || !m_module)
    return;
  HRSRC resourceInfo = FindResourceA(m_module, MAKEINTRESOURCEA(templateId & 0xffff), RT_RCDATA);
  HGLOBAL resource = resourceInfo ? LoadResource(m_module, resourceInfo) : nullptr;
  const char* text = resource ? static_cast<const char*>(LockResource(resource)) : nullptr;
  const size_t size = resource ? SizeofResource(m_module, resourceInfo) : 0;
  if (!text || size == 0)
  {
    if (resource)
      FreeResource(resource);
    return;
  }
  size_t offset = 0;
  while (offset < size)
  {
    if (text[offset] == m_endMarker)
      break;
    if (text[offset] != m_fieldMarker)
    {
      out->push_back(text[offset++]);
      continue;
    }
    if (size - offset < kFieldBytes)
      break;
    const auto code = static_cast<uint8_t>(text[offset + 1]);
    ObjectBase* field = nullptr;
    if ((code & kIndirectFieldFlag) == 0)
    {
      switch (code)
      {
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
          break;
      }
    }
    if (field)
      field->WriteName(out, ReadFieldArgument(text + offset + 2));
    offset += kFieldBytes;
  }
  if (resource)
    FreeResource(resource);
}

void SetMessageTemplateSource(UINT endMarkerId, UINT fieldMarkerId, HMODULE module)
{
  if (MessageTemplate* previous = g_messageTemplate)
  {
    delete previous;
    g_messageTemplate = nullptr;
  }
  if (module)
    g_messageTemplate = NewOrNull<MessageTemplate>(endMarkerId, fieldMarkerId, module);
}

void ExpandMessageTemplate(std::string* out,
                           UINT templateId,
                           ObjectBase* field1,
                           ObjectBase* field2,
                           ObjectBase* field3,
                           ObjectBase* field4)
{
  if (g_messageTemplate)
    g_messageTemplate->Expand(out, templateId, field1, field2, field3, field4);
}
