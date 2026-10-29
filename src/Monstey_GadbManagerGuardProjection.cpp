#include "Monstey_GadbManagerGuardProjection.hpp"
#include <limits>
namespace Monstey {
namespace {
std::size_t Monstey_GuardProjectedOffset(Monstey_GadbBinderMemory memory,
    std::uint64_t address,bool writing){
  if(address<memory.base || address>std::numeric_limits<std::uint64_t>::max()-7u ||
     address-memory.base>memory.bytes.size() ||
     8u>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))
    throw Monstey_GadbBufferedMemoryFault(address,8,writing);
  return static_cast<std::size_t>(address-memory.base);
}
void Monstey_GuardProjectedStore(Monstey_GadbBinderMemory memory,
    std::uint64_t address,std::uint64_t value){
  const auto offset=Monstey_GuardProjectedOffset(memory,address,true);
  for(std::size_t i=0;i<8;++i)memory.bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
}
std::uint64_t Monstey_GuardProjectedLoad(Monstey_GadbBinderMemory memory,std::uint64_t address){
  const auto offset=Monstey_GuardProjectedOffset(memory,address,false);
  std::uint64_t value=0;
  for(std::size_t i=0;i<8;++i)value|=std::uint64_t(memory.bytes[offset+i])<<(i*8u);
  return value;
}
} // namespace
Monstey_GadbManagerGuardProjectedResult Monstey_GadbConstructManagerGuardProjected(
    Monstey_GadbBinderMemory memory,std::uint64_t guard,std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,std::uint64_t incoming_r8,std::uint64_t incoming_r9,
    const Monstey_GadbManagerPrimitiveBindings &bindings,
    const Monstey_GadbManagerEnterProjectionServices &services){
  incoming.rdx=lock;
  Monstey_GuardProjectedStore(memory,guard,lock);
  const auto target=Monstey_GuardProjectedLoad(memory,bindings.enter_cell);
  if(!services.enter)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
      Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
      0x14114BA9FULL,bindings.enter_cell,8,bindings.enter_cell,target,lock);
  const auto result=services.enter(memory,
    {{Monstey_GadbManagerSyncOperation::enter,0x14114BA9FULL,
      bindings.enter_cell,target,lock,incoming},incoming_r8,incoming_r9});
  return {{guard,result.rdx},result.r8,result.r9};
}
} // namespace Monstey
