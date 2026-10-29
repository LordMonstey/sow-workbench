#include "Monstey_GadbCategoryLookup.hpp"
#include <bit>
#include <limits>
namespace Monstey {
Monstey_GadbCategoryLookupFault::Monstey_GadbCategoryLookupFault(
 std::uint64_t at, std::size_t n)
 : std::runtime_error("Consumed SoW category-lookup range unavailable"),
   address(at), width(n) {}
namespace {
std::uint64_t Monstey_CategoryLookupRead(Monstey_GadbBinderMemory memory,
 std::uint64_t address, std::size_t width) {
 if(address<memory.base || address>std::numeric_limits<std::uint64_t>::max()-(width-1) ||
    address-memory.base>memory.bytes.size() ||
    width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))
  throw Monstey_GadbCategoryLookupFault(address,width);
 const auto offset=static_cast<std::size_t>(address-memory.base);
 std::uint64_t value=0;
 for(std::size_t i=0;i<width;++i)value|=std::uint64_t(memory.bytes[offset+i])<<(i*8u);
 return value;
}
}
Monstey_GadbBufferedResult Monstey_GadbFindCategoryByHash(
 Monstey_GadbBinderMemory memory,std::uint64_t object,std::uint64_t hash_pointer) {
 auto end=static_cast<std::uint32_t>(Monstey_CategoryLookupRead(memory,object+0x48,4))-1u;
 const auto array=Monstey_CategoryLookupRead(memory,object+0x50,8);
 std::uint32_t begin=0;
 std::uint64_t rdx=hash_pointer;
 if(std::bit_cast<std::int32_t>(end)<0)return {0,rdx};
 for(;;) {
  const auto sum=std::uint32_t(end+begin);
  const auto midpoint=std::bit_cast<std::int32_t>(sum)/2;
  rdx=static_cast<std::uint64_t>(static_cast<std::int64_t>(midpoint))*0x58u;
  const auto row_hash=static_cast<std::uint32_t>(Monstey_CategoryLookupRead(memory,array+rdx+0x50,4));
  const auto key=static_cast<std::uint32_t>(Monstey_CategoryLookupRead(memory,hash_pointer,4));
  if(key==row_hash)return {array+rdx,rdx};
  if(key<row_hash)end=std::bit_cast<std::uint32_t>(midpoint)-1u;
  else begin=std::bit_cast<std::uint32_t>(midpoint)+1u;
  if(std::bit_cast<std::int32_t>(begin)>std::bit_cast<std::int32_t>(end))return {0,rdx};
 }
}
} // namespace Monstey
