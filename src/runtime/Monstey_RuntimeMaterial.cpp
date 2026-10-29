#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryMaterial(std::string_view op, const Monstey_Json &a)
{
    using J = Monstey_Json;
    if (op == "material_open" || op == "material_load_project")
    {
        const auto path = Monstey_Path(a, "path");
        auto candidate = op == "material_load_project"
                             ? Monstey_MaterialProject::LoadProject(path, Monstey_Text(a, "expected_sha256"))
                             : std::make_unique<Monstey_MaterialProject>(path, Monstey_Text(a, "expected_sha256"));
        const auto description = candidate->Describe();
        settings_->Protect(path);
        settings_->Protect(Monstey_Path(description, "source"));
        material_ = std::move(candidate);
        ++generation_;
        return description;
    }
    if (op == "material_describe")
        return Material().Describe();
    if (op == "material_edit")
    {
        if (a.contains("operations") == a.contains("operations_path"))
            throw Monstey_Error("invalid_arguments", "Select operations or operations_path, exclusively");
        if (a.contains("operations"))
        {
            if (a.contains("expected_sha256"))
                throw Monstey_Error("invalid_arguments", "expected_sha256 requires an operation file");
            return Material().Edit(a.at("operations"));
        }
        const auto path = Monstey_Path(a, "operations_path");
        settings_->Protect(path);
        return Material().EditFile(path, Monstey_Text(a, "expected_sha256"));
    }
    if (op == "material_undo")
        return Material().Undo();
    if (op == "material_redo")
        return Material().Redo();
    if (op == "material_revert")
        return Material().Revert();
    if (op == "material_changes")
    {
        auto options = J::object();
        for (const auto *key : {"include_operations", "offset", "limit"})
            if (a.contains(key))
                options[key] = a.at(key);
        return Material().Changes(options);
    }
    if (op == "material_save_project")
        return Material().SaveProject(Monstey_Path(a, "path"));
    if (op == "material_build")
        return Material().Build(Monstey_Path(a, "output"));
    return std::nullopt;
}
} // namespace Monstey
