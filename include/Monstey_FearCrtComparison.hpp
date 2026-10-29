#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
namespace Monstey {
// Numeric x86 identities into caller-owned stable memory. No host-pointer/native ABI claim.
// Regions describe one consistent byte-address space. Consumed operands may span
// adjacent supplied regions. A genuine hole or 32-bit operand overflow faults at
// the starting address with the original operand width; no whole-image padding.
struct Monstey_CrtRegion { std::uint32_t base; std::span<const std::uint8_t> bytes; };
struct Monstey_CrtMemory { std::span<const Monstey_CrtRegion> regions; };
class Monstey_CrtFault : public std::runtime_error {
public:
 std::uint32_t address, width;
 Monstey_CrtFault(std::uint32_t address, std::uint32_t width);
};
enum class Monstey_CrtCallSite { Left, Right, Direct };
enum class Monstey_CrtBoundary { GetPtd, RefreshLocale, InitialMapScratch, LcMapStringA };
class Monstey_CrtBoundaryFault : public std::runtime_error {
public:
 Monstey_CrtBoundary boundary;
 explicit Monstey_CrtBoundaryFault(Monstey_CrtBoundary boundary);
};
struct Monstey_CrtMapRequest {
 std::uint32_t locale_id, flags;
 std::array<std::uint8_t, 3> source;
 std::uint32_t source_count, destination_capacity, code_page, strict;
 Monstey_CrtCallSite call_site;
 std::uint64_t iteration;
};
struct Monstey_CrtBoundaries {
 // Consumed external dependencies, UNQUALIFIED as historical CRT/TLS/Windows behavior.
 std::function<std::uint32_t()> get_ptd;
 std::function<std::uint32_t()> refresh_locale;
 // Every consumed map invocation gets explicit initial private scratch. Left and Right are
 // distinct frames; caller supplies any history it owns. No inferred stack-slot identity.
 std::function<std::array<std::uint8_t, 3>(Monstey_CrtCallSite, std::uint64_t)> initial_map_scratch;
 // Return bits match the x86 EAX count. May partially write the supplied three bytes.
 std::function<std::int32_t(const Monstey_CrtMapRequest&, std::span<std::uint8_t, 3>)> lc_map_string_a;
};
// _stricmp C-locale helper 7C3432EC. Reads right then left, returns only -1, 0 or +1.
std::int32_t Monstey_CrtStricmpC(Monstey_CrtMemory memory, std::uint32_t left, std::uint32_t right);
// The complete subset of 7C3522CE reachable from _stricmp: unsigned input 0..255.
// The separate classifier 7C3523B8 can only reach its table branch in this subset.
std::int32_t Monstey_CrtFoldByte(Monstey_CrtMemory memory, std::uint32_t locale,
 std::uint8_t input, Monstey_CrtBoundaries& boundaries,
 Monstey_CrtCallSite call_site = Monstey_CrtCallSite::Direct, std::uint64_t iteration = 0);
// _stricmp 7C3505DC. global_locale_slot is the numeric identity of the owned slot
// corresponding to native 7C38EF44. Default preserves its original numeric identity.
std::int32_t Monstey_CrtStricmp(Monstey_CrtMemory memory, std::uint32_t left, std::uint32_t right,
 Monstey_CrtBoundaries& boundaries, std::uint32_t global_locale_slot = 0x7C38EF44u);
}
