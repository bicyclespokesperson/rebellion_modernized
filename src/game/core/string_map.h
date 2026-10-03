#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/game_string.h"

class StringCursor;

// Entry of a StringMap: a name and an integer value.
class StringMapNode : public LinkNode {
public:
    StringMapNode(const String& key, int value);
    virtual ~StringMapNode();

    String m_key;  // +0x0c
    int m_value;   // +0x18
};

// Name -> int dictionary, a linked list searched front to back (newest entry first). It also reads
// "name value name value ..." text where words are separated by spaces and dashes.
class StringMap : public SortedLinkList {
public:
    StringMap();
    StringMap(const StringMap& other);
    virtual ~StringMap();
    virtual int IsLess(LinkNode* a, LinkNode* b) override;

    StringMap& operator=(const StringMap& other);

    // Adds an entry even if the name exists; Set() overwrites an existing entry's value instead.
    int Add(const String* key, int value);
    int Set(const String* key, int value);
    int Find(const String* key, int* value);
    int FindFloat(const String* key, float* value);
    // Looks for each word of `text` as a name, then for the whole text.
    int FindInText(const String* text, int* value);
    // Parses "name number" pairs from `text`, updating the values of names that already exist.
    void LoadValues(const String* text);

    // Text scanning helpers over a cursor into the text being parsed.
    int SkipSeparators(StringCursor* cursor);
    int AtWordCharacter(StringCursor* cursor);
    void ReadWord(StringCursor* cursor, String* word);
    int ReadNumber(StringCursor* cursor, int* value);
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(StringMapNode, 0x1c);
REB_ASSERT_OFFSET(StringMapNode, m_key, 0x0c);
REB_ASSERT_OFFSET(StringMapNode, m_value, 0x18);
REB_ASSERT_SIZE(StringMap, 0x0c);
#pragma clang diagnostic pop
