#include "core/string_map.h"

#include "core/checked_new.h"

namespace {

bool HasMore(StringCursor* cursor) { return cursor->HasMore() != 0; }
int Current(StringCursor* cursor) { return cursor->Current(); }
void Advance(StringCursor* cursor) { ++*cursor; }

}  // namespace

StringMapNode::StringMapNode(const String& key, int value) : LinkNode(), m_key(key.m_text), m_value(value) {}

StringMapNode::~StringMapNode() = default;

StringMap::StringMap() : SortedLinkList() {}

StringMap::StringMap(const StringMap& other) : SortedLinkList() { *this = other; }

StringMap::~StringMap() = default;

int StringMap::IsLess(LinkNode*, LinkNode*) { return 1; }

StringMap& StringMap::operator=(const StringMap& other) {
    if (&other != this) {
        Clear();
        for (LinkNode* node = other.m_head; node; node = node->m_next) {
            const StringMapNode* entry = static_cast<const StringMapNode*>(node);
            Add(&entry->m_key, entry->m_value);
        }
    }
    return *this;
}

int StringMap::Add(const String* key, int value) {
    StringMapNode* node = NewOrNull<StringMapNode>(*key, value);
    // BUG(original): a failed allocation inserts a null node.
    Insert(node);
    return 1;
}

int StringMap::Set(const String* key, int value) {
    for (LinkNode* node = m_head; node; node = node->m_next) {
        StringMapNode* entry = static_cast<StringMapNode*>(node);
        if (entry->m_key.Compare(*key) == 0) {
            entry->m_value = value;
            return 1;
        }
    }
    return Add(key, value);
}

int StringMap::Find(const String* key, int* value) {
    for (LinkNode* node = m_head; node; node = node->m_next) {
        StringMapNode* entry = static_cast<StringMapNode*>(node);
        if (entry->m_key.Compare(*key) == 0) {
            *value = entry->m_value;
            return 1;
        }
    }
    return 0;
}

int StringMap::FindFloat(const String* key, float* value) {
    uint32_t found;
    if (!FindInText(key, reinterpret_cast<int*>(&found))) return 0;
    *value = static_cast<float>(found);
    return 1;
}

int StringMap::FindInText(const String* text, int* value) {
    StringCursor cursorObject(*text);
    StringCursor* cursor = &cursorObject;
    while (HasMore(cursor)) {
        SkipSeparators(cursor);
        if (HasMore(cursor)) {
            String word;
            ReadWord(cursor, &word);
            for (LinkNode* node = m_head; node; node = node->m_next) {
                StringMapNode* entry = static_cast<StringMapNode*>(node);
                if (entry->m_key.Compare(word) == 0) {
                    *value = entry->m_value;
                    return 1;
                }
            }
        }
        Advance(cursor);
    }
    return Find(text, value);
}

void StringMap::LoadValues(const String* text) {
    StringCursor cursorObject(*text);
    StringCursor* cursor = &cursorObject;
    while (HasMore(cursor)) {
        SkipSeparators(cursor);
        if (HasMore(cursor)) {
            String word;
            ReadWord(cursor, &word);
            int existing = 0;
            if (Find(&word, &existing) == 1) {
                int value = 0;
                ReadNumber(cursor, &value);
                Set(&word, value);
            }
        }
        SkipSeparators(cursor);
    }
}

int StringMap::SkipSeparators(StringCursor* cursor) {
    if (!HasMore(cursor)) return 0;
    do {
        if (Current(cursor) != ' ' && Current(cursor) != '-') return 1;
        Advance(cursor);
    } while (HasMore(cursor));
    return 0;
}

int StringMap::AtWordCharacter(StringCursor* cursor) {
    if (HasMore(cursor) && Current(cursor) != '-' && Current(cursor) != ' ') return 1;
    return 0;
}

void StringMap::ReadWord(StringCursor* cursor, String* word) {
    word->Clear();
    SkipSeparators(cursor);
    while (HasMore(cursor) && AtWordCharacter(cursor)) {
        char c = static_cast<char>(Current(cursor));
        word->AppendChar(c);
        Advance(cursor);
    }
}

int StringMap::ReadNumber(StringCursor* cursor, int* value) {
    int found = 0;
    int number = 0;
    SkipSeparators(cursor);
    while (HasMore(cursor)) {
        const int c = Current(cursor);
        if (c < '0' || c > '9') break;
        found = 1;
        number = c - '0' + number * 10;
        Advance(cursor);
    }
    if (found) *value = number;
    return found;
}
