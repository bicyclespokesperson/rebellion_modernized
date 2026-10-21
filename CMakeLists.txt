cmake_minimum_required(VERSION 3.20)
# FreeType 2.13.2 predates the policy floor enforced by CMake 4.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# macOS: one deployment target for arm64 and x86-64; set before project().
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "Minimum macOS version")
endif()

# Windows: CMake <= 3.24 hands rc.exe the -clang: dependency flags that clang-cl's rc rule generates, and it rejects them
# (FreeType's ftver.rc fails). llvm-rc, shipped with LLVM, accepts them. Set before project() so RC picks it up.
if(WIN32 AND NOT CMAKE_RC_COMPILER)
  get_filename_component(REB_LLVM_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(REB_LLVM_RC llvm-rc HINTS "${REB_LLVM_BIN}")
  if(REB_LLVM_RC)
    set(CMAKE_RC_COMPILER "${REB_LLVM_RC}")
  endif()
endif()

project(rebellion LANGUAGES C CXX)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Rebellion requires a Clang-family C++ compiler (Apple Clang or LLVM Clang)")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REB_NATIVE "Build the native desktop executable" ON)
if(NOT REB_NATIVE)
  message(FATAL_ERROR "The active CMake build is native-only. The former hybrid and standalone Windows build machinery has been retired.")
endif()
foreach(LEGACY_OPTION REB_STANDALONE REB_OWN_DATA REB_OWN_VTABLES REB_MINWIN_IMPL)
  if(${LEGACY_OPTION})
    message(FATAL_ERROR "${LEGACY_OPTION} belongs to the retired hybrid/standalone Windows build. Use the native build (REB_NATIVE=ON).")
  endif()
endforeach()

# Native game and battle simulator sources.
file(GLOB_RECURSE REB_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)

include(${CMAKE_SOURCE_DIR}/cmake/native.cmake)
