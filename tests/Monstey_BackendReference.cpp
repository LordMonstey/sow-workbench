#include "Monstey_BackendReference.hpp"
#include <bit>
#include <limits>
namespace Monstey_BackendReview {
Monstey_ImportFault::Monstey_ImportFault(Monstey_Kind k,Monstey_Site q):std::runtime_error("independent backend import fault"),kind(k),site(q){}
Monstey_ExchangeFault::Monstey_ExchangeFault(Monstey_Exchange q):std::runtime_error("independent backend exchange fault"),pc(q.pc),address(q.address),value(q.value){}
namespace {
struct Monstey_Code {Monstey_U64 root,factory,init,place;};
constexpr std::array<Monstey_Code,3> Monstey_Programs{{{0x141175F88,0x141176058,0x1411755F0,0x141175718},{0x141174DBC,0x141174EA8,0x141173A38,0x141173BA0},{0x141177ECC,0x141177FB8,0x141176B48,0x141176CB0}}};
Monstey_U32 Monstey_Read32(Monstey_Image m,Monstey_U64 a,Monstey_U64 pc){return static_cast<Monstey_U32>(m.Read(a,4,pc));}
void Monstey_Or(Monstey_Image m,Monstey_U64 a,Monstey_U64 v,unsigned width,Monstey_U64 pc){const auto old=m.Read(a,width,pc);m.Write(a,old|v,width,pc);}
void Monstey_Zero(Monstey_Image m,Monstey_U64 a,unsigned width,Monstey_U64 pc){const auto old=m.Read(a,width,pc);m.Write(a,old&Monstey_U64{0},width,pc);}
Monstey_Site Monstey_Direct(Monstey_Image m,Monstey_Kind k,Monstey_U64 pc,Monstey_U64 body,const Monstey_Bindings&b){const auto cell=b.imports[static_cast<std::size_t>(k)];return {pc,pc,body,cell,m.Read(cell,8,pc)};}
Monstey_U64 Monstey_FillCall(Monstey_Image m,Monstey_U64 dest,Monstey_U64 count,Monstey_U64 caller_pc,Monstey_U64 body,const Monstey_Services&s,const Monstey_Bindings&b){const auto cell=b.imports[0];const Monstey_Site q{0x1408C4EE0,caller_pc,body-8,cell,m.Read(cell,8,0x1408C4EE0)};if(!s.fill)throw Monstey_ImportFault(Monstey_Kind::fill,q);return s.fill(m,{q,dest,count,0});}
Monstey_U64 Monstey_CsCall(Monstey_Image m,Monstey_U64 lock,Monstey_U64 pc,Monstey_U64 body,const Monstey_Services&s,const Monstey_Bindings&b){const auto q=Monstey_Direct(m,Monstey_Kind::initialize,pc,body,b);if(!s.initialize)throw Monstey_ImportFault(Monstey_Kind::initialize,q);return s.initialize(m,{q,lock,0x80000FA0u});}
Monstey_Raw Monstey_Init(unsigned n,Monstey_Image m,Monstey_U64 i,const Monstey_Services&s,Monstey_Bindings b);
Monstey_Tail Monstey_Place(unsigned n,Monstey_Image m,Monstey_U64 a,Monstey_U64 span,Monstey_U64 h,const Monstey_Services&s,Monstey_Bindings b);
Monstey_Raw Monstey_Allocate(unsigned n,Monstey_Image m,Monstey_U64 descriptor,Monstey_U64 f,Monstey_U64 incoming_d,const Monstey_Services&s,Monstey_Bindings b){
 const auto p=Monstey_Programs[n].factory;const auto g=b.modes[n];auto d=incoming_d;
 if(m.Read(g.cookie,8,p+0x14)==0)d=Monstey_Init(n,m,f-0x60,s,b).d;
 if(m.Read(g.page,8,p+0x29)==Monstey_U64{0}-0x420)return {0,d};
 const auto gran=m.Read(g.granularity,8,p+0x32);const auto size=(gran*2-1)&~(gran-1);
 Monstey_U32 selected=descriptor==0?0:Monstey_Read32(m,descriptor+0x370,p+0x4F);selected&=0x300000u;if(selected==0)selected=0x300000u;
 const auto q=Monstey_Direct(m,Monstey_Kind::allocate,p+0x80,f,b);if(!s.allocate)throw Monstey_ImportFault(Monstey_Kind::allocate,q);
 const auto allocation=s.allocate(m,{q,0,size,0x3000u,(selected&0x200000u)!=0?4u:2u});
 if(allocation.a-1>std::numeric_limits<Monstey_U64>::max()-2)return {0,allocation.d};
 const auto out=Monstey_Place(n,m,allocation.a,size,f-0x30,s,b);
 Monstey_Or(m,out.raw.a+0x370,2,4,p+0x9D);m.Write(out.raw.a+0x3B8,1,4,p+0xA4);return out.raw;
}
Monstey_Raw Monstey_Create(unsigned n,Monstey_Image m,Monstey_U64 flags_register,Monstey_U64 backing,Monstey_U64 count,Monstey_U64 body,const Monstey_Services&s,Monstey_Bindings b){
 const auto p=Monstey_Programs[n].root;const auto flags=static_cast<Monstey_U32>(flags_register);const auto g=b.modes[n];
 if(backing==0){const auto c=body+0x20;const auto d=Monstey_FillCall(m,c,0x3D0,p+0x33,body,s,b);Monstey_Or(m,c+0x370,flags,4,p+0x38);auto out=Monstey_Allocate(n,m,c,body-0x30,d,s,b);if(out.a!=0)Monstey_Or(m,out.a+0x370,flags,4,p+0x4E);return out;}
 auto d=flags_register;if(m.Read(g.cookie,8,p+0x5C)==0)d=Monstey_Init(n,m,body-0x60,s,b).d;
 if(count<=0x420)return {0,d};const auto limit=(Monstey_U64{0}-0x420)-m.Read(g.page,8,p+0x7A);if(count>=limit)return {0,d};
 const auto out=Monstey_Place(n,m,backing,count,body-0x30,s,b);Monstey_Or(m,out.raw.a+0x370,2,4,p+0x91);Monstey_Or(m,out.raw.a+0x370,flags,4,p+0x98);m.Write(out.raw.a+0x3B8,8,4,p+0x9E);return out.raw;
}
Monstey_Raw Monstey_Init(unsigned n,Monstey_Image m,Monstey_U64 i,const Monstey_Services&s,Monstey_Bindings b){
 const auto p=Monstey_Programs[n].init;const auto g=b.modes[n];
 if(std::bit_cast<std::int32_t>(Monstey_Read32(m,g.state,p+4))<=0){
  for(;;){const auto current=Monstey_Read32(m,g.state,p+0xE);m.Write(i+0x60,current,4,p+0x14);if(std::bit_cast<std::int32_t>(current)>0)break;
   if(current==0){const auto acquired=Monstey_PlacementReview::Monstey_RefState(m,g.state,i+0x60,i-0x40,s.lower,b.lower);if(static_cast<std::uint8_t>(acquired.raw.a)!=0){static_cast<void>(Monstey_CsCall(m,g.main_cs,p+0x4B,i,s,b));const Monstey_Exchange q{p+0x56,g.state,1};if(!s.exchange)throw Monstey_ExchangeFault(q);static_cast<void>(s.exchange(m,q));break;}}
   const auto q=Monstey_Direct(m,Monstey_Kind::sleep,p+0x37,i,b);if(!s.sleep)throw Monstey_ImportFault(Monstey_Kind::sleep,q);s.sleep(m,{q,0,0});
  }
 }
 {const auto q=Monstey_Direct(m,Monstey_Kind::enter,p+0x63,i,b);if(!s.enter)throw Monstey_ImportFault(Monstey_Kind::enter,q);s.enter(m,{q,g.main_cs});}
 if(m.Read(g.cookie,8,p+0x69)==0){
  {const auto q=Monstey_Direct(m,Monstey_Kind::info,p+0x7C,i,b);if(!s.info)throw Monstey_ImportFault(Monstey_Kind::info,q);s.info(m,{q,i+0x20});}
  const Monstey_U64 gran=Monstey_Read32(m,i+0x48,p+0x82);const Monstey_U64 page=Monstey_Read32(m,i+0x24,p+0x87);
  if((gran&(gran-1))!=0||(page&(page-1))!=0)Monstey_Zero(m,0,4,p+0xA4);
  const auto tail_cell=n==0?b.lower.g40:(n==1?b.lower.g80:b.lower.gc0);
  if(n!=2){m.Write(g.granularity,gran,8,p+0xB0);m.Write(g.auxiliary,std::numeric_limits<Monstey_U64>::max(),8,p+0xB7);m.Write(tail_cell,std::numeric_limits<Monstey_U64>::max(),8,p+0xC5);m.Write(g.page,page,8,p+0xD6);m.Write(g.flags,7,4,p+0xDD);m.Write(g.secondary,7,4,p+0xE3);}
  else{Monstey_Or(m,tail_cell,std::numeric_limits<Monstey_U64>::max(),8,p+0xAC);m.Write(g.granularity,gran,8,p+0xC0);m.Write(g.flags,7,4,p+0xCC);m.Write(g.secondary,7,4,p+0xD2);m.Write(g.page,page,8,p+0xD8);m.Write(g.auxiliary,0x400000,8,p+0xDF);}
  static_cast<void>(Monstey_CsCall(m,g.template_cs,p+(n==2?0xEAu:0xE9u),i,s,b));
  const auto q=Monstey_Direct(m,Monstey_Kind::random,p+(n==2?0xF0u:0xEFu),i,b);if(!s.random)throw Monstey_ImportFault(Monstey_Kind::random,q);const auto bits=s.random(m,q);
  const auto signed_bits=static_cast<Monstey_U64>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(bits)));const auto cookie=((signed_bits&~Monstey_U64{7})^0x55555550u)|8u;m.Write(g.cookie,cookie,8,p+(n==2?0x108u:0x107u));
 }
 const auto q=Monstey_Direct(m,Monstey_Kind::leave,p+(n==2?0x116u:0x115u),i,b);if(!s.leave)throw Monstey_ImportFault(Monstey_Kind::leave,q);const auto d=s.leave(m,{q,g.main_cs});return {1,d};
}
Monstey_Tail Monstey_Place(unsigned n,Monstey_Image m,Monstey_U64 a,Monstey_U64 span,Monstey_U64 h,const Monstey_Services&s,Monstey_Bindings b){
 const auto p=Monstey_Programs[n].place;const auto g=b.modes[n];const auto pad=(Monstey_U64{0}-(a&7))&7;const auto o=a+0x10+pad;
 static_cast<void>(Monstey_FillCall(m,o,0x3D8,p+0x3E,h,s,b));const auto reset_d=Monstey_CsCall(m,o+0x378,p+0x4F,h,s,b);
 m.Write(o-8,0x3DB,8,p+0x55);m.Write(o+0x18,a,8,p+0x61);m.Write(o+0x3A0,a,8,p+0x65);m.Write(o+0x360,span,8,p+0x6C);m.Write(o+0x358,span,8,p+0x73);m.Write(o+0x3A8,span,8,p+0x7A);
 const auto cookie=m.Read(g.cookie,8,p+0x81);Monstey_Or(m,o+0x38,std::numeric_limits<Monstey_U64>::max(),8,p+0x88);m.Write(o+0x40,cookie,8,p+0x8D);
 const auto flags=Monstey_Read32(m,g.flags,p+0x91);Monstey_Zero(m,o+0x3C0,8,p+0x97);Monstey_Zero(m,o+0x3C8,8,p+0x9F);m.Write(o+0x370,flags,4,p+0xA7);Monstey_Or(m,o+0x370,4,4,p+0xAD);
 if(n==0)static_cast<void>(Monstey_PlacementReview::Monstey_RefReset40(m,o,reset_d));else if(n==1)static_cast<void>(Monstey_PlacementReview::Monstey_RefReset80(m,o,reset_d));else static_cast<void>(Monstey_PlacementReview::Monstey_RefResetC0(m,o,reset_d));
 const auto header=m.Read(o-8,8,p+0xB9);const auto x=(header&~Monstey_U64{7})-0x10+o;const auto available=span-0x48+(a-x);
 Monstey_Tail out{};if(n==0)out=Monstey_PlacementReview::Monstey_RefTail40(m,o,x,available,b.lower);else if(n==1)out=Monstey_PlacementReview::Monstey_RefTail80(m,o,x,available,b.lower);else out=Monstey_PlacementReview::Monstey_RefTailC0(m,o,x,available,b.lower);out.raw.a=o;return out;
}
}
#define MONSTEY_BACKEND_REFERENCE_BODY(T,N) \
Monstey_Raw Monstey_RefCreate##T(Monstey_Image m,Monstey_U64 f,Monstey_U64 a,Monstey_U64 c,Monstey_U64 r,const Monstey_Services&s,Monstey_Bindings b){return Monstey_Create(N,m,f,a,c,r,s,b);} \
Monstey_Raw Monstey_RefAllocate##T(Monstey_Image m,Monstey_U64 d,Monstey_U64 r,Monstey_U64 in,const Monstey_Services&s,Monstey_Bindings b){return Monstey_Allocate(N,m,d,r,in,s,b);} \
Monstey_Raw Monstey_RefInit##T(Monstey_Image m,Monstey_U64 r,const Monstey_Services&s,Monstey_Bindings b){return Monstey_Init(N,m,r,s,b);} \
Monstey_Tail Monstey_RefPlace##T(Monstey_Image m,Monstey_U64 a,Monstey_U64 c,Monstey_U64 r,const Monstey_Services&s,Monstey_Bindings b){return Monstey_Place(N,m,a,c,r,s,b);}
MONSTEY_BACKEND_REFERENCE_BODY(40,0)
MONSTEY_BACKEND_REFERENCE_BODY(80,1)
MONSTEY_BACKEND_REFERENCE_BODY(C0,2)
#undef MONSTEY_BACKEND_REFERENCE_BODY
}
