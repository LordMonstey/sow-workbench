#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeSocketsStep()
{
    const auto ready = [&] { return editor && editor->window && !editor->pending; };
    const auto loaded = [&]
    {
        return !active.load() && !ready() && socketView.contains("total") &&
               !socketView.value("ui_loading", true).as_bool();
    };
    const auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    if (smokeStage == 1 && loaded())
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("name").as_string() == "Alpha")
            {
                if (rowData[i].at("reference").as_string() != socketModel)
                {
                    Monstey_ListSelect(records, static_cast<int>(i));
                    ReadRecord(static_cast<int>(i));
                    return;
                }
                Monstey_Require(socketView.at("total").as_uint() == 0,
                                "Socket UI fixture Alpha must initially be empty");
                smoke["alpha_model"] = rowData[i].at("reference");
                smokeStage = 2;
                SocketCommand(Monstey_SocketCreate);
                return;
            }
    }
    if (smokeStage == 2 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"Ui_Sheath");
        SetWindowTextW(editor->formControls[1], L"Arm");
        smokeStage = 3;
        apply();
        return;
    }
    if (smokeStage == 3 && ready() && editor->components.size() == 3)
    {
        SetWindowTextW(editor->components[0], L"1.25");
        SetWindowTextW(editor->components[1], L"-2.5");
        SetWindowTextW(editor->components[2], L"3.75");
        smokeStage = 4;
        apply();
        return;
    }
    if (smokeStage == 4 && ready() && editor->components.size() == 4)
    {
        SetWindowTextW(editor->components[3], L"-1");
        smokeStage = 5;
        apply();
        return;
    }
    if (smokeStage == 5 && loaded() && fieldData.size() == 1)
    {
        Monstey_Require(fieldData[0].at("position_cm") == J::array({1.25, -2.5, 3.75}) &&
                            fieldData[0].at("rotation_xyzw") == J::array({0.0, 0.0, 0.0, -1.0}),
                        "Socket native UI changed units or quaternion sign");
        smoke["first_override"] = fieldData[0].at("reference");
        smoke["typed_creation"] = true;
        smokeStage = 6;
        SocketCommand(Monstey_SocketEdit);
        return;
    }
    if (smokeStage == 6 && ready() && editor->formControls.size() == 3)
    {
        SetWindowTextW(editor->formControls[0], L"Ui_Independent");
        smokeStage = 7;
        apply();
        return;
    }
    if (smokeStage == 7 && ready() && editor->components.size() == 3)
    {
        SetWindowTextW(editor->components[0], L"2.5");
        smokeStage = 8;
        apply();
        return;
    }
    if (smokeStage == 8 && ready() && editor->components.size() == 4)
    {
        smokeStage = 9;
        apply();
        return;
    }
    if (smokeStage == 9 && loaded() && fieldData.size() == 1 && fieldData[0].at("socket_name") == J("Ui_Independent"))
    {
        Monstey_Require(fieldData[0].at("reference") != smoke.at("first_override"),
                        "Independent socket edit reused the original physical row");
        smoke["independent_override"] = fieldData[0].at("reference");
        smoke["independent_edit"] = true;
        smokeStage = 10;
        SocketCommand(Monstey_SocketCopy);
        return;
    }
    if (smokeStage == 10 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"Beta");
        SendMessageW(editor->formControls[1], CB_SETCURSEL, 1, 0);
        smokeStage = 11;
        apply();
        return;
    }
    if (smokeStage == 11 && loaded())
    {
        smokeStage = 12;
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("name") == J("Beta"))
            {
                Monstey_ListSelect(records, static_cast<int>(i));
                ReadRecord(static_cast<int>(i));
                Monstey_Require(fieldData.empty(), "Model change retained another model's actionable socket slots");
                smoke["selection_clears_stale_slots"] = true;
                return;
            }
        throw Monstey_Error("smoke", "Socket UI fixture has no Beta model");
    }
    if (smokeStage == 12 && loaded() && fieldData.size() == 1)
    {
        Monstey_Require(fieldData[0].at("reference") == smoke.at("independent_override"),
                        "Shared socket copy changed the physical reference");
        smoke["shared_copy"] = true;
        smokeStage = 13;
        SocketCommand(Monstey_SocketRemove);
        return;
    }
    if (smokeStage == 13 && ready() && editor->formControls.size() == 1)
    {
        smokeStage = 14;
        apply();
        return;
    }
    if (smokeStage == 14 && loaded() && fieldData.empty())
    {
        smoke["unlink_selected_slot"] = true;
        smokeStage = 15;
        SocketCommand(Monstey_Undo);
        return;
    }
    if (smokeStage == 15 && loaded() && fieldData.size() == 1)
    {
        smoke["shared_history_undo"] = true;
        smokeStage = 16;
        SocketCommand(Monstey_Redo);
        return;
    }
    if (smokeStage == 16 && loaded() && fieldData.empty())
    {
        smoke["shared_history_redo"] = true;
        smokeStage = 17;
        SetWindowTextW(query, L"NoSuchModel_UI");
        LoadSocketModels();
        return;
    }
    if (smokeStage == 17 && !active.load() && rowData.empty() && socketModel.empty())
    {
        smoke["empty_search_clears_selection"] = true;
        smokeStage = 18;
        Queue("database_save_project",
              J::object({{"path", Monstey_JsonPath(smokeDirectory / L"sockets.project.json")}}),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  smoke["shared_project_saved"] = true;
                  Queue("database_build", J::object({{"output", Monstey_JsonPath(smokeDirectory / L"sockets.gamedb")}}),
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
