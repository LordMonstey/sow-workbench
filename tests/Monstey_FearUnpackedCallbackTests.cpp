#include "Monstey_FearUnpackedCallback.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
namespace {
using U=std::uint32_t;using B=std::uint8_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Op=Monstey::Monstey_FearUnpackedStringOp;using Call=Monstey::Monstey_FearUnpackedCallbackStringCall;
std::uint64_t Monstey_Groups=0,Monstey_Cases=0,Monstey_Checks=0,Monstey_Aborts=0;
void Need(bool p,const char*s){++Monstey_Checks;if(!p)throw std::runtime_error(s);}
struct Abort{};
struct Event {unsigned kind;Call call;U map=0,hidden=0,hint=0,pair=0;std::vector<B> before;};
struct Arena {
 static constexpr U base=0x10000000u,f=0x10020000u,context=0x10005000u,constraints=0x10005020u,parsed=0x10005040u,input=0x10006000u,key=0x10007000u,text=0x10007100u;
 std::vector<B> bytes=std::vector<B>(0x40000,0xA5),replacement;
 Mem memory{base,bytes};Monstey::Monstey_FearUnpackedCallbackLogicalMaps maps;Monstey::Monstey_FearUnpackedCallbackServices services;
 std::vector<Event> events;unsigned mode=0;std::size_t abort_at=SIZE_MAX;U heap=0x10028000u,redirect_node=0,redirect_cell=0x10005080u;bool capture=false;unsigned destructor_calls=0;
 Arena(){
  maps.allocate=[this](Mem,U n){const U p=heap;heap+=(n+3u)&~3u;Need(heap<=base+bytes.size(),"test arena exhausted");return p;};maps.strings.allocate=maps.allocate;
  for(U slot:{0x1000B040u,0x1000B044u,0x1000B050u,0x1000B054u,0x1000B064u,0x1000B06Cu,0x1000B074u,0x1000B078u})W(slot,0xE0000000u+(slot&65535u));W(0x1000B048u,0x1000B1F0u);W(0x1000B1F0u,UINT32_MAX);
  maps.Initialize(memory,constraints,false);maps.Initialize(memory,parsed,true);W(context,constraints);W(context+4,parsed);W(f+0x94,input);W(f+0x98,context);W(f+0x8C,0xD1E2F304);W(f-0x4C,0xE2F30415);
  Put("m",3);Put("a",2);Put("z",9);Line("m=hello");Bind();
 }
 U R(U p,unsigned n=4)const{if(p<memory.base||std::uint64_t(p)+n>0x100000000ull||std::uint64_t(p)-memory.base>memory.bytes.size()||n>memory.bytes.size()-static_cast<std::size_t>(p-memory.base))throw Monstey::Monstey_FearDbFault(p,n,false);U v=0;for(unsigned k=n;k;--k)v=v*256+memory.bytes[p-memory.base+k-1];return v;}
 void W(U p,U v,unsigned n=4){if(p<memory.base||std::uint64_t(p)+n>0x100000000ull||std::uint64_t(p)-memory.base>memory.bytes.size()||n>memory.bytes.size()-static_cast<std::size_t>(p-memory.base))throw Monstey::Monstey_FearDbFault(p,n,true);for(unsigned k=0;k<n;++k){memory.bytes[p-memory.base+k]=static_cast<B>(v);v>>=8;}}
 void Cstr(U at,const std::string&t){for(std::size_t k=0;k<t.size();++k)W(at+static_cast<U>(k),static_cast<B>(t[k]),1);W(at+static_cast<U>(t.size()),0,1);}
 void Line(const std::string&t){Cstr(input,t);}
 void Put(const std::string&t,U type){Cstr(text,t);maps.strings.Invoke(memory,Op::construct_cstring,key,text);maps.PutConstraint(memory,constraints,key,type,0x11223344u,0x55667788u);}
 std::string String(U object)const{const U n=R(object+0x14),p=R(object+0x18)>15?R(object+4):object+4;Need(n<10000,"test string size corrupt");std::string s;for(U k=0;k<n;++k)s.push_back(static_cast<char>(R(p+k,1)));return s;}
 U Node(U map,const std::string&t,bool isparsed)const{const U head=R(map+4);U p=R(head+4);const U nil=isparsed?0x4D:0x35;for(unsigned guard=0;guard<100;++guard){if(R(p+nil,1))return head;auto s=String(p+0x0C);if(s==t)return p;p=R(p+(t<s?0:8));}throw std::runtime_error("test malformed tree");}
 void Before(Event event){if(capture)event.before.assign(memory.bytes.begin(),memory.bytes.end());events.push_back(std::move(event));if(events.size()-1==abort_at)throw Abort{};}
 void Bind(){
  services.string=[this](Mem&m,Call c)->U{
   Need(m.bytes.data()==memory.bytes.data()&&m.base==memory.base,"service receives live view");Before({0,c,0,0,0,0,{}});
   U out=maps.strings.Invoke(m,c.op,c.receiver,c.a,c.b,c.c);
   if(mode==1&&c.native_site==0x10005ED6){const U alt=context+0x100;W(alt,constraints);W(alt+4,parsed);W(f+0x98,alt);W(context,0);W(context+4,0);W(f+0x94,input+0x100);Cstr(input+0x100,"missing=bad");}
   if(mode==2&&c.native_site==0x10005EF0){W(context,0);W(context+4,0);W(0x1000B050,0xA0500500);}
   if(mode==3&&c.native_site==0x10004065)W(0x1000B044,0xA0440440);
   if(mode==4&&c.native_site==0x10005F28)W(f+0x10,Node(constraints,"z",false));
   if(mode==5&&c.native_site==0x10005F28)W(constraints+4,R(f+0x10));
   if(mode==6&&c.native_site==0x10005FA0)W(Node(constraints,"m",false)+0x28,7);
   if(mode==7&&c.native_site==0x10005FB5){W(f+0x6C,8);Cstr(text,"z");maps.strings.Invoke(m,Op::construct_cstring,f+0x18,text);}
   if(mode==8&&c.native_site==0x10005FD7)W(f+0x8C,0xC0FEBABE);
   if(mode==10&&c.op==Op::destroy){++destructor_calls;W(0x1000B074,0xA0740000+destructor_calls);}
   if(mode==11&&c.native_site==0x10005EF0){replacement.assign(m.bytes.begin(),m.bytes.end());m={base,replacement};memory=m;}
   if(mode==12&&c.native_site==0x10005F28)W(f+0x10,Node(constraints,"a",false));
   if(mode==13&&c.native_site==0x10005FB5)W(f+0x8C,0x12345678);
   if(mode==14&&c.native_site==0x10005C69)W(f-0x4C,0x9ABCDEF0);
   if(mode==16&&c.op==Op::less)out|=0x100u;
   if(mode==15&&c.native_site==0x10005F04)out=0xDEADBEEF; // This return is ignored by native caller.
   return out;
  };
  services.validate=[this](Mem&m,U constraint,U value)->B{Before({1,{Op::construct,0,constraint,value,0,0,0x10005F8F},0,0,0,0,{}});if(mode==6){W(constraint,6);Cstr(text,"changed");maps.strings.Invoke(m,Op::construct_cstring,value,text);}return 0;};
  services.insert=[this](Mem&m,U map,U hidden,U hint,U pair)->U{Before({2,{Op::construct,0,0,0,0,0,0x10005C84},map,hidden,hint,pair,{}});const U out=maps.InsertParsed(m,map,hidden,hint,pair);if(mode==9){W(redirect_cell,redirect_node);return redirect_cell;}return out;};
 }
 B Run(){return Monstey::Monstey_FearUnpackedLineCallback(memory,f,f+0x94,f+0x98,services);}
 void CheckParsed(const std::string&keyText,const std::string&value,U type,U opaque){const U n=Node(parsed,keyText,true);Need(n!=R(parsed+4),"expected parsed key");Need(R(n+0x28)==type,"parsed type");Need(String(n+0x2C)==value,"parsed string content");Need(R(n+0x48)==opaque,"opaque word is explicit live input");}
};
void Basic(){++Monstey_Groups;
 for(const auto&line:std::array<std::string,9>{"m=hello","a=3.5","z=ref","m=","m","m==double","m=high\x7f","missing=bad","=empty"}){Arena a;a.Line(line);++Monstey_Cases;const auto selectedKey=line.substr(0,line.find('='));const bool exists=selectedKey=="m"||selectedKey=="a"||selectedKey=="z";const auto result=a.Run();Need(result==(exists?1:0),"missing/known callback AL");if(exists){const auto split=line.find('=');const auto key=line.substr(0,split);const auto value=split==std::string::npos?line:line.substr(split+1);a.CheckParsed(key,value,key=="a"?2:key=="z"?9:3,0xD1E2F304);Need(a.R(Arena::parsed+8)==1,"one parsed node");}else{Need(a.R(Arena::parsed+8)==0,"missing key creates no node");Need(a.events.size()>=2&&a.events[a.events.size()-2].call.native_site==0x10005F51&&a.events.back().call.native_site==0x10005F5B,"missing cleanup order");}}
 {Arena a;a.Line("m=first");Need(a.Run()==1,"first insert");const U node=a.Node(Arena::parsed,"m",true);a.W(Arena::f+0x8C,0x55667788);a.Line("m=second");a.events.clear();Need(a.Run()==1,"duplicate update");a.CheckParsed("m","second",3,0x55667788);Need(a.R(Arena::parsed+8)==1&&a.Node(Arena::parsed,"m",true)==node,"duplicate retains node identity");Need(std::none_of(a.events.begin(),a.events.end(),[](const auto&e){return e.kind==2;}),"existing key bypasses insertion");++Monstey_Cases;}
 {Arena a;a.Line(std::string("m=left\0=right",13));Need(a.Run()==1,"CString stops at NUL");a.CheckParsed("m","left",3,0xD1E2F304);++Monstey_Cases;}
 {Arena a;a.Put("",11);a.Line("=V");Need(a.Run()==1,"empty key supported");a.CheckParsed("","V",11,0xD1E2F304);++Monstey_Cases;}
 {Arena a;const std::string key(60,'K'),value(140,'V');a.Put(key,4);a.Line(key+"="+value);Need(a.Run()==1,"long heap-backed logical strings");a.CheckParsed(key,value,4,0xD1E2F304);++Monstey_Cases;}
}
void RawWordsAndLateFault(){++Monstey_Groups;
 for(U type:std::array<U,9>{0,1,2,3,4,9,0x100u,UINT32_MAX,0x80000000u})for(U opaque:std::array<U,3>{0,0xFFFFFFFFu,0x80FF007Fu}){Arena a;a.Put("m",type);a.W(Arena::f+0x8C,opaque);Need(a.Run()==1,"unrestricted native DWORD type and explicit scratch");a.CheckParsed("m","hello",type,opaque);++Monstey_Cases;}
 {Arena a;auto old=a.services.string;a.services.string=[&,old](Mem&m,Call c){const U result=old(m,c);if(c.native_site==0x10005FD7)m.bytes=m.bytes.first(0x2008E);return result;};bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==Arena::f+0x8C&&e.width==4&&!e.writing;}Need(caught,"opaque operand can fault after view rebinding");a.memory={Arena::base,a.bytes};const U node=a.Node(Arena::parsed,"m",true);Need(a.R(node+0x28)==3&&a.String(node+0x2C)=="hello","late opaque fault retains destination type/string");Need(a.events.back().call.native_site==0x10005FD7,"no cleanup on late operand fault");++Monstey_Cases;}
}
void TraceAndMutations(){++Monstey_Groups;
 {Arena a;Need(a.Run()==1,"baseline success");std::vector<U> sites;for(const auto&e:a.events)sites.push_back(e.call.native_site);const std::vector<U> expected{0x10005ED6,0x10005EF0,0x10005F04,0x10004065,0x10004065,0x10005F28,0x10005F82,0x10005F8F,0x10005FA0,0x10005FB5,0x10005C47,0x10005C52,0x10005C69,0x10005C84,0x10005C8F,0x10005C99,0x10005CA3,0x10005FD7,0x10005FEB,0x10005FF5,0x10005FFF,0x10006009};Need(sites==expected,"complete main and helper boundary sequence");const U f=Arena::f,g=f-0x78u,cm=a.Node(Arena::constraints,"m",false),ca=a.Node(Arena::constraints,"a",false),pm=a.Node(Arena::parsed,"m",true);const std::vector<U> receivers{f+0x34,f+0x34,f+0x34,cm+0x0C,ca+0x0C,f+0x18,f+0x34,cm+0x28,f+0x70,f+0x70,g+0x10,g+0x30,g+0x50,0,g+0x50,g+0x30,g+0x10,pm+0x2C,f+0x70,f+0x50,f+0x18,f+0x34};for(std::size_t k=0;k<receivers.size();++k)Need(a.events[k].call.receiver==receivers[k],"native boundary receiver and cleanup order");Need(a.R(Arena::f-0x78+0x6C)==0xE2F30415,"nested helper opaque copied without zero");++Monstey_Cases;}
 for(unsigned mode=1;mode<=16;++mode){Arena a;a.mode=mode;if(mode==3)a.Line("z=hello");if(mode==9){a.Line("a=old");a.mode=0;Need(a.Run()==1,"redirect setup");a.redirect_node=a.Node(Arena::parsed,"a",true);a.Line("m=hello");a.mode=9;a.events.clear();}const auto result=a.Run();++Monstey_Cases;
  if(mode==5){Need(result==0,"live head can make selected iterator missing");continue;}Need(result==1,"mutation callback normal completion");
  if(mode==4)a.CheckParsed("m","hello",9,0xD1E2F304);
  else if(mode==6)a.CheckParsed("m","changed",7,0xD1E2F304);
  else if(mode==7)a.CheckParsed("z","hello",8,0xD1E2F304);
  else if(mode==8)a.CheckParsed("m","hello",3,0xC0FEBABE);
  else if(mode==9)a.CheckParsed("a","hello",3,0xD1E2F304);
  else if(mode==12)a.CheckParsed("m","hello",2,0xD1E2F304);
  else if(mode==13)a.CheckParsed("m","hello",3,0x12345678);
  else a.CheckParsed(mode==3?"z":"m","hello",mode==3?9:3,0xD1E2F304);
  if(mode==2){for(const auto&e:a.events)if(e.call.op==Op::substr)Need(e.call.target==0xA0500500,"substr target read live");}
  if(mode==3){std::vector<U> targets;for(const auto&e:a.events)if(e.call.native_site==0x10004065)targets.push_back(e.call.target);Need(targets.size()>=2&&std::all_of(targets.begin(),targets.end(),[](U t){return t==0xE000B044;}),"lower_bound retains target across mutable provider state");const auto e=std::find_if(a.events.begin(),a.events.end(),[](const auto&e){return e.call.native_site==0x10005F28;});Need(e!=a.events.end()&&e->call.target==0xA0440440,"outer less reads new target");}
  if(mode==10){U target=0xE000B074;for(const auto&e:a.events)if(e.call.op==Op::destroy){Need(e.call.target==target,"destructor targets read per call");target=0xA0740000+(target==0xE000B074?1u:(target-0xA0740000)+1u);}}
  if(mode==11){Need(a.memory.bytes.data()==a.replacement.data(),"service can rebind view");Need(a.bytes[0x2006C]==0xA5,"old backing receives no later type store");}
  if(mode==14)Need(a.R(Arena::f-0x78+0x6C)==0x9ABCDEF0,"nested opaque read after copy effects");
 }
}
void RawAliases(){++Monstey_Groups;
 for(U destination:std::array<U,4>{Arena::f-4u,0x1000B054u,Arena::f+0x8Cu,Arena::f+0x14u}){
  Arena a;auto oldInsert=a.services.insert;a.services.insert=[&,oldInsert](Mem&m,U map,U hidden,U hint,U pair){(void)oldInsert(m,map,hidden,hint,pair);a.W(a.redirect_cell,destination-0x28u);return a.redirect_cell;};
  auto oldString=a.services.string;bool observed=false;U passed=0,target=0;
  a.services.string=[&,oldString](Mem&m,Call c){if(c.native_site==0x10005FD7){a.Before({0,c,0,0,0,0,{}});observed=true;passed=c.a;target=c.target;return 0u;}return oldString(m,c);};
  Need(a.Run()==1&&observed,"redirected parsed output aliases are consumed");Need(passed==(destination==Arena::f-4u?3u:Arena::f+0x70u),"destination type can overwrite outgoing assignment argument");Need(target==(destination==0x1000B054u?3u:0xE000B054u),"destination type can overwrite live IAT target");Need(a.R(destination+0x20u)==(destination==Arena::f+0x8Cu?3u:0xD1E2F304u),"opaque read occurs after aliased type/assignment");++Monstey_Cases;
 }
}
void FaultsAndPrefixes(){++Monstey_Groups;
 Arena baseline;baseline.capture=true;Need(baseline.Run()==1,"snapshot baseline");
 for(std::size_t n=0;n<baseline.events.size();++n){Arena a;a.abort_at=n;bool caught=false;try{(void)a.Run();}catch(const Abort&){caught=true;}Need(caught,"consumed service exception propagates");Need(a.events.size()==n+1,"no implicit exception cleanup");Need(a.bytes==baseline.events[n].before,"all earlier memory effects retained at consumed boundary");++Monstey_Cases;++Monstey_Aborts;}
 {Arena a;a.services.string={};bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedCallbackBoundaryFault&e){caught=e.native_site==0x10005ED6&&e.target==0xE000B06C;}Need(caught&&a.R(Arena::f-4)==Arena::input,"missing string faults after argument/target consumption");++Monstey_Cases;}
 {Arena a;a.services.validate={};bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedCallbackBoundaryFault&e){caught=e.native_site==0x10005F8F;}Need(caught&&a.R(Arena::f+0x6C)==0xA5A5A5A5,"missing validator has no eager type zero/cleanup");++Monstey_Cases;}
 {Arena a;a.services.insert={};bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedCallbackBoundaryFault&e){caught=e.native_site==0x10005C84;}Need(caught&&a.R(Arena::f-0x78+0x6C)==0xE2F30415,"missing insert retains complete pair and opaque prefix");Need(a.R(Arena::parsed+8)==0,"missing insert doesn't fabricate map node");++Monstey_Cases;}
 {Arena a;a.memory.bytes=a.memory.bytes.first(0x20096);bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==Arena::f+0x94&&e.width==4&&!e.writing;}Need(caught&&a.R(Arena::f-4)==0xA5A5A5A5,"argument DWORD fault before any write");++Monstey_Cases;}
 {Arena a;a.memory.bytes=a.memory.bytes.first(0x2009A);bool caught=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==Arena::f+0x98&&e.width==4&&!e.writing;}Need(caught&&a.String(Arena::f+0x34)=="m=hello","late context argument fault retains constructed line");++Monstey_Cases;}
 {Arena a;const U f=0x10002000,arg=0x10001000,ctx=0x10001004;a.W(arg,Arena::input);a.W(ctx,Arena::context);a.memory.bytes=a.memory.bytes.first(0xB06F);bool caught=false;try{(void)Monstey::Monstey_FearUnpackedLineCallback(a.memory,f,arg,ctx,a.services);}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==0x1000B06C&&e.width==4&&!e.writing;}Need(caught&&a.R(f-4)==Arena::input,"live IAT operand fault retains outgoing push");++Monstey_Cases;}
}
void LowerBounds(){++Monstey_Groups;
 for(bool parsed:{false,true}){Arena a;const U map=parsed?Arena::parsed:Arena::constraints;const U nil=parsed?0x4D:0x35;
  if(parsed){a.Line("m=V");Need(a.Run()==1,"parsed bound setup");a.Line("a=V");Need(a.Run()==1,"parsed left setup");a.Line("z=V");Need(a.Run()==1,"parsed right setup");}
  for(const auto&key:std::array<std::string,7>{"","a","b","m","n","z","zz"}){a.Cstr(Arena::text,key);a.maps.strings.Invoke(a.memory,Op::construct_cstring,Arena::key,Arena::text);a.W(Arena::f-4,Arena::key);const U n=Monstey::Monstey_FearUnpackedCallbackLowerBound(a.memory,map,Arena::f-4,parsed,a.services);const std::string expected=key<="a"?"a":key<="m"?"m":key<="z"?"z":"";Need(expected.empty()?n==a.R(map+4):a.String(n+0x0C)==expected,"lower_bound selected node");Need(a.R(n+nil,1)==(expected.empty()?1u:0u),"selected nil layout");++Monstey_Cases;}
 }
 {Arena a;const U empty=0x100050A0;a.maps.Initialize(a.memory,empty,false);a.services.string={};const U out=Monstey::Monstey_FearUnpackedCallbackLowerBound(a.memory,empty,0xFFFFFFFE,false,a.services);Need(out==a.R(empty+4),"empty map doesn't consume key slot or comparator");++Monstey_Cases;}
 {Arena a;a.Line("z=V");Need(a.Run()==1,"logical right setup");a.Line("a=V");Need(a.Run()==1,"logical left setup");const U head=a.R(Arena::parsed+4),m=a.Node(Arena::parsed,"m",true),lo=a.Node(Arena::parsed,"a",true),hi=a.Node(Arena::parsed,"z",true);Need(a.R(head)==lo&&a.R(head+8)==hi&&a.R(head+4)==hi,"logical head min/max/root");Need(a.R(lo+4)==hi&&a.R(hi)==lo&&m==head,"logical parent-child links");++Monstey_Cases;}
}
void CallableStateAndLifetime(){++Monstey_Groups;
 {Arena a;auto underlying=a.services.string;a.services.string=[underlying,n=0u](Mem&m,Call c)mutable{++n;if(c.op==Op::destroy&&n<10)throw std::runtime_error("value-captured state reset");return underlying(m,c);};Need(a.Run()==1,"mutable captured service state persists");++Monstey_Cases;}
 {Arena a;auto old=a.services.string;auto token=std::make_shared<int>(7);std::weak_ptr<int> weak=token;bool alive=false;unsigned replacements=0;a.services.string=[&a,old,token,weak,&alive,&replacements](Mem&m,Call c)mutable{auto next=old;auto observer=weak;auto*owner=&a;auto*flag=&alive;auto*count=&replacements;owner->services.string=next;*flag=!observer.expired();++*count;return next(m,c);};token.reset();Need(a.Run()==1&&alive&&weak.expired()&&replacements==1,"active string target retained through replacement");++Monstey_Cases;}
 {Arena a;auto token=std::make_shared<int>(8);std::weak_ptr<int> weak=token;bool alive=false;a.services.validate=[&a,token,weak,&alive](Mem&,U,U)->B{auto observer=weak;auto*owner=&a;auto*flag=&alive;owner->services.validate={};*flag=!observer.expired();return 0;};token.reset();Need(a.Run()==1&&alive&&weak.expired(),"validator target self-clear lifetime and ignored AL");++Monstey_Cases;}
 {Arena a;auto old=a.services.insert;auto token=std::make_shared<int>(9);std::weak_ptr<int> weak=token;bool alive=false;a.services.insert=[&a,old,token,weak,&alive](Mem&m,U map,U hidden,U hint,U pair){auto next=old;auto observer=weak;auto*owner=&a;auto*flag=&alive;owner->services.insert=next;*flag=!observer.expired();return next(m,map,hidden,hint,pair);};token.reset();Need(a.Run()==1&&alive&&weak.expired(),"insert target self-replacement lifetime");++Monstey_Cases;}
 {Arena a;auto token=std::make_shared<int>(10);std::weak_ptr<int> weak=token;bool alive=false;a.services.validate=[&a,token,weak,&alive](Mem&,U,U)->B{auto observer=weak;auto*owner=&a;auto*flag=&alive;owner->services.validate={};*flag=!observer.expired();throw Abort{};};token.reset();bool caught=false;try{(void)a.Run();}catch(const Abort&){caught=true;}Need(caught&&alive&&weak.expired(),"active target lifetime through throw");++Monstey_Cases;}
 {Arena a;auto old=a.maps.allocate;auto token=std::make_shared<int>(11);std::weak_ptr<int> weak=token;bool alive=false;a.maps.allocate=[&a,old,token,weak,&alive](Mem m,U n){auto next=old;auto observer=weak;auto*owner=&a;auto*flag=&alive;owner->maps.allocate=next;*flag=!observer.expired();return next(m,n);};token.reset();Need(a.Run()==1&&alive&&weak.expired(),"logical map allocator self-replacement lifetime");++Monstey_Cases;}
}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new report path required");Basic();RawWordsAndLateFault();TraceAndMutations();RawAliases();FaultsAndPrefixes();LowerBounds();CallableStateAndLifetime();std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("report open");out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"boundary_abort_prefix_cases\":"<<Monstey_Aborts<<",\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false,\"original_physical_ABI_qualified\":false}\n";std::cout<<"PASS "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Abort&){std::cerr<<"unexpected consumed callback abort\n";return 1;}}
