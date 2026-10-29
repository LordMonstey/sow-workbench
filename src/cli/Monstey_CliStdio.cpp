#include "Monstey_CliExecution.hpp"
#include "Monstey_Protocol.hpp"
#include <iostream>

namespace Monstey::Monstey_CliDetail
{
namespace
{
bool Monstey_ReadLine(std::string &line, bool &oversized)
{
    line.clear();
    oversized = false;
    char c{};
    bool any = false;
    while (std::cin.get(c))
    {
        any = true;
        if (c == '\n')
            break;
        if (line.size() < 64u * 1024)
            line.push_back(c);
        else
            oversized = true;
    }
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return any;
}
} // namespace
int Monstey_RunCliStdio(Monstey_Runtime &runtime, bool mcp)
{
    using J = Monstey_Json;
    Monstey_Mcp server(runtime);
    std::string line;
    bool oversized = false;
    while (Monstey_ReadLine(line, oversized))
    {
        if (!mcp)
        {
            const auto reply =
                oversized ? J::object({{"ok", false},
                                       {"error", J::object({{"code", "request_too_large"},
                                                            {"message", "JSON requests are limited to 64 KiB"}})}})
                          : runtime.Reply(line);
            std::cout << reply.dump() << '\n' << std::flush;
            continue;
        }
        if (oversized)
        {
            std::cout << Monstey_RpcError(nullptr, -32700, "JSON request exceeds 64 KiB").dump() << '\n' << std::flush;
            continue;
        }
        try
        {
            const auto reply = server.Handle(J::Parse(line));
            if (reply)
                std::cout << reply->dump() << '\n' << std::flush;
        }
        catch (const std::exception &error)
        {
            std::cout << Monstey_RpcError(nullptr, -32700, error.what()).dump() << '\n' << std::flush;
        }
    }
    if (std::cin.bad())
        throw Monstey_Error("stdin_failed", "Failed reading standard input");
    return 0;
}
} // namespace Monstey::Monstey_CliDetail
