#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::UpdateButtons()
{
    for (const auto control : {GetDlgItem(window, Monstey_NativeProfileChoose),
                               GetDlgItem(window, Monstey_NativeProfileClear), nativeProfileLabel})
        ShowWindow(control, (tab == 0 && !databaseNativeMode) || tab == 3 ? SW_SHOW : SW_HIDE);
    NativeProfileLabel();
    for (auto id : {Monstey_Save, Monstey_Build, Monstey_Revert, Monstey_Add, Monstey_Clone, Monstey_Rename,
                    Monstey_Delete, Monstey_Extract})
        EnableWindow(GetDlgItem(window, id), TRUE);
    SetWindowTextW(GetDlgItem(window, Monstey_New), L"New");
    SetWindowTextW(GetDlgItem(window, Monstey_Clone), L"Clone...");
    SetWindowTextW(GetDlgItem(window, Monstey_Add), tab == 4 ? L"Import Blender..." : L"Add...");
    SetWindowTextW(GetDlgItem(window, Monstey_Extract), tab == 4 ? L"Export native..." : L"Extract...");
    SetWindowTextW(GetDlgItem(window, Monstey_Replace), tab == 4 ? L"Open Blender" : L"Replace...");
    EnableWindow(GetDlgItem(window, Monstey_Replace), tab != 4 || !modelBlendPreview.empty());
    EnableWindow(GetDlgItem(window, Monstey_Undo), canUndo[tab]);
    EnableWindow(GetDlgItem(window, Monstey_Redo), canRedo[tab]);
    ShowWindow(GetDlgItem(window, Monstey_Clone), tab == 0 ? SW_SHOW : SW_HIDE);
    for (const auto id : {Monstey_Rename, Monstey_Delete})
        ShowWindow(GetDlgItem(window, id), tab <= 2 ? SW_SHOW : SW_HIDE);
    for (const auto id : {Monstey_Add, Monstey_Replace, Monstey_Extract, Monstey_Folder})
        ShowWindow(GetDlgItem(window, id), tab == 1 || (id == Monstey_Add && tab == 2) ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, Monstey_Dependencies), tab == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, Monstey_Effects), tab == 3 ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(window, Monstey_Effects), tab == 3 && !sourceNames[3].empty());
    ShowWindow(GetDlgItem(window, Monstey_Structure), tab == 0 ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(window, Monstey_Structure), tab == 0 && !sourceNames[0].empty());
    if (Monstey_OperationForm())
    {
        for (const auto button : buttons)
        {
            const auto id = GetDlgCtrlID(button);
            ShowWindow(button,
                       id == Monstey_Open || id == Monstey_Load || id == Monstey_Save || id == Monstey_Build ||
                               (id == Monstey_Revert ||
                                (tab == 4 && (id == Monstey_Add || id == Monstey_Extract || id == Monstey_Replace)))
                           ? SW_SHOW
                           : SW_HIDE);
        }
        SetWindowTextW(GetDlgItem(window, Monstey_Open), L"Source...");
        SetWindowTextW(GetDlgItem(window, Monstey_Load), L"Load args...");
        SetWindowTextW(GetDlgItem(window, Monstey_Save), L"Save args...");
        SetWindowTextW(GetDlgItem(window, Monstey_Build), tab == 4 ? L"Run" : L"Run operation");
        SetWindowTextW(GetDlgItem(window, Monstey_Revert), L"Advanced...");
    }
    else
    {
        for (const auto id : {Monstey_Open, Monstey_New, Monstey_Load, Monstey_Save, Monstey_Build, Monstey_Undo,
                              Monstey_Redo, Monstey_Revert})
            ShowWindow(GetDlgItem(window, id), SW_SHOW);
        SetWindowTextW(GetDlgItem(window, Monstey_Open), L"Open...");
        SetWindowTextW(GetDlgItem(window, Monstey_Load), L"Project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Save), L"Save...");
        SetWindowTextW(GetDlgItem(window, Monstey_Build), L"Build...");
        SetWindowTextW(GetDlgItem(window, Monstey_Revert), L"Revert");
    }
    ShowWindow(GetDlgItem(window, Monstey_Sockets), tab == 4 ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(window, Monstey_Sockets), socketMode ? L"Model tools" : L"Sockets...");
    SetWindowTextW(GetDlgItem(window, Monstey_SocketRig), animatedExtraMode ? L"Details..." : L"Rig...");
    ShowWindow(GetDlgItem(window, Monstey_Extras), tab == 4 && !socketMode ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, Monstey_FxAttachment), tab == 3 ? SW_SHOW : SW_HIDE);
    UpdateFxAttachmentButton();
    ShowWindow(GetDlgItem(window, Monstey_FxNested), tab == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, Monstey_Envelope), tab == 4 && !socketMode ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, Monstey_FxLibrary), tab == 3 ? SW_SHOW : SW_HIDE);
    for (const auto id : {Monstey_Effects, Monstey_FxLibrary, Monstey_FxNested})
        EnableWindow(GetDlgItem(window, id), tab == 3 && !sourceNames[3].empty() && !fxLoading);
    if (tab == 3 && fxLoading)
        for (const auto id : {Monstey_Save, Monstey_Build, Monstey_Undo, Monstey_Redo, Monstey_Revert})
            EnableWindow(GetDlgItem(window, id), FALSE);
    if (tab == 4 && socketMode)
    {
        for (auto button : buttons)
        {
            auto id = GetDlgCtrlID(button);
            ShowWindow(button, id == Monstey_Open || id == Monstey_Save || id == Monstey_Build || id == Monstey_Undo ||
                                       id == Monstey_Redo || id == Monstey_Sockets || id == Monstey_SocketCreate ||
                                       id == Monstey_SocketEdit || id == Monstey_SocketCopy ||
                                       id == Monstey_SocketRemove || id == Monstey_SocketRig
                                   ? SW_SHOW
                                   : SW_HIDE);
        }
        SetWindowTextW(GetDlgItem(window, Monstey_Open), L"Database...");
        SetWindowTextW(GetDlgItem(window, Monstey_Save), L"Save project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Build), L"Build DB...");
        EnableWindow(GetDlgItem(window, Monstey_Undo), canUndo[0]);
        EnableWindow(GetDlgItem(window, Monstey_Redo), canRedo[0]);
        SetWindowTextW(pathLabel, (sourceNames[0] + (dirty[0] ? L" [modified]" : L"")).c_str());
    }
    else
    {
        for (auto id :
             {Monstey_SocketCreate, Monstey_SocketEdit, Monstey_SocketCopy, Monstey_SocketRemove, Monstey_SocketRig})
            ShowWindow(GetDlgItem(window, id), SW_HIDE);
    }
    ShowWindow(GetDlgItem(window, Monstey_Animations), tab == 4 && !socketMode ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(window, Monstey_Animations), animationMode ? L"Model tools" : L"Animations...");
    for (auto id :
         {Monstey_AnimationHeld, Monstey_AnimationTiming, Monstey_AnimationEvents, Monstey_AnimationEventAdd,
          Monstey_AnimationEventEdit, Monstey_AnimationEventRemove, Monstey_AnimationSample, Monstey_AnimationExportAll,
          Monstey_AnimationSkeleton, Monstey_AnimationChanges, Monstey_AnimationEventClear})
        ShowWindow(GetDlgItem(window, id), SW_HIDE);
    if (tab == 4 && animationMode)
    {
        for (auto button : buttons)
        {
            const auto id = GetDlgCtrlID(button);
            const auto visible =
                id == Monstey_Animations || id == Monstey_Open || id == Monstey_New || id == Monstey_Load ||
                id == Monstey_Save || id == Monstey_Build || id == Monstey_Undo || id == Monstey_Redo ||
                id == Monstey_Revert || id == Monstey_Clone || id == Monstey_Rename || id == Monstey_Delete ||
                id == Monstey_Add || id == Monstey_Extract || id == Monstey_AnimationHeld ||
                id == Monstey_AnimationTiming || id == Monstey_AnimationEvents || id == Monstey_AnimationSample ||
                id == Monstey_AnimationExportAll || id == Monstey_AnimationSkeleton || id == Monstey_AnimationChanges ||
                (animationEventMode && (id == Monstey_AnimationEventAdd || id == Monstey_AnimationEventEdit ||
                                        id == Monstey_AnimationEventRemove || id == Monstey_AnimationEventClear));
            ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
        }
        SetWindowTextW(GetDlgItem(window, Monstey_Open), L"Open ANIX...");
        SetWindowTextW(GetDlgItem(window, Monstey_New), L"From scene...");
        SetWindowTextW(GetDlgItem(window, Monstey_Load), L"Project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Save), L"Save project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Build), L"Build ANIX...");
        SetWindowTextW(GetDlgItem(window, Monstey_Revert), L"Revert");
        SetWindowTextW(GetDlgItem(window, Monstey_Clone), L"Duplicate...");
        SetWindowTextW(GetDlgItem(window, Monstey_Add), L"Import clip...");
        SetWindowTextW(GetDlgItem(window, Monstey_Extract), L"Export clip...");
        SetWindowTextW(GetDlgItem(window, Monstey_AnimationEvents),
                       animationEventMode ? L"Clip metadata" : L"Events...");
        const auto loaded = !animationView.empty();
        const auto selected = !animationClip.empty() && animationClip.value("name", "") == J(animationSelected) &&
                              animationClip.value("ui_generation", 0).as_uint() == generation &&
                              animationClip.value("ui_revision", 0).as_uint() == revision;
        for (auto id : {Monstey_Save, Monstey_Build, Monstey_Revert, Monstey_Add, Monstey_AnimationHeld,
                        Monstey_AnimationExportAll, Monstey_AnimationChanges})
            EnableWindow(GetDlgItem(window, id), loaded);
        for (auto id : {Monstey_Clone, Monstey_Rename, Monstey_Delete, Monstey_Extract, Monstey_AnimationTiming,
                        Monstey_AnimationEvents, Monstey_AnimationSample, Monstey_AnimationEventAdd,
                        Monstey_AnimationEventEdit, Monstey_AnimationEventRemove})
            EnableWindow(GetDlgItem(window, id), selected);
        for (auto id : {Monstey_AnimationEventAdd, Monstey_AnimationEventEdit, Monstey_AnimationEventRemove})
            EnableWindow(GetDlgItem(window, id),
                         selected && animationClip.value("event_edit_supported", false).as_bool());
        for (auto id : {Monstey_AnimationTiming, Monstey_AnimationSample})
            EnableWindow(GetDlgItem(window, id), selected && animationClip.value("sample_supported", false).as_bool());
        EnableWindow(GetDlgItem(window, Monstey_AnimationEventClear),
                     selected && !animationClip.value("empty_native_clip", false).as_bool());
        if (selected && animationClip.value("hash", "") == J(Monstey_Hex(Monstey_PathHash("bind"))))
        {
            EnableWindow(GetDlgItem(window, Monstey_Rename), FALSE);
            EnableWindow(GetDlgItem(window, Monstey_Delete), FALSE);
        }
        EnableWindow(GetDlgItem(window, Monstey_Undo), loaded && canUndo[4]);
        EnableWindow(GetDlgItem(window, Monstey_Redo), loaded && canRedo[4]);
        SetWindowTextW(pathLabel, (animationSource + (dirty[4] ? L" [modified]" : L"")).c_str());
    }
    ShowWindow(GetDlgItem(window, Monstey_Materials), tab == 4 && !socketMode && !animationMode ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(window, Monstey_Materials), materialMode ? L"Model tools" : L"Materials...");
    for (auto id : {Monstey_MaterialEdit, Monstey_MaterialRaw, Monstey_MaterialChanges})
        ShowWindow(GetDlgItem(window, id), SW_HIDE);
    if (tab == 4 && materialMode)
    {
        for (auto button : buttons)
        {
            const auto id = GetDlgCtrlID(button);
            const auto visible = id == Monstey_Materials || id == Monstey_Open || id == Monstey_Load ||
                                 id == Monstey_Save || id == Monstey_Build || id == Monstey_Undo ||
                                 id == Monstey_Redo || id == Monstey_Revert || id == Monstey_Add ||
                                 id == Monstey_Rename || id == Monstey_Delete || id == Monstey_MaterialEdit ||
                                 id == Monstey_MaterialRaw || id == Monstey_MaterialChanges;
            ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
            EnableWindow(button, !materialOpening);
        }
        SetWindowTextW(GetDlgItem(window, Monstey_Open), L"Open material...");
        SetWindowTextW(GetDlgItem(window, Monstey_Load), L"Project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Save), L"Save project...");
        SetWindowTextW(GetDlgItem(window, Monstey_Build), L"Build material...");
        SetWindowTextW(GetDlgItem(window, Monstey_Revert), L"Revert");
        SetWindowTextW(GetDlgItem(window, Monstey_Add), L"Add property...");
        const auto loaded = !materialView.empty() && !materialOpening;
        const auto selected = loaded && !materialSelected.empty() && !rowData.empty();
        for (auto id : {Monstey_Save, Monstey_Build, Monstey_Revert, Monstey_Add, Monstey_MaterialChanges})
            EnableWindow(GetDlgItem(window, id), loaded);
        for (auto id : {Monstey_Rename, Monstey_Delete})
            EnableWindow(GetDlgItem(window, id), selected);
        const auto rowIndex = ListView_GetNextItem(records, -1, LVNI_SELECTED);
        const auto editable = selected && rowIndex >= 0 && static_cast<std::size_t>(rowIndex) < rowData.size() &&
                              rowData[static_cast<std::size_t>(rowIndex)].at("raw_storage_editable").as_bool();
        for (auto id : {Monstey_MaterialEdit, Monstey_MaterialRaw})
            EnableWindow(GetDlgItem(window, id), editable);
        EnableWindow(GetDlgItem(window, Monstey_Undo), loaded && materialView.at("can_undo").as_bool());
        EnableWindow(GetDlgItem(window, Monstey_Redo), loaded && materialView.at("can_redo").as_bool());
        SetWindowTextW(
            pathLabel,
            (materialSource + (loaded && materialView.at("dirty").as_bool() ? L" [modified]" : L"")).c_str());
    }
    ShowWindow(GetDlgItem(window, Monstey_ModelPreview), tab == 4 ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(window, Monstey_ModelPreview), !closing);
    ShowWindow(GetDlgItem(window, Monstey_TexturePreview), tab == 4 ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(window, Monstey_TexturePreview), !closing);
    ShowWindow(GetDlgItem(window, Monstey_NativeDatabaseTools), tab == 0 ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(window, Monstey_NativeDatabaseTools),
                   databaseNativeMode ? L"Back to records" : L"Native PE tools...");
    if (Monstey_DatabaseForm())
        SetWindowTextW(pathLabel,
                       (L"Native declarations / " +
                        Monstey_Wide(selectedOperation.value("name", "Choose native banks or class").as_string()))
                           .c_str());
    if (const auto menu = GetMenu(window))
    {
        const bool native = Monstey_DatabaseForm();
        const auto fileMenu = GetSubMenu(menu, 0), editMenu = GetSubMenu(menu, 1), dbMenu = GetSubMenu(menu, 2);
        for (const auto id : {Monstey_New, Monstey_Schema})
            EnableMenuItem(fileMenu, id, MF_BYCOMMAND | (native ? MF_GRAYED : MF_ENABLED));
        for (const auto id : {Monstey_Undo, Monstey_Redo, Monstey_Clone, Monstey_Rename, Monstey_Delete})
            EnableMenuItem(editMenu, id, MF_BYCOMMAND | (native ? MF_GRAYED : MF_ENABLED));
        for (const auto id : {Monstey_CreateCategory, Monstey_CreateRecord, Monstey_AddField, Monstey_DeleteField,
                              Monstey_MoveFieldUp, Monstey_MoveFieldDown})
            EnableMenuItem(dbMenu, id, MF_BYCOMMAND | (native ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(dbMenu, Monstey_InspectStorage,
                       MF_BYCOMMAND | (Monstey_SowAtomAvailable() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(dbMenu, Monstey_DatabaseRecordsMenu,
                       MF_BYCOMMAND | (databaseNativeMode ? MF_ENABLED : MF_GRAYED));
    }
    Layout();
}

} // namespace Monstey::Monstey_AppInternal
