#include "Monstey_PublicationFramedReference.hpp"
namespace Monstey_PublicationFramedReview {
Monstey_Fault::Monstey_Fault(Monstey_FaultKind k,Monstey_U64 p,Monstey_U64 a,unsigned w,bool write,Monstey_U64 s,Monstey_U64 t,Monstey_U64 c,Monstey_U64 m):std::runtime_error("independent publication effect fault"),kind(k),pc(p),address(a),slot(s),target(t),call_site(c),message(m),width(w),writing(write){}
void Monstey_Observer::Record(Monstey_Event e){if(events.size()==fail_before)throw Monstey_Fault(Monstey_FaultKind::probe,e.pc,e.address,e.width,e.kind==Monstey_EventKind::write);events.push_back(e);}
void Monstey_View::Event(Monstey_Event e)const{if(observer)observer->Record(e);}
Monstey_U64 Monstey_View::Load(Monstey_U64 a,unsigned w,Monstey_U64 pc)const{
 if(w==0||w>8||a<base||a>std::numeric_limits<Monstey_U64>::max()-(w-1)||a-base>bytes.size()||w>bytes.size()-static_cast<std::size_t>(a-base))throw Monstey_Fault(Monstey_FaultKind::range,pc,a,w);
 Monstey_U64 v=0;for(unsigned i=0;i<w;++i)v+=Monstey_U64(bytes[static_cast<std::size_t>(a-base)+i])<<(i*8u);
 Event({Monstey_EventKind::read,pc,a,v,0,w,{0,0}});return v;
}
void Monstey_View::Store(Monstey_U64 a,Monstey_U64 v,unsigned w,Monstey_U64 pc)const{
 if(w==0||w>8||a<base||a>std::numeric_limits<Monstey_U64>::max()-(w-1)||a-base>bytes.size()||w>bytes.size()-static_cast<std::size_t>(a-base))throw Monstey_Fault(Monstey_FaultKind::range,pc,a,w,true);
 const auto value=w==8?v:v&((Monstey_U64(1)<<(w*8u))-1u);Event({Monstey_EventKind::write,pc,a,value,0,w,{0,0}});
 for(unsigned i=0;i<w;++i)bytes[static_cast<std::size_t>(a-base)+i]=static_cast<std::uint8_t>(v>>(i*8u));
}
void Monstey_View::Zero(Monstey_U64 a,Monstey_U64 pc)const{const auto previous=Load(a,8,pc);Store(a,previous&0,8,pc);}
namespace {
Monstey_U32 Monstey_Atomic(Monstey_View v,Monstey_U64 pc,Monstey_U64 a,Monstey_U32 delta,Monstey_AtomicOp op,Monstey_Raw r,const Monstey_Services& s){
 v.Event({Monstey_EventKind::atomic,pc,a,delta,static_cast<Monstey_U64>(op),4,r});
 if(!s.atomic)throw Monstey_Fault(Monstey_FaultKind::missing_atomic,pc,a,4,false,0,0,pc);
 return s.atomic(v,{pc,a,delta,op});
}
Monstey_Raw Monstey_Virtual(Monstey_View v,Monstey_U64 pc,Monstey_U64 control,Monstey_U64 vtable,unsigned offset,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){
 const auto slot=vtable+offset;const auto target=v.Load(slot,8,pc);
 v.Event({Monstey_EventKind::virtual_call,pc,control,target,slot,8,r});
 if(!s.virtual_call)throw Monstey_Fault(Monstey_FaultKind::missing_virtual,pc,control,8,false,slot,target,pc);
 return s.virtual_call(v,{pc,control,vtable,slot,target,r,body_rsp});
}
[[noreturn]]void Monstey_Trap(Monstey_View v,Monstey_U64 callSite,Monstey_Raw r,const Monstey_Services& s){
 constexpr Monstey_U64 entry=0x140819104,store=0x140819113,message=0x141F65A38;
 v.Event({Monstey_EventKind::trap,store,0,0,callSite,4,r});
 if(!s.trap)throw Monstey_Fault(Monstey_FaultKind::invariant_trap,store,0,4,true,0,entry,callSite,message);
 s.trap(v,{callSite,entry,store,message,0,0,r});
 throw Monstey_Fault(Monstey_FaultKind::trap_returned,store,0,4,true,0,entry,callSite,message);
}
}
Monstey_Raw Monstey_RefSecondary(Monstey_View v,Monstey_U64 c,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){
 r.a=0xFFFFFFFFu;r.a=Monstey_Atomic(v,0x1403DD713,c+0x0C,0xFFFFFFFFu,Monstey_AtomicOp::fetch_add,r,s);
 if(r.a==1){r.a=v.Load(c,8,0x1403DD71D);r=Monstey_Virtual(v,0x1403DD720,c,r.a,8,body_rsp,r,s);}return r;
}
Monstey_Raw Monstey_RefPrimary(Monstey_View v,Monstey_U64 c,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){
 r.a=0xFFFFFFFFu;r.a=Monstey_Atomic(v,0x1403DD6E8,c+8,0xFFFFFFFFu,Monstey_AtomicOp::fetch_add,r,s);
 if(r.a==1){r.a=v.Load(c,8,0x1403DD6F8);r=Monstey_Virtual(v,0x1403DD6FB,c,r.a,0,body_rsp,r,s);r=Monstey_RefSecondary(v,c,body_rsp,r,s);}return r;
}
Monstey_Raw Monstey_RefReleasePair(Monstey_View v,Monstey_U64 p,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){const auto c=v.Load(p+8,8,0x14115E050);if(c)r=Monstey_RefPrimary(v,c,body_rsp-0x30,r,s);return r;}
Monstey_Raw Monstey_RefAssign(Monstey_View v,Monstey_U64 p,Monstey_U64 data,Monstey_U64 c,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){
 r.d=data;const auto old=v.Load(p+8,8,0x140761689);if(old)r=Monstey_RefPrimary(v,old,body_rsp-0x30,r,s);
 v.Store(p,data,8,0x140761695);v.Store(p+8,c,8,0x14076169D);return r;
}
Monstey_Raw Monstey_RefCopy(Monstey_View v,Monstey_U64 p,Monstey_U64 source,Monstey_U64 body_rsp,Monstey_Raw r,const Monstey_Services& s){
 r.d=source;v.Zero(p,0x1404BAE8E);v.Zero(p+8,0x1404BAE95);
 const auto c=v.Load(r.d+8,8,0x1404BAE9A);r.d=v.Load(r.d,8,0x1404BAE9E);
 if(c)(void)Monstey_Atomic(v,0x1404BAEA6,c+8,1,Monstey_AtomicOp::increment,r,s);
 r=Monstey_RefAssign(v,p,r.d,c,body_rsp-0x30,r,s);r.a=p;return r;
}
Monstey_Raw Monstey_RefPublish(Monstey_View v,Monstey_U64 db,Monstey_U32 h8,Monstey_U32 h9,Monstey_U64 bank,Monstey_U64 q,const Monstey_Services& s){
 Monstey_Raw r{v.Load(q+0x60,4,0x1404BAD06),h8};v.Store(db+0x48,r.a,4,0x1404BAD0D);
 r.a=v.Load(q+0x70,8,0x1404BAD10);v.Store(db+0x50,r.a,8,0x1404BAD15);
 r.a=v.Load(q+0x80,8,0x1404BAD19);v.Store(db+0x28,r.d,4,0x1404BAD21);
 r.d=v.Load(q+0x78,8,0x1404BAD24);v.Store(db+8,r.a,8,0x1404BAD29);v.Store(db,bank,8,0x1404BAD2D);v.Store(db+0x2C,h9,4,0x1404BAD30);
 r=Monstey_RefCopy(v,q+0x20,r.d,q-0x30,r,s);
 auto x=v.Load(db+0x60,8,0x1404BAD3E);r.d=v.Load(r.a+8,8,0x1404BAD42);v.Store(r.a+8,x,8,0x1404BAD46);v.Store(db+0x60,r.d,8,0x1404BAD4A);
 x=v.Load(db+0x58,8,0x1404BAD4E);r.d=v.Load(r.a,8,0x1404BAD52);v.Store(r.a,x,8,0x1404BAD55);v.Store(db+0x58,r.d,8,0x1404BAD5D);
 r=Monstey_RefReleasePair(v,q+0x20,q-0x30,r,s);
 r.a=v.Load(q+0x88,8,0x1404BAD66);const auto destinationOwner=db+0x20;v.Store(db+0x10,r.a,8,0x1404BAD72);
 r.a=v.Load(q+0x90,4,0x1404BAD76);v.Store(db+0x30,r.a,4,0x1404BAD7D);
 r.a=v.Load(q+0x98,4,0x1404BAD80);v.Store(db+0x34,r.a,4,0x1404BAD87);
 r.a=v.Load(q+0xA0,8,0x1404BAD8A);v.Store(db+0x40,r.a,8,0x1404BAD92);
 r.a=v.Load(q+0x68,4,0x1404BAD96);v.Store(db+0x4C,r.a,4,0x1404BAD9A);
 const auto sourceOwner=v.Load(q+0xA8,8,0x1404BAD9D);
 if(destinationOwner!=sourceOwner){r.d=v.Load(sourceOwner,8,0x1404BADAA);v.Zero(sourceOwner,0x1404BADAD);r.a=v.Load(destinationOwner,8,0x1404BADB1);v.Store(destinationOwner,r.d,8,0x1404BADB4);if(r.a)Monstey_Trap(v,0x1410ED7D7,r,s);}
 const auto incomingPair=v.Load(q+0x78,8,0x1404BADC0);r=Monstey_RefReleasePair(v,incomingPair,q-0x30,r,s);
 const auto invariant=v.Load(sourceOwner,8,0x1404BADCA);if(invariant)Monstey_Trap(v,0x1410ED7E9,r,s);return r;
}
}
