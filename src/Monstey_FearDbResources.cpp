#include "Monstey_FearDbResources.hpp"
#include <algorithm>
namespace Monstey {
namespace {
using Monstey_U32=std::uint32_t;
std::span<std::uint8_t> Monstey_ResourceRange(Monstey_FearDbResources& resources,Monstey_U32 address,bool writing){
 for(auto&allocation:resources.allocations){const auto base=allocation.base;const auto size=allocation.allocation.size();if(address>=base&&std::uint64_t(address)+4u<=0x100000000ull&&std::uint64_t(address)-base<=size&&4u<=size-static_cast<std::size_t>(address-base))return std::span(allocation.allocation).subspan(address-base,4);}
 throw Monstey_FearDbFault(address,4,writing);
}
Monstey_U32 Monstey_ResourceRead(Monstey_FearDbResources& resources,Monstey_U32 address){auto bytes=Monstey_ResourceRange(resources,address,false);Monstey_U32 value=0;for(unsigned i=0;i<4;++i)value|=Monstey_U32(bytes[i])<<(i*8);return value;}
void Monstey_ResourceWrite(Monstey_FearDbResources& resources,Monstey_U32 address,Monstey_U32 value){auto bytes=Monstey_ResourceRange(resources,address,true);for(unsigned i=0;i<4;++i)bytes[i]=static_cast<std::uint8_t>(value>>(i*8));}
}
void Monstey_FearDbRetainSelected(Monstey_FearDbResources& resources,std::uint32_t database){const auto count=Monstey_ResourceRead(resources,database+16u);Monstey_ResourceWrite(resources,database+16u,count+1u);}
void Monstey_FearDbRegisterLoaded(Monstey_FearDbResources& resources,std::uint32_t database){resources.cache.push_back(database);Monstey_FearDbRetainSelected(resources,database);}
Monstey_FearDbReleaseObservation Monstey_FearDbReleaseDatabase(Monstey_FearDbResources& resources,std::uint32_t database){
 Monstey_FearDbReleaseObservation result;if(database==0)return result;
 result.counter_after=Monstey_ResourceRead(resources,database+16u)-1u;Monstey_ResourceWrite(resources,database+16u,result.counter_after);
 if(result.counter_after!=0){result.kind=Monstey_FearDbReleaseKind::retained;return result;}
 const auto row=std::find(resources.cache.begin(),resources.cache.end(),database);if(row!=resources.cache.end()){result.removed_cache_index=static_cast<std::size_t>(row-resources.cache.begin());resources.cache.erase(row);}
 result.allocation_base=Monstey_ResourceRead(resources,database);
 if(result.allocation_base==0){result.kind=Monstey_FearDbReleaseKind::null_allocation;return result;}
 const auto allocation=std::find_if(resources.allocations.begin(),resources.allocations.end(),[&](const auto&image){return image.base==result.allocation_base;});
 if(allocation==resources.allocations.end())throw Monstey_FearDbFault(result.allocation_base,0,true);
 resources.allocations.erase(allocation);result.kind=Monstey_FearDbReleaseKind::freed;return result;
}
} // namespace Monstey
