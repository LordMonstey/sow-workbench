#include "Monstey_ManagerPrimitiveReference.hpp"
#include <limits>
namespace Monstey_ManagerPrimitiveReview {
Monstey_Fault::Monstey_Fault(Monstey_FaultKind k,Monstey_U64 p,Monstey_U64 at,unsigned w,bool wr,Monstey_U64 s,Monstey_U64 t,Monstey_U64 a):std::runtime_error("independent manager effect fault"),kind(k),pc(p),address(at),slot(s),target(t),argument(a),width(w),writing(wr){}
Monstey_U64 Monstey_Image::Read(Monstey_U64 at,unsigned n,Monstey_U64 pc)const{
 if(n==0||n>8||at<base||at>std::numeric_limits<Monstey_U64>::max()-(n-1)||at-base>bytes.size()||n>bytes.size()-static_cast<std::size_t>(at-base))throw Monstey_Fault(Monstey_FaultKind::memory,pc,at,n);
 Monstey_U64 v=0;for(unsigned i=0;i<n;++i)v|=Monstey_U64(bytes[static_cast<std::size_t>(at-base)+i])<<(8u*i);
 if(events)events->push_back({Monstey_EventKind::read,pc,at,v,n});return v;
}
void Monstey_Image::Write(Monstey_U64 at,Monstey_U64 v,unsigned n,Monstey_U64 pc)const{
 if(n==0||n>8||at<base||at>std::numeric_limits<Monstey_U64>::max()-(n-1)||at-base>bytes.size()||n>bytes.size()-static_cast<std::size_t>(at-base))throw Monstey_Fault(Monstey_FaultKind::memory,pc,at,n,true);
 if(events)events->push_back({Monstey_EventKind::write,pc,at,v,n});for(unsigned i=0;i<n;++i)bytes[static_cast<std::size_t>(at-base)+i]=static_cast<std::uint8_t>(v>>(8u*i));
}
namespace {
Monstey_Raw Monstey_Import(Monstey_Image m,Monstey_Role role,Monstey_U64 slot,Monstey_U64 arg,Monstey_Raw r,const Monstey_Services& s){
 const Monstey_U64 pc=role==Monstey_Role::enter?0x14114BA9F:0x14114BA88;const auto target=m.Read(slot,8,pc);
 if(!s.sync)throw Monstey_Fault(Monstey_FaultKind::missing_sync,pc,slot,8,false,slot,target,arg);
 if(m.events)m.events->push_back({Monstey_EventKind::sync,pc,slot,target,8});
 return s.sync(m,{role,pc,slot,target,arg,r});
}
Monstey_U64 Monstey_Array(Monstey_Image m,const Monstey_Bindings& b,Monstey_U64 pc){if(!b.gs)throw Monstey_Fault(Monstey_FaultKind::missing_gs,pc,0x58,8);return m.Read(*b.gs,8,pc);}
}
Monstey_Raw Monstey_RefConstruct(Monstey_Image m,Monstey_U64 g,Monstey_U64 lock,Monstey_Raw r,const Monstey_Bindings& b,const Monstey_Services& s){r.d=lock;m.Write(g,lock,8,0x14114BA99);r=Monstey_Import(m,Monstey_Role::enter,b.enter,lock,r,s);r.a=g;return r;}
Monstey_Raw Monstey_RefLeave(Monstey_Image m,Monstey_U64 lock,Monstey_Raw r,const Monstey_Bindings& b,const Monstey_Services& s){return Monstey_Import(m,Monstey_Role::leave,b.leave,lock,r,s);}
Monstey_Raw Monstey_RefDestroy(Monstey_Image m,Monstey_U64 g,Monstey_Raw r,const Monstey_Bindings& b,const Monstey_Services& s){const auto lock=m.Read(g,8,0x14017DE90);return Monstey_RefLeave(m,lock,r,b,s);}
Monstey_Raw Monstey_RefCold(Monstey_Image m,Monstey_Raw r){const auto prior=m.Read(0,4,0x1411224A4);m.Write(0,prior&0u,4,0x1411224A4);return r;}
Monstey_Raw Monstey_RefIncrement(Monstey_Image m,const Monstey_Bindings& b){const auto array=Monstey_Array(m,b,0x1406AAFDC);const auto tls=m.Read(array,8,0x1406AAFEA);const auto at=tls+0x2C;const auto count=static_cast<Monstey_U32>(m.Read(at,4,0x1406AAFED));m.Write(at,Monstey_U32(count+1u),4,0x1406AAFED);return {array,0x28};}
Monstey_Raw Monstey_RefDecrement(Monstey_Image m,const Monstey_Bindings& b){const auto array=Monstey_Array(m,b,0x1406AACFC);const auto tls=m.Read(array,8,0x1406AAD0A);const auto at=tls+0x2C;const auto count=static_cast<Monstey_U32>(m.Read(at,4,0x1406AAD0D));if(count==0)return Monstey_RefCold(m,{0,0x28});const Monstey_U32 next=count-1u;m.Write(at,next,4,0x1406AAD1B);return {next,0x28};}
}
