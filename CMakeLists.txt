cmake_minimum_required(VERSION 3.20)
project(rebellion LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Reconstructed game code: every .cpp under src/game.
file(GLOB_RECURSE REB_GAME_SOURCES CONFIGURE_DEPENDS src/game/*.cpp)

# ---- Hybrid build -------------------------------------------------------------------------------
# A proxy SMACKW32.DLL loaded by the original REBEXE.EXE. Reimplemented functions are patched over the
# originals; everything not yet reimplemented (functions, globals, the CRT) is resolved to its original
# address at link time. See docs/ARCHITECTURE.md.
# /clang:-fcheck-new: the game's operator new returns NULL and callers check it; clang would delete those checks.
# _ACRTIMP/_CRTIMP empty: CRT functions are the game's statically linked ones, not DLL imports.
set(REB_HYBRID_FLAGS /O2 /Zi /GS- /GR- /EHs-c- /Zl /Zc:sizedDealloc- /arch:IA32 /clang:-fcheck-new -Wno-microsoft-cast
    -D_ACRTIMP= -D_CRTIMP= -D_DCRTIMP=
    -DREB_HYBRID=1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_NO_CRT_STDIO_INLINE -D_CRT_SECURE_NO_WARNINGS)

add_library(reb_game OBJECT ${REB_GAME_SOURCES})
target_include_directories(reb_game PRIVATE src/game src)
target_compile_options(reb_game PRIVATE ${REB_HYBRID_FLAGS})

add_library(reb_runtime OBJECT src/hybrid/dllmain.cpp src/hybrid/hook.cpp src/hybrid/capture.cpp src/hybrid/runtime_mem.cpp)
set_source_files_properties(src/hybrid/runtime_mem.cpp PROPERTIES COMPILE_OPTIONS "-fno-builtin;/Oi-")
target_include_directories(reb_runtime PRIVATE src)
target_compile_options(reb_runtime PRIVATE ${REB_HYBRID_FLAGS})

set(REB_GEN ${CMAKE_BINARY_DIR}/gen)
add_custom_command(
  OUTPUT ${REB_GEN}/link_symbols.obj ${REB_GEN}/report.json
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py
          --csv ${CMAKE_SOURCE_DIR}/symbols/symbols.csv
          --out-asm ${REB_GEN}/link_symbols.s --out-report ${REB_GEN}/report.json
          --runtime $<TARGET_OBJECTS:reb_runtime> --game $<TARGET_OBJECTS:reb_game>
  COMMAND llvm-mc-19 -triple=i686-pc-windows-msvc -filetype=obj -o ${REB_GEN}/link_symbols.obj ${REB_GEN}/link_symbols.s
  DEPENDS $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_runtime>
          ${CMAKE_SOURCE_DIR}/symbols/symbols.csv ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py
  COMMAND_EXPAND_LISTS VERBATIM)

add_library(rebhybrid SHARED
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_runtime>
  ${REB_GEN}/link_symbols.obj src/hybrid/smackw32.def)
set_target_properties(rebhybrid PROPERTIES OUTPUT_NAME "smackw32" PREFIX "" LINKER_LANGUAGE CXX)
target_link_options(rebhybrid PRIVATE /NODEFAULTLIB /ENTRY:DllMain /DEBUG /BASE:0x20000000 /DYNAMICBASE:NO
  /MAP:${CMAKE_BINARY_DIR}/smackw32.map)
add_custom_command(TARGET rebhybrid POST_BUILD
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/patch_vtables.py --dll $<TARGET_FILE:rebhybrid>
          --map ${CMAKE_BINARY_DIR}/smackw32.map --csv ${CMAKE_SOURCE_DIR}/symbols/symbols.csv
  VERBATIM)
target_link_libraries(rebhybrid PRIVATE kernel32 user32 dxguid)
