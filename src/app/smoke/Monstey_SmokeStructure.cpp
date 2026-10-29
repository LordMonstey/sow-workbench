#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeStructureStep()
{
    auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    auto ready = [&] { return editor && editor->window && !editor->pending; };
    if (smokeStage == 1 && !ready())
    {
        Monstey_Require(IsWindowEnabled(GetDlgItem(window, Monstey_Structure)) != FALSE,
                        "Untitled database structural controls are unavailable");
        smoke["structure_toolbar_available"] = true;
        smokeStage = 2;
        StructureCommand(Monstey_CreateCategory);
        return;
    }
    if (smokeStage == 2 && ready() && editor->formControls.size() == 3)
    {
        SetWindowTextW(editor->formControls[0], L"Monstey.StructureSmoke");
        SetWindowTextW(editor->formControls[1], L"Structure smoke");
        SetWindowTextW(editor->formControls[2], L"Tests");
        apply();
        return;
    }
    if (smokeStage == 3 && !categoryRows.empty() && category && !ready())
    {
        smokeStage = 4;
        StructureCommand(Monstey_CreateRecord);
        return;
    }
    if (smokeStage == 4 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"CreatedRecord");
        apply();
        return;
    }
    if (smokeStage == 5 && metadata.contains("fields") && !selector.empty() && !ready())
    {
        smokeStage = 6;
        StructureCommand(Monstey_AddField);
        return;
    }
    if ((smokeStage == 6 || smokeStage == 9) && ready() && editor->formControls.size() == 5)
    {
        auto second = smokeStage == 9;
        SetWindowTextW(editor->formControls[0], second ? L"SecondValue" : L"FirstValue");
        SendMessageW(editor->formControls[1], CB_SETCURSEL, second ? 0 : 1, 0);
        smokeStage = second ? 10 : 7;
        apply();
        return;
    }
    if (smokeStage == 7 && ready() && editor->input && editor->formControls.empty())
    {
        SetWindowTextW(editor->input, L"123.5");
        apply();
        return;
    }
    if (smokeStage == 8 && fieldData.size() == 1 && !ready())
    {
        smokeStage = 9;
        StructureCommand(Monstey_AddField);
        return;
    }
    if (smokeStage == 10 && ready() && editor->combo)
    {
        SendMessageW(editor->combo, CB_SETCURSEL, 1, 0);
        apply();
        return;
    }
    auto secondHash = Monstey_Hex(Monstey_NativeHash("SecondValue"));
    if (smokeStage == 11 && fieldData.size() == 2 && !ready())
    {
        Monstey_ListSelect(fields, 1);
        smokeStage = 20;
        StructureCommand(Monstey_MoveFieldUp);
        return;
    }
    if (smokeStage == 12 && fieldData.size() == 2 && fieldData[0].at("hash").as_string() == secondHash)
    {
        Monstey_ListSelect(fields, 1);
        smokeStage = 21;
        StructureCommand(Monstey_DeleteField);
        return;
    }
    if (smokeStage == 14 && fieldData.size() == 1)
    {
        smokeStage = 22;
        auto record = selector;
        Queue("database_undo", J::object(),
              [this, record](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  UpdateState(reply.at("result"));
                  smoke["structural_undo"] = true;
                  smokeStage = 15;
                  RefreshProperties(record);
              });
        return;
    }
    if (smokeStage == 15 && fieldData.size() == 2)
    {
        smokeStage = 23;
        auto record = selector;
        Queue("database_redo", J::object(),
              [this, record](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  UpdateState(reply.at("result"));
                  smoke["structural_redo"] = true;
                  smokeStage = 16;
                  RefreshProperties(record);
              });
        return;
    }
    if (smokeStage == 16 && fieldData.size() == 1 && !ready())
    {
        Monstey_Require(fieldData[0].at("hash").as_string() == secondHash && fieldData[0].at("value").as_bool(),
                        "Structural UI lost typed Boolean data");
        smoke["boolean_value_preserved"] = true;
        smokeStage = 30;
        StructureCommand(Monstey_AddField);
        return;
    }
    if (smokeStage == 30 && ready() && editor->formControls.size() == 5)
    {
        SetWindowTextW(editor->formControls[0], L"UnicodeArray");
        SendMessageW(editor->formControls[1], CB_SETCURSEL, 5, 0);
        SendMessageW(editor->formControls[2], CB_SETCURSEL, 1, 0);
        SetWindowTextW(editor->formControls[3], L"3");
        smokeStage = 31;
        apply();
        return;
    }
    if (smokeStage == 31 && ready() && editor->lineList && editor->input && editor->formControls.empty())
    {
        SetWindowTextW(editor->input, L"  spaced  \r\n\"\"\r\nSpectral \u03a9");
        apply();
        return;
    }
    if (smokeStage == 32 && fieldData.size() == 2 && !ready())
    {
        const auto hash = Monstey_Hex(Monstey_NativeHash("UnicodeArray"));
        auto field = std::find_if(fieldData.begin(), fieldData.end(),
                                  [&](const J &row) { return row.at("hash").as_string() == hash; });
        Monstey_Require(field != fieldData.end() && field->at("value") == J::array({"  spaced  ", "", "Spectral Ω"}),
                        "Native Unicode array form lost values, whitespace or empty strings");
        smoke["unicode_array_named_controls"] = true;
        smoke["unicode_array_value"] = field->at("value");
        smokeStage = 34;
        StructureCommand(Monstey_CreateRecord);
        return;
    }
    if (smokeStage == 34 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"");
        SendMessageW(editor->formControls[1], BM_SETCHECK, BST_CHECKED, 0);
        apply();
        return;
    }
    if (smokeStage == 35 && metadata.contains("fields") && !ready() &&
        selector == smoke.at("anonymous_selector").as_string() &&
        metadata.value("reference", "") == smoke.at("anonymous_selector"))
    {
        Monstey_Require(fieldData.empty() && rowData.size() == 1 && metadata.at("anonymous").as_bool(),
                        "New anonymous record was not selected by its native reference");
        smoke["anonymous_record_selected"] = true;
        smokeStage = 24;
        Queue("database_save_project",
              J::object({{"path", Monstey_JsonPath(smokeDirectory / L"structural-project.json")}}),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  smoke["structural_project_saved"] = true;
                  Queue("database_build",
                        J::object({{"output", Monstey_JsonPath(smokeDirectory / L"structural.gamedb")}}),
                        [this](const J &built)
                        {
                            if (!built.at("ok").as_bool())
                            {
                                Error(built.at("error").at("message").as_string());
                                return;
                            }
                            Monstey_Require(built.at("result").at("strict_named_order").as_bool() &&
                                                built.at("result").at("warnings").empty(),
                                            "Structural UI build was not clean");
                            smoke["structural_native_build"] = built.at("result");
                            smoke["structural_no_source_template"] = true;
                            smokeStage = 25;
                            SmokeComplete();
                        });
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
