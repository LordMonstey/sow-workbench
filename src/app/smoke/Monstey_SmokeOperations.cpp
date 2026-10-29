#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeStep()
{
    if (Monstey_SowAtomSmoke)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 450)
        {
            Error("SoW atom GUI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        Monstey_SowAtomSmokeStep();
        return;
    }
    // A common file dialog pumps messages. Keep this explicit test state machine
    // from reentering its still-active Source/Back continuation.
    const bool nativeDatabaseSmoke = smokeDomain == 0 && !smokeOperation.empty();
    if (nativeDatabaseSmoke && databaseNativeSmokeBusy)
        return;
    struct Monstey_DatabaseSmokeGuard
    {
        bool *busy;
        ~Monstey_DatabaseSmokeGuard()
        {
            if (busy)
                *busy = false;
        }
    } guard{nativeDatabaseSmoke ? &databaseNativeSmokeBusy : nullptr};
    if (nativeDatabaseSmoke)
        databaseNativeSmokeBusy = true;
    if (smokeNativeProfile)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 900)
        {
            Error("Native profile UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeNativeProfileStep();
        return;
    }
    if (smokeTexture)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 600)
        {
            Error("Texture GUI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeTextureStep();
        return;
    }
    if (smokeMaterial)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 1800)
        {
            Error("Material GUI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeMaterialStep();
        return;
    }
    if (smokePreview)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 1800)
        {
            Error("Preview UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokePreviewStep();
        return;
    }
    if (smokeAttachment)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 1800)
        {
            Error("Attachment UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeAttachmentStep();
        return;
    }
    if (smokeAnimation)
    {
        if (smokeReport.empty() || closing)
            return;
        if (heartbeat > 1800)
        {
            Error("Animation UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeAnimationStep();
        return;
    }
    if (smokeReport.empty() || closing)
        return;
    if (smokeArchive || smokeStrings)
    {
        if (heartbeat > 1800)
        {
            Error("Storage UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeStorageStep();
        return;
    }
    if (smokeExtras)
    {
        if (heartbeat > 1800)
        {
            Error("Animated extras UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        SmokeExtrasStep();
        return;
    }
    if (smokeNested)
    {
        if (heartbeat > 1800)
        {
            Error("Nested FX UI smoke timed out at " + std::to_string(smokeStage));
            return;
        }
        if (smokeStage == 6 && fxWorkflow.contains("active_keys") && fxWorkflow.at("active_keys").size() == 1 &&
            (!editor || !editor->window) && metadata.value("category", "") == J("ClientFX.Create"))
        {
            Monstey_Require(smoke.value("required_value_dialogs", 0).as_uint() ==
                                smoke.value("required_native_dialogs_expected", 0).as_uint(),
                            "CreateFX required dialogs differ from native schema");
            smokeStage = 100;
        }
        if (smokeStage >= 50)
            SmokeNestedStep();
        else
            SmokeEffectsStep();
        return;
    }
    if (smokeEnvelope)
    {
        if (heartbeat > 1800)
        {
            Error("Envelope UI smoke timeout");
            return;
        }
        if (smokeStage == 2 && editor && editor->window && !editor->pending && editor->formControls.size() == 1)
        {
            smokeStage = 3;
            SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
        }
        return;
    }
    if (smokeSockets)
    {
        if (heartbeat > 1800)
        {
            Error("Socket UI smoke timeout at " + std::to_string(smokeStage));
            return;
        }
        SmokeSocketsStep();
        return;
    }
    if (smokePrefabs)
    {
        if (heartbeat > 1800)
        {
            Error("Prefab UI smoke timeout at " + std::to_string(smokeStage));
            return;
        }
        if (smokeStage == 6 && fxWorkflow.contains("active_keys") && fxWorkflow.at("active_keys").size() == 1 &&
            (!editor || !editor->window) && rowData.size() == 1)
            smokeStage = 100;
        if (smokeStage >= 100)
            SmokePrefabStep();
        else
            SmokeEffectsStep();
        return;
    }
    if (smokeEffects)
    {
        if (heartbeat > 1800)
        {
            Error("Native Effects UI smoke timed out at stage " + std::to_string(smokeStage));
            return;
        }
        SmokeEffectsStep();
        return;
    }
    if (smokeStructure)
    {
        if (heartbeat > 1800)
        {
            Error("Native structural UI smoke timed out");
            return;
        }
        SmokeStructureStep();
        return;
    }
    if (smokeCloseDuringBuild && smokeStage == 7)
    {
        bool writer = false;
        {
            std::lock_guard lock(mutex);
            writer = active && currentOperation == "database_build";
        }
        if (writer)
        {
            smoke["closed_while_writer_active"] = true;
            Close();
        }
        return;
    }
    if (heartbeat > 1800)
    {
        Error("Native GUI smoke test timed out");
        return;
    }

    if (smokeDomain == 0 && !smokeOperation.empty())
    {
        if (smokeStage == -2 && !Monstey_DatabaseForm() && !metadata.empty() && !selector.empty())
        {
            Monstey_DatabaseNativeSmokeStart();
            return;
        }
        if (smokeStage == 15 && !databaseNativeMode && !metadata.empty() &&
            selector == smoke.at("native_database_before_selector").as_string())
        {
            smokeStage = 16;
            Monstey_Require(tab == 0 && (GetWindowLongPtrW(tree, GWL_STYLE) & WS_VISIBLE) != 0 &&
                                ListView_GetNextItem(records, -1, LVNI_SELECTED) >= 0,
                            "Back did not restore the actual Database record view");
            Monstey_Require(
                sourceNames[0] == Monstey_Wide(smoke.at("native_database_before_source_label").as_string()) &&
                    Monstey_Utf8(Monstey_WindowText(query)) == smoke.at("native_database_before_filter").as_string() &&
                    offset == smoke.at("native_database_before_offset").as_uint() &&
                    dirty[0] == smoke.at("native_database_before_dirty").as_bool() &&
                    canUndo[0] == smoke.at("native_database_before_can_undo").as_bool() &&
                    canRedo[0] == smoke.at("native_database_before_can_redo").as_bool(),
                "Native view changed Database source/selection/dirty history");
            smoke["native_database_records_restored"] = true;
            Queue("describe", J::object(),
                  [this](const J &after)
                  {
                      Monstey_Require(after.at("ok").as_bool() &&
                                          after.at("result") == smoke.at("native_database_before_project") &&
                                          after.at("generation") == smoke.at("native_database_before_generation") &&
                                          after.at("revision") == smoke.at("native_database_before_revision"),
                                      "Native UI changed the actual Runtime Database project or counters");
                      Queue("database_changes", J::object(),
                            [this](const J &changes)
                            {
                                Monstey_Require(changes.at("ok").as_bool() &&
                                                    changes.at("result") == smoke.at("native_database_before_changes"),
                                                "Native UI changed unsaved Database history");
                                Monstey_Require(
                                    !smoke.value("native_database_stale_callback_executed", false).as_bool(),
                                    "A stale native-mode callback was accepted after Back");
                                const auto source =
                                    std::filesystem::path(Monstey_Wide(smokeArguments.at("path").as_string()));
                                smoke["source_unchanged"] =
                                    Monstey_HashSha256(source) == smokeArguments.at("expected_sha256").as_string();
                                Monstey_Require(smoke.at("source_unchanged").as_bool(),
                                                "Native GUI input source changed");
                                smoke["native_database_project_history_preserved"] = true;
                                smoke["native_database_after_generation"] = changes.at("generation");
                                smoke["native_database_after_revision"] = changes.at("revision");
                                SmokeComplete();
                            });
                  });
            return;
        }
    }

    if (!smokeOperation.empty())
    {
        const bool bandSmoke = (smokeOperation == "model_convert" || smokeOperation == "model_export_blend" ||
                                smokeOperation == "model_export_native") &&
                               smokeArguments.contains("triangle_band");
        if (bandSmoke)
        {
            auto openBand = [&]()
            {
                const auto row = std::find_if(fieldData.begin(), fieldData.end(), [](const J &field)
                                              { return field.at("name").as_string() == "triangle_band"; });
                Monstey_Require(row != fieldData.end(), "Native triangle band argument row is missing");
                const int index = static_cast<int>(row - fieldData.begin());
                Monstey_ListSelect(fields, index);
                NMITEMACTIVATE activation{};
                activation.hdr.hwndFrom = fields;
                activation.hdr.idFrom = Monstey_Fields;
                activation.hdr.code = NM_DBLCLK;
                activation.iItem = index;
                SendMessageW(window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&activation));
            };
            if (smokeStage == 0 && !selectedOperation.empty() &&
                selectedOperation.at("name").as_string() == smokeOperation)
            {
                const auto band = smokeArguments.at("triangle_band").as_int();
                Monstey_Require(band >= 0 && band <= 16, "Smoke triangle band is outside 0..16");
                smoke["native_operation_selected"] = true;
                smoke["triangle_band_expected"] = band;
                if (smokeOperation == "model_convert")
                {
                    operationArguments.as_object().erase("triangle_band");
                    if (!operationArguments.contains("options"))
                        operationArguments["options"] = J::object();
                    operationArguments["options"]["triangle_band"] = band;
                }
                else
                    operationArguments["triangle_band"] = band;
                operationPresets[smokeOperation] = operationArguments;
                PopulateOperationFields();
                const auto row = std::find_if(fieldData.begin(), fieldData.end(), [](const J &field)
                                              { return field.at("name").as_string() == "triangle_band"; });
                Monstey_Require(row != fieldData.end() && row->at("present").as_bool() &&
                                    row->at("value").as_int() == band,
                                "Nested triangle band is not displayed as effective");
                smoke["effective_triangle_band_displayed"] = true;
                smoke["nested_triangle_band_displayed"] = smokeOperation == "model_convert";
                smoke["typed_argument_rows"] = fieldData.size();
                smokeStage = 20;
                openBand();
                return;
            }
            if (smokeStage == 20 && editor && editor->window)
            {
                Monstey_Require(editor->formControls.size() == 1 && editor->formFields[0].at("choices").size() == 18 &&
                                    SendMessageW(editor->formControls[0], CB_GETCOUNT, 0, 0) == 18 &&
                                    SendMessageW(editor->formControls[0], CB_GETCURSEL, 0, 0) ==
                                        smoke.at("triangle_band_expected").as_int() + 1,
                                "Native triangle band combo does not show its effective value");
                smoke["argument_editor_visible"] = IsWindowVisible(editor->window) != FALSE;
                smoke["triangle_band_choices"] = 18;
                smokeStage = 21;
                SendMessageW(editor->formControls[0], CB_SETCURSEL, 0, 0);
                SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
                return;
            }
            if (smokeStage == 21 && (!editor || !editor->window))
            {
                Monstey_Require(!operationArguments.contains("triangle_band") &&
                                    (!operationArguments.contains("options") ||
                                     !operationArguments.at("options").contains("triangle_band")),
                                "Keep did not clear both triangle band overrides");
                smoke["triangle_band_keep_cleared_overrides"] = true;
                smokeStage = 22;
                openBand();
                return;
            }
            if (smokeStage == 22 && editor && editor->window)
            {
                Monstey_Require(SendMessageW(editor->formControls[0], CB_GETCURSEL, 0, 0) == 0,
                                "Keep is not selected after clearing the overrides");
                SendMessageW(editor->formControls[0], CB_SETCURSEL, smoke.at("triangle_band_expected").as_int() + 1, 0);
                smokeStage = 23;
                SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
                return;
            }
            if (smokeStage == 23 && (!editor || !editor->window))
            {
                Monstey_Require(operationArguments.at("triangle_band") == smoke.at("triangle_band_expected"),
                                "Native triangle band choice was not committed");
                smoke["triangle_band_choice_committed"] = true;
                smokeStage = 13;
                SendMessageW(window, WM_COMMAND, Monstey_Build, 0);
                return;
            }
            return;
        }
        if (smokeStage == 0 && !selectedOperation.empty() && selectedOperation.at("name").as_string() == smokeOperation)
        {
            smoke["native_operation_selected"] = true;
            smoke["typed_argument_rows"] = fieldData.size();
            if (Monstey_DatabaseForm() && !smoke.value("native_database_source_action", false).as_bool())
            {
                const auto dbMenu = GetSubMenu(GetMenu(window), 2);
                Monstey_Require(GetMenuState(dbMenu, Monstey_NativeBanksMenu, MF_BYCOMMAND) != UINT(-1) &&
                                    GetMenuState(dbMenu, Monstey_NativeClassMenu, MF_BYCOMMAND) != UINT(-1),
                                "Native Database menu entries missing");
                smoke["native_database_menu_route"] = true;
                operationArguments.as_object().erase("path");
                operationArguments.as_object().erase("expected_sha256");
                Monstey_SmokeDialogPaths.push_back(
                    std::filesystem::path(Monstey_Wide(smokeArguments.at("path").as_string())));
                SendMessageW(window, WM_COMMAND, Monstey_Open, 0);
                Monstey_Require(operationArguments.at("expected_sha256") == smokeArguments.at("expected_sha256"),
                                "Native Source action did not compute the actual PE hash");
                smoke["native_database_source_action"] = true;
                const auto originalName = selectedOperation.at("name").as_string();
                const auto otherName =
                    originalName == "database_native_class" ? "database_native_banks" : "database_native_class";
                int originalIndex = -1, otherIndex = -1;
                for (std::size_t i = 0; i < rowData.size(); ++i)
                {
                    if (rowData[i].at("name").as_string() == originalName)
                        originalIndex = static_cast<int>(i);
                    if (rowData[i].at("name").as_string() == otherName)
                        otherIndex = static_cast<int>(i);
                }
                Monstey_Require(originalIndex >= 0 && otherIndex >= 0 && rowData.size() == 2,
                                "Database native form did not expose exactly the two real PE readers");
                const auto sourceArgs = databaseNativeSource;
                SelectOperation(otherIndex);
                Monstey_Require(operationArguments.at("path") == sourceArgs.at("path") &&
                                    operationArguments.at("expected_sha256") == sourceArgs.at("expected_sha256"),
                                "Native readers lost shared source/hash");
                SelectOperation(originalIndex);
                Monstey_ListSelect(records, originalIndex);
                smoke["native_database_shared_source_hash"] = true;
                const auto beforeArguments = operationArguments;
                for (const auto command : {Monstey_Undo, Monstey_Redo, Monstey_Delete, Monstey_Clone,
                                           Monstey_CreateRecord, Monstey_Structure})
                    Command(command);
                StructureCommand(Monstey_AddField);
                for (const auto key : {VK_INSERT, VK_DELETE})
                {
                    NMLVKEYDOWN event{};
                    event.hdr.hwndFrom = fields;
                    event.hdr.idFrom = Monstey_Fields;
                    event.hdr.code = LVN_KEYDOWN;
                    event.wVKey = static_cast<WORD>(key);
                    SendMessageW(window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&event));
                }
                Monstey_Require(operationArguments == beforeArguments, "Record commands changed native form arguments");
                smoke["native_database_record_routes_guarded"] = true;
                auto types = J::object();
                for (const auto &field : fieldData)
                    types[field.at("name").as_string()] = field.at("type_name");
                smoke["native_database_argument_types"] = types;
                Monstey_Require(
                    types.at("path") == J("string") && types.at("expected_sha256") == J("string") &&
                        types.at("offset") == J("integer") && types.at("limit") == J("integer") &&
                        (originalName != "database_native_class" || types.at("class_pointer") == J("string")),
                    "Native Database typed argument form differs from public schema");
                smoke["native_database_stale_callback_executed"] = false;
                smoke["native_database_stale_probe_queued"] = true;
                Queue("database_native_class",
                      J::object({{"path", sourceArgs.at("path")},
                                 {"expected_sha256", sourceArgs.at("expected_sha256")},
                                 {"class_pointer", "0x14263c050"},
                                 {"limit", 1}}),
                      [this](const J &)
                      {
                          smoke["native_database_stale_callback_executed"] = true;
                          Error("Stale native callback reached the restored view");
                      });
                const auto previousEpoch = epoch;
                Command(Monstey_DatabaseRecordsMenu);
                Command(originalName == "database_native_class" ? Monstey_NativeClassMenu : Monstey_NativeBanksMenu);
                Monstey_Require(epoch >= previousEpoch + 2, "Native view changes did not invalidate queued reads");
                smoke["native_database_mode_epoch_invalidated"] = true;
                return;
            }

            if (!fieldData.empty())
            {
                smokeStage = 10;
                int chosen = 0;
                for (std::size_t i = 0; i < fieldData.size(); ++i)
                    if (fieldData[i].at("present").as_bool())
                    {
                        chosen = static_cast<int>(i);
                        break;
                    }
                Monstey_ListSelect(fields, chosen);
                NMITEMACTIVATE activation{};
                activation.hdr.hwndFrom = fields;
                activation.hdr.idFrom = Monstey_Fields;
                activation.hdr.code = NM_DBLCLK;
                activation.iItem = chosen;
                SendMessageW(window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&activation));
            }
            else
                smokeStage = 12;
        }
        if (smokeStage == 10 && editor && editor->window)
        {
            smoke["argument_editor_visible"] = IsWindowVisible(editor->window) != FALSE;
            smokeStage = 12;
            SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
        }
        if (smokeStage == 12 && (!editor || !editor->window))
        {
            smokeStage = 13;
            SendMessageW(window, WM_COMMAND, Monstey_Build, 0);
        }
        return;
    }
    if (smokeStage == 0 && !rowData.empty())
    {
        smokeStage = 1;
        NMITEMACTIVATE activation{};
        activation.hdr.hwndFrom = records;
        activation.hdr.idFrom = Monstey_Records;
        activation.hdr.code = NM_DBLCLK;
        activation.iItem = 0;
        SendMessageW(window, WM_NOTIFY, Monstey_Records, reinterpret_cast<LPARAM>(&activation));
        return;
    }
    if (smokeStage == 1 && inspector && inspector->window)
    {
        smoke["record_properties_visible"] = IsWindowVisible(inspector->window) != FALSE;
        const auto &rows = inspector->metadata.at("fields").as_array();
        int selected = -1;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].at("editable").as_bool() && !rows[i].at("array").as_bool() &&
                rows[i].at("native_type").as_int() == 2)
            {
                selected = static_cast<int>(i);
                if (rows[i].at("name").as_string() == "InitialValue")
                    break;
            }
        if (selected < 0)
        {
            Error("No editable scalar float field in smoke record");
            return;
        }
        smokeStage = 2;
        Monstey_ListSelect(inspector->list, selected);
        NMITEMACTIVATE activation{};
        activation.hdr.hwndFrom = inspector->list;
        activation.hdr.idFrom = Monstey_Fields;
        activation.hdr.code = NM_DBLCLK;
        activation.iItem = selected;
        SendMessageW(inspector->window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&activation));
        return;
    }
    if (smokeStage == 2 && editor && editor->window && editor->input)
    {
        smoke["typed_editor_visible"] = IsWindowVisible(editor->window) != FALSE;
        SetWindowTextW(editor->input, L"123.0");
        smokeStage = 3;
        SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
        return;
    }
    if (smokeStage == 4)
    {
        smokeStage = 5;
        Queue("database_undo", J::object(),
              [this](const J &undoReply)
              {
                  if (!undoReply.at("ok").as_bool() || undoReply.at("result").at("dirty").as_bool())
                  {
                      Error("Native GUI undo did not restore the source state");
                      return;
                  }
                  smoke["undo_verified"] = true;
                  Queue("database_redo", J::object(),
                        [this](const J &redoReply)
                        {
                            if (!redoReply.at("ok").as_bool() || !redoReply.at("result").at("dirty").as_bool())
                            {
                                Error("Native GUI redo did not restore the edit");
                                return;
                            }
                            smoke["redo_verified"] = true;
                            Queue("database_save_project",
                                  J::object({{"path", Monstey_JsonPath(smokeDirectory / L"project.json")}}),
                                  [this](const J &saveReply)
                                  {
                                      if (!saveReply.at("ok").as_bool())
                                      {
                                          Error(saveReply.at("error").at("message").as_string());
                                          return;
                                      }
                                      smoke["project_saved"] =
                                          std::filesystem::is_regular_file(smokeDirectory / L"project.json");
                                      Queue("database_build",
                                            J::object(
                                                {{"output", Monstey_JsonPath(smokeDirectory / L"candidate.gamedb")}}),
                                            [this](const J &buildReply)
                                            {
                                                if (!buildReply.at("ok").as_bool())
                                                {
                                                    Error(buildReply.at("error").at("message").as_string());
                                                    return;
                                                }
                                                if (smokeCloseDuringBuild)
                                                {
                                                    Error("Writer finished before the close test observed it");
                                                    return;
                                                }
                                                smoke["native_build"] = buildReply.at("result");
                                                smoke["build_verified"] =
                                                    buildReply.at("result").at("strict_named_order");
                                                smokeStage = 6;
                                                SmokeComplete();
                                            });
                                      if (smokeCloseDuringBuild)
                                      {
                                          smokeStage = 7;
                                          Queue("database_save_project",
                                                J::object({{"path", Monstey_JsonPath(smokeDirectory /
                                                                                     L"queued-must-not-run.json")}}));
                                      }
                                  });
                        });
              });
    }
}

