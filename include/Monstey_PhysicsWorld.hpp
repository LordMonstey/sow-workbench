#pragma once
#include "Monstey_Physics.hpp"
#include <functional>
namespace Monstey {
using Monstey_PhysicsDonorReader =
    std::function<Monstey_Bytes(const std::filesystem::path &)>;
std::pair<Monstey_Bytes, Monstey_Json>
Monstey_PhysicsWorldApply(std::span<const std::uint8_t> source,
                          const Monstey_Json &spec,
                          const Monstey_PhysicsDonorReader &donor_reader = {},
                          std::size_t offset = 0, std::size_t limit = 250);
Monstey_Json Monstey_PhysicsWorldCall(std::string_view operation,
                                      const Monstey_Json &arguments);
} // namespace Monstey
