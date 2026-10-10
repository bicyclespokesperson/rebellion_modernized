#define MINWIN_IMPLEMENTATION 1
#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"
#include "../tests/mw_test.h"

namespace {
minwin::Window appWindow{};
minwin::Window childWindow{};
int activationCount = 0;
int deactivationCount = 0;
}

namespace minwin {
Window* FirstTopLevel() { return &appWindow; }
bool IsLive(const Window* window) { return window == &appWindow || window == &childWindow; }
Window* FromHandle(HWND handle) {
    if (handle == ToHandle(&appWindow)) return &appWindow;
    if (handle == ToHandle(&childWindow)) return &childWindow;
    return nullptr;
}
Window* TopLevelOf(Window* window) { return window->parent ? window->parent : window; }
bool IsDescendant(const Window* ancestor, const Window* window) { return window->parent == ancestor; }
bool IsWindowVisibleTree(const Window* window) { return window->style & WS_VISIBLE; }
bool IsEnabledTree(const Window* window) { return !window->iconic; }
LRESULT Send(Window* window, UINT message, WPARAM value, LPARAM) {
    if (window == &appWindow && message == WM_ACTIVATEAPP) {
        if (value) {
            ++activationCount;
            appWindow.iconic = false;
            SetFocus(ToHandle(&childWindow));
        } else {
            ++deactivationCount;
            SetFocus(ToHandle(&appWindow));
            appWindow.iconic = true;
        }
    }
    return 0;
}
}

TEST(task_switch_does_not_reactivate_during_deactivation) {
    using namespace minwin;
    appWindow.handle = 1;
    appWindow.style = WS_VISIBLE;
    childWindow.handle = 2;
    childWindow.parent = &appWindow;
    childWindow.style = WS_VISIBLE;
    ActivateTopLevel(&appWindow, false);
    CHECK_EQ(activationCount, 1);
    CHECK(GetFocus() == ToHandle(&childWindow));

    InputFocusChanged(false);
    CHECK_EQ(deactivationCount, 1);
    CHECK_EQ(activationCount, 1);
    CHECK(!AppActive());
    CHECK(appWindow.iconic);

    InputFocusChanged(true);
    CHECK_EQ(activationCount, 2);
    CHECK(AppActive());
    CHECK(!appWindow.iconic);
    CHECK(GetFocus() == ToHandle(&childWindow));
}
