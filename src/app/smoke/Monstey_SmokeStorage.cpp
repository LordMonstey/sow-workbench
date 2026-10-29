#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::PrepareStorageSmoke()
{
    const auto original = smokeDirectory / (smokeArchive ? L"fixture.arch06" : L"fixture.strdb");
    if (smokeArchive)
    {
        const Monstey_Bytes first{0, 1, 2, 3, 254, 255}, second{9, 8, 7}, replacement{3, 0, 4, 0, 255};
        Monstey_WriteNewFile(smokeDirectory / L"first.bin", first);
        Monstey_WriteNewFile(smokeDirectory / L"second.bin", second);
        Monstey_WriteNewFile(smokeDirectory / L"replacement.bin", replacement);
        Monstey_ArchiveProject fixture;
        fixture.AddFile("a/shared.bin", smokeDirectory / L"first.bin");
        fixture.AddFile("z/shared.bin", smokeDirectory / L"second.bin");
        fixture.Build(original, false);
        smoke["payload_hashes"] = J::object();
        for (const auto name : {L"first.bin", L"second.bin", L"replacement.bin"})
            smoke["payload_hashes"][Monstey_Utf8(name)] = Monstey_HashSha256(smokeDirectory / name);
    }
    else
    {
        Monstey_Bytes pool;
        std::map<std::uint32_t, std::u16string> texts{
            {1, u"Original one"}, {2, u"Original two"}, {Monstey_PathHash("Monstey_UI_Original"), u"Original named"}};
        Monstey_Bytes index;
        for (const auto &[hash, text] : texts)
        {
            Monstey_AppendU32(index, hash);
            Monstey_AppendU32(index, static_cast<std::uint32_t>(pool.size() / 2));
            for (const auto unit : text)
                Monstey_AppendU16(pool, static_cast<std::uint16_t>(unit));
            Monstey_AppendU16(pool, 0);
        }
        Monstey_Bytes fixture{'S', 'K', 'D', 'B'};
        for (const std::uint32_t word : {9u, 28u, 3u, 0u, static_cast<std::uint32_t>(pool.size() / 2), 0u})
            Monstey_AppendU32(fixture, word);
        fixture.insert(fixture.end(), index.begin(), index.end());
        fixture.insert(fixture.end(), pool.begin(), pool.end());
        Monstey_WriteNewFile(original, fixture);
        smoke["annotation_hash"] = Monstey_Hex(Monstey_PathHash("Monstey_UI_Original"));
    }
    initialDatabase = original;
    smoke["source_sha256"] = Monstey_HashSha256(original);
    smoke["synthetic_only"] = true;
    smoke["toolbar_gestures"] = J::array();
}

