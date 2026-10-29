#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <span>
#include <map>
#include <optional>
#include <string>
namespace Monstey {
struct Monstey_WorldRead {
    Monstey_Bytes bytes;
    std::uint32_t error{};
    bool complete{};
};
class Monstey_WorldMemory {
public:
    virtual ~Monstey_WorldMemory() = default;
    virtual Monstey_WorldRead Read(std::uint64_t address, std::size_t bytes) = 0;
};
struct Monstey_WorldLimits {
    std::size_t scenes{512}, nodes{8192}, groups{4096}, links_per_node{512}, culls_per_group{1024};
    std::size_t read_bytes{8 * 1024 * 1024}, read_calls{30000};
};
// Snapshot observations are bounded and non-atomic. No native functions are called.
// Repeated reads detect observed changes; unchanged bytes do not prove concurrent lifetime.
Monstey_Json Monstey_WorldRuntimeSnapshot(Monstey_WorldMemory& memory,
    std::uint64_t image_base, const std::array<float, 3>& point,
    std::uint32_t target_hash, const Monstey_WorldLimits& limits = {});

// Neutral owned observations for only the selected C2FDC0 -> C32CA0 call gate.
// No manager/region class, producer association or native capacity is inferred.
struct Monstey_HkaiActivationRow {
    std::uint64_t address{}, value_50{};
    std::optional<std::uint32_t> identity_3c, key_30;
    std::uint8_t value_48{}, value_49{};
    std::optional<std::uint8_t> value_64;
    std::optional<Monstey_Bytes> name_at_10;
};
struct Monstey_HkaiActivationResource {
    std::uint64_t address{};
    std::uint32_t identity_18{};
    std::optional<std::uint64_t> table_78;
    std::optional<std::uint32_t> count_80;
    std::optional<std::uint64_t> string_slots;
};
struct Monstey_HkaiActivationEntry {
    std::optional<std::uint64_t> descriptor;
    std::optional<std::uint32_t> hash_1c;
    std::optional<std::int32_t> index_18;
};
struct Monstey_HkaiActivationName {
    std::optional<std::uint64_t> tagged_pointer;
    std::optional<Monstey_Bytes> bytes;
};
// A caller-supplied read-safety range, not evidence of native allocation capacity.
struct Monstey_HkaiStringExtent {
    std::uint64_t begin{};
    std::uint32_t slots{};
};
struct Monstey_HkaiActivationInput {
    std::string image_sha256;
    std::optional<Monstey_HkaiActivationRow> row;
    std::optional<Monstey_HkaiActivationResource> resource;
    std::map<std::uint32_t, Monstey_HkaiActivationEntry> entries;
    std::map<std::uint32_t, Monstey_HkaiActivationName> names;
    Monstey_HkaiStringExtent string_extent;
};
struct Monstey_HkaiActivationLimits {
    std::uint32_t table_entries{65536}, string_slots{65536}, name_bytes{1024};
    Monstey_WorldLimits capture;
};
// Pure owned-input predicate. Missing observations are UNKNOWN; no input is changed.
Monstey_Json Monstey_WorldHkaiActivationGate(const Monstey_HkaiActivationInput &input);
// Caller owns session/image qualification. No default addresses, scans or original calls.
// Only native lower-bound visited entries are read; table sortedness is not verified.
Monstey_Json Monstey_WorldHkaiActivationSnapshot(Monstey_WorldMemory &memory,
    std::uint64_t row_address, std::uint64_t resource_address, std::string_view image_sha256,
    const Monstey_HkaiStringExtent &string_extent, const Monstey_HkaiActivationLimits &limits = {});
// Observe only the published receiver chain qualified by c06+d30+eba0 evidence.
// image_sha256 is caller-supplied qualification, not process/session authentication.
// Capture and repeated-region verification are bounded and non-atomic; no lifetime
// is retained. No class name, row/resource discovery or initial activation follows.
Monstey_Json Monstey_WorldPublishedReceiverSnapshot(Monstey_WorldMemory &memory,
    std::uint64_t image_base, std::string_view image_sha256,
    const Monstey_WorldLimits &limits = {});
// The two C2FDC0 row-pointer arrays are distinct. This observes their raw
// pointer values only; it never dereferences rows or executes a call gate.
struct Monstey_WorldReceiverRowLimits {
    std::uint32_t rows_per_vector{4096};
    Monstey_WorldLimits capture;
};
// Caller qualifies the receiver/session/profile. Counts are read-safety limits,
// not native capacities. Captured regions are rechecked without retargeting.
Monstey_Json Monstey_WorldReceiverRowsSnapshot(Monstey_WorldMemory &memory,
    std::uint64_t receiver_address, std::string_view image_sha256,
    const Monstey_WorldReceiverRowLimits &limits = {});
}