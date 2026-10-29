#include "Monstey_CategoryConcreteFixtureFinal.hpp"
#include "Monstey_GadbCategoryReader.hpp"
#include <iostream>
namespace R=Monstey_CategoryReview;
namespace {
std::uint64_t Monstey_cases=0,Monstey_checks=0,Monstey_faults=0,Monstey_events=0,Monstey_bytes=0;
void Monstey_Check(bool value,const char* message){++Monstey_checks;if(!value)throw std::runtime_error(message);}
R::Monstey_Machine Monstey_Candidate(R::Monstey_ComposedFixture& f){try{const auto r=Monstey::Monstey_GadbLoadCategoryFromBuffered(f.activation,0x1122334455667788ull,f.bank,f.category,f.optionalLookup,f.F,f.selector,f.buffered,f.cells,f.cell);return {r.rax,r.rdx};}catch(const Monstey::Monstey_GadbCategoryDispatchFault& e){throw R::Monstey_ReferenceDispatchFault(e.native_site,e.slot,e.target,e.expected);}}
void Monstey_Compare(R::Monstey_Case c){R::Monstey_ComposedFixture expected(c),actual(c);const auto reference=R::Monstey_Observe([&]{return expected.Reference();});const auto product=R::Monstey_Observe([&]{return Monstey_Candidate(actual);});
 if(!(reference==product)){std::cerr<<"profile="<<c.profile<<" a="<<c.anon<<" n="<<c.named<<" payload="<<c.payload<<" fault="<<c.faultAt<<" expected="<<reference.kind<<"/"<<reference.rax<<"/"<<reference.rdx<<"/"<<reference.address<<"/"<<reference.other<<" actual="<<product.kind<<"/"<<product.rax<<"/"<<product.rdx<<"/"<<product.address<<"/"<<product.other<<"\n";}
 Monstey_Check(reference==product,"complete outcome/rawRAX/rawRDX/first fault equals pre-source reference");
 Monstey_Check(expected.events==actual.events,"exact external primitive call order/arguments/targets");
 const auto a=actual.memory.Bytes(),b=expected.memory.Bytes();const auto mismatch=std::mismatch(a.begin(),a.end(),b.begin());if(mismatch.first!=a.end())std::cerr<<"first byte mismatch at "<<actual.base+static_cast<std::uint64_t>(mismatch.first-a.begin())<<" profile="<<c.profile<<"\n";
 Monstey_Check(mismatch.first==a.end(),"complete owned memory including private scratch/fault prefix");
 Monstey_Check(actual.epoch==expected.epoch&&actual.global.Load()==expected.global.Load(),"actual activation global/thread epoch");for(unsigned i=0;i<5;++i)Monstey_Check(actual.guards[i]->Load()==expected.guards[i]->Load(),"actual activation guard state");
 const auto x=actual.synchronization.Stats(),y=expected.synchronization.Stats();Monstey_Check(x.header_calls==y.header_calls&&x.wait_calls==y.wait_calls&&x.wake_calls==y.wake_calls,"actual synchronization call counts");
 if(c.faultAt>=0){Monstey_Check(reference.kind=="primitive"&&reference.address==static_cast<unsigned>(c.faultAt),"primitive fault retires at exact injected external call");Monstey_Check(expected.events.size()==static_cast<unsigned>(c.faultAt)+1u,"primitive failure leaves exact call prefix");++Monstey_faults;}
 if(c.profile==31)Monstey_Check(actual.Get(actual.category+0x40,4)==0&&actual.Get(actual.F-0x0C,4)==120,"no Named iteration retains old ESI");
 if(c.profile==32)Monstey_Check(actual.Get(actual.category+0x40,4)==17&&actual.Get(actual.category+0x28,4)==0,"after Named reloads ESI/EDX");
 ++Monstey_cases;Monstey_events+=actual.events.size();Monstey_bytes+=a.size();}
}
int main(){try{
 for(const auto& c:R::Monstey_Cases())Monstey_Compare(c);
 for(unsigned a=0;a<=2;++a)for(unsigned n=0;n<=2;++n)Monstey_Compare({0,a,n,1});
 for(const R::Monstey_Case c:std::vector<R::Monstey_Case>{{0,1,1,0},{0,2,2,1},{4,1,1,0},{5,1,1,0},{3,1,1,0},{23,1,0,1}}){R::Monstey_ComposedFixture baseline(c);const auto o=R::Monstey_Observe([&]{return baseline.Reference();});(void)o;const auto count=baseline.events.size();for(unsigned i=0;i<count;++i)for(bool after:{false,true}){auto f=c;f.faultAt=static_cast<int>(i);f.afterFault=after;Monstey_Compare(f);}}
 std::cout<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<",\"primitive_fault_cases\":"<<Monstey_faults<<",\"primitive_events\":"<<Monstey_events<<",\"compared_arena_bytes\":"<<Monstey_bytes<<",\"all_callees_concrete\":true,\"scripted_machine_results\":false,\"native_original_entrypoint_invoked\":false,\"original_image_loaded\":false,\"original_instruction_bytes_interpreted\":false}\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
