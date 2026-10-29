#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokePrefabStep()
{
    const auto ready = [&] { return editor && editor->window && !editor->pending; };
    const auto idle = [&] { return !active.load() && !ready(); };
    const auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    const auto count = [&] { return fxLibrary.contains("prefabs") ? fxLibrary.at("prefabs").size() : 0; };
    const auto chooseView = [&](const std::string &id)
    {
        for (std::size_t i = 0; i < fxTreeRows.size(); ++i)
            if (fxTreeRows[i].at("id").as_string() == id)
            {
                SelectFxView(static_cast<int>(i));
                return true;
            }
        return false;
    };
    const auto hasLocal = [&]
    {
        if (!smoke.contains("child") || !smoke.contains("override_field_hash"))
            return false;
        const auto &values = fxLibrary.at("instances").at(smoke.at("child").as_string()).at("local_fields").as_array();
        return std::find(values.begin(), values.end(), smoke.at("override_field_hash")) != values.end();
    };
    if (smokeStage == 100 && idle() && metadata.contains("fields"))
    {
        smoke["native_source_key"] = selector;
        smokeStage = 101;
        FxLibraryCommand(Monstey_FxPrefabFromKey);
        return;
    }
    if (smokeStage == 101 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"NativeLightTemplate");
        smokeStage = 102;
        apply();
        return;
    }
    if (smokeStage == 102 && idle() && count() == 1 && rowData.size() == 1)
    {
        smoke["template"] = fxLibrary.at("prefabs").as_object().begin()->first;
        smoke["template_from_key"] = true;
        smokeStage = 103;
        FxLibraryCommand(Monstey_FxGroupCreate);
        return;
    }
    if (smokeStage == 103 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"Lighting library");
        smokeStage = 104;
        apply();
        return;
    }
    if (smokeStage == 104 && idle() && fxLibrary.at("groups").size() == 1)
    {
        smoke["library_group"] = fxLibrary.at("groups").as_object().begin()->first;
        smokeStage = 105;
        FxLibraryCommand(Monstey_FxPrefabMove);
        return;
    }
    if (smokeStage == 105 && ready() && editor->formControls.size() == 1)
    {
        SendMessageW(editor->formControls[0], CB_SETCURSEL, 1, 0);
        smokeStage = 106;
        apply();
        return;
    }
    if (smokeStage == 106 && idle() &&
        fxLibrary.at("prefabs").at(smoke.at("template").as_string()).at("group") == smoke.at("library_group"))
    {
        smoke["group_create_move"] = true;
        smokeStage = 107;
        FxLibraryCommand(Monstey_FxPrefabDuplicate);
        return;
    }
    if (smokeStage == 107 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"DerivedLightTemplate");
        smokeStage = 108;
        apply();
        return;
    }
    if (smokeStage == 108 && idle() && count() == 2 && rowData.size() == 2)
    {
        for (const auto &[id, prefab] : fxLibrary.at("prefabs").as_object())
            if (id != smoke.at("template").as_string())
                smoke["derived"] = id;
        Monstey_Require(fxLibrary.at("prefabs").at(smoke.at("template").as_string()).at("record") !=
                            fxLibrary.at("prefabs").at(smoke.at("derived").as_string()).at("record"),
                        "Prefab duplication reused a physical record");
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].value("fx_prefab", "") == smoke.at("derived"))
            {
                Monstey_ListSelect(records, static_cast<int>(i));
                ReadRecord(static_cast<int>(i));
            }
        smokeStage = 109;
        FxLibraryCommand(Monstey_FxPrefabParent);
        return;
    }
    if (smokeStage == 109 && ready() && editor->formControls.size() == 2)
    {
        const auto &choices = editor->formFields[0].at("choices");
        for (std::size_t i = 0; i < choices.size(); ++i)
            if (choices[i].at("value") == smoke.at("template"))
                SendMessageW(editor->formControls[0], CB_SETCURSEL, i, 0);
        smokeStage = 110;
        apply();
        return;
    }
    if (smokeStage == 110 && idle() &&
        fxLibrary.at("prefabs").at(smoke.at("derived").as_string()).at("parent") == smoke.at("template"))
    {
        smoke["independent_duplicate_and_inheritance"] = true;
        smokeStage = 111;
        FxLibraryCommand(Monstey_FxPrefabActive);
        return;
    }
    if (smokeStage == 111 && ready() && editor->formControls.size() == 1)
    {
        const auto &choices = editor->formFields[0].at("choices");
        for (std::size_t i = 0; i < choices.size(); ++i)
            if (choices[i].at("value") == smoke.at("derived"))
                SendMessageW(editor->formControls[0], CB_SETCURSEL, i, 0);
        smokeStage = 112;
        apply();
        return;
    }
    if (smokeStage == 112 && idle() && fxLibrary.value("active_prefab", "") == smoke.at("derived"))
    {
        smokeStage = 113;
        FxLibraryCommand(Monstey_FxPrefabChild);
        return;
    }
    if (smokeStage == 113 && ready() && editor->formControls.size() == 4)
    {
        SetWindowTextW(editor->formControls[1], L"DerivedNativeLight");
        SetWindowTextW(editor->formControls[2], L"1");
        smokeStage = 114;
        apply();
        return;
    }
    if (smokeStage == 114 && idle() && fxWorkflow.at("active_keys").size() == 2 &&
        fxLibrary.at("instances").size() == 1 && metadata.contains("fields"))
    {
        smoke["child"] = fxLibrary.at("instances").as_object().begin()->first;
        if (metadata.at("reference") != smoke.at("child"))
            return;
        smoke["independent_child_key"] = true;
        for (std::size_t i = 0; i < fieldData.size(); ++i)
        {
            const auto &field = fieldData[i];
            const auto name = field.at("name").as_string();
            if (field.at("editable").as_bool() && !field.at("array").as_bool() &&
                field.at("native_type").as_uint() == 1)
            {
                smoke["override_field"] = name;
                smoke["override_field_hash"] = field.at("hash");
                Monstey_ListSelect(fields, static_cast<int>(i));
                smokeStage = 115;
                FxLibraryCommand(Monstey_FxPrefabOverride);
                return;
            }
        }
        smokeStage = 118;
        FxLibraryCommand(Monstey_FxPrefabDetach);
        return;
    }
    if (smokeStage == 115 && ready() && editor->combo)
    {
        auto selected = SendMessageW(editor->combo, CB_GETCURSEL, 0, 0);
        SendMessageW(editor->combo, CB_SETCURSEL, selected ? 0 : 1, 0);
        smokeStage = 116;
        apply();
        return;
    }
    if (smokeStage == 116 && idle() && hasLocal() && metadata.contains("reference") &&
        metadata.at("reference") == smoke.at("child") && !fieldData.empty())
    {
        smoke["typed_local_override"] = true;
        for (std::size_t i = 0; i < fieldData.size(); ++i)
            if (fieldData[i].at("name") == smoke.at("override_field"))
                Monstey_ListSelect(fields, static_cast<int>(i));
        smokeStage = 117;
        FxLibraryCommand(Monstey_FxPrefabInherit);
        return;
    }
    if (smokeStage == 117 && idle() && !hasLocal() && metadata.contains("reference") &&
        metadata.at("reference") == smoke.at("child") && !fieldData.empty())
    {
        smoke["inherit_property"] = true;
        smokeStage = 118;
        FxLibraryCommand(Monstey_FxPrefabDetach);
        return;
    }
    if (smokeStage == 118 && idle() && fxLibrary.at("instances").empty())
    {
        smoke["detach_keeps_live_key"] = fxWorkflow.at("active_keys").size() == 2;
        if (!chooseView("prefabs") || rowData.size() != 2)
            return;
        smokeStage = 119;
        FxLibraryCommand(Monstey_FxPrefabDelete);
        return;
    }
    if (smokeStage == 119 && idle() && count() == 1)
    {
        smoke["delete_materializes_live_keys"] = fxWorkflow.at("active_keys").size() == 2;
        smokeStage = 120;
        FxApply("fx_undo");
        return;
    }
    if (smokeStage == 120 && idle() && count() == 2)
    {
        smoke["library_shared_undo"] = true;
        smokeStage = 121;
        FxApply("fx_redo");
        return;
    }
    if (smokeStage == 121 && idle() && count() == 1)
    {
        smoke["library_shared_redo"] = true;
        smokeStage = 122;
        Queue("fx_save_project", J::object({{"path", Monstey_JsonPath(smokeDirectory / L"prefabs.project.json")}}),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  Queue("fx_build", J::object({{"output", Monstey_JsonPath(smokeDirectory / L"prefabs.fxp")}}),
                        [this](const J &result)
                        {
                            if (!result.at("ok").as_bool())
                            {
                                Error(result.at("error").at("message").as_string());
                                return;
                            }
                            smoke["native_build"] = result.at("result");
                            smoke["runtime_verified"] = false;
                            SmokeComplete();
                        });
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
