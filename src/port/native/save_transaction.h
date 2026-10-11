#pragma once

#include <functional>

class ostream;

namespace reb::native {

bool WriteSaveAtomically(const char* destination, const std::function<bool(ostream*)>& write);

}
