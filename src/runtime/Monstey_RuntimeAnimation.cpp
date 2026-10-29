#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include <array>

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryAnimation(std::string_view op, const Monstey_Json &a)
{
    using J = Monstey_Json;
    if (op == "animation_open" || op == "animation_new" || op == "animation_load_project")
    {
        const auto path = Monstey_Path(a, op == "animation_new" ? "scene_path" : "path");
        auto candidate = op == "animation_load_project"
                             ? Monstey_AnimationProject::LoadProject(path)
                             : std::make_unique<Monstey_AnimationProject>(path, Monstey_Text(a, "expected_sha256"),
                                                                          op == "animation_new");
        const auto description = candidate->Describe();
        settings_->Protect(path);
        settings_->Protect(Monstey_Path(description, "source"));
        animation_ = std::move(candidate);
        ++generation_;
        return description;
    }
    if (op == "animation_skeleton_inspect" || op == "animation_skeleton_register")
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        settings_->Protect(Monstey_Path(values, "source"));
        return Monstey_AnimationSkeletonCall(op, values);
    }
    if (op == "animation_describe")
        return Animation().Describe();
    if (op == "animation_clips")
        return Animation().Clips(Monstey_Text(a, "query"), static_cast<std::size_t>(a.value("offset", 0).as_uint()),
                                 static_cast<std::size_t>(a.value("limit", 250).as_uint()));
    if (op == "animation_clip")
        return Animation().Clip(Monstey_Text(a, "clip"));
    if (op == "animation_sample")
        return Animation().Sample(Monstey_Text(a, "clip"), a.at("ticks"), a.value("bind_fallback", true).as_bool());
    if (op == "animation_edit")
    {
        if (a.contains("operations") == a.contains("operations_path"))
            throw Monstey_Error("invalid_arguments", "Select operations or operations_path, exclusively");
        if (a.contains("operations"))
        {
            if (a.contains("expected_sha256"))
                throw Monstey_Error("invalid_arguments", "expected_sha256 requires an operation file");
            return Animation().Edit(a.at("operations"));
        }
        const auto path = Monstey_Path(a, "operations_path");
        const auto bytes = Monstey_ReadFile(path);
        const auto sha = Monstey_HashSha256(bytes);
        if ((a.contains("expected_sha256") && sha != Monstey_Text(a, "expected_sha256")) ||
            Monstey_HashSha256(path) != sha)
            throw Monstey_Error("source_hash_mismatch", "Animation operation file changed or hash differs");
        auto text = std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (text.starts_with("\xef\xbb\xbf"))
            text.remove_prefix(3);
        const auto operations = J::Parse(text);
        settings_->Protect(path);
        return Animation().Edit(operations);
    }
    if (op == "animation_import_clip")
    {
        const auto source = Monstey_Path(a, "scene_path");
        settings_->Protect(source);
        return Animation().ImportClip(source, Monstey_Text(a, "clip"), Monstey_Text(a, "name"),
                                      Monstey_Text(a, "expected_sha256"));
    }
    if (op == "animation_export")
        return Animation().Export(Monstey_Path(a, "output"), Monstey_Text(a, "clip"));
    if (op == "animation_undo")
        return Animation().Undo();
    if (op == "animation_redo")
        return Animation().Redo();
    if (op == "animation_revert")
        return Animation().Revert();
    if (op == "animation_changes")
    {
        auto options = J::object();
        for (const auto *key : {"include_operations", "offset", "limit"})
            if (a.contains(key))
                options[key] = a.at(key);
        return Animation().Changes(options);
    }
    if (op == "animation_save_project")
        return Animation().SaveProject(Monstey_Path(a, "path"));
    if (op == "animation_build")
        return Animation().Build(Monstey_Path(a, "output"));
    return std::nullopt;
}
} // namespace Monstey
