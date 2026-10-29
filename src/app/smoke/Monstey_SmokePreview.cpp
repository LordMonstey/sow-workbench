#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeAnimatedPreviewStep()
{
    if (smokeStage == 0)
        return;
    Monstey_Require(modelPreview != nullptr, "Animated preview was not created");
    auto view = modelPreview->Inspect();
    const auto animation = std::filesystem::path(Monstey_Wide(smokeArguments.at("animation").as_string()));
    const auto clip = smokeArguments.at("clip").as_string();
    const auto send = [&](int id) { SendMessageW(modelPreview->Handle(), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0); };
    const auto select = [&](std::string_view name)
    {
        const auto label = Monstey_Wide(name);
        const auto index = SendDlgItemMessageW(modelPreview->Handle(), 111, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
                                               reinterpret_cast<LPARAM>(label.c_str()));
        Monstey_Require(index != CB_ERR, "Expected clip missing from native combo");
        SendDlgItemMessageW(modelPreview->Handle(), 111, CB_SETCURSEL, index, 0);
        SendMessageW(modelPreview->Handle(), WM_COMMAND, MAKEWPARAM(111, CBN_SELCHANGE), 0);
    };
    if (smokeStage == 1 && view["ready"].as_bool())
    {
        smoke["animation_source_sha256"] = Monstey_HashSha256(animation);
        smokeStage = 1001; // Native file picker runs a nested message loop.
        Monstey_SmokeDialogPaths.push_back(animation);
        send(110);
        smokeStage = 2;
        return;
    }
    if (smokePreviewClose)
    {
        bool held = false;
        {
            std::lock_guard lock(mutex);
            held = active.load() && previewCloseTestReady;
        }
        if (smokeStage == 2 && held)
        {
            smokeStage = 3;
            modelPreview->RequestAnimation(animation, clip);
            smoke["closed_while_animation_load_active"] = true;
            Close();
        }
        return;
    }
    if (view["animation_loading"].as_bool())
        return;
    if (smokeStage >= 2 && smokeStage != 8 && !view["animation_error"].as_string().empty())
        throw Monstey_Error("ui_smoke", view["animation_error"].as_string());
    if (smokeStage == 2 && view["animation_ready"].as_bool())
    {
        smoke["native_animation_file_dialog_and_bind_loaded"] = true;
        smokeStage = 3;
        select(clip);
        return;
    }
    if (smokeStage == 3 && view["animation_ready"].as_bool() && view["animation"]["clip"].as_string() == clip)
    {
        Monstey_Require(modelPreview->SeekAnimation(0), "Initial animation seek failed");
        const auto first = modelPreview->Snapshot();
        Monstey_WriteNewFile(smokeDirectory / L"animation-0.bmp", first);
        SetWindowTextW(GetDlgItem(modelPreview->Handle(), 117), L"733");
        send(118);
        Monstey_Require(modelPreview->Inspect()["animation_frame"]["tick_ms"].as_number() == 733,
                        "Native Go did not seek");
        const auto changed = modelPreview->Snapshot();
        Monstey_Require(first != changed, "Animated frame did not change");
        Monstey_WriteNewFile(smokeDirectory / L"animation-733.bmp", changed);
        smoke["native_clip_combo_and_Go_seek"] = true;
        smoke["actual_geometry_changes_at_distinct_ticks"] = true;
        smoke["fixed_animation_framing"] = modelPreview->Inspect()["report"]["fixed_framing"];
        smoke["animation_play_start_revision"] = modelPreview->Inspect()["animation_frame"]["revision"];
        send(112);
        smokeStage = 4;
        return;
    }
    if (smokeStage == 4 &&
        view["animation_frame"]["revision"].as_uint() > smoke["animation_play_start_revision"].as_uint())
    {
        Monstey_Require(view["playing"].as_bool(), "Native timer did not leave playback active");
        smoke["real_native_timer_advances_pose"] = true;
        SetForegroundWindow(modelPreview->Handle());
        SetFocus(GetDlgItem(modelPreview->Handle(), 117));
        Monstey_Require(GetFocus() == GetDlgItem(modelPreview->Handle(), 117),
                        "Cannot focus native animation tick editor");
        SetWindowTextW(GetDlgItem(modelPreview->Handle(), 117), L"123");
        smoke["animation_edit_start_revision"] = view["animation_frame"]["revision"];
        smokeStage = 5;
        return;
    }
    if (smokeStage == 5 &&
        view["animation_frame"]["revision"].as_uint() > smoke["animation_edit_start_revision"].as_uint())
    {
        wchar_t text[64]{};
        GetDlgItemTextW(modelPreview->Handle(), 117, text, 64);
        Monstey_Require(std::wstring(text) == L"123", "Timer overwrote native tick text during edit");
        smoke["playback_preserves_active_tick_edit"] = true;
        smokeStage = 6;
        PostMessageW(GetDlgItem(modelPreview->Handle(), 117), WM_KEYDOWN, VK_RETURN, 0);
        return;
    }
    if (smokeStage == 6)
    {
        Monstey_Require(view["animation_frame"]["tick_ms"].as_number() == 123 && !view["playing"].as_bool(),
                        "Main loop did not route tick Enter or pause seek");
        smoke["native_tick_Enter_routes_before_main_shortcuts"] = true;
        smokeStage = 8;
        modelPreview->RequestAnimation(animation, "Monstey_Missing_Animation_Clip");
        return;
    }
    if (smokeStage == 8 && !view["animation_error"].as_string().empty())
    {
        Monstey_Require(view["ready"].as_bool() && view["animation_ready"].as_bool() && !view["failed"].as_bool() &&
                            view["animation_frame"]["tick_ms"].as_number() == 123,
                        "Failed clip destroyed valid preview");
        smoke["failed_clip_preserves_geometry_and_published_pose"] = true;
        smokeStage = 9;
        select("bind");
        return;
    }
    if (smokeStage == 9 && view["animation_ready"].as_bool() && view["animation"]["clip"].as_string() == "bind")
    {
        smokeStage = 10;
        select(clip);
        return;
    }
    if (smokeStage == 10 && view["animation_ready"].as_bool() && view["animation"]["clip"].as_string() == clip)
    {
        Monstey_Require(generation == smoke.at("preview_session_generation").as_uint() &&
                            revision == smoke.at("preview_session_revision").as_uint(),
                        "Animation result changed Runtime session");
        Monstey_Require(Monstey_HashSha256(animation) == smoke["animation_source_sha256"].as_string(),
                        "Animation source changed");
        auto skel = initialDatabase;
        skel.replace_extension(L".skel");
        Monstey_Require(Monstey_HashSha256(skel) == smoke["preview_skeleton_sha256"].as_string(),
                        "Animation preview changed skeleton");
        smoke["valid_native_clip_recovers_after_failure"] = true;
        smoke["animation_preserves_Runtime_generation_revision"] = true;
        smoke["animation_sources_unchanged"] = true;
        smoke["animated_final_view"] = view;
        smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
        smokeStage = 11;
        PostMessageW(modelPreview->Handle(), WM_CLOSE, 0, 0);
        return;
    }
    if (smokeStage == 11 && !modelPreview->Handle())
    {
        smoke["animated_close_preserves_main_window"] = true;
        SmokeComplete();
    }
}

