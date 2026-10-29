#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

LRESULT CALLBACK Monstey_App::Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto *self = reinterpret_cast<Monstey_App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        self = static_cast<Monstey_App *>(reinterpret_cast<const CREATESTRUCTW *>(lparam)->lpCreateParams);
        self->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(window, message, wparam, lparam);
    try
    {
        switch (message)
        {
        case WM_CREATE:
        {
            self->font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            auto menu = CreateMenu(), file = CreatePopupMenu(), edit = CreatePopupMenu(),
                 databaseMenu = CreatePopupMenu(), tools = CreatePopupMenu(), help = CreatePopupMenu();
            auto effectsMenu = CreatePopupMenu();
            AppendMenuW(file, MF_STRING, Monstey_Open, L"Open source...\tCtrl+O");
            AppendMenuW(file, MF_STRING, Monstey_New, L"New project");
            AppendMenuW(file, MF_STRING, Monstey_Load, L"Open project...");
            AppendMenuW(file, MF_STRING, Monstey_Save, L"Save new project...\tCtrl+S");
            AppendMenuW(file, MF_STRING, Monstey_Build, L"Build new native file...");
            AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(file, MF_STRING, Monstey_Schema, L"Select native schema...");
            AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(file, MF_STRING, Monstey_Exit, L"Exit");
            AppendMenuW(edit, MF_STRING, Monstey_Undo, L"Undo\tCtrl+Z");
            AppendMenuW(edit, MF_STRING, Monstey_Redo, L"Redo\tCtrl+Y");
            AppendMenuW(edit, MF_STRING, Monstey_Revert, L"Revert project");
            AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(edit, MF_STRING, Monstey_Clone, L"Clone record...");
            AppendMenuW(edit, MF_STRING, Monstey_Rename, L"Rename selection...");
            AppendMenuW(edit, MF_STRING, Monstey_Delete, L"Delete selection...");
            for (const auto &[id, label] : {std::pair{Monstey_CreateCategory, L"New category..."},
                                            std::pair{Monstey_CreateRecord, L"New record..."},
                                            std::pair{Monstey_AddField, L"Add property...\tInsert"},
                                            std::pair{Monstey_DeleteField, L"Delete selected property\tDelete"},
                                            std::pair{Monstey_MoveFieldUp, L"Move property up"},
                                            std::pair{Monstey_MoveFieldDown, L"Move property down"}})
                AppendMenuW(databaseMenu, MF_STRING, id, label);
            AppendMenuW(databaseMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(databaseMenu, MF_STRING | MF_GRAYED, Monstey_InspectStorage, L"Inspect selected storage...");
            AppendMenuW(databaseMenu, MF_STRING, Monstey_NativeBanksMenu, L"Inspect native banks...");
            AppendMenuW(databaseMenu, MF_STRING, Monstey_NativeClassMenu, L"Inspect native class...");
            AppendMenuW(databaseMenu, MF_STRING, Monstey_DatabaseRecordsMenu, L"Back to database records");
            AppendMenuW(tools, MF_STRING, Monstey_Add, L"Add file / string...");
            AppendMenuW(tools, MF_STRING, Monstey_Replace, L"Replace archive file...");
            AppendMenuW(tools, MF_STRING, Monstey_Extract, L"Extract archive file...");
            AppendMenuW(tools, MF_STRING, Monstey_Folder, L"Create archive folder...");
            AppendMenuW(tools, MF_STRING, Monstey_Dependencies, L"Effect dependencies");
            AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(tools, MF_STRING, Monstey_ConfigureMenu, L"Settings...");
            AppendMenuW(tools, MF_STRING, Monstey_MountsMenu, L"Mounted assets...");
            AppendMenuW(tools, MF_STRING, Monstey_CatalogMenu, L"Build asset catalog...");
            AppendMenuW(help, MF_STRING, Monstey_About, L"About");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"Edit");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(databaseMenu), L"&Database");
            Monstey_FxMenu(effectsMenu);
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(effectsMenu), L"Effects");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"Tools");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"Help");
            SetMenu(window, menu);
            auto control = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int id, DWORD extended = 0)
            {
                const auto item =
                    CreateWindowExW(extended, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 100, 25, window,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), self->instance, nullptr);
                Monstey_Require(item != nullptr, "Cannot create native control");
                SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(self->font), TRUE);
                return item;
            };
            const std::pair<int, const wchar_t *> controls[] = {{Monstey_Open, L"Open..."},
                                                                {Monstey_New, L"New"},
                                                                {Monstey_Load, L"Project..."},
                                                                {Monstey_Save, L"Save..."},
                                                                {Monstey_Build, L"Build..."},
                                                                {Monstey_Undo, L"Undo"},
                                                                {Monstey_Redo, L"Redo"},
                                                                {Monstey_Revert, L"Revert"},
                                                                {Monstey_Clone, L"Clone..."},
                                                                {Monstey_Rename, L"Rename..."},
                                                                {Monstey_Delete, L"Delete..."},
                                                                {Monstey_Add, L"Add..."},
                                                                {Monstey_Replace, L"Replace..."},
                                                                {Monstey_Extract, L"Extract..."},
                                                                {Monstey_Folder, L"Folder..."},
                                                                {Monstey_Dependencies, L"Dependencies"},
                                                                {Monstey_Structure, L"Structure..."},
                                                                {Monstey_NativeDatabaseTools, L"Native PE tools..."},
                                                                {Monstey_Effects, L"Effects..."},
                                                                {Monstey_Sockets, L"Sockets..."},
                                                                {Monstey_SocketCreate, L"Create..."},
                                                                {Monstey_SocketEdit, L"Edit..."},
                                                                {Monstey_SocketCopy, L"Copy..."},
                                                                {Monstey_SocketRemove, L"Unlink..."},
                                                                {Monstey_SocketRig, L"Rig..."},
                                                                {Monstey_FxLibrary, L"Library..."},
                                                                {Monstey_Envelope, L"Visible radius..."},
                                                                {Monstey_Extras, L"Animated extras..."},
                                                                {Monstey_FxNested, L"Nested FX..."},
                                                                {Monstey_FxAttachment, L"Attachment..."},
                                                                {Monstey_Animations, L"Animations..."},
                                                                {Monstey_ModelPreview, L"Preview 3D..."},
                                                                {Monstey_Materials, L"Materials..."},
                                                                {Monstey_TexturePreview, L"Textures..."},
                                                                {Monstey_MaterialEdit, L"Edit value..."},
                                                                {Monstey_MaterialRaw, L"Raw bytes..."},
                                                                {Monstey_MaterialChanges, L"Changes"},
                                                                {Monstey_AnimationHeld, L"Held pose..."},
                                                                {Monstey_AnimationTiming, L"Timing..."},
                                                                {Monstey_AnimationEvents, L"Events..."},
                                                                {Monstey_AnimationEventAdd, L"Add event..."},
                                                                {Monstey_AnimationEventEdit, L"Edit event..."},
                                                                {Monstey_AnimationEventRemove, L"Remove event"},
                                                                {Monstey_AnimationSample, L"Sample ticks..."},
                                                                {Monstey_AnimationExportAll, L"Export scene..."},
                                                                {Monstey_AnimationSkeleton, L"SKEL library..."},
                                                                {Monstey_AnimationChanges, L"Changes"},
                                                                {Monstey_AnimationEventClear, L"Clear events"}};
            for (const auto &[id, text] : controls)
                self->buttons.push_back(control(L"BUTTON", text, WS_TABSTOP, id));
            self->tabs = control(WC_TABCONTROLW, L"", WS_TABSTOP, Monstey_Tabs);
            const wchar_t *tabNames[] = {L"Database", L"Archive", L"Strings", L"Effects",
                                         L"Models",   L"World",   L"Settings"};
            for (int i = 0; i < 7; ++i)
            {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                item.pszText = const_cast<LPWSTR>(tabNames[i]);
                TabCtrl_InsertItem(self->tabs, i, &item);
            }
            self->pathLabel = control(L"STATIC", L"No source opened", SS_LEFT, 0);
            control(L"BUTTON", L"Native declarations...", WS_TABSTOP, Monstey_NativeProfileChoose);
            control(L"BUTTON", L"Clear", WS_TABSTOP, Monstey_NativeProfileClear);
            self->nativeProfileLabel =
                control(L"STATIC", L"No native profile selected. Legacy build.", SS_LEFTNOWORDWRAP | SS_NOPREFIX, 0);
            self->query = control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, Monstey_Filter, WS_EX_CLIENTEDGE);
            control(L"BUTTON", L"Search", WS_TABSTOP, Monstey_Search);
            control(L"BUTTON", L"< Prev", WS_TABSTOP, Monstey_Previous);
            control(L"BUTTON", L"Next >", WS_TABSTOP, Monstey_Next);
            self->tree = control(WC_TREEVIEWW, L"", TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_TABSTOP,
                                 Monstey_Categories, WS_EX_CLIENTEDGE);
            self->records = control(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                                    Monstey_Records, WS_EX_CLIENTEDGE);
            self->fields = control(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                                   Monstey_Fields, WS_EX_CLIENTEDGE);
            for (const auto item : {self->records, self->fields})
                ListView_SetExtendedListViewStyle(item, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
            self->pageLabel = control(L"STATIC", L"", SS_LEFT, 0);
            self->details = control(L"EDIT",
                                    L"Open a native source to begin. Double-click a record to "
                                    L"open its properties.",
                                    ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, WS_EX_CLIENTEDGE);
            self->status = control(L"STATIC", L"Ready", SS_LEFT, 0);
            self->TabChanged();
            return 0;
        }
        case WM_SIZE:
            self->Layout();
            return 0;
        case WM_GETMINMAXINFO:
        {
            auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
            info->ptMinTrackSize = {1000, 650};
            return 0;
        }
        case WM_COMMAND:
            self->Command(LOWORD(wparam));
            return 0;
        case WM_NOTIFY:
        {
            const auto *header = reinterpret_cast<const NMHDR *>(lparam);
            if (header->hwndFrom == self->tabs && header->code == TCN_SELCHANGING && self->smokeNativeProfile)
            {
                ++self->Monstey_nativeProfileBlockedTabChanges;
                return TRUE;
            }
            if (header->hwndFrom == self->tabs && header->code == TCN_SELCHANGE)
            {
                self->TabChanged();
                return 0;
            }
            if (self->populating)
                break;
            if (header->hwndFrom == self->tree && header->code == TVN_ITEMEXPANDINGW && self->tab == 0 &&
                !self->Monstey_DatabaseForm())
            {
                const auto *change = reinterpret_cast<const NMTREEVIEWW *>(lparam);
                if (change->action == TVE_EXPAND)
                    self->Monstey_ExpandDatabaseTreeItem(change->itemNew.lParam);
                return 0;
            }
            if (header->hwndFrom == self->tree && (header->code == NM_DBLCLK || header->code == NM_RETURN) &&
                self->tab == 0 && !self->Monstey_DatabaseForm())
            {
                TVITEMW selected{};
                selected.mask = TVIF_PARAM;
                selected.hItem = TreeView_GetSelection(self->tree);
                if (selected.hItem && TreeView_GetItem(self->tree, &selected) != FALSE)
                    self->Monstey_SelectDatabaseTreeItem(selected.lParam, true);
                return 0;
            }
            if (header->hwndFrom == self->tree && header->code == TVN_SELCHANGEDW)
            {
                if (self->Monstey_DatabaseForm())
                    return 0;
                const auto *change = reinterpret_cast<const NMTREEVIEWW *>(lparam);
                if (self->tab == 3)
                    self->SelectFxView(static_cast<int>(change->itemNew.lParam));
                else if (self->tab == 0)
                    self->Monstey_SelectDatabaseTreeItem(change->itemNew.lParam);
                return 0;
            }
            if (header->hwndFrom == self->records && header->code == LVN_ITEMCHANGED)
            {
                const auto *change = reinterpret_cast<const NMLISTVIEW *>(lparam);
                if ((change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED))
                    self->ReadRecord(change->iItem);
                return 0;
            }
            if (header->code == NM_DBLCLK && (header->hwndFrom == self->records || header->hwndFrom == self->fields))
            {
                const auto *activation = reinterpret_cast<const NMITEMACTIVATE *>(lparam);
                const auto list = header->hwndFrom;
                const int row =
                    activation->iItem >= 0 ? activation->iItem : ListView_GetNextItem(list, -1, LVNI_SELECTED);
                if (list == self->records)
                    self->ReadRecord(row, true);
                else
                {
                    ++self->fieldDoubleClicks;
                    self->EditField(row);
                }
                return 0;
            }
            if (header->code == LVN_KEYDOWN && (header->hwndFrom == self->records || header->hwndFrom == self->fields))
            {
                const auto *key = reinterpret_cast<const NMLVKEYDOWN *>(lparam);
                if (self->Monstey_DatabaseForm() && (key->wVKey == VK_INSERT || key->wVKey == VK_DELETE))
                {
                    self->SetStatus(
                        L"Native arguments use their typed editor. Return to records for structural edits.");
                    return 0;
                }
                if (self->tab == 4 && self->materialMode && header->hwndFrom == self->records)
                {
                    if (key->wVKey == VK_INSERT || key->wVKey == VK_DELETE)
                    {
                        self->MaterialCommand(key->wVKey == VK_INSERT ? Monstey_Add : Monstey_Delete);
                        return 0;
                    }
                }
                if (self->tab == 4 && self->animationMode)
                {
                    if (header->hwndFrom == self->fields && self->animationEventMode &&
                        (key->wVKey == VK_INSERT || key->wVKey == VK_DELETE))
                    {
                        self->AnimationEvent(key->wVKey == VK_INSERT ? Monstey_AnimationEventAdd
                                                                     : Monstey_AnimationEventRemove);
                        return 0;
                    }
                    if (header->hwndFrom == self->records && key->wVKey == VK_DELETE)
                    {
                        self->AnimationCommand(Monstey_Delete);
                        return 0;
                    }
                }
                if (self->tab == 0 && key->wVKey == VK_INSERT)
                {
                    self->StructureCommand(header->hwndFrom == self->fields ? Monstey_AddField : Monstey_CreateRecord);
                    return 0;
                }
                if (self->tab == 3 && key->wVKey == VK_INSERT)
                {
                    if (header->hwndFrom == self->fields)
                        self->StructureCommand(Monstey_AddField);
                    else
                        self->FxCommand(Monstey_FxKeyCreate);
                    return 0;
                }
                if (self->tab == 3 && key->wVKey == VK_DELETE)
                {
                    if (header->hwndFrom == self->fields)
                        self->StructureCommand(Monstey_DeleteField);
                    else
                        self->FxCommand(self->fxView.starts_with("track-") ? Monstey_FxKeyUnlink : Monstey_FxKeyDelete);
                    return 0;
                }
                if (self->tab == 0 && header->hwndFrom == self->fields && key->wVKey == VK_DELETE)
                {
                    self->StructureCommand(Monstey_DeleteField);
                    return 0;
                }
                if (key->wVKey == VK_RETURN || key->wVKey == VK_F2)
                {
                    const auto list = header->hwndFrom;
                    const auto row = ListView_GetNextItem(list, -1, LVNI_SELECTED);
                    if (list == self->records)
                        self->ReadRecord(row, true);
                    else
                        self->EditField(row);
                    return 0;
                }
            }
            break;
        }
        case WM_INITMENU:
            self->UpdateButtons();
            return 0;
        case WM_TIMER:
            ++self->heartbeat;
            if (!self->smokeReport.empty())
                self->SmokeStep();
            return 0;
        case Monstey_ReplyMessage:
            self->PumpResults();
            return 0;
        case Monstey_WorkerStopped:
            if (self->worker.joinable())
                self->worker.join();
            if (self->smokePreview && self->smokePreviewClose && self->closing)
            {
                const std::size_t expected = self->smokePreviewAnimation ? 2u : 1u;
                self->smoke["current_preview_load_drained"] =
                    self->previewLoadsStarted == expected && self->previewLoadsCompleted == expected;
                self->smoke["queued_preview_load_canceled"] = self->previewLoadsStarted == expected;
                self->smoke["preview_loads_started"] = static_cast<std::uint64_t>(self->previewLoadsStarted);
                self->smoke["preview_loads_completed"] = static_cast<std::uint64_t>(self->previewLoadsCompleted);
                if (self->smokePreviewAnimation)
                {
                    auto skeleton = self->initialDatabase;
                    skeleton.replace_extension(L".skel");
                    self->smoke["animation_sources_unchanged"] =
                        Monstey_HashSha256(
                            std::filesystem::path(Monstey_Wide(self->smokeArguments.at("animation").as_string()))) ==
                            self->smoke.at("animation_source_sha256").as_string() &&
                        Monstey_HashSha256(skeleton) == self->smoke.at("preview_skeleton_sha256").as_string();
                }
                self->smoke["preview_closed_before_app_shutdown"] =
                    !self->modelPreview || !self->modelPreview->Handle();
                self->smoke["source_unchanged"] =
                    Monstey_HashSha256(self->initialDatabase) == self->smoke.at("source_sha256").as_string();
                self->smokeFailed =
                    !(self->smoke["current_preview_load_drained"].as_bool() &&
                      self->smoke["queued_preview_load_canceled"].as_bool() &&
                      self->smoke["preview_closed_before_app_shutdown"].as_bool() &&
                      self->smoke["source_unchanged"].as_bool() &&
                      (!self->smokePreviewAnimation || self->smoke["animation_sources_unchanged"].as_bool()));
                self->smoke["status"] = self->smokeFailed ? "FAIL" : "PASS";
                self->smoke["native_gui"] = true;
                self->smoke["implementation"] = "C++20/Win32";
                self->smoke["heartbeat_ticks"] = self->heartbeat;
                self->smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
                try
                {
                    Monstey_WriteNewText(self->smokeReport, self->smoke.dump(2) + "\n");
                }
                catch (...)
                {
                    self->smokeFailed = true;
                }
            }
            if (self->smokeCloseDuringBuild && self->closing &&
                self->smoke.value("closed_while_writer_active", false).as_bool())
            {
                const auto candidate = self->smokeDirectory / L"candidate.gamedb";
                const auto receipt = std::filesystem::path(candidate.wstring() + L".report.json");
                const bool finished =
                    std::filesystem::is_regular_file(candidate) && std::filesystem::is_regular_file(receipt);
                const bool canceled = !std::filesystem::exists(self->smokeDirectory / L"queued-must-not-run.json");
                self->smoke["current_writer_drained"] = finished;
                self->smoke["queued_operation_canceled"] = canceled;
                self->smoke["source_unchanged"] =
                    Monstey_HashSha256(self->initialDatabase) == self->smoke.at("source_sha256").as_string();
                self->smoke["native_gui"] = true;
                self->smoke["python_runtime"] = false;
                self->smoke["heartbeat_ticks"] = self->heartbeat;
                self->smokeFailed = !(finished && canceled && self->smoke.at("source_unchanged").as_bool());
                self->smoke["status"] = self->smokeFailed ? "FAIL" : "PASS";
                if (finished)
                    self->smoke["native_build"] = J::Parse(Monstey_ReadText(receipt));
                try
                {
                    Monstey_WriteNewText(self->smokeReport, self->smoke.dump(2) + "\n");
                }
                catch (...)
                {
                    self->smokeFailed = true;
                }
            }
            DestroyWindow(window);
            return 0;
        case WM_CLOSE:
            self->Close();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_NCDESTROY:
            self->window = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return DefWindowProcW(window, message, wparam, lparam);
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        self->Error(e.what());
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace Monstey::Monstey_AppInternal
