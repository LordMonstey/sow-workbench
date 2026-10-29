#pragma once
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
struct Monstey_WorldRawMeshContext {
    Monstey_Bytes vertex_buffer, index_buffer;
    std::vector<std::vector<std::array<std::uint8_t, 4>>> layouts;
    std::vector<std::size_t> layout_strides;
    std::vector<std::array<std::uint32_t, 13>> draws;
};
struct Monstey_WorldRawMesh {
    std::array<std::uint32_t, 8> header{};
    Monstey_Bytes opaque_header;
    std::vector<Monstey_WorldRawMeshContext> groups;
};
Monstey_WorldRawMesh Monstey_WorldMeshReadContexts(std::span<const std::uint8_t> source);
// Material paths are the complete ordered slot table for the selected native context.
struct Monstey_WorldMergeSource {
    std::string id, resource_path, expected_sha256;
    Monstey_Bytes mesh;
    std::size_t context = 0;
    std::vector<std::string> material_paths;
};
struct Monstey_WorldMergeResult {
    Monstey_Bytes mesh;
    std::vector<std::string> material_paths;
    Monstey_Json report;
};
// Sources are immutable snapshots with mandatory SHA256 preconditions. This does not author WDSC.
Monstey_WorldMergeResult Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource> sources);

} // namespace Monstey