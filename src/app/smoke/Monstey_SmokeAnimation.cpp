#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeAnimationStep()
{
    if (smokeStorageBusy)
        return;
    smokeStorageBusy = true;
    struct Monstey_AnimationSmokeReset
    {
        bool &value;
        ~Monstey_AnimationSmokeReset()
        {
            value = false;
        }
    } reset{smokeStorageBusy};
    {
        std::lock_guard lock(mutex);
        if (active.load() || !jobs.empty() || !results.empty())
            return;
    }
    const auto ready = editor && editor->window && !editor->pending;
    const auto clipReady = !animationClip.empty() && animationClip.at("name") == J(animationSelected) &&
                           animationClip.at("ui_generation").as_uint() == generation &&
                           animationClip.at("ui_revision").as_uint() == revision;
    const auto find = [&](const std::string &name) -> int
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("name") == J(name))
                return static_cast<int>(i);
        return -1;
    };
    const auto click = [&](int id)
    {
        const auto button = GetDlgItem(window, id);
        Monstey_Require((GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE) && IsWindowEnabled(button),
                        "Animation toolbar action unavailable: " + std::to_string(id));
        if (!smoke.contains("toolbar_gestures"))
            smoke["toolbar_gestures"] = J::array();
        smoke["toolbar_gestures"].as_array().push_back(id);
        SendMessageW(button, BM_CLICK, 0, 0);
    };
    const auto apply = [&]
    {
        Monstey_Require(ready, "Animation form did not open");
        SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
    };
    const auto dialog = [&](int id, const std::filesystem::path &path)
    {
        Monstey_SmokeDialogPaths.push_back(path);
        click(id);
        Monstey_Require(Monstey_SmokeDialogPaths.empty(), "Animation action did not open a file dialog");
    };
    const auto choose = [&](const std::string &name, bool doubleClick = false)
    {
        const auto row = find(name);
        Monstey_Require(row >= 0, "Animation UI clip is missing: " + name);
        ListView_SetItemState(records, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        Monstey_ListSelect(records, row);
        if (doubleClick)
        {
            NMITEMACTIVATE event{};
            event.hdr = {records, Monstey_Records, NM_DBLCLK};
            event.iItem = row;
            SendMessageW(window, WM_NOTIFY, Monstey_Records, reinterpret_cast<LPARAM>(&event));
        }
    };
    const auto project = smokeDirectory / L"edited.animation.json";
    const auto scene = smokeDirectory / L"edited.scene.json";
    const auto exported = smokeDirectory / L"single-clip.scene.json";
    const auto candidate = smokeDirectory / L"candidate.anix";
    if (smokeArguments.value("raw_event_bytes", false).as_bool())
    {
        const auto target = smokeArguments.value("raw_clip", "PL_GL_Might_L1_AOE_LL_2_Idle").as_string();
        const auto sortEvents = [](J &events)
        {
            std::stable_sort(events.as_array().begin(), events.as_array().end(),
                             [](const J &a, const J &b) { return a.at("tick").as_uint() < b.at("tick").as_uint(); });
        };
        const auto rawRow = [&]() -> std::size_t
        {
            const auto &events = animationClip.at("events");
            for (std::size_t row = 0; row < events.size(); ++row)
                if (events[row].value("text_bytes_hex", "") == smoke.at("raw_current_hex") &&
                    events[row].at("tick") == smoke.at("raw_current_tick"))
                    return row;
            throw Monstey_Error("smoke", "Expected raw event row is missing");
        };
        const auto openRawRow = [&]
        {
            const auto row = rawRow();
            Monstey_ListSelect(fields, static_cast<int>(row));
            NMITEMACTIVATE event{};
            event.hdr = {fields, Monstey_Fields, NM_DBLCLK};
            event.iItem = static_cast<int>(row);
            SendMessageW(window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&event));
        };
        const auto verifyEvents = [&]
        {
            Monstey_Require(animationClip.at("events") == smoke.at("raw_expected_events"),
                            "Typed UI changed raw bytes, canonical mirrors or unrelated event rows");
            (void)J::Parse(animationClip.at("events").dump());
        };
        if (smokeStage == 1 && clipReady && !rowData.empty())
        {
            Monstey_Require(Monstey_HashSha256(initialDatabase) == smoke.at("source_sha256").as_string(),
                            "Raw event UI source changed before opening");
            smoke["raw_event_byte_workflow"] = true;
            smoke["original_clip_count"] = animationView.at("clip_count");
            smoke["source_clip"] = target;
            smokeStage = 600;
            choose(target, true);
        }
        else if (smokeStage == 600 && clipReady && animationSelected == target)
        {
            Monstey_Require(animationClip.value("event_edit_supported", false).as_bool() &&
                                animationClip.value("sample_supported", false).as_bool() &&
                                !animationClip.value("empty_native_clip", false).as_bool(),
                            "Raw event smoke requires a qualified nonempty actual clip");
            for (const auto key :
                 {"empty_native_clip", "sample_supported", "unique_track_channels", "repeated_channel_bindings"})
            {
                const auto shown = std::any_of(fieldData.begin(), fieldData.end(),
                                               [&](const J &row) { return row.value("name", "") == J(key); });
                Monstey_Require(shown, "Native capability metadata is not exposed: " + std::string(key));
            }
            smoke["four_capability_fields_visible"] = true;
            smoke["raw_original_events"] = animationClip.at("events");
            smoke["raw_expected_events"] = animationClip.at("events");
            bool found = false;
            for (const auto &event : animationClip.at("events").as_array())
                if (event.contains("text_bytes_hex"))
                {
                    smoke["raw_current_hex"] = event.at("text_bytes_hex");
                    smoke["raw_current_tick"] = event.at("tick");
                    found = true;
                    break;
                }
            Monstey_Require(found, "Actual source clip contains no raw event row");
            smokeStage = 601;
            click(Monstey_AnimationEvents);
            openRawRow();
        }
        else if (smokeStage == 601 && ready)
        {
            Monstey_Require(editor->formFields[1].at("name") == J("text_bytes_hex") &&
                                Monstey_Utf8(Monstey_WindowText(editor->formControls.at(1))) ==
                                    smoke.at("raw_current_hex").as_string(),
                            "Raw event form is not editing explicit hexadecimal bytes");
            const auto tick = smoke.at("raw_current_tick").as_uint() + 1;
            Monstey_Require(tick < animationClip.at("duration_ticks").as_uint(),
                            "Raw event smoke needs room for the typed tick edits");
            auto expected = smoke.at("raw_expected_events");
            expected[rawRow()]["tick"] = tick;
            sortEvents(expected);
            smoke["raw_expected_events"] = expected;
            smoke["raw_current_tick"] = tick;
            SetWindowTextW(editor->formControls.at(0), std::to_wstring(tick).c_str());
            smokeStage = 602;
            apply();
        }
        else if (smokeStage == 602 && clipReady && !ready && animationSelected == target)
        {
            verifyEvents();
            smoke["typed_tick_only_preserves_raw_bytes"] = true;
            smokeStage = 603;
            openRawRow();
        }
        else if (smokeStage == 603 && ready)
        {
            auto expected = smoke.at("raw_expected_events");
            const auto row = rawRow();
            const auto tick = smoke.at("raw_current_tick").as_uint() + 1;
            const auto hex = smoke.at("raw_current_hex").as_string() + "20";
            Monstey_Require(tick <= animationClip.at("duration_ticks").as_uint(),
                            "Typed raw event tick exceeds source duration");
            expected[row]["tick"] = tick;
            expected[row]["text_bytes_hex"] = hex;
            expected[row]["text"] = expected[row].at("text").as_string() + " ";
            sortEvents(expected);
            smoke["raw_expected_events"] = expected;
            smoke["raw_current_hex"] = hex;
            smoke["raw_current_tick"] = tick;
            SetWindowTextW(editor->formControls.at(0), std::to_wstring(tick).c_str());
            SetWindowTextW(editor->formControls.at(1), Monstey_Wide(hex).c_str());
            smokeStage = 604;
            apply();
        }
        else if (smokeStage == 604 && clipReady && !ready && animationSelected == target)
        {
            verifyEvents();
            smoke["typed_raw_hex_and_tick_preserve_bytes"] = true;
            smoke["canonical_raw_mirror_regenerated"] = true;
            smokeStage = 605;
            click(Monstey_AnimationEventAdd);
        }
        else if (smokeStage == 605 && ready)
        {
            const auto tick = std::min<std::uint64_t>(20, animationClip.at("duration_ticks").as_uint());
            auto expected = smoke.at("raw_expected_events");
            expected.as_array().push_back(J::object({{"tick", tick}, {"text", "UI_RAW_ASCII"}}));
            sortEvents(expected);
            smoke["raw_expected_events"] = expected;
            SetWindowTextW(editor->formControls.at(0), std::to_wstring(tick).c_str());
            SetWindowTextW(editor->formControls.at(1), L"UI_RAW_ASCII");
            smokeStage = 606;
            apply();
        }
        else if (smokeStage == 606 && clipReady && !ready && animationSelected == target)
        {
            verifyEvents();
            auto expected = smoke.at("raw_expected_events");
            std::size_t row = expected.size();
            for (std::size_t i = 0; i < expected.size(); ++i)
                if (!expected[i].contains("text_bytes_hex") && expected[i].at("text") == J("UI_RAW_ASCII"))
                {
                    row = i;
                    break;
                }
            Monstey_Require(row < expected.size(), "Typed ASCII event was not added");
            expected.as_array().erase(expected.as_array().begin() + row);
            smoke["raw_expected_events"] = expected;
            Monstey_ListSelect(fields, static_cast<int>(row));
            smokeStage = 607;
            click(Monstey_AnimationEventRemove);
        }
        else if (smokeStage == 607 && clipReady && !ready && animationSelected == target)
        {
            verifyEvents();
            smoke["ascii_add_remove_preserves_other_raw_rows"] = true;
            smokeStage = 608;
            openRawRow();
        }
        else if (smokeStage == 608 && ready)
        {
            smoke["raw_reject_generation"] = generation;
            smoke["raw_reject_revision"] = revision;
            smoke["raw_reject_effective_sha256"] = animationView.at("effective_sha256");
            SetWindowTextW(editor->formControls.at(1), L"zz");
            smokeStage = 609;
            apply();
        }
        else if (smokeStage == 610 && clipReady && !ready && animationSelected == target)
        {
            verifyEvents();
            Monstey_Require(animationView.at("effective_sha256") == smoke.at("raw_reject_effective_sha256") &&
                                J(generation) == smoke.at("raw_reject_generation") &&
                                J(revision) == smoke.at("raw_reject_revision"),
                            "Malformed raw hex mutated native state or history");
            smoke["malformed_raw_hex_rejected_transactionally"] = true;
            smokeStage = 611;
            dialog(Monstey_Save, project);
        }
        else if (smokeStage == 611 && clipReady && std::filesystem::is_regular_file(project))
        {
            smokeStage = 612;
            dialog(Monstey_Load, project);
        }
        else if (smokeStage == 612 && clipReady && animationView.at("source") == J(Monstey_JsonPath(initialDatabase)))
        {
            smokeStage = 613;
            choose(target);
        }
        else if (smokeStage == 613 && clipReady && animationSelected == target)
        {
            verifyEvents();
            smoke["raw_project_save_reload_preserves_bytes"] = true;
            smokeStage = 614;
            dialog(Monstey_Build, candidate);
        }
        else if (smokeStage == 614 && clipReady && std::filesystem::is_regular_file(candidate))
        {
            Monstey_Anix readback(Monstey_ReadFile(candidate));
            Monstey_Require(readback.EventInfo(target).at("events") == smoke.at("raw_expected_events"),
                            "Typed raw event build changed bytes or display mirrors");
            const auto rawBuildMetadata = readback.Describe();
            Monstey_Require(rawBuildMetadata.at("clip_count") == smoke.at("original_clip_count") &&
                                rawBuildMetadata.at("directory_footer_consistent").as_bool() &&
                                Monstey_HashSha256(initialDatabase) == smoke.at("source_sha256").as_string(),
                            "Raw event GUI build readback or original source hash differs");
            smoke["native_build_readback"] = rawBuildMetadata;
            smoke["candidate_sha256"] = Monstey_HashSha256(candidate);
            smoke["raw_native_build_preserves_bytes"] = true;
            smokeStage = 615;
            dialog(Monstey_Open, candidate);
        }
        else if (smokeStage == 615 && clipReady && animationView.at("source") == J(Monstey_JsonPath(candidate)))
        {
            smokeStage = 616;
            choose(target);
        }
        else if (smokeStage == 616 && clipReady && animationSelected == target)
        {
            verifyEvents();
            smoke["built_anix_reopened"] = true;
            smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
            smoke["native_cpp"] = true;
            smoke["runtime_playback_verified"] = false;
            SmokeComplete();
        }
        return;
    }
    if (smokeArguments.value("empty_native_clip", false).as_bool())
    {
        const auto target = smokeArguments.value("empty_clip", "base").as_string();
        const std::string renamed = "UI_EmptyRenamed", held = "UI_FromBindWhileEmpty";
        const auto verifyEmpty = [&]
        {
            Monstey_Require(animationClip.value("empty_native_clip", false).as_bool() &&
                                !animationClip.value("sample_supported", true).as_bool() &&
                                !animationClip.value("event_edit_supported", true).as_bool() &&
                                animationClip.at("segments").empty(),
                            "Selected actual clip is not the native empty capability profile");
            for (auto id : {Monstey_AnimationTiming, Monstey_AnimationSample, Monstey_AnimationEventClear,
                            Monstey_AnimationEventAdd, Monstey_AnimationEventEdit, Monstey_AnimationEventRemove})
                Monstey_Require(!IsWindowEnabled(GetDlgItem(window, id)),
                                "Empty native clip left an unsupported toolbar action enabled");
            for (auto id : {Monstey_Rename, Monstey_Extract, Monstey_AnimationHeld})
                Monstey_Require(IsWindowEnabled(GetDlgItem(window, id)),
                                "Empty selection disabled an independent structural/bind action");
        };
        if (smokeStage == 1 && clipReady && !rowData.empty())
        {
            smoke["empty_native_clip_workflow"] = true;
            smoke["original_clip_count"] = animationView.at("clip_count");
            if (!animationClip.value("empty_native_clip", false).as_bool() &&
                !animationClip.value("event_edit_supported", true).as_bool())
            {
                Monstey_Require(IsWindowEnabled(GetDlgItem(window, Monstey_AnimationEventClear)),
                                "Nonempty recovery clear was disabled by the broad event-edit flag");
                smoke["nonempty_recovery_clear_remains_available"] = true;
            }
            smokeStage = 700;
            choose(target, true);
        }
        else if (smokeStage == 700 && clipReady && animationSelected == target)
        {
            verifyEmpty();
            for (const auto key :
                 {"empty_native_clip", "sample_supported", "unique_track_channels", "repeated_channel_bindings"})
                Monstey_Require(std::any_of(fieldData.begin(), fieldData.end(),
                                            [&](const J &row) { return row.value("name", "") == J(key); }),
                                "Empty capability metadata is not visible: " + std::string(key));
            smoke["empty_unsupported_actions_disabled"] = true;
            smoke["empty_export_rename_and_bind_held_enabled"] = true;
            smoke["four_capability_fields_visible"] = true;
            smokeStage = 701;
            dialog(Monstey_Extract, exported);
        }
        else if (smokeStage == 701 && clipReady && std::filesystem::is_regular_file(exported))
        {
            const auto sceneDocument = J::Parse(Monstey_ReadText(exported));
            Monstey_Require(sceneDocument.at("clips").size() == 1, "Empty selected export changed the clip scope");
            const auto &empty = sceneDocument.at("clips")[0];
            Monstey_Require(empty.at("empty_native_clip").as_bool() && empty.at("structural_only").as_bool() &&
                                !empty.at("semantic_reimport_supported").as_bool() && empty.at("ticks").empty() &&
                                empty.at("tracks").empty(),
                            "Empty GUI export invented semantic pose samples");
            smoke["empty_structural_metadata_exported"] = true;
            smokeStage = 702;
            click(Monstey_Rename);
        }
        else if (smokeStage == 702 && ready)
        {
            SetWindowTextW(editor->formControls.at(0), Monstey_Wide(renamed).c_str());
            smokeStage = 703;
            apply();
        }
        else if (smokeStage == 703 && clipReady && animationSelected == renamed)
        {
            verifyEmpty();
            smoke["empty_native_clip_renamed"] = true;
            smokeStage = 704;
            click(Monstey_AnimationHeld);
        }
        else if (smokeStage == 704 && ready)
        {
            Monstey_Require(editor->formControls.size() == 3, "Held did not retain its existing library-bind form");
            SetWindowTextW(editor->formControls.at(0), Monstey_Wide(held).c_str());
            SetWindowTextW(editor->formControls.at(1), L"0");
            SetWindowTextW(editor->formControls.at(2), L"1000");
            smokeStage = 705;
            apply();
        }
        else if (smokeStage == 705 && clipReady && animationSelected == held)
        {
            Monstey_Require(!animationClip.value("empty_native_clip", true).as_bool() &&
                                animationClip.value("sample_supported", false).as_bool(),
                            "Held sampled the selected empty clip instead of the normal bind source");
            smoke["held_uses_bind_while_empty_selected"] = true;
            smoke["empty_bind_GUI_error_path_exercised"] = false;
            smokeStage = 706;
            click(Monstey_Delete);
        }
        else if (smokeStage == 706 && clipReady && find(held) < 0)
        {
            smokeStage = 707;
            choose(renamed);
        }
        else if (smokeStage == 707 && clipReady && animationSelected == renamed)
        {
            verifyEmpty();
            smokeStage = 708;
            dialog(Monstey_Save, project);
        }
        else if (smokeStage == 708 && clipReady && std::filesystem::is_regular_file(project))
        {
            smokeStage = 709;
            dialog(Monstey_Load, project);
        }
        else if (smokeStage == 709 && clipReady && animationView.at("source") == J(Monstey_JsonPath(initialDatabase)))
        {
            smokeStage = 710;
            choose(renamed);
        }
        else if (smokeStage == 710 && clipReady && animationSelected == renamed)
        {
            verifyEmpty();
            smoke["empty_project_save_reload"] = true;
            smokeStage = 711;
            dialog(Monstey_Build, candidate);
        }
        else if (smokeStage == 711 && clipReady && std::filesystem::is_regular_file(candidate))
        {
            Monstey_Anix readback(Monstey_ReadFile(candidate));
            const auto buildMetadata = readback.Describe();
            const auto &clips = buildMetadata.at("clips").as_array();
            const auto empty =
                std::find_if(clips.begin(), clips.end(), [&](const J &row) { return row.at("name") == J(renamed); });
            Monstey_Require(empty != clips.end() && empty->at("empty_native_clip").as_bool() &&
                                !empty->at("sample_supported").as_bool() &&
                                buildMetadata.at("clip_count") == smoke.at("original_clip_count") &&
                                buildMetadata.at("directory_footer_consistent").as_bool() &&
                                Monstey_HashSha256(initialDatabase) == smoke.at("source_sha256").as_string(),
                            "Empty GUI native build changed its profile or original source");
            smoke["native_build_readback"] = buildMetadata;
            smoke["candidate_sha256"] = Monstey_HashSha256(candidate);
            smoke["empty_native_build_preserves_profile"] = true;
            smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
            smoke["native_cpp"] = true;
            smoke["runtime_playback_verified"] = false;
            SmokeComplete();
        }
        return;
    }
    if (smokeStage == 1 && clipReady && !rowData.empty())
    {
        Monstey_Require(TabCtrl_GetItemCount(tabs) == 7, "Animation mode added a top-level tab");
        smoke["seven_tabs_preserved"] = true;
        smoke["original_clip_count"] = animationView.at("clip_count");
        smoke["opaque_native_source"] = !animationView.at("opaque_codecs").empty();
        const auto unsupportedAuthoring = animationView.value("unsupported_authoring_codecs", J::array());
        smoke["unsupported_authoring_codecs_reported"] = unsupportedAuthoring;
        smoke["requires_structural_animation_smoke"] = smokeArguments.value("structural_only", false).as_bool() ||
                                                       !animationView.at("opaque_codecs").empty() ||
                                                       !unsupportedAuthoring.empty();
        const bool auxiliary = smokeArguments.value("auxiliary_authoring", false).as_bool();
        smoke["auxiliary_authoring_workflow"] = auxiliary;
        std::string target;
        for (const auto &row : rowData)
            if (row.at("name") != J("bind"))
            {
                target = row.at("name").as_string();
                break;
            }
        if (auxiliary)
            target = smokeArguments.value("auxiliary_clip", "OV_GetUp_Inf_A").as_string();
        Monstey_Require(!target.empty(), "Animation smoke requires a non-bind source clip");
        smoke["source_clip"] = target;
        smokeStage = 2;
        choose(target, true);
    }
    else if (smokeStage == 2 && clipReady && animationSelected == smoke.at("source_clip").as_string())
    {
        MoveWindow(window, 0, 0, 1000, 900, FALSE);
        Layout();
        RECT client{};
        GetClientRect(window, &client);
        for (auto button : buttons)
            if (GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE)
            {
                RECT rect{};
                GetWindowRect(button, &rect);
                MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&rect), 2);
                Monstey_Require(rect.left >= 0 && rect.right <= client.right,
                                "Animation action lies outside minimum-width client area");
            }
        smoke["animation_toolbar_fits_minimum_width"] = true;
        if (smoke.at("auxiliary_authoring_workflow").as_bool())
        {
            Monstey_Anix expected(Monstey_ReadFile(initialDatabase));
            expected.Edit(J::array(
                {J::object({{"op", "duplicate"}, {"clip", smoke.at("source_clip")}, {"name", "UI_AuxExpected"}}),
                 J::object({{"op", "length"}, {"clip", "UI_AuxExpected"}, {"duration_ticks", 2000}}),
                 J::object({{"op", "rate"}, {"clip", "UI_AuxExpected"}, {"factor", 2.0}}),
                 J::object({{"op", "delay"}, {"clip", "UI_AuxExpected"}, {"at_tick", 250}, {"duration_ticks", 250}}),
                 J::object({{"op", "trim"}, {"clip", "UI_AuxExpected"}, {"start_tick", 250}, {"end_tick", 1000}})}));
            const std::array<double, 3> ticks{0, 375, 750};
            smoke["auxiliary_expected_samples"] = expected.Sample("UI_AuxExpected", ticks, true);
        }
        smokeStage = 13;
        TabCtrl_SetCurSel(tabs, 2);
        TabChanged();
        SendMessageW(GetDlgItem(window, Monstey_New), BM_CLICK, 0, 0);
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
    }
    else if (smokeStage == 13 && clipReady && !ready)
    {
        smoke["external_domain_mutation_refreshes_animation_guards"] = true;
        smokeStage = 3;
        click(Monstey_Clone);
    }
    else if (smokeStage == 3 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"UI_Duplicate");
        smokeStage = 4;
        apply();
    }
    else if (smokeStage == 4 && clipReady && animationSelected == "UI_Duplicate")
    {
        Monstey_Require(dirty[4] && canUndo[4], "Animation edit did not update dirty/history state");
        smoke["duplicate_selects_native_identity"] = true;
        smokeStage = 5;
        click(Monstey_Rename);
    }
    else if (smokeStage == 5 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"UI_Renamed");
        smokeStage = 6;
        apply();
    }
    else if (smokeStage == 6 && clipReady && animationSelected == "UI_Renamed")
    {
        smokeStage = 7;
        click(Monstey_Undo);
    }
    else if (smokeStage == 7 && clipReady && find("UI_Duplicate") >= 0 && find("UI_Renamed") < 0)
    {
        smokeStage = 8;
        click(Monstey_Redo);
    }
    else if (smokeStage == 8 && clipReady && find("UI_Renamed") >= 0)
    {
        if (animationSelected != "UI_Renamed")
        {
            choose("UI_Renamed");
            return;
        }
        smoke["rename_undo_redo_native_identity"] = true;
        smokeStage = 14;
        click(Monstey_AnimationChanges);
    }
    else if (smokeStage == 14 && ready)
    {
        Monstey_Require(editor->formControls.size() == 3 &&
                            SendMessageW(editor->formControls[2], BM_GETCHECK, 0, 0) == BST_UNCHECKED,
                        "History form does not default to compact payloads");
        smokeStage = 15;
        apply();
    }
    else if (smokeStage == 15 && clipReady && !ready && animationLastReport.contains("operations_included"))
    {
        Monstey_Require(!animationLastReport.at("operations_included").as_bool() &&
                            animationLastReport.at("returned_steps").as_uint() > 0 &&
                            animationLastReport.at("returned_steps").as_uint() <= 100,
                        "Typed history page did not return compact steps");
        for (const auto &step : animationLastReport.at("steps").as_array())
            Monstey_Require(!step.contains("operations"), "Compact history exposed a full operation payload");
        smoke["typed_compact_history_verified"] = true;
        if (smoke.at("requires_structural_animation_smoke").as_bool())
        {
            smoke["opaque_codecs_reported"] = animationView.at("opaque_codecs");
            smoke["opaque_pose_timing_edits_attempted"] = false;
            smokeStage = 200;
        }
        else
        {
            smokeStage = 9;
            dialog(Monstey_Extract, exported);
        }
    }
    else if (smokeStage == 9 && clipReady && std::filesystem::is_regular_file(exported))
    {
        auto bom = Monstey_ReadFile(exported);
        bom.insert(bom.begin(), {0xef, 0xbb, 0xbf});
        const auto bomFile = smokeDirectory / L"utf8-bom.scene.json";
        Monstey_WriteNewFile(bomFile, bom);
        smoke["semantic_bom_source_sha256"] = Monstey_HashSha256(bomFile);
        smokeStage = 10;
        dialog(Monstey_Add, bomFile);
    }
    else if (smokeStage == 10 && ready)
    {
        SetWindowTextW(editor->formControls.at(1), L"UI_Imported");
        smokeStage = 11;
        apply();
    }
    else if (smokeStage == 11 && clipReady && animationSelected == "UI_Imported")
    {
        smoke["semantic_clip_file_import_export"] = true;
        smoke["utf8_bom_semantic_import"] = true;
        smokeStage = 12;
        click(Monstey_Delete);
    }
    else if (smokeStage == 12 && clipReady && find("UI_Imported") < 0)
    {
        if (animationSelected != "UI_Renamed")
        {
            choose("UI_Renamed");
            return;
        }
        smoke["delete_native_clip"] = true;
        if (smoke.at("requires_structural_animation_smoke").as_bool())
        {
            smoke["opaque_codecs_reported"] = animationView.at("opaque_codecs");
            smoke["opaque_pose_timing_edits_attempted"] = false;
            smokeStage = 200;
        }
        else
        {
            smoke["timing_cursor"] = 0;
            smokeStage = 20;
        }
    }
    else if (smokeStage == 20 && clipReady && !ready)
    {
        smokeStage = 21;
        click(Monstey_AnimationTiming);
    }
    else if (smokeStage == 21 && ready && editor->formControls.size() == 1)
    {
        SendMessageW(editor->formControls[0], CB_SETCURSEL, smoke.at("timing_cursor").as_uint(), 0);
        smokeStage = 22;
        apply();
    }
    else if (smokeStage == 22 && ready)
    {
        const auto cursor = smoke.at("timing_cursor").as_uint();
        const wchar_t *first[] = {L"2000", L"2", L"250", L"250"};
        SetWindowTextW(editor->formControls.at(0), first[cursor]);
        if (cursor == 2 || cursor == 3)
            SetWindowTextW(editor->formControls.at(1), cursor == 2 ? L"250" : L"1000");
        smokeStage = 23;
        apply();
    }
    else if (smokeStage == 23 && clipReady && !ready)
    {
        const auto cursor = smoke.at("timing_cursor").as_uint();
        const std::uint64_t durations[] = {2000, 1000, 1250, 750};
        Monstey_Require(animationClip.at("duration_ticks").as_uint() == durations[cursor],
                        "Typed animation timing changed the wrong duration");
        smoke["timing_cursor"] = cursor + 1;
        if (cursor == 3)
        {
            smoke["typed_duration_rate_delay_trim"] = true;
            smokeStage = 30;
        }
        else
            smokeStage = 20;
    }
    else if (smokeStage == 30 && clipReady)
    {
        Monstey_Require(animationClip.at("event_edit_supported").as_bool(),
                        "Synthetic native event binding is not qualified");
        smoke["event_baseline"] = animationClip.at("events").size();
        smokeStage = 31;
        click(Monstey_AnimationEvents);
        click(Monstey_AnimationEventAdd);
    }
    else if (smokeStage == 31 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"100");
        SetWindowTextW(editor->formControls.at(1), L"UI_HIT");
        smokeStage = 32;
        apply();
    }
    else if (smokeStage == 32 && clipReady && !ready && animationEventMode)
    {
        Monstey_Require(fieldData.size() == smoke.at("event_baseline").as_uint() + 1,
                        "Event add changed wrong native event list");
        Monstey_ListSelect(fields, 0);
        NMITEMACTIVATE event{};
        event.hdr = {fields, Monstey_Fields, NM_DBLCLK};
        event.iItem = 0;
        smokeStage = 33;
        SendMessageW(window, WM_NOTIFY, Monstey_Fields, reinterpret_cast<LPARAM>(&event));
    }
    else if (smokeStage == 33 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"200");
        SetWindowTextW(editor->formControls.at(1), L"UI_EDITED");
        smokeStage = 34;
        apply();
    }
    else if (smokeStage == 34 && clipReady && !ready)
    {
        Monstey_Require(fieldData.at(0).at("tick") == J(200) && fieldData.at(0).at("text") == J("UI_EDITED"),
                        "Event double-click did not update the selected event");
        Monstey_ListSelect(fields, 0);
        smokeStage = 35;
        click(Monstey_AnimationEventRemove);
    }
    else if (smokeStage == 35 && clipReady && fieldData.size() == smoke.at("event_baseline").as_uint())
    {
        smokeStage = 36;
        click(Monstey_AnimationEventClear);
    }
    else if (smokeStage == 36 && clipReady && fieldData.empty())
    {
        smokeStage = 37;
        click(Monstey_Undo);
    }
    else if (smokeStage == 37 && clipReady && fieldData.size() == smoke.at("event_baseline").as_uint())
    {
        smokeStage = 38;
        click(Monstey_Redo);
    }
    else if (smokeStage == 38 && clipReady && fieldData.empty())
    {
        smoke["event_add_double_click_edit_remove_clear_history"] = true;
        smokeStage = 39;
        SetWindowTextW(query, L"not-a-real-clip");
        click(Monstey_Search);
    }
    else if (smokeStage == 39 && rowData.empty() && animationClip.empty())
    {
        smoke["held_available_without_filtered_selection"] = true;
        smokeStage = 40;
        click(Monstey_AnimationHeld);
    }
    else if (smokeStage == 40 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"UI_Held");
        SetWindowTextW(editor->formControls.at(1), L"0");
        SetWindowTextW(editor->formControls.at(2), L"1000");
        smokeStage = 41;
        apply();
    }
    else if (smokeStage == 41 && clipReady && animationSelected == "UI_Held")
    {
        smoke["held_from_bind"] = true;
        smokeStage = 42;
        click(Monstey_Delete);
    }
    else if (smokeStage == 42 && clipReady && find("UI_Held") < 0)
    {
        smokeStage = 43;
        choose("UI_Renamed");
    }
    else if (smokeStage == 43 && clipReady && animationSelected == "UI_Renamed")
    {
        smokeStage = 44;
        click(Monstey_AnimationSample);
    }
    else if (smokeStage == 44 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"0");
        SetWindowTextW(editor->formControls.at(1), L"750");
        SetWindowTextW(editor->formControls.at(2), L"3");
        smokeStage = 45;
        apply();
    }
    else if (smokeStage == 45 && clipReady && animationLastReport.contains("complete_pose_codecs"))
    {
        Monstey_Require(animationLastReport.at("complete_pose_codecs").as_bool() &&
                            animationLastReport.at("ticks").size() == 3 && !animationLastReport.at("tracks").empty(),
                        "Typed tick sampling did not return complete native channels");
        Monstey_Require(animationLastReport.at("clip") == J("UI_Renamed"),
                        "Tick sampling inspected a different native clip");
        if (smoke.at("auxiliary_authoring_workflow").as_bool())
        {
            const auto &expected = smoke.at("auxiliary_expected_samples");
            std::size_t count = 0;
            for (const auto &track : expected.at("tracks").as_array())
            {
                if (track.value("native_role", "").as_string() != "auxiliary")
                    continue;
                const auto &rows = animationLastReport.at("tracks").as_array();
                const auto found = std::find_if(rows.begin(), rows.end(), [&](const J &row)
                                                { return row.at("channel_index") == track.at("channel_index"); });
                Monstey_Require(found != rows.end() && found->at("native_codec") == track.at("native_codec") &&
                                    found->at("native_bits") == track.at("native_bits"),
                                "Typed native UI changed an auxiliary target or "
                                "numeric/raw payload");
                ++count;
            }
            Monstey_Require(count == 40, "Typed native UI did not retain all forty auxiliary channels");
            smoke["numeric_auxiliary_trajectory_verified"] = true;
            smoke["auxiliary_channels_verified"] = count;
        }
        else
        {
            bool checkedRoot = false;
            for (const auto &track : animationLastReport.at("tracks").as_array())
                if (track.at("channel_index").as_uint() == 0)
                {
                    const double expected[] = {5.0, 7.5, 15.0};
                    for (std::size_t i = 0; i < 3; ++i)
                        Monstey_Require(std::abs(track.at("values")[i][0].as_number() - expected[i]) < 0.0001,
                                        "Typed timing/sample root trajectory is incorrect");
                    checkedRoot = true;
                }
            Monstey_Require(checkedRoot, "Native root position channel is missing from samples");
        }
        smoke["numeric_tick_trajectory_verified"] = true;
        smoke["native_tick_samples"] = animationLastReport;
        auto rig = smokeArguments.contains("skeleton")
                       ? std::filesystem::path(Monstey_Wide(smokeArguments.at("skeleton").as_string()))
                       : initialDatabase.parent_path() / L"fixture.skel";
        if (std::filesystem::is_regular_file(rig))
        {
            smoke["skeleton_source"] = Monstey_JsonPath(rig);
            smoke["skeleton_source_sha256"] = Monstey_HashSha256(rig);
            smokeStage = 50;
            dialog(Monstey_AnimationSkeleton, rig);
        }
        else
            smokeStage = 200;
    }
    else if (smokeStage == 50 && ready)
    {
        SetWindowTextW(editor->formControls.at(0), L"characters/animationcharacters/monstey/ui_animation_test.anix");
        Monstey_SmokeDialogPaths.push_back(smokeDirectory / L"registered.skel");
        smokeStage = 51;
        apply();
    }
    else if (smokeStage == 51 && clipReady && std::filesystem::is_regular_file(smokeDirectory / L"registered.skel"))
    {
        const auto rig = std::filesystem::path(Monstey_Wide(smoke.at("skeleton_source").as_string()));
        const auto before = Monstey_AnixSkeletonInspect(Monstey_ReadFile(rig));
        const auto after = Monstey_AnixSkeletonInspect(Monstey_ReadFile(smokeDirectory / L"registered.skel"));
        Monstey_Require(after.at("animation_count").as_uint() == before.at("animation_count").as_uint() + 1 &&
                            before.at("rig_sha256") == after.at("rig_sha256") &&
                            before.at("lods_sha256") == after.at("lods_sha256") &&
                            Monstey_HashSha256(rig) == smoke.at("skeleton_source_sha256").as_string(),
                        "SKEL library registration readback or source hash differs");
        smoke["qualified_128bone_skel_registration"] = after;
        smokeStage = 200;
    }
    else if (smokeStage == 200 && clipReady && !ready)
    {
        smokeStage = 201;
        dialog(Monstey_Save, project);
    }
    else if (smokeStage == 201 && clipReady && std::filesystem::is_regular_file(project))
    {
        smokeStage = 202;
        dialog(Monstey_AnimationExportAll, scene);
    }
    else if (smokeStage == 202 && clipReady && std::filesystem::is_regular_file(scene) &&
             animationLastReport.contains("non_reimportable_clips") &&
             animationLastReport.value("path", "") == J(Monstey_JsonPath(scene)))
    {
        const bool allReimportable = animationLastReport.at("non_reimportable_clips").empty();
        smoke["whole_semantic_library_reimportable"] = allReimportable;
        if (smoke.at("requires_structural_animation_smoke").as_bool() || !allReimportable)
        {
            smokeStage = 204;
            dialog(Monstey_Load, project);
        }
        else
        {
            smokeStage = 203;
            dialog(Monstey_New, scene);
        }
        Monstey_Require(rowData.empty() && animationClip.empty(), "Animation replacement left old actionable rows");
    }
    else if (smokeStage == 203 && clipReady && animationView.at("source_kind") == J("semantic"))
    {
        smoke["new_library_from_semantic_scene"] = true;
        smokeStage = 204;
        dialog(Monstey_Load, project);
    }
    else if (smokeStage == 204 && clipReady && animationView.at("source") == J(Monstey_JsonPath(initialDatabase)))
    {
        smoke["project_save_reload"] = true;
        smokeStage = 205;
        dialog(Monstey_Build, candidate);
    }
    else if (smokeStage == 205 && clipReady && std::filesystem::is_regular_file(candidate))
    {
        Monstey_Anix readback(Monstey_ReadFile(candidate));
        const auto buildMetadata = readback.Describe();
        Monstey_Require(buildMetadata.at("clip_count").as_uint() == smoke.at("original_clip_count").as_uint() + 1 &&
                            buildMetadata.at("directory_footer_consistent").as_bool(),
                        "Native ANIX UI build readback differs");
        smoke["native_build_readback"] = buildMetadata;
        smoke["candidate_sha256"] = Monstey_HashSha256(candidate);
        smokeStage = 206;
        dialog(Monstey_Open, candidate);
    }
    else if (smokeStage == 206 && clipReady && animationView.at("source") == J(Monstey_JsonPath(candidate)))
    {
        Monstey_Require(find("UI_Renamed") >= 0 && find("UI_Imported") < 0 && find("UI_Held") < 0,
                        "Native ANIX reopened directory does not reflect UI edits");
        smoke["built_anix_reopened"] = true;
        smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
        smoke["native_cpp"] = true;
        smoke["runtime_playback_verified"] = false;
        SmokeComplete();
    }
}

} // namespace Monstey::Monstey_AppInternal
