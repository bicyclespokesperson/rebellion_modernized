# SDL 3.4.18 uses its cached pointer for focused button events. Cocoa can deliver
# a button NSEvent at a new position without a motion event.
set(SDL_COCOA_WINDOW "${SDL3_SRC}/src/video/cocoa/SDL_cocoawindow.m")
file(READ "${SDL_COCOA_WINDOW}" SDL_COCOA_SOURCE)
set(SDL_COCOA_PATCHED "${SDL_COCOA_SOURCE}")

set(SDL_OLD_CONDITION "if (focus && ([theEvent window] == ((__bridge SDL_CocoaWindowData *)focus->internal).nswindow)) {")
set(SDL_NEW_CONDITION "if (mouse->relative_mode && focus && ([theEvent window] == ((__bridge SDL_CocoaWindowData *)focus->internal).nswindow)) {")
string(FIND "${SDL_COCOA_PATCHED}" "${SDL_NEW_CONDITION}" SDL_CONDITION_PATCHED_AT)
if(SDL_CONDITION_PATCHED_AT LESS 0)
  string(FIND "${SDL_COCOA_PATCHED}" "${SDL_OLD_CONDITION}" SDL_ORIGINAL_AT)
  if(SDL_ORIGINAL_AT LESS 0)
    message(FATAL_ERROR "The pinned SDL Cocoa mouse-button condition changed; review ${SDL_COCOA_WINDOW}")
  endif()
  string(REPLACE "${SDL_OLD_CONDITION}" "${SDL_NEW_CONDITION}" SDL_COCOA_PATCHED "${SDL_COCOA_PATCHED}")
endif()

# CUA drag events carry the right NSEvent location but do not move the hardware
# pointer. SDL's macOS 26 global-cursor workaround must not replace drag locations.
set(SDL_OLD_MOTION_FALLBACK [=[    if (@available(macOS 26.0, *)) {
        // do for all windows for now, not just fullscreen spaces. And hopefully remove this code entirely soon.
        //if ([_data.listener isInFullscreenSpace]) {
            int posx = 0, posy = 0;
            SDL_GetWindowPosition(window, &posx, &posy);
            SDL_RelativeToGlobalForWindow(window, posx, posy, &posx, &posy);
            SDL_GetGlobalMouseState(&x, &y);
            x -= posx;
            y -= posy;
        //}
    }]=])
set(SDL_NEW_MOTION_FALLBACK [=[    if ([theEvent type] == NSEventTypeMouseMoved) {
        if (@available(macOS 26.0, *)) {
            // do for all windows for now, not just fullscreen spaces. And hopefully remove this code entirely soon.
            //if ([_data.listener isInFullscreenSpace]) {
                int posx = 0, posy = 0;
                SDL_GetWindowPosition(window, &posx, &posy);
                SDL_RelativeToGlobalForWindow(window, posx, posy, &posx, &posy);
                SDL_GetGlobalMouseState(&x, &y);
                x -= posx;
                y -= posy;
            //}
        }
    }]=])
string(FIND "${SDL_COCOA_PATCHED}" "${SDL_NEW_MOTION_FALLBACK}" SDL_MOTION_PATCHED_AT)
if(SDL_MOTION_PATCHED_AT LESS 0)
  string(FIND "${SDL_COCOA_PATCHED}" "${SDL_OLD_MOTION_FALLBACK}" SDL_MOTION_ORIGINAL_AT)
  if(SDL_MOTION_ORIGINAL_AT LESS 0)
    message(FATAL_ERROR "The pinned SDL Cocoa mouse-motion workaround changed; review ${SDL_COCOA_WINDOW}")
  endif()
  string(REPLACE "${SDL_OLD_MOTION_FALLBACK}" "${SDL_NEW_MOTION_FALLBACK}" SDL_COCOA_PATCHED "${SDL_COCOA_PATCHED}")
endif()

if(NOT SDL_COCOA_PATCHED STREQUAL SDL_COCOA_SOURCE)
  file(WRITE "${SDL_COCOA_WINDOW}" "${SDL_COCOA_PATCHED}")
endif()

# Cocoa_HandleMouseEvent runs before the NSWindow mouseUp: callback. Reconciliation
# otherwise emits an up event at SDL's cached cursor position, causing mouseUp:
# to be discarded as a duplicate before it can use the NSEvent coordinates.
set(SDL_COCOA_MOUSE "${SDL3_SRC}/src/video/cocoa/SDL_cocoamouse.m")
file(READ "${SDL_COCOA_MOUSE}" SDL_COCOA_MOUSE_SOURCE)
set(SDL_OLD_RECONCILE [=[            Cocoa_ReconcileButtonState(event);]=])
set(SDL_PREVIOUS_RECONCILE [=[            const bool isButtonUp = event_type == NSEventTypeLeftMouseUp ||
                                    event_type == NSEventTypeRightMouseUp ||
                                    event_type == NSEventTypeOtherMouseUp;
            const NSRect contentRect = [[[event window] contentView] frame];
            if (!isButtonUp || !NSMouseInRect([event locationInWindow], contentRect, NO)) {
                Cocoa_ReconcileButtonState(event);
            }]=])
set(SDL_NEW_RECONCILE [=[            const bool isButtonUp = event_type == NSEventTypeLeftMouseUp ||
                                    event_type == NSEventTypeRightMouseUp ||
                                    event_type == NSEventTypeOtherMouseUp;
            const bool isButtonDrag = event_type == NSEventTypeLeftMouseDragged ||
                                      event_type == NSEventTypeRightMouseDragged ||
                                      event_type == NSEventTypeOtherMouseDragged;
            const NSRect contentRect = [[[event window] contentView] frame];
            if (!isButtonDrag && (!isButtonUp || !NSMouseInRect([event locationInWindow], contentRect, NO))) {
                Cocoa_ReconcileButtonState(event);
            }]=])
string(FIND "${SDL_COCOA_MOUSE_SOURCE}" "${SDL_NEW_RECONCILE}" SDL_RECONCILE_PATCHED_AT)
if(SDL_RECONCILE_PATCHED_AT LESS 0)
  string(FIND "${SDL_COCOA_MOUSE_SOURCE}" "${SDL_PREVIOUS_RECONCILE}" SDL_RECONCILE_PREVIOUS_AT)
  if(SDL_RECONCILE_PREVIOUS_AT GREATER_EQUAL 0)
    string(REPLACE "${SDL_PREVIOUS_RECONCILE}" "${SDL_NEW_RECONCILE}" SDL_COCOA_MOUSE_SOURCE "${SDL_COCOA_MOUSE_SOURCE}")
  else()
    string(FIND "${SDL_COCOA_MOUSE_SOURCE}" "${SDL_OLD_RECONCILE}" SDL_RECONCILE_ORIGINAL_AT)
    if(SDL_RECONCILE_ORIGINAL_AT LESS 0)
      message(FATAL_ERROR "The pinned SDL Cocoa mouse-button reconciliation changed; review ${SDL_COCOA_MOUSE}")
    endif()
    string(REPLACE "${SDL_OLD_RECONCILE}" "${SDL_NEW_RECONCILE}" SDL_COCOA_MOUSE_SOURCE "${SDL_COCOA_MOUSE_SOURCE}")
  endif()
  file(WRITE "${SDL_COCOA_MOUSE}" "${SDL_COCOA_MOUSE_SOURCE}")
endif()
