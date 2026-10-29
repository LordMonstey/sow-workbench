#include "Monstey_CliExecution.hpp"
#include <Windows.h>
#include <atomic>
#include <iostream>
#include <utility>

namespace Monstey::Monstey_CliDetail
{
namespace
{
std::atomic<bool> Monstey_HttpStopRequested{};
BOOL WINAPI Monstey_ConsoleControl(DWORD event)
{
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT ||
        event == CTRL_SHUTDOWN_EVENT)
    {
        Monstey_HttpStopRequested.store(true);
        return TRUE;
    }
    return FALSE;
}
} // namespace
int Monstey_RunCliHttp(Monstey_Runtime &runtime, Monstey_HttpOptions options)
{
    Monstey_HttpServer server(runtime, std::move(options));
    const BOOL handler = SetConsoleCtrlHandler(Monstey_ConsoleControl, TRUE);
    std::cout << server.BaseUrl() << '\n' << std::flush;
    while (!Monstey_HttpStopRequested.load() && server.Running())
        Sleep(50);
    server.Stop();
    server.Wait();
    if (handler)
        SetConsoleCtrlHandler(Monstey_ConsoleControl, FALSE);
    return 0;
}
} // namespace Monstey::Monstey_CliDetail
