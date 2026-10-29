#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

std::wstring Monstey_ModelOperationLabel(std::string_view operation)
{
    if (operation == "model_import_blend")
        return L"Import native model to Blender";
    if (operation == "model_export_blend")
        return L"Export Blender project to native";
    if (operation == "model_inspect")
        return L"Inspect model and rig";
    if (operation == "model_export_glb")
        return L"Export native model to GLB";
    if (operation == "model_convert")
        return L"Convert custom GLB to native";
    if (operation == "model_roundtrip")
        return L"Verify native round trip";
    if (operation == "model_preview_render")
        return L"Render bind-geometry preview";
    if (operation == "model_animation_preview_render")
        return L"Render an animation pose preview";
    if (operation == "model_envelope_inspect")
        return L"Inspect native model visible radius";
    if (operation == "model_envelope_build")
        return L"Build expanded native SKEL visible radius";
    return Monstey_Wide(operation);
}

const wchar_t *Monstey_ModelFileFilter(std::string_view operation, std::string_view key)
{
    if ((operation == "model_preview_render" || operation == "model_animation_preview_render") && key == "output")
        return L"Bitmap preview (*.bmp)\0*.bmp\0All files\0*.*\0\0";
    if (operation == "model_animation_preview_render" && key == "animation")
        return L"Native animation (*.anix)\0*.anix\0All files\0*.*\0\0";
    if (key == "mapping" || key == "report")
        return L"JSON file (*.json)\0*.json\0All files\0*.*\0\0";
    if (key == "skeleton" || key == "output_skeleton")
        return L"SoW skeleton (*.skel)\0*.skel\0All files\0*.*\0\0";
    if (key == "blend_template" || (key == "source" && operation == "model_export_blend") ||
        (key == "output" && operation == "model_import_blend"))
        return L"Blender project (*.blend)\0*.blend\0All files\0*.*\0\0";
    if ((key == "source" && operation == "model_convert") || (key == "output" && operation == "model_export_glb"))
        return L"Binary glTF model (*.glb)\0*.glb\0All files\0*.*\0\0";
    if (key == "source" && operation == "model_inspect")
        return L"SoW mesh or GLB (*.mesh;*.glb)\0*.mesh;*.glb\0All files\0*.*\0\0";
    return L"SoW native mesh (*.mesh)\0*.mesh\0All files\0*.*\0\0";
}

const wchar_t *Monstey_ModelOutputExtension(std::string_view operation)
{
    if (operation == "model_preview_render" || operation == "model_animation_preview_render")
        return L"bmp";
    if (operation == "model_import_blend")
        return L"blend";
    if (operation == "model_export_glb")
        return L"glb";
    return L"mesh";
}

void Monstey_App::ModelPreviewCommand()
{
    if (closing)
        return;
    const auto source =
        Monstey_FileDialog(window, false, L"SoW mesh (*.mesh;*.mmsh)\0*.mesh;*.mmsh\0All files\0*.*\0\0", L"mesh",
                           L"Preview native geometry and skeleton");
    if (!source || closing)
        return;
    auto skeleton = *source;
    skeleton.replace_extension(L".skel");
    if (!std::filesystem::is_regular_file(skeleton))
        skeleton.clear();
    if (!modelPreview)
        modelPreview = std::make_shared<Monstey_ModelPreviewWindow>();
    const std::weak_ptr<Monstey_ModelPreviewWindow> weakPreview = modelPreview;
    modelPreview->SetAnimationLoader([this, weakPreview](const auto &request)
                                     { ModelAnimationCommand(request, weakPreview); });
    modelPreview->Create(window);
    const auto request = modelPreview->BeginLoad(*source, skeleton);
    auto load = std::make_shared<Monstey_PreviewLoadRequest>();
    load->source = *source;
    load->skeleton = std::move(skeleton);
    load->target = modelPreview;
    load->request = request;
    load->holdForCloseTest = smokePreview && smokePreviewClose && !smokePreviewAnimation;
    Monstey_Job job;
    job.request = J::object({{"operation", "model_preview_load"}});
    job.epoch = epoch;
    job.originTab = 4;
    job.previewLoad = std::move(load);
    {
        std::lock_guard lock(mutex);
        if (stopping)
            return;
        const std::weak_ptr<Monstey_ModelPreviewWindow> target = modelPreview;
        std::erase_if(jobs,
                      [&](const auto &queued)
                      {
                          return queued.previewLoad && !queued.previewLoad->target.owner_before(target) &&
                                 !target.owner_before(queued.previewLoad->target);
                      });
        jobs.push_back(std::move(job));
    }
    condition.notify_one();
    SetStatus(L"Loading native geometry and skeleton ...");
}

