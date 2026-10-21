# Native build for Windows, Linux and macOS. OS differences: native_platform.cmake.
# Program state is typed source; only text assets are extracted from the local installation.
#
# The game objects use the host ABI. The platform comes from src/port/minwin_impl (SDL3 for window/input/audio, FreeType for
# text) and src/port/native (kernel32 compatibility on POSIX and the host OS adapter).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(MW ${CMAKE_SOURCE_DIR}/src/port/minwin_impl)
include(${CMAKE_SOURCE_DIR}/cmake/native_platform.cmake)
set(REB_NATIVE_SANITIZE "" CACHE STRING "address;undefined: sanitizers for the game, port and runtime objects")
set(REB_STARTUP_ASSET_PACK "${CMAKE_BINARY_DIR}/startup-assets.bin" CACHE FILEPATH "Locally extracted startup asset pack")

set(REB_NATIVE_FLAGS
  -fms-extensions -fwrapv -fno-strict-aliasing -fsigned-char
  -ffp-contract=off -fno-fast-math -fno-omit-frame-pointer
  -Wno-microsoft-anon-tag -Wno-ignored-attributes -Wno-deprecated-declarations
  -Wno-writable-strings -Wno-c++11-narrowing -Wno-microsoft-enum-value -Wno-microsoft-cast)
if(NOT WIN32)
  list(APPEND REB_NATIVE_FLAGS -fPIE -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast)
endif()
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
# clang-cl takes /FI for forced includes and accepts GNU-style -f flags only behind /clang:.
if(MSVC)
  set(REB_NATIVE_RUNTIME /FI${CMAKE_SOURCE_DIR}/src/port/native/crt/native_runtime.h)
  set(REB_FORCE_MSVC_CRT /FI${CMAKE_SOURCE_DIR}/src/port/native/crt/msvc_crt.h)
  set(REB_CLANG_CL_FLAGS)
  foreach(flag IN LISTS REB_NATIVE_FLAGS)
    if(flag MATCHES "^-f(wrapv|signed-char|fp-contract|no-fast-math|no-omit-frame-pointer|sanitize)")
      list(APPEND REB_CLANG_CL_FLAGS /clang:${flag})
    else()
      list(APPEND REB_CLANG_CL_FLAGS ${flag})
    endif()
  endforeach()
  set(REB_NATIVE_FLAGS ${REB_CLANG_CL_FLAGS})
else()
  set(REB_NATIVE_RUNTIME -include ${CMAKE_SOURCE_DIR}/src/port/native/crt/native_runtime.h)
  set(REB_FORCE_MSVC_CRT -include ${CMAKE_SOURCE_DIR}/src/port/native/crt/msvc_crt.h)
