#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <map>
namespace Monstey
{
enum class Monstey_ModelProjection
{
    NativeCentimetres,
    Float32Metres
};
struct Monstey_ModelChannel
{
    unsigned width = 0;
    std::vector<double> values;
};
struct Monstey_ModelPart
{
    std::size_t container = 0, part = 0;
    std::map<std::string, Monstey_ModelChannel> channels;
    std::vector<std::uint32_t> indices;
    // Ordinal native triangle-prefix bands, bound to the current index topology.
    // Empty hash means that the source's effective membership is not qualified.
    std::vector<std::uint8_t> triangle_bands;
    std::string triangle_band_topology_sha256;
};
std::string Monstey_ModelTriangleTopologyHash(std::span<const std::uint32_t> indices);
void Monstey_ModelSetTriangleBands(Monstey_ModelPart &part, std::span<const std::uint8_t> bands);
struct Monstey_ModelBone
{
    std::size_t hk_index = 0, source_index = 0;
    int parent_hk = -1;
    std::uint32_t native_id = 0;
    std::string name;
    std::array<double, 16> global_bind_metres{};
};
struct Monstey_ModelScene
{
    std::string source_sha256, baseline_sha256, skeleton_sha256;
    Monstey_Bytes native_baseline;
    std::vector<Monstey_ModelPart> parts;
    std::vector<Monstey_ModelBone> bones;
    std::vector<std::size_t> hk_to_source, source_to_hk;
    // Semantic positions remain centimetres; the projection records a host float32-metre roundtrip.
    Monstey_ModelProjection position_projection = Monstey_ModelProjection::NativeCentimetres;
};
Monstey_ModelScene Monstey_ModelReadScene(const std::filesystem::path &mesh, const std::filesystem::path &skel = {});
Monstey_ModelScene Monstey_ModelReadSceneBytes(std::span<const std::uint8_t> bytes,
                                               const std::filesystem::path &skel = {});
Monstey_Json Monstey_ModelWriteScene(const Monstey_ModelScene &scene, const std::filesystem::path &nativeTemplate,
                                     const std::filesystem::path &output,
                                     const Monstey_Json &options = Monstey_Json::object());

Monstey_ModelScene Monstey_ModelReadRigidSceneBytes(std::span<const std::uint8_t> bytes);
Monstey_Json Monstey_ModelInspectBytes(std::span<const std::uint8_t> bytes, const std::filesystem::path &skel = {});
Monstey_Json Monstey_ModelInspectBytes(std::span<const std::uint8_t> bytes, const std::filesystem::path &skel,
                                       const Monstey_Json &inspection);
Monstey_Json Monstey_ModelInspect(const std::filesystem::path &mesh, const std::filesystem::path &skel,
                                  const Monstey_Json &inspection);
Monstey_Json Monstey_ModelInspectSkeleton(std::span<const std::uint8_t> bytes);
// Read-only metadata paging: table_offset/table_limit and row_offset/row_limit (total row budget).
Monstey_Json Monstey_ModelInspectSkeleton(std::span<const std::uint8_t> bytes, const Monstey_Json &options);
Monstey_Json Monstey_ModelNativePartMetadata(std::span<const std::uint8_t> bytes);
Monstey_Json Monstey_ModelInspect(const std::filesystem::path &mesh, const std::filesystem::path &skel = {});
Monstey_Json Monstey_ModelExportGlb(const std::filesystem::path &mesh, const std::filesystem::path &output,
                                    const std::filesystem::path &skel = {});
Monstey_Json Monstey_ModelConvert(const std::filesystem::path &source, const std::filesystem::path &nativeTemplate,
                                  const std::filesystem::path &output,
                                  const Monstey_Json &options = Monstey_Json::object());
Monstey_Json Monstey_ModelRoundtrip(const std::filesystem::path &mesh, const std::filesystem::path &output);
} // namespace Monstey
