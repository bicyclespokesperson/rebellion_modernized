#include "mw_test.h"

namespace mwtest
{

Case*& Head()
{
  static Case* head = nullptr;
  return head;
}

int& Failures()
{
  static int failures = 0;
  return failures;
}

void Fail(const char* file, int line, const char* expr)
{
  ++Failures();
  printf("  FAIL %s:%d: %s\n", file, line, expr);
}

} // namespace mwtest

int main(int argc, char** argv)
{
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0, failed = 0;
  // registration order is reversed (list head insertion); run in declaration order
  mwtest::Case* order[1024];
  int n = 0;
  for (mwtest::Case* c = mwtest::Head(); c && n < 1024; c = c->next) {
    order[n++] = c;
  }
  for (int i = n - 1; i >= 0; --i) {
    mwtest::Case* c = order[i];
    if (filter && !strstr(c->name, filter)) {
      continue;
    }
    int before = mwtest::Failures();
    c->fn();
    ++run;
    bool ok = mwtest::Failures() == before;
    if (!ok) {
      ++failed;
    }
    printf("%s %s\n", ok ? "ok  " : "FAIL", c->name);
  }
  printf("%d tests, %d failed\n", run, failed);
  return failed ? 1 : 0;
}
