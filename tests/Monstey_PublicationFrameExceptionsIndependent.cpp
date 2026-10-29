#include "Monstey_PublicationFramedReference.hpp"
#include "Monstey_GadbPublicationFramed.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
namespace R=Monstey_PublicationFramedReview;namespace P=Monstey;
namespace {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
enum class Monstey_Function{publication,copy,assign,pair,primary,secondary};
enum class Monstey_Scenario{plain,swap_vptr,late_homes,cached_owner,assign_mutation,clear_virtual,replace_virtual,retire_virtual,retire_atomic,reentrant,trap_return,trap_throw};
struct Monstey_Config{Monstey_Function function=Monstey_Function::publication;Monstey_Scenario scenario=Monstey_Scenario::plain;unsigned shape=0;Monstey_U32 primary=2,secondary=2;Monstey_U64 incoming_a=0xFEDCBA9876543210ull,incoming_d=0x0123456789ABCDEFull;bool missing_atomic=false,missing_virtual=false;int fail_call=-1;bool after_effect=false;std::size_t visible=0x10000;Monstey_U64 body=0x10005000,nested_body=0x28;unsigned frame_effect=0;bool force_copy_old=false,nested_virtual=false,ignore_frame=false;};
struct Monstey_ServiceEvent{unsigned kind,phase,generation,local;Monstey_U64 pc,address,a,b,c;Monstey_U32 width;R::Monstey_Raw raw;Monstey_U64 frame=0;bool operator==(const Monstey_ServiceEvent&)const=default;};
struct Monstey_Injected:std::runtime_error{unsigned ordinal,kind,phase;Monstey_Injected(unsigned n,unsigned k,unsigned p):std::runtime_error("owned primitive injected"),ordinal(n),kind(k),phase(p){}};
struct Monstey_Outcome{unsigned kind=0;R::Monstey_Raw raw{};Monstey_U64 pc=0,call=0,address=0,slot=0,target=0,message=0;std::size_t width=0;bool writing=false;std::string unexpected;bool operator==(const Monstey_Outcome&)const=default;};
class Monstey_Host {
public:
 const bool product,generic;Monstey_Config config;
 Monstey_U64 base,c,c2,c3,vt,vt2,destination,source,other_pair,owner,owner2,q,db,control_arg,new_control;
 Monstey_U64 new_data=0xC0FFEE1234567890ull,hbank=0x70000000;
 Monstey_U32 h8=0xF00D0001u,h9=0xFACE0002u;
 std::vector<std::uint8_t> bytes;
 std::vector<Monstey_ServiceEvent> events;
 unsigned calls=0,virtual_calls=0;std::vector<unsigned> mutable_atomic;
 // Service holders are destroyed before bytes/events, so host retirement effects
 // have live backing; only the active side is populated.
 R::Monstey_Services reference_services;P::Monstey_GadbPublicationFramedServices product_services;P::Monstey_GadbPublicationServices generic_services;
 static constexpr Monstey_U64 data=0x1111222233334444ull,old_data=0x5555666677778888ull,optional=0x9999AAAABBBBCCCCull;
 struct Monstey_Token {Monstey_Host* host;Monstey_U64 at,value;unsigned width;Monstey_Token(Monstey_Host* h,Monstey_U64 a,Monstey_U64 v,unsigned n):host(h),at(a),value(v),width(n){}~Monstey_Token(){host->Put(at,value,width);host->events.push_back({3,2,0,0,0xDE57,at,value,0,0,width,{0,0}});}};
 explicit Monstey_Host(unsigned actual,Monstey_Config cfg):product(actual!=0),generic(actual==2),config(cfg),base(cfg.shape==19?0:0x10000000),c(base+0x1000),c2(base+0x1100),c3(base+0x1200),vt(base+0x2000),vt2(base+0x2100),destination(base+0x3000),source(base+0x3100),other_pair(base+0x3200),owner(base+0x4000),owner2(base+0x4100),q(base+0x5000),db(base+0x7000),control_arg(c),new_control(c3),bytes(0x10000,0xA5){
  Control(c,cfg.primary,cfg.secondary);Control(c2,1,1);Control(c3,4,2);Put(vt,0x11110001,8);Put(vt+8,0x11110002,8);Put(vt2,0x22220001,8);Put(vt2+8,0x22220002,8);
  Pair(destination,old_data,0);Pair(source,data,c);Pair(other_pair,0xC0FFEE,c3);Put(owner,optional,8);Put(owner2,0xDEADBEEF,8);Pair(db+0x58,old_data,c2);Put(db+0x20,0,8);
  Put(q+0x60,7,4);Put(q+0x68,9,4);Put(q+0x70,0x30000000,8);Put(q+0x78,source,8);Put(q+0x80,0x40000000,8);Put(q+0x88,0x50000000,8);Put(q+0x90,0xFFFFFFFF,4);Put(q+0x98,0x80000000,4);Put(q+0xA0,0x60000000,8);Put(q+0xA8,owner,8);
  switch(cfg.shape){
   case 1:destination=source;break;
   case 2:destination=source+8;break;
   case 3:source=destination+8;Pair(source,data,c);break;
   case 4:destination=c;break;
   case 5:destination=c+8;break;
   case 6:source=base-8;Put(base,0,8);break;
   case 7:Put(q+0x78,q+0x20,8);Pair(q+0x20,data,c);Pair(db+0x58,old_data,0);break;
   case 8:db=q+0x30;Put(db+0x20,0,8);Put(q+0x60,other_pair,4);Put(q+0x68,0,4);Pair(other_pair,0x12345678,0);Put(q+0x90,0,8);break;
   case 9:{db=q+0x50;const auto oldPair=source,newPair=other_pair;Pair(oldPair,0xBAD,c3);Pair(newPair,owner,c);Put(owner,0,8);Put(q+0x60,7,4);Put(q+0x68,0,4);Put(q+0x70,0,8);Put(q+0x78,oldPair,8);Put(q+0x80,0x1122334455667788ull,8);Put(q+0x90,0,8);Put(q+0x98,0,8);Put(q+0xA0,0,8);Put(q+0xA8,owner,8);Put(q+0xB0,0,8);h8=static_cast<Monstey_U32>(newPair);h9=1;break;}
   case 10:Put(q+0xA8,db+0x20,8);Put(db+0x20,0,8);Pair(source,data,0);Pair(db+0x58,0,0);Put(q+0x68,0x89ABCDEFu,4);break;
   case 11:Put(q+0xA8,db+0x20,8);Put(db+0x20,0xCAFE,8);break;
   case 12:Put(db+0x20,0xCAFE,8);break;
   case 13:Pair(source,data,0);Pair(db+0x58,0,0);break;
   case 14:Put(destination+8,1,8);break;
   case 15:Put(source+8,1,8);break;
   case 16:source=base+bytes.size()-4;Put(q+0x78,source,8);break;
   case 17:Put(c,base+bytes.size()-4,8);break;
   case 18:control_arg=c+1;Control(control_arg,cfg.primary,cfg.secondary);Put(source+8,control_arg,8);break;
   case 19:control_arg=0;Control(0,cfg.primary,cfg.secondary);break;
   case 20:Pair(source,data,0);break;
   case 21:Put(q+0xA8,0,8);break;
   case 22:Put(db+0x60,1,8);break;
   case 23:Put(vt,0,8);Put(vt+8,0,8);break;
   case 24:Control(c3,1,1);break;
   case 25:db=q-0x118;Pair(db+0x58,old_data,c2);Put(db+0x20,0,8);Control(c3,1,1);break;
   default:break;
  }
  if(cfg.function==Monstey_Function::assign&&cfg.shape!=14)Pair(destination,old_data,c2);
  if(cfg.scenario==Monstey_Scenario::cached_owner){Control(c,0,2);Pair(db+0x58,0,0);}
  if(cfg.scenario==Monstey_Scenario::assign_mutation){Pair(destination,old_data,c2);Control(c2,1,2);}
  if(cfg.scenario==Monstey_Scenario::reentrant){Control(control_arg,1,2);Control(c3,cfg.nested_virtual?1u:2u,cfg.nested_virtual?1u:2u);}
  if(cfg.scenario==Monstey_Scenario::clear_virtual||cfg.scenario==Monstey_Scenario::replace_virtual||cfg.scenario==Monstey_Scenario::retire_virtual)Control(control_arg,1,1);
  InstallAtomic(0);InstallVirtual(0);
  if(cfg.scenario==Monstey_Scenario::retire_virtual){auto token=std::make_shared<Monstey_Token>(this,control_arg+12,7,4);InstallVirtual(0,token);token.reset();}
  if(cfg.scenario==Monstey_Scenario::retire_atomic){Pair(db+0x58,0,0);auto token=std::make_shared<Monstey_Token>(this,q+0x88,0xABCDEF0011223344ull,8);InstallAtomic(0,token);token.reset();}
  if(cfg.missing_atomic)ClearAtomic();if(cfg.missing_virtual)ClearVirtual();
  if(cfg.scenario==Monstey_Scenario::trap_return||cfg.scenario==Monstey_Scenario::trap_throw)InstallTrap();
 }
 R::Monstey_View View(){return {base,{bytes.data(),std::min(config.visible,bytes.size())},nullptr};}
 P::Monstey_GadbBinderMemory ProductView(){auto v=View();return {v.base,v.bytes};}
 R::Monstey_View Convert(P::Monstey_GadbBinderMemory m){return {m.base,m.bytes,nullptr};}
 R::Monstey_View Convert(R::Monstey_View m){return m;}
 void CheckView(R::Monstey_View v){if(v.base!=base||v.bytes.data()!=bytes.data()||v.bytes.size()!=std::min(config.visible,bytes.size()))throw std::runtime_error("changed provider view");}
 void Put(Monstey_U64 at,Monstey_U64 value,unsigned n){R::Monstey_View{base,bytes,nullptr}.Store(at,value,n,0);}
 Monstey_U64 Get(Monstey_U64 at,unsigned n){return R::Monstey_View{base,bytes,nullptr}.Load(at,n,0);}
 void Pair(Monstey_U64 p,Monstey_U64 d,Monstey_U64 ctrl){Put(p,d,8);Put(p+8,ctrl,8);}
 void Control(Monstey_U64 ctrl,Monstey_U32 p,Monstey_U32 w){Put(ctrl,vt,8);Put(ctrl+8,p,4);Put(ctrl+12,w,4);}
 void ClearAtomic(){if(generic)generic_services.atomic32={};else if(product)product_services.atomic32={};else reference_services.atomic={};}
 void ClearVirtual(){if(generic)generic_services.virtual_call={};else if(product)product_services.virtual_call={};else reference_services.virtual_call={};}
 R::Monstey_AtomicCall Normalize(const P::Monstey_GadbPublicationAtomicCall& x){return {x.native_site,x.address,x.addend,x.operation==P::Monstey_GadbPublicationAtomicOperation::increment32?R::Monstey_AtomicOp::increment:R::Monstey_AtomicOp::fetch_add};}
 R::Monstey_VirtualCall Normalize(const P::Monstey_GadbPublicationVirtualCall& x){return {x.native_site,x.this_identity,x.vtable_identity,x.slot_identity,x.target_identity,{x.incoming.rax,x.incoming.rdx},0};}
 R::Monstey_VirtualCall Normalize(const P::Monstey_GadbPublicationFramedVirtualCall& x){auto n=Normalize(x.call);n.caller_body_rsp=x.caller_body_rsp;return n;}
 R::Monstey_TrapCall Normalize(const P::Monstey_GadbPublicationTrapCall& x){return {x.call_site,x.trap_entry,x.zero_store_site,x.message_identity,x.address,x.value,{x.incoming.rax,x.incoming.rdx}};}
 unsigned Begin(Monstey_ServiceEvent event){const auto ordinal=calls++;events.push_back(event);if(config.fail_call==static_cast<int>(ordinal)&&!config.after_effect)throw Monstey_Injected(ordinal,event.kind,0);return ordinal;}
 void Commit(Monstey_ServiceEvent event,unsigned ordinal){event.phase=1;events.push_back(event);if(config.fail_call==static_cast<int>(ordinal)&&config.after_effect)throw Monstey_Injected(ordinal,event.kind,1);}
 Monstey_U32 Atomic(R::Monstey_View v,const R::Monstey_AtomicCall& x,unsigned generation,unsigned local){CheckView(v);Monstey_ServiceEvent event{0,0,generation,local,x.pc,x.address,x.delta,static_cast<Monstey_U64>(x.operation),0,4,{0,0}};const auto ordinal=Begin(event);const auto old=static_cast<Monstey_U32>(v.Load(x.address,4,x.pc));v.Store(x.address,Monstey_U32(old+x.delta),4,x.pc);mutable_atomic.push_back(local);if(config.force_copy_old&&x.operation==R::Monstey_AtomicOp::increment)Put(config.function==Monstey_Function::publication?q+0x28:destination+8,c3,8);if(config.scenario==Monstey_Scenario::retire_atomic&&generation==0)InstallAtomic(1);event.raw={old,0};Commit(event,ordinal);return old;}
 R::Monstey_Raw Virtual(R::Monstey_View v,const R::Monstey_VirtualCall& x,unsigned generation,unsigned local){CheckView(v);++virtual_calls;Monstey_ServiceEvent event{1,0,generation,local,x.pc,x.control,x.vtable,x.slot,x.target,8,x.incoming};event.frame=config.ignore_frame?0:x.caller_body_rsp;const auto ordinal=Begin(event);if(config.frame_effect){v.Store(x.caller_body_rsp-0x80,~Monstey_U64(1),8,0x141B0444);}
  const auto primary=x.pc==0x1403DD6FB;
  if(primary&&config.scenario==Monstey_Scenario::swap_vptr)v.Store(x.control,vt2,8,0);
  if(primary&&config.scenario==Monstey_Scenario::late_homes){v.Store(q+0x88,0xBABA,8,0);v.Store(q+0x90,0xFEEE,4,0);v.Store(q+0x98,0xABCD,4,0);v.Store(q+0xA0,0xC0DE,8,0);v.Store(q+0x68,0x87654321u,4,0);v.Store(q+0x78,other_pair,8,0);v.Store(q+0xA8,owner2,8,0);}
  if(primary&&config.scenario==Monstey_Scenario::cached_owner)v.Store(q+0xA8,owner2,8,0);
  if(primary&&config.scenario==Monstey_Scenario::assign_mutation){v.Store(destination,0xBAD,8,0);v.Store(destination+8,0xBAD,8,0);}
  if(primary&&config.scenario==Monstey_Scenario::clear_virtual)ClearVirtual();
  if(primary&&(config.scenario==Monstey_Scenario::replace_virtual||config.scenario==Monstey_Scenario::retire_virtual)&&generation==0)InstallVirtual(1);
  if(primary&&config.scenario==Monstey_Scenario::reentrant){const auto nested=Invoke(Monstey_Function::pair,other_pair,{7,6},config.nested_body);if(!config.nested_virtual&&nested!=R::Monstey_Raw{2,6})throw std::runtime_error("actual reentrant nested release result");}
  const R::Monstey_Raw result{0xA000000000000000ull+(Monstey_U64(generation)<<16)+local,0xD000000000000000ull+(Monstey_U64(generation)<<16)+local};event.raw=result;Commit(event,ordinal);return result;
 }
 void Trap(R::Monstey_View v,const R::Monstey_TrapCall& x,unsigned local){CheckView(v);Monstey_ServiceEvent event{2,0,0,local,x.store_site,x.address,x.call_site,x.entry,x.message,4,x.incoming};const auto ordinal=Begin(event);if(config.scenario==Monstey_Scenario::trap_throw)v.Store(base+0x4500,0xFACEB00C,4,0);Commit(event,ordinal);if(config.scenario==Monstey_Scenario::trap_throw)throw Monstey_Injected(ordinal,2,2);}
 void InstallAtomic(unsigned generation,std::shared_ptr<Monstey_Token> token={}){if(generic){generic_services.atomic32=[this,generation,token,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationAtomicCall& x)mutable{return Atomic(Convert(m),Normalize(x),generation,++n);};}else if(product){product_services.atomic32=[this,generation,token,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationAtomicCall& x)mutable{return Atomic(Convert(m),Normalize(x),generation,++n);};}else{reference_services.atomic=[this,generation,token,n=0u](R::Monstey_View m,const R::Monstey_AtomicCall& x)mutable{return Atomic(m,x,generation,++n);};}}
 void InstallVirtual(unsigned generation,std::shared_ptr<Monstey_Token> token={}){if(generic){generic_services.virtual_call=[this,generation,token,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationVirtualCall& x)mutable{const auto result=Virtual(Convert(m),Normalize(x),generation,++n);return P::Monstey_GadbBufferedResult{result.a,result.d};};}else if(product){product_services.virtual_call=[this,generation,token,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationFramedVirtualCall& x)mutable{const auto result=Virtual(Convert(m),Normalize(x),generation,++n);return P::Monstey_GadbBufferedResult{result.a,result.d};};}else{reference_services.virtual_call=[this,generation,token,n=0u](R::Monstey_View m,const R::Monstey_VirtualCall& x)mutable{return Virtual(m,x,generation,++n);};}}
 void InstallTrap(){if(generic){generic_services.trap=[this,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationTrapCall& x)mutable{Trap(Convert(m),Normalize(x),++n);};}else if(product){product_services.trap=[this,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbPublicationTrapCall& x)mutable{Trap(Convert(m),Normalize(x),++n);};}else{reference_services.trap=[this,n=0u](R::Monstey_View m,const R::Monstey_TrapCall& x)mutable{Trap(m,x,++n);};}}
 R::Monstey_Raw Invoke(Monstey_Function f,Monstey_U64 arg,R::Monstey_Raw raw,Monstey_U64 body){if(generic){const P::Monstey_GadbBufferedResult input{raw.a,raw.d};P::Monstey_GadbBufferedResult out{};switch(f){case Monstey_Function::publication:out=P::Monstey_GadbPublishDatabaseStorage(ProductView(),db,h8,h9,hbank,q,generic_services);break;case Monstey_Function::copy:out=P::Monstey_GadbCopyPublicationPair(ProductView(),destination,source,input,generic_services);break;case Monstey_Function::assign:out=P::Monstey_GadbAssignPublicationPair(ProductView(),destination,new_data,new_control,input,generic_services);break;case Monstey_Function::pair:out=P::Monstey_GadbReleasePublicationPair(ProductView(),arg,input,generic_services);break;case Monstey_Function::primary:out=P::Monstey_GadbReleasePublicationPrimary(ProductView(),arg,input,generic_services);break;case Monstey_Function::secondary:out=P::Monstey_GadbReleasePublicationSecondary(ProductView(),arg,input,generic_services);break;}return {out.rax,out.rdx};}if(product){const P::Monstey_GadbBufferedResult input{raw.a,raw.d};P::Monstey_GadbBufferedResult out{};switch(f){case Monstey_Function::publication:out=P::Monstey_GadbPublishDatabaseStorageFramed(ProductView(),db,h8,h9,hbank,q,product_services);break;case Monstey_Function::copy:out=P::Monstey_GadbCopyPublicationPairFramed(ProductView(),destination,source,body,input,product_services);break;case Monstey_Function::assign:out=P::Monstey_GadbAssignPublicationPairFramed(ProductView(),destination,new_data,new_control,body,input,product_services);break;case Monstey_Function::pair:out=P::Monstey_GadbReleasePublicationPairFramed(ProductView(),arg,body,input,product_services);break;case Monstey_Function::primary:out=P::Monstey_GadbReleasePublicationPrimaryFramed(ProductView(),arg,body,input,product_services);break;case Monstey_Function::secondary:out=P::Monstey_GadbReleasePublicationSecondaryFramed(ProductView(),arg,body,input,product_services);break;}return {out.rax,out.rdx};}
  switch(f){case Monstey_Function::publication:return R::Monstey_RefPublish(View(),db,h8,h9,hbank,q,reference_services);case Monstey_Function::copy:return R::Monstey_RefCopy(View(),destination,source,body,raw,reference_services);case Monstey_Function::assign:return R::Monstey_RefAssign(View(),destination,new_data,new_control,body,raw,reference_services);case Monstey_Function::pair:return R::Monstey_RefReleasePair(View(),arg,body,raw,reference_services);case Monstey_Function::primary:return R::Monstey_RefPrimary(View(),arg,body,raw,reference_services);case Monstey_Function::secondary:return R::Monstey_RefSecondary(View(),arg,body,raw,reference_services);}throw std::runtime_error("unknown owned function");}
 R::Monstey_Raw Run(){const auto arg=config.function==Monstey_Function::pair?source:control_arg;return Invoke(config.function,arg,{config.incoming_a,config.incoming_d},config.body);}
};
Monstey_Outcome Monstey_Observe(Monstey_Host& h){try{return {0,h.Run()};}catch(const R::Monstey_Fault& e){Monstey_Outcome x;if(e.kind==R::Monstey_FaultKind::range){x.kind=1;x.address=e.address;x.width=e.width;x.writing=e.writing;return x;}x.kind=e.kind==R::Monstey_FaultKind::missing_atomic?2:e.kind==R::Monstey_FaultKind::missing_virtual?3:e.kind==R::Monstey_FaultKind::invariant_trap?4:e.kind==R::Monstey_FaultKind::trap_returned?5:9;x.pc=e.pc;x.call=e.call_site;x.address=e.address;x.width=e.width;x.slot=e.slot;x.target=e.target;x.message=e.message;return x;}catch(const P::Monstey_GadbBufferedMemoryFault& e){Monstey_Outcome x;x.kind=1;x.address=e.address;x.width=e.width;x.writing=e.writing;return x;}catch(const P::Monstey_GadbPublicationBoundaryFault& e){Monstey_Outcome x;x.kind=e.kind==P::Monstey_GadbPublicationBoundaryKind::missing_atomic32?2:e.kind==P::Monstey_GadbPublicationBoundaryKind::missing_virtual_call?3:e.kind==P::Monstey_GadbPublicationBoundaryKind::invariant_trap?4:5;x.pc=e.native_site;x.call=e.call_site;x.address=e.address;x.width=e.width;x.slot=e.slot;x.target=e.target;x.message=e.message_identity;return x;}catch(const Monstey_Injected& e){Monstey_Outcome x;x.kind=6;x.pc=e.ordinal;x.address=e.kind;x.width=e.phase;return x;}catch(const std::exception& e){Monstey_Outcome x;x.kind=10;x.unexpected=e.what();return x;}}
std::uint64_t Monstey_cases=0,Monstey_checks=0,Monstey_range=0,Monstey_boundary=0,Monstey_provider=0,Monstey_events=0,Monstey_compared=0,Monstey_projection=0,Monstey_frameevents=0;
void Monstey_Check(bool good,const char* why){++Monstey_checks;if(!good)throw std::runtime_error(why);}
void Monstey_Compare(Monstey_Config c,bool projection=false){Monstey_Host expected(projection?2u:0u,c),actual(1u,c);const auto a=Monstey_Observe(expected),b=Monstey_Observe(actual);if(a!=b)std::cerr<<"fn="<<static_cast<unsigned>(c.function)<<" scenario="<<static_cast<unsigned>(c.scenario)<<" shape="<<c.shape<<" count="<<c.primary<<"/"<<c.secondary<<" fault="<<c.fail_call<<" expectedkind="<<a.kind<<" actualkind="<<b.kind<<" expectedraw="<<a.raw.a<<","<<a.raw.d<<" actualraw="<<b.raw.a<<","<<b.raw.d<<" expectedfault="<<a.pc<<","<<a.address<<","<<a.width<<" actualfault="<<b.pc<<","<<b.address<<","<<b.width<<"\n";
 Monstey_Check(a==b,"raw return or exact first actual fault differs");const auto mismatch=std::mismatch(expected.bytes.begin(),expected.bytes.end(),actual.bytes.begin());if(mismatch.first!=expected.bytes.end())std::cerr<<"first byte mismatch="<<expected.base+static_cast<Monstey_U64>(mismatch.first-expected.bytes.begin())<<" fn="<<static_cast<unsigned>(c.function)<<" scenario="<<static_cast<unsigned>(c.scenario)<<" shape="<<c.shape<<"\n";Monstey_Check(mismatch.first==expected.bytes.end(),"full owned image/prefix differs");Monstey_Check(expected.events==actual.events,"actual primitive arguments/order/raw values/target-lifetime events differ");Monstey_Check(expected.calls==actual.calls&&expected.virtual_calls==actual.virtual_calls&&expected.mutable_atomic==actual.mutable_atomic,"provider invocation/state persistence differs");
 if(c.shape==9){Monstey_Check(a.kind==1&&a.address==((Monstey_U64(1)<<32)|expected.other_pair)+8&&a.width==8,"Root DBQ50 exact late natural fault");Monstey_Check(actual.Get(actual.c+8,4)==3&&actual.Get(actual.c3+8,4)==4,"Root DBQ50 middle epoch used new low pointer only");Monstey_Check(actual.Get(actual.db+0x58,8)==actual.owner&&actual.Get(actual.db+0x60,8)==actual.c,"Root DBQ50 committed copy prefix");}
 if((c.shape==24||c.shape==25)&&c.fail_call<0){std::vector<Monstey_U64> observed;for(const auto& e:actual.events)if(e.kind==1&&e.phase==0)observed.push_back(e.frame);Monstey_Check(observed==std::vector<Monstey_U64>({actual.q-0x90,actual.q-0x90,actual.q-0x60,actual.q-0x60}),"literal publication CALL/tail frame sequence");}
 if(a.kind==1)++Monstey_range;else if(a.kind>=2&&a.kind<=5)++Monstey_boundary;else if(a.kind==6)++Monstey_provider;
 ++Monstey_cases;if(projection)++Monstey_projection;for(const auto& e:actual.events)if(e.kind==1&&e.phase==0)++Monstey_frameevents;Monstey_events+=actual.events.size();Monstey_compared+=actual.bytes.size();}
std::vector<Monstey_Config> Monstey_Cases(){std::vector<Monstey_Config> out;
 for(Monstey_U32 p:{0u,1u,2u,0xFFFFFFFFu})for(Monstey_U32 w:{0u,1u,2u,0xFFFFFFFFu})for(Monstey_U64 d:{0ull,0xABCDEF0123456789ull}){Monstey_Config c;c.function=Monstey_Function::primary;c.primary=p;c.secondary=w;c.incoming_d=d;out.push_back(c);}
 for(Monstey_U32 w:{0u,1u,2u,0xFFFFFFFFu}){Monstey_Config c;c.function=Monstey_Function::secondary;c.secondary=w;out.push_back(c);}
 for(Monstey_U64 a:{0ull,0xFFFFFFFF00001234ull,0x1020304050607080ull})for(Monstey_U64 d:{0ull,0xABCDEF1234567890ull}){Monstey_Config c;c.function=Monstey_Function::pair;c.shape=20;c.incoming_a=a;c.incoming_d=d;out.push_back(c);}
 for(unsigned shape:{0u,1u,2u,3u,4u,5u,15u,16u,20u})for(Monstey_U32 p:{0u,1u,2u,0xFFFFFFFFu}){Monstey_Config c;c.function=Monstey_Function::copy;c.shape=shape;c.primary=p;out.push_back(c);}
 for(unsigned shape:{0u,14u}){Monstey_Config c;c.function=Monstey_Function::assign;c.shape=shape;out.push_back(c);}for(Monstey_Scenario s:{Monstey_Scenario::assign_mutation}){Monstey_Config c;c.function=Monstey_Function::assign;c.scenario=s;out.push_back(c);}
 for(unsigned shape:{6u}){Monstey_Config c;c.function=Monstey_Function::pair;c.shape=shape;out.push_back(c);}
 for(unsigned shape:{17u,18u,19u,23u})for(Monstey_Function fn:{Monstey_Function::primary,Monstey_Function::secondary}){Monstey_Config c;c.function=fn;c.shape=shape;c.primary=shape==17||shape==23?1:2;c.secondary=shape==17||shape==23?1:2;out.push_back(c);}
 for(Monstey_Function fn:{Monstey_Function::copy,Monstey_Function::primary,Monstey_Function::secondary}){Monstey_Config c;c.function=fn;c.missing_atomic=true;out.push_back(c);}
 for(Monstey_Function fn:{Monstey_Function::primary,Monstey_Function::secondary})for(Monstey_U32 n:{1u,2u}){Monstey_Config c;c.function=fn;c.primary=n;c.secondary=n;c.missing_virtual=true;out.push_back(c);}
 for(Monstey_Scenario s:{Monstey_Scenario::swap_vptr,Monstey_Scenario::clear_virtual,Monstey_Scenario::replace_virtual,Monstey_Scenario::retire_virtual,Monstey_Scenario::reentrant}){Monstey_Config c;c.function=Monstey_Function::primary;c.primary=1;c.secondary=1;c.scenario=s;out.push_back(c);}
 for(unsigned shape:{0u,7u,8u,9u,10u,11u,12u,13u,16u,21u,22u}){Monstey_Config c;c.shape=shape;out.push_back(c);}
 for(Monstey_Scenario s:{Monstey_Scenario::late_homes,Monstey_Scenario::cached_owner,Monstey_Scenario::retire_atomic}){Monstey_Config c;c.scenario=s;out.push_back(c);}
 for(unsigned shape:{11u,12u})for(Monstey_Scenario s:{Monstey_Scenario::trap_return,Monstey_Scenario::trap_throw}){Monstey_Config c;c.shape=shape;c.scenario=s;out.push_back(c);}
 for(std::size_t n:{0x505Fu,0x5063u,0x704Bu,0x704Cu,0x7057u,0x705Fu,0x7067u,0x7068u}){Monstey_Config c;c.visible=n;out.push_back(c);}
 {Monstey_Config c;c.shape=10;c.missing_atomic=true;c.missing_virtual=true;out.push_back(c);}
 for(Monstey_Function fn:{Monstey_Function::copy,Monstey_Function::assign,Monstey_Function::pair,Monstey_Function::primary,Monstey_Function::secondary})for(Monstey_U64 body:{0ull,0x20ull,0x10005000ull,0xFFFFFFFFFFFFFFFFull}){Monstey_Config c;c.function=fn;c.primary=1;c.secondary=1;c.body=body;c.force_copy_old=fn==Monstey_Function::copy;out.push_back(c);}
 for(unsigned shape:{24u,25u}){Monstey_Config c;c.shape=shape;c.force_copy_old=true;out.push_back(c);c.frame_effect=1;out.push_back(c);}
 for(Monstey_Function fn:{Monstey_Function::assign,Monstey_Function::pair,Monstey_Function::primary,Monstey_Function::secondary})for(Monstey_U64 body:{0x10005000ull,0x100030B0ull,0x1000108Cull,0x1000007Cull}){Monstey_Config c;c.function=fn;c.primary=1;c.secondary=1;c.body=body;c.frame_effect=1;out.push_back(c);}
 {Monstey_Config c;c.function=Monstey_Function::primary;c.scenario=Monstey_Scenario::reentrant;c.primary=1;c.secondary=1;c.nested_virtual=true;c.body=0x10005000;c.nested_body=0x10006000;c.frame_effect=1;out.push_back(c);}
 return out;}
}
int main(){try{
 for(Monstey_Function fn:{Monstey_Function::assign,Monstey_Function::pair,Monstey_Function::primary,Monstey_Function::secondary})for(bool after:{false,true}){Monstey_Config c;c.function=fn;c.primary=1;c.secondary=1;c.frame_effect=1;c.body=fn==Monstey_Function::assign?0x100030B0:0x10005000;c.fail_call=1;c.after_effect=after;Monstey_Compare(c);}
 for(unsigned shape:{24u,25u})for(bool after:{false,true}){Monstey_Config c;c.shape=shape;c.force_copy_old=true;c.frame_effect=1;c.fail_call=2;c.after_effect=after;Monstey_Compare(c);}
 std::cout<<"{\"status\":\"PASS\",\"new_frame_exception_cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<",\"provider_exception_cases\":"<<Monstey_provider<<",\"service_events\":"<<Monstey_events<<",\"compared_image_bytes\":"<<Monstey_compared<<",\"actual_framed_implementation\":true,\"passed365_campaign_rerun\":false,\"native_original_entrypoint_invoked\":false,\"original_image_loaded\":false,\"original_instruction_bytes_interpreted\":false}\n";return 0;}catch(const std::exception& x){std::cerr<<"FAIL: "<<x.what()<<"\n";return 1;}}
