#pragma once
#include "Monstey_Havok.hpp"
namespace Monstey {
struct Monstey_NavigationResult {
  Monstey_Bytes navigation;
  Monstey_Bytes owner;
  Monstey_Json report;
};
Monstey_Json
Monstey_NavigationInspect(std::span<const std::uint8_t> source,
                          std::span<const std::uint8_t> owner = {});
Monstey_NavigationResult
Monstey_NavigationApply(std::span<const std::uint8_t> source,
                        std::span<const std::uint8_t> owner,
                        const Monstey_Json &spec);
Monstey_NavigationResult
Monstey_NavigationBuild(std::span<const std::uint8_t> source,
                        const Monstey_Json &spec,
                        std::span<const std::uint8_t> owner = {},
                        std::span<const std::uint8_t> cache_template = {});
} // namespace Monstey
