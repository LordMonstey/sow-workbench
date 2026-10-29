#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeNestedStep()
{
    auto ready = [&] { return editor && editor->window && !editor->pending; };
    auto loaded = [&]
    {
        return !active.load() && !ready() && fxNestedMode && fxNestedView.contains("slots") &&
               fxNestedView.at("record") == metadata.value("reference", "");
    };
    auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    if (smokeStage == 50 && !active.load() && !ready() && !rowData.empty())
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].value("category", "") == J("ClientFX.Create"))
            {
                smokeStage = 100;
                Monstey_ListSelect(records, static_cast<int>(i));
                ReadRecord(static_cast<int>(i));
                return;
            }
        throw Monstey_Error("smoke", "Supplied native FX fixture contains no "
                                     "ClientFX.Create record on the first page");
    }
    if (smokeStage == 100 && !active.load() && !ready() && metadata.value("category", "") == J("ClientFX.Create"))
    {
        smoke["create_record"] = metadata.at("reference");
        smokeStage = 101;
        FxNestedCommand(Monstey_FxNestedInspect);
        return;
    }
    if (smokeStage == 101 && loaded() && fieldData.size() == 16)
    {
        SetWindowPos(window, nullptr, 0, 0, 1000, 650, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        RECT client{}, button{};
        GetClientRect(window, &client);
        GetWindowRect(GetDlgItem(window, Monstey_FxNested), &button);
        POINT corner{button.right, button.bottom};
        ScreenToClient(window, &corner);
        Monstey_Require(corner.x <= client.right && corner.y < 200,
                        "Nested FX toolbar action escaped the minimum-width window");
        smoke["nested_action_visible_at_minimum_width"] = true;
        smoke["sixteen_native_slots"] = true;
        auto original = fxNestedView.at("nonempty_groups").as_uint();
        for (auto index : {0u, 15u})
            if (fieldData[index].at("selected_by_native_random_choice").as_bool())
                --original;
        smoke["other_groups"] = original;
        smokeStage = 102;
        Monstey_ListSelect(fields, 0);
        FxNestedCommand(Monstey_FxNestedSlot);
        return;
    }
    if (smokeStage == 102 && ready() && editor->lineList && editor->input)
    {
        SetWindowTextW(editor->input, L"resources/fx/synthetic/a.fxp\r\nresources/fx/synthetic/b.fxp");
        smokeStage = 103;
        apply();
        return;
    }
    if (smokeStage == 103 && loaded() && fieldData[0].value("effects", J::array()).size() == 2)
    {
        Monstey_Require(fieldData[0].at("raw") == J("resources/fx/synthetic/a.fxp;resources/fx/synthetic/b.fxp"),
                        "Nested group did not use native semicolon encoding");
        smoke["grouped_resources_native_encoding"] = true;
        smokeStage = 104;
        Monstey_ListSelect(fields, 15);
        FxNestedCommand(Monstey_FxNestedSlot);
        return;
    }
    if (smokeStage == 104 && ready() && editor->lineList && editor->input)
    {
        SetWindowTextW(editor->input, L"resources/fx/synthetic/c.fxp");
        smokeStage = 105;
        apply();
        return;
    }
    if (smokeStage == 105 && loaded() && fieldData[15].value("effects", J::array()).size() == 1)
    {
        smokeStage = 106;
        FxNestedCommand(Monstey_FxNestedOptions);
        return;
    }
    if (smokeStage == 106 && ready() && editor->formControls.size() == 2)
    {
        SetWindowTextW(editor->formControls[0], L"3");
        SendMessageW(editor->formControls[1], BM_SETCHECK, BST_CHECKED, 0);
        smokeStage = 107;
        apply();
        return;
    }
    if (smokeStage == 107 && loaded() && fxNestedView.at("native_num_to_create").as_uint() == 3 &&
        fxNestedView.at("loop").as_bool())
    {
        Monstey_Require(fxNestedView.at("nonempty_groups").as_uint() == smoke.at("other_groups").as_uint() + 2,
                        "Nested alternative count differs");
        smoke["typed_native_count_and_loop"] = true;
        smokeStage = 108;
        Monstey_ListSelect(fields, 0);
        FxNestedCommand(Monstey_FxNestedSlot);
        return;
    }
    if (smokeStage == 108 && ready() && editor->lineList && editor->input)
    {
        SetWindowTextW(editor->input, L"");
        smokeStage = 109;
        apply();
        return;
    }
    if (smokeStage == 109 && loaded() && fieldData[0].at("raw") == J("") &&
        fxNestedView.at("nonempty_groups").as_uint() == smoke.at("other_groups").as_uint() + 1)
    {
        smoke["empty_list_disables_group"] = true;
        smokeStage = 110;
        FxApply("fx_undo");
        return;
    }
    if (smokeStage == 110 && loaded() && fieldData[0].value("effects", J::array()).size() == 2)
    {
        smoke["nested_shared_undo"] = true;
        smokeStage = 111;
        FxApply("fx_redo");
        return;
    }
    if (smokeStage == 111 && loaded() && fieldData[0].at("raw") == J(""))
    {
        smoke["nested_shared_redo"] = true;
        smoke["native_nested_inspection"] = fxNestedView;
        smokeStage = 112;
        FxNestedCommand(Monstey_FxNestedBack);
        return;
    }
    if (smokeStage == 112 && !active.load() && !ready() && !fxNestedMode &&
        metadata.value("reference", "") == smoke.at("create_record") && !fieldData.empty())
    {
        Monstey_Require(Monstey_FxValue(metadata, "Effect0") == J("") &&
                            Monstey_FxValue(metadata, "NumToCreate") == J(3) &&
                            Monstey_FxValue(metadata, "Loop").as_bool(),
                        "Native property readback disagrees with nested controls");
        smoke["back_to_native_properties"] = true;
        smokeStage = 113;
        Queue("fx_save_project", J::object({{"path", Monstey_JsonPath(smokeDirectory / L"nested.project.json")}}),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  Queue("fx_build", J::object({{"output", Monstey_JsonPath(smokeDirectory / L"nested.fxp")}}),
                        [this](const J &built)
                        {
                            if (!built.at("ok").as_bool())
                            {
                                Error(built.at("error").at("message").as_string());
                                return;
                            }
                            smoke["native_build"] = built.at("result");
                            smoke["original_parent_attachment_verified"] = false;
                            smoke["in_game_verified"] = false;
                            SmokeComplete();
                        });
              });
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
