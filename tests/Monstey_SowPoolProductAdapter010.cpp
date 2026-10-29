#include "Monstey_GadbPoolRelocation.hpp"
#include "Monstey_SowPoolDifferentialDriver008.hpp"
namespace Monstey_SowPoolDifferential {
std::uint64_t Monstey_RunProduct(Monstey_SowPoolMemory memory,const Monstey_Arguments& args,const Monstey_SowPoolRead& read){
    Monstey::Monstey_GadbBinderMemory current{memory.base,memory.bytes};
    Monstey::Monstey_GadbPoolReadService service([&](Monstey::Monstey_GadbBinderMemory& view,const Monstey::Monstey_GadbPoolReadCall& call){
        return read({view.base,view.bytes,nullptr},call.stream,call.target,call.destination,call.byte_count,call.native_site);
    });
    try{return Monstey::Monstey_GadbLoadPoolRelativeRecord(current,args.category,args.originalRdx,args.record,args.stream,args.frame,service);}
    catch(const Monstey::Monstey_GadbPoolRelocationFault& fault){throw Monstey_SowPoolFault(fault.address,fault.width,fault.writing);}
}
std::uint64_t Monstey_PredicateProduct(std::uint32_t type,std::uint64_t incoming){return Monstey::Monstey_GadbPoolRelativeTypeRaw(type,incoming);}
}
