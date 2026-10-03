#pragma once

// MSVC stores the class's vtable pointer in every destructor, even one with an empty body, so a destroyed
// object holds the vtable of the last destructor that ran. The optimizer drops that dead store; this keeps
// it, so the object's memory ends up as in the original.
#define REB_KEEP_VPTR_STORE() __asm__ volatile("" : : "r"(this) : "memory")