void Monstey_App::SmokeStorageStep()
{
    if (smokeStorageBusy)
        return;
    smokeStorageBusy = true;
    struct Monstey_ResetBusy
    {
        bool &value;
        ~Monstey_ResetBusy()
        {
            value = false;
        }
    } reset{smokeStorageBusy};
    bool idle;
    {
        std::lock_guard lock(mutex);
        idle = !active.load() && jobs.empty() && results.empty();
    }
    if (!idle)
        return;
    const auto ready = editor && editor->window && !editor->pending;
    const auto key = smokeArchive ? "path" : "hash";
    const auto find = [&](const std::string &identity) -> int
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].value(key, "") == J(identity))
                return static_cast<int>(i);
        return -1;
    };
    const auto click = [&](int id)
    {
        const auto button = GetDlgItem(window, id);
        Monstey_Require((GetWindowLongPtrW(button, GWL_STYLE) & WS_VISIBLE) && IsWindowEnabled(button),
                        "Storage toolbar action is unavailable: " + std::to_string(id));
        smoke["toolbar_gestures"].as_array().push_back(id);
        SendMessageW(button, BM_CLICK, 0, 0);
    };
    const auto choose = [&](const std::string &identity, bool doubleClick = false)
    {
        const auto row = find(identity);
        Monstey_Require(row >= 0, "Storage UI row is missing: " + identity);
        ListView_SetItemState(records, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        Monstey_ListSelect(records, row);
        if (doubleClick)
        {
            NMITEMACTIVATE activation{};
            activation.hdr = {records, Monstey_Records, NM_DBLCLK};
            activation.iItem = row;
            SendMessageW(window, WM_NOTIFY, Monstey_Records, reinterpret_cast<LPARAM>(&activation));
        }
        Monstey_Require(selector == identity, "Storage selected identity differs from selected row");
    };
    const auto enter = [&](const wchar_t *text)
    {
        Monstey_Require(ready && editor->input, "Storage text editor did not open");
        SetWindowTextW(editor->input, text);
        SendMessageW(editor->window, WM_COMMAND, Monstey_EditorApply, 0);
    };
    const auto dialog = [&](int id, const std::filesystem::path &path)
    {
        Monstey_SmokeDialogPaths.push_back(path);
        click(id);
        Monstey_Require(Monstey_SmokeDialogPaths.empty(), "Storage action did not open its file dialog");
    };
    const auto project = smokeDirectory / (smokeArchive ? L"edited.msap" : L"edited.project.json");
    const auto candidate = smokeDirectory / (smokeArchive ? L"candidate.arch06" : L"candidate.strdb");
    if (smokeStage >= 200)
    {
        const auto standalone = smokeDirectory / (smokeArchive ? L"standalone.msap" : L"standalone.json");
        const auto paged = smokeDirectory / (smokeArchive ? L"paged.arch06" : L"paged.strdb");
        const auto standaloneId = smokeArchive ? "standalone-folder" : "0x00000007";
        if (smokeStage == 200 && rowData.empty() && !ready)
        {
            smokeStage = 201;
            click(smokeArchive ? Monstey_Folder : Monstey_Add);
        }
        else if (smokeStage == 201 && ready)
        {
            smokeStage = smokeArchive ? 203 : 202;
            enter(smokeArchive ? L"standalone-folder" : L"0x00000007");
        }
        else if (smokeStage == 202 && ready)
        {
            smokeStage = 203;
            enter(L"Standalone — Ω");
        }
        else if (smokeStage == 203 && !ready && find(standaloneId) >= 0)
        {
            if (smokeArchive)
                Monstey_Require(Monstey_WindowText(pageLabel).find(L"0 - 0 of 0") != std::wstring::npos,
                                "Folder-only archive incorrectly counts folders as files");
            smokeStage = 204;
            dialog(Monstey_Save, standalone);
        }
        else if (smokeStage == 204 && std::filesystem::is_regular_file(standalone))
        {
            smokeStage = 205;
            dialog(Monstey_Open, candidate);
        }
        else if (smokeStage == 205 && sourceNames[tab] == candidate.wstring())
        {
            smokeStage = 206;
            dialog(Monstey_Load, standalone);
            Monstey_Require(rowData.empty(), "Project replacement left stale rows actionable");
        }
        else if (smokeStage == 206 && !ready && find(standaloneId) >= 0)
        {
            Monstey_Require(sourceNames[tab] == (smokeArchive ? L"Untitled archive" : L"Untitled strings"),
                            "Source-less project retained the previous source title");
            smoke["standalone_project_reloaded_with_correct_title"] = true;
            if (smokeArchive)
            {
                auto projectCheck = Monstey_ArchiveProject::Load(standalone);
                Monstey_Require(!projectCheck.Source().has_value() && projectCheck.Entries().empty(),
                                "Source-less archive project readback differs");
                Monstey_ArchiveProject fixture;
                for (int i = 0; i < 260; ++i)
                {
                    auto digits = std::to_string(i);
                    fixture.AddFile("paged/" + std::string(4 - digits.size(), '0') + digits + ".bin",
                                    smokeDirectory / L"first.bin");
                }
                fixture.Build(paged, false);
            }
            else
            {
                auto projectCheck = Monstey_StringsProject::LoadProject(standalone);
                Monstey_Require(projectCheck->Describe().at("source").is_null() &&
                                    projectCheck->Entries().at("rows")[0].at("text") ==
                                        J(Monstey_Utf8(L"Standalone — Ω")),
                                "Source-less Strings project readback differs");
                Monstey_StringsProject fixture;
                for (int i = 1; i <= 260; ++i)
                    fixture.Set(i, "Row " + std::to_string(i));
                fixture.Build(paged);
            }
            smoke["pagination_source_sha256"] = Monstey_HashSha256(paged);
            smokeStage = 207;
            dialog(Monstey_Open, paged);
        }
        else if (smokeStage == 207 && total == 260 && offset == 0)
        {
            Monstey_Require(IsWindowEnabled(GetDlgItem(window, Monstey_Next)),
                            "Storage next-page button is disabled for 260 files/strings");
            smokeStage = 208;
            click(Monstey_Next);
        }
        else if (smokeStage == 208 && offset == 250 && rowData.size() == 10)
        {
            choose(smokeArchive ? "paged/0259.bin" : Monstey_Hex(260));
            smokeStage = 209;
            click(Monstey_Delete);
        }
        else if (smokeStage == 209 && total == 259 && rowData.size() == 9)
        {
            smokeStage = 210;
            click(Monstey_Undo);
        }
        else if (smokeStage == 210 && total == 260 && rowData.size() == 10)
        {
            smokeStage = 211;
            click(Monstey_Redo);
        }
        else if (smokeStage == 211 && total == 259 && rowData.size() == 9)
        {
            SetWindowTextW(query, smokeArchive ? L"paged" : L"Row");
            smokeStage = 212;
            click(Monstey_Search);
        }
        else if (smokeStage == 212 && total == 259 && offset == 0)
        {
            smokeStage = 213;
            click(Monstey_Next);
        }
        else if (smokeStage == 213 && offset == 250 && rowData.size() == 9)
        {
            SetWindowTextW(query, L"not-a-match");
            smokeStage = 214;
            click(Monstey_New);
            Monstey_Require(rowData.empty() && selector.empty() && offset == 0 && Monstey_WindowText(query).empty(),
                            "New project retained stale page, filter or selected row");
        }
        else if (smokeStage == 214 && total == 0 && rowData.empty() && !ready)
        {
            Monstey_Require(Monstey_HashSha256(paged) == smoke.at("pagination_source_sha256").as_string() &&
                                Monstey_HashSha256(candidate) == smoke.at("candidate_sha256").as_string(),
                            "Pagination or reopened native source was modified");
            smoke["pagination_260_last_page_delete_undo_redo_filter"] = true;
            smoke["new_resets_page_filter_and_stale_selection"] = true;
            smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
            SmokeComplete();
        }
        return;
    }
    if (smokeStage == 190 && ready)
    {
        smokeStage = 191;
        enter(smokeArchive ? L"z/renamed.bin" : L"0x00000002");
        return;
    }
    if (smokeStage == 191 && !ready)
    {
        Monstey_Require(selector == (smokeArchive ? "z/renamed.bin" : "0x00000002"),
                        "No-op rename selected an unrelated previous history event");
        smoke["project_reload_noop_rename"] = true;
        smokeStage = 21;
        dialog(Monstey_Build, candidate);
        return;
    }
    if (smokeStage == 1 && !ready && rowData.empty())
    {
        Monstey_Require(!sourceNames[tab].empty(), "New storage project has no visible title");
        smoke["new_empty_project"] = true;
        smokeStage = 2;
        dialog(Monstey_Open, initialDatabase);
        return;
    }
    if (smokeArchive)
    {
        if (smokeStage == 2 && find("z/shared.bin") >= 0)
        {
            choose("z/shared.bin", true);
            smoke["nonfirst_same_basename_selected"] = true;
            smokeStage = 3;
            click(Monstey_Rename);
        }
        else if (smokeStage == 3 && ready)
        {
            smokeStage = 4;
            enter(L"z/renamed.bin");
        }
        else if (smokeStage == 4 && find("z/renamed.bin") >= 0 && !ready)
        {
            Monstey_Require(selector == "z/renamed.bin" && find("a/shared.bin") >= 0 && find("z/shared.bin") < 0,
                            "Archive rename changed or selected the wrong file");
            smoke["rename_retains_virtual_selection"] = true;
            smokeStage = 5;
            dialog(Monstey_Replace, smokeDirectory / L"replacement.bin");
        }
        else if (smokeStage == 5 && !ready)
        {
            Monstey_Require(selector == "z/renamed.bin", "Archive replacement reset selection");
            smokeStage = 6;
            dialog(Monstey_Extract, smokeDirectory / L"extracted.bin");
        }
        else if (smokeStage == 6 && std::filesystem::is_regular_file(smokeDirectory / L"extracted.bin"))
        {
            Monstey_Require(Monstey_HashSha256(smokeDirectory / L"extracted.bin") ==
                                smoke.at("payload_hashes").at("replacement.bin").as_string(),
                            "Extracted effective replacement payload differs");
            smoke["effective_overlay_extract"] = true;
            smokeStage = 7;
            dialog(Monstey_Add, smokeDirectory / L"first.bin");
        }
        else if (smokeStage == 7 && ready)
        {
            smokeStage = 8;
            enter(L"added.bin");
        }
        else if (smokeStage == 8 && find("added.bin") >= 0 && !ready)
        {
            Monstey_Require(selector == "added.bin", "Archive Add did not select new virtual file");
            smokeStage = 9;
            click(Monstey_Delete);
        }
        else if (smokeStage == 9 && find("added.bin") < 0 && !ready)
        {
            smokeStage = 10;
            click(Monstey_Undo);
        }
        else if (smokeStage == 10 && find("added.bin") >= 0)
        {
            smokeStage = 11;
            click(Monstey_Redo);
        }
        else if (smokeStage == 11 && find("added.bin") < 0)
        {
            smoke["remove_undo_redo"] = true;
            smokeStage = 12;
            click(Monstey_Folder);
        }
        else if (smokeStage == 12 && ready)
        {
            smokeStage = 13;
            enter(L"empty-folder");
        }
        else if (smokeStage == 13 && find("empty-folder") >= 0 && !ready)
        {
            choose("empty-folder");
            Monstey_Require(!IsWindowEnabled(GetDlgItem(window, Monstey_Replace)) &&
                                !IsWindowEnabled(GetDlgItem(window, Monstey_Extract)),
                            "Archive folder incorrectly exposes file payload actions");
            smokeStage = 14;
            click(Monstey_Rename);
        }
        else if (smokeStage == 14 && ready)
        {
            smokeStage = 15;
            enter(L"empty-renamed");
        }
        else if (smokeStage == 15 && find("empty-renamed") >= 0 && !ready)
        {
            Monstey_Require(selector == "empty-renamed", "Folder rename lost selection");
            smokeStage = 16;
            click(Monstey_Delete);
        }
        else if (smokeStage == 16 && find("empty-renamed") < 0)
        {
            smokeStage = 17;
            click(Monstey_Undo);
        }
        else if (smokeStage == 17 && find("empty-renamed") >= 0)
        {
            smoke["empty_folder_create_rename_remove_undo"] = true;
            smokeStage = 18;
            dialog(Monstey_Save, project);
        }
        else if (smokeStage == 18 && std::filesystem::is_regular_file(project))
        {
            const auto bytes = Monstey_ReadFile(project);
            Monstey_Require(bytes.size() > 4 && std::string(bytes.begin(), bytes.begin() + 4) == "MSAP",
                            "Archive project is not native MSAP");
            smokeStage = 19;
            click(Monstey_New);
        }
        else if (smokeStage == 19 && rowData.empty())
        {
            smokeStage = 20;
            dialog(Monstey_Load, project);
        }
        else if (smokeStage == 20 && find("z/renamed.bin") >= 0 && find("empty-renamed") >= 0)
        {
            smoke["project_saved_and_reloaded"] = true;
            choose("z/renamed.bin");
            smokeStage = 190;
            click(Monstey_Rename);
        }
        else if (smokeStage == 21 && std::filesystem::is_regular_file(candidate))
        {
            Monstey_Archive built(candidate);
            Monstey_Require(
                built.Version() == 4 && built.Read("a/shared.bin") == Monstey_ReadFile(smokeDirectory / L"first.bin") &&
                    built.Read("z/renamed.bin") == Monstey_ReadFile(smokeDirectory / L"replacement.bin") &&
                    !built.Find("added.bin") && !built.Find("z/shared.bin") &&
                    std::find(built.Folders().begin(), built.Folders().end(), "empty-renamed") != built.Folders().end(),
                "Native archive independent payload/folder readback failed");
            smoke["patchlist_paths"] = J::array();
            for (const auto &path : Monstey_ReadArchivePatchList(built.Read("archivepatchloosefiles.txt")))
                smoke["patchlist_paths"].as_array().push_back(path);
            smoke["native_readback_verified"] = true;
            smoke["candidate_sha256"] = Monstey_HashSha256(candidate);
            smokeStage = 22;
            dialog(Monstey_Open, candidate);
        }
        else if (smokeStage == 22 && sourceNames[1] == candidate.wstring() && find("empty-renamed") >= 0)
        {
            for (const auto &[name, hash] : smoke.at("payload_hashes").as_object())
                Monstey_Require(Monstey_HashSha256(smokeDirectory / Monstey_Wide(name)) == hash.as_string(),
                                "Synthetic archive source payload changed");
            smoke["all_payload_sources_unchanged"] = true;
            smoke["built_archive_reopened_in_ui"] = true;
            smokeStage = 200;
            click(Monstey_New);
        }
        return;
    }
    const auto unicode = Monstey_Utf8(L"  Épée Ω 🗡\r\nSecond line — fin  ");
    if (smokeStage == 2 && find("0x00000002") >= 0)
    {
        choose(smoke.at("annotation_hash").as_string());
        smokeStage = 100;
        click(Monstey_Rename);
    }
    else if (smokeStage == 100 && ready)
    {
        smokeStage = 101;
        enter(L"Monstey_UI_Original");
    }
    else if (smokeStage == 101 && !ready)
    {
        Monstey_Require(dirty[2] && IsWindowEnabled(GetDlgItem(window, Monstey_Undo)),
                        "Annotation-only Strings edit did not mark project modified");
        smoke["annotation_only_dirty_and_history"] = true;
        smokeStage = 102;
        click(Monstey_Undo);
    }
    else if (smokeStage == 102 && !dirty[2] && !ready)
    {
        choose("0x00000002", true);
        smokeStage = 3;
    }
    else if (smokeStage == 3 && ready)
    {
        smokeStage = 4;
        enter(Monstey_Wide(unicode).c_str());
    }
    else if (smokeStage == 4 && !ready && find("0x00000002") >= 0 &&
             rowData[static_cast<std::size_t>(find("0x00000002"))].at("text") == J(unicode))
    {
        Monstey_Require(selector == "0x00000002" && dirty[2] && IsWindowEnabled(GetDlgItem(window, Monstey_Undo)) &&
                            rowData[static_cast<std::size_t>(find("0x00000001"))].at("text") == J("Original one"),
                        "Strings double-click changed wrong row or failed to update history");
        smoke["unicode_double_click_dirty_selection"] = true;
        smokeStage = 5;
        click(Monstey_Undo);
    }
    else if (smokeStage == 5 && !dirty[2])
    {
        smokeStage = 6;
        click(Monstey_Redo);
    }
    else if (smokeStage == 6 && dirty[2])
    {
        smoke["unicode_undo_redo"] = true;
        smokeStage = 7;
        click(Monstey_Add);
    }
    else if (smokeStage == 7 && ready)
    {
        smokeStage = 8;
        enter(L"Monstey_UI_Added");
    }
    else if (smokeStage == 8 && ready)
    {
        smokeStage = 9;
        enter(L"Ajouté — Ω 🗡");
    }
    else if (smokeStage == 9 && !ready && find(Monstey_Hex(Monstey_PathHash("Monstey_UI_Added"))) >= 0)
    {
        Monstey_Require(selector == Monstey_Hex(Monstey_PathHash("Monstey_UI_Added")),
                        "Symbolic string add did not select its native hash");
        smoke["symbolic_key_hash"] = selector;
        smokeStage = 10;
        click(Monstey_Rename);
    }
    else if (smokeStage == 10 && ready)
    {
        smokeStage = 11;
        enter(L"0x00000003");
    }
    else if (smokeStage == 11 && !ready && find("0x00000003") >= 0)
    {
        Monstey_Require(selector == "0x00000003", "Strings rename lost identity after hash reorder");
        smoke["rename_reorders_and_retains_selection"] = true;
        smokeStage = 12;
        click(Monstey_Add);
    }
    else if (smokeStage == 12 && ready)
    {
        smokeStage = 13;
        enter(L"0x00000000");
    }
    else if (smokeStage == 13 && ready)
    {
        smokeStage = 14;
        enter(L"");
    }
    else if (smokeStage == 14 && !ready && find("0x00000000") >= 0)
    {
        Monstey_Require(rowData[static_cast<std::size_t>(find("0x00000000"))].at("text") == J(""),
                        "Explicit empty Unicode text was not retained");
        choose("0x00000003");
        smoke["empty_text_and_zero_hash"] = true;
        smokeStage = 15;
        click(Monstey_Delete);
    }
    else if (smokeStage == 15 && find("0x00000003") < 0)
    {
        smokeStage = 16;
        click(Monstey_Undo);
    }
    else if (smokeStage == 16 && find("0x00000003") >= 0)
    {
        smokeStage = 17;
        click(Monstey_Redo);
    }
    else if (smokeStage == 17 && find("0x00000003") < 0)
    {
        smoke["delete_undo_redo"] = true;
        smokeStage = 18;
        dialog(Monstey_Save, project);
    }
    else if (smokeStage == 18 && std::filesystem::is_regular_file(project))
    {
        smokeStage = 19;
        click(Monstey_New);
    }
    else if (smokeStage == 19 && rowData.empty())
    {
        smokeStage = 20;
        dialog(Monstey_Load, project);
    }
    else if (smokeStage == 20 && find("0x00000000") >= 0 && find("0x00000002") >= 0)
    {
        smoke["project_saved_and_reloaded"] = true;
        choose("0x00000002");
        smokeStage = 190;
        click(Monstey_Rename);
    }
    else if (smokeStage == 21 && std::filesystem::is_regular_file(candidate))
    {
        Monstey_StringsProject check(candidate);
        const auto rows = check.Entries().at("rows").as_array();
        Monstey_Require(rows.size() == 4, "Native SKDB readback entry count differs");
        for (const auto &row : rows)
        {
            const auto hash = row.at("hash").as_string();
            if (hash == "0x00000000")
                Monstey_Require(row.at("text") == J(""), "Native SKDB empty text differs");
            if (hash == "0x00000001")
                Monstey_Require(row.at("text") == J("Original one"), "Unselected native string changed");
            if (hash == "0x00000002")
                Monstey_Require(row.at("text") == J(unicode), "Native SKDB Unicode code units differ");
        }
        const auto raw = Monstey_ReadFile(candidate);
        Monstey_Require(raw.size() >= 28 && Monstey_U32(raw, 4) == 9, "Native SKDB version differs");
        smoke["native_readback_verified"] = true;
        smoke["candidate_sha256"] = Monstey_HashSha256(candidate);
        smokeStage = 22;
        dialog(Monstey_Open, candidate);
    }
    else if (smokeStage == 22 && sourceNames[2] == candidate.wstring() && rowData.size() == 4)
    {
        smoke["built_skdb_reopened_in_ui"] = true;
        smokeStage = 200;
        click(Monstey_New);
    }
}

} // namespace Monstey::Monstey_AppInternal
