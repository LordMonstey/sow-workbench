#include "model/Monstey_ModelPrivate.hpp"

namespace Monstey
{
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelInspect(const std::filesystem::path &mesh, const std::filesystem::path &skel)
{
    return Monstey_ModelInspect(mesh, skel, Monstey_J::object());
}

Monstey_Json Monstey_ModelInspect(const std::filesystem::path &mesh, const std::filesystem::path &skel,
                                  const Monstey_J &inspection)
{
    auto result = Monstey_ModelInspectBytes(Monstey_ReadFile(mesh), skel, inspection);
    result["source"] = Monstey_Utf8(mesh.wstring());
    return result;
}

Monstey_ModelScene Monstey_ModelReadScene(const std::filesystem::path &mesh, const std::filesystem::path &skel)
{
    return Monstey_ModelReadSceneBytes(Monstey_ReadFile(mesh), skel);
}

} // namespace Monstey
