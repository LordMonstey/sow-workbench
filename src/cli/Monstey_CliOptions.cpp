#include "Monstey_CliOptions.hpp"
#include "Monstey_Core.hpp"
#include <Windows.h>
#include <charconv>

namespace Monstey::Monstey_CliDetail
{
namespace
{
std::filesystem::path Monstey_DefaultSchema()
{
    std::wstring module(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length == 0 || length >= module.size())
        return {};
    module.resize(length);
    const auto candidate = std::filesystem::path(module).parent_path() / L"schema" / L"gadb-schema.json";
    return std::filesystem::exists(candidate) ? candidate : std::filesystem::path{};
}
} // namespace
Monstey_CliOptions Monstey_ParseCliOptions(int argc, wchar_t **argv)
{
    Monstey_CliOptions options{.schema = Monstey_DefaultSchema()};
    options.httpSettings.port = 17902;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring option = argv[i];
        if (option == L"--help")
        {
            options.help = true;
            return options;
        }
        if (option == L"--http")
        {
            options.http = true;
            continue;
        }
        if (option == L"--port" || option == L"--host" || option == L"--allow-origin")
        {
            if (i + 1 >= argc)
                throw Monstey_Error("missing_option_value", "Missing HTTP option value");
            const auto value = Monstey_Utf8(argv[++i]);
            options.httpOption = true;
            if (option == L"--port")
            {
                unsigned port{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), port);
                if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    port > 65535)
                    throw Monstey_Error("invalid_port", "Port must be 0..65535");
                options.httpSettings.port = static_cast<std::uint16_t>(port);
            }
            else if (option == L"--host")
                options.httpSettings.host = value;
            else
                options.httpSettings.origins.push_back(value);
            continue;
        }
        if (option == L"--mcp")
        {
            options.mcp = true;
            continue;
        }
        if (option == L"--catalog")
        {
            options.catalog = true;
            continue;
        }
        if (option != L"--schema" && option != L"--database" && option != L"--archive" && option != L"--strings" &&
            option != L"--request" && option != L"--settings" && option != L"--animation")
            throw Monstey_Error("unknown_option", "Unknown command-line option: " + Monstey_Utf8(option));
        if (i + 1 >= argc)
            throw Monstey_Error("missing_option_value", "Missing command-line path");
        const std::filesystem::path value = argv[++i];
        if (option == L"--settings")
            options.settings = value;
        else if (option == L"--schema")
            options.schema = value;
        else if (option == L"--database")
            options.database = value;
        else if (option == L"--archive")
            options.archive = value;
        else if (option == L"--strings")
            options.strings = value;
        else if (option == L"--animation")
            options.animation = value;
        else
            options.request = value;
    }
    if (options.httpOption && !options.http)
        throw Monstey_Error("conflicting_modes", "HTTP options require --http");
    if (static_cast<int>(options.http) + static_cast<int>(options.mcp) + static_cast<int>(options.catalog) +
            static_cast<int>(!options.request.empty()) >
        1)
        throw Monstey_Error("conflicting_modes", "Select one of --mcp, --http, --catalog or --request");
    return options;
}
} // namespace Monstey::Monstey_CliDetail
