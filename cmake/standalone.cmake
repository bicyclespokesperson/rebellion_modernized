# Standalone rebellion.exe (docs/phase2_plan.md, M6). Included from CMakeLists.txt when REB_STANDALONE is on; the objects
# (reb_game, reb_tactical_stubs) are defined there. Driven by tools/build_standalone.sh, which generates gen/*.def and
# gen/data_image.s on the host before the container stage.
set(REB_GEN ${CMAKE_BINARY_DIR}/gen)

# Import libraries for the DLLs the xwin SDK has no library for (DirectPlay, D3DRM) and for the real SMACKW32.DLL.
set(REB_IMPORT_LIBS ${REB_GEN}/smackw32.lib ${REB_GEN}/dplayx.lib ${REB_GEN}/d3drm.lib)
add_custom_command(
  OUTPUT ${REB_IMPORT_LIBS}
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_import_libs.py --imports ${CMAKE_SOURCE_DIR}/symbols/imports.csv
          --smack-def ${CMAKE_SOURCE_DIR}/src/hybrid/smackw32.def --outdir ${REB_GEN}
  DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_import_libs.py ${CMAKE_SOURCE_DIR}/symbols/imports.csv VERBATIM)
add_custom_target(reb_import_libs DEPENDS ${REB_IMPORT_LIBS})

add_library(reb_standalone_runtime OBJECT src/standalone/entry.cpp src/standalone/crt_shims.cpp src/standalone/iostream_vtable_functions.cpp src/standalone/crash_log.cpp src/standalone/determinism.cpp src/port/sim_harness.cpp src/port/heap_probe.cpp)
target_include_directories(reb_standalone_runtime PRIVATE src/game src)
target_compile_options(reb_standalone_runtime PRIVATE ${REB_HYBRID_FLAGS} -DREB_ZERO_NEW=1)

# Stage 1 (container): which original globals the objects still reference -> gen/data_refs.json (the host stage
# tools/gen_data_image.py --standalone turns it into gen/data_image.s; stage 3 sets REB_DATA_IMAGE).
add_custom_command(
  OUTPUT ${REB_GEN}/data_refs.json ${REB_GEN}/report.json
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py --standalone
          --csv ${CMAKE_SOURCE_DIR}/symbols/symbols.csv --out-report ${REB_GEN}/report.json
          --out-data-refs ${REB_GEN}/data_refs.json
          --runtime $<TARGET_OBJECTS:reb_standalone_runtime> --game $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_tactical_stubs>
  DEPENDS $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_tactical_stubs> $<TARGET_OBJECTS:reb_standalone_runtime>
          ${CMAKE_SOURCE_DIR}/symbols/symbols.csv ${CMAKE_SOURCE_DIR}/tools/gen_link_symbols.py
  COMMAND_EXPAND_LISTS VERBATIM)
add_custom_target(reb_standalone_refs DEPENDS ${REB_GEN}/data_refs.json)

set(REB_STANDALONE_DATA)
set(REB_STANDALONE_RES)
option(REB_DATA_IMAGE "Link the generated gen/data_image.s (stage 3 of tools/build_standalone.sh)" OFF)
if(REB_DATA_IMAGE)
  add_custom_command(
    OUTPUT ${REB_GEN}/data_image.obj
    COMMAND llvm-mc-19 -triple=i686-pc-windows-msvc -filetype=obj -o ${REB_GEN}/data_image.obj ${REB_GEN}/data_image.s
    DEPENDS ${REB_GEN}/data_image.s VERBATIM)
  set_source_files_properties(${REB_GEN}/static_init_table.cpp PROPERTIES GENERATED TRUE COMPILE_OPTIONS "${REB_HYBRID_FLAGS}")
  set(REB_STANDALONE_DATA ${REB_GEN}/data_image.obj ${REB_GEN}/static_init_table.cpp)
  set(REB_STANDALONE_RES ${REB_GEN}/rebexe.res)