void Monstey_App::ModelAnimationCommand(const Monstey_PreviewAnimationRequest &request,
                                        std::weak_ptr<Monstey_ModelPreviewWindow> weak)
{
    const auto target = weak.lock();
    if (closing || !target || !target->Handle())
        return;
    if (request.animation.empty())
    {
        const auto selected =
            Monstey_FileDialog(target->Handle(), false, L"Native animation (*.anix)\0*.anix\0All files\0*.*\0\0",
                               L"anix", L"Open animation library");
        if (selected && !closing && target->Handle())
            target->RequestAnimation(*selected);
        return;
    }
    if (!target->AcceptsAnimation(request.request))
        return;
    auto load = std::make_shared<Monstey_PreviewLoadRequest>();
    load->skeleton = request.skeleton;
    load->animation = request.animation;
    load->clip = request.clip;
    load->expectedAnimationHash = request.expected_anix_sha256;
    load->bindScene = request.source;
    load->target = target;
    load->request = request.request;
    load->holdForCloseTest = smokePreviewAnimation && smokePreviewClose;
    Monstey_Require(load->bindScene != nullptr, "Animation request has no immutable bind source");
    Monstey_Job job;
    job.request = J::object({{"operation", "model_animation_preview_load"}});
    job.epoch = epoch;
    job.originTab = 4;
    job.previewLoad = std::move(load);
    {
        std::lock_guard lock(mutex);
        if (stopping)
            return;
        std::erase_if(jobs,
                      [&](const auto &queued)
                      {
                          return queued.previewLoad && !queued.previewLoad->target.owner_before(weak) &&
                                 !weak.owner_before(queued.previewLoad->target);
                      });
        jobs.push_back(std::move(job));
    }
    condition.notify_one();
    SetStatus(L"Loading native animation preview...");
}

void Monstey_App::ModelWorkflow(std::string_view operation)
{
    if (tab != 4)
        return;
    const auto found = std::find_if(rowData.begin(), rowData.end(),
                                    [&](const auto &row) { return row.at("name").as_string() == operation; });
    if (found == rowData.end())
    {
        SetStatus(L"The native model catalogue is still loading.");
        return;
    }
    const auto source = Monstey_FileDialog(window, false, Monstey_ModelFileFilter(operation, "source"), nullptr,
                                           operation == "model_import_blend" ? L"Import native SoW mesh to Blender"
                                                                             : L"Export an edited Blender project");
    if (!source)
        return;
    const auto output = Monstey_FileDialog(
        window, true, Monstey_ModelFileFilter(operation, "output"), Monstey_ModelOutputExtension(operation),
        operation == "model_import_blend" ? L"Save NEW Blender project" : L"Save NEW native SoW mesh");
    if (!output)
        return;
    Monstey_Require(!std::filesystem::exists(*output), "Choose a new output path. Existing files are preserved.");
    const std::string name(operation);
    auto args = operationPresets.contains(name) ? operationPresets.at(name) : J::object();
    args["source"] = Monstey_JsonPath(*source);
    args["output"] = Monstey_JsonPath(*output);
    args.as_object().erase("expected_source_sha256");
    if (operation == "model_import_blend")
    {
        auto skeleton = *source;
        skeleton.replace_extension(L".skel");
        if (std::filesystem::is_regular_file(skeleton))
            args["skeleton"] = Monstey_JsonPath(skeleton);
        else
            args.as_object().erase("skeleton");
    }
    else
    {
        args.as_object().erase("preflight");
        modelBlendPreview = *source;
    }
    operationPresets[name] = std::move(args);
    const auto current = std::find_if(rowData.begin(), rowData.end(),
                                      [&](const auto &row) { return row.at("name").as_string() == name; });
    Monstey_Require(current != rowData.end(), "Native model operation is no longer selected.");
    const auto index = static_cast<int>(current - rowData.begin());
    Monstey_ListSelect(records, index);
    SelectOperation(index);
    RunOperation();
}

