#pragma once
#include <windows.h>
#include "Monstey_ModelPreview.hpp"
#include "Monstey_AnimationPreview.hpp"
#include <functional>
#include <memory>
namespace Monstey {
struct Monstey_PreviewAnimationRequest {
    std::filesystem::path animation, skeleton;
    std::string clip, expected_anix_sha256;
    std::shared_ptr<const Monstey_ModelScene> source;
    std::uint64_t request = 0;
};
// UI-thread owned. Source I/O and binding preparation use the App's owned worker.
class Monstey_ModelPreviewWindow {
public:
    Monstey_ModelPreviewWindow();
    ~Monstey_ModelPreviewWindow();
    Monstey_ModelPreviewWindow(const Monstey_ModelPreviewWindow&) = delete;
    Monstey_ModelPreviewWindow& operator=(const Monstey_ModelPreviewWindow&) = delete;
    void Create(HWND owner, int show = SW_SHOWNORMAL);
    HWND Handle() const;
    std::uint64_t BeginLoad(const std::filesystem::path& source, const std::filesystem::path& skeleton = {});
    bool Accepts(std::uint64_t request) const;
    bool SetScene(std::uint64_t request, std::shared_ptr<const Monstey_ModelScene> scene);
    bool SetError(std::uint64_t request, std::string_view message);
    void SetAnimationLoader(std::function<void(const Monstey_PreviewAnimationRequest&)> loader);
    void RequestAnimation(const std::filesystem::path& animation, std::string clip = {});
    bool AcceptsAnimation(std::uint64_t request) const;
    bool SetAnimation(std::uint64_t request,
        std::shared_ptr<const Monstey_AnimationPreviewBinding> binding,
        std::unique_ptr<Monstey_AnimationPreviewFrame> frame,
        const Monstey_Json& clips, const std::filesystem::path& animation);
    bool SetAnimationError(std::uint64_t request, std::string_view message);
    bool PlayAnimation(bool playing);
    bool SeekAnimation(double milliseconds);
    bool AdvanceAnimationClock(std::uint32_t now);
    void Close();
    bool RouteMessage(MSG& message);
    Monstey_Json Inspect() const;
    Monstey_Bytes Snapshot();
private:
    struct Monstey_State;
    std::unique_ptr<Monstey_State> state_;
};
}