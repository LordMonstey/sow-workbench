function(monstey_inherit_source_properties source destination)
    foreach(property IN ITEMS COMPILE_OPTIONS COMPILE_DEFINITIONS INCLUDE_DIRECTORIES COMPILE_FLAGS SKIP_PRECOMPILE_HEADERS)
        get_source_file_property(value "${source}" ${property})
        if(NOT value STREQUAL "NOTFOUND")
            set_source_files_properties("${destination}" PROPERTIES ${property} "${value}")
        endif()
    endforeach()
endfunction()

target_sources(Monstey_Data PRIVATE
    src/gadb/Monstey_GadbCommon.cpp
    src/gadb/Monstey_GadbNames.cpp
    src/gadb/Monstey_GadbReader.cpp
    src/gadb/Monstey_GadbValues.cpp
    src/gadb/Monstey_GadbRecords.cpp
    src/gadb/Monstey_GadbReferences.cpp
    src/gadb/Monstey_GadbHistory.cpp
    src/gadb/Monstey_GadbInspection.cpp
    src/gadb/Monstey_GadbAtoms.cpp
    src/gadb/Monstey_GadbEditing.cpp
    src/gadb/Monstey_GadbProject.cpp
    src/gadb/Monstey_GadbWriter.cpp
)

foreach(source IN ITEMS
    src/gadb/Monstey_GadbCommon.cpp
    src/gadb/Monstey_GadbNames.cpp
    src/gadb/Monstey_GadbReader.cpp
    src/gadb/Monstey_GadbValues.cpp
    src/gadb/Monstey_GadbRecords.cpp
    src/gadb/Monstey_GadbReferences.cpp
    src/gadb/Monstey_GadbHistory.cpp
    src/gadb/Monstey_GadbInspection.cpp
    src/gadb/Monstey_GadbAtoms.cpp
    src/gadb/Monstey_GadbEditing.cpp
    src/gadb/Monstey_GadbProject.cpp
    src/gadb/Monstey_GadbWriter.cpp
)
    monstey_inherit_source_properties("src/Monstey_Gadb.cpp" "${source}")
endforeach()

target_sources(Monstey_Data PRIVATE
    src/world/Monstey_WorldAuthoring.cpp
    src/world/Monstey_WorldCatalog.cpp
    src/world/Monstey_WorldCommon.cpp
    src/world/Monstey_WorldInstances.cpp
    src/world/Monstey_WorldMeshAuthoring.cpp
    src/world/Monstey_WorldMeshReader.cpp
    src/world/Monstey_WorldNumpy.cpp
    src/world/Monstey_WorldObjectAuthoring.cpp
    src/world/Monstey_WorldObjectResources.cpp
    src/world/Monstey_WorldPropertyDatabase.cpp
    src/world/Monstey_WorldRender.cpp
    src/world/Monstey_WorldScene.cpp
    src/world/Monstey_WorldSections.cpp
    src/model/Monstey_ModelGeometry.cpp
    src/model/Monstey_ModelGlbImport.cpp
    src/model/Monstey_ModelGlbReader.cpp
    src/model/Monstey_ModelGlbWriter.cpp
    src/model/Monstey_ModelInspection.cpp
    src/model/Monstey_ModelNumeric.cpp
    src/model/Monstey_ModelReader.cpp
    src/model/Monstey_ModelScene.cpp
    src/model/Monstey_ModelSkeleton.cpp
    src/model/Monstey_ModelWriter.cpp
)

foreach(source IN ITEMS
    src/model/Monstey_ModelGeometry.cpp
    src/model/Monstey_ModelGlbImport.cpp
    src/model/Monstey_ModelGlbReader.cpp
    src/model/Monstey_ModelGlbWriter.cpp
    src/model/Monstey_ModelInspection.cpp
    src/model/Monstey_ModelNumeric.cpp
    src/model/Monstey_ModelReader.cpp
    src/model/Monstey_ModelScene.cpp
    src/model/Monstey_ModelSkeleton.cpp
    src/model/Monstey_ModelWriter.cpp
)
    monstey_inherit_source_properties("src/Monstey_Model.cpp" "${source}")
endforeach()

