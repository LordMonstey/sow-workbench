# Static libraries shared by the Workbench, the CLI, the tools and the tests.

set(MONSTEY_DATA_SOURCES
  src/Monstey_AnimatedExtras.cpp
  src/Monstey_AnimationHierarchy.cpp
  src/Monstey_AnimationPreview.cpp
  src/Monstey_AnimationPreviewApi.cpp
  src/Monstey_AnimationProject.cpp
  src/Monstey_Anix.cpp
  src/Monstey_Archive.cpp
  src/Monstey_AssetInspect.cpp
  src/Monstey_Blend.cpp
  src/Monstey_BlendGeometry.cpp
  src/Monstey_BlendModel.cpp
  src/Monstey_Catalog.cpp
  src/Monstey_Collision.cpp
  src/Monstey_Curve.cpp
  src/Monstey_FearAudit.cpp
  src/Monstey_FearConstraintReader.cpp
  src/Monstey_FearConstraintTable.cpp
  src/Monstey_FearCrtComparison.cpp
  src/Monstey_FearDbInspector.cpp
  src/Monstey_FearDbNames.cpp
  src/Monstey_FearDbOpen.cpp
  src/Monstey_FearDbPacked.cpp
  src/Monstey_FearDbPrimitives.cpp
  src/Monstey_FearDbResources.cpp
  src/Monstey_FearUnpackedCallback.cpp
  src/Monstey_FearUnpackedCleanup.cpp
  src/Monstey_FearUnpackedInspection.cpp
  src/Monstey_FearUnpackedLine.cpp
  src/Monstey_FearUnpackedMaterialize.cpp
  src/Monstey_FearUnpackedOpen.cpp
  src/Monstey_FearUnpackedOwned.cpp
  src/Monstey_FearUnpackedValidate.cpp
  src/Monstey_FxAttachment.cpp
  src/Monstey_FxLibrary.cpp
  src/Monstey_FxNested.cpp
  src/Monstey_FxReflection.cpp
  src/Monstey_FxWorkflow.cpp
  src/Monstey_Gadb.cpp
  src/Monstey_GadbAnonymousRecord.cpp
  src/Monstey_GadbAudioActionWord.cpp
  src/Monstey_GadbAudioNextKey.cpp
  src/Monstey_GadbBankActivation.cpp
  src/Monstey_GadbBankApi.cpp
  src/Monstey_GadbBankRegistration.cpp
  src/Monstey_GadbBankSelectorRegisters.cpp
  src/Monstey_GadbBankSynchronization.cpp
  src/Monstey_GadbBinderComposition.cpp
  src/Monstey_GadbBufferedStream.cpp
  src/Monstey_GadbCacheTrace.cpp
  src/Monstey_GadbCachedValue.cpp
  src/Monstey_GadbCategoryLookup.cpp
  src/Monstey_GadbCategoryReader.cpp
  src/Monstey_GadbClassCache.cpp
  src/Monstey_GadbClassDeclaration.cpp
  src/Monstey_GadbClassDeclarationApi.cpp
  src/Monstey_GadbConcreteRecords.cpp
  src/Monstey_GadbEncodedString.cpp
  src/Monstey_GadbIntegerWord.cpp
  src/Monstey_GadbManagerBackend.cpp
  src/Monstey_GadbManagerFree.cpp
  src/Monstey_GadbManagerGuardProjection.cpp
  src/Monstey_GadbManagerLeaveProjection.cpp
  src/Monstey_GadbManagerParent.cpp
  src/Monstey_GadbManagerPlacement.cpp
  src/Monstey_GadbManagerPrimitives.cpp
  src/Monstey_GadbNamePointerBoundary.cpp
  src/Monstey_GadbPoolRelocation.cpp
  src/Monstey_GadbPublication.cpp
  src/Monstey_GadbPublicationFramed.cpp
  src/Monstey_GadbRecordLookup.cpp
  src/Monstey_GadbReferenceWord.cpp
  src/Monstey_GadbRemainingContracts.cpp
  src/Monstey_GadbScalarFloat.cpp
  src/Monstey_GadbSelectedCountWord.cpp
  src/Monstey_GadbStringPointer.cpp
  src/Monstey_GadbValueFixup.cpp
  src/Monstey_Havok.cpp
  src/Monstey_LtaDependency.cpp
  src/Monstey_LtmiMaterial.cpp
  src/Monstey_MaterialProject.cpp
  src/Monstey_Model.cpp
  src/Monstey_ModelEnvelope.cpp
  src/Monstey_ModelPreview.cpp
  src/Monstey_ModelPreviewApi.cpp
  src/Monstey_ModelSockets.cpp
  src/Monstey_NavCompactCooker.cpp
  src/Monstey_NavCompactCookerStages.cpp
  src/Monstey_NavCompactOptimizerCore.cpp
  src/Monstey_NavCompactSortCentroids.cpp
  src/Monstey_NavFinalizeHierarchy.cpp
  src/Monstey_NavSeamAlternate.cpp
  src/Monstey_NavSeamBroadphase.cpp
  src/Monstey_NavSeamGeometry.cpp
  src/Monstey_NavSeamGround.cpp
  src/Monstey_NavSeamMatcher.cpp
  src/Monstey_NavStaticTreeApi.cpp
  src/Monstey_Navigation.cpp
  src/Monstey_NavigationBoundaryQuery.cpp
  src/Monstey_NavigationBounds.cpp
  src/Monstey_NavigationRegistration.cpp
  src/Monstey_NavigationStreaming.cpp
  src/Monstey_NavigationWorld.cpp
  src/Monstey_Physics.cpp
  src/Monstey_PhysicsWorld.cpp
  src/Monstey_SkelDefaultPose.cpp
  src/Monstey_StaticNavFaceBounds.cpp
  src/Monstey_StaticPrefab.cpp
  src/Monstey_Strings.cpp
  src/Monstey_TextureApi.cpp
  src/Monstey_TriangleCollision.cpp
  src/Monstey_WdscAuxiliary.cpp
  src/Monstey_World.cpp
  src/Monstey_WorldAssets.cpp
  src/Monstey_WorldBlender.cpp
  src/Monstey_WorldComposition.cpp
  src/Monstey_WorldEdit.cpp
  src/Monstey_WorldLighting.cpp
  src/Monstey_WorldMerge.cpp
  src/Monstey_WorldScene.cpp
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
  src/gadb/Monstey_GadbAtoms.cpp
  src/gadb/Monstey_GadbCommon.cpp
  src/gadb/Monstey_GadbEditing.cpp
  src/gadb/Monstey_GadbHistory.cpp
  src/gadb/Monstey_GadbInspection.cpp
  src/gadb/Monstey_GadbNames.cpp
  src/gadb/Monstey_GadbProject.cpp
  src/gadb/Monstey_GadbReader.cpp
  src/gadb/Monstey_GadbRecords.cpp
  src/gadb/Monstey_GadbReferences.cpp
  src/gadb/Monstey_GadbValues.cpp
  src/gadb/Monstey_GadbWriter.cpp
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

set(MONSTEY_RUNTIME_SOURCES
  src/Monstey_Http.cpp
  src/Monstey_Protocol.cpp
  src/Monstey_Runtime.cpp
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

# Desktop application sources, shared with the independent UI regression test.
set(MONSTEY_APP_SOURCES
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
  src/app/panels/Monstey_CategoryPresentation.cpp
  src/app/panels/Monstey_DatabaseNavigation.cpp
  src/app/panels/Monstey_DatabaseRecords.cpp
  src/app/panels/Monstey_DatabaseStructure.cpp
  src/app/panels/Monstey_DatabaseTree.cpp
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
  src/app/smoke/Monstey_SmokeDatabaseTree.cpp
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

# Bit-exact floating point for format, geometry and navigation code.
set_source_files_properties(
  src/Monstey_AnimationHierarchy.cpp
  src/Monstey_AnimationPreview.cpp
  src/Monstey_AnimationPreviewApi.cpp
  src/Monstey_AssetInspect.cpp
  src/Monstey_Core.cpp
  src/Monstey_FearConstraintReader.cpp
  src/Monstey_FearConstraintTable.cpp
  src/Monstey_FearCrtComparison.cpp
  src/Monstey_FearDbInspector.cpp
  src/Monstey_FearDbPacked.cpp
  src/Monstey_FearDbPrimitives.cpp
  src/Monstey_FearDbResources.cpp
  src/Monstey_GadbAudioActionWord.cpp
  src/Monstey_GadbAudioNextKey.cpp
  src/Monstey_GadbBankActivation.cpp
  src/Monstey_GadbBankSelectorRegisters.cpp
  src/Monstey_GadbBankSynchronization.cpp
  src/Monstey_GadbBinderComposition.cpp
  src/Monstey_GadbBufferedStream.cpp
  src/Monstey_GadbCacheTrace.cpp
  src/Monstey_GadbCachedValue.cpp
  src/Monstey_GadbCategoryLookup.cpp
  src/Monstey_GadbCategoryReader.cpp
  src/Monstey_GadbClassDeclaration.cpp
  src/Monstey_GadbClassDeclarationApi.cpp
  src/Monstey_GadbEncodedString.cpp
  src/Monstey_GadbIntegerWord.cpp
  src/Monstey_GadbNamePointerBoundary.cpp
  src/Monstey_GadbRecordLookup.cpp
  src/Monstey_GadbReferenceWord.cpp
  src/Monstey_GadbRemainingContracts.cpp
  src/Monstey_GadbScalarFloat.cpp
  src/Monstey_GadbSelectedCountWord.cpp
  src/Monstey_GadbStringPointer.cpp
  src/Monstey_GadbValueFixup.cpp
  src/Monstey_Json.cpp
  src/Monstey_LtmiMaterial.cpp
  src/Monstey_MaterialProject.cpp
  src/Monstey_ModelPreview.cpp
  src/Monstey_ModelPreviewWindow.cpp
  src/Monstey_NavCompactCooker.cpp
  src/Monstey_NavCompactCookerStages.cpp
  src/Monstey_NavCompactOptimizerCore.cpp
  src/Monstey_NavCompactSortCentroids.cpp
  src/Monstey_NavFinalizeHierarchy.cpp
  src/Monstey_NavSeamAlternate.cpp
  src/Monstey_NavSeamBroadphase.cpp
  src/Monstey_NavSeamGeometry.cpp
  src/Monstey_NavSeamGround.cpp
  src/Monstey_NavSeamMatcher.cpp
  src/Monstey_NavStaticTreeApi.cpp
  src/Monstey_NavigationBoundaryQuery.cpp
  src/Monstey_NavigationBounds.cpp
  src/Monstey_NavigationRegistration.cpp
  src/Monstey_NavigationStreaming.cpp
  src/Monstey_SkelDefaultPose.cpp
  src/Monstey_StaticNavFaceBounds.cpp
  src/Monstey_TextureApi.cpp
  PROPERTIES COMPILE_OPTIONS /fp:strict)
set_source_files_properties(
  src/Monstey_App.cpp
  src/Monstey_FearDbNames.cpp
  src/Monstey_FearDbOpen.cpp
  src/Monstey_FearUnpackedCallback.cpp
  src/Monstey_FearUnpackedCleanup.cpp
  src/Monstey_FearUnpackedInspection.cpp
  src/Monstey_FearUnpackedLine.cpp
  src/Monstey_FearUnpackedMaterialize.cpp
  src/Monstey_FearUnpackedOpen.cpp
  src/Monstey_FearUnpackedOwned.cpp
  src/Monstey_FearUnpackedValidate.cpp
  src/Monstey_Gadb.cpp
  src/Monstey_GadbAnonymousRecord.cpp
  src/Monstey_GadbConcreteRecords.cpp
  src/Monstey_GadbManagerBackend.cpp
  src/Monstey_GadbManagerFree.cpp
  src/Monstey_GadbManagerGuardProjection.cpp
  src/Monstey_GadbManagerLeaveProjection.cpp
  src/Monstey_GadbManagerParent.cpp
  src/Monstey_GadbManagerPlacement.cpp
  src/Monstey_GadbManagerPrimitives.cpp
  src/Monstey_GadbPoolRelocation.cpp
  src/Monstey_GadbPublication.cpp
  src/Monstey_GadbPublicationFramed.cpp
  src/Monstey_LtaDependency.cpp
  src/Monstey_Runtime.cpp
  src/gadb/Monstey_GadbAtoms.cpp
  src/gadb/Monstey_GadbCommon.cpp
  src/gadb/Monstey_GadbEditing.cpp
  src/gadb/Monstey_GadbHistory.cpp
  src/gadb/Monstey_GadbInspection.cpp
  src/gadb/Monstey_GadbNames.cpp
  src/gadb/Monstey_GadbProject.cpp
  src/gadb/Monstey_GadbReader.cpp
  src/gadb/Monstey_GadbRecords.cpp
  src/gadb/Monstey_GadbReferences.cpp
  src/gadb/Monstey_GadbValues.cpp
  src/gadb/Monstey_GadbWriter.cpp
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
  ${MONSTEY_APP_SOURCES}
  PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/permissive-;/EHsc;/utf-8;/fp:strict")

add_library(Monstey_Core STATIC src/Monstey_Core.cpp src/Monstey_Json.cpp)
target_include_directories(Monstey_Core PUBLIC include)
target_compile_definitions(Monstey_Core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(Monstey_Core PUBLIC ${MONSTEY_CXX_FLAGS})
target_link_libraries(Monstey_Core PUBLIC bcrypt)

add_library(Monstey_Zstd STATIC third_party/zstd/build/single_file_libs/zstddeclib-in.c)
target_include_directories(Monstey_Zstd PUBLIC third_party/zstd/lib)
target_compile_definitions(Monstey_Zstd PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(Monstey_Zstd PRIVATE /W4 /WX /utf-8)
set_property(TARGET Monstey_Zstd PROPERTY C_STANDARD 11)

# BC1-BC7 block codecs from DirectXTex.
add_library(Monstey_TextureCodec STATIC
  src/Monstey_TextureResource.cpp
  third_party/DirectXTex/src/BC.cpp
  third_party/DirectXTex/src/BC4BC5.cpp
  third_party/DirectXTex/src/BC6HBC7.cpp)
target_include_directories(Monstey_TextureCodec PRIVATE third_party/DirectXTex/src)
target_link_libraries(Monstey_TextureCodec PUBLIC Monstey_Core)
target_compile_options(Monstey_TextureCodec PRIVATE /fp:strict)

# Game formats: databases, archives, worlds, models, animations, effects, navigation.
add_library(Monstey_Data STATIC ${MONSTEY_DATA_SOURCES})
target_link_libraries(Monstey_Data PUBLIC Monstey_Core Monstey_Zstd Monstey_TextureCodec ole32 windowscodecs)

# Session, operation catalog and JSON/HTTP/MCP transports.
add_library(Monstey_Runtime STATIC ${MONSTEY_RUNTIME_SOURCES})
target_link_libraries(Monstey_Runtime PUBLIC Monstey_Data ws2_32)

# Dark palette and native-control painting.
add_library(Monstey_Theme STATIC src/Monstey_Theme.cpp)
target_link_libraries(Monstey_Theme PUBLIC Monstey_Core user32 gdi32 comctl32 uxtheme dwmapi)

add_library(Monstey_TextureWindow STATIC src/Monstey_TexturePreviewWindow.cpp)
target_link_libraries(Monstey_TextureWindow PUBLIC Monstey_Data user32 gdi32 comdlg32 comctl32 Monstey_Theme)
target_compile_options(Monstey_TextureWindow PRIVATE /fp:strict)

# Per-component world and database compatibility probes.
add_library(Monstey_WorldRuntime STATIC src/Monstey_WorldRuntimeProbe.cpp src/Monstey_WorldSubmitContract.cpp)
target_link_libraries(Monstey_WorldRuntime PUBLIC Monstey_Core)
target_compile_options(Monstey_WorldRuntime PRIVATE /fp:strict)
