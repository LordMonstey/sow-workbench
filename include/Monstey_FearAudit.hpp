#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Bounded file-backed virtual-address view; the caller retains ownership of rawPe.
std::span<const std::uint8_t> Monstey_FearPeRange(std::span<const std::uint8_t> rawPe,
                                               std::uint64_t virtualAddress, std::size_t count);
Monstey_Json Monstey_FearInspectPe(const std::filesystem::path& path);
Monstey_Json Monstey_FearInspectDatabaseBytes(std::span<const std::uint8_t> bytes, bool includeRecords = false);
Monstey_Json Monstey_FearInspectDatabase(const std::filesystem::path& path, bool includeRecords = false);
Monstey_Json Monstey_FearAudit(const std::filesystem::path& sdkRoot);
}
