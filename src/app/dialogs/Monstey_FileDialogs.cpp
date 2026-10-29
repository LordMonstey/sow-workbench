#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

// Only explicit smoke modes queue native dialog selections.
std::deque<std::filesystem::path> Monstey_SmokeDialogPaths;
std::size_t Monstey_SmokeDialogCount = 0;

std::optional<std::filesystem::path> Monstey_DirectoryDialog(HWND parent, const wchar_t *title)
{
    BROWSEINFOW options{};
    options.hwndOwner = parent;
    options.lpszTitle = title;
    options.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    auto *item = SHBrowseForFolderW(&options);
    if (!item)
        return std::nullopt;
    std::vector<wchar_t> path(32768);
    const auto ok = SHGetPathFromIDListEx(item, path.data(), static_cast<DWORD>(path.size()), GPFIDL_DEFAULT);
    CoTaskMemFree(item);
    if (!ok)
        return std::nullopt;
    return std::filesystem::path(path.data());
}

UINT_PTR CALLBACK Monstey_SmokeFileDialogHook(HWND window, UINT message, WPARAM, LPARAM data)
{
    if (message == WM_NOTIFY)
    {
        const auto *notice = reinterpret_cast<const OFNOTIFYW *>(data);
        if (notice->hdr.code == CDN_INITDONE)
        {
            const auto *path = reinterpret_cast<const std::wstring *>(notice->lpOFN->lCustData);
            SendMessageW(GetParent(window), CDM_SETCONTROLTEXT, edt1, reinterpret_cast<LPARAM>(path->c_str()));
            PostMessageW(GetParent(window), WM_COMMAND, IDOK, 0);
        }
    }
    return 0;
}

std::optional<std::filesystem::path> Monstey_FileDialog(HWND parent, bool save, const wchar_t *filter,
                                                        const wchar_t *extension, const wchar_t *title)
{
    std::vector<wchar_t> buffer(32768);
    OPENFILENAMEW file{};
    file.lStructSize = sizeof(file);
    file.hwndOwner = parent;
    file.lpstrFile = buffer.data();
    file.nMaxFile = static_cast<DWORD>(buffer.size());
    file.lpstrFilter = filter;
    file.lpstrDefExt = extension;
    file.lpstrTitle = title;
    file.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    std::wstring smokePath;
    if (!Monstey_SmokeDialogPaths.empty())
    {
        smokePath = Monstey_SmokeDialogPaths.front().wstring();
        std::copy(smokePath.begin(), smokePath.end(), buffer.begin());
        Monstey_SmokeDialogPaths.pop_front();
        ++Monstey_SmokeDialogCount;
        file.Flags |= OFN_ENABLEHOOK;
        file.lpfnHook = Monstey_SmokeFileDialogHook;
        file.lCustData = reinterpret_cast<LPARAM>(&smokePath);
    }
    const bool selected = save ? GetSaveFileNameW(&file) != FALSE : GetOpenFileNameW(&file) != FALSE;
    if (!selected)
    {
        const auto error = CommDlgExtendedError();
        if (error)
            throw Monstey_Error("file_dialog", "Native file dialog failed with code " + std::to_string(error));
        if (!smokePath.empty())
            throw Monstey_Error("file_dialog", "Automated file selection was canceled before returning a path");
        return std::nullopt;
    }
    return std::filesystem::path(buffer.data());
}

const wchar_t *Monstey_SourceFilter(int tab)
{
    switch (tab)
    {
    case 0:
        return L"SoW database (*.gamedb)\0*.gamedb\0All files\0*.*\0\0";
    case 1:
        return L"Native archive (*.arch*)\0*.arch*\0All files\0*.*\0\0";
    case 2:
        return L"String database (*.strdb)\0*.strdb\0All files\0*.*\0\0";
    default:
        return L"Client FX (*.fxp)\0*.fxp\0All files\0*.*\0\0";
    }
}

} // namespace Monstey::Monstey_AppInternal
