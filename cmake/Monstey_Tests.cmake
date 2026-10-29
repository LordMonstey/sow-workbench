# Regression tests. Default tests only use synthetic inputs; tests that need
# private game files are opt-in through the MONSTEY_*_ACTUAL options below.

find_program(MONSTEY_POWERSHELL NAMES powershell.exe REQUIRED)
set(MONSTEY_TEST_BLEND_TEMPLATE "$ENV{MONSTEY_BLENDER_TEMPLATE}" CACHE FILEPATH
  "Stock Blender startup.blend for native SDNA tests")

set(out "${CMAKE_CURRENT_BINARY_DIR}")
set(fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
set(synthetic_mesh "${out}/tests/model/synthetic.mesh")
set(synthetic_skel "${out}/tests/model/synthetic.skel")

# --- Unit tests: each program receives a scratch directory -------------------

monstey_unit_test(NativeCore Monstey_CoreTests core LIBS Monstey_Core)
monstey_unit_test(NativeGadb Monstey_GadbTests gadb)
monstey_unit_test(NativeFearAudit Monstey_FearAuditTests fear)
monstey_unit_test(NativeStrings Monstey_StringsTests strings)
monstey_unit_test(NativeCatalog Monstey_CatalogTests catalog)
monstey_unit_test(NativeGadbIncoming Monstey_GadbIncomingTests gadb-incoming)
monstey_unit_test(NativeGadbCloneProject Monstey_GadbCloneProjectTests gadb-clone-project)
monstey_unit_test(NativeGadbOrdinals Monstey_GadbOrdinalTests gadb-ordinals)
monstey_unit_test(NativeRuntime Monstey_RuntimeTests runtime LIBS Monstey_Runtime ARGS "${synthetic_mesh}" "${synthetic_skel}")
monstey_unit_test(NativeHttp Monstey_HttpTests http LIBS Monstey_Runtime)
monstey_unit_test(NativeTextureApi Monstey_TextureApiTests texture-api LIBS Monstey_Runtime)

monstey_unit_test(NativeWorld Monstey_WorldTests world)
monstey_unit_test(NativeWorldComposition Monstey_WorldCompositionTests world-composition)
monstey_unit_test(NativeWorldAssets Monstey_WorldAssetsTests world-assets)
monstey_unit_test(NativeWorldLighting Monstey_WorldLightingTests world-lighting)
monstey_unit_test(NativeWorldEdit Monstey_WorldEditTests world-edit)
monstey_unit_test(NativeWorldScene Monstey_WorldSceneTests world-scene)
monstey_unit_test(NativeWorldMerge Monstey_WorldMergeTests world-merge)
monstey_unit_test(NativeWorldBlender Monstey_WorldBlenderTests world-blender ARGS ${MONSTEY_TEST_BLEND_TEMPLATE})
monstey_unit_test(NativeStaticPrefab Monstey_StaticPrefabTests static-prefab)
monstey_unit_test(NativeWdscAuxiliary Monstey_WdscAuxiliaryTests wdsc-auxiliary)
monstey_unit_test(NativePhysicsWorld Monstey_PhysicsWorldTests physics-world)
monstey_unit_test(NativeHavok Monstey_HavokTests havok)
monstey_unit_test(NativeNavigation Monstey_NavigationTests navigation)
monstey_unit_test(NativeCollision Monstey_CollisionTests collision)
monstey_unit_test(NativeTriangleCollision Monstey_TriangleCollisionTests triangle-collision)
monstey_unit_test(NativeTriangleWorld Monstey_TriangleWorldTests triangle-world)

monstey_unit_test(NativeModel Monstey_ModelTests model)
monstey_unit_test(NativeModelPalette Monstey_ModelPaletteTests model-palette)
monstey_unit_test(NativeModelBands Monstey_ModelBandsTests model-bands)
monstey_unit_test(NativeModelSockets Monstey_ModelSocketsTests model-sockets)
monstey_unit_test(NativeModelEnvelope Monstey_ModelEnvelopeTests model-envelope)
monstey_unit_test(NativeModelPreview Monstey_ModelPreviewTests model-preview ARGS "${synthetic_mesh}" "${synthetic_skel}")
monstey_unit_test(NativeModelPreviewApi Monstey_ModelPreviewApiTests model-preview-api)
monstey_unit_test(NativeModelPreviewRuntime Monstey_ModelPreviewRuntimeTests model-preview-runtime LIBS Monstey_Runtime ARGS "${synthetic_mesh}")
monstey_unit_test(NativeLegacyModel Monstey_LegacyModelTests legacy-model LIBS Monstey_Runtime ARGS "${synthetic_mesh}" "${synthetic_skel}")
monstey_unit_test(NativeAssetInspect Monstey_AssetInspectTests asset-inspect ARGS "${out}/tests/model")
monstey_unit_test(NativeBlendModel Monstey_BlendModelTests blend-model ARGS "${synthetic_mesh}" "${synthetic_skel}")
monstey_unit_test(NativeBlendGeometry Monstey_BlendGeometryTests blend-geometry)
monstey_unit_test(NativeAnimatedExtras Monstey_AnimatedExtrasTests animated-extras)

monstey_unit_test(NativeAnix Monstey_AnixTests anix)
monstey_unit_test(NativeAnixAuxiliary Monstey_AnixAuxiliaryTests anix-auxiliary)
monstey_unit_test(NativeAnixAv01 Monstey_AnixAv01Tests anix-av01)
monstey_unit_test(NativeAnixUncompressed Monstey_AnixUncompressedTests anix-uncompressed)
monstey_unit_test(NativeAnixSpecialClips Monstey_AnixSpecialClipTests anix-special-clips)
monstey_unit_test(NativeAnixEventBytes Monstey_AnixEventByteTests anix-event-bytes)
monstey_unit_test(NativeAnixGap Monstey_AnixGapTests anix-gap)
monstey_unit_test(NativeAnixCanonicalChannels Monstey_AnixCanonicalChannelTests anix-canonical-channels)
monstey_unit_test(NativeAnimationProject Monstey_AnimationProjectTests animation-project)
monstey_unit_test(NativeCurve Monstey_CurveTests curve)

monstey_unit_test(NativeFxWorkflow Monstey_FxWorkflowTests fx-workflow)
monstey_unit_test(NativeFxReflection Monstey_FxReflectionTests fx-reflection)
monstey_unit_test(NativeFxLibrary Monstey_FxLibraryTests fx-library)
monstey_unit_test(NativeFxNested Monstey_FxNestedTests fx-nested)
monstey_unit_test(NativeFxAttachment Monstey_FxAttachmentTests fx-attachment)

monstey_program(Monstey_ArchiveTests)
monstey_test(NativeArchive COMMAND Monstey_ArchiveTests)

# These compile the tested sources directly instead of linking Monstey_Data.
monstey_program(Monstey_BlendTests
  SOURCES tests/Monstey_BlendTests.cpp src/Monstey_Blend.cpp src/Monstey_Archive.cpp
  LIBS Monstey_Core Monstey_Zstd)
monstey_test(NativeBlend COMMAND Monstey_BlendTests "${out}/tests/blend")
monstey_program(Monstey_PhysicsTests SOURCES tests/Monstey_PhysicsTests.cpp src/Monstey_Physics.cpp LIBS Monstey_Core)
monstey_test(NativePhysics COMMAND Monstey_PhysicsTests "${out}/tests/physics")
monstey_program(Monstey_CurveControlTests
  SOURCES tests/Monstey_CurveControlTests.cpp src/Monstey_CurveControl.cpp
  LIBS Monstey_Data user32 gdi32 Monstey_Theme)
monstey_test(NativeCurveControl COMMAND Monstey_CurveControlTests "${out}/tests/curve-control")
monstey_program(Monstey_ModelPreviewWindowTests
  SOURCES tests/Monstey_ModelPreviewWindowTests.cpp src/Monstey_ModelPreviewWindow.cpp
  LIBS Monstey_Data user32 gdi32 comctl32 Monstey_Theme)
monstey_test(NativeModelPreviewWindow COMMAND Monstey_ModelPreviewWindowTests
  "${out}/tests/model-preview-window" "${synthetic_mesh}" "${synthetic_skel}")
add_executable(Monstey_CategoryPresentationTests
  tests/Monstey_CategoryPresentationTests.cpp
  src/app/panels/Monstey_CategoryPresentation.cpp)
target_compile_definitions(Monstey_CategoryPresentationTests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(Monstey_CategoryPresentationTests PRIVATE ${MONSTEY_CXX_FLAGS} /O2 /fp:strict)
monstey_test(NativeCategoryPresentation COMMAND Monstey_CategoryPresentationTests)

# NativeModel writes the synthetic mesh/skeleton pair the other model tests read.
set_tests_properties(NativeModel PROPERTIES FIXTURES_SETUP MonsteyNativeModelFixture)
set_tests_properties(NativeRuntime NativeBlendModel NativeLegacyModel NativeAssetInspect
  NativeModelPreview NativeModelPreviewRuntime NativeModelPreviewWindow
  PROPERTIES FIXTURES_REQUIRED MonsteyNativeModelFixture)
if(MONSTEY_TEST_BLEND_TEMPLATE)
  set_tests_properties(NativeBlendModel NativeLegacyModel NativeBlendGeometry NativeWorldBlender
    PROPERTIES ENVIRONMENT "MONSTEY_BLENDER_TEMPLATE=${MONSTEY_TEST_BLEND_TEMPLATE}")
endif()

monstey_test(NativeCli COMMAND "${MONSTEY_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
  -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_CliTests.ps1"
  -Executable "$<TARGET_FILE:Monstey_WorkbenchCli>" -OutputDirectory "${out}/tests/transports")

# --- Animation preview and LTMI material checkpoints ---------------------------

set(MONSTEY_CP017_TEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunCheckpoint017Test.cmake"
  CACHE FILEPATH "Fresh-run helper for checkpoint017 tests")
if(NOT EXISTS "${MONSTEY_CP017_TEST_RUNNER}")
  message(FATAL_ERROR "Missing checkpoint017 test runner")
endif()
set(MONSTEY_CP018_TEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tests/Monstey_RunCheckpoint018Test.cmake")

# monstey_checkpoint_test(<017|018> <name> <program> <mode> [-D<variable>=<value>...])
function(monstey_checkpoint_test checkpoint name program mode)
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}" "-DTEST_EXE=$<TARGET_FILE:${program}>"
    "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/tests/checkpoint${checkpoint}" "-DTEST_LABEL=${name}"
    "-DTEST_MODE=${mode}" ${ARGN} -P "${MONSTEY_CP${checkpoint}_TEST_RUNNER}")
  set_tests_properties(${name} PROPERTIES TIMEOUT 600 LABELS "checkpoint${checkpoint}")
endfunction()

monstey_program(Monstey_SkelDefaultPoseProbe STRICT)
monstey_program(Monstey_AnimationPreviewTests STRICT)
monstey_program(Monstey_AnimationPreviewBankPrivateTests STRICT)
monstey_program(Monstey_AnimationChannelProbe STRICT)
monstey_program(Monstey_AnimationPreviewApiTests STRICT)
monstey_program(Monstey_AnimationPreviewRuntimeTests LIBS Monstey_Runtime STRICT)
add_dependencies(Monstey_AnimationPreviewRuntimeTests Monstey_AnimationPreviewApiTests)
monstey_program(Monstey_AnimationPreviewPublicationPrivateTests STRICT)
monstey_program(Monstey_ModelLayoutAuthoringTests STRICT)
monstey_checkpoint_test(017 NativeSkelDefaultPose Monstey_SkelDefaultPoseProbe default)
monstey_checkpoint_test(017 NativeAnimationPreview Monstey_AnimationPreviewTests plain)
monstey_checkpoint_test(017 NativeAnimationPreviewBank Monstey_AnimationPreviewBankPrivateTests plain)
monstey_checkpoint_test(017 NativeAnimationPreviewApi Monstey_AnimationPreviewApiTests directory)
monstey_checkpoint_test(017 NativeAnimationPreviewRuntime Monstey_AnimationPreviewRuntimeTests runtime
  "-DSETUP_EXE=$<TARGET_FILE:Monstey_AnimationPreviewApiTests>")
monstey_checkpoint_test(017 NativeAnimationPreviewPublication Monstey_AnimationPreviewPublicationPrivateTests directory)
monstey_checkpoint_test(017 NativeModelLayoutAuthoring Monstey_ModelLayoutAuthoringTests layout
  "-DBLEND_TEMPLATE=${MONSTEY_TEST_BLEND_TEMPLATE}")

# The test #includes Monstey_Core.cpp itself, so it links neither Core nor Data.
foreach(target IN ITEMS Monstey_CorePublicationTests Monstey_CorePublicationLegacyTests)
  add_executable(${target} tests/Monstey_CorePublicationTests.cpp src/Monstey_Json.cpp)
  target_include_directories(${target} PRIVATE include)
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(${target} PRIVATE ${MONSTEY_CXX_FLAGS} /fp:strict)
  target_link_libraries(${target} PRIVATE bcrypt)
endforeach()
target_compile_definitions(Monstey_CorePublicationLegacyTests PRIVATE MONSTEY_LEGACY_WRITER)
monstey_checkpoint_test(017 NativeCorePublication Monstey_CorePublicationTests directory)
monstey_checkpoint_test(017 NativeCorePublicationLegacy Monstey_CorePublicationLegacyTests directory)

monstey_program(Monstey_LtmiBaselineTests STRICT)
monstey_program(Monstey_LtmiValueStorageTests STRICT)
monstey_program(Monstey_MaterialProjectTests LIBS Monstey_Runtime STRICT)
monstey_program(Monstey_LtmiAssetTests STRICT)
monstey_program(Monstey_AnimationHierarchyTests STRICT)
monstey_checkpoint_test(018 NativeLtmiMaterial Monstey_LtmiBaselineTests directory)
monstey_checkpoint_test(018 NativeLtmiValueStorage Monstey_LtmiValueStorageTests directory)
monstey_checkpoint_test(018 NativeMaterialProject Monstey_MaterialProjectTests project
  "-DCATALOG=${fixtures}/catalog-017.json")
monstey_checkpoint_test(018 NativeLtmiAsset Monstey_LtmiAssetTests asset
  "-DMANIFEST=${fixtures}/empty-material-manifest.json")
monstey_checkpoint_test(018 NativeAnimationHierarchy Monstey_AnimationHierarchyTests plain)

# --- Textures and theme --------------------------------------------------------

monstey_program(Monstey_TextureResourceTests LIBS Monstey_TextureCodec)
monstey_script_test(NativeTextureResource RunFreshTexture
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TextureResourceTests>" "-DOUTPUT_ROOT=${out}/tests/texture-resource")
monstey_program(Monstey_TexturePreviewWindowTests LIBS Monstey_TextureWindow STRICT)
monstey_script_test(NativeTextureWindow RunFreshWindow TIMEOUT 300 RUN_SERIAL
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TexturePreviewWindowTests>" "-DOUTPUT_ROOT=${out}/tests/texture-window")
monstey_program(Monstey_TexturePreviewWindowHoverTests LIBS Monstey_TextureWindow STRICT)
monstey_script_test(NativeTextureHover RunFreshWindow TIMEOUT 300 RUN_SERIAL
  "-DTEST_EXE=$<TARGET_FILE:Monstey_TexturePreviewWindowHoverTests>" "-DOUTPUT_ROOT=${out}/tests/texture-hover")
monstey_program(Monstey_ThemeTests LIBS Monstey_Theme)
monstey_script_test(NativeTheme RunFreshTheme TIMEOUT 60 RUN_SERIAL
  "-DTEST_EXE=$<TARGET_FILE:Monstey_ThemeTests>" "-DOUTPUT_ROOT=${out}/tests/dark-theme")

# --- World runtime probes --------------------------------------------------------

monstey_program(Monstey_WorldRuntimeTests LIBS Monstey_WorldRuntime STRICT)
monstey_program(Monstey_GadbRegistryTests)
monstey_program(Monstey_GadbNameIdentityTests)
monstey_program(Monstey_GadbRegistryBoundaryTests)
foreach(pair IN ITEMS
    NativeWorldRuntimeProbe:Monstey_WorldRuntimeTests
    NativeGadbRegistry:Monstey_GadbRegistryTests
    NativeGadbNameIdentity:Monstey_GadbNameIdentityTests
    NativeGadbRegistryBoundary:Monstey_GadbRegistryBoundaryTests)
  string(REPLACE ":" ";" pair "${pair}")
  list(GET pair 0 name)
  list(GET pair 1 program)
  monstey_script_test(${name} RunFreshReport TIMEOUT 300
    "-DTEST_EXE=$<TARGET_FILE:${program}>" "-DOUTPUT_ROOT=${out}/tests/world-database-components/${name}")
endforeach()

monstey_program(Monstey_WorldStaticComponentTests STRICT)
monstey_script_test(NativeWorldStaticComponent Monstey_RunWorldStaticComponent TIMEOUT 600
  "-DEXE=$<TARGET_FILE:Monstey_WorldStaticComponentTests>" "-DWORK_ROOT=${out}/tests/world-static-component")
monstey_program(Monstey_WorldNestedBundlesTests)
monstey_script_test(NativeWorldNestedBundles Monstey_RunWorldStaticComponent TIMEOUT 600
  "-DEXE=$<TARGET_FILE:Monstey_WorldNestedBundlesTests>" "-DWORK_ROOT=${out}/tests/world-nested-bundles")

monstey_program(Monstey_GadbDescriptorLayoutsTests STRICT)
monstey_program(Monstey_GadbClassCacheTests STRICT)
monstey_program(Monstey_GadbLayoutCounterexample STRICT)
foreach(case IN ITEMS NativeGadbDescriptorLayouts:Monstey_GadbDescriptorLayoutsTests:layout
                      NativeGadbClassCache:Monstey_GadbClassCacheTests:cache)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 program)
  list(GET case 2 mode)
  monstey_script_test(${name} Monstey_RunGadbAdvanced TIMEOUT 600
    "-DEXE=$<TARGET_FILE:${program}>" "-DMODE=${mode}" "-DWORK_ROOT=${out}/tests/gadb-advanced/${name}"
    "-DGADB_EXE=$<TARGET_FILE:Monstey_GadbTests>"
    "-DNAME_EXE=$<TARGET_FILE:Monstey_GadbNameIdentityTests>"
    "-DCASE_EXE=$<TARGET_FILE:Monstey_GadbLayoutCounterexample>")
endforeach()

monstey_program(Monstey_WorldSubmitTests LIBS Monstey_WorldRuntime STRICT)
monstey_script_test(NativeWorldSubmitContract RunFreshReport TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldSubmitTests>" "-DOUTPUT_ROOT=${out}/tests/world-submit-contract")
monstey_program(Monstey_WorldStaticPassTests LIBS Monstey_WorldRuntime STRICT)
monstey_script_test(NativeWorldStaticPass RunFreshReport TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldStaticPassTests>" "-DOUTPUT_ROOT=${out}/tests/world-static-pass")
monstey_program(Monstey_HkaiActivationTests LIBS Monstey_WorldRuntime STRICT)
monstey_script_test(NativeHkaiActivation RunFreshReport TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_HkaiActivationTests>" "-DOUTPUT_ROOT=${out}/tests/hkai-activation")

set(MONSTEY_PUBLISHED_RECEIVER_NATIVE_PROOF "" CACHE FILEPATH
  "Optional private native evidence JSON for published receiver tests")
set(MONSTEY_RECEIVER_ROWS_NATIVE_PROOF "" CACHE FILEPATH
  "Optional private native evidence JSON for receiver row tests")
monstey_program(Monstey_WorldPublishedReceiverTests LIBS Monstey_WorldRuntime STRICT)
monstey_script_test(NativeWorldPublishedReceiver Monstey_RunPublishedReceiver TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldPublishedReceiverTests>" "-DOUTPUT_ROOT=${out}/tests/published-receiver"
  "-DNATIVE_PROOF=${MONSTEY_PUBLISHED_RECEIVER_NATIVE_PROOF}")
monstey_program(Monstey_WorldReceiverRowsTests LIBS Monstey_WorldRuntime STRICT)
monstey_script_test(NativeWorldReceiverRows Monstey_RunReceiverRows TIMEOUT 180
  "-DTEST_EXE=$<TARGET_FILE:Monstey_WorldReceiverRowsTests>" "-DOUTPUT_ROOT=${out}/tests/receiver-rows"
  "-DNATIVE_PROOF=${MONSTEY_RECEIVER_ROWS_NATIVE_PROOF}")

# Command-line checks of the installed probe tool.
foreach(case IN ITEMS NativeWorldHkaiActivationCli:Monstey_RunHkaiActivationCli:hkai-activation-cli
                      NativePublishedReceiverCli:Monstey_RunPublishedReceiverCli:published-receiver-cli
                      NativeWorldReceiverRowsCli:Monstey_RunReceiverRowsCli:receiver-rows-cli)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 script)
  list(GET case 2 directory)
  monstey_script_test(${name} ${script} TIMEOUT 180
    "-DEXE=$<TARGET_FILE:Monstey_WorldRuntimeProbe>" "-DPOWERSHELL=${MONSTEY_POWERSHELL}"
    "-DOUTPUT_ROOT=${out}/tests/${directory}")
endforeach()

# --- Game database (GADB) ----------------------------------------------------------

monstey_program(Monstey_GadbWriterClassCacheTests STRICT)
monstey_script_test(NativeGadbWriterClassCache Monstey_RunGadbWriterClassCache TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbWriterClassCacheTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-writer-class-cache")
monstey_program(Monstey_GadbRuntimeCacheGateTests LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeGadbRuntimeCacheGate Monstey_RunGadbRuntimeCacheGate TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbRuntimeCacheGateTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-runtime-cache-gate"
  "-DBASELINE_CATALOG=${fixtures}/catalog-before-gadb-runtime-gate.json")
monstey_program(Monstey_GadbCacheTraceTests STRICT)
monstey_script_test(NativeGadbCacheTrace RunFreshReport TIMEOUT 300
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbCacheTraceTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-cache-trace")
monstey_program(Monstey_GadbRuntimeCacheTraceTests LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeGadbRuntimeCacheTrace Monstey_RunGadbRuntimeCacheTrace TIMEOUT 180
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbRuntimeCacheTraceTests>" "-DOUTPUT_ROOT=${out}/tests/runtime-cache-trace"
  "-DBASELINE_CATALOG=${fixtures}/catalog-222-before-cache-trace.json")
monstey_program(Monstey_GadbBankLiteralTests)
monstey_test(NativeGadbBankLiterals COMMAND Monstey_GadbBankLiteralTests)
monstey_program(Monstey_GadbBankApiTests LIBS Monstey_Runtime)
monstey_script_test(NativeGadbBankApi RunGadbBankApi TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBankApiTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-bank-api"
  "-DBASELINE_CATALOG=${fixtures}/catalog223-before-native-banks.json")

foreach(program IN ITEMS Monstey_GadbBinderCompositionTests Monstey_GadbBinderIndependentTests
    Monstey_GadbCachedValueTests Monstey_GadbCachedValueIndependentTests
    Monstey_GadbClassDeclarationTests Monstey_GadbClassRowLimitTests Monstey_ClassDeclarationIndependentTests)
  monstey_program(${program} STRICT)
endforeach()
monstey_script_test(NativeGadbBinderComposition RunBinderComposition TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBinderCompositionTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-binder"
  -DGROUPS=10 -DCASES=9395 -DCHECKS=17316)
monstey_script_test(NativeGadbBinderIndependent RunGadbBinderIndependent TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbBinderIndependentTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-binder-independent")
monstey_script_test(NativeGadbCachedValue RunQualifiedNativeReport TIMEOUT 150
  "-DEXE=$<TARGET_FILE:Monstey_GadbCachedValueTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-cached-value")
monstey_script_test(NativeGadbCachedValueIndependent RunIndependentCachedValue TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbCachedValueIndependentTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-cached-independent"
  -DGROUPS=3 -DCASES=490266 -DCHECKS=1020830)
monstey_script_test(NativeGadbClassDeclaration RunClassDeclaration TIMEOUT 150
  "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_GadbClassDeclarationTests>" "-DREPORT_DIRECTORY=${out}/tests/GadbClassDeclaration")
monstey_script_test(NativeGadbClassRowLimit RunClassRowLimit TIMEOUT 150
  "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_GadbClassRowLimitTests>" "-DREPORT_DIRECTORY=${out}/tests/GadbClassRowLimit")
monstey_script_test(NativeClassDeclarationIndependent RunClassDeclarationIndependent TIMEOUT 150
  "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_ClassDeclarationIndependentTests>"
  "-DREPORT_DIRECTORY=${out}/tests/ClassDeclarationIndependent")

monstey_program(Monstey_GadbNativeClassApiTests SOURCES tests/Monstey_GadbClassApiTests.cpp LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeGadbNativeClassApi RunNativeClassApi TIMEOUT 60
  "-DTEST_PROGRAM=$<TARGET_FILE:Monstey_GadbNativeClassApiTests>"
  "-DBASELINE_CATALOG=${fixtures}/gadb-native-class/catalog225.json"
  "-DREPORT_DIRECTORY=${out}/native-class-api-proof")
monstey_program(Monstey_GadbClassApiIndependentTests LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeGadbNativeClassApiIndependent RunIndependentClassApi TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbClassApiIndependentTests>"
  "-DREPORT_ROOT=${out}/tests/native-class-api-independent"
  "-DBASELINE=${fixtures}/gadb-native-class/catalog225.json")

# Supplied-memory getters.
monstey_program(Monstey_GadbNamePointerBoundaryTests STRICT
  SOURCES tests/Monstey_GadbNamePointerBoundaryTests.cpp
          tests/fixtures/name-pointer-predecessor/Monstey_GadbBankNameSelection.cpp
  INCLUDES tests/fixtures/name-pointer-predecessor)
foreach(program IN ITEMS Monstey_NamePointerIndependent Monstey_GadbScalarFloatTests Monstey_GadbRecordLookupTests
    Monstey_GadbStringPointerTests Monstey_GadbRemainingContractsTests)
  monstey_program(${program} STRICT)
endforeach()
monstey_script_test(NativeGadbNamePointerBoundary RunGadbGetterProbe TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbNamePointerBoundaryTests>" "-DOUT=${out}/tests/name-pointer"
  -DMODE=JSON -DGROUPS=14 -DCASES=16452 -DCHECKS=245278)
monstey_script_test(NativeGadbNamePointerIndependent RunGadbGetterProbe TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_NamePointerIndependent>" "-DOUT=${out}/tests/name-pointer-independent"
  -DMODE=TEXT "-DEXPECTED_TEXT=PASS independent_pointer groups=4 cases=6958 checks=36317")
monstey_script_test(NativeGadbScalarFloat RunGadbGetterProbe TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbScalarFloatTests>" "-DOUT=${out}/tests/scalar-float"
  -DMODE=TEXT "-DEXPECTED_TEXT=PASS scalar_float groups=10 cases=192304 checks=274348")
monstey_script_test(NativeGadbRecordLookup RunGetter TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbRecordLookupTests>" "-DOUT=${out}/tests/runtime-getters" -DGROUPS=3 -DCHECKS=68884)
monstey_script_test(NativeGadbStringPointer RunGetter TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbStringPointerTests>" "-DOUT=${out}/tests/runtime-getters" -DGROUPS=4 -DCHECKS=308)
monstey_script_test(NativeGadbRemainingContracts RunGadbGetterProbe TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbRemainingContractsTests>" "-DOUT=${out}/tests/remaining-contracts"
  -DMODE=FILE -DGROUPS=8 -DCASES=67865 -DCHECKS=133403 -DCHECK_KEY=assertions)
monstey_program(Monstey_GadbScalarIndependentTests STRICT)
monstey_script_test(NativeGadbScalarIndependent RunGadbScalarIndependent TIMEOUT 90
  "-DTEST_EXE=$<TARGET_FILE:Monstey_GadbScalarIndependentTests>" "-DOUTPUT_ROOT=${out}/tests/gadb-scalar-independent")
monstey_program(Monstey_GadbLookupStringIndependent STRICT)
monstey_script_test(Monstey_GadbLookupStringIndependent RunLookupStringIndependent TIMEOUT 75
  "-DEXE=$<TARGET_FILE:Monstey_GadbLookupStringIndependent>" "-DOUTPUT_ROOT=${out}/gadb-lookup-string-independent")
monstey_program(Monstey_GadbRemainingContractsIndependentTests SOURCES tests/Monstey_RemainingIndependent.cpp STRICT)
monstey_script_test(Monstey_GadbRemainingContractsIndependent RunRemainingIndependent TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_GadbRemainingContractsIndependentTests>" "-DOUT=${out}/tests/remaining-count-independent")

# Count consumers.
monstey_program(Monstey_GadbReferenceWordTests STRICT)
monstey_script_test(NativeGadbReferenceWord RunReferenceWord TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_GadbReferenceWordTests>" "-DOUT=${out}/reference-word-results")
monstey_program(Monstey_ReferenceIndependent OPTIMIZED)
monstey_script_test(Monstey.ReferenceGetterIndependent RunReferenceIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_ReferenceIndependent>" "-DOUT=${out}/reports")
monstey_program(Monstey_GadbAudioNextKeyTests OPTIMIZED)
monstey_script_test(Monstey.GadbAudioNextKey RunAudioNextKey TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbAudioNextKeyTests>" "-DREPORT_DIR=${out}/reports")
monstey_program(Monstey_GadbNextKeyIndependent SOURCES tests/Monstey_NextKeyIndependent.cpp STRICT)
monstey_script_test(Monstey_GadbNextKeyIndependent RunNextKeyIndependent TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_GadbNextKeyIndependent>" "-DOUT=${out}/nextkey-independent-results")
monstey_program(Monstey_GadbAudioActionWordTests SOURCES tests/Monstey_AudioActionWordTests.cpp STRICT)
monstey_script_test(NativeGadbAudioActionWord RunAudioActionWord
  "-DEXE=$<TARGET_FILE:Monstey_GadbAudioActionWordTests>" "-DOUTPUT_DIR=${out}/audio-action-test-output")
monstey_program(Monstey_AudioIndependentTests STRICT)
monstey_script_test(NativeGadbAudioActionIndependent RunAudioIndependent
  "-DEXE=$<TARGET_FILE:Monstey_AudioIndependentTests>" "-DOUTPUT_ROOT=${out}/gadb-audio-independent")
monstey_program(Monstey_GadbSelectedCountWordTests SOURCES tests/Monstey_SelectedCountWordTests.cpp STRICT)
monstey_script_test(NativeGadbSelectedCountWord RunSelectedCountWord
  "-DEXE=$<TARGET_FILE:Monstey_GadbSelectedCountWordTests>" "-DOUTPUT_DIR=${out}/selected-count-test-output")
monstey_program(Monstey_SelectedCountIndependent OPTIMIZED)
monstey_script_test(Monstey.SelectedCountIndependent RunSelectedCountIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_SelectedCountIndependent>" "-DOUT=${out}/reports")
monstey_program(Monstey_GadbIntegerWordTests STRICT)
monstey_script_test(NativeGadbIntegerWord RunInteger TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_GadbIntegerWordTests>" "-DOUT=${out}/integer-results")
monstey_program(Monstey_IntegerIndependent OPTIMIZED)
monstey_script_test(Monstey.IntegerWordIndependent RunIntegerIndependent TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_IntegerIndependent>" "-DOUT=${out}/integer-independent-results")
monstey_program(Monstey_GadbBankActivationTests OPTIMIZED)
monstey_script_test(Monstey.GadbBankActivation RunBankActivation TIMEOUT 120 RUN_SERIAL
  "-DEXE=$<TARGET_FILE:Monstey_GadbBankActivationTests>" "-DOUT=${out}/reports")
monstey_program(Monstey_BankActivationIndependent OPTIMIZED)
monstey_script_test(Monstey.BankActivationIndependent RunBankActivationIndependent TIMEOUT 120 RUN_SERIAL
  "-DEXE=$<TARGET_FILE:Monstey_BankActivationIndependent>" "-DOUT=${out}/reports")
monstey_program(Monstey_GadbValueFixupTests OPTIMIZED)
monstey_script_test(NativeGadbValueFixup RunGadbValueFixup TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbValueFixupTests>" "-DOUT=${out}/value-fixup-reports")
monstey_program(Monstey_GadbValueFixupIndependent STRICT)
monstey_script_test(NativeGadbValueFixupIndependent RunGadbValueFixupIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbValueFixupIndependent>" "-DOUT=${out}/fixup-independent-reports")

# Records, pools, publication and categories.
monstey_program(Monstey_AtomUsesTests OPTIMIZED STATIC_CRT)
monstey_test(NativeGadbAtomUses TIMEOUT 60 LABELS native sow database synthetic
  COMMAND Monstey_AtomUsesTests "${out}/atomuses-synthetic")
foreach(case IN ITEMS
    GadbPoolRelocation:pool-relocation-reports
    GadbAnonymousRecord:anonymous-record-reports)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 stem)
  list(GET case 1 directory)
  monstey_program(Monstey_${stem}Tests OPTIMIZED STATIC_CRT)
  monstey_script_test(Native${stem} Run${stem} TIMEOUT 120
    "-DEXE=$<TARGET_FILE:Monstey_${stem}Tests>" "-DOUT=${out}/${directory}")
endforeach()
monstey_program(Monstey_SowPoolIndependent OPTIMIZED STATIC_CRT INCLUDES tests
  SOURCES tests/Monstey_SowPoolProductAdapter010.cpp tests/Monstey_SowPoolIndependent010.cpp)
monstey_script_test(NativeSowPoolIndependent RunSowPoolIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_SowPoolIndependent>" "-DOUT=${out}/sow-pool-independent-reports")
monstey_program(Monstey_SowAnonymousIndependent OPTIMIZED STATIC_CRT INCLUDES tests
  SOURCES tests/Monstey_AnonymousProductAdapter003.cpp tests/Monstey_AnonymousIndependent003.cpp)
monstey_script_test(NativeSowAnonymousIndependent RunSowAnonymousIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_SowAnonymousIndependent>" "-DOUT=${out}/anonymous-independent-reports")
monstey_program(Monstey_SowAtomRuntimeTests LIBS Monstey_Runtime OPTIMIZED STATIC_CRT)
monstey_script_test(Monstey_SowAtomRuntime RunSowAtomRuntime
  "-DEXE=$<TARGET_FILE:Monstey_SowAtomRuntimeTests>" "-DCATALOG=${CMAKE_CURRENT_SOURCE_DIR}/tests/sow-atom/catalog226.json"
  "-DOUTPUT_ROOT=${out}/tests/sow-atom-runtime")

foreach(case IN ITEMS
    NativeGadbBufferedStream:GadbBufferedStream:buffered-stream-reports:120
    NativeGadbConcreteRecords:GadbConcreteRecords:concrete-record-reports:120
    NativeGadbEncodedString:GadbEncodedString:encoded-string-reports:120
    NativeSowGadbCategoryLookup:GadbCategoryLookup:category-lookup-results:60
    NativeGadbBankSelectorRegisters:GadbBankSelectorRegisters:bank-selector-registers-reports:120
    NativeGadbCategoryReader:GadbCategoryReader:category-reader-reports:120)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 stem)
  list(GET case 2 directory)
  list(GET case 3 timeout)
  string(REGEX REPLACE "^Native" "Run" script "${name}")
  monstey_program(Monstey_${stem}Tests OPTIMIZED)
  monstey_script_test(${name} ${script} TIMEOUT ${timeout}
    "-DEXE=$<TARGET_FILE:Monstey_${stem}Tests>" "-DOUT=${out}/${directory}")
endforeach()
monstey_program(Monstey_GadbCategoryCompositionIndependent OPTIMIZED INCLUDES tests/sow-category-independent
  SOURCES tests/sow-category-independent/Monstey_CategoryReference.cpp
          tests/sow-category-independent/Monstey_CategoryConcreteBindings.cpp
          tests/sow-category-independent/Monstey_CategoryCompositionIndependentFinal.cpp)
monstey_script_test(NativeSowCategoryCompositionIndependent RunSowCategoryCompositionIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbCategoryCompositionIndependent>" "-DOUT=${out}/sow-category-independent-results")

foreach(case IN ITEMS
    NativeSowGadbPublication:Monstey_GadbPublicationTests:RunGadbPublication:publication-owner-reports
    NativeSowGadbPublicationFramed:Monstey_GadbPublicationFramedTests:RunGadbPublicationFramed:owner-reports
    NativeSowGadbPublicationFramedProjection:Monstey_GadbPublicationFramedProjectionTests:RunGadbPublicationFramed:projection-reports)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 program)
  list(GET case 2 script)
  list(GET case 3 directory)
  monstey_program(${program} OPTIMIZED STATIC_CRT)
  monstey_script_test(${name} ${script} TIMEOUT 60 "-DEXE=$<TARGET_FILE:${program}>" "-DOUT=${out}/${directory}")
endforeach()
monstey_program(Monstey_GadbPublicationIndependent OPTIMIZED STATIC_CRT INCLUDES tests/sow-publication-independent
  SOURCES tests/sow-publication-independent/Monstey_PublicationReference.cpp tests/Monstey_PublicationIndependent.cpp)
monstey_script_test(NativeSowPublicationIndependent RunSowPublicationIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbPublicationIndependent>" "-DOUT=${out}/publication-independent-results")
foreach(case IN ITEMS Framed:framed FrameExceptions:frame-exceptions)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 stem)
  list(GET case 1 directory)
  monstey_program(Monstey_GadbPublication${stem}Independent OPTIMIZED STATIC_CRT
    INCLUDES tests/sow-publication-framed-independent
    SOURCES tests/sow-publication-framed-independent/Monstey_PublicationFramedReference.cpp
            tests/Monstey_Publication${stem}Independent.cpp)
  monstey_script_test(NativeSowPublication${stem}Independent RunSowPublicationFramedIndependent TIMEOUT 120
    "-DEXE=$<TARGET_FILE:Monstey_GadbPublication${stem}Independent>"
    "-DOUT=${out}/publication-${directory}-independent-results")
endforeach()

# Database manager: each owned test has an independent differential peer.
foreach(stem IN ITEMS Primitives Placement Backend Free GuardProjection)
  monstey_program(Monstey_GadbManager${stem}Tests OPTIMIZED STATIC_CRT)
endforeach()
monstey_program(Monstey_GadbManagerLeaveProjectionTests SOURCES tests/Monstey_LeaveProductOwned.cpp OPTIMIZED STATIC_CRT)
monstey_program(Monstey_GadbManagerParentTests OPTIMIZED STATIC_CRT)
monstey_test(NativeSowManagerPrimitivesOwned TIMEOUT 120 COMMAND $<TARGET_FILE:Monstey_GadbManagerPrimitivesTests>)
monstey_test(NativeSowManagerPlacementOwned TIMEOUT 120 COMMAND $<TARGET_FILE:Monstey_GadbManagerPlacementTests>)
monstey_test(NativeSowManagerBackendOwned TIMEOUT 120 RUN_SERIAL COMMAND $<TARGET_FILE:Monstey_GadbManagerBackendTests>)
foreach(case IN ITEMS Free:free GuardProjection:guard-projection LeaveProjection:leave Parent:parent)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 stem)
  list(GET case 1 report)
  monstey_test(NativeSowManager${stem}Owned TIMEOUT 120 RUN_SERIAL
    COMMAND $<TARGET_FILE:Monstey_GadbManager${stem}Tests> "${out}/gadb-manager-${report}-owner-report.json")
endforeach()
foreach(case IN ITEMS LeaveProjection:leave-owner Parent:parent-owner)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 stem)
  list(GET case 1 map)
  target_link_options(Monstey_GadbManager${stem}Tests PRIVATE "/MAP:${out}/${map}-$<CONFIG>.map" /INCREMENTAL:NO)
endforeach()

monstey_program(Monstey_GadbManagerPrimitivesIndependent OPTIMIZED STATIC_CRT
  INCLUDES tests/sow-manager-primitives-independent
  SOURCES tests/sow-manager-primitives-independent/Monstey_ManagerPrimitiveReference.cpp
          tests/Monstey_ManagerPrimitiveIndependent.cpp)
monstey_script_test(NativeSowGadbManagerPrimitivesIndependent RunSowManagerPrimitiveIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_GadbManagerPrimitivesIndependent>" "-DOUT=${out}/manager-primitives-independent-results")
monstey_program(Monstey_SowManagerPlacementIndependent OPTIMIZED STATIC_CRT INCLUDES include tests
  SOURCES tests/Monstey_SowManagerPlacementIndependent.cpp tests/Monstey_PlacementReference.cpp)
monstey_script_test(NativeSowManagerPlacementIndependent RunSowManagerPlacementIndependent TIMEOUT 120 RUN_SERIAL
  "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerPlacementIndependent>"
  "-DMonstey_ReportDirectory=${out}/sow-manager-placement-independent-results")
monstey_program(Monstey_SowManagerBackendIndependent OPTIMIZED STATIC_CRT INCLUDES include tests
  SOURCES tests/Monstey_BackendDifferential.cpp tests/Monstey_BackendReference.cpp tests/Monstey_PlacementReference.cpp)
monstey_script_test(NativeSowManagerBackendIndependent RunSowManagerBackendIndependent TIMEOUT 120 RUN_SERIAL
  "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerBackendIndependent>" "-DMonstey_Out=${out}/backend-independent-results")
monstey_program(Monstey_SowManagerFreeIndependent OPTIMIZED STATIC_CRT INCLUDES include tests
  SOURCES tests/Monstey_FreeActualIndependent.cpp tests/Monstey_FreeReference.cpp
          tests/Monstey_BackendReference.cpp tests/Monstey_PlacementReference.cpp)
monstey_script_test(NativeSowManagerFreeIndependent RunSowManagerFreeIndependent TIMEOUT 120 RUN_SERIAL
  "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManagerFreeIndependent>"
  "-DMonstey_Out=${out}/free-independent-results"
  "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
  "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
  "-DMonstey_Build=${out}"
  "-DMonstey_ObjectRoot=${out}/Monstey_SowManagerFreeIndependent.dir/$<CONFIG>"
  "-DMonstey_DataObjectRoot=${out}/Monstey_Data.dir/$<CONFIG>"
  "-DMonstey_FreeCPP=${CMAKE_CURRENT_SOURCE_DIR}/src/Monstey_GadbManagerFree.cpp"
  "-DMonstey_FreeHPP=${CMAKE_CURRENT_SOURCE_DIR}/include/Monstey_GadbManagerFree.hpp")
monstey_program(Monstey_SowManagerGuardProjectionIndependent OPTIMIZED STATIC_CRT INCLUDES include tests
  SOURCES tests/Monstey_GuardProjectionDifferential.cpp tests/Monstey_GuardProjectionReference.cpp)
target_link_options(Monstey_SowManagerGuardProjectionIndependent PRIVATE
  "/MAP:${out}/guard-projection-independent-$<CONFIG>.map")
monstey_program(Monstey_SowManagerParentIndependent OPTIMIZED STATIC_CRT INCLUDES include tests tests/sow_manager_parent
  SOURCES
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
target_link_options(Monstey_SowManagerParentIndependent PRIVATE
  "/MAP:${out}/parent-independent-$<CONFIG>.map" /INCREMENTAL:NO)
monstey_program(Monstey_SowManagerLeaveProjectionIndependent OPTIMIZED STATIC_CRT
  INCLUDES include tests tests/sow_manager_parent
  SOURCES tests/Monstey_LeaveProjectionDifferential.cpp tests/Monstey_LeaveProjectionReference.cpp)
target_link_options(Monstey_SowManagerLeaveProjectionIndependent PRIVATE
  "/MAP:${out}/leave-projection-independent-$<CONFIG>.map" /INCREMENTAL:NO)
foreach(case IN ITEMS GuardProjection:sow-manager-guard-projection-independent-results:120
                      Parent:parent-independent-results:180
                      LeaveProjection:leave-projection-independent-results:180)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 stem)
  list(GET case 1 directory)
  list(GET case 2 timeout)
  monstey_script_test(NativeSowManager${stem}Independent RunSowManager${stem}Independent
    TIMEOUT ${timeout} RUN_SERIAL
    "-DMonstey_Exe=$<TARGET_FILE:Monstey_SowManager${stem}Independent>"
    "-DMonstey_Data=$<TARGET_FILE:Monstey_Data>"
    "-DMonstey_Core=$<TARGET_FILE:Monstey_Core>"
    "-DMonstey_Out=${out}/${directory}"
    "-DMonstey_SourceRoot=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DMonstey_BuildRoot=${out}"
    "-DMonstey_Config=$<CONFIG>")
endforeach()

# --- Legacy FEAR database ------------------------------------------------------------

monstey_program(Monstey_FearDbPrimitiveTests STRICT)
monstey_script_test(NativeFearDbPrimitives RunFearDbPrimitives TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearDbPrimitiveTests>" "-DOUTPUT_ROOT=${out}/fear-primitive-results")
monstey_program(Monstey_FearReaderIndependent STRICT)
monstey_script_test(NativeFearReaderIndependent RunFearReaderIndependent TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_FearReaderIndependent>" "-DOUTPUT_ROOT=${out}/fear-reader-independent")

monstey_program(Monstey_FearDbPackedTests OPTIMIZED)
monstey_program(Monstey_PackedNameRescanTests OPTIMIZED)
monstey_program(Monstey_FearResourceTests SOURCES tests/Monstey_FearDbResourceTests.cpp OPTIMIZED)
monstey_program(Monstey_ResourcePackedAliasTests OPTIMIZED)
monstey_program(Monstey_FearNamesTests SOURCES tests/Monstey_FearDbNameTests.cpp OPTIMIZED)
monstey_program(Monstey_FearDbOpenTests OPTIMIZED)
monstey_program(Monstey_OpenPackedAliasTests OPTIMIZED)
foreach(case IN ITEMS
    NativeFearDbPacked:Monstey_FearDbPackedTests:RunFearDbPacked:fear-packed-results
    NativeFearPackedNameRescan:Monstey_PackedNameRescanTests:RunPackedNameRescan:fear-packed-results
    NativeFearDbResources:Monstey_FearResourceTests:RunFearDbResources:fear-resources-results
    NativeFearResourcePackedAlias:Monstey_ResourcePackedAliasTests:RunResourcePackedAlias:fear-resources-results
    NativeFearDbNames:Monstey_FearNamesTests:RunFearDbNames:fear-names-results
    NativeFearDbOpen:Monstey_FearDbOpenTests:RunFearDbOpen:fear-open-results
    NativeFearOpenPackedAlias:Monstey_OpenPackedAliasTests:RunOpenPackedAlias:fear-open-results)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 program)
  list(GET case 2 script)
  list(GET case 3 directory)
  monstey_script_test(${name} ${script} TIMEOUT 120
    "-DEXE=$<TARGET_FILE:${program}>" "-DOUTPUT_ROOT=${out}/${directory}")
endforeach()

monstey_program(Monstey_PackedEpochIndependent OPTIMIZED)
monstey_program(Monstey_FearPackedIndependent OPTIMIZED)
monstey_script_test(Monstey.FearPackedEpochIndependent RunFearPackedIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_PackedEpochIndependent>" "-DMODE=epoch" "-DOUT=${out}/packed-independent")
monstey_script_test(Monstey.FearPackedLayoutIndependent RunFearPackedIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearPackedIndependent>" "-DMODE=layout" "-DOUT=${out}/packed-independent")
monstey_program(Monstey_FearResourceIndependent STRICT)
monstey_script_test(NativeFearResourceIndependent RunFearResourceIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearResourceIndependent>" "-DOUTPUT_ROOT=${out}/fear-resource-independent")
monstey_program(Monstey_FearNamesIndependent OPTIMIZED)
monstey_script_test(Monstey.FearNamesIndependent RunFearNamesIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearNamesIndependent>" "-DOUT=${out}/names-independent")
monstey_program(Monstey_FearOpenIndependent OPTIMIZED)
monstey_script_test(Monstey.FearOpenIndependent RunFearOpenIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearOpenIndependent>" "-DOUT=${out}/open-independent")

foreach(program IN ITEMS Monstey_CrtComparisonTests Monstey_CrtRegionalTests Monstey_CrtMemoryContractTests)
  monstey_program(${program} STRICT)
  monstey_test(${program} COMMAND ${program})
endforeach()
monstey_program(Monstey_CrtRootIndependent STRICT)
monstey_script_test(NativeFearCrtIndependent RunCrtRootIndependent TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_CrtRootIndependent>")

monstey_program(Monstey_FearInspectorIndependent LIBS Monstey_Core STRICT)
monstey_script_test(NativeFearDbInspectorIndependent RunFearDbInspectorIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearInspectorIndependent>" "-DCLI=$<TARGET_FILE:Monstey_FearDbInspector>"
  "-DOUTPUT_ROOT=${out}/fear-db-inspector-independent-results")
monstey_program(Monstey_FearDbInspectorTests STRICT)
monstey_script_test(NativeFearDbInspectorCli RunInspector TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearDbInspectorTests>" "-DCLI=$<TARGET_FILE:Monstey_FearDbInspector>"
  "-DOUTPUT_ROOT=${out}/fear-db-inspector-owner-results")

# Constraint reader and callable table.
monstey_program(Monstey_FearConstraintTests STRICT)
monstey_test(NativeFearConstraints TIMEOUT 120 COMMAND Monstey_FearConstraintTests)
monstey_program(Monstey_FearConstraintCliTests STRICT)
monstey_script_test(NativeFearConstraintCli RunConstraintCli TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearConstraintCliTests>" "-DCLI=$<TARGET_FILE:Monstey_FearConstraintInspect>"
  "-DOUTPUT_ROOT=${out}/cli-tests")
foreach(stem IN ITEMS Independent StatefulRegression)
  monstey_program(Monstey_FearConstraint${stem} STRICT)
  monstey_script_test(NativeFearConstraint${stem} RunConstraintIndependent TIMEOUT 120
    "-DEXE=$<TARGET_FILE:Monstey_FearConstraint${stem}>" "-DOUTPUT_ROOT=${out}/constraint-${stem}")
endforeach()
monstey_program(Monstey_FearConstraintV002Independent STRICT)
monstey_script_test(NativeFearConstraintV002Independent RunConstraintV002Independent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearConstraintV002Independent>" "-DOUTPUT_ROOT=${out}/constraint-v002-independent"
  -DEXPECTED_GROUPS=8 -DEXPECTED_CASES=2073 -DEXPECTED_CHECKS=8648 -DEXPECTED_FAULTS=1596)
# Header-only callable table: no library at all.
foreach(program IN ITEMS Monstey_FearDbCallableTests Monstey_FearCallableIndependent)
  add_executable(${program} tests/${program}.cpp)
  target_include_directories(${program} PRIVATE include)
  target_compile_options(${program} PRIVATE ${MONSTEY_CXX_FLAGS} /fp:strict)
endforeach()
monstey_test(NativeFearDbCallable TIMEOUT 120 COMMAND Monstey_FearDbCallableTests)
monstey_script_test(NativeFearCallableIndependent RunConstraintV002Independent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearCallableIndependent>" "-DOUTPUT_ROOT=${out}/callable-independent"
  -DEXPECTED_GROUPS=5 -DEXPECTED_CASES=5 -DEXPECTED_CHECKS=16)

# Unpacked database: line, materializer, cleanup, validator, open, callbacks.
foreach(program IN ITEMS Monstey_FearUnpackedLineTests Monstey_FearUnpackedLineIndependent
    Monstey_FearUnpackedLineTargetTests Monstey_LineV002Independent Monstey_FearUnpackedMaterializeTests
    Monstey_MaterializerCtorArgumentRegression Monstey_FearUnpackedCleanupTests
    Monstey_FearCleanupLifetimeRegression Monstey_FearCleanupIndependent Monstey_FearUnpackedValidateTests
    Monstey_ValidateIndependent Monstey_FearUnpackedOpenTests Monstey_FearUnpackedOpenIndependent
    Monstey_FearUnpackedCallbackTests Monstey_CallbackIndependent Monstey_LtaDependencyTests
    Monstey_LtaIndependent Monstey_FearComparatorLifetimeTests Monstey_UnpackedInspectorTests)
  monstey_program(${program} OPTIMIZED STATIC_CRT)
endforeach()
monstey_script_test(NativeFearUnpackedLine RunFearUnpackedLine TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineTests>" "-DOUT=${out}/unpacked-line-results")
monstey_script_test(NativeFearUnpackedLineIndependent RunFearUnpackedLineIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineIndependent>" "-DOUT=${out}/unpacked-line-independent-results")
monstey_script_test(NativeFearUnpackedLineTargets RunFearUnpackedLineIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedLineTargetTests>" "-DOUT=${out}/unpacked-line-target-results")
monstey_script_test(NativeLineV002Independent RunLineV002Independent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_LineV002Independent>" "-DOUT=${out}/line-v002-independent")
monstey_script_test(NativeFearUnpackedMaterialize RunFearUnpackedMaterialize TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedMaterializeTests>" "-DOUTPUT_ROOT=${out}/fear-unpacked-materialize")
monstey_script_test(NativeFearMaterializerCtorArguments RunMaterializerCtorArgument TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_MaterializerCtorArgumentRegression>" "-DOUTPUT_ROOT=${out}/fear-unpacked-materialize")
foreach(case IN ITEMS
    NativeMaterializeLinksIndependent:Monstey_MaterializeLinkIndependent:4:9050:18100
    NativeMaterializeSizeIndependent:Monstey_MaterializeSizeIndependent:2:1028:6168
    NativeMaterializeCtorDeltaIndependent:Monstey_MaterializeCtorDeltaIndependent:8:76:495)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 name)
  list(GET case 1 source)
  list(GET case 2 groups)
  list(GET case 3 cases)
  list(GET case 4 checks)
  monstey_program(${name} SOURCES tests/${source}.cpp OPTIMIZED STATIC_CRT)
  monstey_script_test(${name} RunMaterializeV002Independent TIMEOUT 120
    "-DEXE=$<TARGET_FILE:${name}>" "-DOUTPUT_ROOT=${out}/${name}-results"
    "-DEXPECTED_GROUPS=${groups}" "-DEXPECTED_CASES=${cases}" "-DEXPECTED_CHECKS=${checks}")
endforeach()
monstey_test(NativeFearUnpackedCleanup TIMEOUT 60 COMMAND Monstey_FearUnpackedCleanupTests)
monstey_script_test(NativeFearCleanupLifetime RunCleanupLifetime TIMEOUT 60
  "-DEXE=$<TARGET_FILE:Monstey_FearCleanupLifetimeRegression>" "-DOUTPUT_ROOT=${out}/cleanup-lifetime")
monstey_script_test(NativeFearCleanupIndependent RunCleanupIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearCleanupIndependent>" "-DOUT=${out}/cleanup-independent-results")
monstey_script_test(NativeFearUnpackedValidate RunFearUnpackedValidate TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedValidateTests>" "-DOUT=${out}/unpacked-validate-results")
monstey_script_test(NativeFearUnpackedValidateIndependent RunFearUnpackedValidateIndependent TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_ValidateIndependent>" "-DOUTPUT_ROOT=${out}/fear-unpacked-validate-independent")
monstey_script_test(NativeFearUnpackedOpen RunFearUnpackedOpen TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedOpenTests>" "-DOUTPUT_ROOT=${out}/fear-unpacked-open")
monstey_script_test(NativeFearUnpackedOpenIndependent RunFearUnpackedOpenIndependent TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedOpenIndependent>" "-DOUTPUT_ROOT=${out}/open-independent-synthetic")
monstey_script_test(NativeFearUnpackedCallback RunFearUnpackedCallback TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedCallbackTests>" "-DOUT=${out}/unpacked-callback-results")
monstey_script_test(NativeFearCallbackIndependent RunFearCallbackIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_CallbackIndependent>" "-DOUT=${out}/callback-independent-results")
monstey_script_test(NativeFearSidecarLta RunSidecarSynthetic TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_LtaDependencyTests>" "-DOUTPUT_ROOT=${out}/lta-owner-results"
  -DKIND=Lta -DGROUPS=5 -DCASES=282 -DCHECKS=282)
monstey_script_test(NativeLtaIndependent RunLtaIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_LtaIndependent>" "-DOUT=${out}/lta-independent-results")
monstey_script_test(NativeFearComparatorLifetime RunFearComparatorLifetime TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_FearComparatorLifetimeTests>" "-DOUTPUT_ROOT=${out}/comparator-lifetime-synthetic")
foreach(case IN ITEMS Ordinary:Monstey_LogicalMapsIndependent:5765:3966919
                      Epoch:Monstey_LogicalMapsEpochIndependent:15:1604)
  string(REPLACE ":" ";" case "${case}")
  list(GET case 0 kind)
  list(GET case 1 source)
  list(GET case 2 cases)
  list(GET case 3 checks)
  monstey_program(Monstey_LogicalMaps${kind} SOURCES tests/${source}.cpp OPTIMIZED STATIC_CRT)
  monstey_script_test(NativeFearLogicalMaps${kind} RunLogicalMapsIndependent TIMEOUT 120
    "-DEXE=$<TARGET_FILE:Monstey_LogicalMaps${kind}>" "-DOUTPUT_ROOT=${out}/logicalmap-results/${kind}"
    "-DEXPECTED_CASES=${cases}" "-DEXPECTED_CHECKS=${checks}")
endforeach()
monstey_program(Monstey_ComparatorIndependent SOURCES tests/Monstey_ComparatorAddressIndependent.cpp OPTIMIZED STATIC_CRT)
target_compile_definitions(Monstey_ComparatorIndependent PRIVATE MONSTEY_BIND_PRODUCT=1)
monstey_script_test(NativeFearComparatorIndependent RunComparatorIndependent TIMEOUT 120
  "-DEXE=$<TARGET_FILE:Monstey_ComparatorIndependent>" "-DOUTPUT_ROOT=${out}/comparator-independent-results")
monstey_script_test(NativeFearUnpackedOwned RunFearUnpackedOwned TIMEOUT 110
  "-DEXE=$<TARGET_FILE:Monstey_UnpackedInspectorTests>" "-DOUTPUT_ROOT=${out}/fear-unpacked-owned")
monstey_script_test(NativeFearUnpackedInspectorCli RunFearUnpackedInspectorCli TIMEOUT 160
  "-DEXE=$<TARGET_FILE:Monstey_FearUnpackedInspector>" "-DTEST_EXE=$<TARGET_FILE:Monstey_UnpackedInspectorTests>"
  "-DOUTPUT_ROOT=${out}/fear-unpacked-cli")
monstey_program(Monstey_UnpackedInspectorFixtures SOURCES tests/Monstey_UnpackedInspectorFixtureGenerator.cpp
  LIBS Monstey_Core OPTIMIZED STATIC_CRT)
monstey_program(Monstey_UnpackedInspectorIndependent SOURCES tests/Monstey_UnpackedInspectorExpanded.cpp
  OPTIMIZED STATIC_CRT)
monstey_script_test(NativeFearUnpackedInspectorIndependent RunUnpackedInspectorIndependent TIMEOUT 120
  "-DGEN=$<TARGET_FILE:Monstey_UnpackedInspectorFixtures>" "-DEXE=$<TARGET_FILE:Monstey_UnpackedInspectorIndependent>"
  "-DOUTPUT_ROOT=${out}/unpacked-independent-results")

# --- Navigation ------------------------------------------------------------------------

monstey_program(Monstey_NavigationBoundsTests STRICT)
monstey_script_test(NativeNavigationBounds RunNavigationBounds
  "-DEXE=$<TARGET_FILE:Monstey_NavigationBoundsTests>" "-DOUTPUT_ROOT=${out}/tests/navigation-bounds")
monstey_program(Monstey_StaticNavFaceBoundsTests STRICT)
monstey_test(NativeStaticNavFaceBounds COMMAND Monstey_StaticNavFaceBoundsTests)
monstey_program(Monstey_NavAuthorCompatibilityTests STRICT)
monstey_test(NativeNavAuthorCompatibility COMMAND Monstey_NavAuthorCompatibilityTests candidate)

# Earlier owned implementations, kept as test-only regression references.
add_library(Monstey_NavUnusedFacePreimages STATIC
  tests/fixtures/navigation-cooker-preimages/Monstey_NavCompactOptimizerCoreBaseline.cpp
  tests/fixtures/navigation-cooker-preimages/Monstey_NavFinalizeHierarchyBaseline.cpp
  tests/fixtures/navigation-cooker-preimages/Monstey_StaticNavFaceBoundsBaseline.cpp)
target_link_libraries(Monstey_NavUnusedFacePreimages PRIVATE Monstey_Data)
target_compile_options(Monstey_NavUnusedFacePreimages PRIVATE /fp:strict)

foreach(stem IN ITEMS NavigationBoundaryQuery NavigationStreaming NavigationStreamingIndependent)
  monstey_program(Monstey_${stem}Tests STRICT)
  monstey_script_test(Native${stem} RunQualifiedNativeReport TIMEOUT 90
    "-DEXE=$<TARGET_FILE:Monstey_${stem}Tests>" "-DOUTPUT_ROOT=${out}/tests/${stem}")
endforeach()
monstey_program(Monstey_BoundaryIndependentReview STRICT)
monstey_script_test(NativeNavigationBoundaryIndependent RunQualifiedNativeReport TIMEOUT 90
  "-DEXE=$<TARGET_FILE:Monstey_BoundaryIndependentReview>" "-DOUTPUT_ROOT=${out}/tests/NavigationBoundaryIndependent"
  -DFIRST_ARGUMENT=--synthetic)

# Seam matcher, UID registration and compact-tree cooker. Test name = Native<program minus Monstey_ and Tests>.
foreach(program IN ITEMS
    Monstey_NavSeamBroadphaseTests Monstey_NavSeamGeometryTests Monstey_NavSeamGroundTests
    Monstey_NavSeamAlternateTests Monstey_NavSeamMatcherTests Monstey_NavSeamAllRoutesTests
    Monstey_NavSeamFaceNormalReferenceTests Monstey_GroundSeamIndependentTests Monstey_NavSeamFullIndependentTests
    Monstey_NavigationRegistrationTests Monstey_NavigationRegistrationIndependentTests
    Monstey_NavCompactCookerStageTests Monstey_NavCompactOptimizerCoreTests Monstey_NavCompactCookerIntegrationTests
    Monstey_NavCompactStageIndependentTests Monstey_NavCompactCoreIndependentTests
    Monstey_NavCompactSortCentroidsTests Monstey_NavFinalizeHierarchyTests
    Monstey_UnusedFaceFidelityTests Monstey_UnusedFaceIndependentTests)
  if(program MATCHES "^Monstey_NavigationRegistration")
    monstey_program(${program})
  elseif(program MATCHES "^Monstey_UnusedFace")
    monstey_program(${program} LIBS Monstey_Data Monstey_NavUnusedFacePreimages STRICT)
  else()
    monstey_program(${program} STRICT)
  endif()
  string(REGEX REPLACE "^Monstey_(.*)Tests$" "Native\\1" name "${program}")
  monstey_script_test(${name} RunQualifiedNativeReport TIMEOUT 90
    "-DEXE=$<TARGET_FILE:${program}>" "-DOUTPUT_ROOT=${out}/tests/${name}")
endforeach()

monstey_program(Monstey_NavStaticTreeApiTests LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeNavStaticTreeApi RunNavStaticTreeApi TIMEOUT 150
  "-DTEST_EXE=$<TARGET_FILE:Monstey_NavStaticTreeApiTests>" "-DOUTPUT_ROOT=${out}/tests/static-tree-api"
  "-DBASELINE_CATALOG=${fixtures}/static-tree-api/catalog224.json")
foreach(program IN ITEMS Monstey_NavCookerApiIndependentTests Monstey_UnusedFaceApiRebindTests)
  monstey_program(${program} SOURCES tests/${program}.cpp tests/Monstey_ReviewNativeNavFixture.cpp
    LIBS Monstey_Runtime STRICT)
endforeach()
monstey_test(NativeNavStaticTreeIndependent TIMEOUT 150 COMMAND "${MONSTEY_POWERSHELL}"
  -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavStaticTreeIndependent.ps1"
  -TestExecutable "$<TARGET_FILE:Monstey_NavCookerApiIndependentTests>"
  -DomainExecutable "$<TARGET_FILE:Monstey_UnusedFaceApiRebindTests>"
  -BaselineCatalog "${fixtures}/static-tree-api/independent-baseline224-world40.json"
  -OutputRoot "${out}/tests/static-tree-independent")

# --- Desktop application (these open real windows) ----------------------------------------

monstey_program(Monstey_ProfileUiFixtures LIBS Monstey_Runtime STRICT)
monstey_script_test(NativeGadbProfileUi Monstey_RunNativeProfileUi TIMEOUT 450 RUN_SERIAL
  "-DUI_EXE=$<TARGET_FILE:Monstey_Workbench>" "-DFIXTURE_EXE=$<TARGET_FILE:Monstey_ProfileUiFixtures>"
  "-DOUTPUT_ROOT=${out}/tests/gadb-profile-ui")
monstey_script_test(Monstey_NativeDatabaseGui RunNativeDatabaseGui TIMEOUT 90 RUN_SERIAL LABELS native-gui
  "-DGUI=$<TARGET_FILE:Monstey_Workbench>" "-DOUTPUT_ROOT=${out}/native-database-gui")
monstey_test(NativeNavStaticTreeGui TIMEOUT 150 LABELS native-gui COMMAND "${MONSTEY_POWERSHELL}"
  -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunNavStaticTreeGui.ps1"
  -Gui "$<TARGET_FILE:Monstey_Workbench>"
  -TestExecutable "$<TARGET_FILE:Monstey_NavStaticTreeApiTests>"
  -BaselineCatalog "${fixtures}/static-tree-api/catalog224.json"
  -OutputRoot "${out}/tests/static-tree-gui")
monstey_test(NativeSowAtomGui TIMEOUT 90 COMMAND "${MONSTEY_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
  -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunSowAtomGui.ps1"
  -Gui "$<TARGET_FILE:Monstey_Workbench>" -OutputRoot "${out}/tests/sow-atom-gui")

# Links the same application sources as the Workbench.
monstey_program(Monstey_NativeUiIndependentTests STRICT
  SOURCES tests/Monstey_NativeUiIndependent.cpp src/Monstey_CurveControl.cpp src/Monstey_ModelPreviewWindow.cpp
          ${MONSTEY_APP_SOURCES}
  LIBS Monstey_Theme Monstey_Runtime Monstey_TextureWindow user32 gdi32 comctl32 comdlg32 shell32 ole32)
monstey_script_test(NativeGadbUiIndependent RunNativeUiIndependent TIMEOUT 60 RUN_SERIAL LABELS native-gui
  "-DTEST_EXE=$<TARGET_FILE:Monstey_NativeUiIndependentTests>" "-DOUTPUT_ROOT=${out}/native-ui-independent")

# --- Opt-in tests on private game files (never copied or installed) -------------------------

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
  set(actual "-DMESH=${MONSTEY_CP017_ACTUAL_MESH}" "-DSKEL=${MONSTEY_CP017_ACTUAL_SKEL}"
    "-DANIX=${MONSTEY_CP017_ACTUAL_ANIX}" "-DCLIP=${MONSTEY_CP017_ACTUAL_CLIP}")
  monstey_checkpoint_test(017 NativeAnimationPreviewActual Monstey_AnimationPreviewTests animation_actual ${actual})
  monstey_checkpoint_test(017 NativeAnimationChannelProfile Monstey_AnimationChannelProbe channels ${actual})
  monstey_checkpoint_test(017 NativeSkelDefaultActual Monstey_SkelDefaultPoseProbe default
    "-DSKELS=${MONSTEY_CP017_ACTUAL_SKEL}")
  if(MONSTEY_CP017_ENABLE_ARMOR_API_ACTUAL)
    monstey_checkpoint_test(017 NativeAnimationPreviewApiActual Monstey_AnimationPreviewApiTests api_actual ${actual})
  endif()
endif()

# The animated window test needs the bind pose and PL_GL_Might_L1_AOE_LL_2_Idle,
# which the synthetic fixture cannot provide. Paths AND hashes are mandatory.
option(MONSTEY_CP017_ENABLE_ANIMATED_WINDOW "Enable actual native GUI test" OFF)
foreach(kind MESH SKEL ANIX)
  set(MONSTEY_CP017_WINDOW_${kind} "$ENV{MONSTEY_CP017_WINDOW_${kind}}" CACHE FILEPATH "Private GUI ${kind}; not copied")
  set(MONSTEY_CP017_WINDOW_${kind}_SHA256 "$ENV{MONSTEY_CP017_WINDOW_${kind}_SHA256}" CACHE STRING
    "Explicit GUI ${kind} SHA256")
endforeach()
if(MONSTEY_CP017_ENABLE_ANIMATED_WINDOW)
  set(window_args)
  foreach(kind MESH SKEL ANIX)
    set(path "${MONSTEY_CP017_WINDOW_${kind}}")
    set(sha "${MONSTEY_CP017_WINDOW_${kind}_SHA256}")
    string(LENGTH "${sha}" sha_length)
    if(NOT EXISTS "${path}" OR NOT sha_length EQUAL 64 OR NOT sha MATCHES "^[0-9a-fA-F]+$")
      message(FATAL_ERROR "Window actual test needs existing ${kind} and explicit SHA256")
    endif()
    list(APPEND window_args "-D${kind}=${path}" "-D${kind}_SHA256=${sha}")
  endforeach()
  monstey_program(Monstey_AnimatedPreviewWindowTests STRICT
    SOURCES tests/Monstey_AnimatedPreviewWindowTests.cpp src/Monstey_ModelPreviewWindow.cpp
    LIBS Monstey_Data user32 gdi32 comctl32 Monstey_Theme)
  monstey_checkpoint_test(017 NativeAnimatedPreviewWindow Monstey_AnimatedPreviewWindowTests window_actual
    ${window_args})
  set_tests_properties(NativeAnimatedPreviewWindow PROPERTIES RUN_SERIAL TRUE
    LABELS "checkpoint017;native-gui;actual")
endif()

option(MONSTEY_CP018_ENABLE_ACTUAL_MATERIALS "Enable explicit private material corpus" OFF)
set(MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST "" CACHE FILEPATH "Explicit private material JSON manifest")
if(MONSTEY_CP018_ENABLE_ACTUAL_MATERIALS)
  if(NOT EXISTS "${MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST}")
    message(FATAL_ERROR "Actual material test needs an explicit existing JSON manifest")
  endif()
  monstey_checkpoint_test(018 NativeLtmiAssetActual Monstey_LtmiAssetTests asset
    "-DMANIFEST=${MONSTEY_CP018_ACTUAL_MATERIAL_MANIFEST}")
  set_tests_properties(NativeLtmiAssetActual PROPERTIES LABELS "checkpoint018;actual")
endif()

option(MONSTEY_WORLD_STATIC_ENABLE_ACTUAL "Enable explicit private actual static pair tests" OFF)
set(MONSTEY_WORLD_STATIC_ACTUAL_MESH "" CACHE FILEPATH "Explicit private actual MESH5 input")
set(MONSTEY_WORLD_STATIC_ACTUAL_RENDER "" CACHE FILEPATH "Explicit private actual WDSC9 input")
if(MONSTEY_WORLD_STATIC_ENABLE_ACTUAL)
  if(NOT EXISTS "${MONSTEY_WORLD_STATIC_ACTUAL_MESH}" OR NOT EXISTS "${MONSTEY_WORLD_STATIC_ACTUAL_RENDER}")
    message(FATAL_ERROR "Both existing private static pair paths required")
  endif()
  monstey_script_test(NativeWorldStaticComponentActual Monstey_RunWorldStaticComponent TIMEOUT 600 LABELS actual
    "-DEXE=$<TARGET_FILE:Monstey_WorldStaticComponentTests>" "-DWORK_ROOT=${out}/tests/world-static-component-actual"
    "-DACTUAL_MESH=${MONSTEY_WORLD_STATIC_ACTUAL_MESH}" "-DACTUAL_RENDER=${MONSTEY_WORLD_STATIC_ACTUAL_RENDER}")
endif()
