#pragma once
#include "Monstey_Havok.hpp"
namespace Monstey {
struct Monstey_TriangleResult {
  Monstey_Bytes bytes;
  Monstey_Json report;
};
Monstey_TriangleResult Monstey_TriangleCompleteProfile(
    std::span<const std::uint8_t> profile_source, const Monstey_Json &profile,
    std::span<const std::uint8_t> donor_source, const Monstey_Json &donor);
Monstey_TriangleResult Monstey_TriangleCook(
    std::span<const std::uint8_t> profile_source, const Monstey_Json &profile,
    const Monstey_Json &vertices_cm, const Monstey_Json &triangles,
    double max_error_cm = 0.02, const Monstey_Json &tags = Monstey_Json());
Monstey_TriangleResult
Monstey_TriangleCollision(std::span<const std::uint8_t> source,
                          const Monstey_Json &spec,
                          std::span<const std::uint8_t> profile_source);
// Independent native decoders and spatial auditors are shared with geometry RE.
Monstey_Json Monstey_DecodeCompressedMesh(const Monstey_Json &mesh);
Monstey_Json Monstey_ValidateCompressedMesh(const Monstey_Json &mesh);
} // namespace Monstey
