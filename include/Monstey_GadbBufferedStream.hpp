#pragma once
#include "Monstey_FearDbCallable.hpp"
#include "Monstey_GadbBinderComposition.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_GadbBufferedResult {
  std::uint64_t rax, rdx;
};
struct Monstey_GadbBufferedCells {
  std::uint64_t copy = 0x141D92E38ULL, fill = 0x141D92E50ULL,
                read_file = 0x141D924E0ULL;
};
enum class Monstey_GadbBufferedImport { copy, fill, read_file };
class Monstey_GadbBufferedMemoryFault final : public std::runtime_error {
public:
  std::uint64_t address;
  std::size_t width;
  bool writing;
  Monstey_GadbBufferedMemoryFault(std::uint64_t, std::size_t, bool);
};
class Monstey_GadbBufferedImportFault final : public std::runtime_error {
public:
  Monstey_GadbBufferedImport kind;
  std::uint64_t native_site, slot, target;
  Monstey_GadbBufferedImportFault(Monstey_GadbBufferedImport, std::uint64_t,
                                  std::uint64_t, std::uint64_t);
};
class Monstey_GadbBufferedDispatchFault final : public std::runtime_error {
public:
  std::uint64_t native_site, slot, target, expected;
  Monstey_GadbBufferedDispatchFault(std::uint64_t, std::uint64_t, std::uint64_t,
                                    std::uint64_t);
};
struct Monstey_GadbBufferedCopyCall {
  std::uint64_t slot, target, destination, source, count, native_site;
};
struct Monstey_GadbBufferedFillCall {
  std::uint64_t slot, target, destination, count;
  std::uint32_t value;
  std::uint64_t native_site;
};
struct Monstey_GadbBufferedReadFileCall {
  std::uint64_t slot, target, handle, destination;
  std::uint32_t count;
  std::uint64_t bytes_read_address, overlapped, stack_argument_address,
      native_site;
};
struct Monstey_GadbBufferedServices {
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbBufferedCopyCall &)>
      copy;
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbBufferedFillCall &)>
      fill;
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbBufferedReadFileCall &)>
      read_file;
};
// Exactly five SoW bodies,548B total. All addresses are owned numeric
// identities. Memory is an exclusive stable borrowed view passed by value; no
// rebind profile. Providers perform ONLY original-external copy/fill/ReadFile
// effects and supply their explicit raw RAX/RDX; no canonical volatile register
// value is invented. Cells are constant identities for an operation, with
// native defaults or an explicitly declared owned remapping. Their target
// Qwords are read live. Frames are body RSP identities, not entry RSP. Unknown
// savedCPU/return words are supplied bytes. Known private count/overlapped
// stores are retained. Inner virtual+18 must select141204C2C at its actual
// consumed epoch. No allocator/Open/path/string137B/physicalABI/original calls
// are included.
Monstey_GadbBufferedResult
Monstey_GadbBufferedCopyCached(Monstey_GadbBinderMemory, std::uint64_t inner,
                               std::uint64_t destination, std::uint32_t count,
                               const Monstey_GadbBufferedServices &,
                               Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult Monstey_GadbBufferedDirectRead(
    Monstey_GadbBinderMemory, std::uint64_t inner, std::uint64_t destination,
    std::uint32_t count, std::uint64_t out_count, std::uint64_t body_rsp,
    std::uint64_t incoming_rax, const Monstey_GadbBufferedServices &,
    Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult
Monstey_GadbBufferedRefill(Monstey_GadbBinderMemory, std::uint64_t inner,
                           std::uint64_t body_rsp, std::uint64_t incoming_rax,
                           const Monstey_GadbBufferedServices &,
                           Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult Monstey_GadbBufferedReadInner(
    Monstey_GadbBinderMemory, std::uint64_t inner, std::uint64_t destination,
    std::uint32_t count, std::uint64_t body_rsp, std::uint64_t incoming_rax,
    const Monstey_GadbBufferedServices &, Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult Monstey_GadbBufferedRead(
    Monstey_GadbBinderMemory, std::uint64_t outer, std::uint64_t destination,
    std::uint32_t count, std::uint64_t body_rsp,
    const Monstey_GadbBufferedServices &, Monstey_GadbBufferedCells = {});
} // namespace Monstey
