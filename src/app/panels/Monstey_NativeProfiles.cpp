#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::NativeProfileLabel()
{
    if (!nativeProfileLabel)
        return;
    const int i = tab == 3 ? 1 : 0;
    EnableWindow(GetDlgItem(window, Monstey_NativeProfileClear), !nativeProfilePath[i].empty());
    std::wstring text = L"No native profile selected. Legacy build.";
    if (!nativeProfilePath[i].empty())
    {
        text =
            nativeProfilePath[i].filename().wstring() + L" | SHA256 " + Monstey_Wide(nativeProfileSha[i].substr(0, 16));
        if (!nativeProfileBuildStatus[i].empty())
        {
            auto state = nativeProfileBuildStatus[i];
            for (auto &c : state)
                if (c >= 'a' && c <= 'z')
                    c = static_cast<char>(c - 'a' + 'A');
            text = L"Last build: " + Monstey_Wide(state) + L" | " + text;
        }
        text += L" | engine not verified";
    }
    SetWindowTextW(nativeProfileLabel, text.c_str());
}

void Monstey_App::NativeProfileCommand(int command)
{
    const int i = tab == 3 ? 1 : 0;
    if (command == Monstey_NativeProfileClear)
    {
        nativeProfilePath[i].clear();
        nativeProfileSha[i].clear();
        nativeProfileBytes[i] = 0;
        nativeProfileBuildStatus[i].clear();
        ++nativeProfileSerial[i];
        NativeProfileLabel();
        ShowReport(J::object({{"native_declaration_profile", nullptr}, {"build_mode", "legacy"}}));
        return;
    }
    const auto chosen =
        Monstey_FileDialog(window, false, L"Native declaration evidence (*.json)\0*.json\0All files\0*.*\0\0", L"json",
                           L"Choose explicit native declaration profile");
    if (!chosen)
        return;
    const auto path = std::filesystem::absolute(*chosen);
    const auto bytes = Monstey_ReadFile(path, 128ull * 1024 * 1024);
    const auto digest = Monstey_HashSha256(bytes);
    // Assign only after the complete read/hash; a failed or cancelled dialog preserves the previous
    // choice.
    nativeProfilePath[i] = path;
    nativeProfileSha[i] = digest;
    nativeProfileBytes[i] = bytes.size();
    nativeProfileBuildStatus[i].clear();
    ++nativeProfileSerial[i];
    NativeProfileLabel();
    ShowReport(J::object({{"native_declaration_profile", J::object({{"path", Monstey_JsonPath(path)},
                                                                    {"sha256", digest},
                                                                    {"bytes", bytes.size()},
                                                                    {"pinned_when_selected", true}})},
                          {"build_policy", "reject proved unsafe layouts; retain UNKNOWN"},
                          {"runtime_verified", false}}));
}

} // namespace Monstey::Monstey_AppInternal
