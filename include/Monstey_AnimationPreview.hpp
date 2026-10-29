#pragma once
#include "Monstey_Anix.hpp"
#include "Monstey_Model.hpp"
#include <memory>

namespace Monstey
{
struct Monstey_AnimationPreviewPose
{
    std::array<float, 3> position_cm{};
    std::array<float, 4> quaternion_xyzw{0, 0, 0, 1};
};
class Monstey_AnimationPreviewFrame
{
  public:
    Monstey_AnimationPreviewFrame(Monstey_AnimationPreviewFrame &&) noexcept;
    Monstey_AnimationPreviewFrame &operator=(Monstey_AnimationPreviewFrame &&) noexcept;
    ~Monstey_AnimationPreviewFrame();
    const Monstey_ModelScene &Scene() const;
    std::span<const Monstey_AnimationPreviewPose> LocalPose() const;
    std::span<const Monstey_AnimationPreviewPose> GlobalPose() const;
    std::span<const Monstey_AnimationPreviewPose> SkinTransforms() const;
    double Tick() const;
    std::uint64_t Revision() const;
    Monstey_Json Inspect() const;

  private:
    struct Monstey_State;
    std::unique_ptr<Monstey_State> state_;
    explicit Monstey_AnimationPreviewFrame(std::unique_ptr<Monstey_State> state);
    friend class Monstey_AnimationPreviewBinding;
};
// Immutable, exact same-rig binding. It owns its source/library snapshots.
// A frame belongs to its binding; failed evaluation leaves its published data unchanged.
class Monstey_AnimationPreviewBinding
{
  public:
    static Monstey_AnimationPreviewBinding Prepare(Monstey_ModelScene source,
                                                   std::span<const std::uint8_t> explicit_skel,
                                                   std::span<const std::uint8_t> anix, std::string clip);
    Monstey_AnimationPreviewFrame CreateFrame() const;
    void Evaluate(double milliseconds, Monstey_AnimationPreviewFrame &frame) const;
    const Monstey_ModelScene &Source() const;
    Monstey_Json Inspect() const;

  private:
    friend class Monstey_AnimationPreviewFrame;
    struct Monstey_State;
    std::shared_ptr<const Monstey_State> state_;
    explicit Monstey_AnimationPreviewBinding(std::shared_ptr<const Monstey_State> state);
};
} // namespace Monstey
