#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// The original 40-operation world catalogue remains explicit during the C++ port.
// Unimplemented operations throw world_operation_not_ported; no Python fallback.
Monstey_Bytes Monstey_ObjresReadChunk(std::span<const std::uint8_t> raw, std::uint32_t kind);
Monstey_Bytes Monstey_ObjresReplaceChunk(std::span<const std::uint8_t> raw, std::uint32_t kind,
                                         std::span<const std::uint8_t> replacement);
Monstey_Json Monstey_ObjresOwners(std::span<const std::uint8_t> raw, bool include_properties = true);
std::pair<Monstey_Bytes, Monstey_Json> Monstey_ObjresApply(std::span<const std::uint8_t> raw,
                                                           const Monstey_Json &spec);
std::pair<Monstey_Bytes, Monstey_Json>
Monstey_ObjresImportOwner(std::span<const std::uint8_t> target_raw, std::span<const std::uint8_t> donor_raw,
                          std::string_view donor_name, std::string_view new_name,
                          const Monstey_Json &position, const Monstey_Json &quaternion,
                          const Monstey_Json &properties = Monstey_Json::object(),
                          std::uint32_t group_index = 0);
std::pair<Monstey_Bytes, Monstey_Json> Monstey_InstApplyBytes(std::span<const std::uint8_t> raw,
                                                              const Monstey_Json &spec);
std::pair<Monstey_Bytes, Monstey_Json> Monstey_WorldMeshApplyBytes(std::span<const std::uint8_t> raw,
                                                                   const Monstey_Json &spec);
std::pair<Monstey_Bytes, Monstey_Json> Monstey_WdscBindingBytes(std::span<const std::uint8_t> inst,
                                                                std::string_view resource,
                                                                std::uint32_t owner_key,
                                                                std::span<const std::uint8_t> donor);
// Validate authored static geometry against the qualified native descriptor[0]
// consumer. The WDSC is rebuilt by the existing native writer and compared byte
// for byte with the supplied candidate. No runtime visibility claim is made.
Monstey_Json Monstey_WorldStaticPairValidate(
    std::span<const std::uint8_t> source_mesh,
    std::span<const std::uint8_t> source_render,
    std::span<const std::uint8_t> candidate_mesh,
    std::span<const std::uint8_t> candidate_render,
    const Monstey_Json &topology, std::string_view source_resource,
    std::string_view target_resource, bool preserve_section,
    std::span<const std::uint8_t> scene_template = {});
// Same writer used by sow_world_render_build, exposed for coupled transactions.
std::pair<Monstey_Bytes, Monstey_Json> Monstey_WorldStaticRenderBuild(
    std::span<const std::uint8_t> candidate_mesh,
    std::span<const std::uint8_t> source_render,
    const Monstey_Json &topology, std::string_view source_resource,
    std::string_view target_resource, bool preserve_section,
    std::span<const std::uint8_t> scene_template = {});
Monstey_Json Monstey_WorldCatalog();
Monstey_Json Monstey_WorldCall(std::string_view operation, const Monstey_Json &arguments);
} // namespace Monstey
