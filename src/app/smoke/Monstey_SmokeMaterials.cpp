#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeMaterialStep()
{
    // Native file dialogs dispatch nested WM_TIMER messages on this UI thread.
    if (smokeStorageBusy)
        return;
    smokeStorageBusy = true;
    struct Monstey_MaterialSmokeReset
    {
        bool &value;
        ~Monstey_MaterialSmokeReset()
        {
            value = false;
        }
    } reset{smokeStorageBusy};
    if (materialOpening || active.load())
        return;
    const auto ready = [this]() { return editor && editor->window && !editor->pending; };
    const auto apply = [this]()
    { SendMessageW(editor->window, WM_COMMAND, MAKEWPARAM(Monstey_EditorApply, BN_CLICKED), 0); };
    const auto scalar = [this]()
    {
        for (const auto &row : materialView.at("material").at("entries").as_array())
            if (row.at("table") == smoke.at("scalar_table") && row.at("ordinal") == smoke.at("scalar_ordinal"))
                return row;
        throw Monstey_Error("smoke", "Scalar property disappeared");
    };
    const auto find = [this](const char *name)
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("name") == J(name))
                return static_cast<int>(i);
        return -1;
    };
    if (smokeStage == 1 && !materialView.empty())
    {
        smoke["initial_material"] = materialView.at("material");
        smoke["initial_property_count"] = materialView.at("material").at("parameter_count");
        for (std::size_t i = 0; i < rowData.size(); ++i)
        {
            const auto &row = rowData[i];
            if (row.at("native_type_u8") == J(5) && row.at("value_f32")[0].is_number())
            {
                smoke["scalar_table"] = row.at("table");
                smoke["scalar_ordinal"] = row.at("ordinal");
                Monstey_ListSelect(records, static_cast<int>(i));
                NMITEMACTIVATE event{};
                event.hdr.hwndFrom = records;
                event.hdr.idFrom = Monstey_Records;
                event.hdr.code = NM_DBLCLK;
                event.iItem = static_cast<int>(i);
                smokeStage = 2;
                SendMessageW(window, WM_NOTIFY, Monstey_Records, reinterpret_cast<LPARAM>(&event));
                return;
            }
        }
        throw Monstey_Error("smoke", "Fixture has no finite scalar");
    }
    if (smokeStage == 2 && ready() && editor->input)
    {
        SetWindowTextW(editor->input, L"1.25");
        smokeStage = 3;
        apply();
        return;
    }
    if (smokeStage == 3 && scalar().at("value_f32")[0] == J(1.25))
    {
        Monstey_Require(materialView.at("dirty").as_bool(), "Typed edit did not dirty the session");
        smoke["record_double_click_typed_edit"] = true;
        smokeStage = 4;
        Command(Monstey_Undo);
        return;
    }
    if (smokeStage == 4 && !materialView.at("dirty").as_bool())
    {
        smoke["undo_to_exact_source"] = true;
        smokeStage = 5;
        Command(Monstey_Redo);
        return;
    }
    if (smokeStage == 5 && scalar().at("value_f32")[0] == J(1.25))
    {
        smoke["redo"] = true;
        smokeStage = 6;
        Command(Monstey_MaterialRaw);
        return;
    }
    if (smokeStage == 6 && ready() && editor->input)
    {
        const auto before = revision;
        smoke["expect_invalid_raw_hex"] = true;
        SetWindowTextW(editor->input, L"not hex");
        apply();
        smoke["expect_invalid_raw_hex"] = false;
        Monstey_Require(smoke.contains("observed_invalid_raw_hex_error"), "Malformed hex was not rejected");
        Monstey_Require(editor->window && !editor->pending && revision == before,
                        "Invalid raw bytes closed the form or changed session");
        smoke["invalid_raw_hex_retains_form_and_revision"] = true;
        smoke["width_reject_revision"] = revision;
        smoke["expect_backend_width"] = true;
        SetWindowTextW(editor->input, L"00");
        smokeStage = 601;
        apply();
        return;
    }
    if (smokeStage == 601 && ready() && smoke.contains("observed_backend_width_error"))
    {
        Monstey_Require(Monstey_WindowText(editor->input) == L"00" &&
                            revision == smoke.at("width_reject_revision").as_uint(),
                        "Backend validation lost the draft or changed the session");
        smoke["backend_error_retains_editor_draft_and_revision"] = true;
        smoke["expect_backend_width"] = false;
        SetWindowTextW(editor->input, L"0100c07f");
        smokeStage = 7;
        apply();
        return;
    }
    if (smokeStage == 7 && scalar().at("value_bytes_hex") == J("0100c07f"))
    {
        smoke["raw_nan_exact_bytes"] = true;
        smokeStage = 8;
        Command(Monstey_Add);
        return;
    }
    if (smokeStage == 8 && ready() && editor->formControls.size() == 5)
    {
        SetWindowTextW(editor->formControls[0], L"UI_NewScalar");
        smokeStage = 9;
        apply();
        return;
    }
    if (smokeStage == 9 && ready() && editor->input && editor->formControls.empty())
    {
        SetWindowTextW(editor->input, L"2.5");
        smokeStage = 10;
        apply();
        return;
    }
    if (smokeStage == 10 && find("UI_NewScalar") >= 0)
    {
        const auto index = find("UI_NewScalar");
        Monstey_Require(rowData[static_cast<std::size_t>(index)].at("value_f32")[0] == J(2.5),
                        "Added scalar has wrong value");
        smoke["add_via_native_table_type_and_flags_form"] = true;
        SetWindowTextW(query, L"ui_newscalar");
        Command(Monstey_Search);
        Monstey_Require(rowData.size() == 1 && rowData[0].at("name") == J("UI_NewScalar"),
                        "Case-insensitive property filter failed");
        smoke["filter_and_selection"] = true;
        smokeStage = 11;
        Command(Monstey_Rename);
        return;
    }
    if (smokeStage == 11 && ready() && editor->formControls.size() == 1)
    {
        SetWindowTextW(editor->formControls[0], L"UI_Renamed");
        SetWindowTextW(query, L"");
        smokeStage = 12;
        apply();
        return;
    }
    if (smokeStage == 12 && find("UI_Renamed") >= 0)
    {
        const auto index = find("UI_Renamed");
        Monstey_ListSelect(records, index);
        MaterialReadRow(index);
        smoke["rename"] = true;
        smokeStage = 13;
        Command(Monstey_Delete);
        return;
    }
    if (smokeStage == 13 && find("UI_Renamed") < 0 &&
        materialView.at("material").at("parameter_count") == smoke.at("initial_property_count"))
    {
        smoke["remove"] = true;
        smokeStage = 14;
        Command(Monstey_Undo);
        return;
    }
    if (smokeStage == 14 && find("UI_Renamed") >= 0)
    {
        Monstey_Require(scalar().at("value_bytes_hex") == J("0100c07f"), "Unrelated CRUD changed raw NaN value");
        smoke["undo_remove_retains_raw_nan"] = true;
        smokeStage = 15;
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"material.project.json");
        Command(Monstey_Save);
        return;
    }
    if (smokeStage == 15 && std::filesystem::is_regular_file(smokeDirectory / L"material.project.json"))
    {
        smoke["saved_project_sha256"] = Monstey_HashSha256(smokeDirectory / L"material.project.json");
        smokeStage = 16;
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"material.mat");
        Command(Monstey_Build);
        return;
    }
    if (smokeStage == 16 && std::filesystem::is_regular_file(smokeDirectory / L"material.mat"))
    {
        smoke["built_material_sha256"] = Monstey_HashSha256(smokeDirectory / L"material.mat");
        smoke["pre_load_generation"] = generation;
        smokeStage = 17;
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"material.project.json");
        Command(Monstey_Load);
        return;
    }
    if (smokeStage == 17 && find("UI_Renamed") >= 0)
    {
        Monstey_Require(generation == smoke.at("pre_load_generation").as_uint() + 1 &&
                            materialView.at("ui_generation") == J(generation),
                        "Material project acceptance preceded its completed source replacement");
        smoke["project_load_generation_advanced"] = true;
        Monstey_Require(scalar().at("value_bytes_hex") == J("0100c07f") && materialView.at("dirty").as_bool(),
                        "Loaded project did not replay exact edits");
        smoke["project_replay"] = true;
        smoke["final_material"] = materialView;
        smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
        smoke["source_hash_after"] = Monstey_HashSha256(initialDatabase);
        smokeStage = 18;
        Command(Monstey_Revert);
        return;
    }
    if (smokeStage == 18 && !materialView.at("dirty").as_bool())
    {
        Monstey_Require(materialView.at("material").at("rebuilt_sha256") == smoke.at("source_sha256"),
                        "Revert did not restore exact native source");
        smoke["revert_exact_native_bytes"] = true;
        const auto old = epoch;
        TabCtrl_SetCurSel(tabs, 1);
        TabChanged();
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        Monstey_Require(epoch > old && materialMode, "Material mode did not survive tab changes");
        smoke["tab_epoch_and_mode_restore"] = true;
        smokeStage = 19;
        return;
    }
    if (smokeStage == 19 && !active.load() && !materialView.at("dirty").as_bool())
        SmokeComplete();
}

} // namespace Monstey::Monstey_AppInternal
