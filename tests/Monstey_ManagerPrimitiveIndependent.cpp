#include "Monstey_ManagerPrimitiveReference.hpp"
#include "Monstey_GadbManagerPrimitives.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
namespace R=Monstey_ManagerPrimitiveReview;namespace P=Monstey;
namespace {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
enum class Monstey_Entry{construct,destroy,leave,increment,decrement,cold};
enum class Monstey_Mode{plain,replace,reenter,clear_reenter,retire};
struct Monstey_Config{Monstey_Entry entry=Monstey_Entry::construct;Monstey_U64 base=0;Monstey_U32 count=2;unsigned shape=0,unaligned=0;Monstey_U64 target=0x1122334455667788ull,lock=0x600,rawA=0xFEDCBA9876543210ull,rawD=0xABCDEF0123456789ull;std::size_t visible=0x2000;Monstey_Mode mode=Monstey_Mode::plain;int fail=-1;bool after=false,missing=false,chain=false;};
struct Monstey_Event{unsigned phase,generation,local;R::Monstey_Call call;R::Monstey_Raw result;bool operator==(const Monstey_Event&)const=default;};
struct Monstey_Injected{unsigned ordinal,phase;};
struct Monstey_Outcome{unsigned kind=0;R::Monstey_Raw raw{};Monstey_U64 pc=0,address=0,slot=0,target=0,arg=0;std::size_t width=0;bool writing=false;std::string unexpected;bool operator==(const Monstey_Outcome&)const=default;};
struct Monstey_Host{
 const bool product;Monstey_Config c;std::vector<std::uint8_t>bytes=std::vector<std::uint8_t>(0x2000,0xA5);std::vector<Monstey_Event>events;std::vector<unsigned>states;
 Monstey_U64 base,g,gs,array,tls,enter,leave,marker,lock;unsigned calls=0,retired=0;
 R::Monstey_Bindings rb;P::Monstey_GadbManagerPrimitiveBindings pb;R::Monstey_Services rs;P::Monstey_GadbManagerPrimitiveServices ps;
 struct Monstey_Token{Monstey_Host*h;explicit Monstey_Token(Monstey_Host*x):h(x){}~Monstey_Token(){++h->retired;h->Put(h->marker,0xDE57,4);h->events.push_back({2,0,0,{}, {0xDE57,0}});}};
 explicit Monstey_Host(bool p,Monstey_Config cfg):product(p),c(cfg),base(c.base),g(base+0x100),gs(base+0x200),array(base+0x300+c.unaligned),tls(base+0x400+c.unaligned),enter(base+0x1800),leave(base+0x1808),marker(base+0x1600),lock(c.lock){
  Put(g,0xBAD,8);Put(gs,array,8);Put(array,tls,8);Put(tls+0x2C,c.count,4);Put(enter,c.target,8);Put(leave,c.target,8);
  if(c.entry==Monstey_Entry::destroy)Put(g,lock,8);
  switch(c.shape){case 1:g=enter;break;case 2:g=leave;break;case 3:g+=1;Put(g,lock,8);break;case 4:enter=g+4;break;case 5:gs=0;if(base==0)Put(0,array,8);break;case 7:gs=base+0x1FFC;break;case 8:Put(gs,base+0x1FFC,8);break;case 9:Put(array,base+0x1FFE-0x2C,8);break;case 10:Put(array,~Monstey_U64(0)-0x2B,8);if(base==0)Put(0,c.count,4);break;case 11:g=base+0x1FFC;break;case 12:enter=base+0x1FFC;break;case 13:leave=base+0x1FFC;break;case 15:Put(array,gs+4-0x2C,8);break;case 16:Put(array,array+4-0x2C,8);break;case 18:Put(array,~Monstey_U64(0)-0x2D,8);break;default:break;}
  rb={c.shape==6?std::optional<Monstey_U64>{}:std::optional<Monstey_U64>{gs},enter,leave};pb={rb.gs,enter,leave};Install(0);if(c.mode==Monstey_Mode::retire){auto token=std::make_shared<Monstey_Token>(this);Install(0,token);token.reset();}if(c.missing)Clear();
 }
 void Put(Monstey_U64 at,Monstey_U64 value,unsigned n){if(at<base||at-base>bytes.size()||n>bytes.size()-static_cast<std::size_t>(at-base))throw std::runtime_error("fixture bounds");for(unsigned i=0;i<n;++i)bytes[static_cast<std::size_t>(at-base)+i]=static_cast<std::uint8_t>(value>>(8u*i));}
 R::Monstey_Image View(){return {base,{bytes.data(),std::min(c.visible,bytes.size())},nullptr};}
 P::Monstey_GadbBinderMemory ProductView(){auto v=View();return {v.base,v.bytes};}
 R::Monstey_Image Normalize(P::Monstey_GadbBinderMemory v){return {v.base,v.bytes,nullptr};}
 R::Monstey_Call Normalize(const P::Monstey_GadbManagerSyncCall&x){return {x.operation==P::Monstey_GadbManagerSyncOperation::enter?R::Monstey_Role::enter:R::Monstey_Role::leave,x.native_site,x.slot_identity,x.target_identity,x.critical_section,{x.incoming.rax,x.incoming.rdx}};}
 void Clear(){if(product)ps.sync={};else rs.sync={};}
 void SetLeave(Monstey_U64 at){rb.leave=at;pb.leave_cell=at;}
 void Install(unsigned generation,std::shared_ptr<Monstey_Token>token={}){if(product)ps.sync=[this,generation,token,n=0u](P::Monstey_GadbBinderMemory m,const P::Monstey_GadbManagerSyncCall&call)mutable{const auto out=Service(Normalize(m),Normalize(call),generation,++n);return P::Monstey_GadbBufferedResult{out.a,out.d};};else rs.sync=[this,generation,token,n=0u](R::Monstey_Image m,const R::Monstey_Call&call)mutable{return Service(m,call,generation,++n);};}
 R::Monstey_Raw Service(R::Monstey_Image m,const R::Monstey_Call&call,unsigned generation,unsigned local){if(m.base!=base||m.bytes.data()!=bytes.data()||m.bytes.size()!=std::min(c.visible,bytes.size()))throw std::runtime_error("changed service image");const auto ordinal=calls++;states.push_back(local);events.push_back({0,generation,local,call,call.incoming});if(c.fail==static_cast<int>(ordinal)&&!c.after)throw Monstey_Injected{ordinal,0};m.Write(marker,0xC0DE0000u+local,4,0xEFFE);
  R::Monstey_Raw out{0xA000000000000000ull+(Monstey_U64(generation)<<16)+local,0xD000000000000000ull+(Monstey_U64(generation)<<16)+local};
  if(call.role==R::Monstey_Role::enter&&generation==0){const auto arg=call.argument;if(c.mode==Monstey_Mode::replace){m.Write(g,lock+1,8,0xEFFE);SetLeave(base+0x1900);Put(base+0x1900,0xABCDEF0012345678ull,8);Install(1);}else if(c.mode==Monstey_Mode::reenter){Install(1);out=Invoke(Monstey_Entry::construct,g+0x20,arg+1,{0xAA,0xDD});}else if(c.mode==Monstey_Mode::clear_reenter){Clear();out=Invoke(Monstey_Entry::leave,g,arg,{0xAA,0xDD});}else if(c.mode==Monstey_Mode::retire)Clear();}
  events.push_back({1,generation,local,call,out});if(c.fail==static_cast<int>(ordinal)&&c.after)throw Monstey_Injected{ordinal,1};return out;
 }
 R::Monstey_Raw Invoke(Monstey_Entry entry,Monstey_U64 guard,Monstey_U64 arg,R::Monstey_Raw raw){if(product){P::Monstey_GadbBufferedResult out{},in{raw.a,raw.d};switch(entry){case Monstey_Entry::construct:out=P::Monstey_GadbConstructManagerGuard(ProductView(),guard,arg,in,pb,ps);break;case Monstey_Entry::destroy:out=P::Monstey_GadbDestroyManagerGuard(ProductView(),guard,in,pb,ps);break;case Monstey_Entry::leave:out=P::Monstey_GadbLeaveManagerCriticalSection(ProductView(),arg,in,pb,ps);break;case Monstey_Entry::increment:out=P::Monstey_GadbIncrementManagerTls(ProductView(),pb);break;case Monstey_Entry::decrement:out=P::Monstey_GadbDecrementManagerTls(ProductView(),pb);break;case Monstey_Entry::cold:out=P::Monstey_GadbManagerNullAndReturn(ProductView(),in);break;}return {out.rax,out.rdx};}
  switch(entry){case Monstey_Entry::construct:return R::Monstey_RefConstruct(View(),guard,arg,raw,rb,rs);case Monstey_Entry::destroy:return R::Monstey_RefDestroy(View(),guard,raw,rb,rs);case Monstey_Entry::leave:return R::Monstey_RefLeave(View(),arg,raw,rb,rs);case Monstey_Entry::increment:return R::Monstey_RefIncrement(View(),rb);case Monstey_Entry::decrement:return R::Monstey_RefDecrement(View(),rb);case Monstey_Entry::cold:return R::Monstey_RefCold(View(),raw);}throw std::runtime_error("unknown entry");
 }
 R::Monstey_Raw Run(){auto out=Invoke(c.entry,g,lock,{c.rawA,c.rawD});if(c.chain)return Invoke(c.entry==Monstey_Entry::construct?Monstey_Entry::destroy:c.entry,g,lock,out);return out;}
};
Monstey_Outcome Monstey_Observe(Monstey_Host&h){try{return {0,h.Run()};}catch(const R::Monstey_Fault&e){Monstey_Outcome x;x.kind=e.kind==R::Monstey_FaultKind::memory?1:e.kind==R::Monstey_FaultKind::missing_gs?2:3;x.address=e.address;x.width=e.width;x.writing=e.writing;if(x.kind!=1){x.pc=e.pc;x.slot=e.slot;x.target=e.target;x.arg=e.argument;}return x;}catch(const P::Monstey_GadbBufferedMemoryFault&e){Monstey_Outcome x;x.kind=1;x.address=e.address;x.width=e.width;x.writing=e.writing;return x;}catch(const P::Monstey_GadbManagerPrimitiveBoundaryFault&e){Monstey_Outcome x;x.kind=e.kind==P::Monstey_GadbManagerPrimitiveBoundaryKind::missing_gs58_binding?2:3;x.pc=e.native_site;x.address=e.address;x.width=e.width;x.slot=e.slot_identity;x.target=e.target_identity;x.arg=e.argument;return x;}catch(const Monstey_Injected&e){Monstey_Outcome x;x.kind=4;x.pc=e.ordinal;x.width=e.phase;return x;}catch(const std::exception&e){Monstey_Outcome x;x.kind=5;x.unexpected=e.what();return x;}}
std::uint64_t Monstey_cases=0,Monstey_checks=0,Monstey_memory=0,Monstey_boundary=0,Monstey_provider=0,Monstey_bytes=0,Monstey_events=0;
void Monstey_Check(bool ok,const char*why){++Monstey_checks;if(!ok)throw std::runtime_error(why);}
void Monstey_Compare(Monstey_Config c){Monstey_Host expected(false,c),actual(true,c);auto a=Monstey_Observe(expected),b=Monstey_Observe(actual);if(a!=b)std::cerr<<"entry="<<static_cast<unsigned>(c.entry)<<" shape="<<c.shape<<" mode="<<static_cast<unsigned>(c.mode)<<" expected="<<a.kind<<","<<a.raw.a<<","<<a.raw.d<<","<<a.address<<","<<a.width<<" actual="<<b.kind<<","<<b.raw.a<<","<<b.raw.d<<","<<b.address<<","<<b.width<<"\n";Monstey_Check(a==b,"raw words or actual first-fault fields differ");Monstey_Check(expected.bytes==actual.bytes,"whole backing image or fault prefix differs");Monstey_Check(expected.events==actual.events,"actual primitive DTO/order/raw/lifetime events differ");Monstey_Check(expected.calls==actual.calls&&expected.states==actual.states&&expected.retired==actual.retired,"current holder state/invocation/retirement differs");if(a.kind==1)++Monstey_memory;else if(a.kind==2||a.kind==3)++Monstey_boundary;else if(a.kind==4)++Monstey_provider;++Monstey_cases;Monstey_bytes+=actual.bytes.size();Monstey_events+=actual.events.size();}
std::vector<Monstey_Config>Monstey_Cases(){std::vector<Monstey_Config>out;
 for(auto base:std::array<Monstey_U64,3>{0,0x100000000ull,~Monstey_U64(0)-0x1FFF})for(unsigned align:{0u,1u})for(Monstey_U32 count:{0u,1u,2u,0xFFFFFFFFu})for(auto entry:{Monstey_Entry::increment,Monstey_Entry::decrement}){Monstey_Config c;c.base=base;c.unaligned=align;c.count=count;c.entry=entry;out.push_back(c);}
 for(auto entry:{Monstey_Entry::construct,Monstey_Entry::destroy,Monstey_Entry::leave,Monstey_Entry::cold})for(auto a:std::array<Monstey_U64,2>{0,~Monstey_U64(0)})for(auto d:std::array<Monstey_U64,2>{0,0x8000000000000001ull}){Monstey_Config c;c.entry=entry;c.rawA=a;c.rawD=d;out.push_back(c);}
 for(auto entry:{Monstey_Entry::construct,Monstey_Entry::destroy,Monstey_Entry::leave})for(auto target:std::array<Monstey_U64,2>{0,~Monstey_U64(0)}){Monstey_Config c;c.entry=entry;c.target=target;c.lock=~Monstey_U64(0);out.push_back(c);c.missing=true;out.push_back(c);}
 for(unsigned shape:{1u,3u,4u,11u,12u}){Monstey_Config c;c.shape=shape;out.push_back(c);c.missing=true;out.push_back(c);}
 for(unsigned shape:{2u,3u,11u,13u}){Monstey_Config c;c.entry=Monstey_Entry::destroy;c.shape=shape;out.push_back(c);c.missing=true;out.push_back(c);}
 for(auto entry:{Monstey_Entry::increment,Monstey_Entry::decrement})for(unsigned shape:{5u,6u,7u,8u,9u,10u,15u,16u,18u}){Monstey_Config c;c.entry=entry;c.shape=shape;c.count=0;out.push_back(c);c.count=2;out.push_back(c);}
 for(unsigned shape:{5u,6u}){Monstey_Config c;c.entry=Monstey_Entry::increment;c.shape=shape;c.base=0x100000000ull;out.push_back(c);}
 for(auto entry:{Monstey_Entry::construct,Monstey_Entry::destroy})for(std::size_t n:{0x104u,0x1804u,0x180Cu}){Monstey_Config c;c.entry=entry;c.visible=n;out.push_back(c);}
 for(auto entry:{Monstey_Entry::increment,Monstey_Entry::decrement})for(std::size_t n:{0x204u,0x304u,0x42Eu}){Monstey_Config c;c.entry=entry;c.visible=n;out.push_back(c);}
 for(std::size_t n:{0u,3u,4u}){Monstey_Config c;c.entry=Monstey_Entry::cold;c.visible=n;out.push_back(c);}
 for(auto mode:{Monstey_Mode::plain,Monstey_Mode::replace,Monstey_Mode::reenter,Monstey_Mode::clear_reenter,Monstey_Mode::retire}){Monstey_Config c;c.mode=mode;c.chain=mode!=Monstey_Mode::retire;out.push_back(c);}
 for(unsigned shape:{15u,16u}){Monstey_Config c;c.entry=Monstey_Entry::increment;c.shape=shape;c.chain=true;out.push_back(c);}
 return out;}
}
int main(){try{for(const auto&c:Monstey_Cases())Monstey_Compare(c);for(auto entry:{Monstey_Entry::construct,Monstey_Entry::destroy,Monstey_Entry::leave})for(bool after:{false,true}){Monstey_Config c;c.entry=entry;c.fail=0;c.after=after;Monstey_Compare(c);}for(bool after:{false,true}){Monstey_Config c;c.mode=Monstey_Mode::reenter;c.fail=1;c.after=after;Monstey_Compare(c);}std::cout<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<",\"natural_memory_fault_cases\":"<<Monstey_memory<<",\"consumed_boundary_fault_cases\":"<<Monstey_boundary<<",\"provider_exception_cases\":"<<Monstey_provider<<",\"service_lifetime_events\":"<<Monstey_events<<",\"whole_image_bytes_compared\":"<<Monstey_bytes<<",\"actual_manager_implementation\":true,\"all_six_product_functions\":true,\"product_read_hooks\":false,\"reference_passive_events_used_as_product_trace\":false,\"physical_ABI_OS_nullpage_concurrency_qualified\":false,\"native_original_entrypoint_invoked\":false,\"original_image_loaded\":false,\"original_instruction_bytes_interpreted\":false}\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
