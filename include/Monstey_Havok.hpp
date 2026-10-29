#pragma once
#include "Monstey_Core.hpp"
#include <memory>
#include <optional>
#include <utility>

namespace Monstey {
struct Monstey_HavokMember {
  std::string name;
  std::uint32_t flags = 0, offset = 0, type = 0;
};
struct Monstey_HavokType {
  std::string name;
  std::uint32_t parent = 0, flags = 0, subtype = 0, pointer = 0;
  std::uint32_t version = 0, bytes = 0, alignment = 0, abstract_value = 0,
                hash = 0;
  std::vector<std::pair<std::string, std::uint32_t>> templates;
  std::vector<Monstey_HavokMember> members;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> interfaces;
};
struct Monstey_HavokNode {
  std::uint32_t type = 0;
  Monstey_Json scalar;
  std::vector<std::size_t> elements;
  std::vector<std::pair<std::string, std::size_t>> fields;
  std::optional<std::size_t> target;
  Monstey_Bytes original_storage;
};
// Nodes own children by stable IDs rather than C++ pointers. Cyclic native
// pointer graphs therefore have finite ownership and preserve target sharing.
class Monstey_Havok {
public:
  explicit Monstey_Havok(std::span<const std::uint8_t> source);
  explicit Monstey_Havok(const std::filesystem::path &source);
  ~Monstey_Havok();
  Monstey_Havok(Monstey_Havok &&) noexcept;
  Monstey_Havok &operator=(Monstey_Havok &&) noexcept;
  const std::vector<Monstey_HavokType> &Types() const;
  std::uint32_t EffectiveType(std::uint32_t type) const;
  std::vector<Monstey_HavokMember> Members(std::uint32_t type) const;
  std::size_t Root() const;
  std::size_t NodeCount() const;
  const Monstey_HavokNode &Node(std::size_t id) const;
  Monstey_HavokNode &Node(std::size_t id);
  std::size_t Field(std::size_t id, std::string_view name) const;
  std::size_t Dereference(std::size_t id) const;
  std::vector<std::size_t> Objects(std::string_view type_name) const;
  std::size_t New(std::uint32_t type, const Monstey_Json &plain);
  std::size_t Clone(std::size_t id);
  void Replace(std::size_t id, const Monstey_Json &plain);
  Monstey_Json Plain(std::size_t id) const;
  Monstey_Json Canonical(std::optional<std::size_t> root = {}) const;
  Monstey_Json Describe() const;
  Monstey_Json TypeHashCoverage() const;
  Monstey_Json Audit() const;
  Monstey_Bytes Encode(bool force_rebuild = false) const;
  const Monstey_Bytes &Original() const;
  // Donor must have identical SDK and every TYPE subsection except THSH.
  // Existing nonzero hashes must agree. DATA and INDX remain byte exact.
  static Monstey_Bytes CompleteTypeHashes(std::span<const std::uint8_t> source,
                                          std::span<const std::uint8_t> donor);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace Monstey
