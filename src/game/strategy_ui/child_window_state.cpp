#include "core/stream_io.h"
#include "core/vptr_store.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_game_window.h"

ChildWindowState::ChildWindowState()
    : m_type(0), m_x(0), m_y(0), m_width(0), m_height(0), m_shown(0), m_valid(0) {}

ChildWindowState::ChildWindowState(CoolWindow* window) {
    if (!window) {
        m_type = 0;
        m_x = 0;
        m_y = 0;
        m_width = 0;
        m_height = 0;
        m_shown = 0;
        m_valid = 0;
        return;
    }
    m_type = window->m_childId & 0x3f;
    m_x = window->m_x;
    m_y = window->m_y;
    m_width = window->m_width;
    m_height = window->m_height;
    m_shown = static_cast<CoolGameWindow*>(window)->m_shown;
    m_valid = 1;
}

ChildWindowState::~ChildWindowState() { REB_KEEP_VPTR_STORE(); }

void ChildWindowState::Write(ostream* out) {
    WriteValue32(out, &m_type);
    WriteValue32(out, &m_x);
    WriteValue32(out, &m_y);
    WriteValue32(out, &m_width);
    WriteValue32(out, &m_height);
    WriteValue32(out, &m_shown);
}

void ChildWindowState::Read(istream* in) {
    ReadValue32(in, &m_type);
    ReadValue32(in, &m_x);
    ReadValue32(in, &m_y);
    ReadValue32(in, &m_width);
    ReadValue32(in, &m_height);
    ReadValue32(in, &m_shown);
}
