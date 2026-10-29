#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::Command(int command)
{
    if (command == Monstey_InspectStorage)
    {
        Monstey_SowAtomCommand();
        return;
    }
    if (command == Monstey_NativeBanksMenu || command == Monstey_NativeClassMenu ||
        command == Monstey_DatabaseRecordsMenu || command == Monstey_NativeDatabaseTools)
    {
        Monstey_DatabaseNativeCommand(command);
        return;
    }
    if (Monstey_DatabaseForm() && command != Monstey_Exit && command != Monstey_About &&
        command != Monstey_ConfigureMenu && command != Monstey_CatalogMenu && command != Monstey_MountsMenu)
    {
        if (command == Monstey_Open || command == Monstey_Load || command == Monstey_Save || command == Monstey_Build ||
            command == Monstey_Revert || command == Monstey_Search)
            FormCommand(command);
        else
            SetStatus(L"Return to records to edit the database.");
        return;
    }
    if (command == Monstey_NativeProfileChoose || command == Monstey_NativeProfileClear)
    {
        if ((tab == 0 || tab == 3) && !closing)
            NativeProfileCommand(command);
        return;
    }
    if (command == Monstey_TexturePreview)
    {
        if (tab == 4 && !closing)
            TexturePreviewCommand();
        return;
    }
    if (command == Monstey_ModelPreview)
    {
        if (tab == 4 && !closing)
            ModelPreviewCommand();
        return;
    }
    if (tab == 3 && fxLoading &&
        (command == Monstey_Undo || command == Monstey_Redo || command == Monstey_Revert || command == Monstey_Save ||
         command == Monstey_Build))
    {
        SetStatus(L"Wait for the native effect refresh to finish.");
        return;
    }
    if (command == Monstey_Materials ||
        (tab == 4 && materialMode && command != Monstey_Exit && command != Monstey_About && command != Monstey_Schema))
    {
        MaterialCommand(command);
        return;
    }
    if (command == Monstey_Animations ||
        (tab == 4 && animationMode && command != Monstey_Exit && command != Monstey_About && command != Monstey_Schema))
    {
        AnimationCommand(command);
        return;
    }
    if (command == Monstey_FxAttachment)
    {
        FxAttachmentCommand();
        return;
    }
    if (command >= Monstey_FxNested && command <= Monstey_FxNestedBack)
    {
        FxNestedCommand(command);
        return;
    }
    if (command == Monstey_Extras || command == Monstey_ExtraUsage)
    {
        SocketCommand(command);
        return;
    }
    if (command == Monstey_Envelope)
    {
        if (tab == 4)
            ModelEnvelope();
        return;
    }
    if (command >= Monstey_FxLibrary && command <= Monstey_FxPrefabSync)
    {
        FxLibraryCommand(command);
        return;
    }
    if ((command >= Monstey_Sockets && command <= Monstey_SocketNext) ||
        (tab == 4 && socketMode && command != Monstey_Exit && command != Monstey_About && command != Monstey_Schema))
    {
        SocketCommand(command);
        return;
    }
    if (command >= Monstey_Effects && command <= Monstey_FxReconcile)
    {
        FxCommand(command);
        return;
    }
    if (command >= Monstey_Structure && command <= Monstey_MoveFieldDown)
    {
        StructureCommand(command);
        return;
    }
    if (command == Monstey_ConfigureMenu || command == Monstey_CatalogMenu || command == Monstey_MountsMenu)
    {
        preferredOperation = command == Monstey_ConfigureMenu ? "configure"
                             : command == Monstey_CatalogMenu ? "archive_catalog"
                                                              : "archive_mounts";
        TabCtrl_SetCurSel(tabs, 6);
        TabChanged();
        return;
    }
    if (tab >= 4 && command != Monstey_Exit && command != Monstey_Schema && command != Monstey_About)
    {
        FormCommand(command);
        return;
    }
    if (command == Monstey_Open)
    {
        OpenSource();
        return;
    }
    if (command == Monstey_Exit)
    {
        Close();
        return;
    }
    if (command == Monstey_Schema)
    {
        const auto chosen = Monstey_FileDialog(window, false, L"Native schema (*.json)\0*.json\0\0", L"json",
                                               L"Choose native GADB schema");
        if (chosen)
        {
            if (tab == 3)
                fxSchema = *chosen;
            else
                schema = *chosen;
            SetStatus(L"Schema selected. Reopen the database or effect to use it.");
        }
        return;
    }
    if (command == Monstey_About)
    {
        MessageBoxW(window,
                    L"Monstey SoW Workbench\r\nNative C++20, Win32, GADB17, "
                    L"LTAR3/4, SKDB9 and LTFX8.\r\nBuilds are "
                    L"written to new files and read back before publication.",
                    L"About", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (command == Monstey_Search)
    {
        offset = 0;
        Refresh();
        return;
    }
    if (command == Monstey_Previous)
    {
        offset = offset >= 250 ? offset - 250 : 0;
        Refresh();
        return;
    }
    if (command == Monstey_Next)
    {
        if (offset + 250 < total)
            offset += 250;
        Refresh();
        return;
    }
    const std::string prefix = tab == 0 ? "database_" : tab == 1 ? "archive_" : tab == 2 ? "strings_" : "fx_";
    if (command == Monstey_Undo || command == Monstey_Redo || command == Monstey_Revert)
    {
        Transaction(prefix + (command == Monstey_Undo ? "undo" : command == Monstey_Redo ? "redo" : "revert"));
        return;
    }
    if (command == Monstey_New)
    {
        if (tab == 0)
        {
            if (dirty[0] && smokeReport.empty() &&
                MessageBoxW(window, L"Discard unsaved database edits and create an empty database?", L"New database",
                            MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
                return;
            ++epoch;
            Queue(
                "database_new", J::object(),
                [this](const J &reply)
                {
                    if (!reply.at("ok").as_bool())
                    {
                        Error(reply.at("error").at("message").as_string());
                        return;
                    }
                    category.reset();
                    categoryRows.clear();
                    selector.clear();
                    rowData.clear();
                    fieldData.clear();
                    metadata = J::object();
                    TreeView_DeleteAllItems(tree);
                    ListView_DeleteAllItems(records);
                    ListView_DeleteAllItems(fields);
                    SetWindowTextW(query, L"");
                    UpdateState(reply.at("result"));
                    sourceNames[0] = L"Untitled database";
                    UpdateState(J::object());
                    ShowReport(reply.at("result"));
                    LoadCategories();
                    if (smokeStructure)
                    {
                        smoke["empty_database_created"] = true;
                        smokeStage = 1;
                        SmokeStep();
                    }
                },
                false);
            return;
        }
        if (tab == 3)
        {
            if (dirty[3] && smokeReport.empty() &&
                MessageBoxW(window,
                            L"Discard unsaved effect edits and create a new native "
                            L"effect project?",
                            L"New effect", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
                return;
            ++epoch;
            J a = J::object();
            if (!fxSchema.empty())
                a["schema_path"] = Monstey_JsonPath(fxSchema);
            Queue(
                "fx_new", a,
                [this](const J &reply)
                {
                    if (!reply.at("ok").as_bool())
                    {
                        Error(reply.at("error").at("message").as_string());
                        return;
                    }
                    UpdateState(reply.at("result"), 3);
                    fxNestedMode = false;
                    fxNestedView = J::object();
                    ++fxNestedRequest;
                    fxWorkflow = J::object();
                    fxGlobal = J::object();
                    fxView = "all";
                    selector.clear();
                    metadata = J::object();
                    fieldData.clear();
                    ListView_DeleteAllItems(fields);
                    LoadFx();
                    FxCommand(Monstey_FxCreate);
                },
                false);
            return;
        }
        if (dirty[tab] && smokeReport.empty() &&
            MessageBoxW(window, L"Discard edits and create a new project?", L"New project",
                        MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        ++epoch;
        offset = 0;
        selector.clear();
        SetWindowTextW(query, L"");
        ClearStorageRows();
        sourceNames[tab] = L"New project";
        Transaction(tab == 1 ? "archive_project_new" : "strings_new", J::object());
        return;
    }
    if (command == Monstey_Load)
    {
        const auto chosen = Monstey_FileDialog(window, false,
                                               tab == 1 ? L"Archive project (*.msap)\0*.msap\0All files\0*.*\0\0"
                                                        : L"Workbench project (*.json)\0*.json\0All files\0*.*\0\0",
                                               tab == 1 ? L"msap" : L"json", L"Open project");
        if (!chosen)
            return;
        ++epoch;
        ClearStorageRows();
        Queue(
            prefix + (tab == 0 || tab == 1 ? "load_project" : "load_project"),
            J::object({{"path", Monstey_JsonPath(*chosen)}}),
            [this](const J &reply)
            {
                if (!reply.at("ok").as_bool())
                {
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                categoryRows.clear();
                category.reset();
                selector.clear();
                SetWindowTextW(query, L"");
                UpdateState(reply.at("result"));
                Refresh(true);
            },
            false);
        return;
    }
    if (command == Monstey_Save)
    {
        const auto chosen = Monstey_FileDialog(window, true,
                                               tab == 1 ? L"Archive project (*.msap)\0*.msap\0\0"
                                                        : L"Workbench project (*.json)\0*.json\0\0",
                                               tab == 1 ? L"msap" : L"json", L"Save NEW project file");
        if (chosen)
            Queue(prefix + (tab == 0 || tab == 1 ? "save_project" : "save_project"),
                  J::object({{"path", Monstey_JsonPath(*chosen)}}),
                  [this](const J &reply)
                  {
                      if (!reply.at("ok").as_bool())
                      {
                          Error(reply.at("error").at("message").as_string());
                          return;
                      }
                      ShowReport(reply.at("result"));
                  });
        return;
    }
    if (command == Monstey_Build)
    {
        const wchar_t *extension = tab == 0 ? L"gamedb" : tab == 1 ? L"arch06" : tab == 2 ? L"strdb" : L"fxp";
        const auto chosen = Monstey_FileDialog(window, true, Monstey_SourceFilter(tab), extension,
                                               L"Build to a NEW file; existing files are rejected");
        if (chosen)
        {
            auto arguments = J::object({{"output", Monstey_JsonPath(*chosen)}});
            const int profileIndex = tab == 3 ? 1 : 0;
            const bool profileMode = tab == 0 || tab == 3;
            const auto profileToken = nativeProfileSerial[profileIndex];
            if (profileMode && !nativeProfilePath[profileIndex].empty())
            {
                arguments["native_declarations"] = Monstey_JsonPath(nativeProfilePath[profileIndex]);
                arguments["expected_native_declarations_sha256"] = nativeProfileSha[profileIndex];
            }
            if (smokeNativeProfile)
                smoke["profile_build_arguments"] = arguments;
            Queue(prefix + "build", arguments,
                  [this, profileIndex, profileMode, profileToken](const J &reply)
                  {
                      if (smokeNativeProfile)
                          smoke["profile_build_reply"] = reply;
                      if (!reply.at("ok").as_bool())
                      {
                          if (profileMode && profileToken == nativeProfileSerial[profileIndex])
                          {
                              nativeProfileBuildStatus[profileIndex] = "REJECTED";
                              NativeProfileLabel();
                          }
                          Error(reply.at("error").at("message").as_string());
                          return;
                      }
                      if (profileMode && profileToken == nativeProfileSerial[profileIndex])
                      {
                          nativeProfileBuildStatus[profileIndex] =
                              reply["result"].contains("native_class_cache")
                                  ? reply["result"]["native_class_cache"]["status"].as_string()
                                  : "legacy";
                          NativeProfileLabel();
                      }
                      ShowReport(reply.at("result"));
                      SetStatus(L"Build verified. The input source was not overwritten.");
                  });
        }
        return;
    }
    const int selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    const J row = selected >= 0 && static_cast<std::size_t>(selected) < rowData.size()
                      ? rowData[static_cast<std::size_t>(selected)]
                      : J::object();
    if (command == Monstey_Clone && tab == 0)
    {
        if (selector.empty())
            return;
        Prompt(L"New clone name", "database_clone", J::object({{"source", selector}}), "new_name", "Monstey_",
               [this](const J &result)
               {
                   UpdateState(result);
                   if (result.contains("clone"))
                       SetWindowTextW(query, Monstey_Display(result.at("clone").at("id")).c_str());
                   Refresh();
               });
        return;
    }
    if (command == Monstey_Rename)
    {
        if (tab == 0 && !selector.empty())
            Prompt(L"Rename record", "database_rename", J::object({{"record", selector}}), "new_name",
                   metadata.at("name").as_string(),
                   [this](const J &result)
                   {
                       UpdateState(result);
                       SetWindowTextW(query, L"");
                       Refresh();
                   });
        else if (tab == 1 && row.contains("path"))
            Prompt(L"Rename virtual path", "archive_rename", J::object({{"old", row.at("path")}}), "new",
                   row.at("path").as_string(),
                   [this](const J &result)
                   {
                       UpdateState(result);
                       Refresh();
                   });
        else if (tab == 2 && row.contains("hash"))
            Prompt(L"New string key or hash", "strings_rename", J::object({{"old", row.at("hash")}}), "new",
                   row.at("hash").as_string(),
                   [this](const J &result)
                   {
                       UpdateState(result);
                       Refresh();
                   });
        return;
    }
    if (command == Monstey_Delete)
    {
        if (tab == 0 && !selector.empty())
        {
            auto item = std::make_shared<Monstey_Editor>();
            item->app = this;
            item->title = L"Deletion replacement policy";
            item->field = J::object({{"name", "Replacement (null rejects incoming links; 0xffffffff nullifies)"},
                                     {"native_type", 10},
                                     {"array", false},
                                     {"value", nullptr}});
            item->operation = "database_delete";
            item->arguments = J::object({{"record", selector}});
            item->valueKey = "replacement";
            item->generation = generation;
            item->revision = revision;
            item->success = [this](const J &result)
            {
                UpdateState(result);
                Refresh();
            };
            ShowEditor(item);
        }
        else if (tab == 1 && row.contains("path"))
            Transaction("archive_remove", J::object({{"virtual", row.at("path")}}));
        else if (tab == 2 && row.contains("hash"))
            Transaction("strings_remove", J::object({{"key", row.at("hash")}}));
        return;
    }
    if (command == Monstey_Add && tab == 1)
    {
        const auto source = Monstey_FileDialog(window, false, L"All files\0*.*\0\0", nullptr, L"Choose file to add");
        if (source)
            Prompt(L"Virtual archive path", "archive_add", J::object({{"source", Monstey_JsonPath(*source)}}),
                   "virtual", Monstey_Utf8(source->filename().wstring()),
                   [this](const J &result)
                   {
                       UpdateState(result);
                       Refresh();
                   });
        return;
    }
    if (command == Monstey_Add && tab == 2)
    {
        Prompt(L"New string key or hash", "", J::object(), "key", "Monstey_",
               [this](const J &key)
               {
                   Prompt(L"New Unicode text", "strings_set", J::object({{"key", key}}), "text", "",
                          [this](const J &result)
                          {
                              UpdateState(result);
                              Refresh();
                          });
               });
        return;
    }
    if (command == Monstey_Replace && tab == 1 && row.contains("path") && !row.value("folder", false).as_bool())
    {
        const auto source =
            Monstey_FileDialog(window, false, L"All files\0*.*\0\0", nullptr, L"Choose replacement payload");
        if (source)
            Transaction("archive_replace",
                        J::object({{"virtual", row.at("path")}, {"source", Monstey_JsonPath(*source)}}));
        return;
    }
    if (command == Monstey_Extract && tab == 1 && row.contains("path") && !row.value("folder", false).as_bool())
    {
        const auto output = Monstey_FileDialog(window, true, L"All files\0*.*\0\0", nullptr, L"Extract to a NEW file");
        if (output)
            Queue("extract", J::object({{"virtual", row.at("path")}, {"destination", Monstey_JsonPath(*output)}}),
                  [this](const J &reply)
                  {
                      if (!reply.at("ok").as_bool())
                      {
                          Error(reply.at("error").at("message").as_string());
                          return;
                      }
                      ShowReport(reply.at("result"));
                  });
        return;
    }
    if (command == Monstey_Folder && tab == 1)
    {
        Prompt(L"New virtual folder", "archive_create_folder", J::object(), "virtual", "new_folder",
               [this](const J &result)
               {
                   UpdateState(result);
                   Refresh();
               });
        return;
    }
    if (command == Monstey_Dependencies && tab == 3)
    {
        Queue("fx_dependencies", J::object(),
              [this](const J &reply)
              {
                  if (reply.at("ok").as_bool())
                      ShowReport(reply.at("result"));
                  else
                      Error(reply.at("error").at("message").as_string());
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
