#pragma once

#include "Monstey_AppSupport.hpp"
#include "Monstey_Anix.hpp"
#include "Monstey_ModelPreviewWindow.hpp"
#include <functional>
#include <memory>

namespace Monstey::Monstey_AppInternal
{

struct Monstey_PreviewLoadRequest
{
    std::filesystem::path source, skeleton;
    std::weak_ptr<Monstey_ModelPreviewWindow> target;
    std::uint64_t request = 0;
    bool holdForCloseTest = false;
    std::filesystem::path animation;
    std::string clip, expectedAnimationHash;
    std::shared_ptr<const Monstey_ModelScene> bindScene;
};

struct Monstey_Job
{
    J request;
    std::uint64_t epoch = 0;
    std::function<void(const J &)> callback;
    bool mutation = false;
    bool coalesceReads = true;
    int originTab = 0;
    std::shared_ptr<const Monstey_PreviewLoadRequest> previewLoad;
};

struct Monstey_Result
{
    Monstey_Job job;
    J reply;
    std::shared_ptr<const Monstey_ModelScene> previewScene;
    std::string previewError;
    std::shared_ptr<const Monstey_AnimationPreviewBinding> previewBinding;
    std::unique_ptr<Monstey_AnimationPreviewFrame> previewFrame;
    J previewClips = J::array();
};

} // namespace Monstey::Monstey_AppInternal
