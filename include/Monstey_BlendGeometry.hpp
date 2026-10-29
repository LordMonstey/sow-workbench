#pragma once
#include "Monstey_Blend.hpp"
#include "Monstey_Model.hpp"
#include <functional>
#include <optional>
namespace Monstey {
struct Monstey_BlendGeometryAttribute {
    int type = 0, domain = 0;
    std::size_t count = 0;
    Monstey_Bytes data;
};
std::filesystem::path Monstey_BlendGeometryTemplate(const Monstey_Json &options = Monstey_Json::object());
// Positions use exactly the caller's coordinate system unless a scale is explicitly supplied.
Monstey_Json Monstey_BlendGeometryCreateMesh(Monstey_Blend &blend, const Monstey_ModelPart &part,
                                             std::string_view name, double positionScale = 1);
Monstey_ModelPart
Monstey_BlendGeometryReadMesh(Monstey_Blend &blend, std::size_t mesh, const Monstey_ModelPart &baseline,
                              double positionScale = 1,
                              const std::function<void(Monstey_ModelPart &)> &beforeCornerSplit = {},
                              std::optional<std::uint8_t> uniformTriangleBand = {});
Monstey_Json Monstey_BlendGeometryCreateObject(Monstey_Blend &blend, std::string_view name, int type,
                                               std::uint64_t dataAddress = 0);
Monstey_Json Monstey_BlendGeometryObjects(Monstey_Blend &blend);
std::map<std::string, Monstey_BlendGeometryAttribute> Monstey_BlendGeometryAttributes(Monstey_Blend &blend,
                                                                                      std::size_t mesh);
void Monstey_BlendGeometrySetProperty(Monstey_Blend &blend, std::size_t id, std::string_view name,
                                      const Monstey_Json &value);
Monstey_Json Monstey_BlendGeometryProperties(Monstey_Blend &blend, std::size_t id);
Monstey_Json Monstey_BlendGeometryCollection(Monstey_Blend &blend, std::string_view name);
void Monstey_BlendGeometryLinkObjects(Monstey_Blend &blend, std::size_t collection,
                                      const std::vector<Monstey_Json> &objects, bool replace = false);
void Monstey_BlendGeometryLinkCollection(Monstey_Blend &blend, std::size_t parent, const Monstey_Json &child);
std::size_t Monstey_BlendGeometryRootCollection(Monstey_Blend &blend);
void Monstey_BlendGeometryWriteText(Monstey_Blend &blend, std::string_view name, std::string_view text);
std::string Monstey_BlendGeometryReadText(Monstey_Blend &blend, std::string_view name);
} // namespace Monstey