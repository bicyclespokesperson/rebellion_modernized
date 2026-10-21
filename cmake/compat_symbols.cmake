find_program(REB_COMPAT_UV uv REQUIRED)
get_filename_component(REB_COMPAT_CMAKE_NM_NAME "${CMAKE_NM}" NAME)
if(REB_COMPAT_CMAKE_NM_NAME MATCHES "(^|-)nm(-[0-9]+)?(\\.exe)?$")
  set(REB_COMPAT_NM "${CMAKE_NM}")
else()
  get_filename_component(REB_COMPAT_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  find_program(REB_COMPAT_NM NAMES llvm-nm nm HINTS "${REB_COMPAT_COMPILER_DIR}" REQUIRED)
endif()

set(REB_COMPAT_OBJECT_LIST "${CMAKE_CURRENT_BINARY_DIR}/compat-objects-$<CONFIG>.txt")
file(GENERATE OUTPUT "${REB_COMPAT_OBJECT_LIST}" CONTENT
  "$<JOIN:$<TARGET_OBJECTS:reb_minwin_impl>,\n>\n$<JOIN:$<TARGET_OBJECTS:reb_native_port>,\n>\n")
set(REB_COMPAT_SYMBOL_COMMAND
  "${REB_COMPAT_UV}" --no-cache run --offline --no-project "${CMAKE_SOURCE_DIR}/tools/check_compat_symbols.py"
  --nm "${REB_COMPAT_NM}" --headers "${CMAKE_SOURCE_DIR}/src/port/minwin"
  "${CMAKE_SOURCE_DIR}/src/game/platform/dplay.h"
  --objects-file "${REB_COMPAT_OBJECT_LIST}")
add_custom_target(reb_compat_symbols
  COMMAND ${REB_COMPAT_SYMBOL_COMMAND}
  DEPENDS reb_minwin_impl reb_native_port
  COMMENT "Checking compatibility symbols for Windows API collisions"
  VERBATIM)
add_dependencies(rebellion reb_compat_symbols)
add_dependencies(reb_battle_runner reb_compat_symbols)
if(BUILD_TESTING)
  add_test(NAME native_compat_symbols COMMAND ${REB_COMPAT_SYMBOL_COMMAND})
endif()
