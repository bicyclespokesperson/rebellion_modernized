#pragma once
#include <cstdint>
#include <string>

#include "platform/win32.h"

class ObjectBase;

// Expands the RT_RCDATA text templates of a string DLL (TEXTSTRA's player messages). A template is plain text in
// which a field marker byte introduces a 6-byte field: marker, field code, int32 argument. Field code n (1..4)
// appends the name of the n-th object passed to Expand (ObjectBase::WriteName with the argument as the kind).
// High-bit indirect field codes are unsupported and skipped. The text ends at the end marker byte. The two marker
// bytes are themselves one-byte RT_RCDATA resources.
class MessageTemplate
{
  public:
  MessageTemplate(UINT endMarkerId, UINT fieldMarkerId, HMODULE module); // 0x60b7b0
  ~MessageTemplate();                                                    // 0x619730 (does nothing)

  // Appends template `templateId`, with its fields filled in, to `out`.
  void Expand(std::string* out, UINT templateId, ObjectBase* field1, ObjectBase* field2, ObjectBase* field3,
              ObjectBase* field4); // 0x60b840

  ObjectBase* m_noField; // +0x00 always null (see Expand)
  char m_endMarker;      // +0x04 0 when the resource is missing
  char m_fieldMarker;    // +0x05
  uint8_t m_pad06[2];    // +0x06
  HMODULE m_module;      // +0x08
};

// Replaces g_messageTemplate with one reading `module` (none when `module` is null). The galaxy view sets it up
// with TEXTSTRA.DLL.
void SetMessageTemplateSource(UINT endMarkerId, UINT fieldMarkerId, HMODULE module); // 0x60b940
// g_messageTemplate->Expand, if there is one.
void ExpandMessageTemplate(std::string* out, UINT templateId, ObjectBase* field1, ObjectBase* field2,
                           ObjectBase* field3,
                           ObjectBase* field4); // 0x60b9d0

extern MessageTemplate* g_messageTemplate; // 0x6be5c4