foreach(source IN ITEMS
    src/world/Monstey_WorldAuthoring.cpp
    src/world/Monstey_WorldCatalog.cpp
    src/world/Monstey_WorldCommon.cpp
    src/world/Monstey_WorldInstances.cpp
    src/world/Monstey_WorldMeshAuthoring.cpp
    src/world/Monstey_WorldMeshReader.cpp
    src/world/Monstey_WorldNumpy.cpp
    src/world/Monstey_WorldObjectAuthoring.cpp
    src/world/Monstey_WorldObjectResources.cpp
    src/world/Monstey_WorldPropertyDatabase.cpp
    src/world/Monstey_WorldRender.cpp
    src/world/Monstey_WorldScene.cpp
    src/world/Monstey_WorldSections.cpp
)
    monstey_inherit_source_properties("src/Monstey_World.cpp" "${source}")
endforeach()

target_sources(Monstey_Data PRIVATE
    src/animation/Monstey_AnixBinary.cpp
    src/animation/Monstey_AnixCodecMath.cpp
    src/animation/Monstey_AnixCodecReader.cpp
    src/animation/Monstey_AnixCodecWriter.cpp
    src/animation/Monstey_AnixEvents.cpp
    src/animation/Monstey_AnixInspection.cpp
    src/animation/Monstey_AnixLibraryReader.cpp
    src/animation/Monstey_AnixLibraryWriter.cpp
    src/animation/Monstey_AnixSampling.cpp
    src/animation/Monstey_AnixScene.cpp
    src/animation/Monstey_AnixSceneImport.cpp
    src/animation/Monstey_AnixSkeleton.cpp
    src/animation/Monstey_AnixTimeline.cpp
)

foreach(source IN ITEMS
    src/animation/Monstey_AnixBinary.cpp
    src/animation/Monstey_AnixCodecMath.cpp
    src/animation/Monstey_AnixCodecReader.cpp
    src/animation/Monstey_AnixCodecWriter.cpp
    src/animation/Monstey_AnixEvents.cpp
    src/animation/Monstey_AnixInspection.cpp
    src/animation/Monstey_AnixLibraryReader.cpp
    src/animation/Monstey_AnixLibraryWriter.cpp
    src/animation/Monstey_AnixSampling.cpp
    src/animation/Monstey_AnixScene.cpp
    src/animation/Monstey_AnixSceneImport.cpp
    src/animation/Monstey_AnixSkeleton.cpp
    src/animation/Monstey_AnixTimeline.cpp
)
    monstey_inherit_source_properties("src/Monstey_Anix.cpp" "${source}")
endforeach()

target_sources(Monstey_Workbench PRIVATE
    src/app/Monstey_AppCommands.cpp
    src/app/Monstey_AppMessages.cpp
    src/app/Monstey_AppSession.cpp
    src/app/Monstey_AppStartup.cpp
    src/app/Monstey_AppStatus.cpp
    src/app/Monstey_ApplicationPaths.cpp
    src/app/controls/Monstey_ActionAvailability.cpp
    src/app/controls/Monstey_AppWindow.cpp
    src/app/controls/Monstey_ControlHelpers.cpp
    src/app/dialogs/Monstey_FileDialogs.cpp
    src/app/dialogs/Monstey_RecordInspector.cpp
    src/app/dialogs/Monstey_TypedValues.cpp
    src/app/dialogs/Monstey_ValueEditor.cpp
    src/app/dialogs/Monstey_ValueEditorMessages.cpp
    src/app/panels/Monstey_AnimatedExtras.cpp
    src/app/panels/Monstey_AnimationCommands.cpp
    src/app/panels/Monstey_AnimationEvents.cpp
    src/app/panels/Monstey_Animations.cpp
    src/app/panels/Monstey_DatabaseNavigation.cpp
    src/app/panels/Monstey_DatabaseRecords.cpp
    src/app/panels/Monstey_DatabaseStructure.cpp
    src/app/panels/Monstey_EffectAttachments.cpp
    src/app/panels/Monstey_EffectCommands.cpp
    src/app/panels/Monstey_EffectEdits.cpp
    src/app/panels/Monstey_EffectLibrary.cpp
    src/app/panels/Monstey_EffectRows.cpp
    src/app/panels/Monstey_Materials.cpp
    src/app/panels/Monstey_ModelActions.cpp
    src/app/panels/Monstey_NativeDatabaseTools.cpp
    src/app/panels/Monstey_NativeProfiles.cpp
    src/app/panels/Monstey_NestedEffects.cpp
    src/app/panels/Monstey_OperationActions.cpp
    src/app/panels/Monstey_OperationRows.cpp
    src/app/panels/Monstey_SocketCommands.cpp
    src/app/panels/Monstey_SocketRows.cpp
    src/app/smoke/Monstey_SmokeAnimation.cpp
    src/app/smoke/Monstey_SmokeAttachments.cpp
    src/app/smoke/Monstey_SmokeDatabase.cpp
    src/app/smoke/Monstey_SmokeEffects.cpp
    src/app/smoke/Monstey_SmokeExtras.cpp
    src/app/smoke/Monstey_SmokeMaterials.cpp
    src/app/smoke/Monstey_SmokeNativeProfile.cpp
    src/app/smoke/Monstey_SmokeNested.cpp
    src/app/smoke/Monstey_SmokeOperations.cpp
    src/app/smoke/Monstey_SmokePrefabs.cpp
    src/app/smoke/Monstey_SmokePreview.cpp
    src/app/smoke/Monstey_SmokeSockets.cpp
    src/app/smoke/Monstey_SmokeStorage.cpp
    src/app/smoke/Monstey_SmokeStructure.cpp
    src/app/smoke/Monstey_SmokeTexture.cpp
)

