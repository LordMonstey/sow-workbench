#pragma once
#include "Monstey_MaterializeNativeReference.hpp"
namespace Monstey_IndependentMaterialize {
inline void Monstey_RefPush(const Monstey_RefMemory& m,std::uint32_t& sp,std::uint32_t value){sp-=4u;m.Monstey_Write(sp,value);}
// Complete3E11..3E34 argument sequence, then literal6EF0. No CPU return/saved words.
inline void Monstey_RefAttributeCall(Monstey::Monstey_FearDbMutableMemory memory,std::uint32_t frame,std::uint32_t pool,std::uint32_t cached_offset) {
 const Monstey_RefMemory m(memory);auto sp=frame-0x48u;
 auto d=m.Monstey_Read(sp+0x40),a=m.Monstey_Read(sp+0x3C),c=m.Monstey_Read(sp+0x38);
 Monstey_RefPush(m,sp,d);d=m.Monstey_Read(sp+0x38);
 Monstey_RefPush(m,sp,a);a=m.Monstey_Read(sp+0x38);
 Monstey_RefPush(m,sp,c);c=m.Monstey_Read(cached_offset);
 Monstey_RefPush(m,sp,d);c+=pool;Monstey_RefPush(m,sp,a);Monstey_RefPush(m,sp,c);
 const auto out=m.Monstey_Read(sp+0x44);Monstey_RefAttribute16(m,out,sp-4u);
}
// Complete3EC1..3EDA argument sequence, then literal6EA0.
inline void Monstey_RefNodeCall(Monstey::Monstey_FearDbMutableMemory memory,std::uint32_t frame,std::uint32_t pool,std::uint32_t cached_offset) {
 const Monstey_RefMemory m(memory);auto sp=frame-0x40u;
 auto d=m.Monstey_Read(sp+0x38),a=m.Monstey_Read(sp+0x34),c=m.Monstey_Read(sp+0x30);
 Monstey_RefPush(m,sp,d);d=m.Monstey_Read(cached_offset);
 Monstey_RefPush(m,sp,a);Monstey_RefPush(m,sp,c);const auto out=m.Monstey_Read(sp+0x38);
 d+=pool;Monstey_RefPush(m,sp,d);Monstey_RefNode16(m,out,sp-4u);
}
inline void Monstey_RefRootCall(Monstey::Monstey_FearDbMutableMemory memory,std::uint32_t frame,std::uint32_t context,std::uint32_t root,std::uint32_t name_cell) {
 const Monstey_RefMemory m(memory);const auto allocation=m.Monstey_Read(frame+0x13C),categories=m.Monstey_Read(frame+0x3C),count=m.Monstey_Read(context);
 auto sp=frame;Monstey_RefPush(m,sp,allocation);const auto name=m.Monstey_Read(name_cell);
 Monstey_RefPush(m,sp,categories);Monstey_RefPush(m,sp,count);Monstey_RefPush(m,sp,name);
 Monstey_RefRoot20(m,root,sp-4u);Monstey_RefFixLinks(memory,root);
}
}