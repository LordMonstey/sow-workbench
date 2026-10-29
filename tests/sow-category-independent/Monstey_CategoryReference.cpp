#include "Monstey_CategoryReference.hpp"
#include "Monstey_CategoryConcreteBindings.hpp"
#include <bit>
namespace Monstey_CategoryReview {
Monstey_U64 Monstey_Memory::Read(Monstey_U64 a,unsigned w){
 if(accesses.size()==faultAt)throw std::runtime_error("injected memory fault");
 if(w==0||w>8||a<base||a-base>Monstey_bytes.size()||w>Monstey_bytes.size()-(a-base))throw Monstey_ReferenceMemoryFault(a,w,false);
 Monstey_U64 v=0;for(unsigned i=0;i<w;++i)v|=Monstey_U64(Monstey_bytes[static_cast<std::size_t>(a-base)+i])<<(i*8u);
 accesses.push_back({false,a,w,v});return v;
}
void Monstey_Memory::Write(Monstey_U64 a,Monstey_U64 v,unsigned w){
 if(accesses.size()==faultAt)throw std::runtime_error("injected memory fault");
 if(w==0||w>8||a<base||a-base>Monstey_bytes.size()||w>Monstey_bytes.size()-(a-base))throw Monstey_ReferenceMemoryFault(a,w,true);
 accesses.push_back({true,a,w,v});for(unsigned i=0;i<w;++i)Monstey_bytes[static_cast<std::size_t>(a-base)+i]=static_cast<Monstey_U8>(v>>(i*8u));
}
const std::array<Monstey_U8,256> Monstey_Map={
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x25,0x3a,0x33,0x3c,0x3d,0x3e,0x40,0x32,0x42,0x43,0x41,0x28,0x36,0x27,0x37,0x34,
 0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x31,0x30,0x38,0x29,0x39,0x35,
 0x3b,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2c,0x2a,0x2d,0x3f,0x26,
 0x44,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2e,0x2b,0x2f,0x45,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
Monstey_Machine Monstey_Hash(Monstey_Memory& m,Monstey_U64 s,Monstey_U32 seed,Monstey_U64 d){
 auto c=static_cast<Monstey_U8>(m.Read(s,1));
 while(c){++s;d=static_cast<Monstey_U32>(static_cast<std::int32_t>(std::bit_cast<std::int8_t>(Monstey_Map[c])));seed=seed*0x397u+static_cast<Monstey_U32>(d);c=static_cast<Monstey_U8>(m.Read(s,1));}
 return {seed,d};
}
Monstey_RecordExit Monstey_RecordRdx(Monstey_Memory& m,bool named,Monstey_U64 parent,Monstey_U64 originalRdx,Monstey_U64 record,Monstey_U64 stream,Monstey_U64 f,const Monstey_Read& read){
 const auto payloadCell=named?f-0x10:f+0x20;
 const auto descriptorCountCell=named?f+0x38:f+0x58;
 const auto firstCell=named?f-8:f+0x24;
 const auto flagsCell=named?f+0x60:f+0x80;
 const auto poolCell=named?f+0x50:f+0x70;
 const auto descriptorsCell=named?f+0x58:f+0x78;
 const auto cursorCell=named?f+0x68:f+0x88;
 Monstey_U64 rdx=originalRdx;unsigned reads=0,visited=0;
 m.Write(named?f+0x38:f+0x58,originalRdx,8);
 const auto typed=[&](Monstey_U64 destination,Monstey_U64 count){
  const auto vt=m.Read(stream,8);const auto target=m.Read(vt+8,8);
  rdx=read(m,{stream,destination,count,target,reads++}).rdx;
 };
 if(named)typed(f-0x0C,4);
 typed(payloadCell,2);typed(descriptorCountCell,1);typed(firstCell,4);typed(flagsCell,1);
 Monstey_U32 count=0;
 if(named){rdx=m.Read(cursorCell,8);count=static_cast<Monstey_U32>(m.Read(payloadCell,2));}
 else{count=static_cast<Monstey_U32>(m.Read(payloadCell,2));rdx=m.Read(cursorCell,8);}
 Monstey_U64 name=0,pool=0;
 if(named){name=m.Read(f-0x0C,4);pool=m.Read(poolCell,8);name+=pool;}
 const auto values=m.Read(rdx,8);m.Write(rdx,values+Monstey_U64(count)*8,8);
 unsigned i=0;
 if(count){for(;;){
  rdx=values+Monstey_U64(i)*8;m.Write(rdx,0,8);
  const auto vt=m.Read(stream,8);const auto target=m.Read(vt+8,8);
  const auto result=read(m,{stream,rdx,4,target,reads++});rdx=result.rdx;
  if(static_cast<Monstey_U32>(result.rax)!=0)return {rdx,true,reads,visited};
  const auto live=static_cast<Monstey_U32>(m.Read(payloadCell,2));++i;if(i>=live)break;
  if(i>65536)throw std::runtime_error("reference fixture budget");
 }}
 const auto first=m.Read(firstCell,4);
 auto descriptorCount=static_cast<Monstey_U8>(m.Read(descriptorCountCell,1));
 const auto descriptorBase=m.Read(descriptorsCell,8)+(first<<4);
 if(descriptorCount){
  if(!named)pool=m.Read(poolCell,8);
  unsigned di=0;
  for(;;){
   rdx=descriptorBase+Monstey_U64(di)*16;++visited;
   const auto type=m.Read(rdx+0x0A,1)&0x3Fu;
   if((type>=4&&type<=9)||(type>=12&&type<=16)){
    const auto start=m.Read(rdx+0x0E,2);
    const auto slot=values+start*8;
    if(m.Read(rdx+0x0C,2)!=0){
     unsigned vi=0;
     for(;;){const auto a=slot+Monstey_U64(vi)*8;const auto v=m.Read(a,8);m.Write(a,v+pool,8);const auto live=m.Read(rdx+0x0C,2);++vi;if(vi>=live)break;if(vi>65536)throw std::runtime_error("reference relocation budget");}
     descriptorCount=static_cast<Monstey_U8>(m.Read(descriptorCountCell,1));
    }
   }
   ++di;if(di>=descriptorCount)break;if(di>256)throw std::runtime_error("reference descriptor budget");
  }
 }
 if(named){
  m.Write(record+8,descriptorBase,8);m.Write(record+0x20,name,8);m.Write(record,descriptorCount,1);m.Write(record+0x18,parent,8);m.Write(record+0x10,values,8);
  const auto hash=Monstey_Hash(m,name,0,rdx);rdx=hash.rdx;
  const auto flags=m.Read(flagsCell,1)|2;m.Write(record+4,hash.rax,4);m.Write(record+1,flags,1);
 }else{
  const auto flags=m.Read(flagsCell,1);m.Write(record+1,flags,1);m.Write(record,descriptorCount,1);m.Write(record+8,descriptorBase,8);m.Write(record+0x18,parent,8);m.Write(record+0x10,values,8);
 }
 return {rdx,false,reads,visited};
}
Monstey_U64 Monstey_Lookup(Monstey_Memory& m,Monstey_U64 object,Monstey_U64 key){
 auto end=static_cast<Monstey_U32>(m.Read(object+0x48,4))-1u;
 const auto array=m.Read(object+0x50,8);Monstey_U32 begin=0;
 if(std::bit_cast<std::int32_t>(end)<0)return 0;
 for(unsigned budget=0;budget<128;++budget){
  const auto sum=begin+end;const auto mid=std::bit_cast<std::int32_t>(sum)/2;
  const auto offset=static_cast<Monstey_U64>(static_cast<std::int64_t>(mid))*0x58u;
  const auto value=static_cast<Monstey_U32>(m.Read(array+offset+0x50,4));
  const auto requested=static_cast<Monstey_U32>(m.Read(key,4));
  if(requested==value)return array+offset;
  if(requested<value)end=std::bit_cast<Monstey_U32>(mid)-1u;else begin=std::bit_cast<Monstey_U32>(mid)+1u;
  if(std::bit_cast<std::int32_t>(begin)>std::bit_cast<std::int32_t>(end))return 0;
 }
 throw std::runtime_error("reference lookup fixture budget");
}
Monstey_Machine Monstey_Category(Monstey_Memory& m,Monstey_U64 originalRcx,Monstey_U64 bank,Monstey_U64 category,Monstey_U64 lookup,Monstey_U64 F,Monstey_CategoryDependencies& d){
 m.Write(F+0x30,originalRcx,8);
 const auto stream=m.Read(F+0x50,8);auto vt=m.Read(stream,8);auto target=m.Read(vt+8,8);
 auto result=d.read(m,{stream,F-0x18,24,target,0});
 const auto fail=[](Monstey_Machine v){v.rax&=~Monstey_U64(0xFF);return v;};
 if(static_cast<Monstey_U32>(result.rax))return fail(result);
 const auto cursor=m.Read(F+0x78,8);auto anon=static_cast<Monstey_U32>(m.Read(F-0x0C,4));
 const auto pool=m.Read(F+0x68,8);const auto thirdOffset=m.Read(F-0x10,4);const auto oldAnon=m.Read(cursor,8);
 const auto third=thirdOffset+pool;const auto secondOffset=m.Read(F-0x14,4);const auto nameOffset=m.Read(F-0x18,4);
 const auto second=secondOffset+pool;const auto name=nameOffset+pool;
 const auto anonDelta=anon<<5u;m.Write(F-0x20,third,8);m.Write(cursor,oldAnon+anonDelta,8);
 const auto selector=m.Read(F+0x58,8);m.Write(F-0x30,oldAnon,8);m.Write(F-0x28,second,8);
 vt=m.Read(selector,8);target=m.Read(vt,8);result=d.selector(m,{selector,bank,second,target});
 anon=static_cast<Monstey_U32>(m.Read(F-0x0C,4));
 if(anon){Monstey_U32 i=0;for(;;){
  auto p=m.Read(F+0x80,8);m.Write(F-0x38,p,8);p=m.Read(F+0x70,8);m.Write(F-0x48,p,8);
  const auto record=m.Read(F-0x30,8)+(Monstey_U64(i)<<5u);m.Write(F-0x50,pool,8);
  result=d.child(m,{false,category,result.rdx,record,stream,F-0xC0,i});
  if((result.rax&0xFFu)==0)return fail(result);
  anon=static_cast<Monstey_U32>(m.Read(F-0x0C,4));++i;if(i>=anon)break;if(i>1000)throw std::runtime_error("reference anon fixture budget");
 }}
 Monstey_U32 extra=0;
 if(lookup){result=Monstey_ConcreteHashBridge(m,name,0,result.rdx);m.Write(F+0x50,result.rax,4);const auto found=Monstey_ConcreteLookupBridge(m,lookup,F+0x50);if(found)extra=static_cast<Monstey_U32>(m.Read(found+0x28,4));}
 auto named=static_cast<Monstey_U32>(m.Read(F-8,4));const auto namedCursor=m.Read(F+0x78,8);
 const auto total=named+extra;const auto oldNamed=m.Read(namedCursor,8);m.Write(namedCursor,oldNamed+static_cast<Monstey_U32>((total*5u)<<3u),8);
 if(named){Monstey_U32 i=0;for(;;){
  auto p=m.Read(F+0x80,8);m.Write(F-0x38,p,8);const auto record=oldNamed+Monstey_U64(i)*40u;
  p=m.Read(F+0x70,8);m.Write(F-0x48,p,8);m.Write(F-0x50,pool,8);
  result=d.child(m,{true,category,named,record,stream,F-0xA0,i});if((result.rax&0xFFu)==0)return fail(result);
  named=static_cast<Monstey_U32>(m.Read(F-8,4));++i;if(i>=named)break;if(i>1000)throw std::runtime_error("reference named fixture budget");
 }anon=static_cast<Monstey_U32>(m.Read(F-0x0C,4));}
 auto a=m.Read(F-0x28,8);const auto parent=m.Read(F+0x30,8);m.Write(category+8,a,8);a=m.Read(F-0x20,8);m.Write(category+0x10,a,8);a=m.Read(F-0x30,8);
 m.Write(category+0x28,named,4);const auto scalar=m.Read(F+0x60,4);m.Write(category+0x18,parent,8);m.Write(category+0x48,a,8);m.Write(category,name,8);m.Write(category+0x38,oldNamed,8);m.Write(category+0x40,anon,4);m.Write(category+0x2C,scalar,4);
 result=Monstey_ConcreteHashBridge(m,name,0,scalar);const auto old=m.Read(category+0x20,8);m.Write(category+0x20,old&0,8);m.Write(category+0x50,result.rax,4);result.rax=(result.rax&~Monstey_U64(0xFF))|1u;m.Write(category+0x30,0,1);return result;
}
}
