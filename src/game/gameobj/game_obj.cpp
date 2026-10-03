#include "gameobj/game_obj.h"

#include "core/vptr_store.h"

namespace {
constexpr uint32_t kViewSideMask = 0x30;
}

GameObj::GameObj()
    : GameObjNotifier(),
      m_name(nullptr),
      m_packed(0),
      m_eta(0),
      m_unk48(0),
      m_unk4c(0),
      m_stateFlags(0),
      m_data(nullptr) {
    SetFlag31(GetFlag31());
}

GameObj::~GameObj() {
    REB_KEEP_VPTR_STORE();
    if (m_name) {
        delete m_name;
        m_name = nullptr;
    }
    if ((m_flags & kViewSideMask) == 0) DestroyData();
}

bool GameObj::InstallData() {
    m_data = CreateData();
    return m_data != nullptr;
}

int GameObj::DestroyData() {
    if (m_data) {
        delete m_data;
        m_data = nullptr;
    }
    return 1;
}
