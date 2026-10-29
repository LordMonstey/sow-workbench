#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

Monstey_App::Monstey_App(HINSTANCE module, std::filesystem::path nativeSchema, std::filesystem::path settings)
    : instance(module), runtime(std::make_unique<Monstey_Runtime>(nativeSchema, settings)),
      schema(std::move(nativeSchema))
{
}
Monstey_App::~Monstey_App()
{
    closing = true;
    if (worker.joinable())
    {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            jobs.clear();
        }
        condition.notify_all();
        worker.join();
    }
    if (modelPreview)
        modelPreview->Close();
    if (texturePreview)
        texturePreview->Close();
    if (window && IsWindow(window))
        DestroyWindow(window);
    window = nullptr;
    if (font)
    {
        DeleteObject(font);
        font = nullptr;
    }
}

bool Monstey_App::Monstey_DatabaseForm() const
{
    return tab == 0 && databaseNativeMode;
}

bool Monstey_App::Monstey_OperationForm() const
{
    return tab >= 4 || Monstey_DatabaseForm();
}

void Monstey_App::StartWorker()
{
    const auto replyWindow = window;
    worker = std::thread(
        [this, replyWindow]
        {
            for (;;)
            {
                Monstey_Job job;
                {
                    std::unique_lock lock(mutex);
                    condition.wait(lock, [this] { return stopping || !jobs.empty(); });
                    if (stopping)
                        break;
                    job = std::move(jobs.front());
                    jobs.pop_front();
                    active = true;
                    currentOperation = job.request.at("operation").as_string();
                    if (job.previewLoad)
                        ++previewLoadsStarted;
                }
                Monstey_Result result;
                result.job = std::move(job);
                if (result.job.previewLoad)
                {
                    try
                    {
                        const auto &load = *result.job.previewLoad;
                        if (load.bindScene)
                        {
                            const auto animationBytes = Monstey_ReadFile(load.animation);
                            const auto animationHash = Monstey_HashSha256(animationBytes);
                            if (!load.expectedAnimationHash.empty() && animationHash != load.expectedAnimationHash)
                                throw Monstey_Error("source_hash_mismatch",
                                                    "Animation changed since it was opened; reopen the animation file");
                            const auto skeletonBytes = Monstey_ReadFile(load.skeleton);
                            const auto description = Monstey_Anix(animationBytes).Describe();
                            for (const auto &clip : description.at("clips").as_array())
                                result.previewClips.as_array().push_back(
                                    J::object({{"name", clip.at("name")},
                                               {"flags", clip.at("flags")},
                                               {"sample_supported", clip.at("sample_supported")}}));
                            result.previewBinding = std::make_shared<Monstey_AnimationPreviewBinding>(
                                Monstey_AnimationPreviewBinding::Prepare(*load.bindScene, skeletonBytes, animationBytes,
                                                                         load.clip.empty() ? "bind" : load.clip));
                            result.previewFrame =
                                std::make_unique<Monstey_AnimationPreviewFrame>(result.previewBinding->CreateFrame());
                            if (Monstey_HashSha256(load.animation) != animationHash ||
                                Monstey_HashSha256(load.skeleton) != load.bindScene->skeleton_sha256)
                                throw Monstey_Error("source_changed", "Animation preview source changed while loading");
                        }
                        else
                        {
                            result.previewScene = std::make_shared<Monstey_ModelScene>(
                                Monstey_ModelReadScene(load.source, load.skeleton));
                        }
                    }
                    catch (const std::exception &e)
                    {
                        result.previewError = e.what();
                    }
                    std::unique_lock lock(mutex);
                    ++previewLoadsCompleted;
                    if (result.job.previewLoad->holdForCloseTest && (result.previewScene || result.previewBinding))
                    {
                        previewCloseTestReady = true;
                        condition.wait(lock, [this] { return stopping; });
                    }
                }
                else
                {
                    result.reply = runtime->Call(result.job.request);
                }
                {
                    std::lock_guard lock(mutex);
                    active = false;
                    currentOperation.clear();
                    results.push_back(std::move(result));
                }
                PostMessageW(replyWindow, Monstey_ReplyMessage, 0, 0);
            }
            PostMessageW(replyWindow, Monstey_WorkerStopped, 0, 0);
        });
}

