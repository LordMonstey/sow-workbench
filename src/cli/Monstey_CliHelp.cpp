#include "Monstey_CliExecution.hpp"
#include <iostream>

namespace Monstey::Monstey_CliDetail
{
void Monstey_PrintCliHelp()
{
    std::cout << "Monstey SoW Native Workbench 0.3.0-dev\n"
                 "Usage: Monstey_WorkbenchCli [--settings FILE] [--schema FILE] "
                 "[--database FILE] "
                 "[--archive FILE]\n"
                 "       [--strings FILE] [--animation FILE] [--request "
                 "JSON_FILE | --catalog | "
                 "--mcp | --http]\n"
                 "Default input/output: one UTF-8 JSON object per line, 64 KiB "
                 "per request.\n"
                 "--mcp serves MCP JSON-RPC on stdio; diagnostics go to stderr.\n"
                 "--http serves REST, MCP /mcp and legacy /sse on loopback.\n"
                 "HTTP options: --port NUMBER --host LOOPBACK --allow-origin "
                 "ORIGIN.\n"
                 "All writes create new offline files. Source files are never "
                 "overwritten.\n";
}
} // namespace Monstey::Monstey_CliDetail
