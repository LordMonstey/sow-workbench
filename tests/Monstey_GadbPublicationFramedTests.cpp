#include "Monstey_GadbPublicationFramed.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_U64=std::uint64_t;
struct Monstey_TestError:std::runtime_error { using std::runtime_error::runtime_error; };
struct Monstey_ProviderError { int marker; };
unsigned Monstey_Checks=0;
void Monstey_Check(bool v,const char *message) { ++Monstey_Checks; if(!v) throw Monstey_TestError(message); }
std::size_t Monstey_Offset(Monstey_GadbBinderMemory m,Monstey_U64 at,std::size_t n,bool writing) {
 if(at<m.base || at-m.base>m.bytes.size() || n>m.bytes.size()-static_cast<std::size_t>(at-m.base))
  throw Monstey_GadbBufferedMemoryFault(at,n,writing);
 return static_cast<std::size_t>(at-m.base);
}
Monstey_U64 Monstey_Read(Monstey_GadbBinderMemory m,Monstey_U64 at,std::size_t n=8) {
 const auto p=Monstey_Offset(m,at,n,false); Monstey_U64 v=0;
 for(std::size_t i=0;i<n;++i) v|=Monstey_U64(m.bytes[p+i])<<(i*8u); return v;
}
void Monstey_Write(Monstey_GadbBinderMemory m,Monstey_U64 at,Monstey_U64 v,std::size_t n=8) {
 const auto p=Monstey_Offset(m,at,n,true);
 for(std::size_t i=0;i<n;++i) m.bytes[p+i]=static_cast<std::uint8_t>(v>>(i*8u));
}
struct Monstey_Event { Monstey_U64 site,body,object,target,rax,rdx; };
struct Monstey_Fixture {
 static constexpr Monstey_U64 db=0x100,control=0x400,pair=0x500,owner=0x600,vt=0x700,q=0x1200;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x3000,0);
 std::vector<Monstey_Event> events;
 Monstey_GadbPublicationFramedServices services;
 Monstey_GadbBinderMemory memory() { return {0,bytes}; }
 void write(Monstey_U64 at,Monstey_U64 value,std::size_t n=8) { Monstey_Write(memory(),at,value,n); }
 Monstey_U64 read(Monstey_U64 at,std::size_t n=8) { return Monstey_Read(memory(),at,n); }
 std::uint32_t atomic(Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c) {
  const auto old=static_cast<std::uint32_t>(Monstey_Read(m,c.address,4)); Monstey_Write(m,c.address,old+c.addend,4); return old;
 }
 void bind_atomic() { services.atomic32=[this](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c){return atomic(m,c);}; }
 void control_at(Monstey_U64 c,std::uint32_t strong=1,std::uint32_t weak=1) {
  write(c,vt);write(c+8,strong,4);write(c+12,weak,4);write(vt,0xABC0);write(vt+8,0xABC8);
 }
 void record(const Monstey_GadbPublicationFramedVirtualCall &r) {
  const auto &c=r.call;events.push_back({c.native_site,r.caller_body_rsp,c.this_identity,c.target_identity,c.incoming.rax,c.incoming.rdx});
 }
 void bind_virtual() { services.virtual_call=[this](Monstey_GadbBinderMemory,const Monstey_GadbPublicationFramedVirtualCall &r){record(r);return Monstey_GadbBufferedResult{0xAA00+events.size(),0xDD00+events.size()};}; }
 void main() {
  write(q+0x60,9,4);write(q+0x68,7,4);write(q+0x70,0xCAFE);write(q+0x78,pair);
  write(q+0x80,0xD00D);write(q+0x88,0xB000);write(q+0x90,0xAA,4);write(q+0x98,0xBB,4);
  write(q+0xA0,0xA000);write(q+0xA8,owner);write(owner,0xF00D);write(pair,0x1234);write(pair+8,0);
 }
 Monstey_GadbBufferedResult publish() {return Monstey_GadbPublishDatabaseStorageFramed(memory(),db,0x11,0x22,0xBABA,q,services);}
};
void Monstey_Raw(Monstey_GadbBufferedResult r,Monstey_U64 a,Monstey_U64 d) {Monstey_Check(r.rax==a,"RAX");Monstey_Check(r.rdx==d,"RDX");}
void Monstey_ExpectedCounters(std::vector<std::uint8_t> &bytes,Monstey_U64 c,std::uint32_t strong,std::uint32_t weak) {
 Monstey_Write({0,bytes},c+8,strong,4);Monstey_Write({0,bytes},c+12,weak,4);
}
using Monstey_Case=std::pair<const char*,std::function<void()>>;
std::vector<Monstey_Case> Monstey_Cases() { return {
 {"six-real-entry-frame-paths",[] {
  for(unsigned entry=0;entry<6;++entry) {
   Monstey_Fixture f;f.bind_atomic();f.bind_virtual();f.control_at(f.control);const Monstey_U64 body=0x1A13;
   Monstey_U64 expected=body;std::size_t count=2;
   switch(entry) {
    case 0:count=1;(void)Monstey_GadbReleasePublicationSecondaryFramed(f.memory(),f.control,body,{8,9},f.services);break;
    case 1:(void)Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,body,{8,9},f.services);break;
    case 2:f.write(f.pair+8,f.control);expected=body-0x30;(void)Monstey_GadbReleasePublicationPairFramed(f.memory(),f.pair,body,{8,9},f.services);break;
    case 3:f.write(f.pair+8,f.control);expected=body-0x30;(void)Monstey_GadbAssignPublicationPairFramed(f.memory(),f.pair,0xDA,0,body,{8,9},f.services);break;
    case 4:f.control_at(1);f.write(f.pair,0xDA);f.write(f.pair+8,f.db);expected=body-0x60;(void)Monstey_GadbCopyPublicationPairFramed(f.memory(),f.db,f.pair,body,{8,9},f.services);break;
    case 5:f.main();f.write(f.db+0x60,f.control);expected=f.q-0x60;(void)f.publish();break;
    default:throw Monstey_TestError("bad fixture entry");
   }
   Monstey_Check(f.events.size()==count,"direct entry virtual count");
   for(const auto &e:f.events) Monstey_Check(e.body==expected,"actual helper body frame");
   Monstey_Check(f.events.back().site==0x1403DD720ULL,"actual secondary call site");
  }
 }},
 {"publication-same-site-Q90-and-Q60-real-alias",[] {
  Monstey_Fixture f;f.main();f.bind_atomic();f.bind_virtual();f.control_at(1);f.control_at(f.control);
  f.write(f.pair+8,f.q+0x20);f.write(f.db+0x60,f.control);(void)f.publish();
  Monstey_Check(f.events.size()==4,"copy-assign and old-pair virtual paths");
  Monstey_Check(f.events[0].body==f.q-0x90 && f.events[1].body==f.q-0x90,"copy actual CALL chain");
  Monstey_Check(f.events[2].body==f.q-0x60 && f.events[3].body==f.q-0x60,"release-pair actual CALL chain");
  Monstey_Check(f.events[1].site==0x1403DD720ULL && f.events[3].site==0x1403DD720ULL,"same site distinct actual frames");
 }},
 {"unsigned-wrap-without-frame-validation",[] {
  Monstey_Fixture f;f.bind_atomic();f.bind_virtual();f.control_at(f.control);f.write(f.pair+8,f.control);
  (void)Monstey_GadbAssignPublicationPairFramed(f.memory(),f.pair,0xDA,0,0x10,{8,9},f.services);
  Monstey_Check(f.events.size()==2,"wrap event count");
  for(const auto &e:f.events) Monstey_Check(e.body==0xFFFFFFFFFFFFFFE0ULL,"modulo64 body subtraction and same tail");
 }},
 {"unused-frame-is-lazy",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control,2,2);
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0xFFFFFFFFFFFFFFFFULL,{8,9},f.services),2,9);
  Monstey_Raw(Monstey_GadbReleasePublicationSecondaryFramed(f.memory(),f.control,0x11,{8,9},f.services),2,9);
  std::array<std::uint8_t,8> bytes{};Monstey_GadbPublicationFramedServices services;
  Monstey_Raw(Monstey_GadbReleasePublicationPairFramed({0x1008,bytes},0x1000,3,{0xAA,0xDD},services),0xAA,0xDD);
  Monstey_Check(f.events.empty(),"unused virtual context never consumed");
 }},
 {"live-target-zero-and-vptr-before-context",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);f.write(f.vt,0);f.write(f.vt+0x28,0);
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);Monstey_Check(r.caller_body_rsp==0x1811 && r.call.target_identity==0,"live zero target and explicit odd body");
   if(r.call.native_site==0x1403DD6FBULL) Monstey_Write(m,f.control,f.vt+0x20);
   else Monstey_Check(r.call.vtable_identity==f.vt+0x20 && r.call.incoming.rax==f.vt+0x20,"vptr resampled before framed slot8 event");
   return Monstey_GadbBufferedResult{0xFFAA,0xDDCC};
  };
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0x1811,{8,9},f.services),0xFFAA,0xDDCC);
 }},
 {"slot-range-fault-before-missing-framed-handler",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);f.write(f.control,0x2FFC);
  try{(void)Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0xFFFFFFFFFFFFFFFFULL,{8,9},f.services);throw Monstey_TestError("missing slot fault");}
  catch(const Monstey_GadbBufferedMemoryFault &e){Monstey_Check(e.address==0x2FFC && e.width==8 && !e.writing,"consumed target range precedes missing binding");}
  Monstey_Check(f.read(f.control+8,4)==0 && f.read(f.control+12,4)==1,"precise decrement prefix");
 }},
 {"missing-framed-handler-retains-generic-diagnostics",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;
  try{(void)Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0xABCDEF,{8,9},f.services);throw Monstey_TestError("missing handler fault");}
  catch(const Monstey_GadbPublicationBoundaryFault &e){
   Monstey_Check(e.kind==Monstey_GadbPublicationBoundaryKind::missing_virtual_call && e.native_site==0x1403DD6FBULL && e.call_site==0x1403DD6FBULL,"generic boundary kind/site");
   Monstey_Check(e.address==f.control && e.width==8 && e.slot==f.vt && e.target==0xABC0 && e.message_identity==0,"generic boundary packing unchanged");
  }
  auto expected=before;Monstey_Write({0,expected},f.control+8,0,4);Monstey_Check(f.bytes==expected,"no invented frame store on missing handler");
 }},
 {"declared-slot8-marker-full-image",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;const Monstey_U64 body=0x1813;
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);if(r.call.native_site==0x1403DD720ULL) Monstey_Write(m,r.caller_body_rsp-0x80,0xFFFFFFFFFFFFFFFEULL);
   return Monstey_GadbBufferedResult{0x9911,0x9922};
  };
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,body,{8,9},f.services),0x9911,0x9922);
  auto expected=before;Monstey_ExpectedCounters(expected,f.control,0,0);Monstey_Write({0,expected},body-0x80,0xFFFFFFFFFFFFFFFEULL);
  Monstey_Check(f.bytes==expected,"declared normal slot8 marker full image");Monstey_Check(f.events.size()==2 && f.events.back().body==body,"slot8 marker body is current tail body");
 }},
 {"declared-slot8-marker-natural-wrapped-write-fault",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);if(r.call.native_site==0x1403DD720ULL) Monstey_Write(m,r.caller_body_rsp-0x80,0xFFFFFFFFFFFFFFFEULL);
   return Monstey_GadbBufferedResult{0x11,0x22};
  };
  try{(void)Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0x40,{8,9},f.services);throw Monstey_TestError("missing marker fault");}
  catch(const Monstey_GadbBufferedMemoryFault &e){Monstey_Check(e.address==0xFFFFFFFFFFFFFFC0ULL && e.width==8 && e.writing,"natural declared marker fault");}
  auto expected=before;Monstey_ExpectedCounters(expected,f.control,0,0);Monstey_Check(f.bytes==expected && f.events.size()==2,"both decrements before slot8 marker fault");
 }},
 {"declared-slot8-partial-effect-then-provider-exception",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;const Monstey_U64 body=0x1800;
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);if(r.call.native_site==0x1403DD720ULL){Monstey_Write(m,r.caller_body_rsp-0x80,0xFFFFFFFFFFFFFFFEULL);throw Monstey_ProviderError{77};}
   return Monstey_GadbBufferedResult{0x11,0x22};
  };
  try{(void)Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,body,{8,9},f.services);throw Monstey_TestError("missing provider error");}
  catch(const Monstey_ProviderError &e){Monstey_Check(e.marker==77,"provider exception identity");}
  auto expected=before;Monstey_ExpectedCounters(expected,f.control,0,0);Monstey_Write({0,expected},body-0x80,0xFFFFFFFFFFFFFFFEULL);
  Monstey_Check(f.bytes==expected,"declared slot8 partial effect retained without rollback");
 }},
 {"arbitrary-declared-slot0-frame-effect-aliases-secondary",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);Monstey_Write(m,r.caller_body_rsp-0x80,0xFFFFFFFFFFFFFFFEULL);return Monstey_GadbBufferedResult{0x11,0x22};
  };
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,f.control+0x8C,{8,9},f.services),0xFFFFFFFEu,0x22);
  auto expected=before;Monstey_Write({0,expected},f.control+8,0,4);Monstey_Write({0,expected},f.control+12,0xFFFFFFFFFFFFFFFEULL);Monstey_Write({0,expected},f.control+12,0xFFFFFFFDu,4);
  Monstey_Check(f.bytes==expected && f.events.size()==1,"arbitrary declared slot0 alias suppresses slot8 via live counter");
 }},
 {"arbitrary-declared-slot0-frame-effect-aliases-vptr",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);const auto before=f.bytes;
  f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);Monstey_Write(m,r.caller_body_rsp-0x80,0xFFFFFFFFFFFFFFFEULL);return Monstey_GadbBufferedResult{0x11,0x22};
  };
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,f.control+0x80,{8,9},f.services),0x11,0x22);
  auto expected=before;Monstey_ExpectedCounters(expected,f.control,0,0);Monstey_Write({0,expected},f.control,0xFFFFFFFFFFFFFFFEULL);
  Monstey_Check(f.bytes==expected && f.events.size()==2,"arbitrary alias prefix and live wrapped target consumed");
  Monstey_Check(f.events.back().target==0 && f.events.back().site==0x1403DD720ULL,"numeric vptr plus8 wraps to valid zero target slot6");
 }},
 {"mutable-service-state-and-direct-frames",[] {
  Monstey_Fixture f;f.bind_atomic();f.control_at(f.control);
  f.services.virtual_call=[&,calls=0u](Monstey_GadbBinderMemory,const Monstey_GadbPublicationFramedVirtualCall &r) mutable {
   ++calls;f.record(r);Monstey_Check(r.caller_body_rsp==0x1800+calls,"current body per call with persistent target state");return Monstey_GadbBufferedResult{calls,0xDD};
  };
  Monstey_Raw(Monstey_GadbReleasePublicationSecondaryFramed(f.memory(),f.control,0x1801,{8,9},f.services),1,0xDD);f.write(f.control+12,1,4);
  Monstey_Raw(Monstey_GadbReleasePublicationSecondaryFramed(f.memory(),f.control,0x1802,{8,9},f.services),2,0xDD);
 }},
 {"atomic-self-replacement-retirement-before-framed-call",[] {
  Monstey_Fixture f;f.control_at(f.control,1,2);auto token=std::make_shared<int>(7);std::weak_ptr<int> weak=token;
  f.services.atomic32=[&,keep=token](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c) {
   Monstey_Check(*keep==7,"active atomic target alive");const auto old=f.atomic(m,c);f.services.atomic32=[&](Monstey_GadbBinderMemory mm,const Monstey_GadbPublicationAtomicCall &cc){return f.atomic(mm,cc);};
   Monstey_Check(!weak.expired(),"active target through self replacement");return old;
  };token.reset();
  f.services.virtual_call=[&](Monstey_GadbBinderMemory,const Monstey_GadbPublicationFramedVirtualCall &r) {Monstey_Check(weak.expired() && r.caller_body_rsp==0x1801,"retirement before framed virtual consumption");return Monstey_GadbBufferedResult{3,4};};
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0x1801,{8,9},f.services),2,4);
 }},
 {"reentrant-replacement-keeps-independent-current-body",[] {
  Monstey_Fixture f;f.control_at(f.control);const auto other=f.control+0x40;f.control_at(other,9,1);auto token=std::make_shared<int>(7);std::weak_ptr<int> weak=token;
  f.services.atomic32=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c){if(c.native_site==0x1403DD713ULL && c.address==f.control+12)Monstey_Check(weak.expired(),"outer retired target before tail atomic");return f.atomic(m,c);};
  f.services.virtual_call=[&,keep=token](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationFramedVirtualCall &r) {
   f.record(r);Monstey_Check(*keep==7 && r.caller_body_rsp==0x1700,"outer current body");
   f.services.virtual_call=[&](Monstey_GadbBinderMemory,const Monstey_GadbPublicationFramedVirtualCall &next){f.record(next);return Monstey_GadbBufferedResult{0x88,0x99};};
   Monstey_Raw(Monstey_GadbReleasePublicationSecondaryFramed(m,other,0x1817,{0x11,0x22},f.services),0x88,0x99);
   Monstey_Check(!weak.expired(),"outer active target survives reentrant replacement");return Monstey_GadbBufferedResult{0x33,0x44};
  };token.reset();
  Monstey_Raw(Monstey_GadbReleasePublicationPrimaryFramed(f.memory(),f.control,0x1700,{8,9},f.services),0x88,0x99);
  Monstey_Check(f.events.size()==3 && f.events[0].body==0x1700 && f.events[1].body==0x1817 && f.events[2].body==0x1700,"reentrant frame sequence restored by values");
 }},
 {"unchanged-trap-type-after-framed-prefix",[] {
  Monstey_Fixture f;f.main();f.write(f.db+0x20,3);
  f.services.trap=[&](Monstey_GadbBinderMemory,const Monstey_GadbPublicationTrapCall &c){Monstey_Check(c.call_site==0x1410ED7D7ULL && c.zero_store_site==0x140819113ULL && c.trap_entry==0x140819104ULL,"unchanged trap metadata");Monstey_Raw(c.incoming,3,0xF00D);throw Monstey_ProviderError{88};};
  try{(void)f.publish();throw Monstey_TestError("missing trap exception");}catch(const Monstey_ProviderError &e){Monstey_Check(e.marker==88 && f.read(f.db+0x20)==0xF00D && f.read(f.owner)==0,"unchanged trap prefix");}
 }}
 }; }
} // namespace
int main() {
 unsigned passed=0,failed=0;bool first=true;std::cout<<"{\"schema\":\"Monstey.SoWPublicationFramedOwnerTests/1\",\"cases\":[";
 for(const auto &test:Monstey_Cases()) {
  if(!first)std::cout<<',';first=false;std::cout<<"{\"name\":\""<<test.first<<"\",\"pass\":";
  try{test.second();++passed;std::cout<<"true}";}catch(const std::exception &e){++failed;std::cout<<"false,\"error\":\""<<e.what()<<"\"}";}catch(...){++failed;std::cout<<"false,\"error\":\"unexpected provider exception\"}";}
 }
 std::cout<<"],\"passed\":"<<passed<<",\"failed\":"<<failed<<",\"checks\":"<<Monstey_Checks<<"}\n";return failed==0?0:1;
}
