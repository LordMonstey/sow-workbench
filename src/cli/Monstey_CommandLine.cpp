#include "Monstey_CommandLine.hpp"
#include "Monstey_CliExecution.hpp"
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <utility>

namespace Monstey
{
int Monstey_RunCommandLine(int argc, wchar_t **argv)
{
    using namespace Monstey_CliDetail;
    using J = Monstey_Json;
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
    try
    {
        auto options = Monstey_ParseCliOptions(argc, argv);
        if (options.help)
        {
            Monstey_PrintCliHelp();
            return 0;
        }
        Monstey_Runtime runtime(options.schema, options.settings);
        Monstey_OpenCliSessions(runtime, options);
        if (options.http)
            return Monstey_RunCliHttp(runtime, std::move(options.httpSettings));
        if (options.catalog)
            return Monstey_PrintCliCatalog();
        if (!options.request.empty())
            return Monstey_RunCliRequest(runtime, options.request);
        return Monstey_RunCliStdio(runtime, options.mcp);
    }
    catch (const std::exception &error)
    {
        std::cerr << J::object({{"ok", false}, {"error", error.what()}}).dump() << '\n';
        return 1;
    }
}
} // namespace Monstey
