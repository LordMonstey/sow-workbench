#pragma once
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
struct Monstey_InstGeometryData {
    std::array<std::uint32_t, 14> header{};
    std::vector<Monstey_Bytes> primary, secondary;
    Monstey_Bytes vertex_buffer, index_buffer;
    std::vector<std::vector<std::array<std::uint8_t, 4>>> layouts;
    std::vector<std::array<std::uint32_t, 13>> draws;
    std::vector<std::string> material_names;
};
Monstey_InstGeometryData Monstey_InstReadGeometry(std::span<const std::uint8_t> raw);
Monstey_Bytes Monstey_InstBuildRigid(std::span<const std::uint8_t> template_bytes,
                                     std::string_view resource_path, std::string_view material_path,
                                     std::span<const std::uint8_t> geometry_bytes);
// Array of decoded parts: native cm POSITION3, UVn, directions, raw COLORn bytes,
// INDICES triangle matrices, material_index and preserved draw_parameter_words.
Monstey_Json Monstey_InstDecodeGeometry(std::span<const std::uint8_t> raw);
} // namespace Monstey
