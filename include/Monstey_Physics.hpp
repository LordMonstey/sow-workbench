#pragma once
#include "Monstey_Core.hpp"
#include <memory>
#include <optional>

namespace Monstey {
// Exact raw OBJRES.WorldPhysics payload. OBJRES owner editing stays in World.
class Monstey_Physics {
public:
  explicit Monstey_Physics(std::span<const std::uint8_t> bytes);
  ~Monstey_Physics();
  Monstey_Physics(Monstey_Physics &&) noexcept;
  Monstey_Physics &operator=(Monstey_Physics &&) noexcept;
  Monstey_Physics(const Monstey_Physics &) = delete;
  Monstey_Physics &operator=(const Monstey_Physics &) = delete;
  Monstey_Json Describe(std::size_t offset = 0, std::size_t limit = 250) const;
  Monstey_Bytes Encode() const;
  std::uint16_t DefinitionFor(std::uint32_t owner_hash) const;
  void RenameMapping(std::uint32_t before, std::uint32_t after);
  std::uint16_t CloneMapping(std::uint32_t donor_hash, std::uint32_t new_hash);
  std::uint16_t ImportMapping(const Monstey_Physics &donor,
                              std::uint32_t donor_hash, std::uint32_t new_hash);
  void SetTransform(
      std::size_t shape, const Monstey_Json &position_cm,
      const Monstey_Json &quaternion_xyzw,
      const std::optional<std::vector<std::size_t>> &route = std::nullopt);
  void ResizeBox(std::size_t shape, const Monstey_Json &half_extents_cm,
                 const std::vector<std::size_t> &route = {});

  Monstey_Bytes ShapeBytes(std::size_t shape,
                           const std::vector<std::size_t> &route = {}) const;
  Monstey_Bytes RigidTransformBytes(std::size_t shape) const;
  void ReplaceShape(std::size_t shape, std::span<const std::uint8_t> bytes,
                    const std::vector<std::size_t> &route = {});

private:
  struct Monstey_Impl;
  std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
