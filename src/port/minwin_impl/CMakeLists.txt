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
