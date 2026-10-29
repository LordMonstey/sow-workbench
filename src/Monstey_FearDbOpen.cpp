#include "Monstey_FearDbOpen.hpp"
#include <stdexcept>
namespace Monstey {
namespace {
using Monstey_U32=std::uint32_t;
Monstey_U32 Monstey_OpenRead(Monstey_FearDbMemory memory,Monstey_U32 address,unsigned width){
 if(address<memory.base||std::uint64_t(address)+width>0x100000000ull||std::uint64_t(address)-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))throw Monstey_FearDbFault(address,width,false);
 Monstey_U32 word=0;for(unsigned i=0;i<width;++i)word|=Monstey_U32(memory.bytes[address-memory.base+i])<<(i*8);return word;
}
Monstey_U32 Monstey_OpenResourceRead(const Monstey_FearDbResources& resources,Monstey_U32 address){
 for(const auto&image:resources.allocations){const auto base=image.base;const auto size=image.allocation.size();if(address>=base&&std::uint64_t(address)+4u<=0x100000000ull&&std::uint64_t(address)-base<=size&&4u<=size-static_cast<std::size_t>(address-base))return Monstey_OpenRead(image.Memory(),address,4);}
 throw Monstey_FearDbFault(address,4,false);
}
void Monstey_OpenStableCache(const Monstey_FearDbResources& resources,const Monstey_U32* backing){if(resources.cache.data()!=backing)throw std::invalid_argument("Consumed FEAR cache backing changed during external comparison");}
Monstey_U32 Monstey_OpenCacheRead(const Monstey_FearDbResources& resources,const Monstey_U32* backing,std::size_t index){Monstey_OpenStableCache(resources,backing);if(index>=resources.cache.size())throw std::out_of_range("Consumed FEAR cache row is outside the owned live sequence");return resources.cache[index];}
}
std::uint32_t Monstey_FearDbOpenExisting(Monstey_FearDbResources& resources,Monstey_FearDbMemory filename_memory,std::uint32_t filename,const Monstey_FearDbNameCompare& compare){
 if(filename==0)return 0;
 if(Monstey_OpenRead(filename_memory,filename,1)==0)return 0;
 const auto initial_end=resources.cache.size();
 const auto*backing=resources.cache.data();
 if(initial_end==0)return 0;
 //6A6D samples EBX once. Holder copying shares the existing callable target;
 // it does not copy its mutable closure state or reload a replacement target.
 const auto sampled_compare=compare;
 std::size_t index=0;
 for(;;){
  const auto database=Monstey_OpenCacheRead(resources,backing,index);
  const auto candidate=Monstey_OpenResourceRead(resources,database+4u);
  if(candidate!=0){
   if(!sampled_compare)throw std::invalid_argument("Consumed original FEAR filename comparator is unavailable");
   const auto result=sampled_compare(filename,candidate);
   if(result==0){
    const auto selected=Monstey_OpenCacheRead(resources,backing,index);
    Monstey_FearDbRetainSelected(resources,selected);
    return Monstey_OpenCacheRead(resources,backing,index);
   }
  }
  //6A87 loads current end after a miss, then6A8A advances the retained cursor.
  Monstey_OpenStableCache(resources,backing);
  const auto end=resources.cache.size();
  ++index;
  if(index==end)return 0;
 }
}
std::uint32_t Monstey_FearDbOpenNew(Monstey_FearDbResources& resources,Monstey_FearDbMemory filename_memory,std::uint32_t filename,Monstey_FearDbPackedInput& input,std::uint32_t allocation_base,const Monstey_FearDbNameCompare& compare){
 const auto existing=Monstey_FearDbOpenExisting(resources,filename_memory,filename,compare);if(existing!=0)return existing;
 if(filename==0)return 0;if(Monstey_OpenRead(filename_memory,filename,1)==0)return 0;
 const auto name=filename_memory.bytes.subspan(filename-filename_memory.base);
 auto loaded=Monstey_FearDbLoadPacked(input,name,allocation_base);if(!loaded.image)return 0;
 const auto&image=*loaded.image;const auto end=std::uint64_t(image.base)+image.allocation.size();for(const auto&live:resources.allocations){const auto live_end=std::uint64_t(live.base)+live.allocation.size();if(std::uint64_t(image.base)<live_end&&std::uint64_t(live.base)<end)throw std::invalid_argument("Consumed FEAR allocation identity overlaps live owned storage");}
 const auto database=image.database;resources.allocations.push_back(std::move(*loaded.image));Monstey_FearDbRegisterLoaded(resources,database);return database;
}
} // namespace Monstey
