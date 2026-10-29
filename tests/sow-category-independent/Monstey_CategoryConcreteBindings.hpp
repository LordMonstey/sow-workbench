#pragma once
#include "Monstey_CategoryReference.hpp"
#include "Monstey_GadbConcreteRecords.hpp"
#include "Monstey_GadbCategoryLookup.hpp"
#include "Monstey_GadbBankSelectorRegisters.hpp"
namespace Monstey_CategoryReview {
Monstey::Monstey_GadbBinderMemory Monstey_ConcreteView(Monstey_Memory&);
Monstey_Machine Monstey_ConcreteHashBridge(Monstey_Memory&,Monstey_U64,Monstey_U32,Monstey_U64);
Monstey_U64 Monstey_ConcreteLookupBridge(Monstey_Memory&,Monstey_U64,Monstey_U64);
struct Monstey_ReferenceDispatchFault : std::runtime_error {
 Monstey_U64 site,slot,target,expected;
 Monstey_ReferenceDispatchFault(Monstey_U64,Monstey_U64,Monstey_U64,Monstey_U64);
};
Monstey_Machine Monstey_ConcreteCategoryReference(Monstey_Memory&,Monstey::Monstey_GadbBankActivation&,Monstey_U64 parent,Monstey_U64 bank,Monstey_U64 category,Monstey_U64 lookup,Monstey_U64 frame,const Monstey::Monstey_GadbBankSelectorServices&,const Monstey::Monstey_GadbBufferedServices&,Monstey::Monstey_GadbBufferedCells,Monstey_U64 compareCell);
}
