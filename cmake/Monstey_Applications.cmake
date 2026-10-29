# Desktop application, command-line interface and stand-alone tools.

add_executable(Monstey_Workbench WIN32
  src/Monstey_App.cpp
  src/Monstey_CurveControl.cpp
  src/Monstey_ModelPreviewWindow.cpp
  ${MONSTEY_APP_SOURCES})
target_link_libraries(Monstey_Workbench PRIVATE
  Monstey_Theme Monstey_Runtime comctl32 comdlg32 shell32 ole32 user32 gdi32 Monstey_TextureWindow)

add_executable(Monstey_WorkbenchCli
  src/Monstey_Cli.cpp
  src/cli/Monstey_CliHelp.cpp
  src/cli/Monstey_CliHttp.cpp
  src/cli/Monstey_CliOptions.cpp
  src/cli/Monstey_CliRequests.cpp
  src/cli/Monstey_CliSession.cpp
  src/cli/Monstey_CliStdio.cpp
  src/cli/Monstey_CommandLine.cpp)
target_link_libraries(Monstey_WorkbenchCli PRIVATE Monstey_Runtime)

# Development builds read the same runtime schemas as an installed package.
# TARGET_FILE_DIR does not add a dependency on the applications (CMP0112).
set(schema_commands)
foreach(app IN ITEMS Monstey_Workbench Monstey_WorkbenchCli)
  list(APPEND schema_commands
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${app}>/schema")
  foreach(schema IN ITEMS gadb-schema.json fx-schema.json)
    list(APPEND schema_commands COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/schema/${schema}" "$<TARGET_FILE_DIR:${app}>/schema/${schema}")
  endforeach()
endforeach()
add_custom_target(Monstey_RuntimeSchemas ${schema_commands} VERBATIM)
add_dependencies(Monstey_Workbench Monstey_RuntimeSchemas)
add_dependencies(Monstey_WorkbenchCli Monstey_RuntimeSchemas)

# Tools shipped with the package.
monstey_program(Monstey_WdscAuxiliaryProbe SOURCES src/tools/Monstey_WdscAuxiliaryProbe.cpp)
monstey_program(Monstey_LtmiTool SOURCES src/Monstey_LtmiTool.cpp STRICT)
monstey_program(Monstey_WorldRuntimeProbe SOURCES src/Monstey_WorldRuntimeTool.cpp LIBS Monstey_WorldRuntime STRICT)
monstey_program(Monstey_WorldSubmitTool SOURCES src/Monstey_WorldSubmitTool.cpp LIBS Monstey_WorldRuntime)
monstey_program(Monstey_GadbRegistryTool SOURCES src/Monstey_GadbRegistryTool.cpp)
monstey_program(Monstey_GadbClassCacheTool SOURCES src/Monstey_GadbClassCacheTool.cpp STRICT)
monstey_program(Monstey_FearDbInspector SOURCES src/Monstey_FearDbInspectorMain.cpp STRICT)
monstey_program(Monstey_FearConstraintInspect SOURCES src/Monstey_FearConstraintInspect.cpp STRICT)
monstey_program(Monstey_FearUnpackedInspector SOURCES src/Monstey_FearUnpackedInspector.cpp OPTIMIZED STATIC_CRT)

# Developer tools that are built but not installed.
monstey_program(Monstey_AtomInspect SOURCES src/Monstey_AtomInspect.cpp OPTIMIZED STATIC_CRT)
monstey_program(Monstey_AudioCountInspect SOURCES src/Monstey_AudioCountInspect.cpp STRICT)
monstey_program(Monstey_SelectedCountInspect SOURCES src/Monstey_SelectedCountInspect.cpp STRICT)
monstey_program(Monstey_GadbBankActivationInspect SOURCES src/Monstey_GadbBankActivationInspect.cpp OPTIMIZED)
monstey_program(Monstey_GadbBankRegistrationTool SOURCES src/Monstey_GadbBankRegistrationTool.cpp)
monstey_program(Monstey_GadbConditionalBuffTool SOURCES src/Monstey_GadbConditionalBuffTool.cpp)
monstey_program(Monstey_GadbClass30Tool SOURCES src/Monstey_GadbClass30Tool.cpp)
monstey_program(Monstey_GadbRemainingContractsTool SOURCES src/Monstey_GadbRemainingContractsTool.cpp)
monstey_program(Monstey_ActualDeclarationLookupTests SOURCES src/Monstey_ActualDeclarationLookupTests.cpp)
monstey_program(Monstey_NavCompactCookerActual STRICT)

# Native preview distribution; no game assets, SDK files or interpreters are installed.
install(TARGETS
  Monstey_Workbench
  Monstey_WorkbenchCli
  Monstey_WdscAuxiliaryProbe
  Monstey_LtmiTool
  Monstey_WorldRuntimeProbe
  Monstey_GadbRegistryTool
  Monstey_GadbClassCacheTool
  Monstey_WorldSubmitTool
  Monstey_FearDbInspector
  Monstey_FearConstraintInspect
  Monstey_FearUnpackedInspector
  RUNTIME DESTINATION .)
install(FILES README.md LICENSE THIRD_PARTY_NOTICES.md DESTINATION .)
install(FILES schema/gadb-schema.json schema/fx-schema.json DESTINATION schema)
install(FILES third_party/zstd/LICENSE third_party/zstd/COPYING third_party/zstd/PROVENANCE.json
  DESTINATION licenses/zstd)
install(FILES third_party/DirectXTex/LICENSE third_party/DirectXTex/PROVENANCE-001.json
  DESTINATION licenses/DirectXTex)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "Monstey-SoW-Workbench")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-preview")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native C++ SoW Workbench development preview")
include(CPack)
