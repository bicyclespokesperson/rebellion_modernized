# Native build for Linux and macOS, x86-64 and arm64 (docs/native.md, docs/phase3_plan.md P3/P4); OS differences: native_platform.cmake. Included from CMakeLists.txt when REB_NATIVE is on.
# Driven by tools/build_native.sh, which generates gen/*.cpp (data image, static-init table, resources) on the host
# before the second configure (REB_NATIVE_GEN=ON).
#
# The game objects are the same sources as the Windows builds, compiled for the host ABI (LP64, Itanium C++ ABI). The
# platform comes from src/port/minwin_impl (SDL3 for window/input/audio, FreeType for text) and src/port/native (kernel32 on
# POSIX, the MSVC CRT names the game uses).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(REB_GEN ${CMAKE_BINARY_DIR}/gen)
set(MW ${CMAKE_SOURCE_DIR}/src/port/minwin_impl)
include(${CMAKE_SOURCE_DIR}/cmake/native_platform.cmake)
set(REB_NATIVE_SANITIZE "" CACHE STRING "address;undefined: sanitizers for the game, port and runtime objects")
option(REB_NATIVE_GEN "Link the generated gen/*.cpp (data image, static-init table); stage 2 of tools/build_native.sh" OFF)

set(REB_NATIVE_FLAGS
  -fms-extensions -fno-exceptions -fno-rtti -fcheck-new -fwrapv -fno-strict-aliasing -fsigned-char
  -ffp-contract=off -fno-fast-math -fno-omit-frame-pointer -fPIE
  -Wno-microsoft-anon-tag -Wno-ignored-attributes -Wno-deprecated-declarations -Wno-invalid-offsetof
  -Wno-writable-strings -Wno-c++11-narrowing -Wno-microsoft-enum-value -Wno-microsoft-cast
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast)
if(REB_NATIVE_SANITIZE)
  string(REPLACE ";" "," REB_SAN_LIST "${REB_NATIVE_SANITIZE}")  # "address,undefined" or "address;undefined"
  list(APPEND REB_NATIVE_FLAGS -fsanitize=${REB_SAN_LIST} -fsanitize-recover=all -fno-sanitize=function)
  set(REB_NATIVE_LINK_FLAGS -fsanitize=${REB_SAN_LIST})
endif()

# The game's own versions of C library functions whose behaviour is game behaviour (src/game/crt) must not collide with libc's.
set(REB_NATIVE_RENAMES -include ${CMAKE_SOURCE_DIR}/src/port/native/crt/native_prelude.h)
set(REB_NATIVE_DEFS REB_STANDALONE=1 REB_MINWIN=1 REB_NATIVE=1 REB_NO_TACTICAL_3D=1)

add_library(reb_game OBJECT ${REB_GAME_SOURCES})
target_include_directories(reb_game PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_game PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RENAMES})
target_compile_definitions(reb_game PRIVATE ${REB_NATIVE_DEFS})
add_library(reb_tactical_stubs OBJECT src/game/tactical/tactical_3d_absent.cpp)
target_include_directories(reb_tactical_stubs PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_tactical_stubs PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RENAMES})
target_compile_definitions(reb_tactical_stubs PRIVATE ${REB_NATIVE_DEFS})

