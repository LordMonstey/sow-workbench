#include "Monstey_FearDbPacked.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>
namespace Monstey {
namespace {
using Monstey_U32=std::uint32_t;
Monstey_U32 Monstey_Unpack(std::span<const std::uint8_t> bytes,std::size_t at) {
 Monstey_U32 word=0;for(unsigned i=0;i<4;++i)word|=Monstey_U32(bytes[at+i])<<(i*8);return word;
}
bool Monstey_PackedRead(Monstey_FearDbPackedInput& in,std::span<std::uint8_t> destination) {
 ++in.read_calls;
 if(in.position>in.bytes.size()||destination.size()>in.bytes.size()-in.position){std::fill(destination.begin(),destination.end(),std::uint8_t{0});return false;}
 std::copy_n(in.bytes.begin()+static_cast<std::ptrdiff_t>(in.position),destination.size(),destination.begin());
 in.position+=destination.size();return true;
}
struct Monstey_PackedBuilder {
 Monstey_FearDbPackedInput& input;
 Monstey_FearDbPackedImage& image;
 Monstey_U32 cursor;
 std::span<std::uint8_t> Range(Monstey_U32 address,Monstey_U32 size) {
  if(address<image.base||std::uint64_t(address)+size>0x100000000ull||std::uint64_t(address)-image.base>image.allocation.size()||size>image.allocation.size()-static_cast<std::size_t>(address-image.base))throw Monstey_FearDbFault(address,size,true);
  return std::span(image.allocation).subspan(address-image.base,size);
 }
 void Store(Monstey_U32 address,Monstey_U32 word,unsigned width=4) {
  auto out=Range(address,width);for(unsigned i=0;i<width;++i)out[i]=static_cast<std::uint8_t>(word>>(i*8));
 }
 Monstey_U32 Read(Monstey_U32 address,unsigned width=4) const {
  const auto memory=image.Memory();if(address<memory.base||std::uint64_t(address)+width>0x100000000ull||std::uint64_t(address)-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))throw Monstey_FearDbFault(address,width,false);
  Monstey_U32 word=0;for(unsigned i=0;i<width;++i)word|=Monstey_U32(memory.bytes[address-memory.base+i])<<(i*8);return word;
 }
 Monstey_U32 Reserve(Monstey_U32 bytes) {const auto old=cursor;cursor+=bytes;return old;}
 void Node(Monstey_U32 address,Monstey_U32 name,Monstey_U32 parent,Monstey_U32 count,Monstey_U32 children) {
  // 6EA0 store order: name, count, children, parent.
  Store(address,name);Store(address+8u,count);Store(address+12u,children);Store(address+4u,parent);
 }
 bool Attribute(Monstey_U32 address,Monstey_U32 parent) {
  std::array<std::uint8_t,16> header{};if(!Monstey_PackedRead(input,header))return false;
  const auto name=image.base+Monstey_Unpack(header,0);const auto type=Monstey_Unpack(header,4);const auto usage=Monstey_Unpack(header,8);const auto count=Monstey_Unpack(header,12);const auto values=Reserve(count*4u);
  for(Monstey_U32 i=0;i<count;++i){std::array<std::uint8_t,4> raw{};if(!Monstey_PackedRead(input,raw))return false;auto word=Monstey_Unpack(raw,0);const auto signed_type=std::bit_cast<std::int32_t>(type);if(signed_type>=4&&signed_type<=8)word+=image.base;Store(values+i*4u,word);}
  // 6EF0 narrows full physical count/type/usage only after all reads complete.
  Store(address,name);Store(address+10u,type,1);Store(address+11u,usage,1);Store(address+8u,count,2);Store(address+12u,values);Store(address+4u,parent);return true;
 }
 bool Record(Monstey_U32 address,Monstey_U32 parent) {
  std::array<std::uint8_t,8> header{};if(!Monstey_PackedRead(input,header))return false;
  const auto name=image.base+Monstey_Unpack(header,0);const auto count=Monstey_Unpack(header,4);const auto attributes=Reserve(count*16u);
  for(Monstey_U32 i=0;i<count;++i)if(!Attribute(attributes+i*16u,address))return false;
  Node(address,name,parent,count,attributes);return true;
 }
 bool Category(Monstey_U32 address,Monstey_U32 parent) {
  std::array<std::uint8_t,8> header{};if(!Monstey_PackedRead(input,header))return false;
  const auto name=image.base+Monstey_Unpack(header,0);const auto count=Monstey_Unpack(header,4);const auto records=Reserve(count*16u);
  for(Monstey_U32 i=0;i<count;++i)if(!Record(records+i*16u,address))return false;
  Node(address,name,parent,count,records);return true;
 }
 void FixAttribute(Monstey_U32 attribute) {
  if(Read(attribute+10u,1)!=9)return;
  const auto record=Read(attribute+4u);const auto category=Read(record+4u);const auto database=Read(category+4u);
  for(Monstey_U32 i=0;i<Read(attribute+8u,2);++i){const auto at=Read(attribute+12u)+i*4u;const auto packed=Read(at);const auto category_index=packed>>16;const auto record_index=packed&0xffffu;Monstey_U32 resolved=0;
   if(category_index<Read(database+8u)){const auto target_category=Read(database+12u)+category_index*16u;if(record_index<Read(target_category+8u))resolved=Read(target_category+12u)+record_index*16u;}
   Store(at,resolved);
  }
 }
 void FixDatabase() {
  const auto database=image.database;
  for(Monstey_U32 c=0;c<Read(database+8u);++c){const auto category=Read(database+12u)+c*16u;
   for(Monstey_U32 r=0;r<Read(category+8u);++r){const auto record=Read(category+12u)+r*16u;
    for(Monstey_U32 a=0;a<Read(record+8u);++a)FixAttribute(Read(record+12u)+a*16u);
   }
  }
 }
};
}
Monstey_FearDbPackedLoad Monstey_FearDbLoadPacked(
    Monstey_FearDbPackedInput& input, std::span<const std::uint8_t> name,
    std::uint32_t allocation_base) {
    Monstey_FearDbPackedLoad result;
    std::array<std::uint8_t, 28> header{};
    if (!Monstey_PackedRead(input, header)) return result;
    if (Monstey_Unpack(header, 0) != 0x42444147u ||
        Monstey_Unpack(header, 4) != 3u) {
        result.status = Monstey_FearDbPackedStatus::invalid_header;
        return result;
    }
    const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
    if (end == name.end())
        throw std::invalid_argument("Consumed FEAR database name has no terminator");
    const auto name_bytes = static_cast<std::size_t>(end - name.begin()) + 1;
    if (name_bytes > UINT32_MAX)
        throw std::length_error("Owned FEAR name exceeds native32 size");
    const auto pool = Monstey_Unpack(header, 8);
    const auto sum = Monstey_Unpack(header, 12) + Monstey_Unpack(header, 16) +
                     Monstey_Unpack(header, 20);
    const auto bytes = pool + 20u + sum * 16u +
                       Monstey_Unpack(header, 24) * 4u +
                       static_cast<Monstey_U32>(name_bytes);
    result.allocation_bytes = bytes;
    if (allocation_base == 0 ||
        std::uint64_t(allocation_base) + bytes > 0x100000000ull)
        throw std::invalid_argument("Owned FEAR allocation identity range unavailable");
    Monstey_FearDbPackedImage image;
    image.base = allocation_base;
    image.database = allocation_base + pool;
    image.allocation.resize(bytes);
    Monstey_PackedBuilder build{input, image, image.database + 20u};
    if (!Monstey_PackedRead(input, build.Range(image.base, pool))) {
        result.status = Monstey_FearDbPackedStatus::pool_read_failed;
        return result;
    }
    std::array<std::uint8_t, 4> raw_count{};
    if (!Monstey_PackedRead(input, raw_count)) {
        result.status = Monstey_FearDbPackedStatus::graph_read_failed;
        return result;
    }
    const auto count = Monstey_Unpack(raw_count, 0);
    // Native38D0 rescans AFTER its count Read; allocation3890 used an earlier scan.
    const auto graph_end = std::find(name.begin(), name.end(), std::uint8_t{0});
    if (graph_end == name.end())
        throw std::invalid_argument("Consumed FEAR graph name has no terminator");
    const auto graph_name_bytes =
        static_cast<std::size_t>(graph_end - name.begin()) + 1;
    if (graph_name_bytes > UINT32_MAX)
        throw std::length_error("Owned FEAR graph name exceeds native32 size");
    const auto copied_name =
        build.Reserve(static_cast<Monstey_U32>(graph_name_bytes));
    for (Monstey_U32 i = 0; i < graph_name_bytes; ++i)
        build.Store(copied_name + i, name[i], 1);
    const auto categories = build.Reserve(count * 16u);
    for (Monstey_U32 i = 0; i < count; ++i) {
        if (!build.Category(categories + i * 16u, image.database)) {
            result.status = Monstey_FearDbPackedStatus::graph_read_failed;
            return result;
        }
    }
    build.Store(image.database + 4u, copied_name);
    build.Store(image.database + 8u, count);
    build.Store(image.database + 12u, categories);
    build.Store(image.database, image.base);
    build.Store(image.database + 16u, 0);
    build.FixDatabase();
    image.used_bytes = build.cursor - image.base;
    result.status = Monstey_FearDbPackedStatus::loaded;
    result.image = std::move(image);
    return result;
}
} // namespace Monstey
