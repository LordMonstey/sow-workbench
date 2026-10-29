#pragma once
#include "Monstey_Model.hpp"
#include <optional>
namespace Monstey
{
struct Monstey_StaticPrefabResult
{
    Monstey_Bytes bytes;
    Monstey_Json proof;
};
Monstey_StaticPrefabResult Monstey_StaticPrefabBuild(
    std::span<const std::uint8_t> source, std::size_t container, std::string_view resource_path,
    std::string_view material_path, std::span<const std::uint8_t> native_template,
    const std::optional<std::vector<std::size_t>> &selected_parts = std::nullopt);
Monstey_Json Monstey_StaticMaterialInspect(std::span<const std::uint8_t> material);
Monstey_Json Monstey_StaticMaterialShader(std::span<const std::uint8_t> material, std::span<const std::uint8_t> shader);
Monstey_Json Monstey_StaticPrefabConvert(const Monstey_Json &arguments);
} // namespace Monstey
