#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Read-only HKAI3 owned-byte extraction, native-profile compact cooking and
// paging. Page arguments: optional mesh_node, offset and limit (1..512,
// default256). Root bounds are represented by raw F32 words, never non-finite
// JSON numbers.
Monstey_Json Monstey_NavStaticTreeFromBytes(
    std::span<const std::uint8_t> source,
    const Monstey_Json &page = Monstey_Json::object());
// File arguments add required path and expected_sha256. No source/output
// writes.
Monstey_Json Monstey_NavStaticTreeInspect(const Monstey_Json &arguments);
} // namespace Monstey
