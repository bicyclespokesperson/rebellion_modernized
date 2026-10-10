# The OS-specific part of the native build (docs/native.md). native.cmake is shared by Linux and macOS and calls
# reb_native_platform_sources() / reb_native_platform_package() from here; everything OS-specific lives in this file, in
# src/port/native/platform_{linux,macos}.cpp and in packaging/macos/.

if(APPLE)
  set(REB_PLATFORM_NAME macos)
  # CMAKE_OSX_DEPLOYMENT_TARGET (11.0, both architectures) is set before project() in the top-level CMakeLists.txt
else()
  set(REB_PLATFORM_NAME linux)
endif()

set(REB_PLATFORM_SOURCE ${CMAKE_CURRENT_LIST_DIR}/../src/port/native/platform_${REB_PLATFORM_NAME}.cpp)

# Libraries the executable needs beyond SDL3 and FreeType (SDL3-static brings its own frameworks / system libraries).
if(APPLE)
  set(REB_PLATFORM_LIBS pthread m)
else()
  set(REB_PLATFORM_LIBS pthread dl m)
endif()

# Optional Linux-only SDL3 configuration: REB_SDL_HEADLESS builds only the dummy/offscreen video and dummy audio drivers, so a
# cross build (aarch64) needs no X11/Wayland/ALSA/Pulse development packages.
function(reb_native_platform_sdl_options)
  if(REB_SDL_HEADLESS)
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
    foreach(opt SDL_X11 SDL_WAYLAND SDL_KMSDRM SDL_PULSEAUDIO SDL_ALSA SDL_JACK SDL_PIPEWIRE SDL_SNDIO SDL_OSS SDL_LIBUDEV SDL_DBUS SDL_IBUS)
      set(${opt} OFF CACHE BOOL "" FORCE)
    endforeach()
  endif()
endfunction()

# macOS: the executable becomes Rebellion.app (Info.plist from packaging/macos, ad-hoc signed so Gatekeeper accepts it on the
# machine that built it) with the Liberation Sans fallback fonts in Resources/fonts; Linux stays a plain binary.
function(reb_native_platform_package target)
  if(NOT APPLE)
    return()
  endif()
  set(REB_BUNDLE_VERSION "0.1" CACHE STRING "CFBundleShortVersionString of Rebellion.app")
  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME Rebellion
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in
    MACOSX_BUNDLE_BUNDLE_NAME "Star Wars Rebellion (port)"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.rebellion-port.rebellion"
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${REB_BUNDLE_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${REB_BUNDLE_VERSION})
  set(fonts ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../third_party/liberation-fonts-ttf-2.1.5)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/fonts
    COMMAND ${CMAKE_COMMAND} -E copy ${fonts}/LiberationSans-Regular.ttf ${fonts}/LiberationSans-Bold.ttf
            $<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/fonts
    COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:${target}>
    COMMENT "ad-hoc codesign Rebellion.app")
endfunction()
