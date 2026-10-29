#pragma once
#include "Monstey_Physics.hpp"
namespace Monstey {
struct Monstey_CollisionResult {
  Monstey_Bytes bytes;
  Monstey_Json report;
};
Monstey_CollisionResult
Monstey_CollisionBoxes(std::span<const std::uint8_t> source,
                       const Monstey_Json &spec,
                       std::span<const std::uint8_t> profile);
} // namespace Monstey