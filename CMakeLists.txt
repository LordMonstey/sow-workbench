cmake_minimum_required(VERSION 3.24)
project(Monstey_SoWNativeWorkbench VERSION 0.3.0 LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC)
message(FATAL_ERROR "Windows MSVC is required")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_library(Monstey_Core STATIC src/Monstey_Core.cpp src/Monstey_Json.cpp)
target_include_directories(Monstey_Core PUBLIC include)
target_compile_definitions(Monstey_Core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(Monstey_Core PUBLIC /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus)
target_link_libraries(Monstey_Core PUBLIC bcrypt)
add_executable(Monstey_CoreTests tests/Monstey_CoreTests.cpp)
target_link_libraries(Monstey_CoreTests PRIVATE Monstey_Core)
enable_testing()
add_test(NAME NativeCore COMMAND Monstey_CoreTests "${CMAKE_CURRENT_BINARY_DIR}/tests/core")
add_library(Monstey_Data STATIC src/Monstey_Gadb.cpp src/Monstey_Archive.cpp src/Monstey_FearAudit.cpp)
target_link_libraries(Monstey_Data PUBLIC Monstey_Core)
add_executable(Monstey_GadbTests tests/Monstey_GadbTests.cpp)
target_link_libraries(Monstey_GadbTests PRIVATE Monstey_Data)
add_executable(Monstey_ArchiveTests tests/Monstey_ArchiveTests.cpp)
target_link_libraries(Monstey_ArchiveTests PRIVATE Monstey_Data)
add_executable(Monstey_FearAuditTests tests/Monstey_FearAuditTests.cpp)
target_link_libraries(Monstey_FearAuditTests PRIVATE Monstey_Data)
add_test(NAME NativeGadb COMMAND Monstey_GadbTests "${CMAKE_CURRENT_BINARY_DIR}/tests/gadb")
add_test(NAME NativeArchive COMMAND Monstey_ArchiveTests)
add_test(NAME NativeFearAudit COMMAND Monstey_FearAuditTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fear")

target_sources(Monstey_Data PRIVATE src/Monstey_Strings.cpp)
add_library(Monstey_Runtime STATIC src/Monstey_Runtime.cpp)
target_link_libraries(Monstey_Runtime PUBLIC Monstey_Data)
add_executable(Monstey_WorkbenchCli src/Monstey_Cli.cpp)
target_link_libraries(Monstey_WorkbenchCli PRIVATE Monstey_Runtime)
add_executable(Monstey_RuntimeTests tests/Monstey_RuntimeTests.cpp)
target_link_libraries(Monstey_RuntimeTests PRIVATE Monstey_Runtime)
add_executable(Monstey_StringsTests tests/Monstey_StringsTests.cpp)
target_link_libraries(Monstey_StringsTests PRIVATE Monstey_Data)
add_test(NAME NativeRuntime COMMAND Monstey_RuntimeTests "${CMAKE_CURRENT_BINARY_DIR}/tests/runtime" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.skel")
add_test(NAME NativeStrings COMMAND Monstey_StringsTests "${CMAKE_CURRENT_BINARY_DIR}/tests/strings")
find_program(MONSTEY_POWERSHELL NAMES powershell.exe REQUIRED)
add_test(NAME NativeCli COMMAND "${MONSTEY_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_CliTests.ps1" -Executable "$<TARGET_FILE:Monstey_WorkbenchCli>" -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/tests/transports")
target_sources(Monstey_Data PRIVATE src/Monstey_World.cpp)
add_executable(Monstey_WorldTests tests/Monstey_WorldTests.cpp)
target_link_libraries(Monstey_WorldTests PRIVATE Monstey_Data)
add_test(NAME NativeWorld COMMAND Monstey_WorldTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world")
add_library(Monstey_Theme STATIC src/Monstey_Theme.cpp)
target_link_libraries(Monstey_Theme PUBLIC Monstey_Core user32 gdi32 comctl32 uxtheme dwmapi)
add_executable(Monstey_Workbench WIN32 src/Monstey_App.cpp)
target_link_libraries(Monstey_Workbench PRIVATE Monstey_Theme)
target_link_libraries(Monstey_Workbench PRIVATE Monstey_Runtime comctl32 comdlg32 shell32 ole32)
target_sources(Monstey_Runtime PRIVATE src/Monstey_Protocol.cpp src/Monstey_Http.cpp)
target_link_libraries(Monstey_Runtime PUBLIC ws2_32)
add_executable(Monstey_HttpTests tests/Monstey_HttpTests.cpp)
target_link_libraries(Monstey_HttpTests PRIVATE Monstey_Runtime)
add_test(NAME NativeHttp COMMAND Monstey_HttpTests "${CMAKE_CURRENT_BINARY_DIR}/tests/http")
target_sources(Monstey_Data PRIVATE src/Monstey_Model.cpp)
add_executable(Monstey_ModelTests tests/Monstey_ModelTests.cpp)
target_link_libraries(Monstey_ModelTests PRIVATE Monstey_Data)
add_test(NAME NativeModel COMMAND Monstey_ModelTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model")
add_executable(Monstey_ModelPaletteTests tests/Monstey_ModelPaletteTests.cpp)
target_link_libraries(Monstey_ModelPaletteTests PRIVATE Monstey_Data)
add_test(NAME NativeModelPalette COMMAND Monstey_ModelPaletteTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-palette")

enable_language(C)
add_library(Monstey_Zstd STATIC third_party/zstd/build/single_file_libs/zstddeclib-in.c)
target_include_directories(Monstey_Zstd PUBLIC third_party/zstd/lib)
target_compile_definitions(Monstey_Zstd PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(Monstey_Zstd PRIVATE /W4 /WX /utf-8)
set_property(TARGET Monstey_Zstd PROPERTY C_STANDARD 11)
target_link_libraries(Monstey_Data PUBLIC Monstey_Zstd)

# Native preview distribution; no game/SDK assets or interpreters are installed.
install(TARGETS Monstey_Workbench Monstey_WorkbenchCli RUNTIME DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/schema/gadb-schema.json" DESTINATION schema)
install(FILES schema/fx-schema.json DESTINATION schema)
install(FILES third_party/zstd/LICENSE third_party/zstd/COPYING third_party/zstd/PROVENANCE.json DESTINATION licenses/zstd)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME "Monstey-SoW-Workbench")
set(CPACK_PACKAGE_VERSION "0.3.0-preview")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native C++ SoW Workbench development preview")
include(CPack)
target_sources(Monstey_Data PRIVATE src/Monstey_Catalog.cpp)
add_executable(Monstey_CatalogTests tests/Monstey_CatalogTests.cpp)
target_link_libraries(Monstey_CatalogTests PRIVATE Monstey_Data)
add_test(NAME NativeCatalog COMMAND Monstey_CatalogTests "${CMAKE_CURRENT_BINARY_DIR}/tests/catalog")
target_sources(Monstey_Data PRIVATE src/Monstey_Blend.cpp)
add_executable(Monstey_BlendTests tests/Monstey_BlendTests.cpp src/Monstey_Blend.cpp)
target_link_libraries(Monstey_BlendTests PRIVATE Monstey_Core Monstey_Zstd)
add_test(NAME NativeBlend COMMAND Monstey_BlendTests "${CMAKE_CURRENT_BINARY_DIR}/tests/blend")
# Standalone Blend verification also uses the shared native gzip codec.
target_sources(Monstey_BlendTests PRIVATE src/Monstey_Archive.cpp)
target_sources(Monstey_Data PRIVATE src/Monstey_Physics.cpp)
add_executable(Monstey_PhysicsTests tests/Monstey_PhysicsTests.cpp src/Monstey_Physics.cpp)
target_link_libraries(Monstey_PhysicsTests PRIVATE Monstey_Core)
add_test(NAME NativePhysics COMMAND Monstey_PhysicsTests "${CMAKE_CURRENT_BINARY_DIR}/tests/physics")

target_sources(Monstey_Data PRIVATE src/Monstey_PhysicsWorld.cpp)
add_executable(Monstey_PhysicsWorldTests tests/Monstey_PhysicsWorldTests.cpp)
target_link_libraries(Monstey_PhysicsWorldTests PRIVATE Monstey_Data)
add_test(NAME NativePhysicsWorld COMMAND Monstey_PhysicsWorldTests "${CMAKE_CURRENT_BINARY_DIR}/tests/physics-world")
target_sources(Monstey_Data PRIVATE src/Monstey_Havok.cpp)
add_executable(Monstey_HavokTests tests/Monstey_HavokTests.cpp)
target_link_libraries(Monstey_HavokTests PRIVATE Monstey_Data)
add_test(NAME NativeHavok COMMAND Monstey_HavokTests "${CMAKE_CURRENT_BINARY_DIR}/tests/havok")
add_executable(Monstey_NavigationTests tests/Monstey_NavigationTests.cpp)
target_link_libraries(Monstey_NavigationTests PRIVATE Monstey_Data)
add_test(NAME NativeNavigation COMMAND Monstey_NavigationTests "${CMAKE_CURRENT_BINARY_DIR}/tests/navigation")
target_sources(Monstey_Data PRIVATE src/Monstey_Collision.cpp)
add_executable(Monstey_CollisionTests tests/Monstey_CollisionTests.cpp)
target_link_libraries(Monstey_CollisionTests PRIVATE Monstey_Data)
add_test(NAME NativeCollision COMMAND Monstey_CollisionTests "${CMAKE_CURRENT_BINARY_DIR}/tests/collision")
target_sources(Monstey_Data PRIVATE src/Monstey_BlendModel.cpp src/Monstey_Navigation.cpp src/Monstey_NavigationWorld.cpp)
add_executable(Monstey_BlendModelTests tests/Monstey_BlendModelTests.cpp)
target_link_libraries(Monstey_BlendModelTests PRIVATE Monstey_Data)
add_test(NAME NativeBlendModel COMMAND Monstey_BlendModelTests "${CMAKE_CURRENT_BINARY_DIR}/tests/blend-model" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.skel")
set_tests_properties(NativeModel PROPERTIES FIXTURES_SETUP MonsteyNativeModelFixture)
set_tests_properties(NativeRuntime PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)
add_executable(Monstey_ModelBandsTests tests/Monstey_ModelBandsTests.cpp)
target_link_libraries(Monstey_ModelBandsTests PRIVATE Monstey_Data)
add_test(NAME NativeModelBands COMMAND Monstey_ModelBandsTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-bands")
set_tests_properties(NativeBlendModel PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)
target_sources(Monstey_Data PRIVATE src/Monstey_WorldComposition.cpp)
add_executable(Monstey_WorldCompositionTests tests/Monstey_WorldCompositionTests.cpp)
target_link_libraries(Monstey_WorldCompositionTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldComposition COMMAND Monstey_WorldCompositionTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-composition")
target_sources(Monstey_Data PRIVATE src/Monstey_TriangleCollision.cpp)
add_executable(Monstey_TriangleCollisionTests tests/Monstey_TriangleCollisionTests.cpp)
target_link_libraries(Monstey_TriangleCollisionTests PRIVATE Monstey_Data)
add_test(NAME NativeTriangleCollision COMMAND Monstey_TriangleCollisionTests "${CMAKE_CURRENT_BINARY_DIR}/tests/triangle-collision")
add_executable(Monstey_LegacyModelTests tests/Monstey_LegacyModelTests.cpp)
target_link_libraries(Monstey_LegacyModelTests PRIVATE Monstey_Runtime)
add_test(NAME NativeLegacyModel COMMAND Monstey_LegacyModelTests "${CMAKE_CURRENT_BINARY_DIR}/tests/legacy-model" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.skel")
set_tests_properties(NativeLegacyModel PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)
target_sources(Monstey_Data PRIVATE src/Monstey_WorldAssets.cpp)
add_executable(Monstey_WorldAssetsTests tests/Monstey_WorldAssetsTests.cpp)
target_link_libraries(Monstey_WorldAssetsTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldAssets COMMAND Monstey_WorldAssetsTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-assets")
add_executable(Monstey_TriangleWorldTests tests/Monstey_TriangleWorldTests.cpp)
target_link_libraries(Monstey_TriangleWorldTests PRIVATE Monstey_Data)
add_test(NAME NativeTriangleWorld COMMAND Monstey_TriangleWorldTests "${CMAKE_CURRENT_BINARY_DIR}/tests/triangle-world")
target_sources(Monstey_Data PRIVATE src/Monstey_Curve.cpp)
add_executable(Monstey_CurveTests tests/Monstey_CurveTests.cpp)
target_link_libraries(Monstey_CurveTests PRIVATE Monstey_Data)
add_test(NAME NativeCurve COMMAND Monstey_CurveTests "${CMAKE_CURRENT_BINARY_DIR}/tests/curve")
target_sources(Monstey_Data PRIVATE src/Monstey_WorldLighting.cpp src/Monstey_AssetInspect.cpp)
add_executable(Monstey_WorldLightingTests tests/Monstey_WorldLightingTests.cpp)
target_link_libraries(Monstey_WorldLightingTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldLighting COMMAND Monstey_WorldLightingTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-lighting")
add_executable(Monstey_AssetInspectTests tests/Monstey_AssetInspectTests.cpp)
target_link_libraries(Monstey_AssetInspectTests PRIVATE Monstey_Data)
add_test(NAME NativeAssetInspect COMMAND Monstey_AssetInspectTests "${CMAKE_CURRENT_BINARY_DIR}/tests/asset-inspect" "${CMAKE_CURRENT_BINARY_DIR}/tests/model")
set_tests_properties(NativeAssetInspect PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)
target_sources(Monstey_Workbench PRIVATE src/Monstey_CurveControl.cpp)
add_executable(Monstey_CurveControlTests tests/Monstey_CurveControlTests.cpp src/Monstey_CurveControl.cpp)
target_link_libraries(Monstey_CurveControlTests PRIVATE Monstey_Data user32 gdi32)
add_test(NAME NativeCurveControl COMMAND Monstey_CurveControlTests "${CMAKE_CURRENT_BINARY_DIR}/tests/curve-control")
target_sources(Monstey_Data PRIVATE src/Monstey_StaticPrefab.cpp src/Monstey_WorldEdit.cpp)
add_executable(Monstey_StaticPrefabTests tests/Monstey_StaticPrefabTests.cpp)
target_link_libraries(Monstey_StaticPrefabTests PRIVATE Monstey_Data)
add_test(NAME NativeStaticPrefab COMMAND Monstey_StaticPrefabTests "${CMAKE_CURRENT_BINARY_DIR}/tests/static-prefab")
add_executable(Monstey_WorldEditTests tests/Monstey_WorldEditTests.cpp)
target_link_libraries(Monstey_WorldEditTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldEdit COMMAND Monstey_WorldEditTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-edit")
target_sources(Monstey_Data PRIVATE src/Monstey_BlendGeometry.cpp)
add_executable(Monstey_BlendGeometryTests tests/Monstey_BlendGeometryTests.cpp)
target_link_libraries(Monstey_BlendGeometryTests PRIVATE Monstey_Data)
add_test(NAME NativeBlendGeometry COMMAND Monstey_BlendGeometryTests "${CMAKE_CURRENT_BINARY_DIR}/tests/blend-geometry")
target_sources(Monstey_Data PRIVATE src/Monstey_WdscAuxiliary.cpp)
add_executable(Monstey_WdscAuxiliaryTests tests/Monstey_WdscAuxiliaryTests.cpp)
target_link_libraries(Monstey_WdscAuxiliaryTests PRIVATE Monstey_Data)
add_test(NAME NativeWdscAuxiliary COMMAND Monstey_WdscAuxiliaryTests "${CMAKE_CURRENT_BINARY_DIR}/tests/wdsc-auxiliary")
add_executable(Monstey_WdscAuxiliaryProbe src/tools/Monstey_WdscAuxiliaryProbe.cpp)
target_link_libraries(Monstey_WdscAuxiliaryProbe PRIVATE Monstey_Data)
install(TARGETS Monstey_WdscAuxiliaryProbe RUNTIME DESTINATION .)
target_sources(Monstey_Data PRIVATE src/Monstey_WorldBlender.cpp)
add_executable(Monstey_WorldBlenderTests tests/Monstey_WorldBlenderTests.cpp)
target_link_libraries(Monstey_WorldBlenderTests PRIVATE Monstey_Data)
set(MONSTEY_TEST_BLEND_TEMPLATE "$ENV{MONSTEY_BLENDER_TEMPLATE}" CACHE FILEPATH "Stock Blender startup.blend for native SDNA tests")
if(MONSTEY_TEST_BLEND_TEMPLATE)
add_test(NAME NativeWorldBlender COMMAND Monstey_WorldBlenderTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-blender" "${MONSTEY_TEST_BLEND_TEMPLATE}")
set_tests_properties(NativeBlendModel NativeLegacyModel NativeBlendGeometry NativeWorldBlender PROPERTIES ENVIRONMENT "MONSTEY_BLENDER_TEMPLATE=${MONSTEY_TEST_BLEND_TEMPLATE}")
else()
add_test(NAME NativeWorldBlender COMMAND Monstey_WorldBlenderTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-blender")
endif()
target_sources(Monstey_Data PRIVATE src/Monstey_WorldScene.cpp src/Monstey_WorldMerge.cpp)
add_executable(Monstey_WorldSceneTests tests/Monstey_WorldSceneTests.cpp)
target_link_libraries(Monstey_WorldSceneTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldScene COMMAND Monstey_WorldSceneTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-scene")
add_executable(Monstey_WorldMergeTests tests/Monstey_WorldMergeTests.cpp)
target_link_libraries(Monstey_WorldMergeTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldMerge COMMAND Monstey_WorldMergeTests "${CMAKE_CURRENT_BINARY_DIR}/tests/world-merge")
target_sources(Monstey_Data PRIVATE src/Monstey_FxWorkflow.cpp)
add_executable(Monstey_FxWorkflowTests tests/Monstey_FxWorkflowTests.cpp)
target_link_libraries(Monstey_FxWorkflowTests PRIVATE Monstey_Data)
add_test(NAME NativeFxWorkflow COMMAND Monstey_FxWorkflowTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fx-workflow")
target_sources(Monstey_Data PRIVATE src/Monstey_FxReflection.cpp)
add_executable(Monstey_FxReflectionTests tests/Monstey_FxReflectionTests.cpp)
target_link_libraries(Monstey_FxReflectionTests PRIVATE Monstey_Data)
add_test(NAME NativeFxReflection COMMAND Monstey_FxReflectionTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fx-reflection")
add_executable(Monstey_GadbIncomingTests tests/Monstey_GadbIncomingTests.cpp)
target_link_libraries(Monstey_GadbIncomingTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbIncoming COMMAND Monstey_GadbIncomingTests "${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-incoming")
add_executable(Monstey_GadbCloneProjectTests tests/Monstey_GadbCloneProjectTests.cpp)
target_link_libraries(Monstey_GadbCloneProjectTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbCloneProject COMMAND Monstey_GadbCloneProjectTests "${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-clone-project")
target_sources(Monstey_Data PRIVATE src/Monstey_ModelSockets.cpp src/Monstey_FxLibrary.cpp)
add_executable(Monstey_ModelSocketsTests tests/Monstey_ModelSocketsTests.cpp)
target_link_libraries(Monstey_ModelSocketsTests PRIVATE Monstey_Data)
add_test(NAME NativeModelSockets COMMAND Monstey_ModelSocketsTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-sockets")
add_executable(Monstey_FxLibraryTests tests/Monstey_FxLibraryTests.cpp)
target_link_libraries(Monstey_FxLibraryTests PRIVATE Monstey_Data)
add_test(NAME NativeFxLibrary COMMAND Monstey_FxLibraryTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fx-library")
target_sources(Monstey_Data PRIVATE src/Monstey_ModelEnvelope.cpp)
add_executable(Monstey_ModelEnvelopeTests tests/Monstey_ModelEnvelopeTests.cpp)
target_link_libraries(Monstey_ModelEnvelopeTests PRIVATE Monstey_Data)
add_test(NAME NativeModelEnvelope COMMAND Monstey_ModelEnvelopeTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-envelope")
target_sources(Monstey_Data PRIVATE src/Monstey_FxNested.cpp)
add_executable(Monstey_FxNestedTests tests/Monstey_FxNestedTests.cpp)
target_link_libraries(Monstey_FxNestedTests PRIVATE Monstey_Data)
add_test(NAME NativeFxNested COMMAND Monstey_FxNestedTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fx-nested")

target_sources(Monstey_Data PRIVATE src/Monstey_AnimatedExtras.cpp)
add_executable(Monstey_AnimatedExtrasTests tests/Monstey_AnimatedExtrasTests.cpp)
target_link_libraries(Monstey_AnimatedExtrasTests PRIVATE Monstey_Data)
add_test(NAME NativeAnimatedExtras COMMAND Monstey_AnimatedExtrasTests "${CMAKE_CURRENT_BINARY_DIR}/tests/animated-extras")

add_executable(Monstey_GadbOrdinalTests tests/Monstey_GadbOrdinalTests.cpp)
target_link_libraries(Monstey_GadbOrdinalTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbOrdinals COMMAND Monstey_GadbOrdinalTests "${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-ordinals")

target_sources(Monstey_Data PRIVATE src/Monstey_Anix.cpp src/Monstey_AnimationProject.cpp)
add_executable(Monstey_AnixTests tests/Monstey_AnixTests.cpp)
target_link_libraries(Monstey_AnixTests PRIVATE Monstey_Data)
add_test(NAME NativeAnix COMMAND Monstey_AnixTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix")
add_executable(Monstey_AnixAuxiliaryTests tests/Monstey_AnixAuxiliaryTests.cpp)
target_link_libraries(Monstey_AnixAuxiliaryTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixAuxiliary COMMAND Monstey_AnixAuxiliaryTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-auxiliary")
add_executable(Monstey_AnixAv01Tests tests/Monstey_AnixAv01Tests.cpp)
target_link_libraries(Monstey_AnixAv01Tests PRIVATE Monstey_Data)
add_test(NAME NativeAnixAv01 COMMAND Monstey_AnixAv01Tests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-av01")
add_executable(Monstey_AnixUncompressedTests tests/Monstey_AnixUncompressedTests.cpp)
target_link_libraries(Monstey_AnixUncompressedTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixUncompressed COMMAND Monstey_AnixUncompressedTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-uncompressed")
add_executable(Monstey_AnixSpecialClipTests tests/Monstey_AnixSpecialClipTests.cpp)
target_link_libraries(Monstey_AnixSpecialClipTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixSpecialClips COMMAND Monstey_AnixSpecialClipTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-special-clips")
add_executable(Monstey_AnixEventByteTests tests/Monstey_AnixEventByteTests.cpp)
target_link_libraries(Monstey_AnixEventByteTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixEventBytes COMMAND Monstey_AnixEventByteTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-event-bytes")
add_executable(Monstey_AnixGapTests tests/Monstey_AnixGapTests.cpp)
target_link_libraries(Monstey_AnixGapTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixGap COMMAND Monstey_AnixGapTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-gap")
add_executable(Monstey_AnimationProjectTests tests/Monstey_AnimationProjectTests.cpp)
target_link_libraries(Monstey_AnimationProjectTests PRIVATE Monstey_Data)
add_test(NAME NativeAnimationProject COMMAND Monstey_AnimationProjectTests "${CMAKE_CURRENT_BINARY_DIR}/tests/animation-project")

target_sources(Monstey_Data PRIVATE src/Monstey_FxAttachment.cpp)
add_executable(Monstey_FxAttachmentTests tests/Monstey_FxAttachmentTests.cpp)
target_link_libraries(Monstey_FxAttachmentTests PRIVATE Monstey_Data)
add_test(NAME NativeFxAttachment COMMAND Monstey_FxAttachmentTests "${CMAKE_CURRENT_BINARY_DIR}/tests/fx-attachment")

add_executable(Monstey_AnixCanonicalChannelTests tests/Monstey_AnixCanonicalChannelTests.cpp)
target_link_libraries(Monstey_AnixCanonicalChannelTests PRIVATE Monstey_Data)
add_test(NAME NativeAnixCanonicalChannels COMMAND Monstey_AnixCanonicalChannelTests "${CMAKE_CURRENT_BINARY_DIR}/tests/anix-canonical-channels")

# Shared host bind-geometry preview; no native material or animation playback.
target_sources(Monstey_Data PRIVATE src/Monstey_ModelPreview.cpp src/Monstey_ModelPreviewApi.cpp)
target_sources(Monstey_Workbench PRIVATE src/Monstey_ModelPreviewWindow.cpp)
target_link_libraries(Monstey_Workbench PRIVATE user32 gdi32)
add_executable(Monstey_ModelPreviewTests tests/Monstey_ModelPreviewTests.cpp)
target_link_libraries(Monstey_ModelPreviewTests PRIVATE Monstey_Data)
add_test(NAME NativeModelPreview COMMAND Monstey_ModelPreviewTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-preview" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.skel")
add_executable(Monstey_ModelPreviewApiTests tests/Monstey_ModelPreviewApiTests.cpp)
target_link_libraries(Monstey_ModelPreviewApiTests PRIVATE Monstey_Data)
add_test(NAME NativeModelPreviewApi COMMAND Monstey_ModelPreviewApiTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-preview-api")
add_executable(Monstey_ModelPreviewRuntimeTests tests/Monstey_ModelPreviewRuntimeTests.cpp)
target_link_libraries(Monstey_ModelPreviewRuntimeTests PRIVATE Monstey_Runtime)
add_test(NAME NativeModelPreviewRuntime COMMAND Monstey_ModelPreviewRuntimeTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-preview-runtime" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh")
add_executable(Monstey_ModelPreviewWindowTests tests/Monstey_ModelPreviewWindowTests.cpp src/Monstey_ModelPreviewWindow.cpp)
target_link_libraries(Monstey_ModelPreviewWindowTests PRIVATE Monstey_Data user32 gdi32 comctl32)
add_test(NAME NativeModelPreviewWindow COMMAND Monstey_ModelPreviewWindowTests "${CMAKE_CURRENT_BINARY_DIR}/tests/model-preview-window" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.mesh" "${CMAKE_CURRENT_BINARY_DIR}/tests/model/synthetic.skel")
set_tests_properties(NativeModelPreview NativeModelPreviewRuntime NativeModelPreviewWindow PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)

# Ordinary CPU animation and publication regression tests. No game assets installed.
set(MONSTEY_CP017_TEST_RUNNER
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunCheckpoint017Test.cmake"
  CACHE FILEPATH "Fresh-run helper for checkpoint017 tests")
set(_cp017_runner "${MONSTEY_CP017_TEST_RUNNER}")
if(NOT EXISTS "${_cp017_runner}")
  message(FATAL_ERROR "Missing checkpoint017 test runner")
endif()

function(monstey_cp017_add_source_once target relative)
  get_target_property(existing "${target}" SOURCES)
  get_filename_component(wanted "${relative}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT EXISTS "${wanted}")
    message(FATAL_ERROR "Integrate canonical source before wiring: ${wanted}")
  endif()
  foreach(source IN LISTS existing)
    get_filename_component(absolute "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(absolute STREQUAL wanted)
      return()
    endif()
  endforeach()
  target_sources("${target}" PRIVATE "${relative}")
endfunction()
foreach(source src/Monstey_SkelDefaultPose.cpp src/Monstey_AnimationPreview.cpp src/Monstey_AnimationPreviewApi.cpp)
  monstey_cp017_add_source_once(Monstey_Data "${source}")
endforeach()
# Core.cpp/Runtime.cpp/ModelPreview.cpp already belong to existing targets.
# Replace them in place; never add a second source/library copy.
set_property(SOURCE src/Monstey_SkelDefaultPose.cpp src/Monstey_AnimationPreview.cpp
  src/Monstey_ModelPreview.cpp src/Monstey_AnimationPreviewApi.cpp src/Monstey_Core.cpp
  src/Monstey_Json.cpp src/Monstey_Runtime.cpp src/Monstey_ModelPreviewWindow.cpp APPEND PROPERTY COMPILE_OPTIONS /fp:strict)

function(monstey_cp017_test_options target)
  target_compile_features("${target}" PRIVATE cxx_std_20)
  target_compile_definitions("${target}" PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options("${target}" PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /Zc:__cplusplus /fp:strict)
endfunction()
function(monstey_cp017_test name target mode)
  add_test(NAME "${name}" COMMAND "${CMAKE_COMMAND}" "-DTEST_EXE=$<TARGET_FILE:${target}>"
    "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/checkpoint017" "-DTEST_LABEL=${name}"
    "-DTEST_MODE=${mode}" ${ARGN} -P "${_cp017_runner}")
  set_tests_properties("${name}" PROPERTIES TIMEOUT 600 LABELS "checkpoint017")
endfunction()

add_executable(Monstey_SkelDefaultPoseProbe "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_SkelDefaultPoseProbe.cpp")
target_link_libraries(Monstey_SkelDefaultPoseProbe PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_SkelDefaultPoseProbe)
monstey_cp017_test(NativeSkelDefaultPose Monstey_SkelDefaultPoseProbe default)

add_executable(Monstey_AnimationPreviewTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationPreviewTests.cpp")
target_link_libraries(Monstey_AnimationPreviewTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationPreviewTests)
monstey_cp017_test(NativeAnimationPreview Monstey_AnimationPreviewTests plain)

add_executable(Monstey_AnimationPreviewBankPrivateTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationPreviewBankPrivateTests.cpp")
target_link_libraries(Monstey_AnimationPreviewBankPrivateTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationPreviewBankPrivateTests)
monstey_cp017_test(NativeAnimationPreviewBank Monstey_AnimationPreviewBankPrivateTests plain)

add_executable(Monstey_AnimationChannelProbe "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationChannelProbe.cpp")
target_link_libraries(Monstey_AnimationChannelProbe PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationChannelProbe)

add_executable(Monstey_AnimationPreviewApiTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationPreviewApiTests.cpp")
target_link_libraries(Monstey_AnimationPreviewApiTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationPreviewApiTests)
monstey_cp017_test(NativeAnimationPreviewApi Monstey_AnimationPreviewApiTests directory)

add_executable(Monstey_AnimationPreviewRuntimeTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationPreviewRuntimeTests.cpp")
target_link_libraries(Monstey_AnimationPreviewRuntimeTests PRIVATE Monstey_Runtime)
add_dependencies(Monstey_AnimationPreviewRuntimeTests Monstey_AnimationPreviewApiTests)
monstey_cp017_test_options(Monstey_AnimationPreviewRuntimeTests)
monstey_cp017_test(NativeAnimationPreviewRuntime Monstey_AnimationPreviewRuntimeTests runtime "-DSETUP_EXE=$<TARGET_FILE:Monstey_AnimationPreviewApiTests>")

add_executable(Monstey_AnimationPreviewPublicationPrivateTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimationPreviewPublicationPrivateTests.cpp")
target_link_libraries(Monstey_AnimationPreviewPublicationPrivateTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationPreviewPublicationPrivateTests)
monstey_cp017_test(NativeAnimationPreviewPublication Monstey_AnimationPreviewPublicationPrivateTests directory)

# Textually included Core.cpp owns Core symbols. Never link Core/Data/Runtime.
foreach(target Monstey_CorePublicationTests Monstey_CorePublicationLegacyTests)
  add_executable("${target}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_CorePublicationTests.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/Monstey_Json.cpp")
  target_include_directories("${target}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries("${target}" PRIVATE bcrypt)
  monstey_cp017_test_options("${target}")
endforeach()
target_compile_definitions(Monstey_CorePublicationLegacyTests PRIVATE MONSTEY_LEGACY_WRITER)
monstey_cp017_test(NativeCorePublication Monstey_CorePublicationTests directory)
monstey_cp017_test(NativeCorePublicationLegacy Monstey_CorePublicationLegacyTests directory)

option(MONSTEY_CP017_ENABLE_ACTUAL "Enable explicit private actual animation fixtures" OFF)
set(MONSTEY_CP017_ACTUAL_MESH "" CACHE FILEPATH "Private mesh; not copied/installed")
set(MONSTEY_CP017_ACTUAL_SKEL "" CACHE FILEPATH "Private skeleton; not copied/installed")
set(MONSTEY_CP017_ACTUAL_ANIX "" CACHE FILEPATH "Private animation; not copied/installed")
set(MONSTEY_CP017_ACTUAL_CLIP "" CACHE STRING "Qualified moving clip or bind")
option(MONSTEY_CP017_ENABLE_ARMOR_API_ACTUAL "Enable fixture-specific armor API check" OFF)
if(MONSTEY_CP017_ENABLE_ACTUAL)
  foreach(kind MESH SKEL ANIX)
    if(NOT EXISTS "${MONSTEY_CP017_ACTUAL_${kind}}")
      message(FATAL_ERROR "Missing explicit actual ${kind}")
    endif()
  endforeach()
  if(MONSTEY_CP017_ACTUAL_CLIP STREQUAL "")
    message(FATAL_ERROR "Actual clip must be explicit")
  endif()
  set(_actual "-DMESH=${MONSTEY_CP017_ACTUAL_MESH}" "-DSKEL=${MONSTEY_CP017_ACTUAL_SKEL}" "-DANIX=${MONSTEY_CP017_ACTUAL_ANIX}" "-DCLIP=${MONSTEY_CP017_ACTUAL_CLIP}")
  monstey_cp017_test(NativeAnimationPreviewActual Monstey_AnimationPreviewTests animation_actual ${_actual})
  monstey_cp017_test(NativeAnimationChannelProfile Monstey_AnimationChannelProbe channels ${_actual})
  monstey_cp017_test(NativeSkelDefaultActual Monstey_SkelDefaultPoseProbe default "-DSKELS=${MONSTEY_CP017_ACTUAL_SKEL}")
  if(MONSTEY_CP017_ENABLE_ARMOR_API_ACTUAL)
    monstey_cp017_test(NativeAnimationPreviewApiActual Monstey_AnimationPreviewApiTests api_actual ${_actual})
  endif()
endif()

# Actual GUI29 requires bind + hardcoded PL_GL_Might_L1_AOE_LL_2_Idle;
# the synthetic API walk fixture cannot satisfy it. Paths AND hashes mandatory.
option(MONSTEY_CP017_ENABLE_ANIMATED_WINDOW "Enable actual native GUI test" OFF)
foreach(kind MESH SKEL ANIX)
  set(MONSTEY_CP017_WINDOW_${kind} "$ENV{MONSTEY_CP017_WINDOW_${kind}}" CACHE FILEPATH "Private GUI ${kind}; not copied")
  set(MONSTEY_CP017_WINDOW_${kind}_SHA256 "$ENV{MONSTEY_CP017_WINDOW_${kind}_SHA256}" CACHE STRING "Explicit GUI ${kind} SHA256")
endforeach()
if(MONSTEY_CP017_ENABLE_ANIMATED_WINDOW)
  set(_window_args)
  foreach(kind MESH SKEL ANIX)
    set(path "${MONSTEY_CP017_WINDOW_${kind}}")
    set(sha "${MONSTEY_CP017_WINDOW_${kind}_SHA256}")
    string(LENGTH "${sha}" sha_length)
    if(NOT EXISTS "${path}" OR NOT sha_length EQUAL 64 OR NOT sha MATCHES "^[0-9a-fA-F]+$")
      message(FATAL_ERROR "Window actual test needs existing ${kind} and explicit SHA256")
    endif()
    list(APPEND _window_args "-D${kind}=${path}" "-D${kind}_SHA256=${sha}")
  endforeach()
  add_executable(Monstey_AnimatedPreviewWindowTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_AnimatedPreviewWindowTests.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/Monstey_ModelPreviewWindow.cpp")
  target_link_libraries(Monstey_AnimatedPreviewWindowTests PRIVATE Monstey_Data user32 gdi32 comctl32)
  monstey_cp017_test_options(Monstey_AnimatedPreviewWindowTests)
  monstey_cp017_test(NativeAnimatedPreviewWindow Monstey_AnimatedPreviewWindowTests window_actual ${_window_args})
  set_tests_properties(NativeAnimatedPreviewWindow PROPERTIES RUN_SERIAL TRUE LABELS "checkpoint017;native-gui;actual")
endif()
add_executable(Monstey_ModelLayoutAuthoringTests tests/Monstey_ModelLayoutAuthoringTests.cpp)
target_link_libraries(Monstey_ModelLayoutAuthoringTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_ModelLayoutAuthoringTests)
monstey_cp017_test(NativeModelLayoutAuthoring Monstey_ModelLayoutAuthoringTests layout "-DBLEND_TEMPLATE=${MONSTEY_TEST_BLEND_TEMPLATE}")

# Checkpoint018: native LTMI editing and exact ordinary hierarchy arithmetic.
# These sources extend existing targets; no second Core/Runtime copy is added.
foreach(source src/Monstey_LtmiMaterial.cpp src/Monstey_MaterialProject.cpp
    src/Monstey_AnimationHierarchy.cpp)
  monstey_cp017_add_source_once(Monstey_Data "${source}")
endforeach()
set_property(SOURCE src/Monstey_LtmiMaterial.cpp src/Monstey_MaterialProject.cpp
  src/Monstey_AnimationHierarchy.cpp src/Monstey_AnimationPreview.cpp
  src/Monstey_AssetInspect.cpp src/Monstey_App.cpp APPEND PROPERTY COMPILE_OPTIONS /fp:strict)

add_executable(Monstey_LtmiTool src/Monstey_LtmiTool.cpp)
target_link_libraries(Monstey_LtmiTool PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_LtmiTool)
install(TARGETS Monstey_LtmiTool RUNTIME DESTINATION .)

set(_cp018_runner "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunCheckpoint018Test.cmake")
if(NOT EXISTS "${_cp018_runner}")
  message(FATAL_ERROR "Missing checkpoint018 fresh-run helper")
endif()
function(monstey_cp018_test name target mode)
  add_test(NAME "${name}" COMMAND "${CMAKE_COMMAND}" "-DTEST_EXE=$<TARGET_FILE:${target}>"
    "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/checkpoint018" "-DTEST_LABEL=${name}"
    "-DTEST_MODE=${mode}" ${ARGN} -P "${_cp018_runner}")
  set_tests_properties("${name}" PROPERTIES TIMEOUT 600 LABELS "checkpoint018")
endfunction()

add_executable(Monstey_LtmiBaselineTests tests/Monstey_LtmiBaselineTests.cpp)
target_link_libraries(Monstey_LtmiBaselineTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_LtmiBaselineTests)
monstey_cp018_test(NativeLtmiMaterial Monstey_LtmiBaselineTests directory)

add_executable(Monstey_LtmiValueStorageTests tests/Monstey_LtmiValueStorageTests.cpp)
target_link_libraries(Monstey_LtmiValueStorageTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_LtmiValueStorageTests)
monstey_cp018_test(NativeLtmiValueStorage Monstey_LtmiValueStorageTests directory)

add_executable(Monstey_MaterialProjectTests tests/Monstey_MaterialProjectTests.cpp)
target_link_libraries(Monstey_MaterialProjectTests PRIVATE Monstey_Runtime)
monstey_cp017_test_options(Monstey_MaterialProjectTests)
monstey_cp018_test(NativeMaterialProject Monstey_MaterialProjectTests project
  "-DCATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/catalog-017.json")

add_executable(Monstey_LtmiAssetTests tests/Monstey_LtmiAssetTests.cpp)
target_link_libraries(Monstey_LtmiAssetTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_LtmiAssetTests)
monstey_cp018_test(NativeLtmiAsset Monstey_LtmiAssetTests asset
  "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/empty-material-manifest.json")

add_executable(Monstey_AnimationHierarchyTests tests/Monstey_AnimationHierarchyTests.cpp)
target_link_libraries(Monstey_AnimationHierarchyTests PRIVATE Monstey_Data)
monstey_cp017_test_options(Monstey_AnimationHierarchyTests)
monstey_cp018_test(NativeAnimationHierarchy Monstey_AnimationHierarchyTests plain)

# Private actual-material fixtures are opt-in and are never copied or installed.
option(MONSTEY_CP018_ENABLE_ACTUAL_MATERIALS "Enable explicit private material corpus" OFF)
set(MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST "" CACHE FILEPATH "Explicit private material JSON manifest")
if(MONSTEY_CP018_ENABLE_ACTUAL_MATERIALS)
  if(MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST STREQUAL "" OR
      NOT EXISTS "${MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST}")
    message(FATAL_ERROR "Actual material test needs an explicit existing JSON manifest")
  endif()
  monstey_cp018_test(NativeLtmiAssetActual Monstey_LtmiAssetTests asset
    "-DMANIFEST=${MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST}")
  set_tests_properties(NativeLtmiAssetActual PROPERTIES LABELS "checkpoint018;actual")
endif()
# Reviewed native texture codec and shared inspection/export paths.
add_library(Monstey_TextureCodec STATIC src/Monstey_TextureResource.cpp
  third_party/DirectXTex/src/BC.cpp third_party/DirectXTex/src/BC4BC5.cpp
  third_party/DirectXTex/src/BC6HBC7.cpp)
target_include_directories(Monstey_TextureCodec PUBLIC include PRIVATE third_party/DirectXTex/src)
target_link_libraries(Monstey_TextureCodec PUBLIC Monstey_Core)
target_compile_options(Monstey_TextureCodec PRIVATE /fp:strict)
target_link_libraries(Monstey_Data PUBLIC Monstey_TextureCodec)
target_sources(Monstey_Data PRIVATE src/Monstey_TextureApi.cpp)
target_link_libraries(Monstey_Data PUBLIC ole32 windowscodecs)
add_executable(Monstey_TextureResourceTests tests/Monstey_TextureResourceTests.cpp)
target_link_libraries(Monstey_TextureResourceTests PRIVATE Monstey_TextureCodec)
add_test(NAME NativeTextureResource COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TextureResourceTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/texture-resource"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshTexture.cmake")
install(FILES third_party/DirectXTex/LICENSE third_party/DirectXTex/PROVENANCE-001.json
  DESTINATION licenses/DirectXTex)

add_executable(Monstey_TextureApiTests tests/Monstey_TextureApiTests.cpp)
target_link_libraries(Monstey_TextureApiTests PRIVATE Monstey_Runtime)
add_test(NAME NativeTextureApi COMMAND Monstey_TextureApiTests "${CMAKE_CURRENT_BINARY_DIR}/tests/texture-api")

set_source_files_properties(src/Monstey_TextureApi.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

# Native modeless texture inspector; the existing3D preview is independent.
add_library(Monstey_TextureWindow STATIC src/Monstey_TexturePreviewWindow.cpp)
target_include_directories(Monstey_TextureWindow PUBLIC include)
target_link_libraries(Monstey_TextureWindow PUBLIC Monstey_Data user32 gdi32 comdlg32 comctl32)
target_compile_options(Monstey_TextureWindow PRIVATE /fp:strict)
target_link_libraries(Monstey_Workbench PRIVATE Monstey_TextureWindow)
add_executable(Monstey_TexturePreviewWindowTests tests/Monstey_TexturePreviewWindowTests.cpp)
target_link_libraries(Monstey_TexturePreviewWindowTests PRIVATE Monstey_TextureWindow)
target_compile_options(Monstey_TexturePreviewWindowTests PRIVATE /fp:strict)
add_test(NAME NativeTextureWindow COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TexturePreviewWindowTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/texture-window"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshWindow.cmake")
set_tests_properties(NativeTextureWindow PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
add_executable(Monstey_TexturePreviewWindowHoverTests tests/Monstey_TexturePreviewWindowHoverTests.cpp)
target_link_libraries(Monstey_TexturePreviewWindowHoverTests PRIVATE Monstey_TextureWindow)
target_compile_options(Monstey_TexturePreviewWindowHoverTests PRIVATE /fp:strict)
add_test(NAME NativeTextureHover COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TexturePreviewWindowHoverTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/texture-hover"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshWindow.cmake")
set_tests_properties(NativeTextureHover PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)

# Per-component native World/GADB compatibility probes.
add_library(Monstey_WorldRuntime STATIC src/Monstey_WorldRuntimeProbe.cpp)
target_link_libraries(Monstey_WorldRuntime PUBLIC Monstey_Core)
target_compile_options(Monstey_WorldRuntime PRIVATE /fp:strict)
add_executable(Monstey_WorldRuntimeProbe src/Monstey_WorldRuntimeTool.cpp)
target_link_libraries(Monstey_WorldRuntimeProbe PRIVATE Monstey_WorldRuntime)
add_executable(Monstey_WorldRuntimeTests tests/Monstey_WorldRuntimeTests.cpp)
target_link_libraries(Monstey_WorldRuntimeTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_WorldRuntimeTests PRIVATE /fp:strict)
add_executable(Monstey_GadbRegistryTool src/Monstey_GadbRegistryTool.cpp)
target_link_libraries(Monstey_GadbRegistryTool PRIVATE Monstey_Data)
add_executable(Monstey_GadbRegistryTests tests/Monstey_GadbRegistryTests.cpp)
target_link_libraries(Monstey_GadbRegistryTests PRIVATE Monstey_Data)
add_executable(Monstey_GadbNameIdentityTests tests/Monstey_GadbNameIdentityTests.cpp)
target_link_libraries(Monstey_GadbNameIdentityTests PRIVATE Monstey_Data)
foreach(pair IN ITEMS "NativeWorldRuntimeProbe|Monstey_WorldRuntimeTests" "NativeGadbRegistry|Monstey_GadbRegistryTests" "NativeGadbNameIdentity|Monstey_GadbNameIdentityTests")
  string(REPLACE "|" ";" fields "${pair}")
  list(GET fields 0 name)
  list(GET fields 1 target)
  add_test(NAME "${name}" COMMAND "${CMAKE_COMMAND}" "-DTEST_EXE=$<TARGET_FILE:${target}>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-database-components/${name}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
  set_tests_properties("${name}" PROPERTIES TIMEOUT 300)
endforeach()
install(TARGETS Monstey_WorldRuntimeProbe Monstey_GadbRegistryTool RUNTIME DESTINATION .)

# Add after the existing Monstey_Data target and strict test configuration.
add_executable(Monstey_WorldStaticComponentTests tests/Monstey_WorldStaticComponentTests.cpp)
target_link_libraries(Monstey_WorldStaticComponentTests PRIVATE Monstey_Data)
target_compile_options(Monstey_WorldStaticComponentTests PRIVATE /fp:strict)
add_test(NAME NativeWorldStaticComponent
  COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:Monstey_WorldStaticComponentTests>"
    "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-static-component"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunWorldStaticComponent.cmake")
# Actual assets are explicit private test inputs, never installed fixtures.
# cmake -DEXE=... -DWORK_ROOT=... -DACTUAL_MESH=... -DACTUAL_RENDER=... -P ...

set_tests_properties(NativeWorldStaticComponent PROPERTIES TIMEOUT 600)
option(MONSTEY_WORLD_STATIC_ENABLE_ACTUAL "Enable explicit private actual static pair tests" OFF)
set(MONSTEY_WORLD_STATIC_ACTUAL_MESH "" CACHE FILEPATH "Explicit private actual MESH5 input")
set(MONSTEY_WORLD_STATIC_ACTUAL_RENDER "" CACHE FILEPATH "Explicit private actual WDSC9 input")
if(MONSTEY_WORLD_STATIC_ENABLE_ACTUAL)
  if(NOT EXISTS "${MONSTEY_WORLD_STATIC_ACTUAL_MESH}" OR NOT EXISTS "${MONSTEY_WORLD_STATIC_ACTUAL_RENDER}")
    message(FATAL_ERROR "Both existing private static pair paths required")
  endif()
  add_test(NAME NativeWorldStaticComponentActual COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:Monstey_WorldStaticComponentTests>"
    "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-static-component-actual"
    "-DACTUAL_MESH=${MONSTEY_WORLD_STATIC_ACTUAL_MESH}"
    "-DACTUAL_RENDER=${MONSTEY_WORLD_STATIC_ACTUAL_RENDER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunWorldStaticComponent.cmake")
  set_tests_properties(NativeWorldStaticComponentActual PROPERTIES TIMEOUT 600 LABELS "actual")
endif()

add_executable(Monstey_GadbRegistryBoundaryTests tests/Monstey_GadbRegistryBoundaryTests.cpp)
target_link_libraries(Monstey_GadbRegistryBoundaryTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbRegistryBoundary COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbRegistryBoundaryTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-database-components/NativeGadbRegistryBoundary"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
set_tests_properties(NativeGadbRegistryBoundary PROPERTIES TIMEOUT 300)

add_executable(Monstey_WorldNestedBundlesTests tests/Monstey_WorldNestedBundlesTests.cpp)
target_link_libraries(Monstey_WorldNestedBundlesTests PRIVATE Monstey_Data)
add_test(NAME NativeWorldNestedBundles COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_WorldNestedBundlesTests>"
  "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-nested-bundles"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunWorldStaticComponent.cmake")
set_tests_properties(NativeWorldNestedBundles PROPERTIES TIMEOUT 600)

# Read-only advanced database layout and native class-cache inspection.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbClassCache.cpp)
add_executable(Monstey_GadbClassCacheTool src/Monstey_GadbClassCacheTool.cpp)
target_link_libraries(Monstey_GadbClassCacheTool PRIVATE Monstey_Data)
add_executable(Monstey_GadbDescriptorLayoutsTests tests/Monstey_GadbDescriptorLayoutsTests.cpp)
target_link_libraries(Monstey_GadbDescriptorLayoutsTests PRIVATE Monstey_Data)
add_executable(Monstey_GadbClassCacheTests tests/Monstey_GadbClassCacheTests.cpp)
target_link_libraries(Monstey_GadbClassCacheTests PRIVATE Monstey_Data)
add_executable(Monstey_GadbLayoutCounterexample tests/Monstey_GadbLayoutCounterexample.cpp)
target_link_libraries(Monstey_GadbLayoutCounterexample PRIVATE Monstey_Data)
foreach(target Monstey_GadbClassCacheTool Monstey_GadbDescriptorLayoutsTests Monstey_GadbClassCacheTests Monstey_GadbLayoutCounterexample)
  target_compile_options(${target} PRIVATE /fp:strict)
endforeach()
foreach(pair IN ITEMS "NativeGadbDescriptorLayouts|Monstey_GadbDescriptorLayoutsTests|layout" "NativeGadbClassCache|Monstey_GadbClassCacheTests|cache")
  string(REPLACE "|" ";" fields "${pair}")
  list(GET fields 0 name)
  list(GET fields 1 target)
  list(GET fields 2 mode)
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:${target}>" "-DMODE=${mode}"
    "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-advanced/${name}"
    "-DGADB_EXE=$<TARGET_FILE:Monstey_GadbTests>"
    "-DNAME_EXE=$<TARGET_FILE:Monstey_GadbNameIdentityTests>"
    "-DCASE_EXE=$<TARGET_FILE:Monstey_GadbLayoutCounterexample>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunGadbAdvanced.cmake")
  set_tests_properties(${name} PROPERTIES TIMEOUT 600)
endforeach()
install(TARGETS Monstey_GadbClassCacheTool RUNTIME DESTINATION .)
# Exact selected native model resolver and read-only live snapshot adapter.
target_sources(Monstey_WorldRuntime PRIVATE src/Monstey_WorldSubmitContract.cpp)
add_executable(Monstey_WorldSubmitTool src/Monstey_WorldSubmitTool.cpp)
target_link_libraries(Monstey_WorldSubmitTool PRIVATE Monstey_WorldRuntime)
add_executable(Monstey_WorldSubmitTests tests/Monstey_WorldSubmitTests.cpp)
target_link_libraries(Monstey_WorldSubmitTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_WorldSubmitTests PRIVATE /fp:strict)
add_test(NAME NativeWorldSubmitContract COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldSubmitTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-submit-contract"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
set_tests_properties(NativeWorldSubmitContract PROPERTIES TIMEOUT 300)
install(TARGETS Monstey_WorldSubmitTool RUNTIME DESTINATION .)
# Qualified conditional common-static cull modes; no guessed live view state.
add_executable(Monstey_WorldStaticPassTests tests/Monstey_WorldStaticPassTests.cpp)
target_link_libraries(Monstey_WorldStaticPassTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_WorldStaticPassTests PRIVATE /fp:strict)
add_test(NAME NativeWorldStaticPass COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldStaticPassTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/world-static-pass"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
set_tests_properties(NativeWorldStaticPass PROPERTIES TIMEOUT 300)
# Additive public synthetic acceptance target; existing GADB/checker suites remain unchanged.
add_executable(Monstey_GadbWriterClassCacheTests tests/Monstey_GadbWriterClassCacheTests.cpp)
target_link_libraries(Monstey_GadbWriterClassCacheTests PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbWriterClassCacheTests PRIVATE /fp:strict)
add_test(NAME NativeGadbWriterClassCache COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbWriterClassCacheTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-writer-class-cache"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunGadbWriterClassCache.cmake")
set_tests_properties(NativeGadbWriterClassCache PROPERTIES TIMEOUT 300)

# Existing Monstey_Runtime/Data sources receive the three product diffs only.
add_executable(Monstey_GadbRuntimeCacheGateTests tests/Monstey_GadbRuntimeCacheGateTests.cpp)
target_link_libraries(Monstey_GadbRuntimeCacheGateTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_GadbRuntimeCacheGateTests PRIVATE /fp:strict)
add_test(NAME NativeGadbRuntimeCacheGate COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbRuntimeCacheGateTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-runtime-cache-gate"
  "-DBASELINE_CATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/catalog-before-gadb-runtime-gate.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunGadbRuntimeCacheGate.cmake")
set_tests_properties(NativeGadbRuntimeCacheGate PROPERTIES TIMEOUT 300)

# The only product source change is src/Monstey_App.cpp.
# Existing guarded Runtime/core and ordinary GUI dependencies are prerequisites.
add_executable(Monstey_ProfileUiFixtures tests/Monstey_ProfileUiFixtures.cpp)
target_link_libraries(Monstey_ProfileUiFixtures PRIVATE Monstey_Runtime)
target_compile_options(Monstey_ProfileUiFixtures PRIVATE /fp:strict)
add_test(NAME NativeGadbProfileUi COMMAND "${CMAKE_COMMAND}"
  "-DUI_EXE=$<TARGET_FILE:Monstey_Workbench>"
  "-DFIXTURE_EXE=$<TARGET_FILE:Monstey_ProfileUiFixtures>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-profile-ui"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunNativeProfileUi.cmake")
set_tests_properties(NativeGadbProfileUi PROPERTIES TIMEOUT 450 RUN_SERIAL TRUE)
# Fixture producer includes tests/Monstey_RuntimeGateFixtures.inc from the Runtime guard tests.

# Add after the existing Monstey_WorldRuntime target.
add_executable(Monstey_HkaiActivationTests tests/Monstey_HkaiActivationTests.cpp)
target_link_libraries(Monstey_HkaiActivationTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_HkaiActivationTests PRIVATE
  /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_test(NAME NativeHkaiActivation
  COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXE=$<TARGET_FILE:Monstey_HkaiActivationTests>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/hkai-activation"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
set_tests_properties(NativeHkaiActivation PROPERTIES TIMEOUT 300)
# Existing C++20, static CRT, includes and fresh-report runner remain in use.
# No CLI/Runtime/catalog change is made by this snippet.

# The existing Monstey_WorldRuntimeProbe target consumes only the localized Tool diff.
# The reviewed Probe API/implementation and its31-group target are prerequisites.
add_test(NAME NativeWorldHkaiActivationCli COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_WorldRuntimeProbe>"
  "-DPOWERSHELL=${MONSTEY_POWERSHELL}"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/hkai-activation-cli"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunHkaiActivationCli.cmake")
set_tests_properties(NativeWorldHkaiActivationCli PROPERTIES TIMEOUT 180)
target_compile_options(Monstey_WorldRuntimeProbe PRIVATE /fp:strict)

# Conditional reconstruction of explicit cache helper calls; no Game history inference.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbCacheTrace.cpp)
set_source_files_properties(src/Monstey_GadbCacheTrace.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbCacheTraceTests tests/Monstey_GadbCacheTraceTests.cpp)
target_link_libraries(Monstey_GadbCacheTraceTests PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbCacheTraceTests PRIVATE /fp:strict)
add_test(NAME NativeGadbCacheTrace COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbCacheTraceTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-cache-trace"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshReport.cmake")
set_tests_properties(NativeGadbCacheTrace PROPERTIES TIMEOUT 300)
# The reviewed Trace CPP/free declaration must already be present in Monstey_Data.
# If applying the core and Runtime together, add src/Monstey_GadbCacheTrace.cpp
# exactly once through target_sources(Monstey_Data PRIVATE ...).
add_executable(Monstey_GadbRuntimeCacheTraceTests tests/Monstey_GadbRuntimeCacheTraceTests.cpp)
target_link_libraries(Monstey_GadbRuntimeCacheTraceTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_GadbRuntimeCacheTraceTests PRIVATE /fp:strict)
add_test(NAME NativeGadbRuntimeCacheTrace COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbRuntimeCacheTraceTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/runtime-cache-trace"
  "-DBASELINE_CATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/catalog-222-before-cache-trace.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunGadbRuntimeCacheTrace.cmake")
set_tests_properties(NativeGadbRuntimeCacheTrace PROPERTIES TIMEOUT 180)


# Native evidence is optional and stays outside the repository.
set(MONSTEY_PUBLISHED_RECEIVER_NATIVE_PROOF "" CACHE FILEPATH "Optional private native evidence JSON for published receiver tests")
add_executable(Monstey_WorldPublishedReceiverTests tests/Monstey_WorldPublishedReceiverTests.cpp)
target_link_libraries(Monstey_WorldPublishedReceiverTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_WorldPublishedReceiverTests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_test(NAME NativeWorldPublishedReceiver COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldPublishedReceiverTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/published-receiver"
  "-DNATIVE_PROOF=${MONSTEY_PUBLISHED_RECEIVER_NATIVE_PROOF}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunPublishedReceiver.cmake")
set_tests_properties(NativeWorldPublishedReceiver PROPERTIES TIMEOUT 300)
# No Tool or live adapter integration is proposed by this candidate.

# Existing Monstey_WorldRuntimeProbe target keeps its current source/links.
# Copy the reviewed observer Probe source/header and this additive Tool source.
add_test(NAME NativePublishedReceiverCli COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_WorldRuntimeProbe>"
  "-DPOWERSHELL=${MONSTEY_POWERSHELL}"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/published-receiver-cli"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunPublishedReceiverCli.cmake")
set_tests_properties(NativePublishedReceiverCli PROPERTIES TIMEOUT 180)
# Optional -DLEGACY_EXE=<pinned baseline CLI> enables all25 byte comparisons.
# It is not required for the portable message/guard checks or installed product.


# Add only after the existing WorldRuntime target; the Probe CPP is already compiled.
set(MONSTEY_RECEIVER_ROWS_NATIVE_PROOF "" CACHE FILEPATH "Optional private native evidence JSON for receiver row tests")
add_executable(Monstey_WorldReceiverRowsTests tests/Monstey_WorldReceiverRowsTests.cpp)
target_link_libraries(Monstey_WorldReceiverRowsTests PRIVATE Monstey_WorldRuntime)
target_compile_options(Monstey_WorldReceiverRowsTests PRIVATE /fp:strict)
add_test(NAME NativeWorldReceiverRows COMMAND "${CMAKE_COMMAND}"
 "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldReceiverRowsTests>"
 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/receiver-rows"
 "-DNATIVE_PROOF=${MONSTEY_RECEIVER_ROWS_NATIVE_PROOF}"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunReceiverRows.cmake")
add_test(NAME NativeWorldReceiverRowsCli COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_WorldRuntimeProbe>"
 "-DPOWERSHELL=${MONSTEY_POWERSHELL}"
 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/receiver-rows-cli"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunReceiverRowsCli.cmake")
set_tests_properties(NativeWorldReceiverRows NativeWorldReceiverRowsCli PROPERTIES TIMEOUT 180)


# Shared dark palette and native-control painting; data/CLI targets are unchanged.
target_link_libraries(Monstey_CurveControlTests PRIVATE Monstey_Theme)
target_link_libraries(Monstey_ModelPreviewWindowTests PRIVATE Monstey_Theme)
if(TARGET Monstey_AnimatedPreviewWindowTests)
  target_link_libraries(Monstey_AnimatedPreviewWindowTests PRIVATE Monstey_Theme)
endif()
target_link_libraries(Monstey_TextureWindow PUBLIC Monstey_Theme)
add_executable(Monstey_ThemeTests tests/Monstey_ThemeTests.cpp)
target_link_libraries(Monstey_ThemeTests PRIVATE Monstey_Theme)
add_test(NAME NativeTheme COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_ThemeTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/dark-theme"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFreshTheme.cmake")
set_tests_properties(NativeTheme PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
# Qualified principal-navigation arithmetic; owned inputs, no engine calls.
target_sources(Monstey_Data PRIVATE src/Monstey_NavigationBounds.cpp src/Monstey_StaticNavFaceBounds.cpp)
set_source_files_properties(src/Monstey_NavigationBounds.cpp src/Monstey_StaticNavFaceBounds.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_NavigationBoundsTests tests/Monstey_NavigationBoundsTests.cpp)
target_link_libraries(Monstey_NavigationBoundsTests PRIVATE Monstey_Data)
target_compile_options(Monstey_NavigationBoundsTests PRIVATE /fp:strict)
add_test(NAME NativeNavigationBounds COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_NavigationBoundsTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/navigation-bounds"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavigationBounds.cmake")
add_executable(Monstey_StaticNavFaceBoundsTests tests/Monstey_StaticNavFaceBoundsTests.cpp)
target_link_libraries(Monstey_StaticNavFaceBoundsTests PRIVATE Monstey_Data)
target_compile_options(Monstey_StaticNavFaceBoundsTests PRIVATE /fp:strict)
add_test(NAME NativeStaticNavFaceBounds COMMAND Monstey_StaticNavFaceBoundsTests)
add_executable(Monstey_NavAuthorCompatibilityTests tests/Monstey_NavAuthorCompatibilityTests.cpp)
target_link_libraries(Monstey_NavAuthorCompatibilityTests PRIVATE Monstey_Data)
target_compile_options(Monstey_NavAuthorCompatibilityTests PRIVATE /fp:strict)
add_test(NAME NativeNavAuthorCompatibility COMMAND Monstey_NavAuthorCompatibilityTests candidate)
# Qualified owned navigation boundary and UID/cutter staging components.
# Their paired native evidence is external; default tests contain no game data.
target_sources(Monstey_Data PRIVATE src/Monstey_NavigationBoundaryQuery.cpp src/Monstey_NavigationStreaming.cpp)
set_source_files_properties(src/Monstey_NavigationBoundaryQuery.cpp src/Monstey_NavigationStreaming.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(pair IN ITEMS "NavigationBoundaryQuery|Monstey_NavigationBoundaryQueryTests" "NavigationBoundaryIndependent|Monstey_BoundaryIndependentReview" "NavigationStreaming|Monstey_NavigationStreamingTests" "NavigationStreamingIndependent|Monstey_NavigationStreamingIndependentTests")
  string(REPLACE "|" ";" parts "${pair}")
  list(GET parts 0 name)
  list(GET parts 1 target)
  add_executable(${target} tests/${target}.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Data)
  target_compile_options(${target} PRIVATE /fp:strict)
  set(extra)
  if(name STREQUAL "NavigationBoundaryIndependent")
    list(APPEND extra "-DFIRST_ARGUMENT=--synthetic")
  endif()
  add_test(NAME Native${name} COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:${target}>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/${name}"
    ${extra} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQualifiedNativeReport.cmake")
  set_tests_properties(Native${name} PROPERTIES TIMEOUT 90)
endforeach()
# Self-contained, fixed-profile native class-bank recipes. No private map shipped.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbBankRegistration.cpp)
add_executable(Monstey_GadbBankRegistrationTool src/Monstey_GadbBankRegistrationTool.cpp)
target_link_libraries(Monstey_GadbBankRegistrationTool PRIVATE Monstey_Data)
add_executable(Monstey_GadbBankLiteralTests tests/Monstey_GadbBankLiteralTests.cpp)
target_link_libraries(Monstey_GadbBankLiteralTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbBankLiterals COMMAND Monstey_GadbBankLiteralTests)
# Complete qualified NULL-policy seam matcher: ground, alternate and mixed flags.
set(seam_sources src/Monstey_NavSeamBroadphase.cpp src/Monstey_NavSeamGeometry.cpp src/Monstey_NavSeamGround.cpp src/Monstey_NavSeamAlternate.cpp src/Monstey_NavSeamMatcher.cpp)
target_sources(Monstey_Data PRIVATE ${seam_sources})
set_source_files_properties(${seam_sources} PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(target IN ITEMS Monstey_NavSeamBroadphaseTests Monstey_NavSeamGeometryTests Monstey_NavSeamGroundTests Monstey_NavSeamAlternateTests Monstey_NavSeamMatcherTests Monstey_NavSeamAllRoutesTests Monstey_NavSeamFaceNormalReferenceTests Monstey_GroundSeamIndependentTests Monstey_NavSeamFullIndependentTests)
  add_executable(${target} tests/${target}.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Data)
  target_compile_options(${target} PRIVATE /fp:strict)
  string(REGEX REPLACE "^Monstey_" "Native" name "${target}")
  string(REGEX REPLACE "Tests$" "" name "${name}")
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:${target}>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/${name}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQualifiedNativeReport.cmake")
  set_tests_properties(${name} PROPERTIES TIMEOUT 90)
endforeach()
# Bounded read-only bank inspection shares the existing native runtime/transports.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbBankApi.cpp)
add_executable(Monstey_GadbBankApiTests tests/Monstey_GadbBankApiTests.cpp)
target_link_libraries(Monstey_GadbBankApiTests PRIVATE Monstey_Runtime)
add_test(NAME NativeGadbBankApi COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBankApiTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-bank-api"
  "-DBASELINE_CATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/catalog223-before-native-banks.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbBankApi.cmake")
set_tests_properties(NativeGadbBankApi PROPERTIES TIMEOUT 150)
# Qualified successful owned UID-to-slot set registration and reverse unmapping.
target_sources(Monstey_Data PRIVATE src/Monstey_NavigationRegistration.cpp)
set_source_files_properties(src/Monstey_NavigationRegistration.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(target IN ITEMS Monstey_NavigationRegistrationTests Monstey_NavigationRegistrationIndependentTests)
  add_executable(${target} tests/${target}.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Data)
  string(REGEX REPLACE "^Monstey_" "Native" name "${target}")
  string(REGEX REPLACE "Tests$" "" name "${name}")
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:${target}>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/${name}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQualifiedNativeReport.cmake")
  set_tests_properties(${name} PROPERTIES TIMEOUT 90)
endforeach()

# Native compact-tree constructor and exact wrapped unused-face grammar.
# Public tests author synthetic inputs; the optional actual tool takes an explicit source.
set(cooker_sources
  src/Monstey_NavCompactCooker.cpp src/Monstey_NavCompactCookerStages.cpp
  src/Monstey_NavCompactOptimizerCore.cpp src/Monstey_NavCompactSortCentroids.cpp
  src/Monstey_NavFinalizeHierarchy.cpp)
target_sources(Monstey_Data PRIVATE ${cooker_sources})
set_source_files_properties(${cooker_sources} PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(target IN ITEMS Monstey_NavCompactCookerStageTests Monstey_NavCompactOptimizerCoreTests Monstey_NavCompactCookerIntegrationTests Monstey_NavCompactStageIndependentTests Monstey_NavCompactCoreIndependentTests Monstey_NavCompactSortCentroidsTests Monstey_NavFinalizeHierarchyTests Monstey_UnusedFaceFidelityTests Monstey_UnusedFaceIndependentTests)
  add_executable(${target} tests/${target}.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Data)
  target_compile_options(${target} PRIVATE /fp:strict)
  string(REGEX REPLACE "^Monstey_" "Native" name "${target}")
  string(REGEX REPLACE "Tests$" "" name "${name}")
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:${target}>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/${name}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQualifiedNativeReport.cmake")
  set_tests_properties(${name} PROPERTIES TIMEOUT 90)
endforeach()
add_executable(Monstey_NavCompactCookerActual tests/Monstey_NavCompactCookerActual.cpp)
target_link_libraries(Monstey_NavCompactCookerActual PRIVATE Monstey_Data)
target_compile_options(Monstey_NavCompactCookerActual PRIVATE /fp:strict)


# Earlier owned C++ implementations are test-only regression controls.
# They are never linked into the Workbench, Runtime or Data libraries.
add_library(Monstey_NavUnusedFacePreimages STATIC
  tests/fixtures/navigation-cooker-preimages/Monstey_NavCompactOptimizerCoreBaseline.cpp
  tests/fixtures/navigation-cooker-preimages/Monstey_NavFinalizeHierarchyBaseline.cpp
  tests/fixtures/navigation-cooker-preimages/Monstey_StaticNavFaceBoundsBaseline.cpp)
target_link_libraries(Monstey_NavUnusedFacePreimages PRIVATE Monstey_Data)
target_compile_options(Monstey_NavUnusedFacePreimages PRIVATE /fp:strict)
target_link_libraries(Monstey_UnusedFaceFidelityTests PRIVATE Monstey_NavUnusedFacePreimages)
target_link_libraries(Monstey_UnusedFaceIndependentTests PRIVATE Monstey_NavUnusedFacePreimages)


# Bounded read-only adapter over the qualified native compact-tree constructor.
target_sources(Monstey_Data PRIVATE src/Monstey_NavStaticTreeApi.cpp)
set_source_files_properties(src/Monstey_NavStaticTreeApi.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_NavStaticTreeApiTests tests/Monstey_NavStaticTreeApiTests.cpp)
target_link_libraries(Monstey_NavStaticTreeApiTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_NavStaticTreeApiTests PRIVATE /fp:strict)
add_test(NAME NativeNavStaticTreeApi COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_NavStaticTreeApiTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/static-tree-api"
  "-DBASELINE_CATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static-tree-api/catalog224.json"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavStaticTreeApi.cmake")
set_tests_properties(NativeNavStaticTreeApi PROPERTIES TIMEOUT 150)
# Keep world40.json beside catalog224.json. These are public operation schemas,
# not game assets. The default test creates all HKAI bytes synthetically.
add_test(NAME NativeNavStaticTreeGui COMMAND "${MONSTEY_POWERSHELL}"
  -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavStaticTreeGui.ps1"
  -Gui "$<TARGET_FILE:Monstey_Workbench>"
  -TestExecutable "$<TARGET_FILE:Monstey_NavStaticTreeApiTests>"
  -BaselineCatalog "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static-tree-api/catalog224.json"
  -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/tests/static-tree-gui")
set_tests_properties(NativeNavStaticTreeGui PROPERTIES TIMEOUT 150 LABELS native-gui)


foreach(target IN ITEMS Monstey_NavCookerApiIndependentTests Monstey_UnusedFaceApiRebindTests)
  add_executable(${target} tests/${target}.cpp tests/Monstey_ReviewNativeNavFixture.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Runtime)
  target_compile_options(${target} PRIVATE /fp:strict)
endforeach()
add_test(NAME NativeNavStaticTreeIndependent COMMAND "${MONSTEY_POWERSHELL}"
  -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavStaticTreeIndependent.ps1"
  -TestExecutable "$<TARGET_FILE:Monstey_NavCookerApiIndependentTests>"
  -DomainExecutable "$<TARGET_FILE:Monstey_UnusedFaceApiRebindTests>"
  -BaselineCatalog "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static-tree-api/independent-baseline224-world40.json"
  -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/tests/static-tree-independent")
set_tests_properties(NativeNavStaticTreeIndependent PROPERTIES TIMEOUT 150)

# Independently qualified advanced GADB components. Original engine code is never called.
target_sources(Monstey_Data PRIVATE
  src/Monstey_GadbBinderComposition.cpp src/Monstey_GadbCachedValue.cpp
  src/Monstey_GadbClassDeclaration.cpp)
set_source_files_properties(src/Monstey_GadbBinderComposition.cpp
  src/Monstey_GadbCachedValue.cpp src/Monstey_GadbClassDeclaration.cpp
  PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(target IN ITEMS Monstey_GadbBinderCompositionTests Monstey_GadbBinderIndependentTests
    Monstey_GadbCachedValueTests Monstey_GadbCachedValueIndependentTests
    Monstey_GadbClassDeclarationTests Monstey_GadbClassRowLimitTests
    Monstey_ClassDeclarationIndependentTests)
  add_executable(${target} tests/${target}.cpp)
  target_link_libraries(${target} PRIVATE Monstey_Data)
  target_compile_options(${target} PRIVATE /fp:strict)
endforeach()
add_test(NAME NativeGadbBinderComposition COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBinderCompositionTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-binder"
  -DGROUPS=10 -DCASES=9395 -DCHECKS=17316
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunBinderComposition.cmake")
add_test(NAME NativeGadbBinderIndependent COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBinderIndependentTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-binder-independent"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbBinderIndependent.cmake")
add_test(NAME NativeGadbCachedValue COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_GadbCachedValueTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-cached-value"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunQualifiedNativeReport.cmake")
add_test(NAME NativeGadbCachedValueIndependent COMMAND "${CMAKE_COMMAND}"
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbCachedValueIndependentTests>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/gadb-cached-independent"
  -DGROUPS=3 -DCASES=490266 -DCHECKS=1020830
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunIndependentCachedValue.cmake")
foreach(pair IN ITEMS "GadbClassDeclaration;RunClassDeclaration" "GadbClassRowLimit;RunClassRowLimit" "ClassDeclarationIndependent;RunClassDeclarationIndependent")
  list(GET pair 0 stem)
  list(GET pair 1 runner)
  add_test(NAME Native${stem} COMMAND "${CMAKE_COMMAND}"
    "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_${stem}Tests>"
    "-DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/tests/${stem}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/${runner}.cmake")
endforeach()
set_tests_properties(NativeGadbBinderComposition NativeGadbBinderIndependent
  NativeGadbCachedValue NativeGadbCachedValueIndependent NativeGadbClassDeclaration
  NativeGadbClassRowLimit NativeClassDeclarationIndependent PROPERTIES TIMEOUT 150)
add_executable(Monstey_GadbConditionalBuffTool src/Monstey_GadbConditionalBuffTool.cpp)
target_link_libraries(Monstey_GadbConditionalBuffTool PRIVATE Monstey_Data)
add_executable(Monstey_GadbClass30Tool src/Monstey_GadbClass30Tool.cpp)
target_link_libraries(Monstey_GadbClass30Tool PRIVATE Monstey_Data)
# Apply only after the corrected reader and this API pass independent review.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbClassDeclarationApi.cpp)
add_executable(Monstey_GadbNativeClassApiTests tests/Monstey_GadbClassApiTests.cpp)
target_link_libraries(Monstey_GadbNativeClassApiTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_GadbNativeClassApiTests PRIVATE /fp:strict)
add_test(NAME NativeGadbNativeClassApi COMMAND "${CMAKE_COMMAND}"
 "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_GadbNativeClassApiTests>"
 "-DBASELINE_CATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/gadb-native-class/catalog225.json"
 "-DREPORT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/native-class-api-proof"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNativeClassApi.cmake")
set_tests_properties(NativeGadbNativeClassApi PROPERTIES TIMEOUT 60)

set_source_files_properties(src/Monstey_GadbClassDeclarationApi.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbClassApiIndependentTests tests/Monstey_GadbClassApiIndependentTests.cpp)
target_link_libraries(Monstey_GadbClassApiIndependentTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_GadbClassApiIndependentTests PRIVATE /fp:strict)
add_test(NAME NativeGadbNativeClassApiIndependent COMMAND "${CMAKE_COMMAND}"
 "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbClassApiIndependentTests>"
 "-DREPORT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/native-class-api-independent"
 "-DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/gadb-native-class/catalog225.json"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunIndependentClassApi.cmake")
set_tests_properties(NativeGadbNativeClassApiIndependent PROPERTIES TIMEOUT 150)
# The development executables need the same runtime schemas as an installed package.
# CMP0112 is NEW under this project's minimum CMake version; TARGET_FILE_DIR does
# not add a reverse target dependency, so this shared copy target has no cycle.
add_custom_target(Monstey_RuntimeSchemas
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Monstey_Workbench>/schema"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/schema/gadb-schema.json"
    "$<TARGET_FILE_DIR:Monstey_Workbench>/schema/gadb-schema.json"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/schema/fx-schema.json"
    "$<TARGET_FILE_DIR:Monstey_Workbench>/schema/fx-schema.json"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:Monstey_WorkbenchCli>/schema"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/schema/gadb-schema.json"
    "$<TARGET_FILE_DIR:Monstey_WorkbenchCli>/schema/gadb-schema.json"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/schema/fx-schema.json"
    "$<TARGET_FILE_DIR:Monstey_WorkbenchCli>/schema/fx-schema.json"
  VERBATIM)
add_dependencies(Monstey_Workbench Monstey_RuntimeSchemas)
add_dependencies(Monstey_WorkbenchCli Monstey_RuntimeSchemas)
# Independent regression links the same application implementations as the GUI.
add_executable(Monstey_NativeUiIndependentTests
 tests/Monstey_NativeUiIndependent.cpp
 src/Monstey_CurveControl.cpp src/Monstey_ModelPreviewWindow.cpp)
target_include_directories(Monstey_NativeUiIndependentTests PRIVATE include)
target_link_libraries(Monstey_NativeUiIndependentTests PRIVATE
 Monstey_Theme Monstey_Runtime Monstey_TextureWindow
 user32 gdi32 comctl32 comdlg32 shell32 ole32)
target_compile_options(Monstey_NativeUiIndependentTests PRIVATE /fp:strict)
add_test(NAME NativeGadbUiIndependent COMMAND "${CMAKE_COMMAND}"
 "-DTEST_EXE=$<TARGET_FILE:Monstey_NativeUiIndependentTests>"
 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/native-ui-independent"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNativeUiIndependent.cmake")
set_tests_properties(NativeGadbUiIndependent PROPERTIES TIMEOUT 60)


# Synthetic human-facing native database workflow and independent window regression.
add_test(NAME Monstey_NativeDatabaseGui COMMAND "${CMAKE_COMMAND}"
 "-DGUI=$<TARGET_FILE:Monstey_Workbench>"
 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/native-database-gui"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNativeDatabaseGui.cmake")
set_tests_properties(Monstey_NativeDatabaseGui PROPERTIES TIMEOUT 90 RUN_SERIAL TRUE LABELS native-gui)
set_tests_properties(NativeGadbUiIndependent PROPERTIES RUN_SERIAL TRUE LABELS native-gui)


# Independently qualified supplied-memory GADB getter components.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbNamePointerBoundary.cpp
 src/Monstey_GadbRecordLookup.cpp src/Monstey_GadbScalarFloat.cpp
 src/Monstey_GadbStringPointer.cpp src/Monstey_GadbRemainingContracts.cpp)
set_source_files_properties(src/Monstey_GadbNamePointerBoundary.cpp
 src/Monstey_GadbRecordLookup.cpp src/Monstey_GadbScalarFloat.cpp
 src/Monstey_GadbStringPointer.cpp src/Monstey_GadbRemainingContracts.cpp
 PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbNamePointerBoundaryTests
 tests/Monstey_GadbNamePointerBoundaryTests.cpp
 tests/fixtures/name-pointer-predecessor/Monstey_GadbBankNameSelection.cpp)
target_include_directories(Monstey_GadbNamePointerBoundaryTests PRIVATE tests/fixtures/name-pointer-predecessor)
target_link_libraries(Monstey_GadbNamePointerBoundaryTests PRIVATE Monstey_Data)
foreach(pair IN ITEMS "NamePointerIndependent;Monstey_NamePointerIndependent"
 "GadbScalarFloat;Monstey_GadbScalarFloatTests"
 "GadbRecordLookup;Monstey_GadbRecordLookupTests"
 "GadbStringPointer;Monstey_GadbStringPointerTests"
 "GadbRemainingContracts;Monstey_GadbRemainingContractsTests")
 list(GET pair 0 stem)
 list(GET pair 1 name)
 if(stem STREQUAL "NamePointerIndependent")
  set(source tests/Monstey_NamePointerIndependent.cpp)
  set(target Monstey_NamePointerIndependent)
 else()
  set(source "tests/${name}.cpp")
  set(target "${name}")
 endif()
 add_executable(${target} "${source}")
 target_link_libraries(${target} PRIVATE Monstey_Data)
 target_compile_options(${target} PRIVATE /fp:strict)
endforeach()
target_compile_options(Monstey_GadbNamePointerBoundaryTests PRIVATE /fp:strict)
add_test(NAME NativeGadbNamePointerBoundary COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbNamePointerBoundaryTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/name-pointer" -DMODE=JSON
 -DGROUPS=14 -DCASES=16452 -DCHECKS=245278
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbGetterProbe.cmake")
add_test(NAME NativeGadbNamePointerIndependent COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_NamePointerIndependent>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/name-pointer-independent" -DMODE=TEXT
 "-DEXPECTED_TEXT=PASS independent_pointer groups=4 cases=6958 checks=36317"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbGetterProbe.cmake")
add_test(NAME NativeGadbScalarFloat COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbScalarFloatTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/scalar-float" -DMODE=TEXT
 "-DEXPECTED_TEXT=PASS scalar_float groups=10 cases=192304 checks=274348"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbGetterProbe.cmake")
foreach(pair IN ITEMS "GadbRecordLookup;3;68884" "GadbStringPointer;4;308")
 list(GET pair 0 stem)
 list(GET pair 1 groups)
 list(GET pair 2 checks)
 add_test(NAME Native${stem} COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_${stem}Tests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/runtime-getters" "-DGROUPS=${groups}" "-DCHECKS=${checks}"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGetter.cmake")
endforeach()
add_test(NAME NativeGadbRemainingContracts COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbRemainingContractsTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/remaining-contracts" -DMODE=FILE
 -DGROUPS=8 -DCASES=67865 -DCHECKS=133403 -DCHECK_KEY=assertions
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbGetterProbe.cmake")
set_tests_properties(NativeGadbNamePointerBoundary NativeGadbNamePointerIndependent
 NativeGadbScalarFloat NativeGadbRecordLookup NativeGadbStringPointer
 NativeGadbRemainingContracts PROPERTIES TIMEOUT 90)
add_executable(Monstey_GadbRemainingContractsTool src/Monstey_GadbRemainingContractsTool.cpp)
target_link_libraries(Monstey_GadbRemainingContractsTool PRIVATE Monstey_Data)
add_executable(Monstey_ActualDeclarationLookupTests src/Monstey_ActualDeclarationLookupTests.cpp)
target_link_libraries(Monstey_ActualDeclarationLookupTests PRIVATE Monstey_Data)

add_executable(Monstey_GadbScalarIndependentTests
    tests/Monstey_GadbScalarIndependentTests.cpp)
target_link_libraries(Monstey_GadbScalarIndependentTests PRIVATE Monstey_Data)
if(MSVC)
    target_compile_options(Monstey_GadbScalarIndependentTests PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endif()
add_test(NAME NativeGadbScalarIndependent
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbScalarIndependentTests>"
        "-DOUTPUT_ROOT=${CMAKE_BINARY_DIR}/tests/gadb-scalar-independent"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbScalarIndependent.cmake")
set_tests_properties(NativeGadbScalarIndependent PROPERTIES TIMEOUT 90)

add_executable(Monstey_GadbLookupStringIndependent tests/Monstey_GadbLookupStringIndependent.cpp)
target_link_libraries(Monstey_GadbLookupStringIndependent PRIVATE Monstey_Data Monstey_Core)
if(MSVC)
 target_compile_options(Monstey_GadbLookupStringIndependent PRIVATE /W4 /WX /fp:strict /utf-8 /permissive-)
endif()
add_test(NAME Monstey_GadbLookupStringIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbLookupStringIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/gadb-lookup-string-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunLookupStringIndependent.cmake")
set_tests_properties(Monstey_GadbLookupStringIndependent PROPERTIES TIMEOUT 75)


# Product source Monstey_GadbRemainingContracts.cpp belongs in existing Data.
add_executable(Monstey_GadbRemainingContractsIndependentTests
  tests/Monstey_RemainingIndependent.cpp)
target_link_libraries(Monstey_GadbRemainingContractsIndependentTests PRIVATE Monstey_Data Monstey_Core)
if(MSVC)
  target_compile_options(Monstey_GadbRemainingContractsIndependentTests PRIVATE /W4 /WX /fp:strict /utf-8 /permissive-)
endif()
add_test(NAME Monstey_GadbRemainingContractsIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DEXE=$<TARGET_FILE:Monstey_GadbRemainingContractsIndependentTests>"
    "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/tests/remaining-count-independent"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunRemainingIndependent.cmake")
set_tests_properties(Monstey_GadbRemainingContractsIndependent PROPERTIES TIMEOUT 60)



# Independently qualified complete native GADB count consumers.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbReferenceWord.cpp)
add_executable(Monstey_GadbReferenceWordTests tests/Monstey_GadbReferenceWordTests.cpp)
target_link_libraries(Monstey_GadbReferenceWordTests PRIVATE Monstey_Data)
add_test(NAME NativeGadbReferenceWord COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbReferenceWordTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/reference-word-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunReferenceWord.cmake")
set_tests_properties(NativeGadbReferenceWord PROPERTIES TIMEOUT 60)

add_executable(Monstey_ReferenceIndependent tests/Monstey_ReferenceIndependent.cpp)
target_include_directories(Monstey_ReferenceIndependent PRIVATE include)
target_compile_options(Monstey_ReferenceIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
target_link_libraries(Monstey_ReferenceIndependent PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.ReferenceGetterIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_ReferenceIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunReferenceIndependent.cmake")
set_tests_properties(Monstey.ReferenceGetterIndependent PROPERTIES TIMEOUT 120)


# Native Data library addition; use existing project include paths.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbAudioNextKey.cpp)
add_executable(Monstey_GadbAudioNextKeyTests tests/Monstey_GadbAudioNextKeyTests.cpp)
target_include_directories(Monstey_GadbAudioNextKeyTests PRIVATE include)
target_compile_options(Monstey_GadbAudioNextKeyTests PRIVATE /W4 /WX /O2 /fp:strict /EHsc /permissive- /utf-8)
target_link_libraries(Monstey_GadbAudioNextKeyTests PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.GadbAudioNextKey COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbAudioNextKeyTests>" "-DREPORT_DIR=${CMAKE_CURRENT_BINARY_DIR}/reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunAudioNextKey.cmake")
set_tests_properties(Monstey.GadbAudioNextKey PROPERTIES TIMEOUT 120)


add_executable(Monstey_GadbNextKeyIndependent tests/Monstey_NextKeyIndependent.cpp)
target_link_libraries(Monstey_GadbNextKeyIndependent PRIVATE Monstey_Data)
add_test(NAME Monstey_GadbNextKeyIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbNextKeyIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/nextkey-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNextKeyIndependent.cmake")
set_tests_properties(Monstey_GadbNextKeyIndependent PROPERTIES TIMEOUT 60)


# Integrate the source into the existing owned Data library.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbAudioActionWord.cpp)
add_executable(Monstey_GadbAudioActionWordTests tests/Monstey_AudioActionWordTests.cpp)
target_link_libraries(Monstey_GadbAudioActionWordTests PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeGadbAudioActionWord COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbAudioActionWordTests>" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/audio-action-test-output" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunAudioActionWord.cmake")
# Optional command-line source inspection tool, never a default actual-data test.
# add_executable(Monstey_AudioCountInspect src/Monstey_AudioCountInspect.cpp)
# target_link_libraries(Monstey_AudioCountInspect PRIVATE Monstey_Data Monstey_Core)


# Independent synthetic audio accessor regression; no PE/GADB input.
add_executable(Monstey_AudioIndependentTests tests/Monstey_AudioIndependentTests.cpp)
target_link_libraries(Monstey_AudioIndependentTests PRIVATE Monstey_Data Monstey_Core bcrypt)
if(MSVC)
 set_property(TARGET Monstey_AudioIndependentTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_AudioIndependentTests PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
endif()
add_test(NAME NativeGadbAudioActionIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_AudioIndependentTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/gadb-audio-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunAudioIndependent.cmake")


target_sources(Monstey_Data PRIVATE src/Monstey_GadbSelectedCountWord.cpp)
add_executable(Monstey_GadbSelectedCountWordTests tests/Monstey_SelectedCountWordTests.cpp)
target_link_libraries(Monstey_GadbSelectedCountWordTests PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeGadbSelectedCountWord COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbSelectedCountWordTests>" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/selected-count-test-output" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSelectedCountWord.cmake")
# Optional C++ source-read tool; never a default actual-data test:
# add_executable(Monstey_SelectedCountInspect src/Monstey_SelectedCountInspect.cpp)
# target_link_libraries(Monstey_SelectedCountInspect PRIVATE Monstey_Data Monstey_Core)


add_executable(Monstey_SelectedCountIndependent tests/Monstey_SelectedCountIndependent.cpp)
target_include_directories(Monstey_SelectedCountIndependent PRIVATE include)
target_compile_options(Monstey_SelectedCountIndependent PRIVATE /O2 /W4 /WX /EHsc /fp:strict /permissive- /utf-8)
target_link_libraries(Monstey_SelectedCountIndependent PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.SelectedCountIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_SelectedCountIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSelectedCountIndependent.cmake")
set_tests_properties(Monstey.SelectedCountIndependent PROPERTIES TIMEOUT 120)


add_executable(Monstey_AudioCountInspect src/Monstey_AudioCountInspect.cpp)
target_link_libraries(Monstey_AudioCountInspect PRIVATE Monstey_Data)
add_executable(Monstey_SelectedCountInspect src/Monstey_SelectedCountInspect.cpp)
target_link_libraries(Monstey_SelectedCountInspect PRIVATE Monstey_Data)
set_source_files_properties(
 src/Monstey_GadbReferenceWord.cpp src/Monstey_GadbAudioNextKey.cpp
 src/Monstey_GadbAudioActionWord.cpp src/Monstey_GadbSelectedCountWord.cpp
 PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(MONSTEY_COUNT_TARGET IN ITEMS
 Monstey_GadbReferenceWordTests Monstey_ReferenceIndependent
 Monstey_GadbAudioNextKeyTests Monstey_GadbNextKeyIndependent
 Monstey_GadbAudioActionWordTests Monstey_AudioIndependentTests
 Monstey_GadbSelectedCountWordTests Monstey_SelectedCountIndependent
 Monstey_AudioCountInspect Monstey_SelectedCountInspect)
 target_compile_options(${MONSTEY_COUNT_TARGET} PRIVATE /W4 /WX /fp:strict /permissive- /EHsc /utf-8)
endforeach()

# Independently qualified native bank, integer and original-reader families.

target_sources(Monstey_Data PRIVATE src/Monstey_GadbIntegerWord.cpp)
set_source_files_properties(src/Monstey_GadbIntegerWord.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbIntegerWordTests tests/Monstey_GadbIntegerWordTests.cpp)
target_link_libraries(Monstey_GadbIntegerWordTests PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbIntegerWordTests PRIVATE /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbIntegerWord COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbIntegerWordTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/integer-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunInteger.cmake")
set_tests_properties(NativeGadbIntegerWord PROPERTIES TIMEOUT 90)


add_executable(Monstey_IntegerIndependent tests/Monstey_IntegerIndependent.cpp)
target_include_directories(Monstey_IntegerIndependent PRIVATE include)
target_compile_options(Monstey_IntegerIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
target_link_libraries(Monstey_IntegerIndependent PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.IntegerWordIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_IntegerIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/integer-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunIntegerIndependent.cmake")
set_tests_properties(Monstey.IntegerWordIndependent PROPERTIES TIMEOUT 90)


# Existing Data library; synthetic fixture test only, no original SDK/database input.
target_sources(Monstey_Data PRIVATE src/Monstey_FearDbPrimitives.cpp)
set_source_files_properties(src/Monstey_FearDbPrimitives.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_FearDbPrimitiveTests tests/Monstey_FearDbPrimitiveTests.cpp)
target_link_libraries(Monstey_FearDbPrimitiveTests PRIVATE Monstey_Data Monstey_Core bcrypt)
if(MSVC)
 set_property(TARGET Monstey_FearDbPrimitiveTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_FearDbPrimitiveTests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
endif()
add_test(NAME NativeFearDbPrimitives COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearDbPrimitiveTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-primitive-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbPrimitives.cmake")
set_tests_properties(NativeFearDbPrimitives PROPERTIES TIMEOUT 120)


add_executable(Monstey_FearReaderIndependent tests/Monstey_FearReaderIndependent.cpp)
target_link_libraries(Monstey_FearReaderIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_FearReaderIndependent PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_test(NAME NativeFearReaderIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearReaderIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-reader-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearReaderIndependent.cmake")
set_tests_properties(NativeFearReaderIndependent PROPERTIES TIMEOUT 90)

target_sources(Monstey_Data PRIVATE src/Monstey_GadbBankSynchronization.cpp src/Monstey_GadbBankActivation.cpp)
add_executable(Monstey_GadbBankActivationTests tests/Monstey_GadbBankActivationTests.cpp)
target_include_directories(Monstey_GadbBankActivationTests PRIVATE include)
target_compile_options(Monstey_GadbBankActivationTests PRIVATE /O2 /W4 /WX /EHsc /fp:strict /permissive- /utf-8)
target_link_libraries(Monstey_GadbBankActivationTests PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.GadbBankActivation COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbBankActivationTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunBankActivation.cmake")
set_tests_properties(Monstey.GadbBankActivation PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
add_executable(Monstey_GadbBankActivationInspect src/Monstey_GadbBankActivationInspect.cpp)
target_include_directories(Monstey_GadbBankActivationInspect PRIVATE include)
target_compile_options(Monstey_GadbBankActivationInspect PRIVATE /O2 /W4 /WX /EHsc /fp:strict /permissive- /utf-8)
target_link_libraries(Monstey_GadbBankActivationInspect PRIVATE Monstey_Data Monstey_Core bcrypt)


add_executable(Monstey_BankActivationIndependent tests/Monstey_BankActivationIndependent.cpp)
target_include_directories(Monstey_BankActivationIndependent PRIVATE include)
target_compile_options(Monstey_BankActivationIndependent PRIVATE /O2 /W4 /WX /EHsc /fp:strict /permissive- /utf-8)
target_link_libraries(Monstey_BankActivationIndependent PRIVATE Monstey_Data Monstey_Core bcrypt)
add_test(NAME Monstey.BankActivationIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_BankActivationIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunBankActivationIndependent.cmake")
set_tests_properties(Monstey.BankActivationIndependent PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)

set_source_files_properties(src/Monstey_GadbIntegerWord.cpp src/Monstey_FearDbPrimitives.cpp src/Monstey_GadbBankSynchronization.cpp src/Monstey_GadbBankActivation.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

# PROPOSAL ONLY. Append after existing main173 CMake content.

# Existing primitives are already in Monstey_Data and must not be added twice.

target_sources(Monstey_Data PRIVATE src/Monstey_FearDbPacked.cpp src/Monstey_FearDbResources.cpp src/Monstey_FearDbNames.cpp src/Monstey_FearDbOpen.cpp)

set_source_files_properties(src/Monstey_FearDbPacked.cpp src/Monstey_FearDbResources.cpp src/Monstey_FearDbNames.cpp src/Monstey_FearDbOpen.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

add_executable(Monstey_FearDbPackedTests tests/Monstey_FearDbPackedTests.cpp)

target_link_libraries(Monstey_FearDbPackedTests PRIVATE Monstey_Data)

target_compile_options(Monstey_FearDbPackedTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearDbPacked COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearDbPackedTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-packed-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbPacked.cmake")

set_tests_properties(NativeFearDbPacked PROPERTIES TIMEOUT 120)

add_executable(Monstey_PackedNameRescanTests tests/Monstey_PackedNameRescanTests.cpp)

target_link_libraries(Monstey_PackedNameRescanTests PRIVATE Monstey_Data)

target_compile_options(Monstey_PackedNameRescanTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearPackedNameRescan COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_PackedNameRescanTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-packed-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunPackedNameRescan.cmake")

set_tests_properties(NativeFearPackedNameRescan PROPERTIES TIMEOUT 120)

add_executable(Monstey_FearResourceTests tests/Monstey_FearDbResourceTests.cpp)

target_link_libraries(Monstey_FearResourceTests PRIVATE Monstey_Data)

target_compile_options(Monstey_FearResourceTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearDbResources COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearResourceTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-resources-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbResources.cmake")

set_tests_properties(NativeFearDbResources PROPERTIES TIMEOUT 120)

add_executable(Monstey_ResourcePackedAliasTests tests/Monstey_ResourcePackedAliasTests.cpp)

target_link_libraries(Monstey_ResourcePackedAliasTests PRIVATE Monstey_Data)

target_compile_options(Monstey_ResourcePackedAliasTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearResourcePackedAlias COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_ResourcePackedAliasTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-resources-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunResourcePackedAlias.cmake")

set_tests_properties(NativeFearResourcePackedAlias PROPERTIES TIMEOUT 120)

add_executable(Monstey_FearNamesTests tests/Monstey_FearDbNameTests.cpp)

target_link_libraries(Monstey_FearNamesTests PRIVATE Monstey_Data)

target_compile_options(Monstey_FearNamesTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearDbNames COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearNamesTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-names-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbNames.cmake")

set_tests_properties(NativeFearDbNames PROPERTIES TIMEOUT 120)

add_executable(Monstey_FearDbOpenTests tests/Monstey_FearDbOpenTests.cpp)

target_link_libraries(Monstey_FearDbOpenTests PRIVATE Monstey_Data)

target_compile_options(Monstey_FearDbOpenTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearDbOpen COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearDbOpenTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-open-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbOpen.cmake")

set_tests_properties(NativeFearDbOpen PROPERTIES TIMEOUT 120)

add_executable(Monstey_OpenPackedAliasTests tests/Monstey_OpenPackedAliasTests.cpp)

target_link_libraries(Monstey_OpenPackedAliasTests PRIVATE Monstey_Data)

target_compile_options(Monstey_OpenPackedAliasTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeFearOpenPackedAlias COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_OpenPackedAliasTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-open-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunOpenPackedAlias.cmake")

set_tests_properties(NativeFearOpenPackedAlias PROPERTIES TIMEOUT 120)



# Qualified independent packed fragment.

foreach(target Monstey_PackedEpochIndependent Monstey_FearPackedIndependent)

 add_executable(${target} tests/${target}.cpp)

 target_link_libraries(${target} PRIVATE Monstey_Data)

 target_compile_options(${target} PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

endforeach()

add_test(NAME Monstey.FearPackedEpochIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_PackedEpochIndependent>" "-DMODE=epoch" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/packed-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearPackedIndependent.cmake")

add_test(NAME Monstey.FearPackedLayoutIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearPackedIndependent>" "-DMODE=layout" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/packed-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearPackedIndependent.cmake")

set_tests_properties(Monstey.FearPackedEpochIndependent Monstey.FearPackedLayoutIndependent PROPERTIES TIMEOUT 120)





# Qualified independent resources fragment.

add_executable(Monstey_FearResourceIndependent tests/Monstey_FearResourceIndependent.cpp)

target_link_libraries(Monstey_FearResourceIndependent PRIVATE Monstey_Data)

target_compile_options(Monstey_FearResourceIndependent PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)

add_test(NAME NativeFearResourceIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearResourceIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-resource-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearResourceIndependent.cmake")

set_tests_properties(NativeFearResourceIndependent PROPERTIES TIMEOUT 120)



# Qualified independent names fragment.

add_executable(Monstey_FearNamesIndependent tests/Monstey_FearNamesIndependent.cpp)

target_link_libraries(Monstey_FearNamesIndependent PRIVATE Monstey_Data)

target_compile_options(Monstey_FearNamesIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME Monstey.FearNamesIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearNamesIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/names-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearNamesIndependent.cmake")

set_tests_properties(Monstey.FearNamesIndependent PROPERTIES TIMEOUT 120)





# Qualified independent open fragment.

add_executable(Monstey_FearOpenIndependent tests/Monstey_FearOpenIndependent.cpp)

target_link_libraries(Monstey_FearOpenIndependent PRIVATE Monstey_Data)

target_compile_options(Monstey_FearOpenIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME Monstey.FearOpenIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearOpenIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/open-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearOpenIndependent.cmake")

set_tests_properties(Monstey.FearOpenIndependent PROPERTIES TIMEOUT 120)





# Accepted CRT v002 DEFAULT ONLY: no original-byte/private proof targets.

# Proposed default integration only; not applied to the shared source.

target_sources(Monstey_Data PRIVATE src/Monstey_FearCrtComparison.cpp)

foreach(Monstey_CrtTest IN ITEMS Monstey_CrtComparisonTests Monstey_CrtRegionalTests Monstey_CrtMemoryContractTests)

 add_executable(${Monstey_CrtTest} tests/${Monstey_CrtTest}.cpp)

 target_link_libraries(${Monstey_CrtTest} PRIVATE Monstey_Data)

 if(MSVC)

  set_property(TARGET ${Monstey_CrtTest} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  target_compile_options(${Monstey_CrtTest} PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)

 endif()

 add_test(NAME ${Monstey_CrtTest} COMMAND ${Monstey_CrtTest})

endforeach()

# No original-byte interpreter, original file path, instruction literals, private

# asset, executable image or original program is required by this default fragment.





set_source_files_properties(src/Monstey_FearCrtComparison.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

add_executable(Monstey_CrtRootIndependent tests/Monstey_CrtRootIndependent.cpp)

target_link_libraries(Monstey_CrtRootIndependent PRIVATE Monstey_Data)

target_compile_options(Monstey_CrtRootIndependent PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)

add_test(NAME NativeFearCrtIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_CrtRootIndependent>" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunCrtRootIndependent.cmake")

set_tests_properties(NativeFearCrtIndependent PROPERTIES TIMEOUT 60)



# PROPOSAL ONLY: require independent fixup acceptance before appending.

target_sources(Monstey_Data PRIVATE src/Monstey_GadbValueFixup.cpp)

set_source_files_properties(src/Monstey_GadbValueFixup.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

add_executable(Monstey_GadbValueFixupTests tests/Monstey_GadbValueFixupTests.cpp)

target_link_libraries(Monstey_GadbValueFixupTests PRIVATE Monstey_Data)

target_compile_options(Monstey_GadbValueFixupTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)

add_test(NAME NativeGadbValueFixup COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbValueFixupTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/value-fixup-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbValueFixup.cmake")

set_tests_properties(NativeGadbValueFixup PROPERTIES TIMEOUT 120)



# Additional portable independent regression, after the accepted owner module.

add_executable(Monstey_GadbValueFixupIndependent tests/Monstey_GadbValueFixupIndependent.cpp)

target_link_libraries(Monstey_GadbValueFixupIndependent PRIVATE Monstey_Data)

if(MSVC)

 set_property(TARGET Monstey_GadbValueFixupIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

 target_compile_options(Monstey_GadbValueFixupIndependent PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)

endif()

add_test(NAME NativeGadbValueFixupIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbValueFixupIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/fixup-independent-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbValueFixupIndependent.cmake")

set_tests_properties(NativeGadbValueFixupIndependent PROPERTIES TIMEOUT 120)





# Proposal only. Accepted packed-v002/primitives are already owned by Monstey_Data.

# Do not add the standalone candidate CMakeLists.txt or duplicate reader sources.

target_sources(Monstey_Data PRIVATE src/Monstey_FearDbInspector.cpp)

set_source_files_properties(src/Monstey_FearDbInspector.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

add_executable(Monstey_FearDbInspector src/Monstey_FearDbInspectorMain.cpp)

target_link_libraries(Monstey_FearDbInspector PRIVATE Monstey_Data)



add_executable(Monstey_FearInspectorIndependent tests/Monstey_FearInspectorIndependent.cpp)

target_link_libraries(Monstey_FearInspectorIndependent PRIVATE Monstey_Core bcrypt)

if(MSVC)

 foreach(target Monstey_FearDbInspector Monstey_FearInspectorIndependent)

  set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  target_compile_options(${target} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)

 endforeach()

endif()

add_test(NAME NativeFearDbInspectorIndependent COMMAND "${CMAKE_COMMAND}"

 "-DEXE=$<TARGET_FILE:Monstey_FearInspectorIndependent>"

 "-DCLI=$<TARGET_FILE:Monstey_FearDbInspector>"

 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-db-inspector-independent-results"

 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearDbInspectorIndependent.cmake")

set_tests_properties(NativeFearDbInspectorIndependent PROPERTIES TIMEOUT 120)

install(TARGETS Monstey_FearDbInspector RUNTIME DESTINATION .)



# Owner inspector regression remains synthetic and uses the same main executable.

add_executable(Monstey_FearDbInspectorTests tests/Monstey_FearDbInspectorTests.cpp)

target_link_libraries(Monstey_FearDbInspectorTests PRIVATE Monstey_Data)

target_compile_options(Monstey_FearDbInspectorTests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)

add_test(NAME NativeFearDbInspectorCli COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearDbInspectorTests>" "-DCLI=$<TARGET_FILE:Monstey_FearDbInspector>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-db-inspector-owner-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunInspector.cmake")

set_tests_properties(NativeFearDbInspectorCli PROPERTIES TIMEOUT 120)

# Independently accepted constraint reader and persistent owned targets.
# Append after Monstey_Data/Monstey_Core exist; copy mapped files once.
target_sources(Monstey_Data PRIVATE src/Monstey_FearConstraintReader.cpp src/Monstey_FearConstraintTable.cpp)
add_executable(Monstey_FearConstraintInspect src/Monstey_FearConstraintInspect.cpp)
target_link_libraries(Monstey_FearConstraintInspect PRIVATE Monstey_Data Monstey_Core)
install(TARGETS Monstey_FearConstraintInspect RUNTIME DESTINATION .)
add_executable(Monstey_FearConstraintTests tests/Monstey_FearConstraintTests.cpp)
target_link_libraries(Monstey_FearConstraintTests PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeFearConstraints COMMAND Monstey_FearConstraintTests)
add_executable(Monstey_FearConstraintCliTests tests/Monstey_FearConstraintCliTests.cpp)
target_link_libraries(Monstey_FearConstraintCliTests PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeFearConstraintCli COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearConstraintCliTests>" "-DCLI=$<TARGET_FILE:Monstey_FearConstraintInspect>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli-tests" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunConstraintCli.cmake")
if(NOT TARGET Monstey_FearDbCallableTests)
 add_executable(Monstey_FearDbCallableTests tests/Monstey_FearDbCallableTests.cpp)
 target_include_directories(Monstey_FearDbCallableTests PRIVATE include)
 add_test(NAME NativeFearDbCallable COMMAND Monstey_FearDbCallableTests)
endif()
foreach(pair IN ITEMS Independent StatefulRegression)
 add_executable(Monstey_FearConstraint${pair} tests/Monstey_FearConstraint${pair}.cpp)
 target_link_libraries(Monstey_FearConstraint${pair} PRIVATE Monstey_Data Monstey_Core)
 add_test(NAME NativeFearConstraint${pair} COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearConstraint${pair}>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/constraint-${pair}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunConstraintIndependent.cmake")
endforeach()

# Additive independent tests. Reader/callable products come from accepted C2/U.
# The two existing reference/base fixture files are byte-identical to C2 copies.
add_executable(Monstey_FearConstraintV002Independent tests/Monstey_FearConstraintV002Independent.cpp)
target_link_libraries(Monstey_FearConstraintV002Independent PRIVATE Monstey_Data)
add_test(NAME NativeFearConstraintV002Independent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearConstraintV002Independent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/constraint-v002-independent" -DEXPECTED_GROUPS=8 -DEXPECTED_CASES=2073 -DEXPECTED_CHECKS=8648 -DEXPECTED_FAULTS=1596 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunConstraintV002Independent.cmake")
add_executable(Monstey_FearCallableIndependent tests/Monstey_FearCallableIndependent.cpp)
target_include_directories(Monstey_FearCallableIndependent PRIVATE include)
add_test(NAME NativeFearCallableIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearCallableIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/callable-independent" -DEXPECTED_GROUPS=5 -DEXPECTED_CASES=5 -DEXPECTED_CHECKS=16 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunConstraintV002Independent.cmake")
set_tests_properties(NativeFearConstraintV002Independent NativeFearCallableIndependent PROPERTIES TIMEOUT 120)
set_source_files_properties(src/Monstey_FearConstraintReader.cpp src/Monstey_FearConstraintTable.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
foreach(Monstey_NewConstraintTarget IN ITEMS Monstey_FearConstraintInspect Monstey_FearConstraintTests Monstey_FearConstraintCliTests Monstey_FearDbCallableTests Monstey_FearConstraintIndependent Monstey_FearConstraintStatefulRegression Monstey_FearConstraintV002Independent Monstey_FearCallableIndependent)
 set_property(TARGET ${Monstey_NewConstraintTarget} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(${Monstey_NewConstraintTarget} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endforeach()
set_tests_properties(NativeFearConstraints NativeFearConstraintCli NativeFearDbCallable NativeFearConstraintIndependent NativeFearConstraintStatefulRegression PROPERTIES TIMEOUT 120)

# Accepted exact fragment: line-owner
# This replaces the line-v001 family fragment; compile the selected line source once.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedLine.cpp)
foreach(Monstey_Name IN ITEMS Monstey_FearUnpackedLineTests Monstey_FearUnpackedLineIndependent Monstey_FearUnpackedLineTargetTests)
 add_executable(${Monstey_Name} "tests/${Monstey_Name}.cpp")
 target_link_libraries(${Monstey_Name} PRIVATE Monstey_Data)
 if(MSVC)
  set_property(TARGET ${Monstey_Name} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(${Monstey_Name} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
 endif()
endforeach()
add_test(NAME NativeFearUnpackedLine COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-line-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedLine.cmake")
add_test(NAME NativeFearUnpackedLineIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-line-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedLineIndependent.cmake")
add_test(NAME NativeFearUnpackedLineTargets COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineTargetTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-line-target-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedLineIndependent.cmake")
set_tests_properties(NativeFearUnpackedLine NativeFearUnpackedLineIndependent NativeFearUnpackedLineTargets PROPERTIES TIMEOUT 120)


# Accepted exact fragment: line-review
# Additional independent generated test; owner v002 owns product and three tests.
add_executable(Monstey_LineV002Independent tests/Monstey_LineV002Independent.cpp)
target_link_libraries(Monstey_LineV002Independent PRIVATE Monstey_Data Monstey_Core)
target_compile_options(Monstey_LineV002Independent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_test(NAME NativeLineV002Independent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_LineV002Independent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/line-v002-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunLineV002Independent.cmake")
set_tests_properties(NativeLineV002Independent PROPERTIES TIMEOUT 120)


# Accepted exact fragment: M2-owner
# Main-tree append only; root owns application of this fragment.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedMaterialize.cpp)
add_executable(Monstey_FearUnpackedMaterializeTests tests/Monstey_FearUnpackedMaterializeTests.cpp)
target_link_libraries(Monstey_FearUnpackedMaterializeTests PRIVATE Monstey_Data)
target_compile_options(Monstey_FearUnpackedMaterializeTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeFearUnpackedMaterialize COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedMaterializeTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-materialize" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedMaterialize.cmake")
set_tests_properties(NativeFearUnpackedMaterialize PROPERTIES TIMEOUT 90)
# Append to the owner materializer test fragment; root owns application.
add_executable(Monstey_MaterializerCtorArgumentRegression tests/Monstey_MaterializerCtorArgumentRegression.cpp)
target_link_libraries(Monstey_MaterializerCtorArgumentRegression PRIVATE Monstey_Data)
target_compile_options(Monstey_MaterializerCtorArgumentRegression PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeFearMaterializerCtorArguments COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_MaterializerCtorArgumentRegression>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-materialize" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunMaterializerCtorArgument.cmake")
set_tests_properties(NativeFearMaterializerCtorArguments PROPERTIES TIMEOUT 60)


# Accepted exact fragment: M2-review
# Independent supplements. The exact6ed permanent regression is already in M2.
set_source_files_properties(src/Monstey_FearUnpackedMaterialize.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
function(Monstey_AddMaterializeIndependent target file groups cases checks)
 add_executable(${target} "tests/${file}.cpp")
 target_link_libraries(${target} PRIVATE Monstey_Data)
 set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(${target} PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
 add_test(NAME ${target} COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:${target}>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/${target}-results" "-DEXPECTED_GROUPS=${groups}" "-DEXPECTED_CASES=${cases}" "-DEXPECTED_CHECKS=${checks}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunMaterializeV002Independent.cmake")
 set_tests_properties(${target} PROPERTIES TIMEOUT 120)
endfunction()
Monstey_AddMaterializeIndependent(NativeMaterializeLinksIndependent Monstey_MaterializeLinkIndependent 4 9050 18100)
Monstey_AddMaterializeIndependent(NativeMaterializeSizeIndependent Monstey_MaterializeSizeIndependent 2 1028 6168)
Monstey_AddMaterializeIndependent(NativeMaterializeCtorDeltaIndependent Monstey_MaterializeCtorDeltaIndependent 8 76 495)

# Accepted exact fragment: cleanup-owner
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedCleanup.cpp)
set_source_files_properties(src/Monstey_FearUnpackedCleanup.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_FearUnpackedCleanupTests tests/Monstey_FearUnpackedCleanupTests.cpp)
target_link_libraries(Monstey_FearUnpackedCleanupTests PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeFearUnpackedCleanup COMMAND Monstey_FearUnpackedCleanupTests)
add_executable(Monstey_FearCleanupLifetimeRegression tests/Monstey_FearCleanupLifetimeRegression.cpp)
target_link_libraries(Monstey_FearCleanupLifetimeRegression PRIVATE Monstey_Data Monstey_Core)
add_test(NAME NativeFearCleanupLifetime COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearCleanupLifetimeRegression>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cleanup-lifetime" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunCleanupLifetime.cmake")
set_tests_properties(NativeFearUnpackedCleanup NativeFearCleanupLifetime PROPERTIES TIMEOUT 60)


# Accepted exact fragment: cleanup-review
# Additional independent generated-input test; existingData owns cleanup-v002 once.
add_executable(Monstey_FearCleanupIndependent tests/Monstey_FearCleanupIndependent.cpp)
target_link_libraries(Monstey_FearCleanupIndependent PRIVATE Monstey_Data)
if(MSVC)
 set_property(TARGET Monstey_FearCleanupIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_FearCleanupIndependent PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endif()
add_test(NAME NativeFearCleanupIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearCleanupIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/cleanup-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunCleanupIndependent.cmake")
set_tests_properties(NativeFearCleanupIndependent PROPERTIES TIMEOUT 120)


# Accepted exact fragment: validate-owner
# ExistingData owns accepted memory primitives and canonical common2097.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedValidate.cpp)
add_executable(Monstey_FearUnpackedValidateTests tests/Monstey_FearUnpackedValidateTests.cpp)
target_link_libraries(Monstey_FearUnpackedValidateTests PRIVATE Monstey_Data)
if(MSVC)
 set_property(TARGET Monstey_FearUnpackedValidateTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_FearUnpackedValidateTests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endif()
add_test(NAME NativeFearUnpackedValidate COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedValidateTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-validate-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedValidate.cmake")
set_tests_properties(NativeFearUnpackedValidate PROPERTIES TIMEOUT 120)


# Accepted exact fragment: validate-review
# Requires the exact accepted validator source/header and Callable2097 in Monstey_Data.
add_executable(Monstey_ValidateIndependent tests/Monstey_ValidateIndependent.cpp)
target_link_libraries(Monstey_ValidateIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_ValidateIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeFearUnpackedValidateIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_ValidateIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-validate-independent" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedValidateIndependent.cmake")
set_tests_properties(NativeFearUnpackedValidateIndependent PROPERTIES TIMEOUT 90)
# Copy reference header to tests/ beside the CPP. No original/private test assets.


# Accepted exact fragment: Open-owner
# Main-tree append only. Add source after exact dependency cohorts have been assembled once.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedOpen.cpp)
add_executable(Monstey_FearUnpackedOpenTests tests/Monstey_FearUnpackedOpenTests.cpp)
target_link_libraries(Monstey_FearUnpackedOpenTests PRIVATE Monstey_Data)
target_compile_options(Monstey_FearUnpackedOpenTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeFearUnpackedOpen COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedOpenTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-open" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedOpen.cmake")
set_tests_properties(NativeFearUnpackedOpen PROPERTIES TIMEOUT 90)
# Dependencies: constraints-v002, line-v002, callback-v001, materializer-v002,
# cleanup-v002, accepted memory primitives and shared Callable2097.
# No original/private assets or native-proof maps are default test inputs.


# Accepted exact fragment: Open-review
# Add after exact Open/dependency product files are staged into Monstey_Data.
# This fragment adds only the independent synthetic suite; no duplicate product CPP.
add_executable(Monstey_FearUnpackedOpenIndependent tests/Monstey_FearUnpackedOpenIndependent.cpp)
target_link_libraries(Monstey_FearUnpackedOpenIndependent PRIVATE Monstey_Data Monstey_Core bcrypt)
target_compile_options(Monstey_FearUnpackedOpenIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
set_property(TARGET Monstey_FearUnpackedOpenIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeFearUnpackedOpenIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedOpenIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/open-independent-synthetic" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedOpenIndependent.cmake")
set_tests_properties(NativeFearUnpackedOpenIndependent PROPERTIES TIMEOUT 90)


# Accepted exact fragment: callback-owner
# Existing Monstey_Data already owns accepted primitives and the materializer source.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedCallback.cpp)
add_executable(Monstey_FearUnpackedCallbackTests tests/Monstey_FearUnpackedCallbackTests.cpp)
target_link_libraries(Monstey_FearUnpackedCallbackTests PRIVATE Monstey_Data)
if(MSVC)
 set_property(TARGET Monstey_FearUnpackedCallbackTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_FearUnpackedCallbackTests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endif()
add_test(NAME NativeFearUnpackedCallback COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedCallbackTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-callback-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedCallback.cmake")
set_tests_properties(NativeFearUnpackedCallback PROPERTIES TIMEOUT 120)


# Accepted exact fragment: callback-review
# Callback owner map owns product and original test.
add_executable(Monstey_CallbackIndependent tests/Monstey_CallbackIndependent.cpp)
target_link_libraries(Monstey_CallbackIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_CallbackIndependent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_test(NAME NativeFearCallbackIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_CallbackIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/callback-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearCallbackIndependent.cmake")
set_tests_properties(NativeFearCallbackIndependent PROPERTIES TIMEOUT 120)


# Accepted exact fragment: LTA-minimal-accepted
target_sources(Monstey_Data PRIVATE src/Monstey_LtaDependency.cpp)
add_executable(Monstey_LtaDependencyTests tests/Monstey_LtaDependencyTests.cpp)
add_executable(Monstey_LtaIndependent tests/Monstey_LtaIndependent.cpp)
foreach(Monstey_Target IN ITEMS Monstey_LtaDependencyTests Monstey_LtaIndependent)
 target_link_libraries(${Monstey_Target} PRIVATE Monstey_Data)
 target_compile_options(${Monstey_Target} PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endforeach()
add_test(NAME NativeFearSidecarLta COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_LtaDependencyTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/lta-owner-results" "-DKIND=Lta" "-DGROUPS=5" "-DCASES=282" "-DCHECKS=282" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSidecarSynthetic.cmake")
add_test(NAME NativeLtaIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_LtaIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/lta-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunLtaIndependent.cmake")
set_tests_properties(NativeFearSidecarLta NativeLtaIndependent PROPERTIES TIMEOUT 120)


# Accepted exact fragment: comparator-owner-accepted
# Replace the three mapped existing files and fully rebuild all consumers.
# Names.cpp/Open.cpp already occur once in Monstey_Data; do NOT target_sources again.
set_source_files_properties(src/Monstey_FearDbNames.cpp src/Monstey_FearDbOpen.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_FearComparatorLifetimeTests tests/Monstey_FearComparatorLifetimeTests.cpp)
target_link_libraries(Monstey_FearComparatorLifetimeTests PRIVATE Monstey_Data Monstey_Core bcrypt)
target_compile_options(Monstey_FearComparatorLifetimeTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
set_property(TARGET Monstey_FearComparatorLifetimeTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeFearComparatorLifetime COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearComparatorLifetimeTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/comparator-lifetime-synthetic" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearComparatorLifetime.cmake")
set_tests_properties(NativeFearComparatorLifetime PROPERTIES TIMEOUT 120)


# Accepted exact fragment: logicalmaps-review
# Add only after the callback product and accepted M2 materializer are in Monstey_Data.
# This fragment adds independent profile tests; it does not promote callback native bodies.
foreach(kind IN ITEMS Ordinary Epoch)
 if(kind STREQUAL "Ordinary")
  set(source tests/Monstey_LogicalMapsIndependent.cpp)
  set(cases 5765)
  set(checks 3966919)
 else()
  set(source tests/Monstey_LogicalMapsEpochIndependent.cpp)
  set(cases 15)
  set(checks 1604)
 endif()
 add_executable(Monstey_LogicalMaps${kind} ${source})
 target_link_libraries(Monstey_LogicalMaps${kind} PRIVATE Monstey_Data)
 if(MSVC)
  set_property(TARGET Monstey_LogicalMaps${kind} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(Monstey_LogicalMaps${kind} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
 endif()
 add_test(NAME NativeFearLogicalMaps${kind} COMMAND "${CMAKE_COMMAND}"
  "-DEXE=$<TARGET_FILE:Monstey_LogicalMaps${kind}>"
  "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/logicalmap-results/${kind}"
  "-DEXPECTED_CASES=${cases}" "-DEXPECTED_CHECKS=${checks}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunLogicalMapsIndependent.cmake")
 set_tests_properties(NativeFearLogicalMaps${kind} PROPERTIES TIMEOUT 120)
endforeach()


# Accepted exact fragment: comparator-review
# Existing Monstey_Data must be fully rebuilt with the accepted new NameCompare type.
add_executable(Monstey_ComparatorIndependent tests/Monstey_ComparatorAddressIndependent.cpp)
target_compile_definitions(Monstey_ComparatorIndependent PRIVATE MONSTEY_BIND_PRODUCT=1)
target_link_libraries(Monstey_ComparatorIndependent PRIVATE Monstey_Data)
if(MSVC)
 set_property(TARGET Monstey_ComparatorIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 target_compile_options(Monstey_ComparatorIndependent PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
endif()
add_test(NAME NativeFearComparatorIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_ComparatorIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/comparator-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunComparatorIndependent.cmake")
set_tests_properties(NativeFearComparatorIndependent PROPERTIES TIMEOUT 120)


# Accepted exact fragment: inspector-naming-owner
# Add after exact accepted Open/C2/line/CB/M2/cleanup/validator and Callable sources.
target_sources(Monstey_Data PRIVATE src/Monstey_FearUnpackedOwned.cpp src/Monstey_FearUnpackedInspection.cpp)
add_executable(Monstey_FearUnpackedInspector src/Monstey_FearUnpackedInspector.cpp)
target_link_libraries(Monstey_FearUnpackedInspector PRIVATE Monstey_Data)
target_compile_options(Monstey_FearUnpackedInspector PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_executable(Monstey_UnpackedInspectorTests tests/Monstey_UnpackedInspectorTests.cpp)
target_link_libraries(Monstey_UnpackedInspectorTests PRIVATE Monstey_Data)
target_compile_options(Monstey_UnpackedInspectorTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeFearUnpackedOwned COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_UnpackedInspectorTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-owned" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedOwned.cmake")
set_tests_properties(NativeFearUnpackedOwned PROPERTIES TIMEOUT 110)
add_test(NAME NativeFearUnpackedInspectorCli COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedInspector>" "-DTEST_EXE=$<TARGET_FILE:Monstey_UnpackedInspectorTests>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fear-unpacked-cli" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunFearUnpackedInspectorCli.cmake")
set_tests_properties(NativeFearUnpackedInspectorCli PROPERTIES TIMEOUT 160)
# Default inputs are created by the owned C++ fixture generator; no original assets.


# Accepted exact fragment: inspector-final-review
add_executable(Monstey_UnpackedInspectorFixtures tests/Monstey_UnpackedInspectorFixtureGenerator.cpp)
target_link_libraries(Monstey_UnpackedInspectorFixtures PRIVATE Monstey_Core)
add_executable(Monstey_UnpackedInspectorIndependent tests/Monstey_UnpackedInspectorExpanded.cpp)
target_link_libraries(Monstey_UnpackedInspectorIndependent PRIVATE Monstey_Data)
foreach(t IN ITEMS Monstey_UnpackedInspectorFixtures Monstey_UnpackedInspectorIndependent)
 if(MSVC)
  set_property(TARGET ${t} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(${t} PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
 endif()
endforeach()
add_test(NAME NativeFearUnpackedInspectorIndependent COMMAND "${CMAKE_COMMAND}" "-DGEN=$<TARGET_FILE:Monstey_UnpackedInspectorFixtures>" "-DEXE=$<TARGET_FILE:Monstey_UnpackedInspectorIndependent>" "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/unpacked-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunUnpackedInspectorIndependent.cmake")
set_tests_properties(NativeFearUnpackedInspectorIndependent PROPERTIES TIMEOUT 120)


# Final strict/dedup/install guard
# Exact final integration guard/options. Each Data source is already appended once.
set(Monstey_FinalCriticalDataCpp
 "src/Monstey_FearUnpackedLine.cpp"
 "src/Monstey_FearUnpackedMaterialize.cpp"
 "src/Monstey_FearUnpackedCleanup.cpp"
 "src/Monstey_FearUnpackedValidate.cpp"
 "src/Monstey_FearUnpackedOpen.cpp"
 "src/Monstey_FearUnpackedCallback.cpp"
 "src/Monstey_LtaDependency.cpp"
 "src/Monstey_FearUnpackedOwned.cpp"
 "src/Monstey_FearUnpackedInspection.cpp"
 "src/Monstey_FearDbNames.cpp"
 "src/Monstey_FearDbOpen.cpp"
)
get_target_property(Monstey_FinalDataSources Monstey_Data SOURCES)
foreach(Monstey_Cpp IN LISTS Monstey_FinalCriticalDataCpp)
 set(Monstey_Count 0)
 foreach(Monstey_Entry IN LISTS Monstey_FinalDataSources)
  if(Monstey_Entry STREQUAL Monstey_Cpp)
   math(EXPR Monstey_Count "${Monstey_Count}+1")
  endif()
 endforeach()
 if(NOT Monstey_Count EQUAL 1)
  message(FATAL_ERROR "Expected exactly one Data source ${Monstey_Cpp}, got ${Monstey_Count}")
 endif()
endforeach()
set_source_files_properties(${Monstey_FinalCriticalDataCpp} PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/permissive-;/EHsc;/utf-8;/fp:strict")
list(FIND Monstey_FinalDataSources "src/Monstey_FearUnpackedInspector.cpp" Monstey_InspectorMainInData)
if(NOT Monstey_InspectorMainInData EQUAL -1)
 message(FATAL_ERROR "Standalone inspector main must not belong to Data")
endif()
install(TARGETS Monstey_FearUnpackedInspector RUNTIME DESTINATION .)
set(Monstey_FinalNewTargets
 "Monstey_LineV002Independent"
 "Monstey_FearUnpackedMaterializeTests"
 "Monstey_MaterializerCtorArgumentRegression"
 "Monstey_FearUnpackedCleanupTests"
 "Monstey_FearCleanupLifetimeRegression"
 "Monstey_FearCleanupIndependent"
 "Monstey_FearUnpackedValidateTests"
 "Monstey_ValidateIndependent"
 "Monstey_FearUnpackedOpenTests"
 "Monstey_FearUnpackedOpenIndependent"
 "Monstey_FearUnpackedCallbackTests"
 "Monstey_CallbackIndependent"
 "Monstey_LtaDependencyTests"
 "Monstey_LtaIndependent"
 "Monstey_FearComparatorLifetimeTests"
 "Monstey_ComparatorIndependent"
 "Monstey_FearUnpackedInspector"
 "Monstey_UnpackedInspectorTests"
 "Monstey_UnpackedInspectorFixtures"
 "Monstey_UnpackedInspectorIndependent"
 "Monstey_FearUnpackedLineTests"
 "Monstey_FearUnpackedLineIndependent"
 "Monstey_FearUnpackedLineTargetTests"
 "NativeMaterializeLinksIndependent"
 "NativeMaterializeSizeIndependent"
 "NativeMaterializeCtorDeltaIndependent"
 "Monstey_LogicalMapsOrdinary"
 "Monstey_LogicalMapsEpoch"
)
foreach(Monstey_Target IN LISTS Monstey_FinalNewTargets)
 if(MSVC)
  set_property(TARGET ${Monstey_Target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
  target_compile_options(${Monstey_Target} PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
 endif()
endforeach()


# Exact accepted SoW fragment
# Replace guarded Gadb.hpp/cpp; existing Data compiles Gadb.cpp once. Do not add duplicate.
set_source_files_properties(src/Monstey_Gadb.cpp PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/permissive-;/EHsc;/utf-8;/fp:strict")
add_executable(Monstey_AtomInspect src/Monstey_AtomInspect.cpp)
add_executable(Monstey_AtomUsesTests tests/Monstey_AtomUsesTests.cpp)
foreach(Monstey_Target IN ITEMS Monstey_AtomInspect Monstey_AtomUsesTests)
 target_link_libraries(${Monstey_Target} PRIVATE Monstey_Data)
 target_compile_options(${Monstey_Target} PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
 set_property(TARGET ${Monstey_Target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
endforeach()
# Test fixture directory must be absent; future integration proposal, not applied.
add_test(NAME NativeGadbAtomUses COMMAND Monstey_AtomUsesTests "${CMAKE_CURRENT_BINARY_DIR}/atomuses-synthetic")
set_tests_properties(NativeGadbAtomUses PROPERTIES TIMEOUT 60 LABELS "native;sow;database;synthetic")

# Exact accepted SoW fragment
# One new default synthetic test; no private game/PE inputs required.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbPoolRelocation.cpp)
set_source_files_properties(src/Monstey_GadbPoolRelocation.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbPoolRelocationTests tests/Monstey_GadbPoolRelocationTests.cpp)
target_link_libraries(Monstey_GadbPoolRelocationTests PRIVATE Monstey_Data)
set_property(TARGET Monstey_GadbPoolRelocationTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_GadbPoolRelocationTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbPoolRelocation COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbPoolRelocationTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/pool-relocation-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbPoolRelocation.cmake")
set_tests_properties(NativeGadbPoolRelocation PROPERTIES TIMEOUT 120)

# Exact accepted SoW fragment
# One independent synthetic default test. The owner fragment supplies the product CPP.
add_executable(Monstey_SowPoolIndependent tests/Monstey_SowPoolProductAdapter010.cpp tests/Monstey_SowPoolIndependent010.cpp)
target_link_libraries(Monstey_SowPoolIndependent PRIVATE Monstey_Data)
target_include_directories(Monstey_SowPoolIndependent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_features(Monstey_SowPoolIndependent PRIVATE cxx_std_20)
set_property(TARGET Monstey_SowPoolIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_SowPoolIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeSowPoolIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_SowPoolIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/sow-pool-independent-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowPoolIndependent.cmake")
set_tests_properties(NativeSowPoolIndependent PROPERTIES TIMEOUT 120)

# Exact accepted SoW fragment
# Prerequisite: accepted PoolRelocation already belongs to Monstey_Data.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbAnonymousRecord.cpp)
set_source_files_properties(src/Monstey_GadbAnonymousRecord.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbAnonymousRecordTests tests/Monstey_GadbAnonymousRecordTests.cpp)
target_link_libraries(Monstey_GadbAnonymousRecordTests PRIVATE Monstey_Data)
set_property(TARGET Monstey_GadbAnonymousRecordTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_GadbAnonymousRecordTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbAnonymousRecord COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbAnonymousRecordTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/anonymous-record-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbAnonymousRecord.cmake")
set_tests_properties(NativeGadbAnonymousRecord PROPERTIES TIMEOUT 120)

# Exact accepted SoW fragment
# One bounded independent synthetic test. Owner fragment supplies the anonymous CPP.
add_executable(Monstey_SowAnonymousIndependent tests/Monstey_AnonymousProductAdapter003.cpp tests/Monstey_AnonymousIndependent003.cpp)
target_link_libraries(Monstey_SowAnonymousIndependent PRIVATE Monstey_Data)
target_include_directories(Monstey_SowAnonymousIndependent PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_features(Monstey_SowAnonymousIndependent PRIVATE cxx_std_20)
set_property(TARGET Monstey_SowAnonymousIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_SowAnonymousIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeSowAnonymousIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_SowAnonymousIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/anonymous-independent-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowAnonymousIndependent.cmake")
set_tests_properties(NativeSowAnonymousIndependent PROPERTIES TIMEOUT 120)

# Exact accepted SoW fragment
# Additive SoW record storage API and native menu regression.
add_executable(Monstey_SowAtomRuntimeTests tests/Monstey_SowAtomRuntimeTests.cpp)
target_link_libraries(Monstey_SowAtomRuntimeTests PRIVATE Monstey_Runtime)
target_compile_options(Monstey_SowAtomRuntimeTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_SowAtomRuntimeTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME Monstey_SowAtomRuntime COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_SowAtomRuntimeTests>"
 "-DCATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/sow-atom/catalog226.json"
 "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/sow-atom-runtime"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowAtomRuntime.cmake")
add_test(NAME NativeSowAtomGui COMMAND "${MONSTEY_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
 -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowAtomGui.ps1"
 -Gui "$<TARGET_FILE:Monstey_Workbench>"
 -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/tests/sow-atom-gui")
set_tests_properties(NativeSowAtomGui PROPERTIES TIMEOUT 90)

# Final candidate strict/dedup supplement
# Existing and new SoW product CPPs compile once; no build-overlay headers are copied.
set(Monstey_SowFinalDataCpp "src/Monstey_Gadb.cpp" "src/Monstey_GadbPoolRelocation.cpp" "src/Monstey_GadbAnonymousRecord.cpp")
get_target_property(Monstey_SowFinalDataSources Monstey_Data SOURCES)
foreach(Monstey_Source IN LISTS Monstey_SowFinalDataCpp)
 set(Monstey_Count 0)
 foreach(Monstey_Existing IN LISTS Monstey_SowFinalDataSources)
  if(Monstey_Existing STREQUAL Monstey_Source)
   math(EXPR Monstey_Count "${Monstey_Count}+1")
  endif()
 endforeach()
 if(NOT Monstey_Count EQUAL 1)
  message(FATAL_ERROR "SoW Data source must occur exactly once: ${Monstey_Source}")
 endif()
endforeach()
if(MSVC)
 set_source_files_properties(${Monstey_SowFinalDataCpp} src/Monstey_Runtime.cpp src/Monstey_App.cpp PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/permissive-;/EHsc;/utf-8;/fp:strict")
 foreach(Monstey_Target IN ITEMS Monstey_AtomInspect Monstey_AtomUsesTests Monstey_GadbPoolRelocationTests Monstey_SowPoolIndependent Monstey_GadbAnonymousRecordTests Monstey_SowAnonymousIndependent Monstey_SowAtomRuntimeTests)
  target_compile_options(${Monstey_Target} PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
  set_property(TARGET ${Monstey_Target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
 endforeach()
endif()

# Additive SoW concrete category/stream composition (accepted components).

# FUTURE COHORT ONLY: not part of the current Named/Anonymous/AtomUses/UI cut.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbBufferedStream.cpp)
set_source_files_properties(src/Monstey_GadbBufferedStream.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbBufferedStreamTests tests/Monstey_GadbBufferedStreamTests.cpp)
target_link_libraries(Monstey_GadbBufferedStreamTests PRIVATE Monstey_Data)
set_property(TARGET Monstey_GadbBufferedStreamTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_GadbBufferedStreamTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbBufferedStream COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbBufferedStreamTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/buffered-stream-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbBufferedStream.cmake")
set_tests_properties(NativeGadbBufferedStream PROPERTIES TIMEOUT 120)

# Add only after the separately accepted BufferedStream548 component.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbConcreteRecords.cpp)
set_source_files_properties(src/Monstey_GadbConcreteRecords.cpp PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/EHsc;/permissive-;/utf-8;/fp:strict")
add_executable(Monstey_GadbConcreteRecordsTests tests/Monstey_GadbConcreteRecordsTests.cpp)
target_link_libraries(Monstey_GadbConcreteRecordsTests PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbConcreteRecordsTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbConcreteRecords COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbConcreteRecordsTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/concrete-record-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbConcreteRecords.cmake")
set_tests_properties(NativeGadbConcreteRecords PROPERTIES TIMEOUT 120)

# Later coherent cohort only. Requires accepted BufferedStream in Monstey_Data.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbEncodedString.cpp)
set_source_files_properties(src/Monstey_GadbEncodedString.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)
add_executable(Monstey_GadbEncodedStringTests tests/Monstey_GadbEncodedStringTests.cpp)
target_link_libraries(Monstey_GadbEncodedStringTests PRIVATE Monstey_Data)
set_property(TARGET Monstey_GadbEncodedStringTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(Monstey_GadbEncodedStringTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbEncodedString COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbEncodedStringTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/encoded-string-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbEncodedString.cmake")
set_tests_properties(NativeGadbEncodedString PROPERTIES TIMEOUT 120)

# Proposed only. Requires the accepted BinderMemory/Callable/BufferedStream headers.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbCategoryLookup.cpp)
 add_executable(Monstey_GadbCategoryLookupTests tests/Monstey_GadbCategoryLookupTests.cpp)
 target_link_libraries(Monstey_GadbCategoryLookupTests PRIVATE Monstey_Data)
 target_compile_options(Monstey_GadbCategoryLookupTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
 set_property(TARGET Monstey_GadbCategoryLookupTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 add_test(NAME NativeSowGadbCategoryLookup COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbCategoryLookupTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/category-lookup-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowGadbCategoryLookup.cmake")
 set_tests_properties(NativeSowGadbCategoryLookup PROPERTIES TIMEOUT 60)

# Proposed future main integration; do not execute against the current cohort.
# Existing accepted BankActivation/BankSynchronization/Binder/Core must remain.
# Requires existing BufferedResult header00600a3b and Callable2097 declarations.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbBankSelectorRegisters.cpp)
add_executable(Monstey_GadbBankSelectorRegistersTests tests/Monstey_GadbBankSelectorRegistersTests.cpp)
target_link_libraries(Monstey_GadbBankSelectorRegistersTests PRIVATE Monstey_Data Monstey_Core bcrypt)
target_compile_options(Monstey_GadbBankSelectorRegistersTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbBankSelectorRegisters COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbBankSelectorRegistersTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/bank-selector-registers-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbBankSelectorRegisters.cmake")
set_tests_properties(NativeGadbBankSelectorRegisters PROPERTIES TIMEOUT 120)

# Future root-owned integration only. Requires all accepted concrete callees.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbCategoryReader.cpp)
add_executable(Monstey_GadbCategoryReaderTests tests/Monstey_GadbCategoryReaderTests.cpp)
target_link_libraries(Monstey_GadbCategoryReaderTests PRIVATE Monstey_Data Monstey_Core bcrypt)
target_compile_options(Monstey_GadbCategoryReaderTests PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
add_test(NAME NativeGadbCategoryReader COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbCategoryReaderTests>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/category-reader-reports" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbCategoryReader.cmake")
set_tests_properties(NativeGadbCategoryReader PROPERTIES TIMEOUT 120)

# Proposal only; category and accepted callees are compiled once in existing Data.
add_executable(Monstey_GadbCategoryCompositionIndependent
 tests/sow-category-independent/Monstey_CategoryReference.cpp
 tests/sow-category-independent/Monstey_CategoryConcreteBindings.cpp
 tests/sow-category-independent/Monstey_CategoryCompositionIndependentFinal.cpp)
target_include_directories(Monstey_GadbCategoryCompositionIndependent PRIVATE tests/sow-category-independent)
target_link_libraries(Monstey_GadbCategoryCompositionIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbCategoryCompositionIndependent PRIVATE /O2 /W4 /WX /EHsc /permissive- /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbCategoryCompositionIndependent PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_test(NAME NativeSowCategoryCompositionIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbCategoryCompositionIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/sow-category-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowCategoryCompositionIndependent.cmake")
set_tests_properties(NativeSowCategoryCompositionIndependent PROPERTIES TIMEOUT 120)

# New source units only; preserve every inherited target/source property.
set_source_files_properties(src/Monstey_GadbCategoryLookup.cpp src/Monstey_GadbBankSelectorRegisters.cpp src/Monstey_GadbCategoryReader.cpp PROPERTIES COMPILE_OPTIONS /fp:strict)

# Accepted SoW publication components: fresh source compilation in this cohort.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbPublication.cpp src/Monstey_GadbPublicationFramed.cpp)
set_source_files_properties(src/Monstey_GadbPublication.cpp src/Monstey_GadbPublicationFramed.cpp PROPERTIES COMPILE_OPTIONS "/O2;/W4;/WX;/permissive-;/EHsc;/utf-8;/fp:strict")
add_executable(Monstey_GadbPublicationTests tests/Monstey_GadbPublicationTests.cpp)
target_link_libraries(Monstey_GadbPublicationTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbPublicationTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbPublicationTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbPublicationTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowGadbPublication COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbPublicationTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/publication-owner-reports"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbPublication.cmake")
set_tests_properties(NativeSowGadbPublication PROPERTIES TIMEOUT 60)

add_executable(Monstey_GadbPublicationFramedTests tests/Monstey_GadbPublicationFramedTests.cpp)
target_link_libraries(Monstey_GadbPublicationFramedTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbPublicationFramedTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbPublicationFramedTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbPublicationFramedTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowGadbPublicationFramed COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbPublicationFramedTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/owner-reports"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbPublicationFramed.cmake")
set_tests_properties(NativeSowGadbPublicationFramed PROPERTIES TIMEOUT 60)
add_executable(Monstey_GadbPublicationFramedProjectionTests tests/Monstey_GadbPublicationFramedProjectionTests.cpp)
target_link_libraries(Monstey_GadbPublicationFramedProjectionTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbPublicationFramedProjectionTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbPublicationFramedProjectionTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbPublicationFramedProjectionTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowGadbPublicationFramedProjection COMMAND "${CMAKE_COMMAND}"
 "-DEXE=$<TARGET_FILE:Monstey_GadbPublicationFramedProjectionTests>"
 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/projection-reports"
 -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunGadbPublicationFramed.cmake")
set_tests_properties(NativeSowGadbPublicationFramedProjection PROPERTIES TIMEOUT 60)
# Proposed only: actual publication product CPP is compiled once in Data by owner map.
add_executable(Monstey_GadbPublicationIndependent
 tests/sow-publication-independent/Monstey_PublicationReference.cpp
 tests/Monstey_PublicationIndependent.cpp)
target_include_directories(Monstey_GadbPublicationIndependent PRIVATE tests/sow-publication-independent)
target_link_libraries(Monstey_GadbPublicationIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbPublicationIndependent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbPublicationIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowPublicationIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbPublicationIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/publication-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowPublicationIndependent.cmake")
set_tests_properties(NativeSowPublicationIndependent PROPERTIES TIMEOUT 120)

# Proposed only. Compile owner generic and additive framed product CPP once each in Data.
foreach(Monstey_PubTarget IN ITEMS Monstey_GadbPublicationFramedIndependent Monstey_GadbPublicationFrameExceptionsIndependent)
 if(Monstey_PubTarget STREQUAL "Monstey_GadbPublicationFramedIndependent")
  set(Monstey_PubTestSource tests/Monstey_PublicationFramedIndependent.cpp)
  set(Monstey_PubCTest NativeSowPublicationFramedIndependent)
  set(Monstey_PubOut publication-framed-independent-results)
 else()
  set(Monstey_PubTestSource tests/Monstey_PublicationFrameExceptionsIndependent.cpp)
  set(Monstey_PubCTest NativeSowPublicationFrameExceptionsIndependent)
  set(Monstey_PubOut publication-frame-exceptions-independent-results)
 endif()
 add_executable(${Monstey_PubTarget} tests/sow-publication-framed-independent/Monstey_PublicationFramedReference.cpp ${Monstey_PubTestSource})
 target_include_directories(${Monstey_PubTarget} PRIVATE tests/sow-publication-framed-independent)
 target_link_libraries(${Monstey_PubTarget} PRIVATE Monstey_Data)
 target_compile_options(${Monstey_PubTarget} PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
 set_property(TARGET ${Monstey_PubTarget} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
 add_test(NAME ${Monstey_PubCTest} COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:${Monstey_PubTarget}>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/${Monstey_PubOut}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowPublicationFramedIndependent.cmake")
 set_tests_properties(${Monstey_PubCTest} PROPERTIES TIMEOUT 120)
endforeach()


# Accepted manager primitives and placement: coherent integration preparation002.

# M1-owner
# Prepared only. Apply to a fresh coherent main under separate Root integration GO.
# Compile the new product CPP once in existing Monstey_Data (112 -> 113 sources).
target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerPrimitives.cpp)
target_compile_features(Monstey_Data PRIVATE cxx_std_20)
set_property(SOURCE src/Monstey_GadbManagerPrimitives.cpp APPEND PROPERTY
  COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
# The accepted main Monstey_Data runtime must remain MultiThreaded (/MT).
add_executable(Monstey_GadbManagerPrimitivesTests
  tests/Monstey_GadbManagerPrimitivesTests.cpp)
target_link_libraries(Monstey_GadbManagerPrimitivesTests PRIVATE Monstey_Data)
target_compile_features(Monstey_GadbManagerPrimitivesTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerPrimitivesTests PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerPrimitivesTests PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerPrimitivesOwned
  COMMAND $<TARGET_FILE:Monstey_GadbManagerPrimitivesTests>)
set_tests_properties(NativeSowManagerPrimitivesOwned PROPERTIES TIMEOUT 120)


# M1-independent
# Proposed only: owner product CPP must be compiled once in existing Data.
add_executable(Monstey_GadbManagerPrimitivesIndependent
 tests/sow-manager-primitives-independent/Monstey_ManagerPrimitiveReference.cpp
 tests/Monstey_ManagerPrimitiveIndependent.cpp)
target_include_directories(Monstey_GadbManagerPrimitivesIndependent PRIVATE tests/sow-manager-primitives-independent)
target_link_libraries(Monstey_GadbManagerPrimitivesIndependent PRIVATE Monstey_Data)
target_compile_options(Monstey_GadbManagerPrimitivesIndependent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerPrimitivesIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowGadbManagerPrimitivesIndependent COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:Monstey_GadbManagerPrimitivesIndependent>" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/manager-primitives-independent-results" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerPrimitiveIndependent.cmake")
set_tests_properties(NativeSowGadbManagerPrimitivesIndependent PROPERTIES TIMEOUT 120)


# M2-owner
# Proposed only; apply to a fresh coherent main under separate Root integration GO.
# This unit adds one fresh CPP to existing Data; combine sibling increments at Root.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerPlacement.cpp)
target_compile_features(Monstey_Data PRIVATE cxx_std_20)
set_property(SOURCE src/Monstey_GadbManagerPlacement.cpp APPEND PROPERTY
  COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
# Existing Monstey_Data runtime remains MultiThreaded (/MT).
add_executable(Monstey_GadbManagerPlacementTests
  tests/Monstey_GadbManagerPlacementTests.cpp)
target_link_libraries(Monstey_GadbManagerPlacementTests PRIVATE Monstey_Data)
target_compile_features(Monstey_GadbManagerPlacementTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerPlacementTests PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerPlacementTests PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerPlacementOwned
  COMMAND $<TARGET_FILE:Monstey_GadbManagerPlacementTests>)
set_tests_properties(NativeSowManagerPlacementOwned PROPERTIES TIMEOUT 120)


# M2-independent
# Actual product CPP is compiled once into Monstey_Data by its owner map.
# A future full cohort recompiles this frozen independent reference;
# the isolated accepted campaign imported its prior object unchanged.
add_executable(Monstey_SowManagerPlacementIndependent
  tests/Monstey_SowManagerPlacementIndependent.cpp
  tests/Monstey_PlacementReference.cpp)
target_include_directories(Monstey_SowManagerPlacementIndependent PRIVATE include tests)
target_link_libraries(Monstey_SowManagerPlacementIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerPlacementIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerPlacementIndependent PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_SowManagerPlacementIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerPlacementIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerPlacementIndependent>"
    "-DMonstey_ReportDirectory=${CMAKE_CURRENT_BINARY_DIR}/sow-manager-placement-independent-results"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerPlacementIndependent.cmake")
set_tests_properties(NativeSowManagerPlacementIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)


# Prepared owner backend integration only; Root applies to a fresh coherent source.
# Compile the twelve-body product unit exactly once in Data; lower placement is
# already present and must not be copied, re-added or imported separately.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerBackend.cpp)
target_compile_features(Monstey_Data PRIVATE cxx_std_20)
set_property(SOURCE src/Monstey_GadbManagerBackend.cpp APPEND PROPERTY
  COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
# Preserve existing Data MultiThreaded (/MT) Release runtime.
add_executable(Monstey_GadbManagerBackendTests
  tests/Monstey_GadbManagerBackendTests.cpp)
target_link_libraries(Monstey_GadbManagerBackendTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbManagerBackendTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerBackendTests PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerBackendTests PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerBackendOwned
  COMMAND $<TARGET_FILE:Monstey_GadbManagerBackendTests>)
set_tests_properties(NativeSowManagerBackendOwned PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

# Independent backend peer target only; Root appends after the owner Data115 unit.
# Recompile all oracle/observer source; use the existing exact lower reference
# CPP already staged in BaseK. No precompiled oracle object or old archive.
add_executable(Monstey_SowManagerBackendIndependent
  tests/Monstey_BackendDifferential.cpp
  tests/Monstey_BackendReference.cpp
  tests/Monstey_PlacementReference.cpp)
target_include_directories(Monstey_SowManagerBackendIndependent PRIVATE include tests)
target_link_libraries(Monstey_SowManagerBackendIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerBackendIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerBackendIndependent PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict /MT)
set_property(TARGET Monstey_SowManagerBackendIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerBackendIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerBackendIndependent>"
    "-DMonstey_Out=${CMAKE_CURRENT_BINARY_DIR}/backend-independent-results"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerBackendIndependent.cmake")
set_tests_properties(NativeSowManagerBackendIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)


# Free owner: append under a future Root coherent-cohort execution gate.
# Existing Data already owns BK and M2; compile only the new Free CPP once.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerFree.cpp)
target_compile_features(Monstey_Data PRIVATE cxx_std_20)
set_property(SOURCE src/Monstey_GadbManagerFree.cpp APPEND PROPERTY
  COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_executable(Monstey_GadbManagerFreeTests
  tests/Monstey_GadbManagerFreeTests.cpp)
target_link_libraries(Monstey_GadbManagerFreeTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbManagerFreeTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerFreeTests PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerFreeTests PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerFreeOwned
  COMMAND $<TARGET_FILE:Monstey_GadbManagerFreeTests>
    "${CMAKE_CURRENT_BINARY_DIR}/gadb-manager-free-owner-report.json")
set_tests_properties(NativeSowManagerFreeOwned PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

# Append only after owner adds src/Monstey_GadbManagerFree.cpp to Monstey_Data.
# Compile current source for all independent algorithms; no frozen .obj/.lib.
add_executable(Monstey_SowManagerFreeIndependent
  tests/Monstey_FreeActualIndependent.cpp
  tests/Monstey_FreeReference.cpp
  tests/Monstey_BackendReference.cpp
  tests/Monstey_PlacementReference.cpp)
target_include_directories(Monstey_SowManagerFreeIndependent PRIVATE include tests)
target_link_libraries(Monstey_SowManagerFreeIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerFreeIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerFreeIndependent PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict /MT)
set_property(TARGET Monstey_SowManagerFreeIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerFreeIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerFreeIndependent>"
    "-DMonstey_Out=${CMAKE_CURRENT_BINARY_DIR}/free-independent-results"
    "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
    "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
    "-DMonstey_Build=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMonstey_ObjectRoot=${CMAKE_CURRENT_BINARY_DIR}/Monstey_SowManagerFreeIndependent.dir/$<CONFIG>"
    "-DMonstey_DataObjectRoot=${CMAKE_CURRENT_BINARY_DIR}/Monstey_Data.dir/$<CONFIG>"
    "-DMonstey_FreeCPP=${CMAKE_CURRENT_SOURCE_DIR}/src/Monstey_GadbManagerFree.cpp"
    "-DMonstey_FreeHPP=${CMAKE_CURRENT_SOURCE_DIR}/include/Monstey_GadbManagerFree.hpp"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerFreeIndependent.cmake")
set_tests_properties(NativeSowManagerFreeIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)


# Additive30B Guard projection only; accepted old M1 remains unchanged.
target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerGuardProjection.cpp)
target_compile_features(Monstey_Data PRIVATE cxx_std_20)
set_property(SOURCE src/Monstey_GadbManagerGuardProjection.cpp APPEND PROPERTY
  COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_executable(Monstey_GadbManagerGuardProjectionTests
  tests/Monstey_GadbManagerGuardProjectionTests.cpp)
target_link_libraries(Monstey_GadbManagerGuardProjectionTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbManagerGuardProjectionTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerGuardProjectionTests PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerGuardProjectionTests PROPERTY
  MSVC_RUNTIME_LIBRARY MultiThreaded)
add_test(NAME NativeSowManagerGuardProjectionOwned
  COMMAND $<TARGET_FILE:Monstey_GadbManagerGuardProjectionTests>
    "${CMAKE_CURRENT_BINARY_DIR}/gadb-manager-guard-projection-owner-report.json")
set_tests_properties(NativeSowManagerGuardProjectionOwned PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

# One independent peer test, fresh reference/observer CPPs and future Data/Core.
add_executable(Monstey_SowManagerGuardProjectionIndependent
  tests/Monstey_GuardProjectionDifferential.cpp
  tests/Monstey_GuardProjectionReference.cpp)
target_include_directories(Monstey_SowManagerGuardProjectionIndependent PRIVATE include tests)
target_link_libraries(Monstey_SowManagerGuardProjectionIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerGuardProjectionIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerGuardProjectionIndependent PRIVATE
  /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_SowManagerGuardProjectionIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_link_options(Monstey_SowManagerGuardProjectionIndependent PRIVATE
  "/MAP:${CMAKE_CURRENT_BINARY_DIR}/guard-projection-independent-$<CONFIG>.map")
add_test(NAME NativeSowManagerGuardProjectionIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerGuardProjectionIndependent>"
    "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
    "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
    "-DMonstey_Out=${CMAKE_CURRENT_BINARY_DIR}/sow-manager-guard-projection-independent-results"
    "-DMonstey_SourceRoot=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DMonstey_BuildRoot=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMonstey_Config=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerGuardProjectionIndependent.cmake")
set_tests_properties(NativeSowManagerGuardProjectionIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

include(cmake/Monstey_Modules.cmake)

target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerLeaveProjection.cpp)
set_property(SOURCE src/Monstey_GadbManagerLeaveProjection.cpp APPEND PROPERTY COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_executable(Monstey_GadbManagerLeaveProjectionTests tests/Monstey_LeaveProductOwned.cpp)
target_link_libraries(Monstey_GadbManagerLeaveProjectionTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbManagerLeaveProjectionTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerLeaveProjectionTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerLeaveProjectionTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_link_options(Monstey_GadbManagerLeaveProjectionTests PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/leave-owner-$<CONFIG>.map" /INCREMENTAL:NO)
add_test(NAME NativeSowManagerLeaveProjectionOwned COMMAND $<TARGET_FILE:Monstey_GadbManagerLeaveProjectionTests> "${CMAKE_CURRENT_BINARY_DIR}/gadb-manager-leave-owner-report.json")
set_tests_properties(NativeSowManagerLeaveProjectionOwned PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

target_sources(Monstey_Data PRIVATE src/Monstey_GadbManagerParent.cpp)
set_property(SOURCE src/Monstey_GadbManagerParent.cpp APPEND PROPERTY COMPILE_OPTIONS /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
add_executable(Monstey_GadbManagerParentTests tests/Monstey_GadbManagerParentTests.cpp)
target_link_libraries(Monstey_GadbManagerParentTests PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_GadbManagerParentTests PRIVATE cxx_std_20)
target_compile_options(Monstey_GadbManagerParentTests PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_GadbManagerParentTests PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_link_options(Monstey_GadbManagerParentTests PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/parent-owner-$<CONFIG>.map" /INCREMENTAL:NO)
add_test(NAME NativeSowManagerParentOwned COMMAND $<TARGET_FILE:Monstey_GadbManagerParentTests> "${CMAKE_CURRENT_BINARY_DIR}/gadb-manager-parent-owner-report.json")
set_tests_properties(NativeSowManagerParentOwned PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)

add_executable(Monstey_SowManagerParentIndependent
  tests/sow_manager_parent/Monstey_ParentCanonicalServices.cpp
  tests/sow_manager_parent/Monstey_ParentCapture.cpp
  tests/sow_manager_parent/Monstey_ParentCases.cpp
  tests/sow_manager_parent/Monstey_ParentDifferentialMain.cpp
  tests/sow_manager_parent/Monstey_ParentReferenceServices.cpp
  tests/sow_manager_parent/Monstey_ParentSession.cpp
  tests/Monstey_ParentReference.cpp
  tests/Monstey_LeaveProjectionReference.cpp
  tests/Monstey_GuardProjectionReference.cpp
  tests/Monstey_FreeReference.cpp
  tests/Monstey_BackendReference.cpp
  tests/Monstey_PlacementReference.cpp)
target_include_directories(Monstey_SowManagerParentIndependent PRIVATE include tests tests/sow_manager_parent)
target_link_libraries(Monstey_SowManagerParentIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerParentIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerParentIndependent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_SowManagerParentIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_link_options(Monstey_SowManagerParentIndependent PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/parent-independent-$<CONFIG>.map" /INCREMENTAL:NO)
add_test(NAME NativeSowManagerParentIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerParentIndependent>"
    "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
    "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
    "-DMonstey_Out=${CMAKE_CURRENT_BINARY_DIR}/parent-independent-results"
    "-DMonstey_SourceRoot=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DMonstey_BuildRoot=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMonstey_Config=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerParentIndependent.cmake")
set_tests_properties(NativeSowManagerParentIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)

add_executable(Monstey_SowManagerLeaveProjectionIndependent
  tests/Monstey_LeaveProjectionDifferential.cpp
  tests/Monstey_LeaveProjectionReference.cpp)
target_include_directories(Monstey_SowManagerLeaveProjectionIndependent PRIVATE include tests tests/sow_manager_parent)
target_link_libraries(Monstey_SowManagerLeaveProjectionIndependent PRIVATE Monstey_Data Monstey_Core)
target_compile_features(Monstey_SowManagerLeaveProjectionIndependent PRIVATE cxx_std_20)
target_compile_options(Monstey_SowManagerLeaveProjectionIndependent PRIVATE /O2 /W4 /WX /permissive- /EHsc /utf-8 /fp:strict)
set_property(TARGET Monstey_SowManagerLeaveProjectionIndependent PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
target_link_options(Monstey_SowManagerLeaveProjectionIndependent PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/leave-projection-independent-$<CONFIG>.map" /INCREMENTAL:NO)
add_test(NAME NativeSowManagerLeaveProjectionIndependent
  COMMAND "${CMAKE_COMMAND}"
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerLeaveProjectionIndependent>"
    "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
    "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
    "-DMonstey_Out=${CMAKE_CURRENT_BINARY_DIR}/leave-projection-independent-results"
    "-DMonstey_SourceRoot=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DMonstey_BuildRoot=${CMAKE_CURRENT_BINARY_DIR}"
    "-DMonstey_Config=$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowManagerLeaveProjectionIndependent.cmake")
set_tests_properties(NativeSowManagerLeaveProjectionIndependent PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)


install(FILES THIRD_PARTY_NOTICES.md DESTINATION .)
