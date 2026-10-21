// A minimal unit-test harness shared by the minwin_impl libraries: TEST(name) { CHECK(...); }, run by mw_test_main.cpp.
// Works on Linux and on i686-windows (no dependencies beyond the C/C++ runtime).
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

namespace mwtest
{

struct Case {
  const char* name;
  void (*fn)();
  Case* next;
};
Case*& Head();
int& Failures();
void Fail(const char* file, int line, const char* expr);

struct Register {
  Register(Case* c)
  {
    c->next = Head();
    Head() = c;
  }
};

} // namespace mwtest

#define TEST(NAME)                                                                                                     \
  static void NAME();                                                                                                  \
  static mwtest::Case case_##NAME = {#NAME, NAME, nullptr};                                                            \
  static mwtest::Register reg_##NAME(&case_##NAME);                                                                    \
  static void NAME()

#define CHECK(EXPR)                                                                                                    \
  do {                                                                                                                 \
    if (!(EXPR))                                                                                                       \
      mwtest::Fail(__FILE__, __LINE__, #EXPR);                                                                         \
  } while (0)

#define CHECK_EQ(A, B)                                                                                                 \
  do {                                                                                                                 \
    long long a_ = static_cast<long long>(A), b_ = static_cast<long long>(B);                                          \
    if (a_ != b_) {                                                                                                    \
      char msg_[256];                                                                                                  \
      snprintf(msg_, sizeof msg_, "%s == %s  (%lld vs %lld)", #A, #B, a_, b_);                                         \
      mwtest::Fail(__FILE__, __LINE__, msg_);                                                                          \
    }                                                                                                                  \
  } while (0)

#define CHECK_STR(A, B)                                                                                                \
  do {                                                                                                                 \
    std::string a_ = (A);                                                                                              \
    std::string b_ = (B);                                                                                              \
    if (a_ != b_) {                                                                                                    \
      char msg_[512];                                                                                                  \
      snprintf(msg_, sizeof msg_, "%s == %s  (\"%s\" vs \"%s\")", #A, #B, a_.c_str(), b_.c_str());                     \
      mwtest::Fail(__FILE__, __LINE__, msg_);                                                                          \
    }                                                                                                                  \
  } while (0)
