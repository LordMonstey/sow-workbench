// Additive SoW publication: explicit numeric body context at each actual CALL.
#pragma once
#include "Monstey_GadbPublication.hpp"
namespace Monstey {
struct Monstey_GadbPublicationFramedVirtualCall {
  Monstey_GadbPublicationVirtualCall call;
  std::uint64_t caller_body_rsp;
};
struct Monstey_GadbPublicationFramedServices {
  Monstey_FearDbCallable<std::uint32_t(Monstey_GadbBinderMemory,
      const Monstey_GadbPublicationAtomicCall&)> atomic32;
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(Monstey_GadbBinderMemory,
      const Monstey_GadbPublicationFramedVirtualCall&)> virtual_call;
  Monstey_FearDbCallable<void(Monstey_GadbBinderMemory,
      const Monstey_GadbPublicationTrapCall&)> trap;
};
// Every body_rsp is THIS native function's logical body RSP, not entry RSP.
Monstey_GadbBufferedResult Monstey_GadbPublishDatabaseStorageFramed(
    Monstey_GadbBinderMemory,std::uint64_t database,
    std::uint32_t header_h8,std::uint32_t header_h9,std::uint64_t bank_name,
    std::uint64_t body_rsp,const Monstey_GadbPublicationFramedServices&);
Monstey_GadbBufferedResult Monstey_GadbCopyPublicationPairFramed(
    Monstey_GadbBinderMemory,std::uint64_t destination,std::uint64_t source,
    std::uint64_t body_rsp,Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices&);
Monstey_GadbBufferedResult Monstey_GadbAssignPublicationPairFramed(
    Monstey_GadbBinderMemory,std::uint64_t destination,std::uint64_t new_data,
    std::uint64_t new_control,std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices&);
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPairFramed(
    Monstey_GadbBinderMemory,std::uint64_t pair,std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices&);
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPrimaryFramed(
    Monstey_GadbBinderMemory,std::uint64_t control,std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices&);
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationSecondaryFramed(
    Monstey_GadbBinderMemory,std::uint64_t control,std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices&);
}
