#pragma once
#include "Monstey_FearDbPrimitives.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace Monstey {
// Concrete owned byte input. On a short read it zeros the requested destination,
// leaves position unchanged, and increments read_calls. That cursor policy is
// this span adapter's policy, not a claim about every original ILTInStream.
struct Monstey_FearDbPackedInput {
 std::span<const std::uint8_t> bytes;
 std::size_t position=0;
 std::uint64_t read_calls=0;
};
// Single original-layout allocation represented with native32 virtual identities.
// Zero-filled never-written allocation bytes are owned placeholders, not evidence
// of the original allocator's byte contents. No original service object is built.
struct Monstey_FearDbPackedImage {
 std::uint32_t base=0, database=0, used_bytes=0;
 std::vector<std::uint8_t> allocation;
 Monstey_FearDbMemory Memory() const {return {base,allocation};}
 Monstey_FearDbMutableMemory MutableMemory() {return {base,allocation};}
};
enum class Monstey_FearDbPackedStatus { loaded,header_read_failed,invalid_header,pool_read_failed,graph_read_failed };
struct Monstey_FearDbPackedLoad {
 Monstey_FearDbPackedStatus status=Monstey_FearDbPackedStatus::header_read_failed;
 std::uint32_t allocation_bytes=0;
 std::optional<Monstey_FearDbPackedImage> image;
};
// 100039A0 plus 10003680..1000399D and 10006E80..10007018.
// Name is live borrowed bytes: content may change as the input fields advance.
// Allocation uses the after-header NUL extent; graph reservation/copy rescans
// AFTER a successful root-count Read. Each consumed extent must contain a NUL.
// This private loader permits empty names; original public OpenNew filters them.
// Native allocation overflow/out-of-owned-range execution is not fabricated:
// consumed invalid owned ranges throw Monstey_FearDbFault and RAII frees storage.
// Allocation failure uses the owned C++ allocator exception contract, not original
// malloc return/exception ABI. Stream ownership remains with the caller.
Monstey_FearDbPackedLoad Monstey_FearDbLoadPacked(Monstey_FearDbPackedInput& input,std::span<const std::uint8_t> name,std::uint32_t allocation_base);
} // namespace Monstey
