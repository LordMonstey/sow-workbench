#pragma once
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
struct Monstey_WorldSceneGeometry {
    std::array<std::uint32_t, 5> words{};
    Monstey_Bytes vertices, indices, extras;
};
struct Monstey_WorldSceneNode {
    std::array<std::uint32_t, 15> words{};
    Monstey_Bytes planes;
    std::vector<std::uint32_t> geometry, children;
};
struct Monstey_WorldSceneBspNode {
    std::array<std::uint32_t, 5> words{};
    std::vector<std::uint32_t> nodes;
};
struct Monstey_WorldScene {
    std::array<std::uint32_t, 13> header{};
    std::vector<Monstey_WorldSceneGeometry> geometry;
    std::vector<Monstey_WorldSceneNode> nodes;
    std::vector<Monstey_WorldSceneBspNode> bsp_nodes;
    static Monstey_WorldScene Read(std::span<const std::uint8_t> bytes, std::uint32_t groupCount);
    void Validate(std::uint32_t groupCount) const;
    Monstey_Bytes Encode(std::uint32_t groupCount) const;
    Monstey_Json Inspect(std::uint32_t groupCount) const;
};
// Native qualified single-group routing cloned from a verified root/leaf/connection prototype.
// Opaque words, node identifiers and flags are retained from the donor, never synthesized.
Monstey_WorldScene Monstey_WorldSceneRouteSingleGroup(const Monstey_WorldScene &donor,
                                                      const std::array<float, 6> &bounds);
} // namespace Monstey