endif()
execute_process(COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE REB_LAN_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND git symbolic-ref -q HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE REB_LAN_HEAD_REF OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
foreach(REB_GIT_TRACKED_PATH IN ITEMS HEAD "${REB_LAN_HEAD_REF}" packed-refs)
  if(NOT REB_GIT_TRACKED_PATH)
    continue()
  endif()
  execute_process(COMMAND git rev-parse --git-path "${REB_GIT_TRACKED_PATH}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE REB_GIT_DEPENDENCY
    RESULT_VARIABLE REB_GIT_PATH_RESULT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(REB_GIT_PATH_RESULT EQUAL 0)
    get_filename_component(REB_GIT_DEPENDENCY "${REB_GIT_DEPENDENCY}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    while(NOT EXISTS "${REB_GIT_DEPENDENCY}")
      get_filename_component(REB_GIT_DEPENDENCY "${REB_GIT_DEPENDENCY}" DIRECTORY)
    endwhile()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${REB_GIT_DEPENDENCY}")
  endif()
endforeach()
set(REB_NATIVE_DEFS REB_STANDALONE=1 REB_MINWIN=1 REB_NATIVE=1)

add_library(reb_game OBJECT ${REB_GAME_SOURCES})
target_include_directories(reb_game PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_game PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
target_compile_definitions(reb_game PRIVATE ${REB_NATIVE_DEFS})

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
if(APPLE)
  include(${CMAKE_SOURCE_DIR}/cmake/patch_sdl3_cocoa_mouse.cmake)
endif()
add_subdirectory(${SDL3_SRC} ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL)
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
target_compile_options(reb_minwin_impl PRIVATE ${REB_NATIVE_FLAGS} ${REB_FORCE_MSVC_CRT})
target_compile_definitions(reb_minwin_impl PRIVATE REB_MINWIN=1 REB_NATIVE=1)

file(GLOB REB_NATIVE_PORT_SOURCES CONFIGURE_DEPENDS src/port/native/*.cpp)
list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "platform_(linux|macos|windows)\\.cpp$")
if(WIN32)
  list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "(kernel32_posix|crt_posix|win_compat)\\.cpp$")
else()
  list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "crt_windows\\.cpp$")
endif()
list(APPEND REB_NATIVE_PORT_SOURCES ${REB_PLATFORM_SOURCE})
list(APPEND REB_NATIVE_PORT_SOURCES src/port/sim_harness.cpp)
# main() stays out of the shared port objects so that tools (reb_battle_runner) can link them with their own.
list(FILTER REB_NATIVE_PORT_SOURCES EXCLUDE REGEX "src/port/native/main\\.cpp$")
add_library(reb_native_port OBJECT ${REB_NATIVE_PORT_SOURCES})
target_include_directories(reb_native_port PRIVATE src/game src src/port/native/crt ${SDL3_SRC}/include)
target_compile_options(reb_native_port PRIVATE ${REB_NATIVE_FLAGS})
target_compile_definitions(reb_native_port PRIVATE REB_MINWIN=1 ${REB_NATIVE_DEFS})
target_compile_definitions(reb_native_port PRIVATE REB_LAN_BUILD_ID="${REB_LAN_BUILD_ID}")
add_library(reb_native_main OBJECT src/port/native/main.cpp)
target_include_directories(reb_native_main PRIVATE src/game src src/port/native/crt ${SDL3_SRC}/include)
target_compile_options(reb_native_main PRIVATE ${REB_NATIVE_FLAGS})
target_compile_definitions(reb_native_main PRIVATE REB_MINWIN=1 ${REB_NATIVE_DEFS})

# The live battle screen's SDL side (docs/design/tactical_combat.md §11.2): its own directory, as the port glob above
# is not recursive.
file(GLOB REB_SPACE_BATTLE_PORT_SOURCES CONFIGURE_DEPENDS src/port/native/space_battle/*.cpp)
add_library(reb_space_battle_port OBJECT ${REB_SPACE_BATTLE_PORT_SOURCES})
target_include_directories(reb_space_battle_port PRIVATE src/game src ${SDL3_SRC}/include ${FREETYPE_ROOT}/include)
target_compile_options(reb_space_battle_port PRIVATE ${REB_NATIVE_FLAGS})
target_compile_definitions(reb_space_battle_port PRIVATE REB_MINWIN=1 REB_NATIVE=1)

add_custom_target(reb_native_objects DEPENDS reb_game reb_minwin_impl reb_native_port
  reb_space_battle_port reb_native_main)
add_executable(rebellion
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_minwin_impl>
  $<TARGET_OBJECTS:reb_native_port> $<TARGET_OBJECTS:reb_space_battle_port> $<TARGET_OBJECTS:reb_native_main>)
set_target_properties(rebellion PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rebellion PRIVATE freetype SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
reb_native_platform_package(rebellion)

# Headless battle runner for calibration (docs/ai_matches.md): the game's objects with a command-line main; not
# part of the shipped app.
add_executable(reb_battle_runner src/tools/battle_runner.cpp
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_minwin_impl>
  $<TARGET_OBJECTS:reb_native_port> $<TARGET_OBJECTS:reb_space_battle_port>)
target_include_directories(reb_battle_runner PRIVATE src/game src src/port/native/crt)
target_compile_options(reb_battle_runner PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
target_compile_definitions(reb_battle_runner PRIVATE ${REB_NATIVE_DEFS})
set_target_properties(reb_battle_runner PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(reb_battle_runner PRIVATE freetype SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})

if(CMAKE_GENERATOR MATCHES "Ninja|Makefiles" AND NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  add_custom_target(reb_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Updating repo-root compile_commands.json"
    VERBATIM)
  add_dependencies(rebellion reb_compile_commands)
  add_dependencies(reb_native_objects reb_compile_commands)
endif()

include(CTest)
include(${CMAKE_SOURCE_DIR}/cmake/compat_symbols.cmake)
if(BUILD_TESTING)
  add_executable(reb_lan_transport_tests tests/native/lan_transport_tests.cpp src/port/native/lan_transport.cpp)
  target_include_directories(reb_lan_transport_tests PRIVATE src)
  target_link_libraries(reb_lan_transport_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  target_compile_options(reb_lan_transport_tests PRIVATE ${REB_NATIVE_FLAGS})
  add_test(NAME native_lan_transport COMMAND reb_lan_transport_tests)
  add_executable(reb_lan_wire_tests tests/native/lan_wire_tests.cpp)
  target_include_directories(reb_lan_wire_tests PRIVATE src)
  target_compile_options(reb_lan_wire_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_lan_wire_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_lan_wire COMMAND reb_lan_wire_tests)
  add_executable(reb_campaign_checkpoint_tests tests/native/campaign_checkpoint_tests.cpp
    src/port/native/campaign_checkpoints.cpp)
  target_include_directories(reb_campaign_checkpoint_tests PRIVATE src)
  target_compile_options(reb_campaign_checkpoint_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_campaign_checkpoint_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_campaign_checkpoints COMMAND reb_campaign_checkpoint_tests)
  set(REB_STREAM_TEST_SOURCES
    tests/stream_io_test_support.cpp
    src/game/iostream/filebuf.cpp
    src/game/iostream/ios.cpp
    src/game/iostream/istream.cpp
    src/game/iostream/ostream.cpp
    src/game/iostream/stream_ctors.cpp
    src/game/iostream/streambuf.cpp
    src/game/iostream/strstreambuf.cpp)
  add_executable(reb_memory_stream_tests tests/native/memory_stream_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/core/memory_stream.cpp src/game/core/game_string.cpp src/game/core/stream_io.cpp)
  target_include_directories(reb_memory_stream_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_memory_stream_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_memory_stream_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_memory_stream_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_memory_streams COMMAND reb_memory_stream_tests)
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
  add_executable(reb_sprite_pack_tests tests/native/sprite_pack_tests.cpp src/port/native/space_battle/sprite_pack.cpp)
  target_include_directories(reb_sprite_pack_tests PRIVATE src)
  target_compile_options(reb_sprite_pack_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_sprite_pack_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_space_battle_sprite_pack COMMAND reb_sprite_pack_tests)
  add_executable(reb_thread_sync_tests tests/native/thread_sync_tests.cpp)
  target_include_directories(reb_thread_sync_tests PRIVATE src)
  target_compile_options(reb_thread_sync_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_thread_sync_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_thread_sync COMMAND reb_thread_sync_tests)
  add_executable(reb_math_tests tests/native/game_math_test.cpp src/game/data/game_math.cpp)
  target_include_directories(reb_math_tests PRIVATE src/game)
  target_compile_options(reb_math_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_math_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_integer_math COMMAND reb_math_tests)
  add_executable(reb_battle_sound_tests tests/native/battle_sounds_tests.cpp src/game/battle_ui/battle_sounds.cpp)
  target_include_directories(reb_battle_sound_tests PRIVATE src/game)
  target_compile_options(reb_battle_sound_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_battle_sound_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_battle_sounds COMMAND reb_battle_sound_tests)
  add_executable(reb_battle_audio_resources_tests tests/native/battle_audio_resources_tests.cpp
    src/port/native/space_battle/audio_player.cpp src/port/native/space_battle/audio_resources.cpp
    src/game/battle_ui/battle_sounds.cpp ${MW}/dsound/mixer.cpp ${MW}/config/config.cpp
    ${MW}/res/pe_module.cpp ${MW}/res/mapped_file.cpp ${MW}/res/path.cpp ${REB_PLATFORM_SOURCE})
  target_include_directories(reb_battle_audio_resources_tests PRIVATE src/game src)
  target_compile_options(reb_battle_audio_resources_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_battle_audio_resources_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_battle_audio_resources COMMAND reb_battle_audio_resources_tests)
  add_executable(reb_container_tests tests/native/container_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/ai/ai_ranked_list.cpp src/game/ai/ai_selector.cpp
    src/game/core/entry_identity.cpp src/game/core/game_string.cpp src/game/core/stream_io.cpp
    src/game/core/utility_classes.cpp src/game/gameobj/object_key.cpp src/game/gameobj/status_code.cpp)
  target_include_directories(reb_container_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_container_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_container_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_container_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
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
  add_executable(reb_enhancements_tests tests/native/enhancements_tests.cpp src/port/native/enhancements.cpp
    ${MW}/config/config.cpp ${MW}/res/path.cpp ${REB_PLATFORM_SOURCE})
  target_include_directories(reb_enhancements_tests PRIVATE src/game src)
  target_compile_options(reb_enhancements_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_enhancements_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_enhancements COMMAND reb_enhancements_tests)
  add_executable(reb_save_transaction_tests
    tests/native/save_transaction_tests.cpp ${REB_STREAM_TEST_SOURCES}
    src/game/core/game_string.cpp src/game/core/stream_io.cpp
    src/port/native/save_transaction.cpp src/port/minwin_impl/res/path.cpp)
  target_include_directories(reb_save_transaction_tests PRIVATE src/game src src/port/native/crt)
  target_compile_options(reb_save_transaction_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_NATIVE_RUNTIME})
  target_compile_definitions(reb_save_transaction_tests PRIVATE ${REB_NATIVE_DEFS})
  target_link_libraries(reb_save_transaction_tests PRIVATE ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_save_transaction COMMAND reb_save_transaction_tests)
  add_executable(reb_rle8_tests tests/native/rle8_decoder_tests.cpp src/game/gfx/rle8_decoder.cpp)
  target_include_directories(reb_rle8_tests PRIVATE src/game)
  target_compile_options(reb_rle8_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_rle8_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_rle8_decoder COMMAND reb_rle8_tests)
  add_executable(reb_hd_asset_format_tests tests/native/hd_asset_format_tests.cpp src/game/gfx/hd_asset_format.cpp)
  target_include_directories(reb_hd_asset_format_tests PRIVATE src/game)
  target_compile_options(reb_hd_asset_format_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_hd_asset_format_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_hd_asset_format COMMAND reb_hd_asset_format_tests)
  add_executable(reb_presentation_scale_tests tests/native/presentation_scale_tests.cpp)
  target_include_directories(reb_presentation_scale_tests PRIVATE src)
  target_compile_options(reb_presentation_scale_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_presentation_scale_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_presentation_scale COMMAND reb_presentation_scale_tests)
  add_executable(reb_presentation_integration_tests
    tests/native/presentation_integration_tests.cpp
    ${MW}/user/present.cpp ${MW}/config/config.cpp)
  target_include_directories(reb_presentation_integration_tests PRIVATE src src/game ${MW} ${SDL3_SRC}/include)
  target_compile_options(reb_presentation_integration_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_compile_definitions(reb_presentation_integration_tests PRIVATE
    PRESENT_SCREENSHOT_PATH="${CMAKE_CURRENT_BINARY_DIR}/reb-presentation-integration.bmp")
  target_link_libraries(reb_presentation_integration_tests PRIVATE SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_presentation_integration COMMAND reb_presentation_integration_tests)
  add_executable(reb_audio_recovery_tests tests/native/audio_recovery_tests.cpp
    ${MW}/dsound/mixer.cpp ${MW}/config/config.cpp)
  target_include_directories(reb_audio_recovery_tests PRIVATE src src/game ${SDL3_SRC}/include)
  target_compile_options(reb_audio_recovery_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_audio_recovery_tests PRIVATE SDL3-static ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_audio_recovery COMMAND reb_audio_recovery_tests)
  set_tests_properties(native_presentation_integration PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;REB_SCREENSHOT_PATH=${CMAKE_CURRENT_BINARY_DIR}/reb-presentation-integration.bmp")

  set(REB_GDI_TEST_SUPPORT_SOURCES
    ${MW}/gdi/objects.cpp ${MW}/gdi/dc.cpp ${MW}/gdi/raster.cpp ${MW}/gdi/shapes.cpp ${MW}/gdi/blit_api.cpp
    ${MW}/gdi/tests/user_stub.cpp)
  add_library(reb_gdi_test_support STATIC ${REB_GDI_TEST_SUPPORT_SOURCES})
  target_include_directories(reb_gdi_test_support PRIVATE src/game src ${MW})
  target_compile_options(reb_gdi_test_support PRIVATE ${REB_NATIVE_FLAGS} ${REB_FORCE_MSVC_CRT})
  target_compile_definitions(reb_gdi_test_support PRIVATE REB_MINWIN=1 REB_NATIVE=1)

  add_executable(reb_gdi_render_tests ${MW}/tests/mw_test_main.cpp ${MW}/gdi/gdi_test.cpp)
  target_include_directories(reb_gdi_render_tests PRIVATE src/game src ${MW})
  target_compile_options(reb_gdi_render_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_FORCE_MSVC_CRT})
  target_compile_definitions(reb_gdi_render_tests PRIVATE REB_MINWIN=1 REB_NATIVE=1)
  target_link_libraries(reb_gdi_render_tests PRIVATE reb_gdi_test_support ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_gdi_device_render COMMAND reb_gdi_render_tests device_screen_scales_clipped_drawing)
  add_test(NAME native_gdi_device_line COMMAND reb_gdi_render_tests device_line_uses_subpixel_geometry)
  add_test(NAME native_gdi_screen_resize COMMAND reb_gdi_render_tests screen_resize_preserves_pixels_and_exposes_wider_canvas)
  add_test(NAME native_gdi_device_bitmap_import COMMAND reb_gdi_render_tests device_bitmap_import_preserves_unedited_detail)
  add_test(NAME native_gdi_hd_palette_mapping COMMAND reb_gdi_render_tests device_dib_blit_maps_source_palette_at_both_resolutions)

  add_executable(reb_text_render_tests
    ${MW}/tests/mw_test_main.cpp ${MW}/text/text_test.cpp
    ${MW}/text/font_files.cpp ${MW}/text/ft_font.cpp ${MW}/text/text_api.cpp
    ${MW}/config/config.cpp ${MW}/config/advapi32_api.cpp
    ${MW}/res/pe_module.cpp ${MW}/res/mapped_file.cpp ${MW}/res/formats.cpp ${MW}/res/path.cpp ${MW}/res/modules.cpp ${MW}/res/res_api.cpp
    ${REB_PLATFORM_SOURCE})
  target_include_directories(reb_text_render_tests PRIVATE src/game src ${MW} ${FREETYPE_ROOT}/include src/port/native/crt)
  target_compile_options(reb_text_render_tests PRIVATE ${REB_NATIVE_FLAGS} ${REB_FORCE_MSVC_CRT})
  target_compile_definitions(reb_text_render_tests PRIVATE REB_MINWIN=1 REB_NATIVE=1)
  target_link_libraries(reb_text_render_tests PRIVATE reb_gdi_test_support freetype ${REB_PLATFORM_LIBS} ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_text_device_render COMMAND reb_text_render_tests offscreen_text_keeps_device_detail)

  # The battle screen (docs/design/tactical_combat.md §11.7): SDL-free UI logic, the SDL executor on a software
  # renderer, and the review-capture smoke on the dummy video driver.
  file(GLOB REB_BATTLE_UI_SOURCES CONFIGURE_DEPENDS src/game/battle_ui/*.cpp)
  add_library(reb_battle_ui_test_support STATIC ${REB_BATTLE_UI_SOURCES})
  target_include_directories(reb_battle_ui_test_support PUBLIC src/game src tests/native)
  target_compile_options(reb_battle_ui_test_support PRIVATE ${REB_NATIVE_FLAGS})
  target_sources(reb_battle_ui_test_support PRIVATE src/game/space_battle/unit_names.cpp)
  foreach(REB_BATTLE_UI_TEST camera selection orders draw angled)
    add_executable(reb_battle_ui_${REB_BATTLE_UI_TEST}_tests tests/native/battle_ui_${REB_BATTLE_UI_TEST}_tests.cpp)
    target_compile_options(reb_battle_ui_${REB_BATTLE_UI_TEST}_tests PRIVATE ${REB_NATIVE_FLAGS})
    target_link_libraries(reb_battle_ui_${REB_BATTLE_UI_TEST}_tests PRIVATE reb_battle_ui_test_support
      ${REB_NATIVE_LINK_FLAGS})
  endforeach()
  add_test(NAME native_battle_ui_camera COMMAND reb_battle_ui_camera_tests)
  add_test(NAME native_battle_ui_selection COMMAND reb_battle_ui_selection_tests)
  add_test(NAME native_battle_ui_orders COMMAND reb_battle_ui_orders_tests)
  add_test(NAME native_battle_ui_draw_list COMMAND reb_battle_ui_draw_tests)
  add_test(NAME native_battle_ui_angled COMMAND reb_battle_ui_angled_tests)
  foreach(REB_BATTLE_AUDIO_TEST cues mixing)
    add_executable(reb_battle_audio_${REB_BATTLE_AUDIO_TEST}_tests
      tests/native/battle_audio_${REB_BATTLE_AUDIO_TEST}_tests.cpp)
    target_compile_options(reb_battle_audio_${REB_BATTLE_AUDIO_TEST}_tests PRIVATE ${REB_NATIVE_FLAGS})
    target_link_libraries(reb_battle_audio_${REB_BATTLE_AUDIO_TEST}_tests PRIVATE reb_battle_ui_test_support
      ${REB_NATIVE_LINK_FLAGS})
    add_test(NAME native_battle_audio_${REB_BATTLE_AUDIO_TEST} COMMAND reb_battle_audio_${REB_BATTLE_AUDIO_TEST}_tests)
  endforeach()
  add_executable(reb_battle_ui_executor_tests tests/native/battle_ui_executor_tests.cpp
    src/port/native/space_battle/draw_executor.cpp src/port/native/space_battle/text_atlas.cpp
    src/port/native/space_battle/pack_sprites.cpp src/port/native/space_battle/sprite_pack.cpp
    src/port/native/space_battle/sprite_pack_config.cpp
    src/port/native/space_battle/settings_store.cpp
    ${MW}/text/font_files.cpp ${MW}/config/config.cpp ${MW}/res/path.cpp ${REB_PLATFORM_SOURCE})
  target_include_directories(reb_battle_ui_executor_tests PRIVATE src/game src tests/native ${MW} ${SDL3_SRC}/include
    ${FREETYPE_ROOT}/include src/port/native/crt)
  target_compile_options(reb_battle_ui_executor_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_compile_definitions(reb_battle_ui_executor_tests PRIVATE REB_MINWIN=1 REB_NATIVE=1)
  target_link_libraries(reb_battle_ui_executor_tests PRIVATE reb_battle_ui_test_support freetype SDL3-static ${REB_PLATFORM_LIBS}
    ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_battle_ui_executor COMMAND reb_battle_ui_executor_tests)
  set_tests_properties(native_battle_ui_executor PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
  add_test(NAME native_battle_ui_review_smoke COMMAND rebellion)
  set_tests_properties(native_battle_ui_review_smoke PROPERTIES TIMEOUT 120 ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;REB_SPACE_BATTLE_DEMO=${CMAKE_SOURCE_DIR}/tests/native/battle_runner/synthetic.snapshot;REB_SPACE_BATTLE_REVIEW_SIZE=960x540;REB_SPACE_BATTLE_REVIEW_DIR=${CMAKE_CURRENT_BINARY_DIR}/battle-review-smoke")
  add_test(NAME native_battle_ui_live_smoke COMMAND rebellion)
  set_tests_properties(native_battle_ui_live_smoke PROPERTIES TIMEOUT 60 ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;REB_SPACE_BATTLE_DEMO=${CMAKE_SOURCE_DIR}/tests/native/battle_runner/synthetic.snapshot;REB_SPACE_BATTLE_DEMO_SECONDS=2")
  add_test(NAME native_battle_ui_review_smoke_fabricated COMMAND rebellion)
  set_tests_properties(native_battle_ui_review_smoke_fabricated PROPERTIES TIMEOUT 120 ENVIRONMENT
    "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;REB_SPACE_BATTLE_DEMO=fabricated;REB_SPACE_BATTLE_REVIEW_SIZE=960x540;REB_SPACE_BATTLE_REVIEW_DIR=${CMAKE_CURRENT_BINARY_DIR}/battle-review-smoke-fabricated")

  add_executable(reb_space_battle_snapshot_tests tests/native/space_battle_snapshot_tests.cpp
    src/game/space_battle/snapshot.cpp)
  target_include_directories(reb_space_battle_snapshot_tests PRIVATE src/game)
  target_compile_options(reb_space_battle_snapshot_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_space_battle_snapshot_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_space_battle_snapshot COMMAND reb_space_battle_snapshot_tests)

  add_executable(reb_dib_hd_tests tests/native/dib_hd_tests.cpp src/game/gfx/dib_hd.cpp src/game/gfx/dib_blit.cpp
    src/game/gfx/indexed_sprite_filter.cpp)
  target_include_directories(reb_dib_hd_tests PRIVATE src/game src)
  target_compile_options(reb_dib_hd_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_compile_definitions(reb_dib_hd_tests PRIVATE REB_MINWIN=1 REB_NATIVE=1)
  target_link_libraries(reb_dib_hd_tests PRIVATE ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_dib_hd_composition COMMAND reb_dib_hd_tests)

  # The space battle simulator is pure C++ and the standard library, so its tests link it on its own.
  file(GLOB REB_SPACE_BATTLE_SOURCES CONFIGURE_DEPENDS src/game/space_battle/*.cpp)
  add_library(reb_space_battle STATIC ${REB_SPACE_BATTLE_SOURCES})
  target_include_directories(reb_space_battle PUBLIC src/game)
  target_compile_options(reb_space_battle PRIVATE ${REB_NATIVE_FLAGS} -Wall -Wextra -Wno-unused-parameter)
  add_executable(reb_space_battle_engine_tests tests/native/space_battle_engine_tests.cpp
    src/game/app/battle_engine_space.cpp)
  target_include_directories(reb_space_battle_engine_tests PRIVATE tests/native)
  target_compile_options(reb_space_battle_engine_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_space_battle_engine_tests PRIVATE reb_space_battle ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_space_battle_engine COMMAND reb_space_battle_engine_tests)
  add_test(NAME native_battle_runner_space COMMAND ${CMAKE_COMMAND}
    -DRUNNER=$<TARGET_FILE:reb_battle_runner>
    -DSNAPSHOT=${CMAKE_SOURCE_DIR}/tests/native/battle_runner/synthetic.snapshot
    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/battle_runner_space_test
    -P ${CMAKE_SOURCE_DIR}/cmake/battle_runner_test.cmake)
  set_tests_properties(native_battle_runner_space PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
  foreach(REB_SB_TEST profiles damage battles fighters commands special save performance ai_matchups ai_decisions deployment
          planetary)
    add_executable(reb_space_battle_${REB_SB_TEST}_tests tests/native/space_battle_${REB_SB_TEST}_tests.cpp)
    target_include_directories(reb_space_battle_${REB_SB_TEST}_tests PRIVATE tests/native)
    target_compile_options(reb_space_battle_${REB_SB_TEST}_tests PRIVATE ${REB_NATIVE_FLAGS})
    target_link_libraries(reb_space_battle_${REB_SB_TEST}_tests PRIVATE reb_space_battle ${REB_NATIVE_LINK_FLAGS})
    add_test(NAME native_space_battle_${REB_SB_TEST} COMMAND reb_space_battle_${REB_SB_TEST}_tests)
  endforeach()
  # A readable log of one snapshot run through the simulator (for snapshots dumped from campaigns).
  add_executable(reb_space_battle_log src/tools/space_battle_log.cpp)
  target_compile_options(reb_space_battle_log PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_space_battle_log PRIVATE reb_space_battle ${REB_NATIVE_LINK_FLAGS})
  add_test(NAME native_space_battle_log
    COMMAND reb_space_battle_log --status 60 ${CMAKE_SOURCE_DIR}/tests/native/battle_runner/synthetic.snapshot)
  set_tests_properties(native_space_battle_log PROPERTIES PASS_REGULAR_EXPRESSION "ended after [0-9.]+ s by")

  # The battle HUD, driven through the battle screen against the fabricated battle.
  add_executable(reb_battle_ui_hud_tests tests/native/battle_ui_hud_tests.cpp)
  target_compile_options(reb_battle_ui_hud_tests PRIVATE ${REB_NATIVE_FLAGS})
  target_link_libraries(reb_battle_ui_hud_tests PRIVATE reb_battle_ui_test_support ${REB_NATIVE_LINK_FLAGS})
  foreach(REB_HUD_CASE doctrine overrides folding priorities targeting superlaser roster briefing planetary settings
          autopause)
    add_test(NAME native_battle_ui_hud_${REB_HUD_CASE} COMMAND reb_battle_ui_hud_tests ${REB_HUD_CASE})
  endforeach()
endif()
