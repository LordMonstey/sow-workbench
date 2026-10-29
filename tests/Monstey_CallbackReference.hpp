#pragma once
#include "Monstey_FearUnpackedCallback.hpp"
#include <cstdint>
namespace Monstey_CallbackReference {
using U=std::uint32_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Svc=Monstey::Monstey_FearUnpackedCallbackServices;using Op=Monstey::Monstey_FearUnpackedStringOp;
struct Boundary{U site,target;};
inline U Read(const Mem&m,U at,U width=4){if(at<m.base||std::uint64_t(at)+width>0x100000000ull||std::uint64_t(at)-m.base>m.bytes.size()||width>m.bytes.size()-static_cast<std::size_t>(at-m.base))throw Monstey::Monstey_FearDbFault(at,width,false);U value=0;for(U n=0;n<width;++n)value|=U(m.bytes[at-m.base+n])<<(n*8u);return value;}
inline void Write(Mem&m,U at,U value,U width=4){if(at<m.base||std::uint64_t(at)+width>0x100000000ull||std::uint64_t(at)-m.base>m.bytes.size()||width>m.bytes.size()-static_cast<std::size_t>(at-m.base))throw Monstey::Monstey_FearDbFault(at,width,true);for(U n=0;n<width;++n)m.bytes[at-m.base+n]=static_cast<std::uint8_t>(value>>(n*8u));}
inline U String(Mem&m,const Svc&s,Op op,U target,U receiver,U a,U b,U c,U site){if(!s.string)throw Boundary{site,target};return s.string(m,{op,target,receiver,a,b,c,site});}
inline U Fixed(Mem&m,const Svc&s,Op op,U slot,U receiver,U a,U b,U c,U site){const U target=Read(m,slot);return String(m,s,op,target,receiver,a,b,c,site);}
// Complete3F90/4040. Cache query and operator< only on the nonempty path.
inline U LowerBound(Mem&m,U map,U keyCell,bool parsed,const Svc&s){
 const U head=Read(m,map+4);U node=Read(m,head+4),candidate=head;const U nil=parsed?0x4Du:0x35u;
 if(Read(m,node+nil,1)!=0)return candidate;
 const U key=Read(m,keyCell),target=Read(m,0x1000B044u);const U sp=keyCell-0x14u;
 for(;;){const U receiver=node+0xCu;Write(m,sp-4,key);Write(m,sp-8,receiver);
  const U left=Read(m,sp-8),right=Read(m,sp-4);const U result=String(m,s,Op::less,target,left,right,0,0,parsed?0x10003FB5u:0x10004065u);
  if((result&255u)!=0)node=Read(m,node+8);else{candidate=node;node=Read(m,node);}
  if(Read(m,node+nil,1)!=0)return candidate;
 }
}
// Complete5C10. The opaque G+2C word is supplied, never initialized here.
inline U ParsedValue(Mem&m,U map,U g,const Svc&s){
 const U key=Read(m,g+0x74u);Write(m,g-4,key);U selected=LowerBound(m,map,g-4,true,s);
 bool insert=selected==Read(m,map+4);
 if(!insert){Write(m,g-4,selected+0xCu);Write(m,g-8,key);const U left=Read(m,g-8),right=Read(m,g-4);insert=(Fixed(m,s,Op::less,0x1000B044u,left,right,0,0,0x10005C2Eu)&255u)!=0;}
 if(insert){
  Write(m,g+0xCu,0);(void)Fixed(m,s,Op::construct,0x1000B064u,g+0x10u,0,0,0,0x10005C47u);
  Write(m,g-4,key);(void)Fixed(m,s,Op::copy,0x1000B078u,g+0x30u,Read(m,g-4),0,0,0x10005C52u);
  const U type=Read(m,g+0xCu);Write(m,g+0x4Cu,type);Write(m,g-4,g+0x10u);
  (void)Fixed(m,s,Op::copy,0x1000B078u,g+0x50u,Read(m,g-4),0,0,0x10005C69u);
  const U opaque=Read(m,g+0x2Cu);Write(m,g-4,g+0x30u);Write(m,g-8,selected);Write(m,g-12,g+0x74u);Write(m,g+0x6Cu,opaque);
  if(!s.insert)throw Boundary{0x10005C84u,0};
  const U result=s.insert(m,map,Read(m,g-12),Read(m,g-8),Read(m,g-4));selected=Read(m,result);
  (void)Fixed(m,s,Op::destroy,0x1000B074u,g+0x50u,0,0,0,0x10005C8Fu);
  (void)Fixed(m,s,Op::destroy,0x1000B074u,g+0x30u,0,0,0,0x10005C99u);
  (void)Fixed(m,s,Op::destroy,0x1000B074u,g+0x10u,0,0,0,0x10005CA3u);
 }
 return selected+0x28u;
}
// Complete5EC0..601C from static native operands, not owner CPP.
inline std::uint8_t Callback(Mem&m,U f,U lineCell,U contextCell,const Svc&s){
 const U line=Read(m,lineCell);Write(m,f-4,line);
 (void)Fixed(m,s,Op::construct_cstring,0x1000B06Cu,f+0x34u,Read(m,f-4),0,0,0x10005ED6u);
 const U context=Read(m,contextCell),constraints=Read(m,context),parsed=Read(m,context+4u);
 Write(m,f-4,0);Write(m,f-8,0x3D);U equal=Fixed(m,s,Op::find,0x1000B040u,f+0x34u,Read(m,f-8),Read(m,f-4),0,0x10005EF0u);
 Write(m,f-4,equal);Write(m,f-8,0);Write(m,f-12,f+0x18u);
 (void)Fixed(m,s,Op::substr,0x1000B050u,f+0x34u,Read(m,f-12),Read(m,f-8),Read(m,f-4),0x10005F04u);
 Write(m,f-4,f+0x18u);const U found=LowerBound(m,constraints,f-4,false,s);const bool atHead=found==Read(m,constraints+4u);Write(m,f+0x10u,found);
 U iteratorCell=f+0x10u;bool absent=atHead;
 if(!atHead){Write(m,f-4,found+0xCu);Write(m,f-8,f+0x18u);absent=(Fixed(m,s,Op::less,0x1000B044u,Read(m,f-8),Read(m,f-4),0,0,0x10005F28u)&255u)!=0;}
 if(absent){const U head=Read(m,constraints+4u);Write(m,f+0x14u,head);iteratorCell=f+0x14u;}
 const U selected=Read(m,iteratorCell);
 if(selected==Read(m,constraints+4u)){
  (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x18u,0,0,0,0x10005F51u);
  (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x34u,0,0,0,0x10005F5Bu);return 0;
 }
 const U npos=Read(m,Read(m,s.npos_pointer_cell));Write(m,f-4,npos);++equal;Write(m,f-8,equal);Write(m,f-12,f+0x50u);
 (void)Fixed(m,s,Op::substr,0x1000B050u,f+0x34u,Read(m,f-12),Read(m,f-8),Read(m,f-4),0x10005F82u);
 const U constraintValue=selected+0x28u;if(!s.validate)throw Boundary{0x10005F8Fu,0};(void)s.validate(m,constraintValue,f+0x50u);
 Write(m,f+0x6Cu,0);(void)Fixed(m,s,Op::construct,0x1000B064u,f+0x70u,0,0,0,0x10005FA0u);
 const U type=Read(m,constraintValue);Write(m,f-4,f+0x50u);Write(m,f+0x6Cu,type);
 (void)Fixed(m,s,Op::assign,0x1000B054u,f+0x70u,Read(m,f-4),0,0,0x10005FB5u);
 Write(m,f-4,f+0x18u);const U value=ParsedValue(m,parsed,f-0x78u,s);const U selectedType=Read(m,f+0x6Cu);
 Write(m,f-4,f+0x70u);Write(m,value,selectedType);
 (void)Fixed(m,s,Op::assign,0x1000B054u,value+4u,Read(m,f-4),0,0,0x10005FD7u);
 const U opaque=Read(m,f+0x8Cu);Write(m,value+0x20u,opaque);
 (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x70u,0,0,0,0x10005FEBu);
 (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x50u,0,0,0,0x10005FF5u);
 (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x18u,0,0,0,0x10005FFFu);
 (void)Fixed(m,s,Op::destroy,0x1000B074u,f+0x34u,0,0,0,0x10006009u);return 1;
}
}
