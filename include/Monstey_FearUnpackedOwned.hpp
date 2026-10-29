#pragma once
#include "Monstey_FearDbInspector.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace Monstey {
struct Monstey_FearUnpackedOwnedOptions {
    std::uint32_t arena_budget_bytes = 64u * 1024u * 1024u;
    // Explicit owned profile values. Neither is a historical observation or zero fallback.
    std::uint32_t callback_opaque_word = 0x4D535459u;
    std::uint32_t insert_opaque_word = 0x554E504Bu;
};
// Diagnostic signatures only. No decoding or .s editor-metadata routing.
Monstey_Json Monstey_FearUnpackedTextProfile(std::span<const std::uint8_t>);
class Monstey_FearUnpackedOwnedFault final : public std::runtime_error {
  public:
    std::string code;
    Monstey_Json details;
    Monstey_FearUnpackedOwnedFault(std::string code, std::string message,
                                   Monstey_Json details = Monstey_Json::object());
};
// One bounded native32 arena. Output never points into the input spans. All storage
// belongs to this RAII result; numeric identities are not host pointers.
struct Monstey_FearUnpackedOwnedImage {
    std::uint32_t arena_base = 0, arena_used = 0, root = 0, root_allocation_bytes = 0,
                  pool_begin = 0, pool_end = 0;
    std::array<std::uint32_t, 3>
        table_counts{}; // actually allocated category/record/attribute tables
    std::vector<std::uint8_t> arena;
    Monstey_Json profile;
    Monstey_FearDbMemory Memory() const {
        return {arena_base, std::span<const std::uint8_t>(arena).first(arena_used)};
    }
};
// Concrete frozen Open+C2+line+callback+M2+cleanup+validator composition. Primitive
// binding is a declared owned MSVC2026 C-locale/logical-memory profile, not original
// CRT/STL/allocator/x87 ABI/history. Input spans must remain stable for this call.
// Budget:256KiB..1GiB, default64MiB. Exhaustion throws a precise OwnedFault; no4GiB
// arena, wrap-to-small allocation, heuristic parser or second graph is substituted.
Monstey_FearUnpackedOwnedImage
Monstey_FearLoadUnpackedOwned(std::span<const std::uint8_t> text,
                              std::span<const std::uint8_t> constraints,
                              const Monstey_FearUnpackedOwnedOptions &options = {});
// Same public ordinal/page options as the packed inspector, with the actual
// unpacked pool-after-graph range. Uses accepted typed native-layout getters.
Monstey_Json Monstey_FearInspectUnpackedOwned(const Monstey_FearUnpackedOwnedImage &,
                                              const Monstey_FearInspectorOptions &);
} // namespace Monstey
