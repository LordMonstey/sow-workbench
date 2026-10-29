#pragma once

#include <windows.h>

#include "Monstey_Theme.hpp"
#include "Monstey_Anix.hpp"
#include "Monstey_CurveControl.hpp"
#include "Monstey_Runtime.hpp"
#include "Monstey_ModelPreviewWindow.hpp"
#include "Monstey_TexturePreviewWindow.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <atomic>
#include <cmath>
#include <commctrl.h>
#include <commdlg.h>
#include <condition_variable>
#include <deque>
#include <dlgs.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "Monstey_AppIds.hpp"
#include "Monstey_AppSupport.hpp"
#include "Monstey_AppDialogs.hpp"
#include "Monstey_AppWorker.hpp"
#include "panels/Monstey_DatabaseTreeState.hpp"

namespace Monstey::Monstey_AppInternal
{

class Monstey_App
{
  public:
    HINSTANCE instance;
    HWND window = nullptr, tabs = nullptr, tree = nullptr, records = nullptr, fields = nullptr, query = nullptr,
         status = nullptr, pathLabel = nullptr, details = nullptr, pageLabel = nullptr, nativeProfileLabel = nullptr;
    HFONT font = nullptr;
    std::vector<HWND> buttons;
    std::unique_ptr<Monstey_Runtime> runtime;
    std::filesystem::path schema, fxSchema = Monstey_DefaultFxSchema();
    std::shared_ptr<Monstey_Editor> editor;
    std::shared_ptr<Monstey_Inspector> inspector;
    std::shared_ptr<Monstey_ModelPreviewWindow> modelPreview;
    std::unique_ptr<Monstey_TexturePreviewWindow> texturePreview;
    bool smokeTexture = false;
    std::size_t previewLoadsStarted = 0, previewLoadsCompleted = 0;
    bool previewCloseTestReady = false;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<Monstey_Job> jobs;
    std::deque<Monstey_Result> results;
    bool stopping = false, closing = false;
    std::atomic<bool> active{false};
    std::uint64_t epoch = 1, generation = 0, revision = 0;
    std::size_t offset = 0, total = 0;
    std::optional<std::uint32_t> category;
    std::vector<J> categoryRows, rowData, fieldData;
    HTREEITEM databaseTreeRoot = nullptr;
    std::uint64_t databaseTreeGeneration = 0, databaseTreeRevision = 0, databaseTreeSession = 0;
    LPARAM databaseTreeNextToken = 1;
    std::wstring databaseTreeQuery;
    std::map<LPARAM, Monstey_DatabaseTreeBinding> databaseTreeBindings;
    std::map<std::uint32_t, Monstey_DatabaseCategoryNode> databaseTreeCategories;
    std::string databaseTreeInspectSelector;
    int databaseTreeSmokeStage = 0;
    J metadata = J::object();
    J fxWorkflow = J::object(), fxGlobal = J::object(), fxLibrary = J::object();
    std::vector<J> fxTreeRows;
    std::string fxView = "all";
    std::string selector;
    bool socketMode = false, animatedExtraMode = false, fxNestedMode = false, fxLoading = false;
    std::string socketModel;
    std::filesystem::path socketRig;
    std::size_t socketOffset = 0, socketTotal = 0;
    std::uint64_t socketRequest = 0;
    J socketView = J::object(), fxNestedView = J::object();
    std::uint64_t fxNestedRequest = 0, fxAttachmentRequest = 0, fxLoadRequest = 0;
    J fxAttachmentView = J::object();
    bool materialMode = false, materialOpening = false, smokeMaterial = false;
    J materialView = J::object(), materialLastReport = J::object();
    std::wstring materialSource;
    std::string materialSelected;
    std::uint64_t materialRowsRequest = 0, materialOpenEpoch = 0;
    bool animationMode = false, animationEventMode = false, animationOpening = false;
    J animationView = J::object(), animationClip = J::object(), animationLastReport = J::object();
    std::wstring animationSource;
    std::string animationSelected;
    std::uint64_t animationRowsRequest = 0, animationClipRequest = 0, animationOpenEpoch = 0;
    int tab = 0;
    bool databaseNativeMode = false, databaseRestorePending = false, databaseViewSaved = false;
    bool Monstey_SowAtomGadb17 = false, Monstey_SowAtomSmoke = false;
    std::optional<std::uint32_t> databaseSavedCategory;
    std::size_t databaseSavedOffset = 0;
    std::string databaseSavedSelector;
    std::wstring databaseSavedFilter;
    int databaseSavedField = -1;
    bool databaseRestoreField = false;
    J databaseNativeSource = J::object();
    bool populating = false;
    std::filesystem::path nativeProfilePath[2];
    std::string nativeProfileSha[2], nativeProfileBuildStatus[2];
    std::uint64_t nativeProfileBytes[2]{}, nativeProfileSerial[2]{};
    bool smokeNativeProfile = false;
    std::size_t Monstey_nativeProfileBlockedTabChanges = 0;
    std::wstring sourceNames[7];
    bool dirty[7]{};
    bool canUndo[7]{}, canRedo[7]{};
    std::size_t recordDoubleClicks = 0, fieldDoubleClicks = 0;
    J selectedOperation = J::object(), operationArguments = J::object();
    std::map<std::string, J> operationPresets;
    std::string preferredOperation;
    std::filesystem::path modelBlendPreview;
    std::filesystem::path initialDatabase, smokeReport, smokeDirectory;
    J smoke = J::object();
    int smokeStage = 0;
    std::string smokeOperation, smokeExpectedError;
    J smokeArguments = J::object();
    int smokeDomain = 5;
    std::size_t heartbeat = 0;
    bool smokeFailed = false, smokeCloseDuringBuild = false, smokeStructure = false, smokeEffects = false,
         smokeSockets = false, smokePrefabs = false, smokeEnvelope = false, smokeExtras = false, smokeNested = false,
         smokeArchive = false, smokeStrings = false, smokeStorageBusy = false, databaseNativeSmokeBusy = false,
         smokeAnimation = false, smokeAttachment = false, smokePreview = false, smokePreviewClose = false,
         smokePreviewAnimation = false, smokeStartupFailure = false;
    std::string currentOperation;
    explicit Monstey_App(HINSTANCE module, std::filesystem::path nativeSchema, std::filesystem::path settings = {});
    ~Monstey_App();
    void Create(int show);
    void Layout();
    void StartWorker();
    void Queue(std::string operation, J arguments, std::function<void(const J &)> callback = {},
               bool checkGeneration = true, bool coalesceReads = true);
    void PumpResults();
    void Close();
    void Error(const std::string &message);
    void SetStatus(const std::wstring &text);
    void UpdateState(const J &result, int targetTab = -1);
    void UpdateButtons();
    void TabChanged();
    void OpenSource(std::optional<std::filesystem::path> path = std::nullopt);
    void Refresh(bool reset = false);
    void LoadCategories();
    void SelectCategory(int index);
    const J *Monstey_CategoryRow(std::uint32_t index) const;
    std::wstring Monstey_CategoryDetails(std::uint32_t index) const;
    void Monstey_ResetDatabaseTree();
    LPARAM Monstey_BindDatabaseTreeItem(HTREEITEM item, Monstey_DatabaseTreeValue value, std::string label = {});
    bool Monstey_DatabaseTreeItemCurrent(LPARAM token, HTREEITEM item) const;
    void Monstey_AddDatabaseTreePlaceholder(HTREEITEM parent);
    void Monstey_ClearDatabaseTreeRecords(std::uint32_t index);
    void Monstey_RefreshDatabaseTreeQuery();
    void Monstey_LoadDatabaseTreeRecords(std::uint32_t index, std::size_t pageOffset = 0);
    void Monstey_ExpandDatabaseTreeItem(LPARAM token);
    void Monstey_SelectDatabaseTreeItem(LPARAM token, bool inspect = false);
    void Monstey_SyncDatabaseTreeSelection();
    bool Monstey_DatabaseTreeSmokeStep();
    void LoadRecords();
    void ClearStorageRows();
    void ReadRecord(int row, bool explicitActivation = false);
    void PopulateFields(const J &reply);
    void EditField(int index, const J *snapshot = nullptr);
    void OpenInspector(const J &snapshot);
    void Command(int command);
    void ShowEditor(std::shared_ptr<Monstey_Editor> item);
    void Prompt(const std::wstring &title, std::string operation, J args, const std::string &valueKey,
                std::string initial, std::function<void(const J &)> success = {});
    void Transaction(std::string operation, J arguments = {});
    void StructureCommand(int command, const J *snapshot = nullptr, int fieldIndex = -1);
    void RefreshProperties(const std::string &record, const std::string &field = {});
    void StructureForm(const std::wstring &title, const J &rows, std::function<void(const J &)> apply,
                       std::string operation = {}, J arguments = J::object());
    void SmokeStructureStep();
    void SmokeEffectsStep();
    void LoadFx();
    void SelectFxView(int index);
    void PopulateFxRows();
    void FxCommand(int command);
    void FxLibraryCommand(int command);
    void SocketCommand(int command);
    void LoadSocketModels();
    void LoadSocketOverrides();
    void SocketAccepted(const J &result);
    void SocketTransform(std::string operation, J args, const J &position, const J &rotation);
    void SmokeSocketsStep();
    void SmokePrefabStep();
    void ExtraCommand(int command);
    void ExtraValues(std::string operation, J args, const J &row);
    void FxNestedCommand(int command);
    void LoadFxNested();
    void FxAttachmentCommand();
    void UpdateFxAttachmentButton();
    void SmokeAttachmentStep();
    void SmokeExtrasStep();
    void SmokeNestedStep();
    void SmokeMaterialStep();
    void TexturePreviewCommand();
    void SmokeTextureStep();
    void MaterialCommand(int command);
    void MaterialAccept(const J &result);
    void MaterialSubmit(std::string operation, J args);
    void MaterialLoadRows();
    void MaterialPopulate();
    void MaterialReadRow(int index, bool edit = false);
    void MaterialValue(J draft, const J &row, bool raw);
    void AnimationCommand(int command);
    void AnimationLoadRows();
    void AnimationReadClip(int index);
    void AnimationPopulate();
    void AnimationAccept(const J &result);
    void AnimationSubmit(std::string operation, J args, std::string selected = {});
    void AnimationEvent(int command);
    void SmokeAnimationStep();
    void PrepareStorageSmoke();
    void SmokeStorageStep();
    void FxApply(std::string operation, J arguments = J::object());
    void FxAccepted(const J &result);
    void FxCreateValues(std::string operation, J arguments, std::string className);
    void FxRequiredValue(std::shared_ptr<J> draft);
    void ShowReport(const J &value);
    bool Monstey_DatabaseForm() const;
    bool Monstey_OperationForm() const;
    bool Monstey_SowAtomAvailable() const;
    void Monstey_SowAtomCommand();
    void Monstey_SowAtomSmokePrepare();
    void Monstey_SowAtomSmokeStep();
    void Monstey_SaveDatabaseView();
    void Monstey_DatabaseNativeCommand(int command);
    void Monstey_DatabaseNativeSmokePrepare();
    void Monstey_DatabaseNativeSmokeStart();
    void Monstey_DatabaseNativeSmokeFinish();
    void LoadOperations();
    void SelectOperation(int row);
    void PopulateOperationFields();
    void EditOperationField(int index);
    void RunOperation();
    void FormCommand(int command);
    void ModelWorkflow(std::string_view operation);
    void ModelEnvelope(J inputs = J::object());
    void OpenModelBlender();
    void ModelPreviewCommand();
    void ModelAnimationCommand(const Monstey_PreviewAnimationRequest &request,
                               std::weak_ptr<Monstey_ModelPreviewWindow> target);
    void SmokePreviewStep();
    void SmokeAnimatedPreviewStep();
    void NativeProfileCommand(int command);
    void NativeProfileLabel();
    void SmokeNativeProfileStep();
    void SmokeStep();
    void SmokeComplete();
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
};

} // namespace Monstey::Monstey_AppInternal
