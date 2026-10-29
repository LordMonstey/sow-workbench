#pragma once
#include "Monstey_FearConstraintReader.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace Monstey_Test {
using U8=std::uint8_t;using U32=std::uint32_t;using U64=std::uint64_t;
inline constexpr U32 Base=0x10000000,Frame=0x10012000,Vtable=0x10011100;
inline void Word(std::vector<U8>& b,U32 v){for(unsigned i=0;i<4;++i)b.push_back(static_cast<U8>(v>>(8*i)));}
inline void Record(std::vector<U8>& b,const std::string& key,U32 type,U32 discard,U32 second,U32 third){Word(b,static_cast<U32>(key.size()));for(char c:key)b.push_back(static_cast<U8>(static_cast<unsigned char>(c)));b.push_back(static_cast<U8>(type));Word(b,discard);Word(b,second);Word(b,third);}
struct Fixture {
 std::vector<U8> data=std::vector<U8>(0x30000,0xCD),input;
 Monstey::Monstey_FearDbMutableMemory memory{Base,data};
 Monstey::Monstey_FearConstraintServices services;
 U32 stream=0x10011033,map=0x10011200,next=0x10015000,last_allocation=0,value=0x10018000,expected_destroy=0xB074;
 U64 position=0,reads=0,pos_calls=0,alloc_calls=0,free_calls=0,ctor_calls=0,dtor_calls=0,map_calls=0;
 std::optional<U64> total;std::vector<Monstey::Monstey_FearConstraintReadCall> requests;
 std::vector<U32> allocated_sizes,destroy_targets;std::string key;
 std::function<std::optional<U32>(Fixture&,Monstey::Monstey_FearConstraintReadCall)> on_read;
 std::function<void(Fixture&)> on_alloc,on_ctor,on_map,on_position;
 Fixture(){Put(Frame+0x48,stream);Put(Frame+0x4C,map);Put(stream,Vtable);Put(Vtable+4,0xA004);Put(Vtable+0x18,0xA018);Put(Vtable+0x1C,0xA01C);Put(0x1000B064,0xB064);Put(0x1000B068,0xB068);Put(0x1000B074,0xB074);Bind();}
 std::span<U8> Range(U32 at,U32 n,bool writing){const auto o=static_cast<U64>(at)-Base;if(at<Base||static_cast<U64>(at)+n>0x100000000ull||o>data.size()||n>data.size()-static_cast<std::size_t>(o))throw Monstey::Monstey_FearDbFault(at,n,writing);return std::span<U8>(data).subspan(static_cast<std::size_t>(o),n);}
 U32 Get(U32 at,U32 n=4){const auto b=Range(at,n,false);U32 x=0;for(U32 i=0;i<n;++i)x|=U32(b[i])<<(8*i);return x;}
 void Put(U32 at,U32 x,U32 n=4){auto b=Range(at,n,true);for(U32 i=0;i<n;++i)b[i]=static_cast<U8>(x>>(8*i));}
 void Call(Monstey::Monstey_FearConstraintCall c,U32 target){if(c.object!=stream||c.target!=target)throw std::runtime_error("stream identity/target differs");}
 void Bind(){
  services.get_length=[this](auto c){Call(c,0xA01C);return total.value_or(static_cast<U64>(input.size()));};
  services.get_position=[this](auto c){Call(c,0xA018);++pos_calls;if(on_position)on_position(*this);return position;};
  services.read=[this](Monstey::Monstey_FearConstraintReadCall c){
   if(c.stream!=stream||(c.target!=0xA004&&c.target!=0xA005))throw std::runtime_error("read dispatch differs");++reads;requests.push_back(c);
   if(on_read){const auto status=on_read(*this,c);if(status)return *status;}
   const bool ok=position<=input.size()&&c.bytes<=input.size()-static_cast<std::size_t>(position);
   if(c.bytes){auto b=Range(c.destination,c.bytes,true);if(ok)std::copy_n(input.begin()+static_cast<std::ptrdiff_t>(position),c.bytes,b.begin());else std::fill(b.begin(),b.end(),U8{0});}
   if(ok)position+=c.bytes;return ok?U32{0}:U32{1};
  };
  services.new_array=[this](U32 n){++alloc_calls;allocated_sizes.push_back(n);last_allocation=next;if(on_alloc)on_alloc(*this);return last_allocation;};
  services.delete_array=[this](U32 p){if(p!=last_allocation)throw std::runtime_error("free target differs");++free_calls;};
  services.string_default=[this](auto c){if(c.target!=0xB064||c.object!=Frame+0x28)throw std::runtime_error("string constructor differs");++ctor_calls;Put(c.object+0x18,15);Put(c.object+0x14,0);Put(c.object+4,0,1);if(on_ctor)on_ctor(*this);};
  services.string_assign_cstr=[this](auto c,U32 p){if(c.target!=0xB068||c.object!=Frame+0x28)throw std::runtime_error("string assign differs");key.clear();for(;;++p){const auto b=Get(p,1);if(!b)break;key.push_back(static_cast<char>(b));}};
  services.string_destroy=[this](auto c){if(c.target!=expected_destroy||c.object!=Frame+0x28)throw std::runtime_error("string destroy differs");destroy_targets.push_back(c.target);++dtor_calls;};
  services.map_value=[this](U32 m,U32 k){if(m!=map||k!=Frame+0x28)throw std::runtime_error("map call differs");++map_calls;if(on_map)on_map(*this);return value;};
 }
 bool Run(){return Monstey::Monstey_FearReadConstraints(memory,Frame,services);}
};
}
