#include "Monstey_CliExecution.hpp"
#include <iostream>

namespace Monstey::Monstey_CliDetail
{
int Monstey_PrintCliCatalog()
{
    std::cout << Monstey_Runtime::Catalog().dump(2) << '\n';
    return 0;
}
int Monstey_RunCliRequest(Monstey_Runtime &runtime, const std::filesystem::path &request)
{
    const auto reply = runtime.Reply(Monstey_ReadText(request, 64u * 1024));
    std::cout << reply.dump(2) << '\n';
    return reply.at("ok").as_bool() ? 0 : 2;
}
} // namespace Monstey::Monstey_CliDetail
