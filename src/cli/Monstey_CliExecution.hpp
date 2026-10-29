#pragma once
#include "Monstey_CliOptions.hpp"
namespace Monstey::Monstey_CliDetail
{
void Monstey_PrintCliHelp();
void Monstey_OpenCliSessions(Monstey_Runtime &runtime, const Monstey_CliOptions &options);
int Monstey_RunCliHttp(Monstey_Runtime &runtime, Monstey_HttpOptions options);
int Monstey_RunCliStdio(Monstey_Runtime &runtime, bool mcp);
int Monstey_PrintCliCatalog();
int Monstey_RunCliRequest(Monstey_Runtime &runtime, const std::filesystem::path &request);
} // namespace Monstey::Monstey_CliDetail