void Monstey_App::Queue(std::string operation, J arguments, std::function<void(const J &)> callback,
                        bool checkGeneration, bool coalesceReads)
{
    if (closing)
        return;
    if (!arguments.is_object())
        arguments = J::object();
    if (operation.starts_with("fx_") && arguments.contains("track") && arguments.at("track").is_string() &&
        arguments.at("track").as_string().empty())
        arguments.as_object().erase("track");
    if (checkGeneration && !arguments.contains("expected_generation"))
        arguments["expected_generation"] = generation;
    Monstey_Job job{J::object({{"operation", operation}, {"arguments", arguments}}), epoch, std::move(callback)};
    job.coalesceReads = coalesceReads;
    static const auto catalog = Monstey_Runtime::Catalog();
    job.originTab = (operation == "open_database" || operation.starts_with("database_") ||
                     operation.starts_with("model_socket") || operation.starts_with("animated_extra"))
                        ? 0
                    : operation.starts_with("animation_") || operation.starts_with("material_") ? 4
                    : operation.starts_with("fx_")                                              ? 3
                                                                                                : tab;
    for (const auto &descriptor : catalog.as_array())
        if (descriptor.at("name").as_string() == operation)
            job.mutation = !descriptor.at("read_only").as_bool();
    {
        std::lock_guard lock(mutex);
        if (stopping)
            return;
        if (job.coalesceReads && !job.mutation && operation != "database_metadata" && operation != "fx_record")
            std::erase_if(jobs,
                          [&](const auto &queued)
                          {
                              return queued.coalesceReads && !queued.mutation && queued.epoch == job.epoch &&
                                     queued.request.at("operation") == job.request.at("operation");
                          });
        jobs.push_back(std::move(job));
    }
    condition.notify_one();
    SetStatus(L"Working: " + Monstey_Wide(operation) + L" ...");
}

void Monstey_App::PumpResults()
{
    std::deque<Monstey_Result> ready;
    {
        std::lock_guard lock(mutex);
        ready.swap(results);
    }
    for (auto &result : ready)
    {
        if (result.job.previewLoad)
        {
            // Scene completion has no Runtime session reply. It is independent of tab epochs.
            const auto &load = *result.job.previewLoad;
            if (!closing)
            {
                if (const auto target = load.target.lock();
                    target && (load.bindScene ? target->AcceptsAnimation(load.request) : target->Accepts(load.request)))
                {
                    if (load.bindScene)
                    {
                        if (result.previewBinding && result.previewFrame && result.previewError.empty())
                            target->SetAnimation(load.request, std::move(result.previewBinding),
                                                 std::move(result.previewFrame), result.previewClips, load.animation);
                        else
                            target->SetAnimationError(load.request, result.previewError.empty()
                                                                        ? "Native animation preview load failed"
                                                                        : result.previewError);
                    }
                    else if (result.previewScene)
                        target->SetScene(load.request, std::move(result.previewScene));
                    else
                        target->SetError(load.request, result.previewError.empty() ? "Native model load failed"
                                                                                   : result.previewError);
                }
                else if (smokePreview)
                {
                    smoke["preview_stale_results_rejected"] =
                        smoke.value("preview_stale_results_rejected", 0u).as_uint() + 1;
                }
            }
            continue;
        }
        generation = result.reply.at("generation").as_uint();
        revision = result.reply.at("revision").as_uint();
        if (smokePreview && result.job.request.at("operation").as_string() == "strings_new" &&
            result.reply.at("ok").as_bool())
        {
            smoke["preview_session_generation"] = generation;
            smoke["preview_session_revision"] = revision;
        }
        if (closing)
            continue;
        const auto animationOperation = result.job.request.at("operation").as_string();
        const auto animationReplacement = animationOperation == "animation_open" ||
                                          animationOperation == "animation_new" ||
                                          animationOperation == "animation_load_project";
        if (animationReplacement && result.job.epoch == animationOpenEpoch)
            animationOpening = false;
        if ((animationOperation == "material_open" || animationOperation == "material_load_project") &&
            result.job.epoch == materialOpenEpoch)
            materialOpening = false;
        if (result.job.epoch != epoch)
        {
            if (Monstey_SowAtomSmoke && animationOperation == "database_atom_uses")
                smoke["sow_atom_stale_callbacks_discarded"] =
                    smoke.value("sow_atom_stale_callbacks_discarded", 0u).as_uint() + 1;
            if (!smokeReport.empty() && smokeDomain == 0 && !smokeOperation.empty() && !result.job.mutation)
            {
                auto &discarded = smoke["native_database_discarded_callbacks"];
                if (!discarded.is_object())
                    discarded = J::object();
                discarded[animationOperation] = discarded.value(animationOperation, 0u).as_uint() + 1;
            }
            if (result.job.mutation && result.reply.at("ok").as_bool())
            {
                const auto &value = result.reply.at("result");
                if (animationOperation.starts_with("material_") && value.contains("material"))
                {
                    MaterialAccept(value);
                }
                else if (animationOperation.starts_with("animation_") && value.contains("dirty"))
                {
                    animationView = value;
                    animationSource = Monstey_Wide(value.at("source").as_string());
                    if (!materialMode)
                        UpdateState(value, 4);
                    if (tab == 4 && animationMode && !animationOpening)
                        AnimationLoadRows();
                }
                else
                {
                    UpdateState(value, result.job.originTab);
                    if (tab == 4 && animationMode && !animationOpening && !animationView.empty() &&
                        !animationOperation.starts_with("animation_"))
                        AnimationLoadRows();
                }
            }
            continue;
        }
        if (result.job.callback)
            result.job.callback(result.reply);
        else if (!result.reply.at("ok").as_bool())
            Error(result.reply.at("error").at("message").as_string());
        if (!closing && result.job.mutation && result.reply.at("ok").as_bool() &&
            !animationOperation.starts_with("animation_") && tab == 4 && animationMode && !animationOpening &&
            !animationView.empty())
            AnimationLoadRows();
        if (!closing && result.job.mutation && result.reply.at("ok").as_bool() &&
            !animationOperation.starts_with("material_") && tab == 4 && materialMode && !materialOpening &&
            !materialView.empty())
            MaterialLoadRows();
    }
    if (!active.load())
        SetStatus(L"Ready. Sources remain unchanged; builds create new files.");
}

