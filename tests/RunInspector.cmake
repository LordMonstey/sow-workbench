file(MAKE_DIRECTORY "${OUTPUT_ROOT}")
string(TIMESTAMP timestamp "%Y%m%dT%H%M%SZ" UTC)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef unique)
execute_process(COMMAND "${EXE}" "${CLI}" "${OUTPUT_ROOT}/cli-${timestamp}-${unique}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 110)
message("${stdout}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Inspector CLI test failed: ${result}\n${stderr}")
endif()
