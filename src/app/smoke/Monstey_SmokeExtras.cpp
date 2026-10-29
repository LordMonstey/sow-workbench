#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeExtrasStep()
{
    auto ready = [&] { return editor && editor->window && !editor->pending; };
    auto loaded = [&]
    {
        return !active.load() && !ready() && socketView.contains("total") &&
               !socketView.value("ui_loading", true).as_bool();
    };
    auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    if (smokeStage == 1 && loaded())
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("name") == J("Alpha"))
            {
                if (rowData[i].at("reference").as_string() != socketModel)
                {
                    Monstey_ListSelect(records, static_cast<int>(i));
                    ReadRecord(static_cast<int>(i));
                    return;
                }
                Monstey_Require(fieldData.empty(), "Animated extra UI fixture Alpha must initially be empty");
                smokeStage = 2;
                SocketCommand(Monstey_SocketCreate);
                return;
            }
    }
    if (smokeStage == 2 && ready() && editor->formControls.size() == 6)
    {
        SetWindowTextW(editor->formControls[0], L"UI_Extra");
        SetWindowTextW(editor->formControls[1], L"Body");
        SetWindowTextW(editor->formControls[2], L"");
        SetWindowTextW(editor->formControls[3], L"Arm");
        SendMessageW(editor->formControls[4], BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(editor->formControls[5], BM_SETCHECK, BST_UNCHECKED, 0);
        smokeStage = 3;
        apply();
        return;
    }
    if (smokeStage == 3 && ready() && editor->formControls.size() == 2)
    {
        smokeStage = 4;
        apply();
        return;
    }
    if (smokeStage == 4 && ready() && editor->components.size() == 2)
    {
        SetWindowTextW(editor->components[0], L"0.25");
        SetWindowTextW(editor->components[1], L"0.75");
        smokeStage = 5;
        apply();
        return;
    }
    if (smokeStage == 5 && loaded() && fieldData.size() == 1)
    {
        const auto &row = fieldData[0];
        Monstey_Require(row.at("base_animation") == J("") && !row.at("start_attached").as_bool() &&
                            !row.at("start_visible").as_bool() &&
                            row.at("dissolve_param_range") == J::array({0.25, 0.75}),
                        "Animated extra human form forced animation/start flags or "
                        "changed dissolve values");
        Monstey_Require(!row.contains("incoming_references"), "Animated extra page eagerly computed usage");
        smoke["first_extra"] = row.at("reference");
        smoke["empty_animation_hidden_detached_and_typed_range"] = true;
        smoke["paged_usage_disabled"] = true;
        smokeStage = 6;
        SocketCommand(Monstey_SocketEdit);
        return;
    }
    if (smokeStage == 6 && ready() && editor->formControls.size() == 6)
    {
        SetWindowTextW(editor->formControls[2], L"Root");
        SendMessageW(editor->formControls[4], BM_SETCHECK, BST_CHECKED, 0);
        smokeStage = 7;
        apply();
        return;
    }
    if (smokeStage == 7 && ready() && editor->formControls.size() == 2)
    {
        smokeStage = 8;
        apply();
        return;
    }
    if (smokeStage == 8 && ready() && editor->components.size() == 2)
    {
        smokeStage = 9;
        apply();
        return;
    }
    if (smokeStage == 9 && loaded() && fieldData.size() == 1 && fieldData[0].at("owner_attach_node") == J("Root"))
    {
        Monstey_Require(fieldData[0].at("reference") != smoke.at("first_extra") &&
                            fieldData[0].at("start_visible").as_bool(),
                        "Independent extra edit reused source or lost start state");
        smoke["independent_extra"] = fieldData[0].at("reference");
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
                Monstey_Require(fieldData.empty(), "Extra owner switch kept stale actionable slots");
                smoke["owner_switch_clears_slots"] = true;
                return;
            }
        throw Monstey_Error("smoke", "Animated extra fixture has no Beta");
    }
    if (smokeStage == 12 && loaded() && fieldData.size() == 1)
    {
        Monstey_Require(fieldData[0].at("reference") == smoke.at("independent_extra"),
                        "Shared extra copy changed native reference");
        smoke["shared_copy"] = true;
        smokeStage = 13;
        ExtraCommand(Monstey_ExtraUsage);
        return;
    }
    if (smokeStage == 13 && loaded() && socketView.contains("selected_usage"))
    {
        const auto &rows = socketView.at("selected_usage").at("rows");
        Monstey_Require(rows.size() == 1 && rows[0].at("incoming_references").at("total").as_uint() == 2,
                        "Selected-only extra usage missed shared owner");
        smoke["selected_usage_two_owners"] = true;
        smokeStage = 14;
        SocketCommand(Monstey_SocketRemove);
        return;
    }
    if (smokeStage == 14 && ready() && editor->formControls.size() == 1)
    {
        smokeStage = 15;
        apply();
        return;
    }
    if (smokeStage == 15 && loaded() && fieldData.empty())
    {
        smoke["unlink"] = true;
        smokeStage = 16;
        SocketCommand(Monstey_Undo);
        return;
    }
    if (smokeStage == 16 && loaded() && fieldData.size() == 1)
    {
        smoke["shared_history_undo"] = true;
        smokeStage = 17;
        SocketCommand(Monstey_Redo);
        return;
    }
    if (smokeStage == 17 && loaded() && fieldData.empty())
    {
        smoke["shared_history_redo"] = true;
        smokeStage = 18;
        Queue("database_save_project", J::object({{"path", Monstey_JsonPath(smokeDirectory / L"extras.project.json")}}),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  Queue("database_build", J::object({{"output", Monstey_JsonPath(smokeDirectory / L"extras.gamedb")}}),
                        [this](const J &built)
                        {
                            if (!built.at("ok").as_bool())
                            {
                                Error(built.at("error").at("message").as_string());
                                return;
                            }
                            smoke["native_build"] = built.at("result");
                            smoke["animation_merge_verified"] = false;
                            SmokeComplete();
                        });
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
