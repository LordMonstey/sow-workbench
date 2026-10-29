#include "Monstey_FreeReference.hpp"
#include <array>
#include <bit>
#include <limits>
#include <utility>
namespace Monstey_FreeReview {
namespace C=Monstey;
namespace B=Monstey_BackendReview;
namespace L=Monstey_PlacementReview;
Monstey_BoundaryFault::Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport k,const C::Monstey_GadbManagerBackendImportSite&q,C::Monstey_GadbManagerFreeImportArguments a):std::runtime_error("independent free primitive binding"),kind(k),site(q),arguments(std::move(a)){}
Monstey_DivideFault::Monstey_DivideFault(Monstey_U64 s,Monstey_U64 b,Monstey_U64 v,Monstey_U64 d):std::runtime_error("independent free unsigned divide"),site(s),body(b),high(0),low(v),divisor(d){}
namespace {
using U=Monstey_U64;using W=Monstey_U32;
struct Monstey_Code {U free,region,trim,segments,find,link,enter,leave;};
constexpr std::array<Monstey_Code,3> Monstey_CodeTable{{
 {0x1404D6AC0,0x140C234B4,0x141175E78,0x141175BE8,0x140487600,0x14117559C,0x1404D6AEF,0x1404D6F69},
 {0x14081A514,0x140C233D8,0x141174770,0x141174240,0x1411744D0,0x1411739E4,0x14081A543,0x14081A992},
 {0x14081A06C,0x140C235AC,0x141177880,0x141177350,0x1411775E0,0x141176AF4,0x14081A09B,0x14081A4EA}}};
C::Monstey_GadbManagerBackendImportSite Monstey_Convert(B::Monstey_Site q){return {q.pc,q.call,q.body,q.cell,q.target};}
C::Monstey_GadbBinderMemory Monstey_View(Monstey_Image m){return {m.base,m.bytes};}
Monstey_Raw Monstey_Convert(C::Monstey_GadbBufferedResult q){return {q.rax,q.rdx};}
class Monstey_Engine {
 unsigned n_;Monstey_Image m_;const Monstey_Services&s_;Monstey_Bindings b_;Monstey_Code c_;U anchor_;
 U Monstey_R(U a,unsigned w=8){return m_.Read(a,w,anchor_);}
 void Monstey_W(U a,U v,unsigned w=8){m_.Write(a,v,w,anchor_);}
 U Monstey_And(U a,U v,unsigned w=8){const U q=Monstey_R(a,w)&v;Monstey_W(a,q,w);return q;}
 U Monstey_Or(U a,U v,unsigned w=8){const U q=Monstey_R(a,w)|v;Monstey_W(a,q,w);return q;}
 U Monstey_Add(U a,U v){const U q=Monstey_R(a)+v;Monstey_W(a,q);return q;}
 U Monstey_Sub(U a,U v){const U q=Monstey_R(a)-v;Monstey_W(a,q);return q;}
 C::Monstey_GadbManagerBackendImportSite Monstey_Site(U pc,U body,U cell){return {pc,pc,body,cell,m_.Read(cell,8,pc)};}
 const C::Monstey_GadbManagerBackendModeCells&Monstey_Mode()const{return n_==0?b_.backend.mode_40:(n_==1?b_.backend.mode_80:b_.backend.mode_c0);}
 Monstey_Raw Monstey_Sync(bool enter,U object,U frame){const auto pc=enter?c_.enter:c_.leave;const auto cell=enter?b_.backend.imports.enter_cs:b_.backend.imports.leave_cs;const C::Monstey_GadbManagerFreeSyncCall q{enter?C::Monstey_GadbManagerFreeSyncOperation::enter:C::Monstey_GadbManagerFreeSyncOperation::leave,Monstey_Site(pc,frame,cell),object+0x378};if(!s_.sync)throw Monstey_BoundaryFault(enter?C::Monstey_GadbManagerFreeImport::enter_cs:C::Monstey_GadbManagerFreeImport::leave_cs,q.site,q);return Monstey_Convert(s_.sync(Monstey_View(m_),q));}
 B::Monstey_Bindings Monstey_OldBindings()const {B::Monstey_Bindings out;const auto&i=b_.backend.imports;out.imports={i.fill,i.allocate,i.system_info,i.sleep,i.random,i.initialize_cs,i.enter_cs,i.leave_cs};const auto&p=b_.backend.placement;out.lower={p.tail_40,p.tail_80,p.tail_c0,p.order_table};const std::array<C::Monstey_GadbManagerBackendModeCells,3> a{b_.backend.mode_40,b_.backend.mode_80,b_.backend.mode_c0};for(unsigned k=0;k<3;++k){const auto&g=a[k];out.modes[k]={g.cookie,g.page,g.granularity,g.auxiliary,g.flags,g.secondary_flags,g.template_cs,g.state,g.main_cs};}return out;}
 Monstey_Raw Monstey_Init(U frame){
  B::Monstey_Services v;
  v.info=[this](B::Monstey_Image m,const B::Monstey_Info&q){const C::Monstey_GadbManagerBackendSystemInfoCall a{Monstey_Convert(q.site),q.output};if(!s_.system_info)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::system_info,a.site,a);s_.system_info(Monstey_View(m),a);};
  v.sleep=[this](B::Monstey_Image m,const B::Monstey_Sleep&q){const C::Monstey_GadbManagerBackendSleepCall a{Monstey_Convert(q.site),q.milliseconds,q.alertable};if(!s_.sleep)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::sleep,a.site,a);s_.sleep(Monstey_View(m),a);};
  v.random=[this](B::Monstey_Image m,const B::Monstey_Site&q){const auto a=Monstey_Convert(q);if(!s_.random)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::random,a,a);return s_.random(Monstey_View(m),a);};
  v.initialize=[this](B::Monstey_Image m,const B::Monstey_Cs&q){const C::Monstey_GadbManagerBackendInitializeCsCall a{Monstey_Convert(q.site),q.lock,q.spin};if(!s_.initialize_cs)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::initialize_cs,a.site,a);return s_.initialize_cs(Monstey_View(m),a).rdx;};
  v.enter=[this](B::Monstey_Image m,const B::Monstey_Sync&q){const C::Monstey_GadbManagerFreeSyncCall a{C::Monstey_GadbManagerFreeSyncOperation::enter,Monstey_Convert(q.site),q.lock};if(!s_.sync)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::enter_cs,a.site,a);static_cast<void>(s_.sync(Monstey_View(m),a));};
  v.leave=[this](B::Monstey_Image m,const B::Monstey_Sync&q){const C::Monstey_GadbManagerFreeSyncCall a{C::Monstey_GadbManagerFreeSyncOperation::leave,Monstey_Convert(q.site),q.lock};if(!s_.sync)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::leave_cs,a.site,a);return s_.sync(Monstey_View(m),a).rdx;};
  v.exchange=[this](B::Monstey_Image m,const B::Monstey_Exchange&q){if(!s_.exchange32)throw B::Monstey_ExchangeFault(q);return s_.exchange32(Monstey_View(m),{q.pc,q.address,q.value});};
  v.lower.cas=[this](L::Monstey_Image m,const L::Monstey_Call&q){if(!s_.placement.compare_exchange32)throw L::Monstey_Fault(L::Monstey_FaultKind::missing_cas,q.pc,q.address,4,false,q.comparand,q.desired);return s_.placement.compare_exchange32(Monstey_View(m),{q.pc,q.address,q.comparand,q.desired});};
  const auto b=Monstey_OldBindings();if(n_==0)return B::Monstey_RefInit40(m_,frame,v,b);if(n_==1)return B::Monstey_RefInit80(m_,frame,v,b);return B::Monstey_RefInitC0(m_,frame,v,b);
 }
 Monstey_Raw Monstey_Tail(U object,U start,U span){const auto p=Monstey_OldBindings().lower;L::Monstey_TailResult out;if(n_==0)out=L::Monstey_RefTail40(m_,object,start,span,p);else if(n_==1)out=L::Monstey_RefTail80(m_,object,start,span,p);else out=L::Monstey_RefTailC0(m_,object,start,span,p);return out.raw;}
 static W Monstey_Class(U size){const U x=size>>8;if(x==0)return 0;if(x>0xFFFF)return 31;const W k=31u-static_cast<W>(std::countl_zero(static_cast<W>(x)));return k*2u+static_cast<W>((size>>((k+7u)&63u))&1u);}
 void Monstey_SmallRemove(U object,U x,U size,Monstey_Raw&r,bool forty){const U forward=Monstey_R(x+0x10);r.a=forward;const U back=Monstey_R(x+0x18);if(forty)r.d=back;if(back==forward){W map=static_cast<W>(Monstey_R(object,4));map&=~(W{1}<<(static_cast<W>(size>>3)&31u));r.a=map;Monstey_W(object,map,4);}else{Monstey_W(forward+0x18,back);Monstey_W(back+0x10,forward);}}
 void Monstey_TreeRemove(U object,U x,Monstey_Raw&r,bool forty,bool rmw_zero){
  U replacement=Monstey_R(x+0x18);const U parent=Monstey_R(x+0x30);
  if(replacement!=x){const U forward=Monstey_R(x+0x10);r.a=forward;Monstey_W(forward+0x18,replacement);Monstey_W(replacement+0x10,forward);}
  else{U slot=x+0x28;replacement=Monstey_R(slot);if(replacement==0){slot=x+0x20;replacement=Monstey_R(slot);}if(replacement!=0){for(;;){U child=Monstey_R(replacement+0x28);r.a=replacement+0x28;if(forty)r.d=child;if(child!=0){slot=replacement+0x28;replacement=child;continue;}child=Monstey_R(replacement+0x20);r.a=replacement+0x20;if(forty)r.d=child;if(child!=0){slot=replacement+0x20;replacement=child;continue;}if(rmw_zero)static_cast<void>(Monstey_And(slot,0));else Monstey_W(slot,0);break;}}}
  if(parent==0)return;
  const W index=static_cast<W>(Monstey_R(x+0x38,4));r.a=index;const U cell=object+0x258+U{index}*8;
  if(x==Monstey_R(cell)){Monstey_W(cell,replacement);if(replacement==0){W map=static_cast<W>(Monstey_R(object+4,4));const W reread=static_cast<W>(Monstey_R(x+0x38,4));r.a=reread;map&=~(W{1}<<(reread&31u));Monstey_W(object+4,map,4);return;}}
  else{if(Monstey_R(parent+0x20)==x)Monstey_W(parent+0x20,replacement);else Monstey_W(parent+0x28,replacement);if(replacement==0)return;}
  Monstey_W(replacement+0x30,parent);U child=Monstey_R(x+0x20);r.a=child;if(child!=0){Monstey_W(replacement+0x20,child);Monstey_W(child+0x30,replacement);}child=Monstey_R(x+0x28);r.a=child;if(child!=0){Monstey_W(replacement+0x28,child);Monstey_W(child+0x30,replacement);}
 }
 void Monstey_SmallInsert(U object,U x,U size,Monstey_Raw&r){const U index=size>>3;r.d=index;W map=static_cast<W>(Monstey_R(object,4));const W mask=W{1}<<(static_cast<W>(index)&31u);const U head=object+(index*2+9)*8;U forward=head;r.a=head;if((map&mask)==0){map|=mask;Monstey_W(object,map,4);}else forward=Monstey_R(head+0x10);Monstey_W(head+0x10,x);Monstey_W(forward+0x18,x);Monstey_W(x+0x10,forward);Monstey_W(x+0x18,head);}
 void Monstey_TreeInsert(U object,U x,U size,Monstey_Raw&r,bool twins){
  const W index=Monstey_Class(size);Monstey_W(x+0x38,index,4);
  if(twins){static_cast<void>(Monstey_And(x+0x28,0));static_cast<void>(Monstey_And(x+0x20,0));}else{Monstey_W(x+0x28,0);Monstey_W(x+0x20,0);}
  W map=static_cast<W>(Monstey_R(object+4,4));r.a=map;const W mask=W{1}<<(index&31u);const U cell=object+0x258+U{index}*8;r.d=cell;
  if((map&mask)==0){map|=mask;r.a=map;Monstey_W(object+4,map,4);Monstey_W(cell,x);Monstey_W(x+0x30,cell);Monstey_W(x+0x18,x);Monstey_W(x+0x10,x);return;}
  U tree=Monstey_R(cell);r.d=tree;const W shift=index==31?0:57u-(index>>1);U key=size<<(shift&63u);
  for(;;){r.a=Monstey_R(tree+8)&~U{7};if(r.a==size){const U forward=Monstey_R(tree+0x10);r.a=forward;Monstey_W(forward+0x18,x);Monstey_W(tree+0x10,x);if(twins)static_cast<void>(Monstey_And(x+0x30,0));Monstey_W(x+0x10,forward);Monstey_W(x+0x18,tree);if(!twins)Monstey_W(x+0x30,0);return;}
   const U side=key>>63;key+=key;const U child=Monstey_R(tree+0x20+side*8);if(child==0){r.a=twins?side:0;Monstey_W(tree+0x20+side*8,x);Monstey_W(x+0x30,tree);Monstey_W(x+0x18,x);Monstey_W(x+0x10,x);return;}tree=child;r.d=tree;
  }
 }
 void Monstey_Header(U x,U size,Monstey_Raw&r){r.a=size|1;Monstey_W(x+8,r.a);Monstey_W(x+size,size);}
