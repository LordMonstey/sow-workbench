#pragma once
#include "Monstey_GadbBufferedStream.hpp"
namespace Monstey {
class Monstey_GadbEncodedMemoryFault final : public std::runtime_error {
public:
  std::uint64_t address;
  std::size_t width;
  bool writing;
  Monstey_GadbEncodedMemoryFault(std::uint64_t, std::size_t, bool);
};
class Monstey_GadbEncodedTargetFault final : public std::runtime_error {
public:
  std::uint64_t native_site, stream, target, expected;
  Monstey_GadbEncodedTargetFault(std::uint64_t, std::uint64_t, std::uint64_t,
                                 std::uint64_t);
};
// Additive raw-result wrappers for exact115FD2C/115FCEC. No accepted API
// changes. wrapper_body_rsp=entryRSP-28h. Current stream vtable+8
// must141B50DE8; then the accepted concrete BufferedRead runs at
// wrapper_body_rsp-30h. Return RAX=stream and preserve its raw RDX,even on a
// Read error.
Monstey_GadbBufferedResult Monstey_GadbEncodedReadU16(
    Monstey_GadbBinderMemory, std::uint64_t stream, std::uint64_t destination,
    std::uint64_t wrapper_body_rsp, const Monstey_GadbBufferedServices &,
    Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult Monstey_GadbEncodedReadByte(
    Monstey_GadbBinderMemory, std::uint64_t stream, std::uint64_t destination,
    std::uint64_t wrapper_body_rsp, const Monstey_GadbBufferedServices &,
    Monstey_GadbBufferedCells = {});
// Complete140503628. body_rsp=entryRSP-28h; local U16 length at body_rsp+40h
// is not initialized here. Initial unsigned capacity gate,live count reloads,
// one-byte reads,NUL epoch and raw RAX/RDX are retained. No capacity repair.
// Stable borrowed backing/span; raw bytes,not inferred UTF8/OS filename.
// Buffered services remain only copy/fill/ReadFile. No wholeRead callback.
Monstey_GadbBufferedResult Monstey_GadbReadEncodedString(
    Monstey_GadbBinderMemory, std::uint64_t stream, std::uint64_t destination,
    std::uint32_t capacity, std::uint64_t body_rsp,
    const Monstey_GadbBufferedServices &, Monstey_GadbBufferedCells = {});
} // namespace Monstey