void Monstey_App::Close()
{
    if (closing)
        return;
    if (smokeReport.empty() &&
        (std::any_of(std::begin(dirty), std::end(dirty), [](bool value) { return value; }) ||
         (!materialView.empty() && materialView.at("dirty").as_bool()) ||
         (!animationView.empty() && animationView.at("dirty").as_bool())) &&
        MessageBoxW(window, L"Discard unsaved project edits and close?", L"Close Workbench",
                    MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return;
    closing = true;
    ++epoch;
    EnableWindow(window, FALSE);
    if (editor && editor->window)
        editor->Close();
    if (modelPreview)
        modelPreview->Close();
    if (texturePreview)
        texturePreview->Close();
    SetStatus(L"Finishing the current operation before closing ...");
    {
        std::lock_guard lock(mutex);
        stopping = true;
        jobs.clear();
    }
    condition.notify_all();
}

void Monstey_App::UpdateState(const J &value, int targetTab)
{
    const auto index = targetTab < 0 ? tab : targetTab;
    if (value.is_object())
    {
        if (index == 0 && value.contains("format") && value.contains("version"))
            Monstey_SowAtomGadb17 = value.at("format") == J("GADB17") && value.at("version") == J(17);
        if (value.contains("dirty"))
            dirty[index] = value.at("dirty").as_bool();
        if (index == 2 && value.contains("annotations_dirty"))
            dirty[index] = dirty[index] || value.at("annotations_dirty").as_bool();
        if (value.contains("can_undo"))
            canUndo[index] = value.at("can_undo").as_bool();
        if (value.contains("can_redo"))
            canRedo[index] = value.at("can_redo").as_bool();
        if (value.contains("source") && value.at("source").is_string())
            sourceNames[index] = value.at("source").as_string().empty() && (index == 0 || index == 3)
                                     ? (index == 0 ? L"Untitled database" : L"Untitled effect")
                                     : Monstey_Wide(value.at("source").as_string());
        if ((index == 1 || index == 2) && value.contains("source") && value.at("source").is_null())
            sourceNames[index] = index == 1 ? L"Untitled archive" : L"Untitled strings";
    }
    if (index != tab)
    {
        if (index == 0 && tab == 4 && socketMode)
            UpdateButtons();
        return;
    }
    const auto title = sourceNames[index].empty() ? L"No source opened" : sourceNames[index];
    SetWindowTextW(pathLabel, (title + (dirty[index] ? L"  [modified]" : L"")).c_str());
    UpdateButtons();
}

} // namespace Monstey::Monstey_AppInternal
