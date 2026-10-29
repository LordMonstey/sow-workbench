#include "Monstey_PlacementReference.hpp"
#include "Monstey_GadbManagerPlacement.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
using namespace Monstey_PlacementReview;
namespace {
using Monstey_U8=std::uint8_t;
struct Monstey_Patch{Monstey_U64 a,v;unsigned w;};
struct Monstey_Spec{
 std::string name;unsigned entry=6;Monstey_U64 base=0;std::size_t size=0x2000;Monstey_U64 object=0x100,start=0x500,count=0x20,target=0x300,expected=0x304,desired=0xFEDCBA9812345678,frame=0x1000;Monstey_Cells cells{0x900,0x908,0x910,0x800};std::vector<Monstey_Patch>patches;unsigned provider=0,repeat=1;
};
struct Monstey_Event{unsigned kind;Monstey_U64 site,address;Monstey_U32 comparand,desired,state;bool operator==(const Monstey_Event&)const=default;};
struct Monstey_Return{Monstey_U64 a,d,x,y;bool operator==(const Monstey_Return&)const=default;};
struct Monstey_Error{unsigned kind=0;Monstey_U64 site=0,address=0,width=0;bool writing=false;Monstey_U32 comparand=0,desired=0;bool operator==(const Monstey_Error&)const=default;};
struct Monstey_Result{std::vector<Monstey_U8>bytes;std::vector<Monstey_Event>events;std::vector<Monstey_Return>returns;Monstey_Error error;bool operator==(const Monstey_Result&)const=default;};
struct Monstey_ProviderException{unsigned identity;};
struct Monstey_ProviderRange{Monstey_U64 a;unsigned w;bool writing;};
Monstey_U64 Monstey_Read(Monstey_U64 base,std::span<Monstey_U8>b,Monstey_U64 a,unsigned n){
 if(a<base||a>std::numeric_limits<Monstey_U64>::max()-(n-1)||a-base>b.size()||n>b.size()-static_cast<std::size_t>(a-base))throw Monstey_ProviderRange{a,n,false};
 Monstey_U64 v=0;for(unsigned i=0;i<n;++i)v|=Monstey_U64(b[static_cast<std::size_t>(a-base)+i])<<(8*i);return v;
}
void Monstey_Write(Monstey_U64 base,std::span<Monstey_U8>b,Monstey_U64 a,Monstey_U64 v,unsigned n){
 if(a<base||a>std::numeric_limits<Monstey_U64>::max()-(n-1)||a-base>b.size()||n>b.size()-static_cast<std::size_t>(a-base))throw Monstey_ProviderRange{a,n,true};
 for(unsigned i=0;i<n;++i)b[static_cast<std::size_t>(a-base)+i]=static_cast<Monstey_U8>(v>>(8*i));
}
struct Monstey_Retire{
 Monstey_U64 base,address;std::span<Monstey_U8>b;std::vector<Monstey_Event>*events;
 Monstey_Retire(Monstey_U64 x,std::span<Monstey_U8>y,Monstey_U64 z,std::vector<Monstey_Event>&e):base(x),address(z),b(y),events(&e){}
 ~Monstey_Retire(){for(unsigned i=0;i<4;++i)b[static_cast<std::size_t>(address-base)+i]=static_cast<Monstey_U8>(Monstey_U32{0xDE57}>>(8*i));events->push_back({9,0,address,0,0,0});}
};
template<bool Monstey_Product>using Monstey_Memory=std::conditional_t<Monstey_Product,Monstey::Monstey_GadbBinderMemory,Monstey_Image>;
template<bool Monstey_Product>using Monstey_Services=std::conditional_t<Monstey_Product,Monstey::Monstey_GadbManagerPlacementServices,Monstey_PlacementReview::Monstey_Services>;
template<bool Monstey_Product>using Monstey_Request=std::conditional_t<Monstey_Product,Monstey::Monstey_GadbManagerCompareExchange32Call,Monstey_Call>;
template<bool Monstey_Product>auto& Monstey_Holder(Monstey_Services<Monstey_Product>&s){if constexpr(Monstey_Product)return s.compare_exchange32;else return s.cas;}
template<bool Monstey_Product>Monstey_Return Monstey_Invoke(const Monstey_Spec&s,Monstey_Memory<Monstey_Product>m,const Monstey_Services<Monstey_Product>&svc){
 if constexpr(Monstey_Product){
  const Monstey::Monstey_GadbManagerPlacementCells c{s.cells.g40,s.cells.g80,s.cells.gc0,s.cells.table};
  if(s.entry<3){const std::array f{Monstey::Monstey_GadbResetManagerBackend40,Monstey::Monstey_GadbResetManagerBackend80,Monstey::Monstey_GadbResetManagerBackendC0};const auto v=f[s.entry](m,s.object,s.desired);return{v.rax,v.rdx,0,0};}
  if(s.entry<6){const std::array f{Monstey::Monstey_GadbInitializeManagerBackendTail40,Monstey::Monstey_GadbInitializeManagerBackendTail80,Monstey::Monstey_GadbInitializeManagerBackendTailC0};const auto v=f[s.entry-3](m,s.object,s.start,s.count,c);return{v.raw.rax,v.raw.rdx,v.remaining,0};}
  const auto v=s.entry==6?Monstey::Monstey_GadbCompareExchangeManagerWord(m,s.target,s.expected,s.desired,s.frame,svc,c):Monstey::Monstey_GadbCompareExchangeManagerState(m,s.target,s.expected,s.frame,svc,c);return{v.raw.rax,v.raw.rdx,v.desired_register,v.target_identity};
 }else{
  if(s.entry<3){const std::array f{Monstey_RefReset40,Monstey_RefReset80,Monstey_RefResetC0};const auto v=f[s.entry](m,s.object,s.desired);return{v.a,v.d,0,0};}
  if(s.entry<6){const std::array f{Monstey_RefTail40,Monstey_RefTail80,Monstey_RefTailC0};const auto v=f[s.entry-3](m,s.object,s.start,s.count,s.cells);return{v.raw.a,v.raw.d,v.remaining,0};}
  const auto v=s.entry==6?Monstey_RefWord(m,s.target,s.expected,s.desired,s.frame,svc,s.cells):Monstey_RefState(m,s.target,s.expected,s.frame,svc,s.cells);return{v.raw.a,v.raw.d,v.desired,v.target};
 }
}
template<bool Monstey_Product>Monstey_Result Monstey_Run(const Monstey_Spec&s){
 Monstey_Result out;out.bytes.assign(s.size,0xA5);out.events.reserve(64);
 auto put=[&](Monstey_U64 a,Monstey_U64 v,unsigned w){if(a>=s.base&&a-s.base<=out.bytes.size()&&w<=out.bytes.size()-static_cast<std::size_t>(a-s.base))Monstey_Write(s.base,out.bytes,a,v,w);};
 put(0x1028,5,4);for(Monstey_U64 i=0;i<6;++i)put(0x800+i*4,5,4);put(0x300,7,4);put(0x304,7,4);put(0x314,11,4);put(0x900,0x1111111122222222,8);put(0x908,0x3333333344444444,8);put(0x910,0x5555555566666666,8);for(const auto&p:s.patches)put(p.a,p.v,p.w);
 Monstey_Memory<Monstey_Product>m{s.base,out.bytes};Monstey_Services<Monstey_Product>svc;
 auto primitive=[&out](auto memory,const auto&request,Monstey_U32 state){
  const Monstey_U64 site=[&]{if constexpr(Monstey_Product)return request.native_site;else return request.pc;}();
  out.events.push_back({1,site,request.address,request.comparand,request.desired,state});
  const auto old=static_cast<Monstey_U32>(Monstey_Read(memory.base,memory.bytes,request.address,4));if(old==request.comparand)Monstey_Write(memory.base,memory.bytes,request.address,request.desired,4);return old;
 };
 auto&holder=Monstey_Holder<Monstey_Product>(svc);
 if(s.provider==0)holder=[primitive](auto memory,const auto&request){return primitive(memory,request,0);};
 if(s.provider==2||s.provider==3)holder=[primitive,mode=s.provider](auto memory,const auto&request)->Monstey_U32{if(mode==3){static_cast<void>(primitive(memory,request,0));}throw Monstey_ProviderException{71};};
 if(s.provider==4)holder=[primitive,n=Monstey_U32{0}](auto memory,const auto&request)mutable{return primitive(memory,request,++n);};
 if(s.provider==5||s.provider==6){
  auto token=std::make_shared<Monstey_Retire>(s.base,std::span<Monstey_U8>(out.bytes),s.expected,out.events);
  holder=[token,primitive,&svc,&out,mode=s.provider](auto memory,const auto&request){const auto old=primitive(memory,request,0);auto*const owner=&svc;auto*const events=&out.events;events->push_back({2,0,0,0,0,0});Monstey_Holder<Monstey_Product>(*owner)={};events->push_back({3,0,0,0,0,0});if(mode==6)throw Monstey_ProviderException{72};return old;};
 }
 if(s.provider==7||s.provider==8){
  holder=[primitive,&svc,&out,s](auto memory,const auto&request){
   const auto old=primitive(memory,request,0);auto*const owner=&svc;auto*const events=&out.events;auto next=s;next.entry=7;next.frame=0x1100;next.target=0x310;next.expected=0x314;next.provider=0;next.repeat=1;
   if(s.provider==7)Monstey_Holder<Monstey_Product>(*owner)=[primitive](auto x,const auto&y){return primitive(x,y,99);};else Monstey_Holder<Monstey_Product>(*owner)={};
   events->push_back({4,0,0,0,0,0});
   try{const auto v=Monstey_Invoke<Monstey_Product>(next,memory,*owner);events->push_back({5,v.a,v.d,static_cast<Monstey_U32>(v.x),static_cast<Monstey_U32>(v.y),0});}
   catch(const Monstey_Fault&e){static_cast<void>(e);if constexpr(!Monstey_Product){if(e.kind!=Monstey_FaultKind::missing_cas)throw;events->push_back({6,e.pc,e.address,e.comparand,e.desired,0});}else throw;}
   catch(const Monstey::Monstey_GadbManagerCompareExchange32Fault&e){static_cast<void>(e);if constexpr(Monstey_Product)events->push_back({6,e.native_site,e.address,e.comparand,e.desired,0});else throw;}
   return old;
  };
 }
 try{for(unsigned i=0;i<s.repeat;++i)out.returns.push_back(Monstey_Invoke<Monstey_Product>(s,m,svc));}
 catch(const Monstey_Fault&e){out.error=e.kind==Monstey_FaultKind::memory?Monstey_Error{1,0,e.address,e.width,e.writing,0,0}:Monstey_Error{2,e.pc,e.address,e.width,false,e.comparand,e.desired};}
 catch(const Monstey::Monstey_GadbBufferedMemoryFault&e){out.error={1,0,e.address,e.width,e.writing,0,0};}
 catch(const Monstey::Monstey_GadbManagerCompareExchange32Fault&e){out.error={2,e.native_site,e.address,e.width,false,e.comparand,e.desired};}
 catch(const Monstey_ProviderException&e){out.error={3,e.identity,0,0,false,0,0};}
 catch(const Monstey_ProviderRange&e){out.error={4,0,e.a,e.w,e.writing,0,0};}
 return out;
}
std::vector<Monstey_Spec> Monstey_Cases(){
 std::vector<Monstey_Spec>v;
 auto add=[&v](Monstey_Spec s){s.name=std::to_string(v.size())+":"+s.name;v.push_back(std::move(s));};
 for(unsigned k=0;k<3;++k){
  Monstey_Spec s;s.entry=k;s.name="reset-normal";add(s);
  for(const std::size_t n:{std::size_t{0x15F},std::size_t{0x160},std::size_t{0x167},std::size_t{0x168},std::size_t{0x177},std::size_t{0x200},std::size_t{0x34F},std::size_t{0x350}}){s.size=n;s.name="reset-natural-cut";add(s);}
  s={};s.entry=k;s.base=0x10000000;s.object=s.base-0x60;s.size=0x100;s.name="reset-firststore-retained";add(s);
  s={};s.entry=k;s.object=std::numeric_limits<Monstey_U64>::max()-0x57;s.size=0x300;s.name="reset-modular-object";add(s);
 }
 for(unsigned k=3;k<6;++k){
  for(Monstey_U64 off=0;off<8;++off)for(const Monstey_U64 n:{Monstey_U64{0},Monstey_U64{0x20}}){Monstey_Spec s;s.entry=k;s.start+=off;s.count=n;s.name="tail-alignment-underflow";add(s);}
  for(const Monstey_U64 a:{Monstey_U64{0x128},Monstey_U64{0x110},Monstey_U64{0x508},Monstey_U64{0x528},Monstey_U64{0x12C},Monstey_U64{0x12F}}){Monstey_Spec s;s.entry=k;s.cells={a,a,a,0x800};s.name="tail-live-global-alias";add(s);}
  Monstey_Spec s;s.entry=k;s.start=std::numeric_limits<Monstey_U64>::max()-3;s.name="tail-wrapped-zero";add(s);
  s={};s.entry=k;s.base=0x10000000;s.object=s.base-0x28;s.size=0x100;s.start=s.base+0x50;s.name="tail-secondstore-firstfault";add(s);
  s={};s.entry=k;s.cells={0x8000,0x8000,0x8000,0x800};s.name="tail-late-global-range";add(s);
  s={};s.entry=k;s.size=0x134;s.start=0x40;s.cells={0x80,0x80,0x80,0};s.name="tail-final-store-range";add(s);
  for(const std::size_t n:{std::size_t{0x127},std::size_t{0x130},std::size_t{0x507},std::size_t{0x510},std::size_t{0x52F}}){s={};s.entry=k;s.size=n;s.cells={0x80,0x80,0x80,0x800};s.name="tail-prefix-cut";add(s);}
 }
 for(const Monstey_U32 sel:{0u,1u,2u,3u,4u,5u,6u,0xFFFFFFFFu})for(const Monstey_U32 old:{0u,5u,7u}){Monstey_Spec s;s.name="cas-selector-old";s.patches={{0x1028,sel,4},{0x300,old,4},{0x304,old==5?7:old,4}};if(sel>5)s.cells.table=0x8000;add(s);}
 for(const Monstey_U32 pol:{0xFFFFFFFFu,0u,1u,2u,3u,4u,5u,6u,0x7FFFFFFFu}){Monstey_Spec s;s.name="cas-live-policy";s.patches={{0x814,pol,4}};if(pol>5){s.provider=1;s.expected=0x8000;}add(s);}
 for(Monstey_U64 expected=0x2FD;expected<=0x303;++expected){Monstey_Spec s;s.expected=expected;s.name="cas-partial-exact-alias";s.patches={{0x2FC,0x0403020108070605,8},{0x304,0x0C0B0A0904030201,8}};add(s);}
 for(unsigned provider=0;provider<=8;++provider){Monstey_Spec s;s.provider=provider;s.name="cas-service-profile";if(provider==4)s.repeat=3;if(provider==5||provider==6)s.patches={{0x300,6,4}};add(s);}
 for(unsigned entry=6;entry<8;++entry){
  Monstey_Spec s;s.entry=entry;s.provider=1;s.target=0x8000;s.name="cas-missing-target-lazy";add(s);
  s.expected=0x8000;s.name="cas-expected-before-service";add(s);
  s={};s.entry=entry;s.provider=1;s.frame=0x8000;s.name="cas-frame-firstfault";add(s);
  s={};s.entry=entry;s.provider=1;s.cells.table=0x8000;s.name="cas-table-firstfault";add(s);
  s={};s.entry=entry;s.target=0;s.name="cas-zero-provider-consumed";add(s);
  s={};s.entry=entry;s.target=0x8000;s.name="cas-provider-range";add(s);
 }
 {Monstey_Spec s;s.entry=7;s.frame=0;s.name="wrapper-frame-wrap";add(s);}
 {Monstey_Spec s;s.entry=7;s.cells.table=0x100C;s.expected=0x1020;s.patches={{0x1020,0xFFFFFFFF,4},{0x300,5,4}};s.name="wrapper-table-and-expected-alias";add(s);}
 {Monstey_Spec s;s.entry=7;s.patches={{0x814,6,4},{0x1028,6,4}};s.name="wrapper-child-frame-required";add(s);}
 {Monstey_Spec s;s.entry=7;s.target=0x1020;s.expected=0x1020;s.name="wrapper-target-expected-knownstore";add(s);}
 {Monstey_Spec s;s.expected=0x300;s.name="cas-alias-success-no-ordinary-restore";add(s);}
 {Monstey_Spec s;s.provider=5;s.name="cas-success-retirement-expected";add(s);}
 return v;
}
}
int main(int argc,char**argv){
 try{
  if(argc!=2)throw std::runtime_error("result path required");
  const auto cases=Monstey_Cases();std::size_t checks=0,mismatch=0,natural=0,boundary=0,provider=0,events=0,bytes=0;std::vector<std::string>bad;
  for(const auto&s:cases){const auto want=Monstey_Run<false>(s);const auto got=Monstey_Run<true>(s);++checks;if(want.bytes!=got.bytes){++mismatch;bad.push_back(s.name+":bytes");}++checks;if(want.returns!=got.returns){++mismatch;bad.push_back(s.name+":raw-results");}++checks;if(want.error!=got.error){++mismatch;bad.push_back(s.name+":first-fault");}++checks;if(want.events!=got.events){++mismatch;bad.push_back(s.name+":service-lifetime-events");}if(got.error.kind==1)++natural;if(got.error.kind==2)++boundary;if(got.error.kind==3||got.error.kind==4)++provider;events+=got.events.size();bytes+=got.bytes.size();}
  std::ofstream out(argv[1]);out<<"{\"status\":\""<<(mismatch?"FAIL":"PASS")<<"\",\"cases\":"<<cases.size()<<",\"checks\":"<<checks<<",\"mismatches\":"<<mismatch<<",\"natural_memory_fault_cases\":"<<natural<<",\"missing_binding_fault_cases\":"<<boundary<<",\"provider_exception_cases\":"<<provider<<",\"service_lifetime_events\":"<<events<<",\"image_bytes_compared\":"<<bytes<<",\"all_eight_actual_functions\":true,\"product_memory_read_trace_claimed\":false,\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false,\"physical_atomic_ABI_qualified\":false,\"differences\":[";
  for(std::size_t i=0;i<bad.size();++i){if(i)out<<",";out<<"\""<<bad[i]<<"\"";}out<<"]}\n";if(!out)throw std::runtime_error("output failed");
  std::cout<<cases.size()<<" cases / "<<checks<<" comparisons / "<<mismatch<<" mismatches\n";for(const auto&x:bad)std::cout<<x<<"\n";return mismatch?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