void Monstey_App::ModelEnvelope(J inputs)
{
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    if (inputs.empty())
    {
        auto baseline = Monstey_FileDialog(window, false, L"SoW mesh (*.mesh)\0*.mesh\0\0", L"mesh",
                                           L"Choose the original baseline mesh");
        if (!baseline)
            return;
        auto encoded = Monstey_FileDialog(window, false, L"SoW mesh (*.mesh)\0*.mesh\0\0", L"mesh",
                                          L"Choose the final encoded replacement mesh");
        if (!encoded)
            return;
        auto rig = Monstey_FileDialog(window, false, L"SoW skeleton (*.skel)\0*.skel\0\0", L"skel",
                                      L"Choose the matching original skeleton");
        if (!rig)
            return;
        inputs = J::object({{"baseline_mesh", Monstey_JsonPath(*baseline)},
                            {"encoded_mesh", Monstey_JsonPath(*encoded)},
                            {"skeleton", Monstey_JsonPath(*rig)}});
    }
    for (auto [path, hash] :
         {std::pair{"baseline_mesh", "expected_baseline_sha256"}, std::pair{"encoded_mesh", "expected_encoded_sha256"},
          std::pair{"skeleton", "expected_skeleton_sha256"}})
        inputs[hash] = Monstey_HashSha256(std::filesystem::path(Monstey_Wide(inputs.at(path).as_string())));
    Queue("model_envelope_inspect", inputs,
          [this, inputs](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              const auto report = reply.at("result");
              ShowReport(report);
              auto item = std::make_shared<Monstey_Editor>();
              item->app = this;
              item->title = L"Native model visible radius";
              const auto summary = "Centimetres: old " + report.at("source_radius_cm").dump() + ", required " +
                                   report.at("required_radius_cm").dump() + ", output " +
                                   report.at("output_radius_cm").dump() +
                                   ".\r\nBind geometry covered; donor padding retained.\r\nAll animation "
                                   "poses, cloth deformation and in-game result remain unverified.";
              item->field = J::object({{"name", "Review native SKEL17 sphere growth"},
                                       {"native_type", 0},
                                       {"value", J::object()},
                                       {"editable", true},
                                       {"review_text", summary}});
              item->formFields = J::array({J::object({{"name", "write_report"},
                                                      {"label", "Also write a JSON report beside the NEW skeleton"},
                                                      {"kind", "bool"},
                                                      {"value", true}})});
              item->success = [this, inputs, report](const J &options) mutable
              {
                  auto output = smokeEnvelope
                                    ? std::optional<std::filesystem::path>(smokeDirectory / L"visible-radius.skel")
                                    : Monstey_FileDialog(window, true, L"SoW skeleton (*.skel)\0*.skel\0\0", L"skel",
                                                         L"Save visible radius to a NEW skeleton");
                  if (!output)
                      return;
                  inputs["output"] = Monstey_JsonPath(*output);
                  if (options.at("write_report").as_bool())
                      inputs["report"] = Monstey_JsonPath(std::filesystem::path(output->wstring() + L".report.json"));
                  Queue("model_envelope_build", inputs,
                        [this, report](const J &result)
                        {
                            if (!result.at("ok").as_bool())
                            {
                                Error(result.at("error").at("message").as_string());
                                return;
                            }
                            ShowReport(result.at("result"));
                            if (smokeEnvelope)
                            {
                                smoke["human_input_hashes_computed"] = true;
                                smoke["native_inspection"] = report;
                                smoke["native_build"] = result.at("result");
                                smoke["review_before_save"] = true;
                                smoke["runtime_verified"] = false;
                                SmokeComplete();
                            }
                            else
                                SetStatus(L"New native skeleton and optional report written. All "
                                          L"other skeleton bytes retained.");
                        });
              };
              ShowEditor(item);
              if (smokeEnvelope)
                  smokeStage = 2;
          });
}

void Monstey_App::OpenModelBlender()
{
    if (modelBlendPreview.empty() || !std::filesystem::is_regular_file(modelBlendPreview))
    {
        SetStatus(L"Import a native model first, or select a Blender source.");
        return;
    }
    const auto project = modelBlendPreview;
    Queue("components", J::object(),
          [this, project](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              const auto &configuration = reply.at("result").at("configuration");
              HINSTANCE launched = nullptr;
              if (configuration.contains("blender") && configuration.at("blender").is_string() &&
                  !configuration.at("blender").as_string().empty())
              {
                  const auto executable = Monstey_Wide(configuration.at("blender").as_string());
                  const auto argument = L"\"" + project.wstring() + L"\"";
                  launched = ShellExecuteW(window, L"open", executable.c_str(), argument.c_str(),
                                           project.parent_path().c_str(), SW_SHOWNORMAL);
              }
              else
                  launched = ShellExecuteW(window, L"open", project.c_str(), nullptr, project.parent_path().c_str(),
                                           SW_SHOWNORMAL);
              Monstey_Require(reinterpret_cast<INT_PTR>(launched) > 32,
                              "Blender could not be opened. Select its executable in Settings.");
              SetStatus(L"Blender opened with the native model project.");
          });
}

void Monstey_App::TexturePreviewCommand()
{
    if (closing)
        return;
    const auto chosen = Monstey_FileDialog(
        window, false, L"Native textures (*.tex;*.texa;*.dds)\0*.tex;*.texa;*.dds\0All files\0*.*\0\0", L"tex",
        L"Open native texture");
    if (!chosen || closing)
        return;
    if (!texturePreview)
        texturePreview = std::make_unique<Monstey_TexturePreviewWindow>();
    texturePreview->Open(window, *chosen);
    SetStatus(L"Texture inspector opened. Loading and export status are shown in its window.");
}

} // namespace Monstey::Monstey_AppInternal
