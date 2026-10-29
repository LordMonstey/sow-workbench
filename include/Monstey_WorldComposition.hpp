#pragma once
#include "Monstey_Core.hpp"
#include <map>
namespace Monstey {
struct Monstey_BundleDependency {
    std::string resource;
    std::uint32_t size_hint{};
};
struct Monstey_EmbeddedResource {
    std::string resource;
    Monstey_Bytes bytes;
};
struct Monstey_Bundle {
    std::string kind;
    std::vector<Monstey_BundleDependency> resources;
};
Monstey_Bundle Monstey_ReadBundle(std::span<const std::uint8_t> bytes);
Monstey_Bytes Monstey_BuildBundle(const Monstey_Bundle &bundle);
std::vector<Monstey_EmbeddedResource> Monstey_ReadEmbedded(std::span<const std::uint8_t> bytes);
Monstey_Bytes Monstey_BuildEmbedded(const std::vector<Monstey_EmbeddedResource> &resources);
Monstey_Bytes Monstey_RebuildBundle(std::span<const std::uint8_t> original,
                                    const std::map<std::string, std::uint32_t> &updates,
                                    const std::map<std::string, std::uint32_t> &additions = {});
Monstey_Bytes Monstey_RebuildEmbedded(std::span<const std::uint8_t> original,
                                      const std::map<std::string, Monstey_Bytes> &updates,
                                      const std::map<std::string, Monstey_Bytes> &additions = {});
Monstey_Json Monstey_WorldCompositionCatalog();
Monstey_Json Monstey_WorldCompositionCall(std::string_view operation, const Monstey_Json &arguments);
} // namespace Monstey
