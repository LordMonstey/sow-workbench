#pragma once
#include "Monstey_FearAudit.hpp"
namespace Monstey {
// Decodes explicit native reflection entries and their actual parent chain.
// The PE snapshot is read-only; no registry scan, native execution or guessed class registration occurs.
Monstey_Json Monstey_FxReflectCategories(std::span<const std::uint8_t> image,
                                         std::span<const std::uint64_t> categoryEntries,
                                         std::string_view expectedImageSha256 = {});
} // namespace Monstey
