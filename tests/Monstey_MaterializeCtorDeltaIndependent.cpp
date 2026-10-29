#include "Monstey_MaterializeFixture.hpp"
#include "Monstey_MaterializeCtorCallReference.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace Monstey;using namespace Monstey_IndependentMaterialize;
namespace {
using Monstey_U32=std::uint32_t;
constexpr Monstey_U32 Monstey_None=0x7FFFFFFFu;
enum class Monstey_Route {attribute,node,root};
struct Monstey_Config {
 Monstey_Route route=Monstey_Route::attribute;Monstey_U32 output_delta=Monstey_None;int offset_delta=0;
 bool root_pool_zero=false,raw_args=false,two_tokens=false,partial_scan=false,root_name_alias=false,late_root_count=false;
 Monstey_U32 raw_type=3,allocation=0;unsigned lower_fault=0,root_cut=0xFFFFFFFFu;
};
struct Monstey_Stop {}; struct Monstey_Injected {};
struct Monstey_Outcome {unsigned kind=0;Monstey_U32 at=0,width=0;bool write=false;bool operator==(const Monstey_Outcome&)const=default;};
std::uint64_t Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0,Monstey_Groups=0;
void Monstey_Check(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
Monstey_U32 Monstey_Add(Monstey_U32 x,int d){return x+static_cast<Monstey_U32>(d);}
Monstey_U32 Monstey_Token(const Monstey_RefMemory& m,Monstey_U32 source){const auto ch=m.Monstey_Read(source,1);if(ch=='x')return 0;if(ch=='y')return 1;if(ch=='z')return 2;throw std::runtime_error("Unexpected explicit numeric fixture token");}
void Monstey_Run(const Monstey_Config& cfg) {
 ++Monstey_Cases;Monstey_StringFixture fixture("Cat/Rec/A","x;",3);fixture.high_bool=true;
 const auto base=fixture.base,frame=base+(cfg.lower_fault?0x100u:0x6000u),context=base+0x1000u,map=base+0x2000u,head=base+0x2100u,node=base+0x2200u,cursor=base+0x3000u,allocation=base+0x8000u;
 const Monstey_RefMemory full(fixture.Monstey_Memory());
 const auto tokens=cfg.two_tokens?2u:1u;const bool is_attr=cfg.route==Monstey_Route::attribute;
 Monstey_U32 root=allocation,initial=allocation+20u;
 if(cfg.route==Monstey_Route::root&&cfg.output_delta!=Monstey_None)root=frame+cfg.output_delta;
 if(cfg.root_cut!=0xFFFFFFFFu)root=base+0xF000u;
 if(is_attr&&cfg.output_delta!=Monstey_None)initial=frame+cfg.output_delta-32u;
 if(cfg.route==Monstey_Route::node&&cfg.output_delta!=Monstey_None)initial=frame+cfg.output_delta;
 const std::array<Monstey_U32,6> counts{is_attr?1u:0u,is_attr?1u:0u,is_attr?1u:0u,is_attr?tokens:1u,is_attr?(cfg.two_tokens?64u:14u):(cfg.route==Monstey_Route::root&&!cfg.root_pool_zero?64u:0u),0};
 for(unsigned n=0;n<6;++n)full.Monstey_Write(context+4*n,counts[n]);
 const auto value_base=initial+16u*(counts[0]+counts[1]+counts[2]),pool=value_base+4u*counts[3];
 full.Monstey_Write(context+20,pool+counts[4]);full.Monstey_Write(cursor,initial);
 full.Monstey_Write(map+4,head);full.Monstey_Write(head,is_attr?node:head);full.Monstey_Write(head+4,head);full.Monstey_Write(head+8,is_attr?node:head);full.Monstey_Write(head+0x4D,1,1);fixture.Monstey_Set(head+12,"Cat/Rec/A");
 full.Monstey_Write(node,head);full.Monstey_Write(node+4,head);full.Monstey_Write(node+8,head);full.Monstey_Write(node+0x4D,0,1);fixture.Monstey_Set(node+12,"Cat/Rec/A");fixture.Monstey_Set(node+0x2C,cfg.two_tokens?"x;y;":"x;");full.Monstey_Write(node+0x28,cfg.raw_type);full.Monstey_Write(node+0x48,0x87654321);
 full.Monstey_Write(frame+0x130,root);full.Monstey_Write(frame+0x134,context);full.Monstey_Write(frame+0x138,map);full.Monstey_Write(frame+0x13C,cfg.allocation?cfg.allocation:allocation);
 full.Monstey_Write(base+0x7000,base+0x7004);full.Monstey_Write(base+0x7004,0xFFFFFFFF);full.Monstey_Write(base+0x7010,base+0x7014);full.Monstey_Write(base+0x7020,0,1);
 auto imports=fixture.Monstey_Imports();imports.npos_pointer_cell=base+0x7000;imports.empty_cstring=base+0x7020;imports.root_name_cell=cfg.root_name_alias?frame-4u:base+0x7010;
 const auto begin=cfg.lower_fault?frame-0x60u+cfg.lower_fault-base:0u;
 const auto end=cfg.root_cut==0xFFFFFFFFu?static_cast<Monstey_U32>(fixture.image.size()):root-base+cfg.root_cut;
 const auto view=[&](std::vector<std::uint8_t>& image){return Monstey_FearDbMutableMemory{base+begin,std::span(image).subspan(begin,end-begin)};};
 std::vector<std::uint8_t> before;Monstey_U32 cached_offset=0;unsigned name_calls=0,name_destroys=0;bool captured=false;
 const auto name_object=frame-(is_attr?0x38u:0x30u);
 imports.atoi_bits=[&](auto memory,auto source){const auto t=Monstey_Token(Monstey_RefMemory(memory),source);return cfg.raw_type==1?(t?0x80000000u:0u):0x11111111u*(t+1u);};
 imports.atof_fstp32_bits=[](auto memory,auto source){return Monstey_Token(Monstey_RefMemory(memory),source)?0xBF800002u:0x3F800001u;};
 imports.sscanf=[&](auto memory,auto source,auto dimensions,auto destinations){const Monstey_RefMemory m(memory);const auto t=Monstey_Token(m,source);for(Monstey_U32 k=0;k<(cfg.partial_scan?1u:dimensions);++k)m.Monstey_Write(destinations[k],0xF0000000u+0x100u*t+k);return 0xFFFFFFFFu;};
 imports.strncpy=[&](auto memory,auto destination,auto source,auto count){const Monstey_RefMemory m(memory);if(cfg.root_pool_zero){Monstey_Check(count==0xFFFFFFFBu&&destination==pool+4u&&m.Monstey_Read(source,1)=='C',"wrapped remaining count/import epoch differs");before=fixture.image;captured=true;m.Monstey_Write(destination,0x77,1);throw Monstey_Injected{};}bool padding=false;for(Monstey_U32 k=0;k<count;++k){const auto byte=padding?0u:m.Monstey_Read(source+k,1);padding=padding||byte==0;m.Monstey_Write(destination+k,byte,1);}if(cfg.route!=Monstey_Route::root&&source==name_object+4&&name_calls==1){before=fixture.image;Monstey_RefMemory(view(before)).Monstey_Write(destination+count,0,1);captured=true;}};
 auto primitive=imports.string;
 imports.string=[&](auto memory,auto op,auto receiver,auto a,auto b,auto c)->Monstey_U32 {
  const auto result=primitive(memory,op,receiver,a,b,c);const Monstey_RefMemory m(memory);
  if(op==Monstey_FearUnpackedStringOp::copy&&receiver==name_object&&cfg.route!=Monstey_Route::root) {
   if(is_attr) {
    const bool consumes=cfg.raw_type>=1&&cfg.raw_type<=8&&cfg.raw_type!=5;const auto expected_count=consumes?tokens:0u;
    Monstey_Check(m.Monstey_Read(frame-12)==expected_count&&m.Monstey_Read(frame-20)==cfg.raw_type,"native full-type/count dispatch before ctor differs");
    Monstey_Check(m.Monstey_Read(context+12)==tokens*2u,"native token count increment differs");
    Monstey_Check(m.Monstey_Read(frame+0x18)==0&&m.Monstey_Read(frame+0x20)==0&&m.Monstey_Read(frame+0x38)==0&&m.Monstey_Read(frame+0x1C)==0,"AL-only initial comparisons flushed a spurious record/category");
    for(Monstey_U32 t=0;t<tokens;++t) {
     Monstey_U32 expected=0xA5A5A5A5u;
     if(cfg.raw_type==1)expected=t?1u:0u;else if(cfg.raw_type==2)expected=t?0xBF800002u:0x3F800001u;else if(cfg.raw_type==3)expected=0x11111111u*(t+1u);
     else if(cfg.raw_type==4)expected=pool+t*2u;else if(cfg.raw_type>=6&&cfg.raw_type<=8)expected=pool+t*(cfg.raw_type-4u)*4u;
     Monstey_Check(m.Monstey_Read(value_base+t*4u)==expected,"native scalar/vector/string cell or skip differs");
     if(cfg.raw_type>=6&&cfg.raw_type<=8)for(Monstey_U32 k=0;k<cfg.raw_type-4u;++k)Monstey_Check(m.Monstey_Read(expected+4*k)==((!cfg.partial_scan||k==0)?0xF0000000u+t*0x100u+k:0xA5A5A5A5u),"partial scan outputs/ignored return differ");
    }
   }
   if(cfg.raw_args){m.Monstey_Write(frame-12,0x10001);m.Monstey_Write(frame-(is_attr?20u:16u),is_attr?0x10003u:0xEF123456u);m.Monstey_Write(frame-(is_attr?16u:8u),is_attr?0xBEEF1200u:0xAB345678u);if(is_attr)m.Monstey_Write(frame-8,0xF1234567);}
   if(cfg.offset_delta){const auto pointer=Monstey_Add(frame,cfg.offset_delta);m.Monstey_Write(frame-4,pointer);m.Monstey_Write(pointer,0);}
  }
  if(cfg.route!=Monstey_Route::root&&op==Monstey_FearUnpackedStringOp::c_str&&receiver==name_object) {
   if(++name_calls==1){cached_offset=m.Monstey_Read(frame-4);before=fixture.image;captured=true;}
   else if(name_calls==2)throw Monstey_Stop{};
  }
  if(cfg.route==Monstey_Route::root&&op==Monstey_FearUnpackedStringOp::destroy&&receiver==frame-0x30u&&++name_destroys==2) {
   if(cfg.late_root_count)m.Monstey_Write(context,0x10001);
   before=fixture.image;captured=true;
  }
  return result;
 };
 Monstey_Outcome actual,expected;
 try{Monstey_Check(Monstey_FearUnpackedMaterialize(view(fixture.image),cursor,frame,imports)==1,"materializer AL differs");}
 catch(const Monstey_Stop&){actual.kind=1;}catch(const Monstey_Injected&){actual.kind=3;}catch(const Monstey_FearDbFault& f){actual={2,f.address,f.width,f.writing};}
 Monstey_Check(captured,"native pre-ctor snapshot not reached");auto reference=before;
 if(cfg.root_pool_zero){Monstey_RefMemory(view(reference)).Monstey_Write(pool+4u,0x77,1);Monstey_Check(actual.kind==3&&name_destroys==1,"injected import exception swallowed or cleanup fabricated");Monstey_Check(reference==fixture.image,"injected import failure memory prefix differs");++Monstey_Faults;return;}
 try {
  if(cfg.route==Monstey_Route::attribute){Monstey_RefAttributeCall(view(reference),frame,pool,cached_offset);expected.kind=1;}
  else if(cfg.route==Monstey_Route::node){Monstey_RefNodeCall(view(reference),frame,pool,cached_offset);expected.kind=1;}
  else Monstey_RefRootCall(view(reference),frame,context,root,imports.root_name_cell);
 }catch(const Monstey_RefFault& f){expected={2,f.address,f.width,f.writing};}
 Monstey_Check(expected==actual,"ctor caller/callee first-fault or checkpoint mismatch");
 Monstey_Check(reference==fixture.image,"constructor whole-memory/PUSH/interleaved alias effects differ");if(expected.kind==2)++Monstey_Faults;
}
void Monstey_DeltaCases() {
 ++Monstey_Groups;
 for(const auto d:std::array<int,7>{-0x60,-0x5C,-0x58,-0x54,-0x50,-0x4C,-0x48}){Monstey_Config c;c.output_delta=static_cast<Monstey_U32>(d);Monstey_Run(c);}
 {Monstey_Config c;c.raw_args=true;Monstey_Run(c);}
 for(const auto d:std::array<int,4>{-0x50,-0x58,-0x54,-0x4C}){Monstey_Config c;c.offset_delta=d;Monstey_Run(c);}
 ++Monstey_Groups;
 for(const auto type:std::array<Monstey_U32,13>{0,1,2,3,4,5,6,7,8,9,0x10003u,0xFFFFFFFFu,0x80000008u}){Monstey_Config c;c.raw_type=type;c.two_tokens=true;Monstey_Run(c);if(type>=6&&type<=8){c.partial_scan=true;Monstey_Run(c);}}
 ++Monstey_Groups;
 for(const auto d:std::array<int,5>{-0x50,-0x4C,-0x48,-0x44,-0x40}){Monstey_Config c;c.route=Monstey_Route::node;c.output_delta=static_cast<Monstey_U32>(d);Monstey_Run(c);}
 {Monstey_Config c;c.route=Monstey_Route::node;c.raw_args=true;Monstey_Run(c);c.offset_delta=-0x48;Monstey_Run(c);}
 for(const auto d:std::array<int,3>{-0x44,-0x48,-0x4C}){Monstey_Config c;c.route=Monstey_Route::node;c.offset_delta=d;Monstey_Run(c);}
 ++Monstey_Groups;
 for(const auto d:std::array<int,4>{-0x10,-0x0C,-8,-4}){Monstey_Config c;c.route=Monstey_Route::root;c.output_delta=static_cast<Monstey_U32>(d);Monstey_Run(c);}
 {Monstey_Config c;c.route=Monstey_Route::root;Monstey_Run(c);c.root_name_alias=true;Monstey_Run(c);c.root_name_alias=false;c.late_root_count=true;Monstey_Run(c);c.late_root_count=false;c.allocation=0xDEADBEEF;Monstey_Run(c);}
 ++Monstey_Groups;
 for(unsigned delta=1;delta<=21;delta+=4){Monstey_Config c;c.lower_fault=delta;Monstey_Run(c);}
 ++Monstey_Groups;
 for(unsigned cut=0;cut<=20;++cut){Monstey_Config c;c.route=Monstey_Route::root;c.root_cut=cut;Monstey_Run(c);}
 ++Monstey_Groups;{Monstey_Config c;c.route=Monstey_Route::root;c.root_pool_zero=true;Monstey_Run(c);}
}
void Monstey_CompleteAssociations() {
 ++Monstey_Groups;
 for(bool two_records:{false,true}) {
  ++Monstey_Cases;Monstey_StringFixture fixture("Cat/Rec/A","x;y;",3);fixture.high_bool=true;
  const auto b=fixture.base,f=b+0x6000u,ctx=b+0x1000u,map=b+0x2000u,head=b+0x2100u,n0=b+0x2200u,n1=b+0x2300u,cursor=b+0x3000u,root=b+0x8000u;
  const Monstey_RefMemory m(fixture.Monstey_Memory());
  const Monstey_U32 records=two_records?2u:1u,pool_bytes=two_records?28u:24u;
  const std::array<Monstey_U32,6> counts{1,records,2,3,pool_bytes,0};for(unsigned k=0;k<6;++k)m.Monstey_Write(ctx+4*k,counts[k]);
  const auto cats=root+20,recs=cats+16,attrs=recs+16*records,values=attrs+32,pool=values+12,end=pool+pool_bytes;
  m.Monstey_Write(ctx+20,end);m.Monstey_Write(cursor,cats);m.Monstey_Write(f+0x130,root);m.Monstey_Write(f+0x134,ctx);m.Monstey_Write(f+0x138,map);m.Monstey_Write(f+0x13C,root);
  m.Monstey_Write(map+4,head);m.Monstey_Write(head,n0);m.Monstey_Write(head+4,n0);m.Monstey_Write(head+8,n1);m.Monstey_Write(head+0x4D,1,1);
  const auto node=[&](Monstey_U32 at,Monstey_U32 parent,Monstey_U32 right,const std::string& key,const std::string& value){m.Monstey_Write(at,head);m.Monstey_Write(at+4,parent);m.Monstey_Write(at+8,right);m.Monstey_Write(at+0x4D,0,1);fixture.Monstey_Set(at+12,key);fixture.Monstey_Set(at+0x2C,value);m.Monstey_Write(at+0x28,3);m.Monstey_Write(at+0x48,0xABCDEF01);};
  node(n0,head,n1,"Cat/Rec/A","x;y;");node(n1,n0,head,two_records?"Cat/Rel/B":"Cat/Rec/B","z;");
  m.Monstey_Write(b+0x7000,b+0x7004);m.Monstey_Write(b+0x7004,0xFFFFFFFF);m.Monstey_Write(b+0x7010,b+0x7014);m.Monstey_Write(b+0x7020,0,1);
  auto imports=fixture.Monstey_Imports();imports.npos_pointer_cell=b+0x7000;imports.root_name_cell=b+0x7010;imports.empty_cstring=b+0x7020;
  imports.atoi_bits=[](auto memory,auto source){return Monstey_Token(Monstey_RefMemory(memory),source)+1u;};
  imports.strncpy=[](auto memory,auto destination,auto source,auto count){const Monstey_RefMemory copy(memory);bool padding=false;for(Monstey_U32 k=0;k<count;++k){const auto byte=padding?0u:copy.Monstey_Read(source+k,1);padding=padding||byte==0;copy.Monstey_Write(destination+k,byte,1);}};
  Monstey_Check(Monstey_FearUnpackedMaterialize(fixture.Monstey_Memory(),cursor,f,imports)==1,"whole materializer AL");
  Monstey_Check(m.Monstey_Read(root)==root&&m.Monstey_Read(root+4)==b+0x7014&&m.Monstey_Read(root+8)==1&&m.Monstey_Read(root+12)==cats&&m.Monstey_Read(root+16)==0,"whole root fields");
  Monstey_Check(m.Monstey_Read(cats+4)==root&&m.Monstey_Read(cats+8)==records&&m.Monstey_Read(cats+12)==recs,"whole category links");
  Monstey_Check(m.Monstey_Read(recs+4)==cats&&m.Monstey_Read(recs+8)==(two_records?1u:2u)&&m.Monstey_Read(recs+12)==attrs,"whole first record links");
  if(two_records)Monstey_Check(m.Monstey_Read(recs+16+4)==cats&&m.Monstey_Read(recs+16+8)==1&&m.Monstey_Read(recs+16+12)==attrs+16,"whole second record links");
  for(unsigned a=0;a<2;++a){const auto at=attrs+16*a;Monstey_Check(m.Monstey_Read(at+4)==recs,"native attribute parent is first category record, not current record");Monstey_Check(m.Monstey_Read(at+8,2)==(a?1u:2u)&&m.Monstey_Read(at+10,1)==3&&m.Monstey_Read(at+11,1)==0&&m.Monstey_Read(at+12)==values+4*a,"whole attribute count/type/usage/value association");}
  Monstey_Check(m.Monstey_Read(values)==1&&m.Monstey_Read(values+4)==3&&m.Monstey_Read(values+8)==0xA5A5A5A5,"native per-attribute4-byte cursor must retain multivalue overwrite");
  const std::string expected=two_records?std::string("A\0Rec\0B\0Rel\0Cat\0",16):std::string("A\0B\0Rec\0Cat\0",12);
  for(Monstey_U32 k=0;k<pool_bytes;++k)Monstey_Check(m.Monstey_Read(pool+k,1)==(k<expected.size()?static_cast<unsigned char>(expected[k]):0u),"whole name pool associations/padding");
  Monstey_Check(m.Monstey_Read(attrs)==pool&&m.Monstey_Read(attrs+16)==pool+(two_records?6u:2u)&&m.Monstey_Read(recs)==pool+(two_records?2u:4u)&&m.Monstey_Read(cats)==pool+(two_records?12u:8u),"name pointer associations");
  if(two_records)Monstey_Check(m.Monstey_Read(recs+16)==pool+8,"second record name");
  Monstey_Check(m.Monstey_Read(ctx+12)==6&&m.Monstey_Read(cursor)==end&&m.Monstey_Read(ctx+16)==pool_bytes,"whole live context/cursor");
 }
}
}
int main(int argc,char**argv){
 try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");Monstey_DeltaCases();Monstey_CompleteAssociations();std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"fault_cases\":"<<Monstey_Faults<<"}\n";std::cout<<"PASS ctor delta "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}
 catch(const std::exception& e){std::cerr<<"case="<<Monstey_Cases<<" "<<e.what()<<'\n';return 1;}catch(const Monstey_RefFault& f){std::cerr<<"Unexpected reference fixture fault case="<<Monstey_Cases<<" address="<<std::hex<<f.address<<'\n';return 1;}
}