#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace Monstey {
struct Monstey_LtaDecodeObservation{
 std::string status;std::uint64_t consumed_bits=0,literals=0,spans=0,span_bytes=0;
 std::uint32_t unavailable_window_index=0;std::vector<std::uint8_t> decoded;
};
// File-contained diagnostic profile of native4AE560/4AE650/4AE8F0 grammar.
// No original buffer/stream/exception ABI; malformed short-bit failure behavior
// is reported explicitly, not claimed equal to ignored native secondary reads.
// Initial ring bytes are unknown: only a previously written byte can be consumed.
Monstey_LtaDecodeObservation Monstey_ObserveLtaCompressed(std::span<const std::uint8_t>,std::uint32_t output_budget=64u*1024u*1024u);
}
