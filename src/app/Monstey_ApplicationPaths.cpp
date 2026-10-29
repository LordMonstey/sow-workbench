#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

std::filesystem::path Monstey_ExecutableDirectory()
{
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    Monstey_Require(length && length < path.size(), "Cannot resolve executable path");
    return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
}

std::filesystem::path Monstey_DefaultSchema()
{
    auto root = Monstey_ExecutableDirectory();
    for (int i = 0; i < 8 && !root.empty(); ++i)
    {
        for (const auto &candidate : {root / L"schema/gadb-schema.json", root / L"runtime/schema/gadb-schema.json",
                                      root / L"sow-port/schema/gadb-schema.json"})
            if (std::filesystem::is_regular_file(candidate))
                return candidate;
        auto next = root.parent_path();
        if (next == root)
            break;
        root = next;
    }
    return {};
}

std::filesystem::path Monstey_DefaultFxSchema()
{
    for (auto root : {Monstey_ExecutableDirectory(), std::filesystem::current_path()})
        for (int i = 0; i < 8 && !root.empty(); ++i)
        {
            for (const auto &candidate : {root / L"schema/fx-schema.json", root / L"runtime/schema/fx-schema.json",
                                          root / L"sow-port/fear-sdk/native-v001/schema/fx-schema.json"})
                if (std::filesystem::is_regular_file(Monstey_WindowsFilePath(candidate)))
                    return candidate;
            auto next = root.parent_path();
            if (next == root)
                break;
            root = next;
        }
    return {};
}

std::string Monstey_JsonPath(const std::filesystem::path &path)
{
    return Monstey_Utf8(std::filesystem::absolute(path).wstring());
}

} // namespace Monstey::Monstey_AppInternal