endif()

add_executable(rebellion
  $<TARGET_OBJECTS:reb_game> $<TARGET_OBJECTS:reb_tactical_stubs> $<TARGET_OBJECTS:reb_standalone_runtime>
  ${REB_STANDALONE_DATA})
add_dependencies(rebellion reb_import_libs)
set_target_properties(rebellion PROPERTIES LINKER_LANGUAGE CXX OUTPUT_NAME "rebellion")
target_link_options(rebellion PRIVATE /NODEFAULTLIB /SUBSYSTEM:WINDOWS,4.0 /ENTRY:WinMainCRTStartup /DEBUG /OPT:REF /OPT:NOICF
  /BASE:0x400000 /DYNAMICBASE:NO /ERRORLIMIT:0 /DEMANGLE:NO /MAP:${CMAKE_BINARY_DIR}/rebellion.map
  )
if(REB_ALLOW_UNRESOLVED)
  target_link_options(rebellion PRIVATE /FORCE:UNRESOLVED)
endif()
# REB_MINWIN_IMPL (src/port/minwin_impl, docs/phase3_plan.md P1): the native USER32/GDI32/COMCTL32 on SDL3 replaces the real import libraries.
option(REB_MINWIN_IMPL "Link the native minwin implementation (SDL3) instead of user32/gdi32/comctl32" OFF)
set(REB_WINDOWS_LIBS kernel32 user32 gdi32 advapi32 winmm comctl32 version ddraw dsound dxguid avifil32 ole32 uuid)
if(REB_MINWIN_IMPL)
  list(REMOVE_ITEM REB_WINDOWS_LIBS user32 gdi32 comctl32)
endif()
target_link_libraries(rebellion PRIVATE
  libcmt libvcruntime libucrt oldnames legacy_stdio_definitions legacy_stdio_wide_specifiers
  ${REB_WINDOWS_LIBS}
  ${REB_IMPORT_LIBS} ${REB_STANDALONE_RES})

# REB_MINWIN: the game code (reb_game, reb_tactical_stubs) includes the own Win32/DirectX declarations of src/port/minwin
# through platform/win32.h instead of the Windows SDK headers (docs/phase3_plan.md P0c). The runtime objects and the link
# (real import libraries) are unchanged, so the executable must come out the same. The hybrid build never reads this option.
option(REB_MINWIN "Compile the game code against src/port/minwin instead of the Windows SDK headers" OFF)
if(REB_MINWIN)
  target_compile_definitions(reb_game PRIVATE REB_MINWIN=1)
  target_compile_definitions(reb_tactical_stubs PRIVATE REB_MINWIN=1)
  # static_asserts every minwin struct size, field offset and constant against the values measured on the real SDK.
  add_library(reb_minwin_layout OBJECT src/port/minwin/layout_check.cpp)
  target_include_directories(reb_minwin_layout PRIVATE src/game src)
  target_compile_options(reb_minwin_layout PRIVATE ${REB_HYBRID_FLAGS})
  add_dependencies(rebellion reb_minwin_layout)
endif()

# REB_MSG_TRACE_SHIM: log the real user32's per-window message sequence (needs REB_MINWIN; src/standalone/msg_trace_shim.cpp).
option(REB_MSG_TRACE_SHIM "Wrap the game's window procedures to log messages when REB_MSG_TRACE is set" OFF)
if(REB_MSG_TRACE_SHIM)
  if(NOT REB_MINWIN)
    message(FATAL_ERROR "REB_MSG_TRACE_SHIM needs REB_MINWIN=ON")
  endif()
  target_compile_definitions(reb_game PRIVATE REB_MSG_TRACE_SHIM=1)
  target_compile_definitions(reb_tactical_stubs PRIVATE REB_MSG_TRACE_SHIM=1)
  add_library(reb_msg_trace OBJECT src/standalone/msg_trace_shim.cpp src/port/msg_trace.cpp)
  target_include_directories(reb_msg_trace PRIVATE src/game src)
  target_compile_options(reb_msg_trace PRIVATE ${REB_HYBRID_FLAGS})
  target_sources(rebellion PRIVATE $<TARGET_OBJECTS:reb_msg_trace>)
