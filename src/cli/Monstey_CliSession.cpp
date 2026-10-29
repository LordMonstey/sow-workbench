#include "Monstey_CliExecution.hpp"

namespace Monstey::Monstey_CliDetail
{
void Monstey_OpenCliSessions(Monstey_Runtime &runtime, const Monstey_CliOptions &options)
{
    using J = Monstey_Json;
    const auto open = [&](const char *operation, const char *key, const std::filesystem::path &path)
    {
        if (path.empty())
            return;
        const auto reply = runtime.Call(
            J::object({{"operation", operation}, {"arguments", J::object({{key, Monstey_Utf8(path.wstring())}})}}));
        if (!reply.at("ok").as_bool())
            throw Monstey_Error(reply.at("error").at("code").as_string(), reply.at("error").at("message").as_string());
    };
    open("open_database", "path", options.database);
    open("open_archive", "path", options.archive);
    open("strings_new", "source", options.strings);
    open("animation_open", "path", options.animation);
}
} // namespace Monstey::Monstey_CliDetail
