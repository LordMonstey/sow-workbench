#pragma once
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
// Raw native F32 lanes. Positions are cm; quaternion ordering is XYZW.
struct Monstey_SkelDefaultNode {
    std::size_t hk_index = 0, source_index = 0;
    int parent_hk = -1;
    std::uint32_t name_hash = 0;
    std::uint8_t raw_flag_byte = 0;
    std::string name;
    std::array<std::uint32_t, 7> global_words{}, inverse_words{}, local_words{};
    std::array<std::uint32_t, 8> default_pose_words{}, inverse_pose_words{};
};
struct Monstey_SkelDefaultPose {
    std::string source_sha256;
    std::vector<Monstey_SkelDefaultNode> nodes;
    std::vector<std::size_t> hk_to_source, source_to_hk;
};
// Read-only selected SKEL17 profile: one rooted source-preorder tree, 1..256 nodes.
// No quaternion normalization, animation overlay, retargeting or asset write.
Monstey_SkelDefaultPose Monstey_SkelDefaultPoseRead(std::span<const std::uint8_t> bytes);
}