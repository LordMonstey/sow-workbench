#pragma once
#include "Monstey_Model.hpp"
namespace Monstey {
Monstey_Json Monstey_ModelImportNative(const std::filesystem::path& mesh,
 const std::filesystem::path& outputBlend,const std::filesystem::path& skel={},
 const Monstey_Json& options=Monstey_Json::object());
Monstey_ModelScene Monstey_ModelReadBlendScene(const std::filesystem::path& blend);
Monstey_ModelScene Monstey_ModelReadBlendScene(const std::filesystem::path& blend, const Monstey_Json& options);
Monstey_Json Monstey_ModelExportNative(const std::filesystem::path& blend,
 const std::filesystem::path& nativeTemplate,const std::filesystem::path& outputMesh,
 const Monstey_Json& options=Monstey_Json::object());
}