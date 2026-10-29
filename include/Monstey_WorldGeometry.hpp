#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
Monstey_Json Monstey_WorldMeshDecodeGeometry(std::span<const std::uint8_t> source,
                                            std::size_t group, std::size_t draw);
Monstey_Json Monstey_InstSceneInstances(std::span<const std::uint8_t> source);
} // namespace Monstey