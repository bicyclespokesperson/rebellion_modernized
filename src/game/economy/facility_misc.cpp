#include "economy/facility_misc.h"

OkIdAvlTree::OkIdAvlTree()
{
  m_ok = 1;
}

OkIdAvlTree::~OkIdAvlTree()
{
  IdAvlTree::Clear();
}
