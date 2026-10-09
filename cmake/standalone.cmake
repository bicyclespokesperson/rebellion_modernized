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
target_link_libraries(rebellion PRIVATE
  libcmt libvcruntime libucrt oldnames legacy_stdio_definitions legacy_stdio_wide_specifiers
  kernel32 user32 gdi32 advapi32 winmm comctl32 version ddraw dsound dxguid avifil32 ole32 uuid
  ${REB_IMPORT_LIBS} ${REB_STANDALONE_RES})
