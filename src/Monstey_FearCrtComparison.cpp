#include "Monstey_FearCrtComparison.hpp"
#include <bit>
namespace Monstey {
Monstey_CrtFault::Monstey_CrtFault(std::uint32_t at, std::uint32_t count)
 : std::runtime_error("Consumed CRT owned-memory range unavailable"), address(at), width(count) {}
Monstey_CrtBoundaryFault::Monstey_CrtBoundaryFault(Monstey_CrtBoundary value)
 : std::runtime_error("Consumed CRT boundary UNQUALIFIED and unavailable"), boundary(value) {}
namespace {
std::uint32_t Monstey_CrtRead(Monstey_CrtMemory memory, std::uint32_t at, std::uint32_t width) {
 // Preserve the first complete-region fast path and its existing precedence.
 for (const auto& region : memory.regions) {
  if (at < region.base) continue;
  const auto offset = static_cast<std::uint64_t>(at) - region.base;
  if (static_cast<std::uint64_t>(at) + width > 0x100000000ull ||
      offset > region.bytes.size() || width > region.bytes.size() - static_cast<std::size_t>(offset)) continue;
  std::uint32_t value = 0;
  for (std::uint32_t i = 0; i < width; ++i)
   value |= static_cast<std::uint32_t>(region.bytes[static_cast<std::size_t>(offset) + i]) << (i * 8u);
  return value;
 }
 // A native operand consumes consecutive byte addresses, even when supplied views
 // split that operand. A genuine hole faults at the original operand and width.
 if (static_cast<std::uint64_t>(at) + width > 0x100000000ull)
  throw Monstey_CrtFault(at, width);
 std::uint32_t value = 0;
 for (std::uint32_t i = 0; i < width; ++i) {
  const auto address = at + i;
  bool found = false;
  for (const auto& region : memory.regions) {
   if (address < region.base) continue;
   const auto offset = static_cast<std::uint64_t>(address) - region.base;
   if (offset >= region.bytes.size()) continue;
   value |= static_cast<std::uint32_t>(region.bytes[static_cast<std::size_t>(offset)]) << (i * 8u);
   found = true;
   break;
  }
  if (!found) throw Monstey_CrtFault(at, width);
 }
 return value;
}
std::uint32_t Monstey_CrtAsciiFold(std::uint32_t value) {
 return value >= 0x41u && value <= 0x5Au ? value + 0x20u : value;
}
}
std::int32_t Monstey_CrtStricmpC(Monstey_CrtMemory memory, std::uint32_t left, std::uint32_t right) {
 for (;;) {
  const auto raw_right = Monstey_CrtRead(memory, right, 1);
  ++right;
  const auto raw_left = Monstey_CrtRead(memory, left, 1);
  ++left;
  if (raw_left == raw_right) {
   if (raw_right == 0) return 0;
   continue;
  }
  const auto folded_right = Monstey_CrtAsciiFold(raw_right);
  const auto folded_left = Monstey_CrtAsciiFold(raw_left);
  if (folded_left == folded_right) continue;
  return folded_left < folded_right ? -1 : 1;
 }
}
std::int32_t Monstey_CrtFoldByte(Monstey_CrtMemory memory, std::uint32_t locale,
 std::uint8_t input, Monstey_CrtBoundaries& boundaries, Monstey_CrtCallSite call_site, std::uint64_t iteration) {
 const auto value = static_cast<std::uint32_t>(input);
 if (Monstey_CrtRead(memory, locale + 0x14u, 4) == 0 ||
     (Monstey_CrtRead(memory, locale + 0x24u, 4) != 0 && value <= 0x7Fu))
  return static_cast<std::int32_t>(Monstey_CrtAsciiFold(value));
 const auto table_mode = Monstey_CrtRead(memory, locale + 0x28u, 4);
 const auto table = Monstey_CrtRead(memory, locale + 0x48u, 4);
 const bool classifier = std::bit_cast<std::int32_t>(table_mode) > 1;
 // >1 calls the classifier and consumes a WORD; <=1 consumes only its low BYTE.
 const auto classification = Monstey_CrtRead(memory, table + value * 2u, classifier ? 2u : 1u);
 if ((classification & 1u) == 0) return static_cast<std::int32_t>(value);
 // Reload as the native fold does after its optional classifier call.
 const auto mapping_table = Monstey_CrtRead(memory, locale + 0x48u, 4);
 const bool double_byte = (Monstey_CrtRead(memory, mapping_table + 1u, 1) & 0x80u) != 0;
 const std::array<std::uint8_t, 3> source = double_byte
  ? std::array<std::uint8_t, 3>{0, input, 0} : std::array<std::uint8_t, 3>{input, 0, 0};
 const std::uint32_t source_count = double_byte ? 2u : 1u;
 // Native push order reads +04 before +14 at the map boundary.
 const auto code_page = Monstey_CrtRead(memory, locale + 0x04u, 4);
 const auto locale_id = Monstey_CrtRead(memory, locale + 0x14u, 4);
 if (!boundaries.initial_map_scratch) throw Monstey_CrtBoundaryFault(Monstey_CrtBoundary::InitialMapScratch);
 auto output = boundaries.initial_map_scratch(call_site, iteration);
 if (!boundaries.lc_map_string_a) throw Monstey_CrtBoundaryFault(Monstey_CrtBoundary::LcMapStringA);
 const Monstey_CrtMapRequest request{locale_id, 0x100u, source, source_count, 3u, code_page, 1u, call_site, iteration};
 const auto count = boundaries.lc_map_string_a(request, output);
 if (count == 0) return static_cast<std::int32_t>(value);
 if (count == 1) return output[0];
 return static_cast<std::int32_t>((static_cast<std::uint32_t>(output[0]) << 8u) | output[1]);
}
std::int32_t Monstey_CrtStricmp(Monstey_CrtMemory memory, std::uint32_t left, std::uint32_t right,
 Monstey_CrtBoundaries& boundaries, std::uint32_t global_locale_slot) {
 if (!boundaries.get_ptd) throw Monstey_CrtBoundaryFault(Monstey_CrtBoundary::GetPtd);
 const auto ptd = boundaries.get_ptd();
 auto locale = Monstey_CrtRead(memory, ptd + 0x64u, 4);
 const auto global_locale = Monstey_CrtRead(memory, global_locale_slot, 4);
 if (locale != global_locale) {
  if (!boundaries.refresh_locale) throw Monstey_CrtBoundaryFault(Monstey_CrtBoundary::RefreshLocale);
  locale = boundaries.refresh_locale();
 }
 if (Monstey_CrtRead(memory, locale + 0x14u, 4) == 0)
  return Monstey_CrtStricmpC(memory, left, right);
 for (std::uint64_t iteration = 0;; ++iteration) {
  const auto left_byte = static_cast<std::uint8_t>(Monstey_CrtRead(memory, left, 1));
  const auto folded_left = Monstey_CrtFoldByte(memory, locale, left_byte, boundaries, Monstey_CrtCallSite::Left, iteration);
  const auto right_byte = static_cast<std::uint8_t>(Monstey_CrtRead(memory, right, 1));
  ++left;
  const auto folded_right = Monstey_CrtFoldByte(memory, locale, right_byte, boundaries, Monstey_CrtCallSite::Right, iteration);
  ++right;
  if (folded_left != 0 && folded_left == folded_right) continue;
  return folded_left - folded_right;
 }
}
}
