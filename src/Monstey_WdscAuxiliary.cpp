#include "Monstey_WdscAuxiliary.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
namespace Monstey
{
namespace
{
using J = Monstey_Json;
constexpr std::size_t Monstey_MaximumSourceBytes = 64ull * 1024 * 1024;
constexpr std::size_t Monstey_MaximumMetadataRows = 200000;
class Monstey_AuxiliaryReader
{
  public:
    std::span<const std::uint8_t> bytes;
    std::size_t at = 44;
    std::size_t metadata_rows = 0;
    explicit Monstey_AuxiliaryReader(std::span<const std::uint8_t> source) : bytes(source)
    {
    }
    std::span<const std::uint8_t> Take(std::size_t count)
    {
        Monstey_CheckRange(bytes, at, count);
        auto result = bytes.subspan(at, count);
        at += count;
        return result;
    }
    std::uint32_t U32()
    {
        return Monstey_U32(Take(4), 0);
    }
    void Rows(std::size_t count)
    {
        Monstey_Require(count <= Monstey_MaximumMetadataRows - metadata_rows,
                        "Auxiliary probe metadata limit exceeded");
        metadata_rows += count;
    }
};
std::string Monstey_AuxiliaryName(std::span<const std::uint8_t> pool, std::uint32_t offset)
{
    Monstey_Require(offset < pool.size(), "WDSC dictionary offset exceeds pool");
    const auto end = std::find(pool.begin() + offset, pool.end(), std::uint8_t(0));
    Monstey_Require(end != pool.end(), "WDSC dictionary path is unterminated");
    return Monstey_DisplayUtf8(std::string_view(reinterpret_cast<const char *>(pool.data() + offset),
                                                static_cast<std::size_t>(end - (pool.begin() + offset))));
}
J Monstey_AuxiliaryVector(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    J::Array values;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        const auto bits = Monstey_U32(bytes, offset + 4 * axis);
        const auto value = std::bit_cast<float>(bits);
        values.push_back(std::isfinite(value) ? J(static_cast<double>(value)) : J{});
    }
    return values;
}
} // namespace
Monstey_Json Monstey_WdscAuxiliaryInspect(std::span<const std::uint8_t> source)
{
    Monstey_Require(source.size() >= 96 && source.size() <= Monstey_MaximumSourceBytes &&
                        std::memcmp(source.data(), "WDSC", 4) == 0 && Monstey_U32(source, 4) == 9,
                    "Expected bounded WDSC9 source");
    std::array<std::uint32_t, 9> header{};
    J::Array header_words;
    for (std::size_t i = 0; i < header.size(); ++i)
    {
        header[i] = Monstey_U32(source, 8 + 4 * i);
        header_words.push_back(header[i]);
    }
    Monstey_AuxiliaryReader reader(source);
    const auto strings = reader.Take(header[8]);
    Monstey_Require(header[4] <= (source.size() - reader.at) / 12, "WDSC model descriptors exceed extent");
    reader.Rows(header[4]);
    J::Array models;
    std::uint64_t material_total = 0;
    for (std::uint32_t model = 0; model < header[4]; ++model)
    {
        const auto offset = reader.at;
        const auto count = reader.U32();
        const auto path = reader.U32();
        const auto geometry_group = reader.U32();
        Monstey_Require(count <= (source.size() - reader.at) / 4, "WDSC materials exceed extent");
        reader.Rows(count);
        J::Array materials;
        for (std::uint32_t i = 0; i < count; ++i)
            materials.push_back(Monstey_AuxiliaryName(strings, reader.U32()));
        models.push_back(J::object({{"ordinal", model},
                                    {"offset", static_cast<std::uint64_t>(offset)},
                                    {"path", Monstey_AuxiliaryName(strings, path)},
                                    {"geometry_group", geometry_group},
                                    {"materials", materials}}));
        material_total += count;
    }
    Monstey_Require(material_total == header[5], "WDSC material allocation total differs");
    const auto bindings_offset = reader.at;
    (void)reader.Take(76ull * header[2]);
    Monstey_Require(header[0] <= (source.size() - reader.at) / 12, "WDSC groups exceed extent");
    reader.Rows(header[0]);
    std::uint64_t static_total = 0, array_total = 0, point_total = 0;
    J::Array groups;
    for (std::uint32_t group = 0; group < header[0]; ++group)
    {
        const auto group_offset = reader.at;
        const auto static_count = reader.U32();
        const auto array_count = reader.U32();
        const auto owner_count = reader.U32();
        Monstey_Require(static_count <= (source.size() - reader.at) / 32, "WDSC static rows exceed extent");
        reader.Rows(static_count);
        J::Array statics;
        for (std::uint32_t i = 0; i < static_count; ++i)
        {
            const auto offset = reader.at;
            const auto row = reader.Take(32);
            statics.push_back(J::object({{"offset", static_cast<std::uint64_t>(offset)},
                                         {"bounds_min", Monstey_AuxiliaryVector(row, 0)},
                                         {"bounds_max", Monstey_AuxiliaryVector(row, 12)},
                                         {"culling_flag_u8_24", row[24]},
                                         {"draw_ordinal_u16_28", Monstey_U16(row, 28)},
                                         {"opaque_tail_u16_30", Monstey_U16(row, 30)}}));
        }
        reader.Rows(array_count);
        J::Array arrays;
        for (std::uint32_t array = 0; array < array_count; ++array)
        {
            const auto offset = reader.at;
            const auto count = reader.Take(1)[0];
            reader.Rows(count);
            const auto payload = reader.Take(12ull * count);
            J::Array points;
            bool finite = true;
            for (std::uint32_t point = 0; point < count; ++point)
            {
                J::Array words;
                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    const auto bits = Monstey_U32(payload, 12ull * point + 4 * axis);
                    words.push_back(bits);
                    finite = finite && std::isfinite(std::bit_cast<float>(bits));
                }
                points.push_back(J::object({{"offset", static_cast<std::uint64_t>(offset + 1 + 12ull * point)},
                                            {"raw_words", words},
                                            {"xyz", Monstey_AuxiliaryVector(payload, 12ull * point)}}));
            }
            arrays.push_back(J::object({{"ordinal", array},
                                        {"offset", static_cast<std::uint64_t>(offset)},
                                        {"point_count_u8", count},
                                        {"payload_bytes", static_cast<std::uint64_t>(payload.size())},
                                        {"payload_sha256", Monstey_HashSha256(payload)},
                                        {"all_xyz_finite", finite},
                                        {"points", points}}));
            point_total += count;
        }
        Monstey_Require(owner_count <= (source.size() - reader.at) / 8, "WDSC owner groups exceed extent");
        reader.Rows(owner_count);
        J::Array owners;
        for (std::uint32_t owner = 0; owner < owner_count; ++owner)
        {
            const auto offset = reader.at;
            const auto key = reader.U32();
            const auto count = reader.U32();
            Monstey_Require(count <= (source.size() - reader.at) / 4, "WDSC owner associations exceed extent");
            reader.Rows(count);
            J::Array ordinals;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto ordinal = reader.U32();
                Monstey_Require(ordinal < header[2], "WDSC owner ordinal exceeds binding table");
                ordinals.push_back(ordinal);
            }
            owners.push_back(J::object(
                {{"offset", static_cast<std::uint64_t>(offset)}, {"key", key}, {"binding_ordinals", ordinals}}));
        }
        groups.push_back(J::object({{"ordinal", group},
                                    {"offset", static_cast<std::uint64_t>(group_offset)},
                                    {"static_records", statics},
                                    {"auxiliary_polygon_arrays", arrays},
                                    {"owner_groups", owners}}));
        static_total += static_count;
        array_total += array_count;
    }
    Monstey_Require(static_total == header[1] && array_total == header[6] && point_total == header[7],
                    "WDSC static or auxiliary allocation totals differ");
    const auto scene_offset = reader.at;
    const auto scene = reader.Take(source.size() - reader.at);
    Monstey_Require(scene.size() >= 52, "WDSC scene header is truncated");
    J::Array scene_header;
    for (std::size_t word = 0; word < 13; ++word)
        scene_header.push_back(Monstey_U32(scene, 4 * word));
    return J::object({{"schema", "Monstey.WdscAuxiliaryEvidence/1"},
                      {"source_sha256", Monstey_HashSha256(source)},
                      {"source_bytes", static_cast<std::uint64_t>(source.size())},
                      {"header_words", header_words},
                      {"models", models},
                      {"groups", groups},
                      {"binding_table_offset", static_cast<std::uint64_t>(bindings_offset)},
                      {"scene_offset", static_cast<std::uint64_t>(scene_offset)},
                      {"scene_header_words", scene_header},
                      {"scene_suffix_sha256", Monstey_HashSha256(scene)},
                      {"historical_field_label", "lods"},
                      {"polygon_render_semantics_qualified", false},
                      {"draw_reference_semantics_qualified", false},
                      {"reader_only", true}});
}
Monstey_Json Monstey_WdscAuxiliaryInspectFile(const std::filesystem::path &source)
{
    const auto raw = Monstey_ReadFile(source, Monstey_MaximumSourceBytes);
    auto report = Monstey_WdscAuxiliaryInspect(raw);
    Monstey_Require(Monstey_HashSha256(source) == report.at("source_sha256").as_string(),
                    "WDSC source changed during read-only inspection");
    report["source_unchanged"] = true;
    return report;
}
} // namespace Monstey