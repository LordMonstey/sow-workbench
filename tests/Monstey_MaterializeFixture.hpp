#pragma once
#include "Monstey_MaterializeNativeReference.hpp"
#include <string>
#include <vector>
#include <algorithm>
namespace Monstey_IndependentMaterialize {
struct Monstey_StringFixture {
 static constexpr std::uint32_t base=0x10000000u,context=base+0x100,map=base+0x200,head=base+0x300,node=base+0x400,frame=base+0x4000;
 std::vector<std::uint8_t> image=std::vector<std::uint8_t>(0x10000,0xA5);bool high_bool=false;
 Monstey::Monstey_FearDbMutableMemory Monstey_Memory(){return {base,image};}
 void Monstey_Set(std::uint32_t at,const std::string& value) {
  if(value.size()>15)throw std::runtime_error("Independent small-string fixture length");
  const Monstey_RefMemory m(Monstey_Memory());m.Monstey_Write(at+0x14,static_cast<std::uint32_t>(value.size()));m.Monstey_Write(at+0x18,15);
  for(std::size_t i=0;i<value.size();++i)m.Monstey_Write(at+4+static_cast<std::uint32_t>(i),static_cast<unsigned char>(value[i]),1);
  m.Monstey_Write(at+4+static_cast<std::uint32_t>(value.size()),0,1);
 }
 std::string Monstey_Get(std::uint32_t at) {
  const Monstey_RefMemory m(Monstey_Memory());const auto length=m.Monstey_Read(at+0x14),capacity=m.Monstey_Read(at+0x18);
  const auto data=capacity<=15?at+4:m.Monstey_Read(at+4);std::string result;
  for(std::uint32_t i=0;i<length;++i)result.push_back(static_cast<char>(m.Monstey_Read(data+i,1)));return result;
 }
 Monstey_StringFixture(std::string key,std::string value,std::uint32_t type) {
  const Monstey_RefMemory m(Monstey_Memory());for(unsigned i=0;i<6;++i)m.Monstey_Write(context+4*i,0);
  m.Monstey_Write(map+4,head);m.Monstey_Write(head,node);m.Monstey_Write(head+4,head);m.Monstey_Write(head+8,node);m.Monstey_Write(head+0x4D,1,1);
  m.Monstey_Write(node,head);m.Monstey_Write(node+4,head);m.Monstey_Write(node+8,head);m.Monstey_Write(node+0x4D,0,1);
  Monstey_Set(node+0xC,key);Monstey_Set(node+0x2C,value);m.Monstey_Write(node+0x28,type);m.Monstey_Write(node+0x48,0x87654321);
  m.Monstey_Write(frame+0xBC,map);m.Monstey_Write(base+0x80,base+0x90);m.Monstey_Write(base+0x90,0xFFFFFFFF);
 }
 Monstey::Monstey_FearUnpackedImports Monstey_Imports() {
  Monstey::Monstey_FearUnpackedImports imports;imports.npos_pointer_cell=base+0x80;imports.empty_cstring=base+0xA0;imports.root_name_cell=base+0xA4;
  imports.string=[this](Monstey::Monstey_FearDbMutableMemory memory,Monstey::Monstey_FearUnpackedStringOp op,std::uint32_t receiver,std::uint32_t a,std::uint32_t b,std::uint32_t c)->std::uint32_t {
   using Op=Monstey::Monstey_FearUnpackedStringOp;const Monstey_RefMemory m(memory);
   switch(op) {
   case Op::construct:case Op::clear:Monstey_Set(receiver,"");return receiver;
   case Op::destroy:return receiver;
   case Op::copy:case Op::assign:{const auto value=Monstey_Get(a);Monstey_Set(receiver,value);return receiver;}
   case Op::c_str:return m.Monstey_Read(receiver+0x18)<=15?receiver+4:m.Monstey_Read(receiver+4);
   case Op::subscript:return (m.Monstey_Read(receiver+0x18)<=15?receiver+4:m.Monstey_Read(receiver+4))+a;
   case Op::append:{auto value=Monstey_Get(receiver);value.push_back(static_cast<char>(a));Monstey_Set(receiver,value);return receiver;}
   case Op::find:{const auto value=Monstey_Get(receiver);const auto p=value.find(static_cast<char>(a),b);return p==std::string::npos?0xFFFFFFFFu:static_cast<std::uint32_t>(p);}
   case Op::rfind:{const auto value=Monstey_Get(receiver);const auto p=value.rfind(static_cast<char>(a),b==0xFFFFFFFFu?std::string::npos:b);return p==std::string::npos?0xFFFFFFFFu:static_cast<std::uint32_t>(p);}
   case Op::substr:{const auto value=Monstey_Get(receiver).substr(b,c==0xFFFFFFFFu?std::string::npos:c);Monstey_Set(a,value);return a;}
   case Op::unequal:{const bool value=Monstey_Get(receiver)!=Monstey_Get(a);return (high_bool?0x100u:0u)|(value?1u:0u);}
   default:throw std::runtime_error("Unexpected independent imported string primitive");
   }
  };
  return imports;
 }
};
}