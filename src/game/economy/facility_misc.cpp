#include "economy/facility_misc.h"

#include "core/vptr_store.h"

OkIdAvlTree::OkIdAvlTree() { m_ok = 1; }

OkIdAvlTree::~OkIdAvlTree() {
    REB_KEEP_VPTR_STORE();
    IdAvlTree::Clear();
}
