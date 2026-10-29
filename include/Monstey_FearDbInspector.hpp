#pragma once
#include "Monstey_Core.hpp"
#include "Monstey_FearDbPacked.hpp"
#include <optional>
namespace Monstey {
struct Monstey_FearInspectorOptions {
    std::optional<std::uint32_t> category, record, attribute;
    std::uint32_t offset = 0, limit = 50, string_bytes = 256;
    bool summary = false;
};
// Browses only the accepted owned packed runtime image. Never parses a second graph.
Monstey_Json Monstey_FearInspectPackedImage(const Monstey_FearDbPackedImage& image,
    const Monstey_FearInspectorOptions& options);
}
