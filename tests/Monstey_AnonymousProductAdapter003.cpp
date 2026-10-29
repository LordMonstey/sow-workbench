#include "Monstey_GadbAnonymousRecord.hpp"
#include "Monstey_AnonymousDifferential.hpp"
namespace Monstey_AnonymousDifferential {
std::uint64_t Monstey_Product(Monstey_GadbBinderMemory& memory,const Monstey_Arguments& args,const Monstey_GadbPoolReadService& reader,std::vector<Monstey_AnonymousReference::Monstey_Access>*){
 return Monstey::Monstey_GadbLoadAnonymousRecord(memory,args.category,args.originalRdx,args.record,args.stream,args.frame,reader);
}
}
