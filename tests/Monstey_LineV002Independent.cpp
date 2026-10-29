#include "Monstey_LineV002Reference.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;using U8=std::uint8_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Dispatch=Monstey::Monstey_FearUnpackedLineDispatch;
U Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
void Need(bool v,const char* why){++Monstey_Checks;if(!v)throw std::runtime_error(why);}
struct Stop{};
struct Event {U callback,line,context,ordinal,kind;std::string bytes;bool operator==(const Event&)const=default;};
struct Result {U kind=0,value=0,address=0,width=0;bool writing=false;bool operator==(const Result&)const=default;};
struct Fixture;
struct Life {Fixture*owner;U action;~Life();};
struct Fixture {
 static constexpr U Base=0x1000,Args=0x1100,Scratch=0x1500,Source=0x1A00,Source2=0x2100;
 std::array<std::vector<U8>,2> images{std::vector<U8>(0x2400,0xCC),std::vector<U8>(0x2400,0xCC)};
 Mem memory{Base,images[0]};Dispatch dispatch;std::vector<Event>events;
 U active=0,mode=0,action=0,arguments=Args,scratch=Scratch,calls=0;bool testIdentity=true,finishing=false,runProduct=false;
 void Put(U at,U value,U width=4){Monstey_LineReference::Write(memory,at,value,width);}
 U Get(U at,U width=4)const{return Monstey_LineReference::Read(memory,at,width);}
 void Text(U at,const std::string& text){for(U n=0;n<text.size();++n)Put(at+n,static_cast<U8>(text[n]),1);}
 Fixture(const std::string& input,U selected):mode(selected){Put(Args,Source);Put(Args+4,static_cast<U>(input.size()));Put(Args+8,0xAABBCCDD);Put(Args+12,0x31415926);Text(Source,input);Text(Source2,"a\nZ\nq\n");Bind();}
 void Retire(U a){if(finishing)return;events.push_back({0,0,0,0,2,{}});if(a==1)Put(arguments+4,2);else if(a==2)Put(arguments,Source2);}
 ~Fixture(){finishing=true;dispatch={};}
 U Callback(Mem& view,U callback,U line,U context,U ordinal){
  if(testIdentity)Need(&view==&memory,"callback must observe same caller view object");
  std::string s;for(U n=0;n<1100;++n){const auto c=Monstey_LineReference::Read(view,line+n,1);if(c==0)break;s.push_back(static_cast<char>(c));}
  events.push_back({callback,line,context,ordinal,1,s});++calls;if(calls>16)throw Stop{};
  if(calls!=1)return (ordinal&1u)?0xF0F0F000u+ordinal:0x100u;
  switch(mode){
   case 0:break;
   case 1:images[1]=images[0];active=1;memory.bytes=images[1];Put(arguments,Source2);Put(arguments+4,4);Put(arguments+8,0x12345678);Put(arguments+12,0x90ABCDEF);Put(scratch-1,0,1);break;
   case 2:images[1]=images[0];active=1;memory.bytes=images[1];memory.base=Base+1;Put(arguments,Source2);Put(arguments+4,4);Put(arguments+8,0xFA11);Put(arguments+12,0xBEEF);Text(Source2,"a\nY\n");Put(scratch-1,0,1);break;
   case 3:images[1]=images[0];active=1;memory.bytes=std::span<U8>(images[1]).first(arguments+7-memory.base);break;
   case 4:images[1]=images[0];active=1;memory.bytes=images[1];Put(arguments,Source2);throw Stop{};
   case 5:dispatch=[](Mem&,U,U,U)->U{return 0x1234;};break;
   case 6:dispatch={};break;
   case 7:Put(arguments+4,2);break;
   case 8:Put(scratch-1,1,1);Put(arguments,Source2);break;
   case 9:Put(arguments+8,0);Put(arguments+12,0xFFFFFFFFu);break;
   case 10:memory.base=0xFFFFF000;memory.bytes=images[0];break;
   case 11:Put(arguments,Source2);Put(arguments+4,6);Put(scratch-1,0,1);break;
   case 12:Nested();break;
   case 13:dispatch=[owner=this,n=50](Mem& v,U c,U l,U x)mutable{return owner->Callback(v,c,l,x,++n);};Nested();break;
   case 14:dispatch={};Nested();break;
   default:throw std::runtime_error("unrecognized independent action");
  }
  return (mode&1u)?0xFFFFFFFFu:0x100u;
 }
 void Nested(){const U a=0x2500,src=0x2600,buffer=0x2800;Put(a,src);Put(a+4,2);Put(a+8,0x5500);Put(a+12,0x6600);Text(src,"N\n");const auto v=runProduct?Monstey::Monstey_FearUnpackedFrameLines(memory,a,buffer,dispatch):Monstey_LineReference::Run(memory,a,buffer,dispatch);events.push_back({0,0,0,v,3,{}});}
 void Bind(){if(action==0){dispatch=[this,n=0](Mem& m,U c,U l,U x)mutable{return Callback(m,c,l,x,++n);};return;}
  auto token=std::make_shared<Life>();token->owner=this;token->action=action;
  dispatch=[this,token,n=0](Mem& m,U c,U l,U x)mutable{auto* owner=this;const U ordinal=++n;return owner->Callback(m,c,l,x,ordinal);};
 }
 Result Run(bool product){runProduct=product;try{const auto v=product?Monstey::Monstey_FearUnpackedFrameLines(memory,arguments,scratch,dispatch):Monstey_LineReference::Run(memory,arguments,scratch,dispatch);return{0,v,0,0,false};}
  catch(const Monstey::Monstey_FearDbFault&e){return{1,0,e.address,e.width,e.writing};}
  catch(const Monstey::Monstey_FearUnpackedLineBoundaryFault&e){return{2,e.callback_identity,e.line_identity,e.context_identity,false};}
  catch(const Monstey_LineReference::Boundary&e){return{2,e.callback,e.line,e.context,false};}
  catch(const Stop&){return{3,0,0,0,false};}
 }
};
Life::~Life(){owner->Retire(action);}
void Compare(Fixture& actual,Fixture& expected){const auto x=actual.Run(true),y=expected.Run(false);Need(x==y,"native-derived outcome/fault prefix");Need(actual.events==expected.events,"callback identity/content/context/target-state/destructor sequence");Need(actual.images==expected.images,"both entire owned image side effects");Need(actual.memory.base==expected.memory.base&&actual.memory.bytes.size()==expected.memory.bytes.size()&&actual.active==expected.active,"propagated caller view");Need(actual.calls==expected.calls,"callback cardinality");if(x.kind!=0)++Monstey_Faults;++Monstey_Cases;}
void Matrix(){
 const std::array<std::string,12> inputs{"","a","a\n","a\nb\n","a\r\nb\r\n","a\rb\n",";comment\na\n",std::string("a\0b\n",4),"\r","\n\n",";;;\nq\n","a\n\rb\n"};
 for(const auto& text:inputs)for(U mode=0;mode<15;++mode){Fixture x(text,mode),y(text,mode);Compare(x,y);}
 for(U mode=0;mode<15;++mode)for(U action=1;action<=2;++action){Fixture x("a\nb\n",mode),y("a\nb\n",mode);x.action=action;y.action=action;x.Bind();y.Bind();Compare(x,y);}
 for(U n:{0u,1u,2u,15u,1022u,1023u,1024u,1025u,1100u})for(U mode=0;mode<15;++mode){std::string input(n,'x');input+="\nz\n";Fixture x(input,mode),y(input,mode);Compare(x,y);}
 for(U cut=0;cut<0x1200;cut+=7){Fixture x("a\nb\n",0),y("a\nb\n",0);x.memory.bytes=x.memory.bytes.first(cut);y.memory.bytes=y.memory.bytes.first(cut);Compare(x,y);}
 for(int delta=-16;delta<=1040;delta+=4){Fixture x("a\nb\n",0),y("a\nb\n",0);x.arguments=static_cast<U>(static_cast<int>(Fixture::Scratch)+delta);y.arguments=x.arguments;for(auto*f:{&x,&y}){f->Put(f->arguments,Fixture::Source);f->Put(f->arguments+4,4);f->Put(f->arguments+8,0xA001);f->Put(f->arguments+12,0xA002);}Compare(x,y);}
 for(U n=0;n<256;++n){const char c=static_cast<char>(n);std::string text;for(U k=0;k<3;++k){text.push_back(c);text.push_back('\n');}Fixture x(text,n%12),y(text,n%12);Compare(x,y);}
 {Fixture x("a\nb\n",0),y("a\nb\n",0);x.dispatch={};y.dispatch={};Compare(x,y);}
}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report path required");Matrix();std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"fault_cases\":"<<Monstey_Faults<<",\"original_instruction_bytes_interpreted\":false,\"native_original_entrypoint_invoked\":false}\n";std::cout<<"PASS "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