void Monstey_App::SmokeComplete()
{
    if (smokeReport.empty() || closing)
        return;
    KillTimer(window, 1);
    smoke["status"] = smokeFailed ? "FAIL" : "PASS";
    smoke["native_gui"] = true;
    smoke["implementation"] = "C++20/Win32";
    smoke["python_runtime"] = false;
    smoke["record_double_click_events"] = recordDoubleClicks;
    smoke["field_double_click_events"] = fieldDoubleClicks;
    smoke["heartbeat_ticks"] = heartbeat;
    auto verifiedSource = initialDatabase;
    if (verifiedSource.empty() && smokeArguments.contains("source"))
        verifiedSource = std::filesystem::path(Monstey_Wide(smokeArguments.at("source").as_string()));
    if (!verifiedSource.empty())
    {
        smoke["source"] = Monstey_JsonPath(verifiedSource);
        if (smoke.contains("source_sha256"))
            smoke["source_unchanged"] = Monstey_HashSha256(verifiedSource) == smoke.at("source_sha256").as_string();
    }
    try
    {
        Monstey_WriteNewText(smokeReport, smoke.dump(2) + "\n");
    }
    catch (...)
    {
        smokeFailed = true;
    }
    Close();
}

} // namespace Monstey::Monstey_AppInternal