# ---- third-party: SDL3 and FreeType, static, from the pinned sources (tools/fetch_deps.sh) -------------------------------
set(SDL3_SRC ${CMAKE_SOURCE_DIR}/third_party/SDL3-3.4.18-src)
set(FREETYPE_ROOT ${CMAKE_SOURCE_DIR}/third_party/freetype-2.13.2)
if(NOT EXISTS ${SDL3_SRC}/CMakeLists.txt OR NOT EXISTS ${FREETYPE_ROOT}/CMakeLists.txt)
  message(FATAL_ERROR "SDL3 or FreeType sources are missing: run tools/fetch_deps.sh")
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_DBUS OFF CACHE BOOL "" FORCE)
set(SDL_IBUS OFF CACHE BOOL "" FORCE)
set(SDL_LIBURING OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
reb_native_platform_sdl_options()
add_subdirectory(${SDL3_SRC} ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL)
# SDL's X11 clipboard code has a global function named GetWindow, which is also a Win32 name minwin defines (harmless elsewhere).
target_compile_definitions(SDL3-static PRIVATE GetWindow=SDL_X11_GetWindow)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(${FREETYPE_ROOT} ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)

# ---- the platform: minwin_impl (user32/gdi32/... on SDL3) and src/port/native (kernel32 and the CRT on POSIX) ---------------
set(REB_MINWIN_IMPL_SOURCES
  ${MW}/user/region.cpp ${MW}/user/window.cpp ${MW}/user/focus.cpp ${MW}/user/message.cpp ${MW}/user/paint.cpp
  ${MW}/user/input.cpp ${MW}/user/platform.cpp ${MW}/user/present.cpp ${MW}/user/misc.cpp ${MW}/user/mmtimer.cpp src/port/msg_trace.cpp
  ${MW}/config/config.cpp ${MW}/config/advapi32_api.cpp
  ${MW}/res/pe_module.cpp ${MW}/res/mapped_file.cpp ${MW}/res/formats.cpp ${MW}/res/path.cpp ${MW}/res/modules.cpp ${MW}/res/res_api.cpp
  ${MW}/gdi/objects.cpp ${MW}/gdi/dc.cpp ${MW}/gdi/raster.cpp ${MW}/gdi/shapes.cpp ${MW}/gdi/blit_api.cpp
  ${MW}/text/font_files.cpp ${MW}/text/ft_font.cpp ${MW}/text/text_api.cpp
  ${MW}/dsound/mixer.cpp ${MW}/dsound/dsound_api.cpp
  ${MW}/smack/smk_movie.cpp ${MW}/smack/smack_api.cpp ${MW}/smack/smack_clock.cpp
  ${MW}/ddraw/ddraw_api.cpp ${MW}/stubs/unavailable.cpp ${MW}/audio/sdl_audio.cpp ${MW}/glue/wiring.cpp)
add_library(reb_minwin_impl OBJECT ${REB_MINWIN_IMPL_SOURCES})
target_include_directories(reb_minwin_impl PRIVATE src/game src ${MW} ${SDL3_SRC}/include ${FREETYPE_ROOT}/include)
target_compile_options(reb_minwin_impl PRIVATE ${REB_NATIVE_FLAGS} -include ${CMAKE_SOURCE_DIR}/src/port/native/crt/msvc_crt.h)
target_compile_definitions(reb_minwin_impl PRIVATE REB_MINWIN=1 MW_IIDS_IN_GAME_DATA=1)

file(GLOB REB_NATIVE_PORT_SOURCES CONFIGURE_DEPENDS src/port/native/*.cpp)
list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "platform_(linux|macos)\\.cpp$")
list(APPEND REB_NATIVE_PORT_SOURCES ${REB_PLATFORM_SOURCE})
list(APPEND REB_NATIVE_PORT_SOURCES src/port/sim_harness.cpp)
add_library(reb_native_port OBJECT ${REB_NATIVE_PORT_SOURCES})
target_include_directories(reb_native_port PRIVATE src/game src src/port/native/crt ${SDL3_SRC}/include)
target_compile_options(reb_native_port PRIVATE ${REB_NATIVE_FLAGS})
target_compile_definitions(reb_native_port PRIVATE REB_MINWIN=1 ${REB_NATIVE_DEFS})

add_custom_target(reb_native_objects DEPENDS reb_game reb_tactical_stubs reb_minwin_impl reb_native_port)
set(REB_NATIVE_GENERATED)
if(REB_NATIVE_GEN)
  file(GLOB REB_NATIVE_GENERATED CONFIGURE_DEPENDS ${REB_GEN}/*.cpp)
  add_library(reb_native_gen OBJECT ${REB_NATIVE_GENERATED})
  target_include_directories(reb_native_gen PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_native_gen PRIVATE ${REB_NATIVE_FLAGS})
  target_compile_definitions(reb_native_gen PRIVATE REB_MINWIN=1 ${REB_NATIVE_DEFS})
  set(REB_NATIVE_GEN_OBJECTS $<TARGET_OBJECTS:reb_native_gen>)
endif()

add_executable(rebellion
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_tactical_stubs> $<TARGET_OBJECTS:reb_minwin_impl>
  $<TARGET_OBJECTS:reb_native_port> ${REB_NATIVE_GEN_OBJECTS})
set_target_properties(rebellion PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rebellion PRIVATE freetype SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
reb_native_platform_package(rebellion)