void Monstey_App::SmokePreviewStep()
{
    if (smokePreviewAnimation)
    {
        SmokeAnimatedPreviewStep();
        return;
    }
    if (smokeStage == 0)
        return;
    Monstey_Require(modelPreview != nullptr, "Preview instance was not created");
    if (smokePreviewClose)
    {
        bool held = false;
        {
            std::lock_guard lock(mutex);
            held = active.load() && previewCloseTestReady;
        }
        if (smokeStage == 1 && held)
        {
            smokeStage = 2;
            TabCtrl_SetCurSel(tabs, 4);
            TabChanged();
            Monstey_SmokeDialogPaths.push_back(initialDatabase);
            Command(Monstey_ModelPreview);
            smoke["closed_while_preview_load_active"] = true;
            Close();
        }
        return;
    }
    auto view = modelPreview->Inspect();
    if (smokeStage == 1 && view["ready"].as_bool())
    {
        Monstey_Require(IsWindowEnabled(window), "Modeless preview disabled the Workbench");
        Monstey_Require(tab == 1 && view["report"]["source_sha256"] == smoke.at("source_sha256"),
                        "Tab switch discarded or replaced the preview scene");
        smoke["preview_survives_tab_epoch_change"] = true;
        smoke["preview_parent_remains_enabled"] = true;
        modelPreview->Close();
        Monstey_Require(IsWindow(window) && !closing, "Preview close shut down the Workbench");
        smoke["preview_close_preserves_parent"] = true;
        smokeStage = 2;
        animationMode = true;
        socketMode = false;
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        Command(Monstey_ModelPreview);
        modelPreview->Close();
        return;
    }
    if (smokeStage == 2 && smoke.value("preview_stale_results_rejected", 0u).as_uint() > 0)
    {
        Monstey_Require(!modelPreview->Handle(), "Delayed completion reopened a closed preview");
        smoke["preview_closed_load_result_rejected"] = true;
        smokeStage = 3;
        const auto bad = smokeDirectory / L"malformed.mesh";
        Monstey_WriteNewFile(bad, Monstey_Bytes{1, 2, 3});
        animationMode = false;
        socketMode = true;
        UpdateButtons();
        Monstey_SmokeDialogPaths.push_back(bad);
        Command(Monstey_ModelPreview);
        return;
    }
    if (smokeStage == 3 && view["failed"].as_bool())
    {
        Monstey_Require(!view["loading"].as_bool() && !view["error"].as_string().empty() && IsWindowEnabled(window),
                        "Malformed preview left UI pending or disabled");
        smoke["preview_malformed_load_reports_and_recovers"] = true;
        smokeStage = 4;
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        Command(Monstey_ModelPreview);
        return;
    }
    if (smokeStage == 4 && view["ready"].as_bool())
    {
        smokeStage = 5;
        SendMessageW(modelPreview->Handle(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
        Monstey_Require(modelPreview->Inspect()["zoom"].as_number() > 1, "Preview zoom did not change");
        PostMessageW(GetDlgItem(modelPreview->Handle(), 100), WM_KEYDOWN, 'F', 0);
        return;
    }
    if (smokeStage == 5)
    {
        Monstey_Require(modelPreview->Inspect()["zoom"].as_number() == 1,
                        "Main loop did not route child-focus preview F");
        Monstey_Require(smoke.contains("preview_session_generation") &&
                            generation == smoke["preview_session_generation"].as_uint() &&
                            revision == smoke["preview_session_revision"].as_uint(),
                        "Typed preview result altered the Runtime session");
        smoke["preview_keyboard_routes_before_main_accelerators"] = true;
        smoke["preview_preserves_runtime_generation_revision"] = true;
        Monstey_WriteNewFile(smokeDirectory / L"preview.bmp", modelPreview->Snapshot());
        if (smoke.contains("preview_skeleton_sha256"))
        {
            auto sibling = initialDatabase;
            sibling.replace_extension(L".skel");
            Monstey_Require(Monstey_HashSha256(sibling) == smoke["preview_skeleton_sha256"].as_string(),
                            "Preview changed source skeleton");
        }
        smoke["preview_skeleton_unchanged"] = true;
        smoke["preview_final_view"] = modelPreview->Inspect();
        smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
        smokeStage = 6;
        PostMessageW(modelPreview->Handle(), WM_CLOSE, 0, 0);
        return;
    }
    if (smokeStage == 6 && !modelPreview->Handle())
    {
        smoke["preview_own_close_has_no_WM_QUIT"] = true;
        SmokeComplete();
    }
}

} // namespace Monstey::Monstey_AppInternal