foreach(source IN ITEMS
    src/app/Monstey_AppCommands.cpp
    src/app/Monstey_AppMessages.cpp
    src/app/Monstey_AppSession.cpp
    src/app/Monstey_AppStartup.cpp
    src/app/Monstey_AppStatus.cpp
    src/app/Monstey_ApplicationPaths.cpp
    src/app/controls/Monstey_ActionAvailability.cpp
    src/app/controls/Monstey_AppWindow.cpp
    src/app/controls/Monstey_ControlHelpers.cpp
    src/app/dialogs/Monstey_FileDialogs.cpp
    src/app/dialogs/Monstey_RecordInspector.cpp
    src/app/dialogs/Monstey_TypedValues.cpp
    src/app/dialogs/Monstey_ValueEditor.cpp
    src/app/dialogs/Monstey_ValueEditorMessages.cpp
    src/app/panels/Monstey_AnimatedExtras.cpp
    src/app/panels/Monstey_AnimationCommands.cpp
    src/app/panels/Monstey_AnimationEvents.cpp
    src/app/panels/Monstey_Animations.cpp
    src/app/panels/Monstey_DatabaseNavigation.cpp
    src/app/panels/Monstey_DatabaseRecords.cpp
    src/app/panels/Monstey_DatabaseStructure.cpp
    src/app/panels/Monstey_EffectAttachments.cpp
    src/app/panels/Monstey_EffectCommands.cpp
    src/app/panels/Monstey_EffectEdits.cpp
    src/app/panels/Monstey_EffectLibrary.cpp
    src/app/panels/Monstey_EffectRows.cpp
    src/app/panels/Monstey_Materials.cpp
    src/app/panels/Monstey_ModelActions.cpp
    src/app/panels/Monstey_NativeDatabaseTools.cpp
    src/app/panels/Monstey_NativeProfiles.cpp
    src/app/panels/Monstey_NestedEffects.cpp
    src/app/panels/Monstey_OperationActions.cpp
    src/app/panels/Monstey_OperationRows.cpp
    src/app/panels/Monstey_SocketCommands.cpp
    src/app/panels/Monstey_SocketRows.cpp
    src/app/smoke/Monstey_SmokeAnimation.cpp
    src/app/smoke/Monstey_SmokeAttachments.cpp
    src/app/smoke/Monstey_SmokeDatabase.cpp
    src/app/smoke/Monstey_SmokeEffects.cpp
    src/app/smoke/Monstey_SmokeExtras.cpp
    src/app/smoke/Monstey_SmokeMaterials.cpp
    src/app/smoke/Monstey_SmokeNativeProfile.cpp
    src/app/smoke/Monstey_SmokeNested.cpp
    src/app/smoke/Monstey_SmokeOperations.cpp
    src/app/smoke/Monstey_SmokePrefabs.cpp
    src/app/smoke/Monstey_SmokePreview.cpp
    src/app/smoke/Monstey_SmokeSockets.cpp
    src/app/smoke/Monstey_SmokeStorage.cpp
    src/app/smoke/Monstey_SmokeStructure.cpp
    src/app/smoke/Monstey_SmokeTexture.cpp
)
    monstey_inherit_source_properties("src/Monstey_App.cpp" "${source}")
endforeach()

