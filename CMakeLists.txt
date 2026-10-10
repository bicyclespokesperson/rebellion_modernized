cmake_minimum_required(VERSION 3.20)
# FreeType 2.13.2 predates the policy floor enforced by CMake 4.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# macOS: one deployment target for arm64 and x86-64 (must be set before project(); docs/native.md)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "Minimum macOS version")
endif()
project(rebellion LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reconstructed game code: every .cpp under src/game, except the stubs of the 3D tactical code (below).
file(GLOB_RECURSE REB_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)
list(FILTER REB_GAME_SOURCES EXCLUDE REGEX "tactical_3d_absent\\.cpp$")

# With REB_NO_TACTICAL_3D the hybrid replaces the original 3D tactical code that reconstructed code still references by
# logged-nothing stubs (src/game/tactical/tactical_3d_absent.cpp: they carry the original names, so they become hooks), and
# the battle flow resolves a "tactical" vote as auto-resolve. Off by default: the hybrid then runs the original 3D code.
# The stubs are compiled either way (reb_tactical_stubs) so they cannot rot; they are linked only when the option is on.
option(REB_NO_TACTICAL_3D "Stub out the 3D tactical code; battles are always auto-resolved" OFF)

# REB_NATIVE: the native Linux x86-64 executable `rebellion` (docs/native_linux.md); everything lives in cmake/native.cmake.
option(REB_NATIVE "Build the native x86-64 Linux executable (tools/build_native.sh)" OFF)
if(REB_NATIVE)
  include(${CMAKE_SOURCE_DIR}/cmake/native.cmake)
  return()
endif()

# REB_STANDALONE: build rebellion.exe, the executable that runs without REBEXE.EXE (docs/phase2_plan.md, M6). Driven by
# tools/build_standalone.sh; it implies REB_NO_TACTICAL_3D and replaces everything below the "Standalone" marker.
option(REB_STANDALONE "Build the standalone rebellion.exe instead of the hybrid proxy DLL" OFF)
if(REB_STANDALONE)
  set(REB_NO_TACTICAL_3D ON)
endif()

# ---- Hybrid build -------------------------------------------------------------------------------
# A proxy SMACKW32.DLL loaded by the original REBEXE.EXE. Reimplemented functions are patched over the
# originals; everything not yet reimplemented (functions, globals, the CRT) is resolved to its original
# address at link time. See docs/ARCHITECTURE.md.
# /clang:-fcheck-new: the game's operator new returns NULL and callers check it; clang would delete those checks.
# _ACRTIMP/_CRTIMP empty: CRT functions are the game's statically linked ones, not DLL imports.
set(REB_HYBRID_FLAGS /O2 /Zi /GS- /GR- /EHs-c- /Zl /Zc:sizedDealloc- /arch:IA32 /clang:-fcheck-new -Wno-microsoft-cast
    -D_ACRTIMP= -D_CRTIMP= -D_DCRTIMP=
    -DREB_HYBRID=1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_NO_CRT_STDIO_INLINE -D_CRT_SECURE_NO_WARNINGS)

# REB_OWN_DATA: the game's globals live in the DLL (tools/gen_data_image.py, docs/phase2_plan.md M5) instead of being
# bound to their addresses in REBEXE.EXE. tools/build_hybrid.sh --own-data drives the stages (the generator needs the EXE).
option(REB_OWN_DATA "Hybrid DLL owns the game's data (generated data image)" OFF)
# Experiment: objects built by reconstructed code keep our vtable copies instead of the originals' (needs complete vtables).
option(REB_OWN_VTABLES "Do not redirect vptr stores to the original vtables" OFF)
set(REB_PATCH_VTABLES_ARGS)
if(REB_OWN_VTABLES)
  set(REB_PATCH_VTABLES_ARGS --keep-own-vtables)
endif()

add_library(reb_game OBJECT ${REB_GAME_SOURCES})
target_include_directories(reb_game PRIVATE src/game src)
target_compile_options(reb_game PRIVATE ${REB_HYBRID_FLAGS})

add_library(reb_tactical_stubs OBJECT src/game/tactical/tactical_3d_absent.cpp)
target_include_directories(reb_tactical_stubs PRIVATE src/game src)
target_compile_options(reb_tactical_stubs PRIVATE ${REB_HYBRID_FLAGS})
if(REB_NO_TACTICAL_3D)
  target_compile_definitions(reb_game PRIVATE REB_NO_TACTICAL_3D=1)
  set(REB_STUB_OBJECTS $<TARGET_OBJECTS:reb_tactical_stubs>)
endif()

if(REB_STANDALONE)
  # The UCRT's setlocale pulls in atox.obj, which defines atoi/atol; the game's VC5 versions (src/game/crt, 32-bit
  # wrap-around instead of clamping) are renamed so both can link.
  target_compile_definitions(reb_game PRIVATE REB_STANDALONE=1 atoi=reb_atoi atol=reb_atol)
  target_compile_definitions(reb_tactical_stubs PRIVATE REB_STANDALONE=1 atoi=reb_atoi atol=reb_atol)
  include(${CMAKE_SOURCE_DIR}/cmake/standalone.cmake)
  return()
endif()

add_library(reb_runtime OBJECT src/hybrid/dllmain.cpp src/hybrid/hook.cpp src/hybrid/capture.cpp src/hybrid/runtime_mem.cpp src/hybrid/determinism.cpp src/hybrid/own_data.cpp)
set_source_files_properties(src/hybrid/runtime_mem.cpp PROPERTIES COMPILE_OPTIONS "-fno-builtin;/Oi-")
target_include_directories(reb_runtime PRIVATE src)
target_compile_options(reb_runtime PRIVATE ${REB_HYBRID_FLAGS})
if(REB_OWN_DATA)
  target_compile_definitions(reb_runtime PRIVATE REB_OWN_DATA=1)
endif()

set(REB_GEN ${CMAKE_BINARY_DIR}/gen)
set(REB_OWN_DATA_ARGS)
set(REB_OWN_DATA_OUTPUTS)
if(REB_OWN_DATA)
  set(REB_OWN_DATA_ARGS --own-data --out-data-refs ${REB_GEN}/data_refs.json)
  set(REB_OWN_DATA_OUTPUTS ${REB_GEN}/data_refs.json)
endif()
add_custom_command(
  OUTPUT ${REB_GEN}/link_symbols.obj ${REB_GEN}/report.json ${REB_OWN_DATA_OUTPUTS}
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py
          --csv ${CMAKE_SOURCE_DIR}/symbols/symbols.csv
          --out-asm ${REB_GEN}/link_symbols.s --out-report ${REB_GEN}/report.json ${REB_OWN_DATA_ARGS}
          --runtime $<TARGET_OBJECTS:reb_runtime> --game $<TARGET_OBJECTS:reb_game> ${REB_STUB_OBJECTS}
  COMMAND llvm-mc-19 -triple=i686-pc-windows-msvc -filetype=obj -o ${REB_GEN}/link_symbols.obj ${REB_GEN}/link_symbols.s
  DEPENDS $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_runtime> ${REB_STUB_OBJECTS}
          ${CMAKE_SOURCE_DIR}/symbols/symbols.csv ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py
          ${CMAKE_SOURCE_DIR}/tools/own_data.py
  COMMAND_EXPAND_LISTS VERBATIM)
add_custom_target(reb_link_symbols DEPENDS ${REB_GEN}/link_symbols.obj)

set(REB_DATA_IMAGE_OBJECT)
if(REB_OWN_DATA)
  # data_image.s is written by tools/gen_data_image.py on the host (build_hybrid.sh), between reb_link_symbols and the link.
  add_custom_command(
    OUTPUT ${REB_GEN}/data_image.obj
    COMMAND llvm-mc-19 -triple=i686-pc-windows-msvc -filetype=obj -o ${REB_GEN}/data_image.obj ${REB_GEN}/data_image.s
    DEPENDS ${REB_GEN}/data_image.s VERBATIM)
  set(REB_DATA_IMAGE_OBJECT ${REB_GEN}/data_image.obj)
endif()

add_library(rebhybrid SHARED
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_runtime> ${REB_STUB_OBJECTS}
  ${REB_GEN}/link_symbols.obj ${REB_DATA_IMAGE_OBJECT} src/hybrid/smackw32.def)
add_dependencies(rebhybrid reb_tactical_stubs)
set_target_properties(rebhybrid PROPERTIES OUTPUT_NAME "smackw32" PREFIX "" LINKER_LANGUAGE CXX)
target_link_options(rebhybrid PRIVATE /NODEFAULTLIB /ENTRY:DllMain /DEBUG /BASE:0x20000000 /DYNAMICBASE:NO
  /MAP:${CMAKE_BINARY_DIR}/smackw32.map)
add_custom_command(TARGET rebhybrid POST_BUILD
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/patch_vtables.py --dll $<TARGET_FILE:rebhybrid>
          --map ${CMAKE_BINARY_DIR}/smackw32.map --csv ${CMAKE_SOURCE_DIR}/symbols/symbols.csv ${REB_PATCH_VTABLES_ARGS}
  VERBATIM)
# Win32 calls normally bind to REBEXE.EXE's own import table (gen_link_symbols.py); these import libraries only
# serve the hybrid runtime and any API the original never imported.
target_link_libraries(rebhybrid PRIVATE kernel32 user32 gdi32 winmm advapi32 dxguid)