endif()

if(REB_MINWIN_IMPL)
  if(NOT REB_MINWIN)
    message(FATAL_ERROR "REB_MINWIN_IMPL needs REB_MINWIN=ON")
  endif()
  set(SDL3_ROOT ${CMAKE_SOURCE_DIR}/third_party/SDL3)
  if(NOT EXISTS ${SDL3_ROOT}/lib/x86/SDL3.lib)
    message(FATAL_ERROR "SDL3 is missing: run tools/fetch_deps.sh")
  endif()
  # The GDI component (src/port/minwin_impl/gdi) is built by another workstream; until it lands this stand-in provides gdi32.
  option(REB_GDI_STUB "Use the throwaway GDI stand-in" ON)
  set(REB_MINWIN_USER_SOURCES
    src/port/minwin_impl/user/region.cpp src/port/minwin_impl/user/window.cpp src/port/minwin_impl/user/focus.cpp
    src/port/minwin_impl/user/message.cpp src/port/minwin_impl/user/paint.cpp src/port/minwin_impl/user/input.cpp
    src/port/minwin_impl/user/platform.cpp src/port/minwin_impl/user/misc.cpp src/port/minwin_impl/user/mmtimer.cpp
    src/port/minwin_impl/user/cursor.cpp
    src/port/msg_trace.cpp)
  if(REB_GDI_STUB)
    list(APPEND REB_MINWIN_USER_SOURCES src/port/minwin_impl/gdi_stub/gdi_stub.cpp)
  endif()
  add_library(reb_minwin_impl OBJECT ${REB_MINWIN_USER_SOURCES})
  target_include_directories(reb_minwin_impl PRIVATE src/game src ${SDL3_ROOT}/include)
  target_compile_options(reb_minwin_impl PRIVATE ${REB_HYBRID_FLAGS})
  target_compile_definitions(reb_standalone_runtime PRIVATE REB_MINWIN_IMPL=1)

  set(REB_MINWIN_IMPORTS ${REB_GEN}/minwin_imports.cpp)
  add_custom_command(
    OUTPUT ${REB_MINWIN_IMPORTS}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gen_minwin_imports.py --out ${REB_MINWIN_IMPORTS}
            --header ${CMAKE_SOURCE_DIR}/src/port/minwin/user32.h --header ${CMAKE_SOURCE_DIR}/src/port/minwin/gdi32.h
            --header ${CMAKE_SOURCE_DIR}/src/port/minwin/comctl32.h
            --header ${CMAKE_SOURCE_DIR}/src/port/minwin/winmm.h:timeKillEvent
            --extra "wvsprintfA:12:int __stdcall wvsprintfA(char*, const char*, va_list)"
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_minwin_imports.py ${CMAKE_SOURCE_DIR}/src/port/minwin/user32.h
            ${CMAKE_SOURCE_DIR}/src/port/minwin/gdi32.h ${CMAKE_SOURCE_DIR}/src/port/minwin/comctl32.h VERBATIM)
  add_library(reb_minwin_imports OBJECT ${REB_MINWIN_IMPORTS})
  target_include_directories(reb_minwin_imports PRIVATE src/game src)
  target_compile_options(reb_minwin_imports PRIVATE ${REB_HYBRID_FLAGS})

  target_sources(rebellion PRIVATE $<TARGET_OBJECTS:reb_minwin_impl> $<TARGET_OBJECTS:reb_minwin_imports>)
  target_link_libraries(rebellion PRIVATE ${SDL3_ROOT}/lib/x86/SDL3.lib)
endif()
