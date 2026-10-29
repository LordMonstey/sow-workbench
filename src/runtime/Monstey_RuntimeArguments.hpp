#pragma once
#include "Monstey_Runtime.hpp"
#include <initializer_list>
namespace Monstey
{
namespace Monstey_RuntimeDetail
{
std::string Monstey_Text(const Monstey_Json &arguments, std::string_view key, std::string fallback = {});
std::filesystem::path Monstey_Path(const Monstey_Json &arguments, std::string_view key,
                                   const std::filesystem::path &fallback = {});
Monstey_Json Monstey_SelectArguments(const Monstey_Json &arguments, std::initializer_list<std::string_view> keys);
} // namespace Monstey_RuntimeDetail
struct Monstey_Runtime::Monstey_Arguments
{
    std::size_t offset;
    std::size_t limit;
    std::string query;
    explicit Monstey_Arguments(const Monstey_Json &arguments);
};
} // namespace Monstey
