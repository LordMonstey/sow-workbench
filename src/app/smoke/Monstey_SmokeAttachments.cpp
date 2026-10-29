#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeAttachmentStep()
{
    if (smokeStorageBusy)
        return;
    smokeStorageBusy = true;
    struct Monstey_AttachmentSmokeReset
    {
        bool &value;
        ~Monstey_AttachmentSmokeReset()
        {
            value = false;
        }
    } reset{smokeStorageBusy};
    {
        std::lock_guard lock(mutex);
        if (active.load() || !jobs.empty() || !results.empty())
            return;
    }
    const bool ready = editor && editor->window && !editor->pending;
    auto click = [&](int id) { SendMessageW(GetDlgItem(window, id), BM_CLICK, 0, 0); };
    auto apply = [&] { SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0); };
    auto values = [&](const char *mode, const char *name)
    {
        return metadata.contains("fields") && Monstey_FxValue(metadata, "UpdatePos") == J(mode) &&
               Monstey_FxValue(metadata, "AttachName") == J(name);
    };
    auto choose = [&](const char *mode, const wchar_t *name)
    {
        Monstey_Require(ready && editor->formControls.size() == 2 && editor->operation == "fx_attachment_set",
                        "Attachment form controls are unavailable");
        const auto &choices = editor->formFields[0].at("choices");
        bool found = false;
        for (std::size_t i = 0; i < choices.size(); ++i)
            if (choices[i].at("value") == J(mode))
            {
                SendMessageW(editor->formControls[0], CB_SETCURSEL, i, 0);
                found = true;
            }
        Monstey_Require(found, "Native owner mode is absent from combo box");
        SetWindowTextW(editor->formControls[1], name);
    };
    if ((smokeStage == 1 || smokeStage == 3 || smokeStage == 16) && !ready &&
        !metadata.value("category", "").as_string().starts_with("ClientFX."))
    {
        Monstey_Require(!IsWindowEnabled(GetDlgItem(window, Monstey_FxAttachment)),
                        "Attachment must be disabled for global or loading rows");
        smoke["global_or_loading_attachment_disabled"] = true;
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].value("category", "").as_string().starts_with("ClientFX."))
            {
                Monstey_ListSelect(records, static_cast<int>(i));
                ReadRecord(static_cast<int>(i));
                return;
            }
        throw Monstey_Error("smoke", "Owner attachment fixture has no component on its first page");
    }
    if (smokeStage == 1 && !ready && !rowData.empty() && metadata.contains("reference"))
    {
        Monstey_Require(IsWindowEnabled(GetDlgItem(window, Monstey_FxAttachment)),
                        "Attachment must be enabled for a loaded component");
        smoke["loaded_component_attachment_enabled"] = true;
        smoke["attachment_record"] = metadata.at("reference");
        smoke["native_before"] = metadata;
        smokeStage = 2;
        click(Monstey_FxAttachment);
        TabCtrl_SetCurSel(tabs, 0);
        TabChanged();
        return;
    }
    if (smokeStage == 2 && tab == 0 && !ready)
    {
        smoke["stale_inspection_tab_switch_discarded"] = true;
        smokeStage = 3;
        TabCtrl_SetCurSel(tabs, 3);
        TabChanged();
        return;
    }
    if (smokeStage == 3 && tab == 3 && !ready && metadata.contains("reference"))
    {
        smokeStage = 4;
        click(Monstey_FxAttachment);
        return;
    }
    if (smokeStage == 4 && ready)
    {
        Monstey_Require(editor->formFields[0].at("choices").size() == 8,
                        "Owner mode combo does not expose all eight native choices");
        SetWindowPos(window, nullptr, 0, 0, 1000, 650, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        RECT client{}, button{};
        GetClientRect(window, &client);
        GetWindowRect(GetDlgItem(window, Monstey_FxAttachment), &button);
        POINT edge{button.right, button.bottom};
        ScreenToClient(window, &edge);
        Monstey_Require(edge.x <= client.right && edge.y < 200,
                        "Attachment toolbar action escaped the minimum-width window");
        smoke["attachment_action_visible_at_minimum_width"] = true;
        smoke["all_eight_native_modes_in_combo"] = true;
        choose("NodeAttach", L"Head");
        smokeStage = 5;
        apply();
        return;
    }
    if (smokeStage == 5 && !ready && values("NodeAttach", "Head"))
    {
        Monstey_Require(fxAttachmentView.at("native_follow_flags").as_uint() == 0x28 &&
                            !fxAttachmentView.at("external_owner_resolved").as_bool(),
                        "Named attachment result disagrees with native contract");
        smoke["named_mode_name_and_native_bits"] = fxAttachmentView;
        smokeStage = 6;
        click(Monstey_FxAttachment);
        return;
    }
    if (smokeStage == 6 && ready)
    {
        Monstey_Require(Monstey_WindowText(editor->formControls[1]) == L"Head",
                        "Attachment name did not round trip through form");
        choose("Follow", L"Head");
        smokeStage = 7;
        apply();
        return;
    }
    if (smokeStage == 7 && !ready && values("Follow", "Head"))
    {
        Monstey_Require(fxAttachmentView.at("name_is_dormant").as_bool(), "Unnamed mode did not preserve dormant name");
        smoke["dormant_name_preserved"] = true;
        smokeStage = 8;
        click(Monstey_FxAttachment);
        return;
    }
    if (smokeStage == 8 && ready)
    {
        choose("SocketAttach", L"");
        smokeStage = 9;
        apply();
        return;
    }
    if (smokeStage == 9 && !ready && values("SocketAttach", ""))
    {
        Monstey_Require(fxAttachmentView.at("empty_or_zero_name_uses_context_frame").as_bool(),
                        "Empty attachment name did not retain native fallback");
        smoke["empty_name_native_fallback"] = true;
        smokeStage = 10;
        click(Monstey_Undo);
        return;
    }
    if (smokeStage == 10 && !ready && values("Follow", "Head"))
    {
        smoke["two_fields_single_undo_step"] = true;
        smokeStage = 11;
        click(Monstey_Redo);
        return;
    }
    if (smokeStage == 11 && !ready && values("SocketAttach", ""))
    {
        smoke["shared_redo"] = true;
        smokeStage = 12;
        click(Monstey_FxAttachment);
        return;
    }
    if (smokeStage == 12 && ready)
    {
        choose("Fixed", L"Cancelled");
        smokeStage = 13;
        SendMessageW(editor->window, WM_COMMAND, Monstey_EditorCancel, 0);
        return;
    }
    if (smokeStage == 13 && !ready && values("SocketAttach", ""))
    {
        smoke["cancel_preserved_native_values"] = true;
        smokeStage = 14;
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"attachment.project.json");
        click(Monstey_Save);
        return;
    }
    if (smokeStage == 14 && !ready && std::filesystem::exists(smokeDirectory / L"attachment.project.json"))
    {
        smoke["native_project_saved_with_dialog"] = true;
        smokeStage = 15;
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"attachment.fxp");
        click(Monstey_Build);
        return;
    }
    if (smokeStage == 15 && !ready && std::filesystem::exists(smokeDirectory / L"attachment.fxp"))
    {
        smoke["native_output_built_with_dialog"] = true;
        smoke["native_after"] = metadata;
        smokeStage = 16;
        OpenSource(smokeDirectory / L"attachment.fxp");
        return;
    }
    if (smokeStage == 16 && !ready && values("SocketAttach", ""))
    {
        smoke["native_build_reopened_in_ui"] = true;
        smoke["external_owner_resolved"] = false;
        smoke["in_game_verified"] = false;
        SmokeComplete();
    }
}

} // namespace Monstey::Monstey_AppInternal
