if(NOT DEFINED GUI OR NOT EXISTS "${GUI}")
  message(FATAL_ERROR "GUI must name the candidate Workbench executable")
endif()
if(NOT DEFINED OUTPUT_ROOT)
  message(FATAL_ERROR "OUTPUT_ROOT must name a fresh owned test directory")
endif()
find_program(MONSTEY_POWERSHELL NAMES powershell.exe powershell REQUIRED)
execute_process(COMMAND "${MONSTEY_POWERSHELL}" -NoLogo -NoProfile -ExecutionPolicy Bypass
  -File "${CMAKE_CURRENT_LIST_DIR}/RunNativeDatabaseGui.ps1"
  -Gui "${GUI}" -OutputRoot "${OUTPUT_ROOT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
message("${output}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Synthetic native Database GUI failed (${result}): ${error}")
endif()
