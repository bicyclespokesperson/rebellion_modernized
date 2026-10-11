#include "tactical/tactical_moving_object.h"


MovingObject::MovingObject() {
    m_scale = 1.0f;
    m_unk50 = 0;
    m_forward.Set(0, 0, 1.0f);
}

MovingObject::~MovingObject() {  }
