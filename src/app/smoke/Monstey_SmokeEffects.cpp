#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeEffectsStep()
{
    if (fxLoading)
        return;
    auto ready = [&] { return editor && editor->window && !editor->pending; };
    auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    const auto keys = [&]() -> std::size_t
    { return fxWorkflow.contains("active_keys") ? fxWorkflow.at("active_keys").size() : 0; };
    const auto tracks = [&]() -> std::size_t
    { return fxWorkflow.contains("layout") ? fxWorkflow.at("layout").at("tracks").size() : 0; };
    const auto chooseTrack = [&](const std::string &id)
    {
        for (std::size_t i = 0; i < fxTreeRows.size(); ++i)
            if (fxTreeRows[i].at("id").as_string() == id)
            {
                SelectFxView(static_cast<int>(i));
                return true;
            }
        return false;
    };
    const auto chooseKey = [&](const std::string &ref)
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].contains("reference") && rowData[i].at("reference").as_string() == ref)
            {
                Monstey_ListSelect(records, static_cast<int>(i));
                return true;
            }
        return false;
    };
    if (smokeStage == 1 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"resources/fx/synthetic/native_ui.fxp");
        SetWindowTextW(editor->formControls[1], L"2000");
        smokeStage = 2;
        apply();
        return;
    }
    if (smokeStage == 2 && fxWorkflow.value("effect_exists", false).as_bool() && !ready())
    {
        smoke["independent_effect_created"] = true;
        smokeStage = 3;
        FxCommand(Monstey_FxTrackCreate);
        return;
    }
    if (smokeStage == 3 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"Primary track");
        smokeStage = 4;
        apply();
        return;
    }
    if (smokeStage == 4 && tracks() == 1 && !ready())
    {
        smoke["primary_track"] = fxWorkflow.at("layout").at("tracks")[0].at("id");
        smoke["track_created"] = true;
        smokeStage = 5;
        FxCommand(Monstey_FxKeyCreate);
        return;
    }
    if (smokeStage == 5 && ready() && editor->formControls.size() == 5)
    {
        if (smokeNested)
        {
            const auto &choices = editor->formFields[0].at("choices");
            bool found = false;
            for (std::size_t i = 0; i < choices.size(); ++i)
                if (choices[i].at("value") == J("ClientFX.Create"))
                {
                    SendMessageW(editor->formControls[0], CB_SETCURSEL, i, 0);
                    found = true;
                }
            Monstey_Require(found, "FX schema has no qualified ClientFX.Create class");
        }
        SetWindowTextW(editor->formControls[1], L"Alpha");
        SetWindowTextW(editor->formControls[2], L"0.25");
        SetWindowTextW(editor->formControls[3], L"1.75");
        SendMessageW(editor->formControls[4], CB_SETCURSEL, 1, 0);
        smokeStage = 6;
        apply();
        return;
    }
    if (smokeStage == 6 && ready() && editor->formControls.empty())
    {
        smoke["required_value_dialogs"] = smoke.value("required_value_dialogs", 0).as_uint() + 1;
        apply();
        return;
    }
    if (smokeStage == 6 && keys() == 1 && !ready() && rowData.size() == 1)
    {
        Monstey_Require(smoke.value("required_value_dialogs", 0).as_uint() ==
                            smoke.value("required_native_dialogs_expected", 0).as_uint(),
                        "Native FX required-value dialogs differ from authoritative schema");
        smoke["alpha"] = fxWorkflow.at("active_keys")[0].at("reference");
        smoke["native_class_created"] = true;
        smokeStage = 7;
        FxCommand(Monstey_FxKeyRetime);
        return;
    }
    if (smokeStage == 7 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"0.5");
        SetWindowTextW(editor->formControls[1], L"1.5");
        smokeStage = 8;
        apply();
        return;
    }
    if (smokeStage == 8 && keys() == 1 && !ready() &&
        Monstey_FxValue(fxWorkflow.at("active_keys")[0], "StartTime") == J(0.5))
    {
        smoke["retime_named_controls"] = true;
        smokeStage = 9;
        FxCommand(Monstey_FxKeyDuplicate);
        return;
    }
    if (smokeStage == 9 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"Beta");
        smokeStage = 10;
        apply();
        return;
    }
    if (smokeStage == 10 && keys() == 2 && !ready() && rowData.size() == 2)
    {
        smoke["beta"] = fxWorkflow.at("active_keys")[1].at("reference");
        Monstey_Require(smoke.at("alpha") != smoke.at("beta"), "Duplicate UI reused a physical native record");
        chooseKey(smoke.at("beta").as_string());
        smokeStage = 11;
        FxCommand(Monstey_FxKeyUp);
        return;
    }
    if (smokeStage == 11 && keys() == 2 && fxWorkflow.at("active_keys")[0].at("reference") == smoke.at("beta") &&
        !ready())
    {
        smoke["physical_duplicate_and_key_reorder"] = true;
        chooseKey(smoke.at("beta").as_string());
        smokeStage = 12;
        FxCommand(Monstey_FxKeyUnlink);
        return;
    }
    if (smokeStage == 12 && keys() == 1 && fxWorkflow.at("unlinked_elements").size() == 1 && !ready())
    {
        smoke["unlink_retained_physical_element"] = true;
        smokeStage = 13;
        FxCommand(Monstey_FxKeyLink);
        return;
    }
    if (smokeStage == 13 && ready() && editor->formControls.size() == 2)
    {
        SendMessageW(editor->formControls[0], CB_SETCURSEL, 1, 0);
        SendMessageW(editor->formControls[1], CB_SETCURSEL, 1, 0);
        smokeStage = 14;
        apply();
        return;
    }
    if (smokeStage == 14 && keys() == 2 && fxWorkflow.at("unlinked_elements").empty() && !ready())
    {
        smoke["linked_existing_element"] = true;
        smokeStage = 15;
        FxCommand(Monstey_FxTrackCreate);
        return;
    }
    if (smokeStage == 15 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"Secondary track");
        smokeStage = 16;
        apply();
        return;
    }
    if (smokeStage == 16 && tracks() == 2 && !ready())
    {
        smoke["secondary_track"] = fxWorkflow.at("layout").at("tracks")[1].at("id");
        smokeStage = 17;
        FxCommand(Monstey_FxTrackRename);
        return;
    }
    if (smokeStage == 17 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"Auxiliary");
        smokeStage = 18;
        apply();
        return;
    }
    if (smokeStage == 18 && tracks() == 2 && fxWorkflow.at("layout").at("tracks")[1].at("name") == J("Auxiliary") &&
        !ready())
    {
        smokeStage = 19;
        FxCommand(Monstey_FxTrackUp);
        return;
    }
    if (smokeStage == 19 && tracks() == 2 && fxWorkflow.at("layout").at("tracks")[0].at("name") == J("Auxiliary") &&
        !ready())
    {
        smokeStage = 20;
        FxCommand(Monstey_FxTrackDelete);
        return;
    }
    if (smokeStage == 20 && tracks() == 1 && keys() == 2 && !ready())
    {
        smoke["track_rename_move_delete"] = true;
        chooseTrack(smoke.at("primary_track").as_string());
        if (!chooseKey(smoke.at("beta").as_string()))
            return;
        smokeStage = 21;
        FxCommand(Monstey_FxKeyDelete);
        return;
    }
    if (smokeStage == 21 && keys() == 1 && !ready())
    {
        smoke["physical_element_deleted"] = true;
        smokeStage = 22;
        Command(Monstey_Undo);
        return;
    }
    if (smokeStage == 22 && keys() == 2 && !ready())
    {
        smoke["effects_undo"] = true;
        smokeStage = 23;
        Command(Monstey_Redo);
        return;
    }
    if (smokeStage == 23 && keys() == 1 && !ready() && metadata.contains("fields"))
    {
        smoke["effects_redo"] = true;
        smokeStage = 24;
        StructureCommand(Monstey_AddField);
        return;
    }
    if (smokeStage == 24 && ready() && editor->formControls.size() == 5)
    {
        SetWindowTextW(editor->formControls[0], L"UiExtra");
        SendMessageW(editor->formControls[1], CB_SETCURSEL, 1, 0);
        smokeStage = 25;
        apply();
        return;
    }
    if (smokeStage == 25 && ready() && editor->input && editor->formControls.empty())
    {
        SetWindowTextW(editor->input, L"42.5");
        smokeStage = 26;
        apply();
        return;
    }
    const auto extraHash = Monstey_Hex(Monstey_NativeHash("UiExtra"));
    auto extra = std::find_if(fieldData.begin(), fieldData.end(),
                              [&](const J &field) { return field.at("hash").as_string() == extraHash; });
    if (smokeStage == 26 && extra != fieldData.end() && !ready())
    {
        Monstey_Require(extra->at("value") == J(42.5), "FX typed property form lost numeric value");
        Monstey_ListSelect(fields, static_cast<int>(extra - fieldData.begin()));
        smokeStage = 27;
        StructureCommand(Monstey_DeleteField);
        return;
    }
    if (smokeStage == 27 && extra == fieldData.end() && metadata.contains("fields") && !ready())
    {
        smoke["effects_property_add_delete"] = true;
        smokeStage = 28;
        FxCommand(Monstey_FxRename);
        return;
    }
    if (smokeStage == 28 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"resources/fx/synthetic/native_ui_renamed.fxp");
        smokeStage = 29;
        apply();
        return;
    }
    if (smokeStage == 29 && Monstey_FxValue(fxGlobal, "FxName") == J("resources/fx/synthetic/native_ui_renamed.fxp") &&
        !ready())
    {
        smoke["resource_renamed"] = true;
        smokeStage = 30;
        FxCommand(Monstey_FxDuration);
        return;
    }
    if (smokeStage == 30 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"2500");
        smokeStage = 31;
        apply();
        return;
    }
    if (smokeStage == 31 && Monstey_FxValue(fxGlobal, "ResourceLenMS") == J(2500) && !ready())
    {
        smoke["resource_duration_changed"] = true;
        smokeStage = 32;
        FxCommand(Monstey_FxDelete);
        return;
    }
    if (smokeStage == 32 && !fxWorkflow.value("effect_exists", true).as_bool() && !ready())
    {
        smoke["resource_deleted"] = true;
        smokeStage = 33;
        Command(Monstey_Undo);
        return;
    }
    if (smokeStage == 33 && fxWorkflow.value("effect_exists", false).as_bool() && keys() == 1 && !ready())
    {
        Monstey_Require(fxWorkflow.at("layout_synchronized").as_bool(),
                        "Effects UI lost native KeyList/track association");
        smoke["resource_delete_undo"] = true;
        smokeStage = 34;
        Queue("fx_save_project", J::object({{"path", Monstey_JsonPath(smokeDirectory / L"effects-project.json")}}),
              [this](const J &saved)
              {
                  if (!saved.at("ok").as_bool())
                  {
                      Error(saved.at("error").at("message").as_string());
                      return;
                  }
                  smoke["effects_project_saved"] = true;
                  Queue("fx_build", J::object({{"output", Monstey_JsonPath(smokeDirectory / L"native-ui.fxp")}}),
                        [this](const J &built)
                        {
                            if (!built.at("ok").as_bool())
                            {
                                Error(built.at("error").at("message").as_string());
                                return;
                            }
                            Monstey_Require(built.at("result").at("warnings").empty() &&
                                                built.at("result").at("strict_named_order").as_bool(),
                                            "Effects UI native build was not clean");
                            smoke["effects_native_build"] = built.at("result");
                            smokeStage = 35;
                            SmokeComplete();
                        });
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
