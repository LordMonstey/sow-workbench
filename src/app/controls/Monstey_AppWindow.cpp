#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::Create(int show)
{
    Monstey_CurveControl::Register(instance);
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpfnWndProc = Procedure;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = Monstey_ThemeBackgroundBrush();
    cls.lpszClassName = Monstey_MainClass;
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&cls);
    WNDCLASSEXW edit = cls;
    edit.lpfnWndProc = Monstey_Editor::Procedure;
    edit.lpszClassName = Monstey_EditorClass;
    RegisterClassExW(&edit);
    edit.lpfnWndProc = Monstey_Inspector::Procedure;
    edit.lpszClassName = Monstey_InspectorClass;
    RegisterClassExW(&edit);
    window = CreateWindowExW(0, Monstey_MainClass, L"Monstey SoW Workbench 0.3.0 - Native C++", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1460, 900, nullptr, nullptr, instance, this);
    Monstey_Require(window != nullptr, "Cannot create native Workbench window");
    Monstey_ThemeApply(window);
    StartWorker();
    if (smokeStartupFailure)
        throw Monstey_Error("startup_test", "Expected startup failure after native window and worker creation");
    ShowWindow(window, show);
    UpdateWindow(window);
    SetTimer(window, 1, 100, nullptr);
    if (Monstey_SowAtomSmoke)
    {
        Monstey_SowAtomSmokePrepare();
    }
    else if (smokeNativeProfile)
    {
        TabCtrl_SetCurSel(tabs, smokeArguments.value("domain", "database").as_string() == "effects" ? 3 : 0);
        TabChanged();
        smokeStage = 1;
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        Command(Monstey_Load);
    }
    else if (smokeTexture)
    {
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        for (int mode = 0; mode < 4; ++mode)
        {
            materialMode = mode == 1;
            animationMode = mode == 2;
            socketMode = mode == 3;
            UpdateButtons();
            const auto button = GetDlgItem(window, Monstey_TexturePreview);
            Monstey_Require(IsWindowEnabled(button) && (GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE),
                            "Texture inspector unavailable in a Models mode");
        }
        materialMode = animationMode = socketMode = false;
        UpdateButtons();
        smoke["texture_available_in_all_model_modes"] = true;
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        SendMessageW(GetDlgItem(window, Monstey_TexturePreview), BM_CLICK, 0, 0);
        smokeStage = 1;
    }
    else if (smokeMaterial)
    {
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        Command(Monstey_Materials);
        smokeStage = 1;
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        Command(Monstey_Open);
    }
    else if (smokePreview)
    {
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smokeStage = 0;
        for (int mode = 0; mode < 3; ++mode)
        {
            animationMode = mode == 1;
            socketMode = mode == 2;
            UpdateButtons();
            Monstey_Require((GetWindowLongPtrW(GetDlgItem(window, Monstey_ModelPreview), GWL_STYLE) & WS_VISIBLE) &&
                                IsWindowEnabled(GetDlgItem(window, Monstey_ModelPreview)),
                            "Preview is unavailable in a Models mode");
        }
        animationMode = socketMode = false;
        UpdateButtons();
        smoke["preview_available_in_model_animation_socket_modes"] = true;
        Queue("strings_new", J::object(), {}, false);
        auto sibling = initialDatabase;
        sibling.replace_extension(L".skel");
        if (std::filesystem::is_regular_file(sibling))
            smoke["preview_skeleton_sha256"] = Monstey_HashSha256(sibling);
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        Command(Monstey_ModelPreview);
        smokeStage = 1;
        TabCtrl_SetCurSel(tabs, 1);
        TabChanged();
        smoke["preview_epoch_changed_while_loading"] = true;
    }
    else if (smokeAnimation)
    {
        animationMode = true;
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smokeStage = 1;
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        AnimationCommand(Monstey_Open);
        TabCtrl_SetCurSel(tabs, 1);
        TabChanged();
        for (auto id : {Monstey_Add, Monstey_Save, Monstey_Build})
            Monstey_Require(IsWindowEnabled(GetDlgItem(window, id)), "Animation mode left Archive actions disabled");
        TabCtrl_SetCurSel(tabs, 0);
        TabChanged();
        for (auto id : {Monstey_Rename, Monstey_Delete})
            Monstey_Require(IsWindowEnabled(GetDlgItem(window, id)), "Animation mode left Database actions disabled");
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smoke["deferred_open_tab_switch_toolbar_restoration"] = true;
    }
    else if (smokeArchive || smokeStrings)
    {
        PrepareStorageSmoke();
        TabCtrl_SetCurSel(tabs, smokeArchive ? 1 : 2);
        TabChanged();
        smokeStage = 1;
        SendMessageW(GetDlgItem(window, Monstey_New), BM_CLICK, 0, 0);
    }
    else if (smokeExtras)
    {
        animatedExtraMode = true;
        socketMode = true;
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smokeStage = 1;
        Queue(
            "open_database",
            J::object({{"path", Monstey_JsonPath(initialDatabase)}, {"schema", Monstey_JsonPath(schema)}}),
            [this](const J &reply)
            {
                if (!reply.at("ok").as_bool())
                {
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                UpdateState(reply.at("result"), 0);
                LoadSocketModels();
            },
            false);
    }
    else if (smokeAttachment)
    {
        TabCtrl_SetCurSel(tabs, 3);
        TabChanged();
        smokeStage = 1;
        OpenSource(initialDatabase);
    }
    else if (smokeNested && !initialDatabase.empty())
    {
        TabCtrl_SetCurSel(tabs, 3);
        TabChanged();
        smokeStage = 50;
        OpenSource(initialDatabase);
    }
    else if (smokeEnvelope)
    {
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smokeStage = 1;
        ModelEnvelope(smokeArguments);
    }
    else if (smokeSockets)
    {
        socketMode = true;
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        smokeStage = 1;
        Queue(
            "open_database",
            J::object({{"path", Monstey_JsonPath(initialDatabase)}, {"schema", Monstey_JsonPath(schema)}}),
            [this](const J &reply)
            {
                if (!reply.at("ok").as_bool())
                {
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                UpdateState(reply.at("result"), 0);
                LoadSocketModels();
            },
            false);
    }
    else if (smokeEffects || smokePrefabs || smokeNested)
    {
        TabCtrl_SetCurSel(tabs, 3);
        TabChanged();
        smokeStage = 1;
        Command(Monstey_New);
    }
    else if (smokeStructure)
        Command(Monstey_New);
    else if (!initialDatabase.empty())
        OpenSource(initialDatabase);
    else if (!smokeOperation.empty())
    {
        if (smokeDomain == 0)
        {
            if (!smokeReport.empty())
                Monstey_DatabaseNativeSmokePrepare();
            else
                Command(smokeOperation == "database_native_class" ? Monstey_NativeClassMenu : Monstey_NativeBanksMenu);
        }
        else
        {
            TabCtrl_SetCurSel(tabs, smokeDomain);
            TabChanged();
        }
    }
}

void Monstey_App::Layout()
{
    if (!window || !tabs)
        return;
    RECT rect{};
    GetClientRect(window, &rect);
    const int width = std::max(800L, rect.right), height = std::max(520L, rect.bottom);
    int buttonX = 8, buttonY = 10;
    for (const auto button : buttons)
        if (GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE)
        {
            const int buttonWidth = std::max(65, static_cast<int>(Monstey_WindowText(button).size()) * 8 + 20);
            if (buttonX + buttonWidth > width - 8 && buttonX > 8)
            {
                buttonX = 8;
                buttonY += 33;
            }
            MoveWindow(button, buttonX, buttonY, buttonWidth, 28, TRUE);
            buttonX += buttonWidth + 5;
        }
    const auto toolbarExtra = buttonY - 10;
    const int profileExtra = (tab == 0 && !databaseNativeMode) || tab == 3 ? 32 : 0;
    if (profileExtra)
    {
        MoveWindow(GetDlgItem(window, Monstey_NativeProfileChoose), 10, 106 + toolbarExtra, 166, 26, TRUE);
        MoveWindow(GetDlgItem(window, Monstey_NativeProfileClear), 181, 106 + toolbarExtra, 58, 26, TRUE);
        MoveWindow(nativeProfileLabel, 249, 110 + toolbarExtra, width - 259, 22, TRUE);
    }
    MoveWindow(tabs, 8, 46 + toolbarExtra, width - 16, 28, TRUE);
    MoveWindow(pathLabel, 10, 80 + toolbarExtra, width - 20, 22, TRUE);
    MoveWindow(query, 10, 108 + toolbarExtra + profileExtra, width - 250, 25, TRUE);
    MoveWindow(GetDlgItem(window, Monstey_Search), width - 232, 108 + toolbarExtra + profileExtra, 80, 25, TRUE);
    MoveWindow(GetDlgItem(window, Monstey_Previous), width - 145, 108 + toolbarExtra + profileExtra, 58, 25, TRUE);
    MoveWindow(GetDlgItem(window, Monstey_Next), width - 80, 108 + toolbarExtra + profileExtra, 65, 25, TRUE);
    const int top = 142 + toolbarExtra + profileExtra, bottom = height - 170;
    const bool db = tab == 0 && !databaseNativeMode, fx = tab == 3, form = Monstey_OperationForm();
    const int treeWidth = (db || fx) ? 255 : 0;
    ShowWindow(tree, db || fx ? SW_SHOW : SW_HIDE);
    MoveWindow(tree, 10, top, treeWidth, std::max(100, bottom - top), TRUE);
    const int recordX = db || fx ? 275 : 10,
              recordWidth = (db || fx || form) ? std::max(290, (width - recordX) / 2 - 10) : width - 20;
    MoveWindow(records, recordX, top, recordWidth, std::max(100, bottom - top), TRUE);
    ShowWindow(fields, db || fx || form ? SW_SHOW : SW_HIDE);
    if (db || fx || form)
        MoveWindow(fields, recordX + recordWidth + 10, top, width - recordX - recordWidth - 20,
                   std::max(100, bottom - top), TRUE);
    MoveWindow(pageLabel, 10, bottom + 5, width - 20, 20, TRUE);
    MoveWindow(details, 10, bottom + 30, width - 20, 105, TRUE);
    MoveWindow(status, 0, height - 26, width, 26, TRUE);
}

void Monstey_App::TabChanged()
{
    const auto selected = TabCtrl_GetCurSel(tabs);
    if (selected < 0 || selected > 6)
        return;
    if (tab == 0 && !databaseNativeMode && selected != tab)
        Monstey_SaveDatabaseView();
    ++epoch;
    tab = selected;
    ++materialRowsRequest;
    if (tab == 4 && materialMode)
    {
        sourceNames[4] = materialSource;
        if (!materialView.empty())
            UpdateState(materialView, 4);
    }
    else if (tab == 4 && animationMode)
        sourceNames[4] = animationSource;
    if (tab != 3)
    {
        fxNestedMode = false;
        fxNestedView = J::object();
        ++fxNestedRequest;
    }
    offset = 0;
    category.reset();
    selector.clear();
    metadata = J::object();
    categoryRows.clear();
    Monstey_ResetDatabaseTree();
    rowData.clear();
    fieldData.clear();
    populating = true;
    TreeView_DeleteAllItems(tree);
    ListView_DeleteAllItems(records);
    ListView_DeleteAllItems(fields);
    while (ListView_DeleteColumn(records, 0))
    {
    }
    while (ListView_DeleteColumn(fields, 0))
    {
    }
    if ((tab == 0 && !databaseNativeMode) || tab == 3)
    {
        Monstey_ListColumn(records, 0, L"Record / element", 260);
        Monstey_ListColumn(records, 1, L"Reference", 120);
        Monstey_ListColumn(records, 2, L"Category", 220);
        Monstey_ListColumn(fields, 0, L"Property", 190);
        Monstey_ListColumn(fields, 1, L"Type", 100);
        Monstey_ListColumn(fields, 2, L"Value", 350);
    }
    else if (tab == 4 && materialMode)
    {
        Monstey_ListColumn(records, 0, L"Material property", 240);
        Monstey_ListColumn(records, 1, L"Native type", 95);
        Monstey_ListColumn(records, 2, L"Value", 320);
        Monstey_ListColumn(fields, 0, L"Property metadata", 210);
        Monstey_ListColumn(fields, 1, L"Stored value", 340);
        Monstey_ListColumn(fields, 2, L"Role", 130);
    }
    else if (tab == 4 && animationMode)
    {
        Monstey_ListColumn(records, 0, L"Clip", 260);
        Monstey_ListColumn(records, 1, L"Duration (ms)", 130);
        Monstey_ListColumn(records, 2, L"Codecs", 240);
        Monstey_ListColumn(fields, 0, L"Property / tick (ms)", 190);
        Monstey_ListColumn(fields, 1, L"Value / event text", 360);
        Monstey_ListColumn(fields, 2, L"Native metadata", 140);
    }
    else if (Monstey_OperationForm())
    {
        Monstey_ListColumn(records, 0, L"Native operation", 320);
        Monstey_ListColumn(records, 1, L"Status", 100);
        Monstey_ListColumn(records, 2, L"Description", 550);
        Monstey_ListColumn(fields, 0, L"Argument", 200);
        Monstey_ListColumn(fields, 1, L"Type", 110);
        Monstey_ListColumn(fields, 2, L"Value", 400);
    }
    else if (tab == 1)
    {
        Monstey_ListColumn(records, 0, L"Virtual path", 720);
        Monstey_ListColumn(records, 1, L"Bytes", 140);
        Monstey_ListColumn(records, 2, L"Stored bytes", 140);
    }
    else
    {
        Monstey_ListColumn(records, 0, L"Key hash", 140);
        Monstey_ListColumn(records, 1, L"Unicode text", 800);
        Monstey_ListColumn(records, 2, L"Modified", 90);
    }
    populating = false;
    SetWindowTextW(query,
                   tab == 0 && !databaseNativeMode && databaseRestorePending ? databaseSavedFilter.c_str() : L"");
    Layout();
    UpdateState(J::object());
    SendMessageW(details, EM_SETREADONLY, !Monstey_OperationForm() || (tab == 4 && (animationMode || materialMode)), 0);
    if (Monstey_OperationForm() || !sourceNames[tab].empty())
        Refresh(true);
}

} // namespace Monstey::Monstey_AppInternal
