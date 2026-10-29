# Helpers shared by the Workbench build scripts.

# Warning and conformance flags. Every target linked with Monstey_Core inherits them.
set(MONSTEY_CXX_FLAGS /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)

# monstey_program(<target> [SOURCES <file>...] [LIBS <library>...]
#                 [INCLUDES <directory>...] [STRICT])
#
# Console program built from tests/<target>.cpp unless SOURCES is given, and
# linked with Monstey_Data unless LIBS is given. STRICT compiles it with
# /fp:strict.
function(monstey_program target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "STRICT" "" "SOURCES;LIBS;INCLUDES")
  if(NOT arg_SOURCES)
    set(arg_SOURCES "tests/${target}.cpp")
  endif()
  if(NOT arg_LIBS)
    set(arg_LIBS Monstey_Data)
  endif()
  add_executable(${target} ${arg_SOURCES})
  target_link_libraries(${target} PRIVATE ${arg_LIBS})
  if(arg_INCLUDES)
    target_include_directories(${target} PRIVATE ${arg_INCLUDES})
  endif()
  if(arg_STRICT)
    target_compile_options(${target} PRIVATE /fp:strict)
  endif()
endfunction()

macro(_monstey_test_properties name)
  if(arg_TIMEOUT)
    set_tests_properties(${name} PROPERTIES TIMEOUT ${arg_TIMEOUT})
  endif()
  if(arg_RUN_SERIAL)
    set_tests_properties(${name} PROPERTIES RUN_SERIAL TRUE)
  endif()
  if(arg_LABELS)
    set_tests_properties(${name} PROPERTIES LABELS "${arg_LABELS}")
  endif()
endmacro()

# monstey_test(<name> [TIMEOUT <seconds>] [RUN_SERIAL] [LABELS <label>...]
#              COMMAND <program> [<argument>...])
function(monstey_test name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "RUN_SERIAL" "TIMEOUT" "LABELS;COMMAND")
  add_test(NAME ${name} COMMAND ${arg_COMMAND})
  _monstey_test_properties(${name})
endfunction()

# monstey_script_test(<name> <script> [TIMEOUT <seconds>] [RUN_SERIAL]
#                     [LABELS <label>...] -D<variable>=<value>...)
#
# Runs tests/<script>.cmake in script mode with the given definitions.
function(monstey_script_test name script)
  cmake_parse_arguments(PARSE_ARGV 2 arg "RUN_SERIAL" "TIMEOUT" "LABELS")
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}" ${arg_UNPARSED_ARGUMENTS}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/${script}.cmake")
  _monstey_test_properties(${name})
endfunction()

# monstey_unit_test(<name> <program> <directory> [LIBS <library>...] [STRICT] [ARGS <argument>...])
#
# Builds tests/<program>.cpp and runs it with the scratch directory
# <build>/tests/<directory> followed by ARGS.
function(monstey_unit_test name program directory)
  cmake_parse_arguments(PARSE_ARGV 3 arg "STRICT" "" "LIBS;ARGS")
  set(strict)
  if(arg_STRICT)
    set(strict STRICT)
  endif()
  monstey_program(${program} LIBS ${arg_LIBS} ${strict})
  add_test(NAME ${name} COMMAND ${program} "${CMAKE_CURRENT_BINARY_DIR}/tests/${directory}" ${arg_ARGS})
endfunction()
