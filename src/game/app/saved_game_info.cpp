#include "app/saved_game_info_decl.h"

#include "core/stream_io.h"
#include "core/vptr_store.h"

SavedGameInfo::SavedGameInfo() : m_name() {
    m_name.Clear();
    m_unknown10 = 0;
    m_unknown14 = 0;
    m_gameType = 0;
    m_slot = 0;
    m_side = 0;
    m_unknown24 = 0;
}

SavedGameInfo::SavedGameInfo(const SavedGameInfo& other) : m_name() {
    {
        String name = other.GetName();
        m_name.Assign(name);
    }
    m_unknown10 = other.m_unknown10;
    m_unknown14 = other.m_unknown14;
    m_gameType = other.m_gameType;
    m_slot = other.m_slot;
    m_side = other.m_side;
    m_unknown24 = other.m_unknown24;
}

SavedGameInfo::~SavedGameInfo() { REB_KEEP_VPTR_STORE(); }

SavedGameInfo& SavedGameInfo::operator=(const SavedGameInfo& other) {
    if (this != &other) {
        {
            String name = other.GetName();
            m_name.Assign(name);
        }
        m_unknown10 = other.m_unknown10;
        m_unknown14 = other.m_unknown14;
        m_gameType = other.m_gameType;
        m_slot = other.m_slot;
        m_side = other.m_side;
        m_unknown24 = other.m_unknown24;
    }
    return *this;
}

void SavedGameInfo::Read(istream* in) {
    m_name.ReadFrom(in);
    StreamRead32(in, &m_unknown10);
    StreamRead32(in, &m_unknown14);
    StreamRead32(in, &m_gameType);
    StreamRead32(in, &m_slot);
    StreamRead32(in, &m_side);
    StreamRead32(in, &m_unknown24);
}

void SavedGameInfo::Write(ostream* out) {
    m_name.WriteTo(out);
    StreamWrite32(out, &m_unknown10);
    StreamWrite32(out, &m_unknown14);
    StreamWrite32(out, &m_gameType);
    StreamWrite32(out, &m_slot);
    StreamWrite32(out, &m_side);
    StreamWrite32(out, &m_unknown24);
}
