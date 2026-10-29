#pragma once
#include "Monstey_FearConstraintReader.hpp"
#include <cstdint>
#include <cstddef>
namespace Monstey_Independent {
enum class Monstey_Boundary {Length,Position,Read,Allocate,Delete,StringDefault,StringAssign,StringDestroy,Map};
struct Monstey_RefFault {std::uint32_t address;std::uint32_t width;bool writing;};
struct Monstey_RefBoundary {Monstey_Boundary kind;};
class Monstey_Memory {
 Monstey::Monstey_FearDbMutableMemory& m_;
 std::size_t Offset(std::uint32_t at,std::uint32_t n,bool write)const{
  if(at<m_.base||std::uint64_t(at)+n>0x100000000ull||std::uint64_t(at)-m_.base>m_.bytes.size()||
     n>m_.bytes.size()-static_cast<std::size_t>(at-m_.base))throw Monstey_RefFault{at,n,write};
  return static_cast<std::size_t>(at-m_.base);
 }
public:
 explicit Monstey_Memory(Monstey::Monstey_FearDbMutableMemory& m):m_(m){}
 std::uint32_t Read(std::uint32_t at,std::uint32_t n=4)const{
  const auto p=Offset(at,n,false);std::uint32_t result=0;
  for(auto i=n;i!=0;--i)result=(result<<8)|m_.bytes[p+i-1];return result;
 }
 void Write(std::uint32_t at,std::uint32_t value,std::uint32_t n=4)const{
  const auto p=Offset(at,n,true);
  for(std::uint32_t i=0;i<n;++i){m_.bytes[p+i]=static_cast<std::uint8_t>(value&255u);value>>=8;}
 }
};
template<class Monstey_Fn,class...Monstey_Args>
decltype(auto) Monstey_Invoke(Monstey_Fn& fn,Monstey_Boundary boundary,Monstey_Args...args){
 if(!fn)throw Monstey_RefBoundary{boundary};return fn(args...);
}
// Independent native-derived60020 reference. No candidate implementation used.
// The memory object is a reference; no borrowed span is retained across a call.
inline bool Monstey_ReferenceConstraints(Monstey::Monstey_FearDbMutableMemory& view,
 std::uint32_t frame,Monstey::Monstey_FearConstraintServices& s) {
 using Monstey_Call=Monstey::Monstey_FearConstraintCall;
 using Monstey_ReadCall=Monstey::Monstey_FearConstraintReadCall;
 const Monstey_Memory m(view);
 const auto stream=m.Read(frame+0x48);
 auto table=m.Read(stream);
 auto target=m.Read(table+0x1C);
 const auto full_length=Monstey_Invoke(s.get_length,Monstey_Boundary::Length,Monstey_Call{target,stream});
 //6034 loads table BEFORE603A/603E write the cached length into the frame.
 table=m.Read(stream);
 m.Write(frame+0x20,static_cast<std::uint32_t>(full_length));
 m.Write(frame+0x24,static_cast<std::uint32_t>(full_length>>32));
 target=m.Read(table+0x18);
 auto position=Monstey_Invoke(s.get_position,Monstey_Boundary::Position,Monstey_Call{target,stream});
 if(position==full_length)return true;
 for(;;) {
  table=m.Read(stream);m.Write(frame+0x10,0);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,frame+0x10,4})!=0)return false;
  const auto allocation_size=m.Read(frame+0x10)+1u;
  const auto allocation=Monstey_Invoke(s.new_array,Monstey_Boundary::Allocate,allocation_size);
  //6078 reloads name length after allocator;6094 reloads it after zero writes.
  const auto initialized=m.Read(frame+0x10)+1u;
  for(std::uint32_t i=0;i<(initialized>>2);++i)m.Write(allocation+4u*i,0);
  for(std::uint32_t i=0;i<(initialized&3u);++i)m.Write(allocation+(initialized&~3u)+i,0,1);
  const auto payload_bytes=m.Read(frame+0x10);
  table=m.Read(stream);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,allocation,payload_bytes})!=0) {
   Monstey_Invoke(s.delete_array,Monstey_Boundary::Delete,allocation);return false;
  }
  target=m.Read(0x1000B064u);
  Monstey_Invoke(s.string_default,Monstey_Boundary::StringDefault,Monstey_Call{target,frame+0x28});
  target=m.Read(0x1000B068u);
  Monstey_Invoke(s.string_assign_cstr,Monstey_Boundary::StringAssign,Monstey_Call{target,frame+0x28},allocation);
  Monstey_Invoke(s.delete_array,Monstey_Boundary::Delete,allocation);
  // Native ordering is VT read, destination zero, then target read for every field.
  table=m.Read(stream);m.Write(frame+0x48,0,1);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,frame+0x48,1})!=0) {
   target=m.Read(0x1000B074u);Monstey_Invoke(s.string_destroy,Monstey_Boundary::StringDestroy,Monstey_Call{target,frame+0x28});return false;
  }
  table=m.Read(stream);m.Write(frame+0x1C,0);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,frame+0x1C,4})!=0) {
   target=m.Read(0x1000B074u);Monstey_Invoke(s.string_destroy,Monstey_Boundary::StringDestroy,Monstey_Call{target,frame+0x28});return false;
  }
  table=m.Read(stream);m.Write(frame+0x14,0);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,frame+0x14,4})!=0) {
   target=m.Read(0x1000B074u);Monstey_Invoke(s.string_destroy,Monstey_Boundary::StringDestroy,Monstey_Call{target,frame+0x28});return false;
  }
  table=m.Read(stream);m.Write(frame+0x18,0);target=m.Read(table+4);
  if(Monstey_Invoke(s.read,Monstey_Boundary::Read,Monstey_ReadCall{target,stream,frame+0x18,4})!=0) {
   target=m.Read(0x1000B074u);Monstey_Invoke(s.string_destroy,Monstey_Boundary::StringDestroy,Monstey_Call{target,frame+0x28});return false;
  }
  const auto type=m.Read(frame+0x48,1),word4=m.Read(frame+0x14),word8=m.Read(frame+0x18);
  const auto map=m.Read(frame+0x4C);
  const auto value=Monstey_Invoke(s.map_value,Monstey_Boundary::Map,map,frame+0x28);
  m.Write(value,type);m.Write(value+4,word4);m.Write(value+8,word8);
  target=m.Read(0x1000B074u);Monstey_Invoke(s.string_destroy,Monstey_Boundary::StringDestroy,Monstey_Call{target,frame+0x28});
  table=m.Read(stream);target=m.Read(table+0x18);
  position=Monstey_Invoke(s.get_position,Monstey_Boundary::Position,Monstey_Call{target,stream});
  if(m.Read(frame+0x20)!=static_cast<std::uint32_t>(position))continue;
  if(m.Read(frame+0x24)==static_cast<std::uint32_t>(position>>32))return true;
 }
}
}
