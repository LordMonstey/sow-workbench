#include "Monstey_CallbackReference.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;using U8=std::uint8_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Op=Monstey::Monstey_FearUnpackedStringOp;using Call=Monstey::Monstey_FearUnpackedCallbackStringCall;
U Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
void Need(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
struct Stop{};
struct Event {U kind,op,target,receiver,a,b,c,site,ordinal;bool operator==(const Event&)const=default;};
struct Result{U kind=0,value=0,address=0,width=0;bool writing=false;bool operator==(const Result&)const=default;};
struct Fixture;
struct Life{Fixture*owner;~Life();};
struct Fixture{
 static constexpr U Base=0x10000000,DefaultFrame=Base+0x6000,DefaultC=Base+0x1000,DefaultP=Base+0x1100,Context=Base+0x1200,AltContext=Base+0x1220,AltMap=Base+0x1020,HeadC=Base+0x1400,HeadP=Base+0x1500,AltHead=Base+0x1480,NodeC=Base+0x1800,NodeP=Base+0x1900,NewNode=Base+0x1A00,AltNode=Base+0x1C00,Input=Base+0x2000,OtherKey=Base+0x2600,Key=Base+0x2500;
 std::array<std::vector<U8>,2>images{std::vector<U8>(0xD000,0xCC),std::vector<U8>(0xD000,0xCC)};
 Mem memory{Base,images[0]};Monstey::Monstey_FearUnpackedCallbackServices services;Monstey::Monstey_FearUnpackedLogicalStrings strings;
 std::vector<Event>events;U frame=DefaultFrame,cmap=DefaultC,pmap=DefaultP,heap=Base+0x8000,mode=0,active=0,calls=0,stop=0,entry=0,keyCell=Base+0x2700,outResult=Base+0x1380;bool finishing=false,parsedExists=false;
 void Put(U at,U v,U width=4){Monstey_CallbackReference::Write(memory,at,v,width);}U Get(U at,U width=4)const{return Monstey_CallbackReference::Read(memory,at,width);}
 void Text(U at,const std::string& v){for(U n=0;n<v.size();++n)Put(at+n,static_cast<U8>(v[n]),1);Put(at+static_cast<U>(v.size()),0,1);}
 void Str(U object,const std::string& value){Text(Base+0x3000,value);(void)strings.Invoke(memory,Op::construct_cstring,object,Base+0x3000);}
 void Node(U node,U head,bool parsed,const std::string& name,U type=3){Put(node,head);Put(node+4,head);Put(node+8,head);Str(node+0xC,name);Put(node+0x28,type);if(parsed){Str(node+0x2C,"old");Put(node+0x48,0xFACE1234);Put(node+0x4D,0,1);}else{Put(node+0x2C,0x11223344);Put(node+0x30,0x55667788);Put(node+0x35,0,1);}}
 void Map(U map,U head,U root,bool parsed){Put(map+4,head);Put(map+8,root?1u:0u);Put(head,root?root:head);Put(head+4,root?root:head);Put(head+8,root?root:head);Put(head+(parsed?0x4Du:0x35u),1,1);}
 Fixture(const std::string& input,U selected,bool existing=false,U chosenFrame=DefaultFrame,U chosenC=DefaultC,U chosenP=DefaultP,bool tree=false):frame(chosenFrame),cmap(chosenC),pmap(chosenP),mode(selected),parsedExists(existing){
  strings.allocate=[this](Mem,U bytes){const U p=heap;heap=(heap+bytes+15u)&~15u;if(heap<Base||heap>Base+0xC000)throw Stop{};return p;};
  Node(NodeC,HeadC,false,"Cat/Rec/A");Node(AltNode,HeadC,false,"Cat/Rec/A",7);Node(NodeP,HeadP,true,"Cat/Rec/A");Map(cmap,HeadC,NodeC,false);Map(pmap,HeadP,existing?NodeP:0,true);Map(AltMap,AltHead,0,false);
  if(tree){Node(Base+0x1D00,HeadC,false,"Cat/Rec/Q");Node(Base+0x1E00,HeadC,false,"Cat/Rec/Z");Put(HeadC+4,Base+0x1D00);Put(Base+0x1D00,NodeC);Put(Base+0x1D00+8,Base+0x1E00);Put(NodeC+4,Base+0x1D00);Put(Base+0x1E00+4,Base+0x1D00);}
  Str(Key,"Cat/Rec/A");Str(OtherKey,"ZZZ");Put(keyCell,Key);
  Text(Input,input);Put(Context,cmap);Put(Context+4,pmap);Put(AltContext,AltMap);Put(AltContext+4,pmap);Put(frame+0x94,Input);Put(frame+0x98,Context);Put(frame+0x8C,0xABCDEF01);Put(frame-0x4C,0x1234ABCD);
  for(U offset:{0x40u,0x44u,0x50u,0x54u,0x64u,0x6Cu,0x74u,0x78u})Put(Base+0xB000+offset,0xAA000000u+offset);Put(Base+0xB048,Base+0xB0F0);Put(Base+0xB0F0,0xFFFFFFFF);
  BindString(0,mode==9||mode==10);
  services.validate=[this,n=0u](Mem&view,U cv,U vs)mutable->std::uint8_t{Need(&view==&memory,"same mutable validator view");events.push_back({1,0,0,cv,vs,0,0,0x10005F8F,++n});++calls;if(stop&&calls==stop)throw Stop{};return mode==8?0u:1u;};
  services.insert=[this,n=0u](Mem&view,U map,U result,U hint,U pair)mutable->U{Need(&view==&memory,"same mutable insertion view");events.push_back({2,0,0,map,result,hint,pair,0x10005C84,++n});++calls;if(stop&&calls==stop)throw Stop{};const U node=mode==6?frame-0x2Cu:NewNode;
   if(mode!=6){Put(node,HeadP);Put(node+4,HeadP);Put(node+8,HeadP);(void)strings.Invoke(memory,Op::copy,node+0xC,pair);Put(node+0x28,Get(pair+0x1C));(void)strings.Invoke(memory,Op::copy,node+0x2C,pair+0x20);Put(node+0x48,Get(pair+0x3C));Put(node+0x4D,0,1);Put(HeadP,node);Put(HeadP+4,node);Put(HeadP+8,node);Put(pmap+8,1);}
   if(mode==15)Put(frame+0x6C,0x1199CCDD);Put(result,node);Put(outResult,mode==16?NodeP:node);return outResult;};
 }
 ~Fixture(){finishing=true;services.string={};services.validate={};services.insert={};strings.allocate={};}
 void Retire(){if(finishing)return;events.push_back({3,0,0,0,0,0,0,0,0});Put(frame+0x8C,0xD70A0001);}
 U String(Mem&view,Call c,U ordinal){Need(&view==&memory,"same caller-owned string view");events.push_back({0,static_cast<U>(c.op),c.target,c.receiver,c.a,c.b,c.c,c.native_site,ordinal});++calls;if(calls>100||stop&&calls==stop)throw Stop{};
  if(mode==6&&c.native_site==0x10005FD7)throw Stop{};
  if(mode==1&&c.native_site==0x10005F82){images[1]=images[0];active=1;memory.bytes=images[1];Put(NodeC+0x28,17);}
  if(mode==2&&c.native_site==0x10005FA0)Put(NodeC+0x28,0xFF000003);
  if(mode==3)Put(Base+0xB054,Get(Base+0xB054)+0x1000);
  if(mode==4&&c.native_site==0x10005ED6)Put(frame+0x98,AltContext);
  if(mode==9&&c.native_site==0x10005ED6)BindString(100,false);
  if(mode==10&&c.native_site==0x10005ED6)services.string={};
  if(mode==11&&c.native_site==0x10005F82){images[1]=images[0];active=1;memory.bytes=images[1];Put(frame+0x8C,0xEEFF0011);throw Stop{};}
  if(mode==12&&c.native_site==0x10005ED6)Put(Context,AltMap);
  U value=strings.Invoke(memory,c.op,c.receiver,c.a,c.b,c.c);if(c.op==Op::less)value=(value&255u)?0x101u:0x100u;
  if(mode==5&&c.native_site==0x10005F28)Put(frame+0x10,AltNode);
  if(mode==7&&c.native_site==0x10005FD7)Put(frame+0x8C,0x87654321);
  if(mode==13&&(c.native_site==0x10004065||c.native_site==0x10003FB5))Put(Base+0xB044,Get(Base+0xB044)+0x1000);
  if(mode==14&&(c.native_site==0x10004065||c.native_site==0x10003FB5))Put(entry==0?frame-4:keyCell,OtherKey);
  if(mode==17&&c.native_site==0x10005C8F)Put(outResult,NodeP);
  return value;
 }
 void BindString(U tag,bool track){std::shared_ptr<Life>token;if(track){token=std::make_shared<Life>();token->owner=this;}services.string=[this,token,n=tag](Mem&m,Call c)mutable{auto*owner=this;const U ordinal=++n;return owner->String(m,c,ordinal);};}
 Result Run(bool actual){try{U result=0;if(entry==0)result=actual?Monstey::Monstey_FearUnpackedLineCallback(memory,frame,frame+0x94,frame+0x98,services):Monstey_CallbackReference::Callback(memory,frame,frame+0x94,frame+0x98,services);else if(entry==3)result=actual?Monstey::Monstey_FearUnpackedCallbackParsedValue(memory,pmap,frame,services):Monstey_CallbackReference::ParsedValue(memory,pmap,frame,services);else{const bool parsed=entry==2;const U map=parsed?pmap:cmap;result=actual?Monstey::Monstey_FearUnpackedCallbackLowerBound(memory,map,keyCell,parsed,services):Monstey_CallbackReference::LowerBound(memory,map,keyCell,parsed,services);}return{0,result,0,0,false};}
  catch(const Monstey::Monstey_FearDbFault&e){return{1,0,e.address,e.width,e.writing};}
  catch(const Monstey::Monstey_FearUnpackedCallbackBoundaryFault&e){return{2,0,e.native_site,e.target,false};}
  catch(const Monstey_CallbackReference::Boundary&e){return{2,0,e.site,e.target,false};}
  catch(const Stop&){return{3,0,0,0,false};}
 }
};
Life::~Life(){owner->Retire();}
void Compare(Fixture&a,Fixture&e){const auto x=a.Run(true),y=e.Run(false);Need(x==y,"native-derived callback outcome or consumed fault");Need(a.events==e.events,"exact primitive call order/operands/targets/state/retirement");Need(a.images==e.images,"complete borrowed images after callback");Need(a.heap==e.heap&&a.memory.base==e.memory.base&&a.memory.bytes.size()==e.memory.bytes.size()&&a.active==e.active,"owned adapter and published view state");if(x.kind)++Monstey_Faults;++Monstey_Cases;}
void Matrix(){
 for(U mode=0;mode<18;++mode)for(bool existing:{false,true})for(bool tree:{false,true}){Fixture a("Cat/Rec/A=value",mode,existing,Fixture::DefaultFrame,Fixture::DefaultC,Fixture::DefaultP,tree),e("Cat/Rec/A=value",mode,existing,Fixture::DefaultFrame,Fixture::DefaultC,Fixture::DefaultP,tree);Compare(a,e);}
 for(const std::string input:{"","=","Cat/Rec/A","Cat/Rec/A=","Cat/Rec/A=a=b","Cat/Rec/Other=x","Cat/Rec/A =x","Other/Thing/A=x","Cat/Rec/A=long-text-value-with-more-than-fifteen-bytes"}){Fixture a(input,0),e(input,0);Compare(a,e);}
 for(U selected:{1u,2u,3u})for(U mode=0;mode<18;++mode)for(bool existing:{false,true}){Fixture a("unused",mode,existing),e("unused",mode,existing);for(auto*f:{&a,&e}){f->entry=selected;if(selected==3){f->Put(f->frame+0x74,Fixture::Key);f->Put(f->frame+0x2C,0x5A5A1234);}}Compare(a,e);}
 for(U at=1;at<=40;++at){Fixture a("Cat/Rec/A=value",0),e("Cat/Rec/A=value",0);a.stop=at;e.stop=at;Compare(a,e);}
 for(U cut=0;cut<0xD000;cut+=97){Fixture a("Cat/Rec/A=value",0),e("Cat/Rec/A=value",0);a.memory.bytes=a.memory.bytes.first(cut);e.memory.bytes=e.memory.bytes.first(cut);Compare(a,e);}
 for(U offset:{0u,4u,8u,0xCu,0x10u,0x14u,0x18u,0x6Cu,0x8Cu}){Fixture a("Cat/Rec/A=value",0,false,Fixture::DefaultFrame,Fixture::DefaultFrame+offset),e("Cat/Rec/A=value",0,false,Fixture::DefaultFrame,Fixture::DefaultFrame+offset);Compare(a,e);}
 {const U f=0x1000B054-0x6C;Fixture a("Cat/Rec/A=value",0,false,f),e("Cat/Rec/A=value",0,false,f);Compare(a,e);}
 for(U which=0;which<3;++which){Fixture a("Cat/Rec/A=value",0),e("Cat/Rec/A=value",0);for(auto*f:{&a,&e}){if(which==0)f->services.string={};else if(which==1)f->services.validate={};else f->services.insert={};}Compare(a,e);}
}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report path required");Matrix();std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"fault_cases\":"<<Monstey_Faults<<",\"original_instruction_bytes_interpreted\":false,\"native_original_entrypoint_invoked\":false}\n";std::cout<<"PASS "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
