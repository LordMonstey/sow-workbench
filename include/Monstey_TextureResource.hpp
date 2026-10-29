#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <memory>
namespace Monstey {
struct Monstey_TextureSubresource {
  std::uint32_t array_slice = 0, cube_face = 0, mip = 0;
  std::uint32_t width = 0, height = 0, depth = 1;
  std::uint64_t offset = 0, bytes = 0, row_pitch = 0, slice_pitch = 0;
};
struct Monstey_TextureImage {
  std::uint32_t width = 0, height = 0;
  std::vector<std::array<float, 4>> rgba;
};
struct Monstey_TextureEdit {
  std::size_t subresource = 0;
  Monstey_Bytes bytes;
};
// Owns encoded bytes. Unknown fields are preserved; no header/semantic
// normalization.
class Monstey_TextureResource {
public:
  explicit Monstey_TextureResource(std::span<const std::uint8_t> source);
  explicit Monstey_TextureResource(const std::filesystem::path &source,
                                   std::string_view expected_sha256 = {});
  ~Monstey_TextureResource();
  Monstey_TextureResource(const Monstey_TextureResource &);
  Monstey_TextureResource &operator=(const Monstey_TextureResource &);
  Monstey_TextureResource(Monstey_TextureResource &&) noexcept;
  Monstey_TextureResource &operator=(Monstey_TextureResource &&) noexcept;
  Monstey_Json Describe() const;
  std::vector<Monstey_TextureSubresource> Subresources() const;
  Monstey_Bytes EncodedSubresource(std::size_t index) const;
  Monstey_Bytes Build() const;
  void Replace(std::size_t index, std::span<const std::uint8_t> encoded);
  void Edit(std::span<const Monstey_TextureEdit> edits);
  // Raw decoded numeric components; no implicit gamma, normal reconstruction or
  // tone map.
  Monstey_TextureImage Decode(std::size_t index, std::uint32_t depth_slice = 0,
                              std::uint64_t maximum_pixels = 16777216) const;
  // New offline output; exclusive Core publisher and exact readback/source
  // guards.
  Monstey_Json Build(const std::filesystem::path &new_output) const;

private:
  struct Monstey_Impl;
  std::unique_ptr<Monstey_Impl> impl_;
  Monstey_Impl &Monstey_State();
  const Monstey_Impl &Monstey_State() const;
};
} // namespace Monstey