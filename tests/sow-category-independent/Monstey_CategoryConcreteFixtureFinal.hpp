#pragma once
#include "Monstey_CategoryConcreteBindings.hpp"
#include "Monstey_GadbValueFixup.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>
namespace Monstey_CategoryReview {
struct Monstey_Event { unsigned kind; Monstey_U64 site,target,a,b,c; bool operator==(const Monstey_Event&) const=default; };
struct Monstey_PrimitiveFault : std::runtime_error { unsigned index; explicit Monstey_PrimitiveFault(unsigned n):std::runtime_error("synthetic primitive fault"),index(n){} };
struct Monstey_Case { unsigned profile=0,anon=0,named=0,payload=0; int faultAt=-1; bool afterFault=false; };
class Monstey_ComposedFixture {
public:
 Monstey_Memory memory{0x60000};
 Monstey_U64 base=0x142A58000ull,F=base+0x40000,outer=base+0x50000,outerVt=base+0x50100,innerVt=base+0x50200,selectorObject=base+0x50300,selectorVt=base+0x50400,bankTable=base+0x50800,cell=base+0x50C00,cursor=base+0x50E00,valueCursor=base+0x50E20,bank=base+0x51000,classObject=base+0x51800,buffer=base+0x52000,pool=base+0x24000,descriptors=base+0x28000,values=base+0x2C000,records=base+0x30000,category=base+0x20000,optionalLookup=0;
 Monstey::Monstey_GadbBufferedCells cells{base+0x50C10,base+0x50C18,base+0x50C20};
 Monstey::Monstey_GadbBankEpochWord global{0x142AA2B80,-100},g0{0x142AA2B68,-10},g1{0x142AA2B3C,-10},g2{0x142A58DF0,-10},g3{0x142AA2B44,-10},g4{0x142AA2B4C,-10};
 std::array<Monstey::Monstey_GadbBankEpochWord*,5> guards{&g0,&g1,&g2,&g3,&g4};
 std::int32_t epoch=-1;
 Monstey::Monstey_GadbBankSynchronizer synchronization{{true,true,true,true,0}};
 Monstey::Monstey_GadbNonnullPointerComparator oldComparator;
 std::array<Monstey::Monstey_GadbBankRecipe,6> recipes{{{0x14162DC24,0x142AA2B68,0x142A59EA0,2687,{}},{0x1416248B8,0x142AA2B3C,0x142A58C70,24,{}},{0x1404BAA24,0x142A58DF0,0x142A58E00,50,{}},{0x14161D674,0x142AA2B44,0x142A590E0,375,{}},{0x14161D594,0x142AA2B4C,0x142A59E10,15,{}},{0x14482A370,0,0x142A59E08,0,{}}}};
 Monstey::Monstey_GadbBankActivation activation;
 Monstey::Monstey_GadbBufferedServices buffered;
 Monstey::Monstey_GadbBankSelectorServices selector;
 Monstey_Case configuration;
 std::vector<Monstey_Event> events;
 std::vector<std::uint8_t> encoded;
 unsigned comparisons=0,copies=0,reads=0,fills=0;
 bool failureFile=false;
 explicit Monstey_ComposedFixture(Monstey_Case c):activation{{base,{}},recipes,guards,global,epoch,synchronization,bankTable,oldComparator},configuration(c){
  memory.base=base;activation.memory=Monstey_ConcreteView(memory);
  Put(outer,outerVt,8);Put(outerVt+8,0x141B50DE8ull,8);Put(outer+8,innerVt,8);Put(innerVt+0x18,0x141204C2Cull,8);Put(outer+0x10,0x9988,8);Put(outer+0x18,buffer,8);Put(outer+0x20,0,4);Put(outer+0x28,0,1);
  Put(selectorObject,selectorVt,8);Put(selectorVt,0x1404BA96Cull,8);Put(cell,0xC001,8);Put(cells.copy,0xC002,8);Put(cells.fill,0xC003,8);Put(cells.read_file,0xC004,8);
  Put(F+0x50,outer,8);Put(F+0x58,selectorObject,8);Put(F+0x60,0x12345678,4);Put(F+0x68,pool,8);Put(F+0x70,descriptors,8);Put(F+0x78,cursor,8);Put(F+0x80,valueCursor,8);Put(cursor,records,8);Put(valueCursor,values,8);
  Text(pool,"Ab");Text(pool+8,"Class");Text(pool+16,"Trait");Text(bank,"B4");Put(classObject+0x20,pool+8,8);
  for(unsigned i=0;i<6;++i){Text(bank+0x100+i*16,std::string("B")+char('0'+i));Put(bankTable+i*24,bank+0x100+i*16,8);Put(bankTable+i*24+8,recipes[i].enumerator,8);for(unsigned j=0;j<recipes[i].returned_count;++j)Put(recipes[i].returned_array+Monstey_U64(j)*8,classObject,8);}
  for(unsigned i=0;i<8;++i){Put(descriptors+i*16+0x0A,0,1);Put(descriptors+i*16+0x0C,0,2);Put(descriptors+i*16+0x0E,0,2);}
  auto a=c.anon,n=c.named;unsigned encodedA=a,encodedN=n;
  switch(c.profile){
   case 1:Text(bank,"missing");break;
   case 2:Text(bank,"B5");break;
   case 3:Text(pool+8,"NoClass");Text(bank+0x400,"Different");Put(classObject+0x20,bank+0x400,8);break;
   case 4:recipes[4].constructor_classes.assign(15,classObject);g4.Store(0);global.Store(std::numeric_limits<std::int32_t>::min());epoch=std::numeric_limits<std::int32_t>::min();break;
   case 5:optionalLookup=base+0x53000;Put(optionalLookup+0x48,1,4);Put(optionalLookup+0x50,base+0x53100,8);Put(base+0x53150,0x399,4);Put(base+0x53128,3,4);break;
   case 6:category=F-0x28;break;
   case 7:category=F-0x40;break;
   case 8:category=F+0x38;break;
   case 9:category=pool;break;
   case 10:Put(valueCursor,F+0x68,8);break;
   case 11:Put(valueCursor,F-0x0C,8);break;
   case 12:Put(valueCursor,F-8,8);break;
   case 13:Put(valueCursor,F+0x70,8);break;
   case 14:Put(valueCursor,F+0x78,8);break;
   case 15:Put(valueCursor,F+0x80,8);break;
   case 16:Put(valueCursor,F-0x30,8);break;
   case 17:Put(F+0x78,F+0x58,8);Put(selectorObject+32,selectorVt,8);break;
   case 18:Put(F+0x78,F+0x58,8);Put(selectorObject+32,selectorVt+32,8);Put(selectorVt+32,0x12345678,8);break;
   case 19:encodedA=1;a=0;break;
   case 20:encodedN=1;n=0;break;
   case 21:a=0x80000000u;encodedA=1;failureFile=true;break;
   case 22:n=0x40000000u;encodedN=1;failureFile=true;break;
   case 23:failureFile=true;break;
   case 24:failureFile=true;break;
   case 25:Put(outer+0x28,1,1);break;
   case 26:Text(pool,"");break;
   case 27:Put(outerVt+8,0xDEAD,8);break;
   case 28:Put(selectorVt,0xBEEF,8);break;
   case 29:break;
   case 31:Put(F+0x78,F-0x0C,8);optionalLookup=base+0x53000;Put(optionalLookup+0x48,1,4);Put(optionalLookup+0x50,base+0x53100,8);Put(base+0x53150,0x399,4);Put(base+0x53128,3,4);break;
   case 32:Put(valueCursor,F-0x0C,8);break;
   case 33:category=F+0x28;break;
   case 34:failureFile=true;break;
   case 30:optionalLookup=base+0x53000;Put(optionalLookup+0x48,1,4);Put(optionalLookup+0x50,base+0x53100,8);Put(base+0x53150,0x399,4);Put(base+0x53128,0xFFFFFFFFu,4);break;
   default:break;
  }
  Emit(c.profile==29?0xFFFFFFF0u:0,4);Emit(8,4);Emit(16,4);Emit(a,4);Emit(n,4);Emit(0xD00D1234,4);
  for(unsigned i=0;i<encodedA;++i){Emit(c.payload,2);Emit(0,1);Emit(0,4);Emit(0x35,1);for(unsigned j=0;j<c.payload;++j)Emit((c.profile>=10&&c.profile<=16)?0u:0x700u+i*4+j,4);}
  for(unsigned i=0;i<encodedN;++i){Emit(0,4);Emit(c.payload,2);Emit(0,1);Emit(0,4);Emit(0x72,1);for(unsigned j=0;j<c.payload;++j)Emit(c.profile==32?17u:(c.profile>=10&&c.profile<=16)?0u:0x900u+i*4+j,4);}
  for(std::size_t i=0;i<encoded.size();++i)Put(buffer+i,encoded[i],1);
  auto end=static_cast<Monstey_U32>(encoded.size());
  if(c.profile==21||c.profile==23)end=24+8;
  if(c.profile==22||c.profile==24)end=24+12;
  if(c.profile==25||c.profile==34)end=0;
  Put(outer+0x24,end,4);
  buffered.copy=[this](Monstey::Monstey_GadbBinderMemory m,const Monstey::Monstey_GadbBufferedCopyCall& x){
   const auto index=Event({0,x.native_site,x.target,x.destination,x.source,x.count});Before(index);auto dst=static_cast<std::size_t>(x.destination-m.base),src=static_cast<std::size_t>(x.source-m.base);if(x.destination<m.base||x.source<m.base||dst+x.count>m.bytes.size()||src+x.count>m.bytes.size())throw std::runtime_error("copy fixture unavailable");std::memmove(m.bytes.data()+dst,m.bytes.data()+src,static_cast<std::size_t>(x.count));++copies;After(index);return Monstey::Monstey_GadbBufferedResult{x.destination,0xC0A0000000000000ull+index};
  };
  buffered.fill=[this](Monstey::Monstey_GadbBinderMemory m,const Monstey::Monstey_GadbBufferedFillCall& x){const auto index=Event({1,x.native_site,x.target,x.destination,x.count,x.value});Before(index);for(Monstey_U64 i=0;i<x.count;++i)RawPut(m,x.destination+i,x.value,1);++fills;After(index);return Monstey::Monstey_GadbBufferedResult{x.destination,0xF111000000000000ull+index};};
  buffered.read_file=[this](Monstey::Monstey_GadbBinderMemory m,const Monstey::Monstey_GadbBufferedReadFileCall& x){const auto index=Event({2,x.native_site,x.target,x.destination,x.count,x.bytes_read_address});Before(index);++reads;RawPut(m,x.bytes_read_address,0,4);After(index);return Monstey::Monstey_GadbBufferedResult{failureFile?0ull:1ull,0xF11E000000000000ull+index};};
  selector.compare_nonnull=[this](Monstey::Monstey_GadbBinderMemory,const Monstey::Monstey_GadbBankSelectorCompareCall& x){const auto index=Event({3,x.native_site,x.target,x.left,x.right,x.slot});Before(index);++comparisons;const auto left=String(x.left),right=String(x.right);if(comparisons==1&&configuration.profile==19)Put(F-0x0C,1,4);if(comparisons==1&&configuration.profile==20)Put(F-8,1,4);After(index);return Monstey::Monstey_GadbBufferedResult{0xA1B2000000000000ull|(left==right?0ull:1ull),0xCCAA000000000000ull+index};};
  memory.ClearTrace();
 }
 static void RawPut(Monstey::Monstey_GadbBinderMemory m,Monstey_U64 a,Monstey_U64 v,unsigned n){if(a<m.base||a-m.base>m.bytes.size()||n>m.bytes.size()-static_cast<std::size_t>(a-m.base))throw std::runtime_error("primitive fixture range");for(unsigned i=0;i<n;++i)m.bytes[static_cast<std::size_t>(a-m.base)+i]=static_cast<std::uint8_t>(v>>(i*8u));}
 void Put(Monstey_U64 a,Monstey_U64 v,unsigned n){RawPut(activation.memory,a,v,n);}
 Monstey_U64 Get(Monstey_U64 a,unsigned n)const{const auto bytes=memory.Bytes();if(a<base||a-base+n>bytes.size())throw std::runtime_error("fixture get range");Monstey_U64 v=0;for(unsigned i=0;i<n;++i)v|=Monstey_U64(bytes[static_cast<std::size_t>(a-base)+i])<<(i*8u);return v;}
 void Text(Monstey_U64 a,const std::string& s){for(std::size_t i=0;i<s.size();++i)Put(a+i,static_cast<unsigned char>(s[i]),1);Put(a+s.size(),0,1);}
 std::string String(Monstey_U64 a)const{std::string result;for(unsigned i=0;i<128;++i){const auto c=Get(a+i,1);if(!c)return result;result.push_back(static_cast<char>(c>='A'&&c<='Z'?c+32:c));}throw std::runtime_error("fixture compare string budget");}
 void Emit(Monstey_U64 v,unsigned n){for(unsigned i=0;i<n;++i)encoded.push_back(static_cast<std::uint8_t>(v>>(i*8u)));}
 unsigned Event(Monstey_Event e){const auto n=static_cast<unsigned>(events.size());events.push_back(e);if(n>256)throw std::runtime_error("fixture primitive budget");return n;}
 void Before(unsigned n)const{if(configuration.faultAt==static_cast<int>(n)&&!configuration.afterFault)throw Monstey_PrimitiveFault(n);}
 void After(unsigned n)const{if(configuration.faultAt==static_cast<int>(n)&&configuration.afterFault)throw Monstey_PrimitiveFault(n);}
 Monstey_Machine Reference(){return Monstey_ConcreteCategoryReference(memory,activation,0x1122334455667788ull,bank,category,optionalLookup,F,selector,buffered,cells,cell);}
};
struct Monstey_Outcome{std::string kind;Monstey_U64 rax=0,rdx=0,address=0,other=0;bool operator==(const Monstey_Outcome&)const=default;};
template<class F> Monstey_Outcome Monstey_Observe(F&& f){try{const auto r=f();return {"return",r.rax,r.rdx,0,0};}
 catch(const Monstey_ReferenceMemoryFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey::Monstey_GadbBufferedMemoryFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey::Monstey_GadbPoolRelocationFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey::Monstey_GadbValueFixupFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey::Monstey_GadbCategoryLookupFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey::Monstey_GadbBankSelectorMemoryFault& e){return {"range",0,0,e.address,e.width};}
 catch(const Monstey_ReferenceDispatchFault& e){return {"dispatch",e.target,e.expected,e.site,e.slot};}
 catch(const Monstey_PrimitiveFault& e){return {"primitive",0,0,e.index,0};}
 catch(const std::exception& e){return {std::string("exception:")+e.what(),0,0,0,0};}}
inline std::vector<Monstey_Case> Monstey_Cases(){std::vector<Monstey_Case> c;for(unsigned a=0;a<=2;++a)for(unsigned n=0;n<=2;++n)c.push_back({0,a,n,0});for(unsigned p=1;p<=34;++p){unsigned a=1,n=1,payload=0;if(p>=10&&p<=16)payload=1;if(p==11||p==13||p==15||p==16)a=2;if(p==12){a=0;n=2;}if(p==19)a=0;if(p==20)n=0;if(p==21){a=1;n=0;payload=1;}if(p==22||p==24){a=0;n=1;payload=1;}if(p==23){a=1;n=0;payload=1;}if(p==30){a=0;n=1;}if(p==31){a=0;n=0;}if(p==32){a=0;n=1;payload=1;}c.push_back({p,a,n,payload});}return c;}
}
