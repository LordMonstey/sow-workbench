#include "Monstey_FearConstraintTable.hpp"
#include "Monstey_FearConstraintReader.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
namespace Monstey {
namespace {
constexpr std::uint32_t Monstey_Base=0x10000000u,Monstey_Frame=0x10012000u;
constexpr std::uint32_t Monstey_Stream=0x10011000u,Monstey_Vtable=0x10011100u,Monstey_Map=0x10011200u;
struct Monstey_ByteLess {
 bool operator()(const std::string& a,const std::string& b)const {
  return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[](char x,char y){return static_cast<unsigned char>(x)<static_cast<unsigned char>(y);});
 }
};
struct Monstey_ConstraintAdapter {
 std::span<const std::uint8_t> input;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x20000,0xCD);
 Monstey_FearDbMutableMemory memory{Monstey_Base,bytes};
 std::uint32_t next=0x10014000u;
 std::uint64_t position=0,reads=0,records=0;
 std::map<std::uint32_t,std::string> strings;
 std::map<std::string,std::uint32_t,Monstey_ByteLess> cells;
 std::set<std::uint32_t> live_arrays;
 explicit Monstey_ConstraintAdapter(std::span<const std::uint8_t> source):input(source) {
  Monstey_Put(Monstey_Frame+0x48,Monstey_Stream);Monstey_Put(Monstey_Frame+0x4C,Monstey_Map);
  Monstey_Put(Monstey_Stream,Monstey_Vtable);
  Monstey_Put(Monstey_Vtable+4,0xA004);Monstey_Put(Monstey_Vtable+0x18,0xA018);Monstey_Put(Monstey_Vtable+0x1C,0xA01C);
  Monstey_Put(0x1000B064,0xB064);Monstey_Put(0x1000B068,0xB068);Monstey_Put(0x1000B074,0xB074);
 }
 std::span<std::uint8_t> Monstey_Range(std::uint32_t at,std::uint32_t count,bool write) {
  const auto offset=static_cast<std::uint64_t>(at)-Monstey_Base;
  if(at<Monstey_Base||static_cast<std::uint64_t>(at)+count>0x100000000ull||offset>bytes.size()||count>bytes.size()-static_cast<std::size_t>(offset))throw Monstey_FearDbFault(at,count,write);
  return std::span<std::uint8_t>(bytes).subspan(static_cast<std::size_t>(offset),count);
 }
 void Monstey_Put(std::uint32_t at,std::uint32_t word,std::uint32_t width=4) {
  auto destination=Monstey_Range(at,width,true);for(std::uint32_t i=0;i<width;++i)destination[i]=static_cast<std::uint8_t>(word>>(8u*i));
 }
 std::uint32_t Monstey_Get(std::uint32_t at,std::uint32_t width=4) {
  const auto source=Monstey_Range(at,width,false);std::uint32_t word=0;for(std::uint32_t i=0;i<width;++i)word|=std::uint32_t(source[i])<<(8u*i);return word;
 }
 std::uint32_t Monstey_Allocate(std::uint32_t count) {
  const auto reservation=std::max<std::uint32_t>(count,1),address=next;
  const auto end=static_cast<std::uint64_t>(address)+reservation;
  if(end>std::numeric_limits<std::uint32_t>::max())throw std::bad_alloc();
  const auto wanted=static_cast<std::size_t>(end-Monstey_Base);
  if(wanted>bytes.size()){bytes.resize(wanted,0xCD);memory.bytes=bytes;}
  next=static_cast<std::uint32_t>(end);return address;
 }
 void Monstey_StreamCall(Monstey_FearConstraintCall c,std::uint32_t target)const {
  if(c.object!=Monstey_Stream||c.target!=target)throw std::runtime_error("Owned constraint stream dispatch differs");
 }
 Monstey_FearConstraintServices Monstey_Services() {
  Monstey_FearConstraintServices s;
  s.get_length=[this](auto c){Monstey_StreamCall(c,0xA01C);return static_cast<std::uint64_t>(input.size());};
  s.get_position=[this](auto c){Monstey_StreamCall(c,0xA018);return position;};
  s.read=[this](Monstey_FearConstraintReadCall c){
   Monstey_StreamCall({c.target,c.stream},0xA004);++reads;
   const bool available=position<=input.size()&&c.bytes<=input.size()-static_cast<std::size_t>(position);
   if(c.bytes!=0){auto dest=Monstey_Range(c.destination,c.bytes,true);if(available)std::copy_n(input.begin()+static_cast<std::ptrdiff_t>(position),c.bytes,dest.begin());else std::fill(dest.begin(),dest.end(),std::uint8_t{0});}
   if(!available)return std::uint32_t{1};position+=c.bytes;return std::uint32_t{0};
  };
  s.new_array=[this](auto n){const auto p=Monstey_Allocate(n);live_arrays.insert(p);return p;};
  s.delete_array=[this](auto p){if(live_arrays.erase(p)!=1)throw std::runtime_error("Owned constraint array lifetime differs");};
  s.string_default=[this](auto c){
   if(c.target!=0xB064)throw std::runtime_error("Owned string constructor dispatch differs");
   strings[c.object].clear();
   // Only the independently observed normal default-constructor writes are mirrored.
   Monstey_Put(c.object+0x18,15);Monstey_Put(c.object+0x14,0);Monstey_Put(c.object+4,0,1);
  };
  s.string_assign_cstr=[this](auto c,auto source){
   if(c.target!=0xB068||!strings.contains(c.object))throw std::runtime_error("Owned string assignment dispatch differs");
   auto& text=strings.at(c.object);text.clear();
   for(auto at=source;;++at){const auto byte=Monstey_Get(at,1);if(byte==0)break;text.push_back(static_cast<char>(byte));}
   // Logical C-string assignment is qualified; no fabricated physical string
   // capacity/growth or original member ABI is exported by this adapter.
  };
  s.string_destroy=[this](auto c){if(c.target!=0xB074||strings.erase(c.object)!=1)throw std::runtime_error("Owned string destruction dispatch differs");};
  s.map_value=[this](auto map,auto key){
   if(map!=Monstey_Map||!strings.contains(key))throw std::runtime_error("Owned constraint map dispatch differs");
   const auto& name=strings.at(key);auto found=cells.find(name);
   if(found==cells.end())found=cells.emplace(name,Monstey_Allocate(12)).first;
   ++records;return found->second;
  };
  return s;
 }
};
}
Monstey_FearConstraintTable Monstey_FearDecodeConstraints(std::span<const std::uint8_t> input) {
 Monstey_ConstraintAdapter context(input);auto services=context.Monstey_Services();
 Monstey_FearConstraintTable report;
 report.complete=Monstey_FearReadConstraints(context.memory,Monstey_Frame,services);
 report.consumed=context.position;report.read_calls=context.reads;report.records=context.records;
 for(const auto& [key,address]:context.cells)report.rows.push_back({key,context.Monstey_Get(address),context.Monstey_Get(address+4),context.Monstey_Get(address+8)});
 return report;
}
}