public:
 Monstey_Engine(unsigned n,Monstey_Image m,const Monstey_Services&s,Monstey_Bindings b):n_(n),m_(m),s_(s),b_(b),c_(Monstey_CodeTable.at(n)),anchor_(c_.free){}
 Monstey_Raw Monstey_Find(U object,U query){U node=object+0x3A0;for(;;){const U base=m_.Read(node,8,c_.find+7);if(query>=base){const U end=base+m_.Read(node+8,8,c_.find+(n_==0?0x13u:0xFu));if(query<end)return {node,query};}node=m_.Read(node+0x10,8,n_==0?0x1410E9048:c_.find+0x18);if(node==0)return {0,query};}}
 Monstey_Raw Monstey_Link(U object,U segment){const U base=m_.Read(segment,8,c_.link);U node=object+0x3A0;for(;;){if(node>=base){const U end=base+m_.Read(segment+8,8,c_.link+0xF);if(node<end)return {1,segment};}node=m_.Read(node+0x10,8,c_.link+0x1B);if(node==0)return {0,segment};}}
 Monstey_Raw Monstey_Region(U pointer,U remaining,U frame){anchor_=c_.region;if(pointer==0||remaining==0)return {0,remaining};for(;;){const C::Monstey_GadbManagerFreeQueryCall query{Monstey_Site(c_.region+0x28,frame,b_.query_cell),pointer,frame+0x20,0x30};if(!s_.query)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::query,query.site,query);const auto q=s_.query(Monstey_View(m_),query);if(q.rax==0){static_cast<void>(Monstey_And(0,0,4));return {0xFFFFFFFFu,q.rdx};}if(Monstey_R(frame+0x20)!=pointer||Monstey_R(frame+0x28)!=pointer||Monstey_R(frame+0x40,4)!=0x1000||Monstey_R(frame+0x38)>remaining)return {0xFFFFFFFFu,q.rdx};const C::Monstey_GadbManagerFreeReleaseCall release{Monstey_Site(c_.region+0x5D,frame,b_.release_cell),pointer,0,0x8000};if(!s_.release)throw Monstey_BoundaryFault(C::Monstey_GadbManagerFreeImport::release,release.site,release);const auto freed=s_.release(Monstey_View(m_),release);if(freed.eax==0){static_cast<void>(Monstey_And(0,0,4));return {0xFFFFFFFFu,freed.rdx};}pointer+=Monstey_R(frame+0x38);remaining-=Monstey_R(frame+0x38);if(remaining==0)return {0,freed.rdx};}}
 Monstey_Raw Monstey_Segments(U object,U frame,U incoming_d){anchor_=c_.segments;U predecessor=object+0x3A0;U segment=Monstey_R(predecessor+0x10);U released=0;Monstey_Raw r{0,incoming_d};while(segment!=0){const W flags=static_cast<W>(Monstey_R(segment+0x18,4));const U base=Monstey_R(segment);const U span=Monstey_R(segment+8);const U next=Monstey_R(segment+0x10);if((flags&9u)==1u){const U x=base+((U{0}-(base&7u))&7u);const U header=Monstey_R(x+8);const U size=header&~U{7};if((header&3)==1&&x+size>=base+span-0x48){if(x==Monstey_R(object+0x20)){Monstey_W(object+0x20,0);Monstey_W(object+8,0);}else Monstey_TreeRemove(object,x,r,false,false);r=Monstey_Region(base,span,frame-0x60);anchor_=c_.segments;if(static_cast<W>(r.a)==0){released+=span;Monstey_Sub(object+0x358,span);Monstey_W(predecessor+0x10,next);segment=predecessor;}else Monstey_TreeInsert(object,x,size,r,false);}}
  predecessor=segment;segment=next;
 }static_cast<void>(Monstey_Or(object+0x38,std::numeric_limits<U>::max()));return {released,r.d};}
 Monstey_Raw Monstey_Trim(U object,U frame,U incoming_d){anchor_=c_.trim;U d=incoming_d;if(Monstey_R(Monstey_Mode().cookie)==0)d=Monstey_Init(frame-0x60).d;anchor_=c_.trim;if(Monstey_R(object+0x28)==0)return {0,d};U released=0;const U top=Monstey_R(object+0x10);if(top>0x48){const U unit=Monstey_R(Monstey_Mode().granularity);const U dividend=unit-0x49+top;if(unit==0)throw Monstey_DivideFault(c_.trim+0x59,frame,dividend,unit);const U extra=dividend-(dividend%unit)-unit;const U query=Monstey_R(object+0x28);const U segment=Monstey_Find(object,query).a;d=query;anchor_=c_.trim;if((Monstey_R(segment+0x18,1)&8)==0&&(Monstey_R(segment+0x18,1)&1)!=0&&Monstey_R(segment+8)>=extra){const auto linked=Monstey_Link(object,segment);d=linked.d;if(static_cast<W>(linked.a)==0){const U address=Monstey_R(d)-extra+Monstey_R(d+8);const auto out=Monstey_Region(address,extra,frame-0x60);d=out.d;anchor_=c_.trim;if(static_cast<W>(out.a)==0){released=extra;if(extra!=0){Monstey_Sub(segment+8,extra);const U live_top_size=Monstey_R(object+0x10);Monstey_Sub(object+0x358,extra);const U live_top=Monstey_R(object+0x28);d=Monstey_Tail(object,live_top,live_top_size-extra).d;}}}}}
  const auto unused=Monstey_Segments(object,frame-0x50,d);anchor_=c_.trim;released+=unused.a;if(released==0){const U threshold=Monstey_R(object+0x30);if(Monstey_R(object+0x10)>threshold)static_cast<void>(Monstey_Or(object+0x30,std::numeric_limits<U>::max()));}return {released!=0?1u:0u,unused.d};}
 Monstey_Raw Monstey_Free(U object,U payload,U frame,U incoming_a){Monstey_Raw r{incoming_a,payload};if(payload==0)return r;anchor_=c_.free;U x=payload-0x10;if((Monstey_R(object+0x370,1)&2)!=0){r=Monstey_Sync(true,object,frame);anchor_=c_.free;}const U header=Monstey_R(x+8);U size=header&~U{7};const U next=x+size;if(n_!=0)r.d=next;else r.a=header;
  const auto finish=[this,object,frame](Monstey_Raw q){anchor_=c_.free;if((Monstey_R(object+0x370,1)&2)!=0)return Monstey_Sync(false,object,frame);return q;};
  const U pinuse=n_==0?(header&1):(Monstey_R(x+8,1)&1);
  if(pinuse==0){const U previous=Monstey_R(x);const U mapped=n_==0?(header&3):(Monstey_R(x+8,1)&3);if(mapped==0){const U total=previous+0x20+size;r=Monstey_Region(x-previous,total,frame-0x60);anchor_=c_.free;if(static_cast<W>(r.a)==0)Monstey_Sub(object+0x358,total);return finish(r);}size+=previous;x-=previous;if(x==Monstey_R(object+0x20)){if((Monstey_R(next+8,1)&3)==3){Monstey_W(object+8,size);static_cast<void>(Monstey_And(next+8,~U{1}));Monstey_Header(x,size,r);return finish(r);}}else if((previous>>3)<32)Monstey_SmallRemove(object,x,previous,r,n_==0);else Monstey_TreeRemove(object,x,r,n_==0,n_!=0);}
  const U following=Monstey_R(next+8);
  if((following&2)==0){if(next==Monstey_R(object+0x28)){Monstey_Add(object+0x10,size);const U total=Monstey_R(object+0x10);Monstey_W(object+0x28,x);r.a=total|1;Monstey_W(x+8,r.a);if(x==Monstey_R(object+0x20)){if(n_==0){Monstey_W(object+0x20,0);Monstey_W(object+8,0);}else{static_cast<void>(Monstey_And(object+0x20,0));static_cast<void>(Monstey_And(object+8,0));}}if(total>Monstey_R(object+0x30))r=Monstey_Trim(object,frame-0x30,r.d);return finish(r);}if(next==Monstey_R(object+0x20)){Monstey_Add(object+8,size);const U total=Monstey_R(object+8);Monstey_W(object+0x20,x);Monstey_Header(x,total,r);return finish(r);}const U added=following&~U{7};size+=added;if((added>>3)<32)Monstey_SmallRemove(object,next,added,r,n_==0);else Monstey_TreeRemove(object,next,r,n_==0,n_!=0);Monstey_Header(x,size,r);if(x==Monstey_R(object+0x20)){Monstey_W(object+8,size);return finish(r);}}
  else{Monstey_W(next+8,following&~U{1});Monstey_Header(x,size,r);}
  if((size>>3)<32)Monstey_SmallInsert(object,x,size,r);else{Monstey_TreeInsert(object,x,size,r,n_!=0);if(Monstey_Sub(object+0x38,1)==0)r=Monstey_Segments(object,frame-0x50,r.d);}return finish(r);
 }
 Monstey_Raw Monstey_Trap(U entry,Monstey_Raw raw,U r8,U r9){m_.Write(entry+0x10,raw.d,8,0x140819104);m_.Write(entry+0x18,r8,8,0x140819109);m_.Write(entry+0x20,r9,8,0x14081910E);m_.Write(0,0,4,0x140819113);return raw;}
 Monstey_Raw Monstey_Dispatch(U manager,U payload,const C::Monstey_GadbManagerFreeContext&q){anchor_=0x1401B08C4;const U object=Monstey_R(manager+0x10);if(payload==0)return {q.incoming_rax,0};const W bits=static_cast<W>(Monstey_R(object+0x370,4))&0xC0000000u;if(bits==0)return Monstey_Trap(q.body_rsp+0x28,{0,payload},0xC0000000u,q.incoming_r9);const unsigned mode=bits==0x40000000u?0u:(bits==0x80000000u?1u:2u);Monstey_Engine target(mode,m_,s_,b_);return target.Monstey_Free(object,payload,bits==0x40000000u?q.body_rsp-0x30:q.body_rsp,bits);}
};
}
Monstey_Raw Monstey_Run(unsigned n,Monstey_Image m,Monstey_Operation op,const Monstey_Request&q,const Monstey_Services&s,Monstey_Bindings b){Monstey_Engine e(n,m,s,b);switch(op){case Monstey_Operation::dispatch:{const auto&a=std::get<Monstey_Dispatch>(q);return e.Monstey_Dispatch(a.manager,a.payload,a.context);}case Monstey_Operation::thunk:{const auto&a=std::get<Monstey_Thunk>(q);return e.Monstey_Free(a.backend,a.payload,a.entry.entry_rsp-0x28,a.entry.incoming_rax);}case Monstey_Operation::backend:{const auto&a=std::get<Monstey_Backend>(q);return e.Monstey_Free(a.backend,a.payload,a.body.body_rsp,a.body.incoming_rax);}case Monstey_Operation::region:{const auto&a=std::get<Monstey_Region>(q);return e.Monstey_Region(a.address,a.length,a.frame);}case Monstey_Operation::trim:{const auto&a=std::get<Monstey_Trim>(q);return e.Monstey_Trim(a.backend,a.frame,a.incoming_d);}case Monstey_Operation::segments:{const auto&a=std::get<Monstey_Segments>(q);return e.Monstey_Segments(a.backend,a.frame,a.incoming_d);}case Monstey_Operation::find:{const auto&a=std::get<Monstey_Find>(q);return e.Monstey_Find(a.backend,a.address);}case Monstey_Operation::link:{const auto&a=std::get<Monstey_Link>(q);return e.Monstey_Link(a.backend,a.segment);}case Monstey_Operation::trap:{const auto&a=std::get<Monstey_Trap>(q);return e.Monstey_Trap(a.entry,a.incoming,a.r8,a.r9);}}throw std::runtime_error("independent entry discriminator");}
}
