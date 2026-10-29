#include "Monstey_CategoryConcreteBindings.hpp"
namespace Monstey_CategoryReview {
Monstey::Monstey_GadbBinderMemory Monstey_ConcreteView(Monstey_Memory& m){const auto bytes=m.Bytes();return {m.base,{const_cast<std::uint8_t*>(bytes.data()),bytes.size()}};}
Monstey_Machine Monstey_ConcreteHashBridge(Monstey_Memory& m,Monstey_U64 s,Monstey_U32 seed,Monstey_U64 rdx){const auto r=Monstey::Monstey_GadbHashCStringRegisters(Monstey_ConcreteView(m),s,seed,rdx);return {r.rax,r.rdx};}
Monstey_U64 Monstey_ConcreteLookupBridge(Monstey_Memory& m,Monstey_U64 o,Monstey_U64 k){return Monstey::Monstey_GadbFindCategoryByHash(Monstey_ConcreteView(m),o,k).rax;}
Monstey_ReferenceDispatchFault::Monstey_ReferenceDispatchFault(Monstey_U64 a,Monstey_U64 b,Monstey_U64 c,Monstey_U64 d):std::runtime_error("independent dispatch target"),site(a),slot(b),target(c),expected(d){}
Monstey_Machine Monstey_ConcreteCategoryReference(Monstey_Memory& m,Monstey::Monstey_GadbBankActivation& activation,Monstey_U64 parent,Monstey_U64 bank,Monstey_U64 category,Monstey_U64 lookup,Monstey_U64 F,const Monstey::Monstey_GadbBankSelectorServices& selector,const Monstey::Monstey_GadbBufferedServices& stream,Monstey::Monstey_GadbBufferedCells cells,Monstey_U64 compareCell){
 Monstey_CategoryDependencies dependencies;
 dependencies.read=[&](Monstey_Memory& memory,const Monstey_ReadCall& c){
  if(c.target!=0x141B50DE8ull)throw Monstey_ReferenceDispatchFault(0x1404BA0BCull,memory.Read(c.stream,8)+8,c.target,0x141B50DE8ull);
  const auto r=Monstey::Monstey_GadbBufferedRead(Monstey_ConcreteView(memory),c.stream,c.destination,static_cast<Monstey_U32>(c.count),F-0xA0,stream,cells);return Monstey_Machine{r.rax,r.rdx};
 };
 dependencies.selector=[&](Monstey_Memory& memory,const Monstey_SelectorCall& c){
  if(c.target!=0x1404BA96Cull)throw Monstey_ReferenceDispatchFault(0x1404BA10Bull,memory.Read(c.object,8),c.target,0x1404BA96Cull);
  const auto r=Monstey::Monstey_GadbFindParserBankClassRegisters(activation,c.bank,c.second,F-0xC0,selector,compareCell);return Monstey_Machine{r.rax,r.rdx};
 };
 dependencies.child=[&](Monstey_Memory& memory,const Monstey_ChildCall& c){
  const auto r=c.named?Monstey::Monstey_GadbLoadNamedRecordFromBuffered(Monstey_ConcreteView(memory),c.parent,c.originalRdx,c.record,c.stream,c.frame,stream,cells):Monstey::Monstey_GadbLoadAnonymousRecordFromBuffered(Monstey_ConcreteView(memory),c.parent,c.originalRdx,c.record,c.stream,c.frame,stream,cells);return Monstey_Machine{r.rax,r.rdx};
 };
 return Monstey_Category(m,parent,bank,category,lookup,F,dependencies);
}
}
