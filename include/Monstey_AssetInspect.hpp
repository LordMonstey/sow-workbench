#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
Monstey_Json Monstey_MaterialInspect(std::span<const std::uint8_t> bytes);
Monstey_Json Monstey_TextureInspect(std::span<const std::uint8_t> bytes);
Monstey_Json Monstey_AssetInspect(const std::filesystem::path &source);
} // namespace Monstey