# Native build for Linux and macOS, x86-64 and arm64. OS differences: native_platform.cmake.
# Program state is typed source; only text assets are extracted from the local installation.
#
# The game objects are the same sources as the Windows builds, compiled for the host ABI (LP64, Itanium C++ ABI). The
# platform comes from src/port/minwin_impl (SDL3 for window/input/audio, FreeType for text) and src/port/native (kernel32 on
# POSIX, the MSVC CRT names the game uses).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(MW ${CMAKE_SOURCE_DIR}/src/port/minwin_impl)
include(${CMAKE_SOURCE_DIR}/cmake/native_platform.cmake)
set(REB_NATIVE_SANITIZE "" CACHE STRING "address;undefined: sanitizers for the game, port and runtime objects")
set(REB_STARTUP_ASSET_PACK "${CMAKE_BINARY_DIR}/startup-assets.bin" CACHE FILEPATH "Locally extracted startup asset pack")

set(REB_NATIVE_FLAGS
  -fms-extensions -fwrapv -fno-strict-aliasing -fsigned-char
  -ffp-contract=off -fno-fast-math -fno-omit-frame-pointer -fPIE
  -Wno-microsoft-anon-tag -Wno-ignored-attributes -Wno-deprecated-declarations
  -Wno-writable-strings -Wno-c++11-narrowing -Wno-microsoft-enum-value -Wno-microsoft-cast
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast)
if(REB_NATIVE_SANITIZE)
  string(REPLACE ";" "," REB_SAN_LIST "${REB_NATIVE_SANITIZE}")  # "address,undefined" or "address;undefined"
  list(APPEND REB_NATIVE_FLAGS -fsanitize=${REB_SAN_LIST} -fsanitize-recover=all -fno-sanitize=function)
  set(REB_NATIVE_LINK_FLAGS -fsanitize=${REB_SAN_LIST})

endif()

# Modern runtime files get focused warnings without imposing the legacy backlog on every game file.
set_source_files_properties(
  src/port/native/game_context.cpp src/port/native/audio_context.cpp src/port/native/main.cpp
  src/game/gfx/directdraw_host.cpp src/game/gfx/directdraw_errors.cpp
  src/game/iostream/ios.cpp src/game/iostream/streambuf.cpp src/game/iostream/stream_ctors.cpp
  src/game/core/string_map.cpp src/game/core/stopwatch.cpp src/game/data/game_math.cpp
  PROPERTIES COMPILE_OPTIONS
  "-Werror=return-type;-Werror=uninitialized;-Werror=delete-non-virtual-dtor;-Wdeprecated-declarations;-Wwritable-strings")

# Host runtime declarations for remaining platform compatibility calls.
set(REB_NATIVE_RUNTIME -include ${CMAKE_SOURCE_DIR}/src/port/native/crt/native_runtime.h)
set(REB_NATIVE_DEFS REB_STANDALONE=1 REB_MINWIN=1 REB_NATIVE=1 REB_NO_TACTICAL_3D=1)

add_library(reb_game OBJECT ${REB_GAME_SOURCES})
target_include_directories(reb_game PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_game PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
target_compile_definitions(reb_game PRIVATE ${REB_NATIVE_DEFS})
add_library(reb_tactical_stubs OBJECT src/game/tactical/tactical_3d_absent.cpp)
target_include_directories(reb_tactical_stubs PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_tactical_stubs PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
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
  ${MW}/user/region.cpp ${MW}/user/window.cpp ${MW}/user/focus.cpp ${MW}/user/message.cpp ${MW}/user/tooltip.cpp ${MW}/user/paint.cpp
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
target_compile_definitions(reb_minwin_impl PRIVATE REB_MINWIN=1)

file(GLOB REB_NATIVE_PORT_SOURCES CONFIGURE_DEPENDS src/port/native/*.cpp)
list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "platform_(linux|macos)\\.cpp$")
list(APPEND REB_NATIVE_PORT_SOURCES ${REB_PLATFORM_SOURCE})
list(APPEND REB_NATIVE_PORT_SOURCES src/port/sim_harness.cpp)
add_library(reb_native_port OBJECT ${REB_NATIVE_PORT_SOURCES})
target_include_directories(reb_native_port PRIVATE src/game src src/port/native/crt ${SDL3_SRC}/include)
target_compile_options(reb_native_port PRIVATE ${REB_NATIVE_FLAGS})
target_compile_definitions(reb_native_port PRIVATE REB_MINWIN=1 ${REB_NATIVE_DEFS})

add_custom_target(reb_native_objects DEPENDS reb_game reb_tactical_stubs reb_minwin_impl reb_native_port)
add_executable(rebellion
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_tactical_stubs> $<TARGET_OBJECTS:reb_minwin_impl>
  $<TARGET_OBJECTS:reb_native_port>)
set_target_properties(rebellion PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rebellion PRIVATE freetype SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
reb_native_platform_package(rebellion)

include(CTest)
if(BUILD_TESTING)
  set(REB_STREAM_TEST_SOURCES
    tests/stream_io_test_support.cpp
    src/game/iostream/filebuf.cpp
    src/game/iostream/ios.cpp
    src/game/iostream/istream.cpp
    src/game/iostream/ostream.cpp
    src/game/iostream/stream_ctors.cpp
    src/game/iostream/streambuf.cpp
    src/game/iostream/strstreambuf.cpp)
  add_executable(reb_stream_tests tests/stream_io_tests.cpp ${REB_STREAM_TEST_SOURCES})
  target_include_directories(reb_stream_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_stream_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_stream_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_stream_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_stream_io COMMAND reb_stream_tests)
  add_executable(reb_settings_tests
    tests/string_map_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/core/game_string.cpp src/game/core/string_map.cpp
    src/game/core/stream_io.cpp src/game/core/icf_stubs.cpp)
  target_include_directories(reb_settings_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_settings_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_settings_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_settings_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_settings COMMAND reb_settings_tests)
  add_executable(reb_string_tests
    tests/native/string_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/core/game_string.cpp src/game/core/stream_io.cpp)
  target_include_directories(reb_string_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_string_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_string_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_string_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_strings COMMAND reb_string_tests)
  add_executable(reb_math_tests tests/native/game_math_test.cpp src/game/data/game_math.cpp)
  target_include_directories(reb_math_tests PRIVATE src/game)
  target_compile_options(reb_math_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_math_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_integer_math COMMAND reb_math_tests)
  add_executable(reb_container_tests tests/native/container_tests.cpp src/game/core/containers.cpp)
  target_include_directories(reb_container_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_container_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_container_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_container_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_containers COMMAND reb_container_tests)
  add_executable(reb_random_tests
    tests/native/random_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/core/game_string.cpp src/game/core/stream_io.cpp
    src/game/core/utility_classes.cpp src/game/data/random.cpp)
  target_include_directories(reb_random_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_random_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_random_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_random_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_randomness COMMAND reb_random_tests)
endif()
