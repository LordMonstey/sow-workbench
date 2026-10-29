#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeNativeProfileStep()
{
    if (smokeStorageBusy)
        return;
    struct Monstey_ProfileSmokeGuard
    {
        bool &busy;
        ~Monstey_ProfileSmokeGuard()
        {
            busy = false;
        }
    } guard{smokeStorageBusy};
    smokeStorageBusy = true;
    const auto idle = [&]
    {
        std::lock_guard lock(mutex);
        return !active.load() && jobs.empty() && results.empty() && !fxLoading;
    };
    if (!idle())
        return;
    const auto domain = smokeArguments.value("domain", "database").as_string();
    const int expectedTab = domain == "effects" ? 3 : 0, i = expectedTab == 3 ? 1 : 0;
    const auto mode = smokeArguments.value("expect", "qualified").as_string();
    const auto extension = expectedTab == 3 ? L".fxp" : L".gamedb";
    const auto output = smokeDirectory / (std::wstring(L"guarded") + extension);
    const auto legacy = smokeDirectory / (std::wstring(L"legacy") + extension);
    const auto capture = [&](const wchar_t *name)
    {
        RECT r{};
        GetClientRect(window, &r);
        const auto width = static_cast<std::uint32_t>(r.right), height = static_cast<std::uint32_t>(r.bottom);
        Monstey_Require(width && height && width <= 8192 && height <= 8192, "Invalid owned-window capture bounds");
        struct Monstey_CaptureHandles
        {
            HWND window{};
            HDC display{}, memory{};
            HBITMAP bitmap{};
            HGDIOBJ old{};
            ~Monstey_CaptureHandles()
            {
                if (old && old != HGDI_ERROR)
                    SelectObject(memory, old);
                if (bitmap)
                    DeleteObject(bitmap);
                if (memory)
                    DeleteDC(memory);
                if (display)
                    ReleaseDC(window, display);
            }
        } h;
        h.window = window;
        h.display = GetDC(window);
        Monstey_Require(h.display != nullptr, "Cannot acquire owned window DC");
        h.memory = CreateCompatibleDC(h.display);
        Monstey_Require(h.memory != nullptr, "Cannot create capture DC");
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(width);
        info.bmiHeader.biHeight = -static_cast<LONG>(height);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void *pixels = nullptr;
        h.bitmap = CreateDIBSection(h.display, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        Monstey_Require(h.bitmap && pixels, "Cannot allocate owned capture pixels");
        h.old = SelectObject(h.memory, h.bitmap);
        Monstey_Require(h.old && h.old != HGDI_ERROR, "Cannot select capture bitmap");
        SendMessageW(window, WM_PRINT, reinterpret_cast<WPARAM>(h.memory), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
        const auto pixelBytes = static_cast<std::size_t>(width) * height * 4;
        BITMAPFILEHEADER file{};
        file.bfType = 0x4d42;
        file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
        file.bfSize = static_cast<DWORD>(file.bfOffBits + pixelBytes);
        Monstey_Bytes bytes(file.bfSize);
        CopyMemory(bytes.data(), &file, sizeof(file));
        CopyMemory(bytes.data() + sizeof(file), &info.bmiHeader, sizeof(info.bmiHeader));
        CopyMemory(bytes.data() + file.bfOffBits, pixels, pixelBytes);
        const auto path = smokeDirectory / name;
        Monstey_WriteNewFile(path, bytes);
        smoke["captures"][Monstey_Utf8(name)] = Monstey_JsonPath(path);
    };
    if (smokeStage == 1)
    {
        if (sourceNames[expectedTab].empty())
            return;
        Monstey_Require(tab == expectedTab, "Wrong native profile domain");
        if (!rowData.empty())
        {
            Monstey_ListSelect(records, 0);
            ReadRecord(0);
        }
        smokeStage = 2;
        return;
    }
    if (smokeStage == 2)
    {
        const auto original = std::filesystem::path(Monstey_Wide(smokeArguments.at("profile").as_string()));
        const auto copy = smokeDirectory / L"selected-native-profile.json";
        const auto bytes = Monstey_ReadFile(original, 128ull * 1024 * 1024);
        Monstey_WriteNewFile(copy, bytes);
        smoke["original_profile_sha256"] = Monstey_HashSha256(bytes);
        smoke["profile_copy"] = Monstey_JsonPath(copy);
        const auto selected = selector;
        const auto rowCount = ListView_GetItemCount(records);
        const auto fieldCount = ListView_GetItemCount(fields);
        Monstey_Require(!selector.empty() && rowCount >= 2 && fieldCount >= 2,
                        "Profile UI smoke requires real selected records and fields");
        Monstey_SmokeDialogPaths.push_back(copy);
        Command(Monstey_NativeProfileChoose);
        Monstey_Require(nativeProfileSha[i] == smoke.at("original_profile_sha256").as_string() &&
                            nativeProfilePath[i] == copy,
                        "Native chooser did not pin selected bytes");
        Monstey_Require(selector == selected && ListView_GetItemCount(records) == rowCount &&
                            ListView_GetItemCount(fields) == fieldCount,
                        "Profile selection disturbed record properties");
        smoke["selection_preserves_records_and_fields"] = true;
        const auto beforeGeneration = generation, beforeRevision = revision;
        SendMessageW(GetDlgItem(window, Monstey_NativeProfileClear), WM_KEYDOWN, VK_SPACE, 0);
        SendMessageW(GetDlgItem(window, Monstey_NativeProfileClear), WM_KEYUP, VK_SPACE, 0);
        Monstey_Require(nativeProfilePath[i].empty() && nativeProfileSha[i].empty(),
                        "Native Clear button Space key did not clear selection");
        Monstey_Require(generation == beforeGeneration && revision == beforeRevision,
                        "Profile UI changed runtime state");
        smoke["clear_keyboard_and_session_preservation"] = true;
        Monstey_SmokeDialogPaths.push_back(copy);
        Command(Monstey_NativeProfileChoose);
        Monstey_Require(nativeProfileSha[i] == smoke.at("original_profile_sha256").as_string(),
                        "Re-selection pin differs");
        smoke["selected_sha256"] = nativeProfileSha[i];
        smoke["selected_label"] = Monstey_Utf8(Monstey_WindowText(nativeProfileLabel));
        RECT client{};
        GetClientRect(window, &client);
        for (const auto child : {GetDlgItem(window, Monstey_NativeProfileChoose),
                                 GetDlgItem(window, Monstey_NativeProfileClear), nativeProfileLabel})
        {
            RECT bounds{};
            GetWindowRect(child, &bounds);
            MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&bounds), 2);
            Monstey_Require(IsWindowVisible(child) && bounds.left >= 0 && bounds.top >= 0 &&
                                bounds.right <= client.right && bounds.bottom <= client.bottom,
                            "Native profile row outside client area");
        }
        smoke["native_controls_within_client"] = true;
        if (mode == "changed")
        {
            HANDLE file = CreateFileW(Monstey_WindowsFilePath(copy).c_str(), FILE_APPEND_DATA, 0, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Monstey_Require(file != INVALID_HANDLE_VALUE, "Cannot mutate owned stale-profile copy");
            DWORD written = 0;
            const char space = ' ';
            const bool ok = WriteFile(file, &space, 1, &written, nullptr) != FALSE;
            CloseHandle(file);
            Monstey_Require(ok && written == 1, "Cannot append owned stale-profile control");
            smoke["expected_error_code"] = "source_hash_mismatch";
        }
        else if (mode == "unsafe")
            smoke["expected_error_code"] = "gadb_class_cache_unsafe";
        capture(L"selected.bmp");
        const auto Monstey_BlockedBefore = Monstey_nativeProfileBlockedTabChanges;
        const auto Monstey_EpochBefore = epoch;
        SendMessageW(tabs, WM_KEYDOWN, VK_RIGHT, 1);
        Monstey_Require(Monstey_nativeProfileBlockedTabChanges == Monstey_BlockedBefore + 1,
                        "Native profile smoke keyboard navigation did not reach the tab selection guard");
        Monstey_Require(tab == expectedTab && TabCtrl_GetCurSel(tabs) == expectedTab && epoch == Monstey_EpochBefore,
                        "Native profile smoke tab navigation escaped isolation");
        smoke["native_tab_navigation_intercepted"] = true;
        smoke["native_tab_navigation_blocked_count"] = Monstey_nativeProfileBlockedTabChanges;
        smokeStage = 3;
        return;
    }
    if (smokeStage == 3)
    {
        const auto Monstey_SelectedTab = TabCtrl_GetCurSel(tabs);
        Monstey_Require(tab == expectedTab && Monstey_SelectedTab == expectedTab,
                        "Native profile smoke domain changed before Build: expected=" + std::to_string(expectedTab) +
                            ", tab=" + std::to_string(tab) + ", selected=" + std::to_string(Monstey_SelectedTab));
        smokeStage = 4;
        Monstey_SmokeDialogPaths.push_back(output);
        Command(Monstey_Build);
        return;
    }
    if (smokeStage == 4)
    {
        if (!smoke.contains("profile_build_reply"))
            return;
        const auto reply = smoke.at("profile_build_reply");
        Monstey_Require(smoke["profile_build_arguments"]["expected_native_declarations_sha256"] ==
                            smoke["selected_sha256"],
                        "Build silently repinned selected profile");
        Monstey_Require(nativeProfileSha[i] == smoke["selected_sha256"].as_string(), "Stored UI selection pin changed");
        if (mode == "unsafe" || mode == "changed")
        {
            Monstey_Require(!reply["ok"].as_bool() && reply["error"]["code"] == smoke["expected_error_code"] &&
                                smoke.contains("observed_expected_error"),
                            "Wrong native UI guard rejection");
            Monstey_Require(!std::filesystem::exists(output) &&
                                !std::filesystem::exists(output.wstring() + L".namemap.json") &&
                                !std::filesystem::exists(output.wstring() + L".report.json"),
                            "Rejected UI guard published artifacts");
        }
        else
            Monstey_Require(reply["ok"].as_bool() &&
                                reply["result"]["native_class_cache"]["status"].as_string() == mode,
                            "Guard status not routed from UI");
        const auto label = Monstey_WindowText(nativeProfileLabel);
        const auto expected = mode == "unknown" ? L"UNKNOWN" : mode == "qualified" ? L"QUALIFIED" : L"REJECTED";
        Monstey_Require(label.find(expected) != label.npos, "Native layout outcome is not visible");
        smoke["guarded_label"] = Monstey_Utf8(label);
        smoke["guarded_reply"] = reply;
        capture(L"guarded.bmp");
        const auto beforeGeneration = generation, beforeRevision = revision;
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(Monstey_NativeProfileClear, BN_CLICKED),
                     reinterpret_cast<LPARAM>(GetDlgItem(window, Monstey_NativeProfileClear)));
        Monstey_Require(nativeProfilePath[i].empty() && generation == beforeGeneration && revision == beforeRevision,
                        "Clear changed project or failed");
        smoke.as_object().erase("profile_build_reply");
        smokeStage = 5;
        Monstey_SmokeDialogPaths.push_back(legacy);
        Command(Monstey_Build);
        return;
    }
    if (smokeStage == 5)
    {
        if (!smoke.contains("profile_build_reply"))
            return;
        const auto reply = smoke.at("profile_build_reply");
        Monstey_Require(reply["ok"].as_bool() && std::filesystem::is_regular_file(legacy),
                        "Cleared legacy UI build failed");
        Monstey_Require(!reply["result"].contains("native_class_cache") &&
                            !smoke["profile_build_arguments"].contains("native_declarations") &&
                            !smoke["profile_build_arguments"].contains("expected_native_declarations_sha256"),
                        "Cleared profile leaked into legacy route");
        smoke["clear_restores_legacy_build"] = true;
        capture(L"legacy.bmp");
        for (const int modeTab : {1, 2, 4, 5, 6})
        {
            TabCtrl_SetCurSel(tabs, modeTab);
            TabChanged();
            Monstey_Require(!IsWindowVisible(nativeProfileLabel) &&
                                !IsWindowVisible(GetDlgItem(window, Monstey_NativeProfileChoose)),
                            "Profile row leaked into another mode");
        }
        TabCtrl_SetCurSel(tabs, expectedTab);
        TabChanged();
        smoke["other_tabs_keep_profile_row_hidden"] = true;
        smoke["original_profile_unchanged"] =
            Monstey_HashSha256(std::filesystem::path(Monstey_Wide(smokeArguments.at("profile").as_string()))) ==
            smoke["original_profile_sha256"].as_string();
        smoke["common_file_dialogs"] = Monstey_SmokeDialogCount;
        smokeStage = 6;
        return;
    }
    if (smokeStage == 6)
        SmokeComplete();
}

} // namespace Monstey::Monstey_AppInternal
