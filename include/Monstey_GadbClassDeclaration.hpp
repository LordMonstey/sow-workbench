#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Read-only, explicit-pointer declaration view. Duplicate full names remain
// separate class identities. Raw class+30 is retained and is never invoked or
// used as a declaration-shape discriminator. Only file-backed bytes are read;
// a PE virtual-only tail is reported as unavailable, never synthesized as zero.
// Parent links are followed by pointer, with bounded cycle/depth/row checks.
Monstey_Json
Monstey_GadbInspectClassPointers(std::span<const std::uint8_t> image,
                                 std::span<const std::uint64_t> class_pointers,
                                 std::string_view expected_sha256 = {});
} // namespace Monstey
