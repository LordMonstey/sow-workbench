#include "Monstey_FearDbNames.hpp"
#include <bit>
namespace Monstey {
namespace {
using Monstey_U32=std::uint32_t;
Monstey_U32 Monstey_NameRead(Monstey_FearDbMemory memory,Monstey_U32 address,unsigned width){
 if(address<memory.base||std::uint64_t(address)+width>0x100000000ull||std::uint64_t(address)-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))throw Monstey_FearDbFault(address,width,false);
 Monstey_U32 word=0;for(unsigned i=0;i<width;++i)word|=Monstey_U32(memory.bytes[address-memory.base+i])<<(i*8);return word;
}
}
std::uint32_t Monstey_FearDbBinaryNameSearch(Monstey_FearDbMemory memory,std::uint32_t children,std::uint32_t count,std::uint32_t query,const Monstey_FearDbNameCompare& compare){
 Monstey_U32 low=0,high=count-1u;if(std::bit_cast<std::int32_t>(high)<0)return 0;
 for(;;){const auto sum=high+low;const auto adjusted=sum+(sum>>31);const auto middle=std::bit_cast<Monstey_U32>(std::bit_cast<std::int32_t>(adjusted)>>1);const auto row=children+middle*16u;const auto candidate=Monstey_NameRead(memory,row,4);
  std::int32_t result=0;
  if(query==0)result=candidate==0?0:-1;
  else if(candidate==0){low=middle+1u;if(std::bit_cast<std::int32_t>(low)>std::bit_cast<std::int32_t>(high))return 0;continue;}
  else {if(!compare)throw std::invalid_argument("Consumed original FEAR name comparator is unavailable");result=compare(query,candidate);}
  if(result==0)return row;
  if(result<0)high=middle-1u;else low=middle+1u;
  if(std::bit_cast<std::int32_t>(low)>std::bit_cast<std::int32_t>(high))return 0;
 }
}
std::uint32_t Monstey_FearDbRawChildrenName(Monstey_FearDbMemory memory,std::uint32_t parent,std::uint32_t query,const Monstey_FearDbNameCompare& compare){const auto children=Monstey_NameRead(memory,parent+12u,4);const auto count=Monstey_NameRead(memory,parent+8u,4);return Monstey_FearDbBinaryNameSearch(memory,children,count,query,compare);}
std::uint32_t Monstey_FearDbGuardedChildName(Monstey_FearDbMemory memory,std::uint32_t parent,std::uint32_t query,const Monstey_FearDbNameCompare& compare){if(parent==0||query==0)return 0;if(Monstey_NameRead(memory,query,1)==0)return 0;return Monstey_FearDbRawChildrenName(memory,parent,query,compare);}
std::uint32_t Monstey_FearDbRecordByNames(Monstey_FearDbMemory memory,std::uint32_t database,std::uint32_t category_name,std::uint32_t record_name,const Monstey_FearDbNameCompare& compare){const auto category=Monstey_FearDbGuardedChildName(memory,database,category_name,compare);return category?Monstey_FearDbRawChildrenName(memory,category,record_name,compare):0;}
std::uint32_t Monstey_FearDbAttributeByNames(Monstey_FearDbMemory memory,std::uint32_t category,std::uint32_t record_name,std::uint32_t attribute_name,const Monstey_FearDbNameCompare& compare){const auto record=Monstey_FearDbGuardedChildName(memory,category,record_name,compare);return record?Monstey_FearDbRawChildrenName(memory,record,attribute_name,compare):0;}
std::uint32_t Monstey_FearDbAttributeInDatabaseByNames(Monstey_FearDbMemory memory,std::uint32_t database,std::uint32_t category_name,std::uint32_t record_name,std::uint32_t attribute_name,const Monstey_FearDbNameCompare& compare){const auto record=Monstey_FearDbRecordByNames(memory,database,category_name,record_name,compare);return record?Monstey_FearDbRawChildrenName(memory,record,attribute_name,compare):0;}
} // namespace Monstey