target_sources(Monstey_Runtime PRIVATE
    src/runtime/Monstey_RuntimeAnimation.cpp
    src/runtime/Monstey_RuntimeArchives.cpp
    src/runtime/Monstey_RuntimeArguments.cpp
    src/runtime/Monstey_RuntimeCatalog.cpp
    src/runtime/Monstey_RuntimeDatabase.cpp
    src/runtime/Monstey_RuntimeEffects.cpp
    src/runtime/Monstey_RuntimeMaterial.cpp
    src/runtime/Monstey_RuntimeModels.cpp
    src/runtime/Monstey_RuntimeNativeDeclarations.cpp
    src/runtime/Monstey_RuntimeOperations.cpp
    src/runtime/Monstey_RuntimeSession.cpp
    src/runtime/Monstey_RuntimeSettings.cpp
    src/runtime/Monstey_RuntimeStrings.cpp
    src/runtime/Monstey_RuntimeTextures.cpp
    src/runtime/Monstey_RuntimeValidation.cpp
    src/runtime/Monstey_RuntimeWorld.cpp
)

foreach(source IN ITEMS
    src/runtime/Monstey_RuntimeAnimation.cpp
    src/runtime/Monstey_RuntimeArchives.cpp
    src/runtime/Monstey_RuntimeArguments.cpp
    src/runtime/Monstey_RuntimeCatalog.cpp
    src/runtime/Monstey_RuntimeDatabase.cpp
    src/runtime/Monstey_RuntimeEffects.cpp
    src/runtime/Monstey_RuntimeMaterial.cpp
    src/runtime/Monstey_RuntimeModels.cpp
    src/runtime/Monstey_RuntimeNativeDeclarations.cpp
    src/runtime/Monstey_RuntimeOperations.cpp
    src/runtime/Monstey_RuntimeSession.cpp
    src/runtime/Monstey_RuntimeSettings.cpp
    src/runtime/Monstey_RuntimeStrings.cpp
    src/runtime/Monstey_RuntimeTextures.cpp
    src/runtime/Monstey_RuntimeValidation.cpp
    src/runtime/Monstey_RuntimeWorld.cpp
)
    monstey_inherit_source_properties("src/Monstey_Runtime.cpp" "${source}")
endforeach()

target_sources(Monstey_WorkbenchCli PRIVATE
    src/cli/Monstey_CliHelp.cpp
    src/cli/Monstey_CliHttp.cpp
    src/cli/Monstey_CliOptions.cpp
    src/cli/Monstey_CliRequests.cpp
    src/cli/Monstey_CliSession.cpp
    src/cli/Monstey_CliStdio.cpp
    src/cli/Monstey_CommandLine.cpp
)

foreach(source IN ITEMS
    src/cli/Monstey_CliHelp.cpp
    src/cli/Monstey_CliHttp.cpp
    src/cli/Monstey_CliOptions.cpp
    src/cli/Monstey_CliRequests.cpp
    src/cli/Monstey_CliSession.cpp
    src/cli/Monstey_CliStdio.cpp
    src/cli/Monstey_CommandLine.cpp
)
    monstey_inherit_source_properties("src/Monstey_Cli.cpp" "${source}")
endforeach()

target_sources(Monstey_Workbench PRIVATE
    src/app/panels/Monstey_CategoryPresentation.cpp
    src/app/panels/Monstey_DatabaseTree.cpp
    src/app/smoke/Monstey_SmokeDatabaseTree.cpp
)
foreach(source IN ITEMS
    src/app/panels/Monstey_CategoryPresentation.cpp
    src/app/panels/Monstey_DatabaseTree.cpp
    src/app/smoke/Monstey_SmokeDatabaseTree.cpp
)
    monstey_inherit_source_properties("src/Monstey_App.cpp" "${source}")
endforeach()

add_executable(Monstey_CategoryPresentationTests
    tests/Monstey_CategoryPresentationTests.cpp
    src/app/panels/Monstey_CategoryPresentation.cpp
)
target_compile_definitions(Monstey_CategoryPresentationTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(Monstey_CategoryPresentationTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus /fp:strict)
add_test(NAME NativeCategoryPresentation COMMAND Monstey_CategoryPresentationTests)

get_target_property(Monstey_ApplicationTestSources Monstey_Workbench SOURCES)
list(FILTER Monstey_ApplicationTestSources INCLUDE REGEX "^src/app/.*[.]cpp$")
target_sources(Monstey_NativeUiIndependentTests PRIVATE ${Monstey_ApplicationTestSources})
