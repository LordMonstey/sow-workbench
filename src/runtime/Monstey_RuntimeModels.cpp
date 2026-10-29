#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_AnimationPreviewApi.hpp"
#include "Monstey_ModelPreviewApi.hpp"
#include "Monstey_ModelEnvelope.hpp"
#include "Monstey_ModelSockets.hpp"
#include "Monstey_AnimatedExtras.hpp"
#include "Monstey_BlendModel.hpp"
#include "Monstey_Model.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryModelViews(std::string_view op, const Monstey_Json &a)
{
    if (op == "model_animation_preview_render")
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_AnimationPreviewCall(op, values);
    }
    if (op == "model_preview_render")
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_ModelPreviewCall(op, values);
    }
    if (op == "model_envelope_inspect" || op == "model_envelope_build")
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return op == "model_envelope_inspect" ? Monstey_ModelEnvelopeInspect(values)
                                              : Monstey_ModelEnvelopeBuild(values);
    }
    if (op.starts_with("model_socket"))
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_ModelSocketsCall(Database(), op, values);
    }
    if (op.starts_with("animated_extra"))
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_AnimatedExtrasCall(Database(), op, values);
    }
    return std::nullopt;
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryModels(std::string_view op, const Monstey_Json &a)
{
    using J = Monstey_Json;
    if (op == "model_import_native" || op == "model_export_native")
    {
        auto options = J::object();
        J result;
        if (op == "model_import_native")
        {
            for (const auto *key : {"expected_source_sha256", "expected_skeleton_sha256"})
                if (a.contains(key))
                    options[key] = a.at(key);
            result = Monstey_ModelImportNative(Monstey_Path(a, "source"), Monstey_Path(a, "output_blend"),
                                               Monstey_Path(a, "skeleton"), options);
            result["output_blend"] = result.at("output");
            result["canonical_operation"] = "model_import_blend";
        }
        else
        {
            if (a.contains("expected_sha256"))
                options["expected_source_sha256"] = a.at("expected_sha256");
            if (a.contains("triangle_band"))
                options["triangle_band"] = a.at("triangle_band");
            result = Monstey_ModelExportNative(Monstey_Path(a, "blend"), {}, Monstey_Path(a, "output_mesh"), options);
            result["output_mesh"] = result.at("output");
            result["canonical_operation"] = "model_export_blend";
        }
        result["legacy_operation"] = std::string(op);
        result["blender_argument_ignored"] = a.contains("blender");
        result["pipeline"] = "native_cpp";
        return result;
    }
    if (op == "model_import_blend" || op == "model_export_blend")
    {
        auto options = J::object();
        for (const char *key : {"blend_template", "blender", "preflight", "expected_source_sha256",
                                "expected_template_sha256", "expected_skeleton_sha256", "triangle_band"})
            if (a.contains(key))
                options[key] = a.at(key);
        if (op == "model_import_blend")
        {
            if (!options.contains("blender"))
            {
                const auto config = settings_->Describe();
                if (config.contains("blender") && config.at("blender").is_string() &&
                    !config.at("blender").as_string().empty())
                    options["blender"] = config.at("blender");
            }
            return Monstey_ModelImportNative(Monstey_Path(a, "source"), Monstey_Path(a, "output"),
                                             Monstey_Path(a, "skeleton"), options);
        }
        return Monstey_ModelExportNative(Monstey_Path(a, "source"), Monstey_Path(a, "template"),
                                         Monstey_Path(a, "output"), options);
    }
    if (op == "model_inspect")
        return Monstey_ModelInspect(Monstey_Path(a, "source"), Monstey_Path(a, "skeleton"),
                                    a.value("inspection", J::object()));
    if (op == "model_export_glb")
        return Monstey_ModelExportGlb(Monstey_Path(a, "source"), Monstey_Path(a, "output"),
                                      Monstey_Path(a, "skeleton"));
    if (op == "model_roundtrip")
        return Monstey_ModelRoundtrip(Monstey_Path(a, "source"), Monstey_Path(a, "output"));
    if (op == "model_convert")
    {
        const auto source = Monstey_Path(a, "source"), nativeTemplate = Monstey_Path(a, "template");
        auto options = a.value("options", J::object());
        if (a.contains("container"))
            options["container"] = a.at("container");
        if (a.contains("binding"))
            options["binding"] = a.at("binding");
        if (a.contains("bone"))
            options["rigid_bone"] = a.at("bone");
        if (a.contains("scale"))
            options["scale_cm_per_metre"] = 100.0 * a.at("scale").as_number();
        if (a.contains("rotate_x") || a.contains("rotate_y") || a.contains("rotate_z"))
            options["rotate_degrees"] =
                J::array({a.value("rotate_x", 0.0), a.value("rotate_y", 0.0), a.value("rotate_z", 0.0)});
        if (a.contains("mapping"))
            options["joint_map"] = J::Parse(Monstey_ReadText(Monstey_Path(a, "mapping")));
        for (const char *key :
             {"primitive", "preflight", "expected_source_sha256", "expected_template_sha256", "triangle_band"})
            if (a.contains(key))
                options[key] = a.at(key);
        return Monstey_ModelConvert(source, nativeTemplate, Monstey_Path(a, "output"), options);
    }
    return std::nullopt;
}
} // namespace Monstey
