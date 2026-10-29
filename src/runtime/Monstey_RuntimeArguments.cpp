#include "Monstey_RuntimeArguments.hpp"

namespace Monstey
{
namespace Monstey_RuntimeDetail
{
std::string Monstey_Text(const Monstey_Json &a, std::string_view key, std::string fallback)
{
    if (!a.contains(key) || a.at(key).is_null())
        return fallback;
    return a.at(key).as_string();
}
std::filesystem::path Monstey_Path(const Monstey_Json &a, std::string_view key, const std::filesystem::path &fallback)
{
    if (!a.contains(key) || a.at(key).is_null())
        return fallback;
    const auto &value = a.at(key).as_string();
    if (value.empty() || value.find('\0') != value.npos)
        throw Monstey_Error("invalid_path", "Path must be nonempty and contain no NUL");
    return std::filesystem::path(Monstey_Wide(value));
}
Monstey_Json Monstey_SelectArguments(const Monstey_Json &a, std::initializer_list<std::string_view> keys)
{
    using J = Monstey_Json;
    auto result = J::object();
    for (const auto key : keys)
        if (a.contains(key))
            result[std::string(key)] = a.at(key);
    return result;
}
} // namespace Monstey_RuntimeDetail
Monstey_Runtime::Monstey_Arguments::Monstey_Arguments(const Monstey_Json &a)
    : offset(static_cast<std::size_t>(a.value("offset", 0).as_uint())),
      limit(static_cast<std::size_t>(a.value("limit", 250).as_uint())),
      query(Monstey_RuntimeDetail::Monstey_Text(a, "query"))
{
}
} // namespace Monstey
