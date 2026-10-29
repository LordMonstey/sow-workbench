#pragma once
#include "Monstey_AnonymousCorpus.hpp"
namespace Monstey_AnonymousDifferential {
using namespace Monstey_AnonymousCorpus;
std::uint64_t Monstey_Product(Monstey_GadbBinderMemory&,const Monstey_Arguments&,const Monstey_GadbPoolReadService&,std::vector<Monstey_AnonymousReference::Monstey_Access>*);
struct Monstey_Totals {std::uint64_t cases=0,checks=0,faults=0,bytes=0,events=0;};
inline Monstey_Totals Monstey_CompareAll(){
 Monstey_Totals totals;
 for(const auto& test:Monstey_Cases()){
  const auto reference=Monstey_Execute(test,Monstey_Reference),product=Monstey_Execute(test,Monstey_Product);
  const auto check=[&](bool good,const char* description){++totals.checks;if(!good)throw std::runtime_error(test.name+": "+description);};
  check(reference.bytes==product.bytes,"entire owned arena/frame differs");
  check(reference.result==product.result,"raw RAX differs");
  check(reference.fault==product.fault,"fault classification differs");
  check(reference.address==product.address&&reference.width==product.width&&reference.writing==product.writing&&reference.target==product.target,"fault identity/width/direction or boundary differs");
  check(reference.events==product.events,"primitive read events differ");check(reference.position==product.position,"primitive stream position differs");
  ++totals.cases;totals.faults+=reference.fault!=0;totals.bytes+=reference.bytes.size();totals.events+=reference.events.size();
 }
 return totals;
}
} // namespace Monstey_AnonymousDifferential
