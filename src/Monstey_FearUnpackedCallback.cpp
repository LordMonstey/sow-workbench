#include "Monstey_FearUnpackedCallback.hpp"
#include <utility>
namespace Monstey {
Monstey_FearUnpackedCallbackBoundaryFault::Monstey_FearUnpackedCallbackBoundaryFault(const char* name,std::uint32_t site,std::uint32_t value):std::runtime_error(std::string("Consumed unpacked callback boundary: ")+name),native_site(site),target(value){}
namespace {
using U=std::uint32_t;using Mem=Monstey_FearDbMutableMemory;using Op=Monstey_FearUnpackedStringOp;using Services=Monstey_FearUnpackedCallbackServices;
std::size_t Monstey_Offset(Mem m,U at,U width,bool write){if(at<m.base||std::uint64_t(at)+width>0x100000000ull||std::uint64_t(at)-m.base>m.bytes.size()||width>m.bytes.size()-static_cast<std::size_t>(std::uint64_t(at)-m.base))throw Monstey_FearDbFault(at,width,write);return static_cast<std::size_t>(at-m.base);}
U Monstey_Read(Mem m,U at,U width=4){const auto k=Monstey_Offset(m,at,width,false);U v=0;for(U n=0;n<width;++n)v|=U(m.bytes[k+n])<<(n*8);return v;}
void Monstey_Write(Mem m,U at,U value,U width=4){const auto k=Monstey_Offset(m,at,width,true);for(U n=0;n<width;++n)m.bytes[k+n]=static_cast<std::uint8_t>(value>>(n*8));}
void Monstey_Push(Mem m,U&sp,U value){sp-=4u;Monstey_Write(m,sp,value);}
U Monstey_String(Mem&m,const Services&s,Op op,U site,U slot,U receiver,U a=0,U b=0,U c=0){const U target=Monstey_Read(m,slot);if(!s.string)throw Monstey_FearUnpackedCallbackBoundaryFault("string",site,target);return s.string(m,{op,target,receiver,a,b,c,site});}
U Monstey_Less(Mem&m,const Services&s,U site,U target,U left,U right){if(!s.string)throw Monstey_FearUnpackedCallbackBoundaryFault("string_less",site,target);return s.string(m,{Op::less,target,left,right,0,0,site});}
}
std::uint32_t Monstey_FearUnpackedCallbackLowerBound(Mem&m,U map,U argument,bool parsed,const Services&s){
 const U head=Monstey_Read(m,map+4u);U node=Monstey_Read(m,head+4u),candidate=head;const U nil=parsed?0x4Du:0x35u;
 if(Monstey_Read(m,node+nil,1)==0){const U key=Monstey_Read(m,argument);const U target=Monstey_Read(m,0x1000B044u);
  do{U sp=argument-0x14u;Monstey_Push(m,sp,key);Monstey_Push(m,sp,node+0x0Cu);
   const U less=Monstey_Less(m,s,parsed?0x10003FB5u:0x10004065u,target,node+0x0Cu,key);
   if((less&255u)!=0)node=Monstey_Read(m,node+8u);else{candidate=node;node=Monstey_Read(m,node);}
  }while(Monstey_Read(m,node+nil,1)==0);
 }return candidate;
}
std::uint32_t Monstey_FearUnpackedCallbackParsedValue(Mem&m,U map,U g,const Services&s){
 const U key=Monstey_Read(m,g+0x74u);U sp=g;Monstey_Push(m,sp,key);
 U selected=Monstey_FearUnpackedCallbackLowerBound(m,map,sp,true,s);
 bool insert=selected==Monstey_Read(m,map+4u);
 if(!insert){sp=g;Monstey_Push(m,sp,selected+0x0Cu);Monstey_Push(m,sp,key);const U target=Monstey_Read(m,0x1000B044u);insert=(Monstey_Less(m,s,0x10005C2Eu,target,key,selected+0x0Cu)&255u)!=0;}
 if(insert){
  Monstey_Write(m,g+0x0Cu,0);Monstey_String(m,s,Op::construct,0x10005C47u,0x1000B064u,g+0x10u);
  sp=g;Monstey_Push(m,sp,key);Monstey_String(m,s,Op::copy,0x10005C52u,0x1000B078u,g+0x30u,key);
  const U type=Monstey_Read(m,g+0x0Cu);Monstey_Write(m,g+0x4Cu,type);sp=g;Monstey_Push(m,sp,g+0x10u);Monstey_String(m,s,Op::copy,0x10005C69u,0x1000B078u,g+0x50u,g+0x10u);
  const U opaque=Monstey_Read(m,g+0x2Cu);sp=g;Monstey_Push(m,sp,g+0x30u);Monstey_Push(m,sp,selected);Monstey_Push(m,sp,g+0x74u);Monstey_Write(m,g+0x6Cu,opaque);
  if(!s.insert)throw Monstey_FearUnpackedCallbackBoundaryFault("map_insert5890",0x10005C84u);
  const U returned=s.insert(m,map,g+0x74u,selected,g+0x30u);selected=Monstey_Read(m,returned);
  Monstey_String(m,s,Op::destroy,0x10005C8Fu,0x1000B074u,g+0x50u);Monstey_String(m,s,Op::destroy,0x10005C99u,0x1000B074u,g+0x30u);Monstey_String(m,s,Op::destroy,0x10005CA3u,0x1000B074u,g+0x10u);
 }return selected+0x28u;
}
std::uint8_t Monstey_FearUnpackedLineCallback(Mem&m,U f,U lineCell,U contextCell,const Services&s){
 const U line=Monstey_Read(m,lineCell);U sp=f;Monstey_Push(m,sp,line);Monstey_String(m,s,Op::construct_cstring,0x10005ED6u,0x1000B06Cu,f+0x34u,line);
 const U context=Monstey_Read(m,contextCell);const U constraints=Monstey_Read(m,context),parsed=Monstey_Read(m,context+4u);
 sp=f;Monstey_Push(m,sp,0);Monstey_Push(m,sp,61);const U delimiter=Monstey_String(m,s,Op::find,0x10005EF0u,0x1000B040u,f+0x34u,61,0);
 sp=f;Monstey_Push(m,sp,delimiter);Monstey_Push(m,sp,0);Monstey_Push(m,sp,f+0x18u);(void)Monstey_String(m,s,Op::substr,0x10005F04u,0x1000B050u,f+0x34u,f+0x18u,0,delimiter);
 sp=f;Monstey_Push(m,sp,f+0x18u);const U lower=Monstey_FearUnpackedCallbackLowerBound(m,constraints,sp,false,s);
 bool missing=lower==Monstey_Read(m,constraints+4u);Monstey_Write(m,f+0x10u,lower);
 if(!missing){sp=f;Monstey_Push(m,sp,lower+0x0Cu);Monstey_Push(m,sp,f+0x18u);const U target=Monstey_Read(m,0x1000B044u);missing=(Monstey_Less(m,s,0x10005F28u,target,f+0x18u,lower+0x0Cu)&255u)!=0;}
 U iteratorCell=f+0x10u;if(missing){const U currentHead=Monstey_Read(m,constraints+4u);Monstey_Write(m,f+0x14u,currentHead);iteratorCell=f+0x14u;}
 const U constraintNode=Monstey_Read(m,iteratorCell);
 if(constraintNode==Monstey_Read(m,constraints+4u)){Monstey_String(m,s,Op::destroy,0x10005F51u,0x1000B074u,f+0x18u);Monstey_String(m,s,Op::destroy,0x10005F5Bu,0x1000B074u,f+0x34u);return 0;}
 const U npos=Monstey_Read(m,Monstey_Read(m,s.npos_pointer_cell));const U start=delimiter+1u;
 sp=f;Monstey_Push(m,sp,npos);Monstey_Push(m,sp,start);Monstey_Push(m,sp,f+0x50u);(void)Monstey_String(m,s,Op::substr,0x10005F82u,0x1000B050u,f+0x34u,f+0x50u,start,npos);
 const U constraintValue=constraintNode+0x28u;if(!s.validate)throw Monstey_FearUnpackedCallbackBoundaryFault("validator3F20",0x10005F8Fu);(void)s.validate(m,constraintValue,f+0x50u);
 Monstey_Write(m,f+0x6Cu,0);Monstey_String(m,s,Op::construct,0x10005FA0u,0x1000B064u,f+0x70u);
 const U type=Monstey_Read(m,constraintValue);sp=f;Monstey_Push(m,sp,f+0x50u);Monstey_Write(m,f+0x6Cu,type);Monstey_String(m,s,Op::assign,0x10005FB5u,0x1000B054u,f+0x70u,f+0x50u);
 sp=f;Monstey_Push(m,sp,f+0x18u);const U value=Monstey_FearUnpackedCallbackParsedValue(m,parsed,f-0x78u,s);
 const U finalType=Monstey_Read(m,f+0x6Cu);sp=f;Monstey_Push(m,sp,f+0x70u);Monstey_Write(m,value,finalType);Monstey_String(m,s,Op::assign,0x10005FD7u,0x1000B054u,value+4u,Monstey_Read(m,f-4u));
 const U opaque=Monstey_Read(m,f+0x8Cu);Monstey_Write(m,value+0x20u,opaque);
 Monstey_String(m,s,Op::destroy,0x10005FEBu,0x1000B074u,f+0x70u);Monstey_String(m,s,Op::destroy,0x10005FF5u,0x1000B074u,f+0x50u);Monstey_String(m,s,Op::destroy,0x10005FFFu,0x1000B074u,f+0x18u);Monstey_String(m,s,Op::destroy,0x10006009u,0x1000B074u,f+0x34u);return 1;
}
namespace {
struct Monstey_LogicalLocation {U node,head,parent;bool left,fresh;};
Monstey_LogicalLocation Monstey_LogicalNode(Mem m,const Monstey_FearUnpackedCallbackLogicalMaps&p,U map,U key,bool parsed){
 const U head=Monstey_Read(m,map+4u),nil=parsed?0x4Du:0x35u;U node=Monstey_Read(m,head+4u),parent=head;bool left=false;
 while(Monstey_Read(m,node+nil,1)==0){parent=node;if(p.strings.Invoke(m,Op::less,key,node+0x0Cu)){left=true;node=Monstey_Read(m,node);}else if(p.strings.Invoke(m,Op::less,node+0x0Cu,key)){left=false;node=Monstey_Read(m,node+8u);}else{return{node,head,parent,left,false};}}
 if(!p.allocate)throw Monstey_FearUnpackedCallbackBoundaryFault("logical_node_allocate",0);
 node=p.allocate(m,parsed?0x50u:0x38u);Monstey_Write(m,node,head);Monstey_Write(m,node+4u,parent);Monstey_Write(m,node+8u,head);Monstey_Write(m,node+nil-1u,0,1);Monstey_Write(m,node+nil,0,1);p.strings.Invoke(m,Op::copy,node+0x0Cu,key);
 return{node,head,parent,left,true};
}
void Monstey_LogicalPublish(Mem m,U map,Monstey_LogicalLocation at){
 if(!at.fresh)return;const U node=at.node,head=at.head,parent=at.parent;
 if(parent==head){Monstey_Write(m,head+4u,node);Monstey_Write(m,head,node);Monstey_Write(m,head+8u,node);}else if(at.left){Monstey_Write(m,parent,node);if(parent==Monstey_Read(m,head))Monstey_Write(m,head,node);}else{Monstey_Write(m,parent+8u,node);if(parent==Monstey_Read(m,head+8u))Monstey_Write(m,head+8u,node);}
 const U count=Monstey_Read(m,map+8u);Monstey_Write(m,map+8u,count+1u);
}
}
void Monstey_FearUnpackedCallbackLogicalMaps::Initialize(Mem m,U map,bool parsed)const{
 if(!allocate)throw Monstey_FearUnpackedCallbackBoundaryFault("logical_head_allocate",0);const U head=allocate(m,parsed?0x50u:0x38u),nil=parsed?0x4Du:0x35u;Monstey_Write(m,head,head);Monstey_Write(m,head+4u,head);Monstey_Write(m,head+8u,head);Monstey_Write(m,head+nil-1u,1,1);Monstey_Write(m,head+nil,1,1);Monstey_Write(m,map+4u,head);Monstey_Write(m,map+8u,0);
}
std::uint32_t Monstey_FearUnpackedCallbackLogicalMaps::InsertParsed(Mem&m,U map,U hidden,U hint,U pair)const{
 (void)hint;const auto at=Monstey_LogicalNode(m,*this,map,pair,true);const U node=at.node;
 if(at.fresh){const U type=Monstey_Read(m,pair+0x1Cu);Monstey_Write(m,node+0x28u,type);strings.Invoke(m,Op::copy,node+0x2Cu,pair+0x20u);const U opaque=Monstey_Read(m,pair+0x3Cu);Monstey_Write(m,node+0x48u,opaque);}Monstey_LogicalPublish(m,map,at);Monstey_Write(m,hidden,node);return hidden;
}
void Monstey_FearUnpackedCallbackLogicalMaps::PutConstraint(Mem m,U map,U key,U type,U word4,U word8)const{
 const auto at=Monstey_LogicalNode(m,*this,map,key,false);const U node=at.node;Monstey_Write(m,node+0x28u,type);Monstey_Write(m,node+0x2Cu,word4);Monstey_Write(m,node+0x30u,word8);Monstey_LogicalPublish(m,map,at);
}
}
