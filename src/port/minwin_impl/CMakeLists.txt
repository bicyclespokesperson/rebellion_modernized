# minwin_impl: the self-contained P1 libraries of the SDL port (docs/phase3_plan.md): config, res, gdi, text, smack, dsound.
# Built stand-alone (not part of the game's CMake): src/port/minwin_impl/run_tests.sh native|i686 configures and runs this.
#   native  host x86-64 clang, runs the unit tests directly
#   i686    clang-cl --target=i686-pc-windows-msvc (cmake/toolchain-win32-clangcl.cmake), tests run under Wine
cmake_minimum_required(VERSION 3.20)
project(minwin_impl C CXX)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
get_filename_component(REPO ${REPO} ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  add_compile_options(/GR- /EHs-c- /arch:IA32 /Zc:sizedDealloc- -Wno-microsoft-cast -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN
                      -Wno-deprecated-declarations)
else()
  add_compile_options(-fms-extensions -fno-rtti -fno-exceptions -fwrapv -fno-strict-aliasing -fsigned-char -ffp-contract=off
                      -Wall -Wno-unused-function -Wno-microsoft-anon-tag -Wno-ignored-attributes)
endif()
include_directories(${REPO}/src ${REPO}/src/game ${CMAKE_CURRENT_SOURCE_DIR})

add_library(mw_testmain STATIC tests/mw_test_main.cpp)

function(mw_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE mw_testmain)
  add_test(NAME ${name} COMMAND ${name})
endfunction()
enable_testing()

# config
add_library(mw_config STATIC config/config.cpp config/advapi32_api.cpp)
mw_test(config_test config/config_test.cpp)
target_link_libraries(config_test PRIVATE mw_config)

# res
add_library(mw_res STATIC res/pe_module.cpp res/mapped_file.cpp res/formats.cpp res/path.cpp res/modules.cpp res/res_api.cpp)
target_link_libraries(mw_res PUBLIC mw_config)
mw_test(res_test res/res_test.cpp)
target_link_libraries(res_test PRIVATE mw_res)
add_executable(res_dump res/tests/res_dump.cpp)
target_link_libraries(res_dump PRIVATE mw_res)

# gdi
add_library(mw_gdi STATIC gdi/objects.cpp gdi/dc.cpp gdi/raster.cpp gdi/shapes.cpp gdi/blit_api.cpp)
add_library(mw_gdi_userstub STATIC gdi/tests/user_stub.cpp)
add_executable(gdi_dump_ours gdi/tests/gdi_dump.cpp gdi/tests/gdi_ops.cpp gdi/tests/dump_io.cpp)
target_compile_definitions(gdi_dump_ours PRIVATE MINWIN_IMPLEMENTATION=1)
target_link_libraries(gdi_dump_ours PRIVATE mw_gdi mw_gdi_userstub)
if(WIN32)
  add_executable(gdi_dump_real gdi/tests/gdi_dump.cpp gdi/tests/gdi_ops.cpp gdi/tests/dump_io.cpp)
  target_link_libraries(gdi_dump_real PRIVATE gdi32 user32)
endif()
mw_test(gdi_test gdi/gdi_test.cpp)
target_link_libraries(gdi_test PRIVATE mw_gdi mw_gdi_userstub)

# text (FreeType from third_party/, fetched by tools/fetch_deps.sh)
set(MW_FREETYPE_DIR ${REPO}/third_party/freetype-2.13.3 CACHE PATH "FreeType source tree")
set(FT_DIR ${MW_FREETYPE_DIR})
if(EXISTS ${FT_DIR}/CMakeLists.txt)
  set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
  set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  if(WIN32 AND XWIN_DIR)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I${XWIN_DIR}/sdk/include/um -I${XWIN_DIR}/sdk/include/shared")
  endif()
  add_subdirectory(${FT_DIR} ${CMAKE_BINARY_DIR}/freetype EXCLUDE_FROM_ALL)
  add_library(mw_text STATIC text/font_files.cpp text/ft_font.cpp text/text_api.cpp)
  target_link_libraries(mw_text PUBLIC freetype mw_gdi mw_res mw_config)
  target_include_directories(mw_text PRIVATE ${FT_DIR}/include)
  mw_test(text_test text/text_test.cpp)
  target_link_libraries(text_test PRIVATE mw_text)
  target_sources(text_test PRIVATE gdi/tests/user_stub.cpp)
  add_executable(text_dump_ours text/tests/text_dump.cpp text/tests/text_ops.cpp gdi/tests/dump_io.cpp gdi/tests/user_stub.cpp)
  target_compile_definitions(text_dump_ours PRIVATE MINWIN_IMPLEMENTATION=1)
  target_link_libraries(text_dump_ours PRIVATE mw_text)
else()
  message(STATUS "third_party/freetype-2.13.3 missing: run tools/fetch_deps.sh; the text library is not built")
endif()
if(WIN32)
  add_executable(text_dump_real text/tests/text_dump.cpp text/tests/text_ops.cpp gdi/tests/dump_io.cpp)
  target_link_libraries(text_dump_real PRIVATE gdi32 user32)
endif()

# dsound
add_library(mw_dsound STATIC dsound/mixer.cpp dsound/dsound_api.cpp)
mw_test(dsound_test dsound/dsound_test.cpp)
target_link_libraries(dsound_test PRIVATE mw_dsound)
