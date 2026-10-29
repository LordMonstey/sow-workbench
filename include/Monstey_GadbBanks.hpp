#pragma once
#include "Monstey_FearAudit.hpp"
namespace Monstey {
// Fixed seven-byte RIP-relative literal shape, qualified by native disassembly.
// This validates a supplied instruction map. It performs no instruction
// discovery.
std::uint64_t
Monstey_GadbBankRipLiteral(std::span<const std::uint8_t> instruction,
                           std::uint64_t instructionAddress, bool store);
// The self-contained entrypoint reads only an exact supported image. Its
// compiled profile contains native-proved layout offsets and digests, no
// original code.
Monstey_Json Monstey_GadbBankRegistration(std::span<const std::uint8_t> image);
// The legacy overload additionally validates the optional pinned native map.
// Reports constructor recipes, never actual runtime BSS or initialization
// history.
Monstey_Json
Monstey_GadbBankRegistration(std::span<const std::uint8_t> image,
                             std::span<const std::uint8_t> nativeSemanticMap);
// Bounded metadata projection of a reader report. Search is a display filter,
// never a replacement for the unqualified native name comparator.
Monstey_Json Monstey_GadbBankPage(const Monstey_Json &report,
                                  const Monstey_Json &arguments);
// Read a qualified file without changing editor state. No private map is
// needed.
Monstey_Json Monstey_GadbBankInspect(const Monstey_Json &arguments);
} // namespace Monstey