#pragma once
#include "Monstey_Http.hpp"
#include <filesystem>
namespace Monstey::Monstey_CliDetail
{
struct Monstey_CliOptions
{
    std::filesystem::path schema, database, archive, strings, animation, request, settings;
    bool mcp = false, catalog = false, http = false, httpOption = false, help = false;
    Monstey_HttpOptions httpSettings;
};
Monstey_CliOptions Monstey_ParseCliOptions(int argc, wchar_t **argv);
} // namespace Monstey::Monstey_CliDetail
