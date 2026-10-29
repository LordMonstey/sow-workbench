#include "Monstey_GadbManagerFree.hpp"
#include <bit>
#include <limits>
#include <utility>
namespace Monstey {
Monstey_GadbManagerFreeImportFault::Monstey_GadbManagerFreeImportFault(
    Monstey_GadbManagerFreeImport k,const Monstey_GadbManagerBackendImportSite &s,
    Monstey_GadbManagerFreeImportArguments a)
  :std::runtime_error("Unprovided SoW Free import primitive"),kind(k),site(s),arguments(std::move(a)){}
Monstey_GadbManagerFreeDivideFault::Monstey_GadbManagerFreeDivideFault(
    std::uint64_t s,std::uint64_t f,std::uint64_t low,std::uint64_t div)
  :std::runtime_error("SoW Free unsigned DIV divisor zero"),native_site(s),body_rsp(f),dividend_low(low),divisor(div){}
namespace {
using Monstey_U64=std::uint64_t;
using Monstey_U32=std::uint32_t;
using Monstey_Raw=Monstey_GadbBufferedResult;
using Monstey_Services=Monstey_GadbManagerFreeServices;
using Monstey_Bindings=Monstey_GadbManagerFreeBindings;
struct Monstey_FreeImage {
  Monstey_GadbBinderMemory m;
  std::size_t Monstey_Offset(Monstey_U64 a,std::size_t n,bool w) const {
    if(a<m.base || a>std::numeric_limits<Monstey_U64>::max()-(n-1) ||
       a-m.base>m.bytes.size() || n>m.bytes.size()-static_cast<std::size_t>(a-m.base))
      throw Monstey_GadbBufferedMemoryFault(a,n,w);
    return static_cast<std::size_t>(a-m.base);
  }
  Monstey_U64 Monstey_Read(Monstey_U64 a,std::size_t n=8) const {
    const auto o=Monstey_Offset(a,n,false);Monstey_U64 v=0;
    for(std::size_t i=0;i<n;++i)v|=Monstey_U64(m.bytes[o+i])<<(8*i);
    return v;
  }
  void Monstey_Write(Monstey_U64 a,Monstey_U64 v,std::size_t n=8) const {
    const auto o=Monstey_Offset(a,n,true);
    for(std::size_t i=0;i<n;++i)m.bytes[o+i]=static_cast<std::uint8_t>(v>>(8*i));
  }
  void Monstey_ZeroRmw(Monstey_U64 a,std::size_t n=8) const {
    static_cast<void>(Monstey_Read(a,n));Monstey_Write(a,0,n);
  }
  Monstey_U64 Monstey_Sub(Monstey_U64 a,Monstey_U64 v) const {
    const auto r=Monstey_Read(a)-v;Monstey_Write(a,r);return r;
  }
  void Monstey_Add(Monstey_U64 a,Monstey_U64 v) const {const auto r=Monstey_Read(a)+v;Monstey_Write(a,r);}
  void Monstey_And(Monstey_U64 a,Monstey_U64 mask) const {const auto r=Monstey_Read(a)&mask;Monstey_Write(a,r);}
  void Monstey_MaxRmw(Monstey_U64 a) const {const auto r=Monstey_Read(a)|~Monstey_U64{0};Monstey_Write(a,r);}
};
Monstey_GadbManagerBackendImportSite Monstey_FreeSite(const Monstey_FreeImage &m,
    Monstey_U64 pc,Monstey_U64 f,Monstey_U64 cell){return {pc,pc,f,cell,m.Monstey_Read(cell)};}
Monstey_Raw Monstey_FreeSync(const Monstey_FreeImage &m,const Monstey_Services &s,
    Monstey_GadbManagerFreeSyncOperation op,Monstey_U64 pc,Monstey_U64 f,Monstey_U64 cell,Monstey_U64 cs){
  const Monstey_GadbManagerFreeSyncCall c{op,Monstey_FreeSite(m,pc,f,cell),cs};
  if(!s.sync)throw Monstey_GadbManagerFreeImportFault(op==Monstey_GadbManagerFreeSyncOperation::enter?
      Monstey_GadbManagerFreeImport::enter_cs:Monstey_GadbManagerFreeImport::leave_cs,c.site,c);
  return s.sync(m.m,c);
}
// Reference-only delegates. Every primitive resolves the current Free holder
// inside the actual BK call; no provider or import target is snapshotted here.
Monstey_GadbManagerBackendServices Monstey_FreeBackendView(const Monstey_Services &s){
  Monstey_GadbManagerBackendServices b;
  b.system_info=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendSystemInfoCall &c){
    if(!s.system_info)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::system_info,c.site,c);
    s.system_info(m,c);
  };
  b.sleep=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendSleepCall &c){
    if(!s.sleep)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::sleep,c.site,c);
    s.sleep(m,c);
  };
  b.random=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendImportSite &c){
    if(!s.random)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::random,c,c);
    return s.random(m,c);
  };
  b.initialize_cs=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendInitializeCsCall &c){
    if(!s.initialize_cs)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::initialize_cs,c.site,c);
    return s.initialize_cs(m,c);
  };
  b.enter_cs=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendSyncCall &c){
    const Monstey_GadbManagerFreeSyncCall f{Monstey_GadbManagerFreeSyncOperation::enter,c.site,c.critical_section};
    if(!s.sync)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::enter_cs,c.site,f);
    static_cast<void>(s.sync(m,f));
  };
  b.leave_cs=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendSyncCall &c){
    const Monstey_GadbManagerFreeSyncCall f{Monstey_GadbManagerFreeSyncOperation::leave,c.site,c.critical_section};
    if(!s.sync)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::leave_cs,c.site,f);
    return Monstey_GadbManagerBackendRdxResult{s.sync(m,f).rdx};
  };
  b.exchange32=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerBackendExchange32Call &c){
    if(!s.exchange32)throw Monstey_GadbManagerBackendExchange32Fault(c.native_site,c.address,c.value);
    return s.exchange32(m,c);
  };
  b.placement.compare_exchange32=[&s](Monstey_GadbBinderMemory m,const Monstey_GadbManagerCompareExchange32Call &c){
    if(!s.placement.compare_exchange32)throw Monstey_GadbManagerCompareExchange32Fault(c.native_site,c.address,c.comparand,c.desired);
    return s.placement.compare_exchange32(m,c);
  };
  return b;
}
template<int M> const Monstey_GadbManagerBackendModeCells &Monstey_FreeMode(const Monstey_Bindings &b){
  if constexpr(M==40)return b.backend.mode_40;
  else if constexpr(M==80)return b.backend.mode_80;
  else return b.backend.mode_c0;
}
template<int M> Monstey_Raw Monstey_FreeInit(Monstey_GadbBinderMemory m,Monstey_U64 f,const Monstey_Services &s,const Monstey_Bindings &b){
  const auto v=Monstey_FreeBackendView(s);
  if constexpr(M==40)return Monstey_GadbInitializeManagerBackend40(m,f,v,b.backend);
  else if constexpr(M==80)return Monstey_GadbInitializeManagerBackend80(m,f,v,b.backend);
  else return Monstey_GadbInitializeManagerBackendC0(m,f,v,b.backend);
}
template<int M> Monstey_Raw Monstey_FreeTail(Monstey_GadbBinderMemory m,Monstey_U64 b,Monstey_U64 x,Monstey_U64 n,const Monstey_Bindings &v){
  if constexpr(M==40)return Monstey_GadbInitializeManagerBackendTail40(m,b,x,n,v.backend.placement).raw;
  else if constexpr(M==80)return Monstey_GadbInitializeManagerBackendTail80(m,b,x,n,v.backend.placement).raw;
  else return Monstey_GadbInitializeManagerBackendTailC0(m,b,x,n,v.backend.placement).raw;
}
Monstey_Raw Monstey_FreeFind(const Monstey_FreeImage &m,Monstey_U64 b,Monstey_U64 q){
  auto n=b+0x3A0u;
  for(;;){const auto base=m.Monstey_Read(n);if(q>=base){const auto end=base+m.Monstey_Read(n+8);if(q<end)return {n,q};}
    n=m.Monstey_Read(n+0x10);if(n==0)return {0,q};}
}
Monstey_Raw Monstey_FreeLink(const Monstey_FreeImage &m,Monstey_U64 b,Monstey_U64 g){
  const auto base=m.Monstey_Read(g);auto n=b+0x3A0u;
  for(;;){if(n>=base){const auto end=base+m.Monstey_Read(g+8);if(n<end)return {1,g};}
    n=m.Monstey_Read(n+0x10);if(n==0)return {0,g};}
}
template<int M> Monstey_Raw Monstey_FreeUnmap(const Monstey_FreeImage &m,Monstey_U64 p,Monstey_U64 left,
    Monstey_U64 f,const Monstey_Services &s,const Monstey_Bindings &b){
  if(p==0 || left==0)return {0,left};
  constexpr Monstey_U64 query=M==40?0x140C234DCULL:M==80?0x140C23400ULL:0x140C235D4ULL;
  constexpr Monstey_U64 release=M==40?0x140C23511ULL:M==80?0x140C23435ULL:0x140C23609ULL;
  for(;;){
    const Monstey_GadbManagerFreeQueryCall q{Monstey_FreeSite(m,query,f,b.query_cell),p,f+0x20,48};
    if(!s.query)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::query,q.site,q);
    const auto r=s.query(m.m,q);
    if(r.rax==0){m.Monstey_ZeroRmw(0,4);return {0xFFFFFFFFu,r.rdx};}
    if(m.Monstey_Read(f+0x20)!=p)return {0xFFFFFFFFu,r.rdx};
    if(m.Monstey_Read(f+0x28)!=p)return {0xFFFFFFFFu,r.rdx};
    if(m.Monstey_Read(f+0x40,4)!=0x1000)return {0xFFFFFFFFu,r.rdx};
    if(m.Monstey_Read(f+0x38)>left)return {0xFFFFFFFFu,r.rdx};
    const Monstey_GadbManagerFreeReleaseCall c{Monstey_FreeSite(m,release,f,b.release_cell),p,0,0x8000};
    if(!s.release)throw Monstey_GadbManagerFreeImportFault(Monstey_GadbManagerFreeImport::release,c.site,c);
    const auto rr=s.release(m.m,c);
    if(rr.eax==0){m.Monstey_ZeroRmw(0,4);return {0xFFFFFFFFu,rr.rdx};}
    p+=m.Monstey_Read(f+0x38);left-=m.Monstey_Read(f+0x38);
    if(left==0)return {0,rr.rdx};
  }
}
// Concrete inline tree repair, factored only across the three equal ledgers.
// RDX is altered only by the 40 leaf descent; twins keep the next identity.
template<bool Forty,bool Periodic=false> void Monstey_FreeUnlinkTree(const Monstey_FreeImage &m,Monstey_U64 b,Monstey_U64 x,
    Monstey_U64 &a,Monstey_U64 &d){
  auto c=m.Monstey_Read(x+0x18);const auto parent=m.Monstey_Read(x+0x30);
  if(c!=x){a=m.Monstey_Read(x+0x10);m.Monstey_Write(a+0x18,c);m.Monstey_Write(c+0x10,a);}
  else {
    Monstey_U64 slot;
    if constexpr(Forty){c=m.Monstey_Read(x+0x28);slot=x+0x28;if(c==0){c=m.Monstey_Read(x+0x20);slot=x+0x20;}}
    else {a=x+0x28;c=m.Monstey_Read(a);if(c==0){a=x+0x20;c=m.Monstey_Read(a);}slot=a;}
    if(c!=0){for(;;){
      auto child=m.Monstey_Read(c+0x28);auto childSlot=c+0x28;
      if constexpr(Forty){d=child;a=childSlot;}
      if(child==0){child=m.Monstey_Read(c+0x20);childSlot=c+0x20;if constexpr(Forty){d=child;a=childSlot;}}
      if(child==0)break;
      slot=childSlot;if constexpr(!Forty)a=slot;c=child;
    }
    if constexpr(Forty || Periodic)m.Monstey_Write(slot,0);else m.Monstey_ZeroRmw(slot);
    }
  }
  if(parent==0)return;
  a=m.Monstey_Read(x+0x38,4);const auto root=b+0x258u+a*8;
  if(x==m.Monstey_Read(root)){
    m.Monstey_Write(root,c);
    if(c==0){auto map=m.Monstey_Read(b+4,4);a=m.Monstey_Read(x+0x38,4);map&=~(Monstey_U64{1}<<(a&31));m.Monstey_Write(b+4,map,4);return;}
  } else {if(m.Monstey_Read(parent+0x20)==x)m.Monstey_Write(parent+0x20,c);else m.Monstey_Write(parent+0x28,c);if(c==0)return;}
  m.Monstey_Write(c+0x30,parent);a=m.Monstey_Read(x+0x20);
  if(a!=0){m.Monstey_Write(c+0x20,a);m.Monstey_Write(a+0x30,c);}
  a=m.Monstey_Read(x+0x28);if(a!=0){m.Monstey_Write(c+0x28,a);m.Monstey_Write(a+0x30,c);}
}
// Style40 uses direct zero stores; Style80 uses the two native memory AND0s.
// Style0 is the periodic large-only reinsertion (direct stores, parent last).
template<int Style> Monstey_Raw Monstey_FreeInsertLarge(const Monstey_FreeImage &m,
    Monstey_U64 b,Monstey_U64 h,Monstey_U64 size){
  Monstey_U64 a=size>>8;Monstey_U32 cls;
  if(a==0)cls=0;
  else if(a>0xFFFF)cls=31;
  else {const auto bit=static_cast<Monstey_U32>(std::bit_width(static_cast<Monstey_U32>(a))-1);
    cls=bit*2+static_cast<Monstey_U32>((size>>((bit+7)&63))&1);}
  m.Monstey_Write(h+0x38,cls,4);
  Monstey_U64 d;
  if constexpr(Style!=80){d=Monstey_U64(cls)+0x4B;m.Monstey_Write(h+0x28,0);m.Monstey_Write(h+0x20,0);}
  else {m.Monstey_ZeroRmw(h+0x28);m.Monstey_ZeroRmw(h+0x20);}
  a=m.Monstey_Read(b+4,4);
  if constexpr(Style==80)d=Monstey_U64(cls)+0x4B;
  d=b+d*8;const auto bit=Monstey_U32{1}<<(cls&31);
  if((static_cast<Monstey_U32>(a)&bit)==0){a=static_cast<Monstey_U32>(a)|bit;
    m.Monstey_Write(b+4,a,4);m.Monstey_Write(d,h);m.Monstey_Write(h+0x30,d);
    m.Monstey_Write(h+0x18,h);m.Monstey_Write(h+0x10,h);return {a,d};}
  d=m.Monstey_Read(d);const auto shift=cls==31?0u:57u-(cls>>1);
  a=m.Monstey_Read(d+8);auto key=size<<(shift&63);
  for(;;){a&=~Monstey_U64{7};if(a==size)break;
    const auto side=key>>63;key+=key;const auto slot=d+0x20+side*8;
    if constexpr(Style==40){a=m.Monstey_Read(slot);if(a==0){
      m.Monstey_Write(slot,h);m.Monstey_Write(h+0x30,d);m.Monstey_Write(h+0x18,h);m.Monstey_Write(h+0x10,h);return {a,d};}
      d=a;a=m.Monstey_Read(a+8);
    }else{a=side;const auto child=m.Monstey_Read(slot);if(child==0){
      m.Monstey_Write(slot,h);m.Monstey_Write(h+0x30,d);m.Monstey_Write(h+0x18,h);m.Monstey_Write(h+0x10,h);return {a,d};}
      a=m.Monstey_Read(child+8);d=child;
    }
  }
  a=m.Monstey_Read(d+0x10);m.Monstey_Write(a+0x18,h);m.Monstey_Write(d+0x10,h);
  if constexpr(Style==80)m.Monstey_ZeroRmw(h+0x30);
  m.Monstey_Write(h+0x10,a);m.Monstey_Write(h+0x18,d);
  if constexpr(Style!=80)m.Monstey_Write(h+0x30,0);
  return {a,d};
}
template<int M> Monstey_Raw Monstey_FreePeriodic(const Monstey_FreeImage &m,Monstey_U64 b,
    Monstey_U64 f,Monstey_U64 d,const Monstey_Services &s,const Monstey_Bindings &v){
  Monstey_U64 sum=0;auto pred=b+0x3A0u;auto sp=m.Monstey_Read(pred+0x10);
  while(sp!=0){
    const auto flags=m.Monstey_Read(sp+0x18,4);const auto base=m.Monstey_Read(sp);
    const auto span=m.Monstey_Read(sp+8);const auto next=m.Monstey_Read(sp+0x10);
    if((flags&9)==1){const auto h=base+((Monstey_U64{0}-(base&7))&7);
      auto a=m.Monstey_Read(h+8);const auto size=a&~Monstey_U64{7};
      if((a&3)==1 && h+size>=base-0x48u+span){
        if(h==m.Monstey_Read(b+0x20)){m.Monstey_Write(b+0x20,0);m.Monstey_Write(b+8,0);}
        else Monstey_FreeUnlinkTree<false,true>(m,b,h,a,d);
        const auto r=Monstey_FreeUnmap<M>(m,base,span,f-0x60u,s,v);d=r.rdx;
        if(static_cast<Monstey_U32>(r.rax)==0){sum+=span;sp=pred;m.Monstey_Sub(b+0x358u,span);m.Monstey_Write(pred+0x10,next);}
        else d=Monstey_FreeInsertLarge<0>(m,b,h,size).rdx;
      }
    }
    pred=sp;sp=next;
  }
  m.Monstey_MaxRmw(b+0x38);return {sum,d};
}
template<int M> Monstey_Raw Monstey_FreeTrim(const Monstey_FreeImage &m,Monstey_U64 b,
    Monstey_U64 f,Monstey_U64 d,const Monstey_Services &s,const Monstey_Bindings &v){
  const auto mode=Monstey_FreeMode<M>(v);Monstey_U64 released=0;
  if(m.Monstey_Read(mode.cookie)==0)d=Monstey_FreeInit<M>(m.m,f-0x60u,s,v).rdx;
  if(m.Monstey_Read(b+0x28)==0)return {0,d};
  auto top=m.Monstey_Read(b+0x10);
  if(top>0x48){
    const auto gran=m.Monstey_Read(mode.granularity);const auto dividend=gran-0x49u+top;
    if(gran==0)throw Monstey_GadbManagerFreeDivideFault(M==40?0x141175ED1ULL:M==80?0x1411747C9ULL:0x1411778D9ULL,f,dividend,gran);
    const auto extra=dividend-(dividend%gran)-gran;
    const auto query=m.Monstey_Read(b+0x28);const auto found=Monstey_FreeFind(m,b,query);d=found.rdx;const auto g=found.rax;
    if((m.Monstey_Read(g+0x18,1)&8)==0 && (m.Monstey_Read(g+0x18,1)&1)!=0 && m.Monstey_Read(g+8)>=extra){
      const auto linked=Monstey_FreeLink(m,b,g);d=linked.rdx;
      if(static_cast<Monstey_U32>(linked.rax)==0){auto address=m.Monstey_Read(d);address-=extra;address+=m.Monstey_Read(d+8);
        const auto r=Monstey_FreeUnmap<M>(m,address,extra,f-0x60u,s,v);d=r.rdx;
        if(static_cast<Monstey_U32>(r.rax)==0){released=extra;if(extra!=0){
          m.Monstey_Sub(g+8,extra);top=m.Monstey_Read(b+0x10);m.Monstey_Sub(b+0x358u,extra);top-=extra;
          const auto start=m.Monstey_Read(b+0x28);d=Monstey_FreeTail<M>(m.m,b,start,top,v).rdx;
        }}
      }
    }
  }
  const auto periodic=Monstey_FreePeriodic<M>(m,b,f-0x50u,d,s,v);released+=periodic.rax;d=periodic.rdx;
  if(released==0){const auto threshold=m.Monstey_Read(b+0x30);if(m.Monstey_Read(b+0x10)>threshold)m.Monstey_MaxRmw(b+0x30);}
  return {released!=0?Monstey_U64{1}:Monstey_U64{0},d};
}
template<int M> Monstey_Raw Monstey_FreeBody(const Monstey_FreeImage &m,Monstey_U64 b,Monstey_U64 p,
    const Monstey_GadbManagerFreeBody &f,const Monstey_Services &s,const Monstey_Bindings &v){
  if(p==0)return {f.incoming_rax,0};
  Monstey_U64 a=f.incoming_rax,d=p;auto h=p-0x10u;
  if((m.Monstey_Read(b+0x370u,1)&2)!=0){const auto r=Monstey_FreeSync(m,s,Monstey_GadbManagerFreeSyncOperation::enter,
      M==40?0x1404D6AEFULL:M==80?0x14081A543ULL:0x14081A09BULL,f.body_rsp,v.backend.imports.enter_cs,b+0x378u);a=r.rax;d=r.rdx;}
  Monstey_U64 size,n;bool prevInUse;
  if constexpr(M==40){a=m.Monstey_Read(h+8);size=a&~Monstey_U64{7};n=h+size;prevInUse=(a&1)!=0;}
  else {size=m.Monstey_Read(h+8)&~Monstey_U64{7};prevInUse=(m.Monstey_Read(h+8,1)&1)!=0;d=h+size;n=d;}
  const auto finish=[&]() -> Monstey_Raw {
    if((m.Monstey_Read(b+0x370u,1)&2)!=0)return Monstey_FreeSync(m,s,Monstey_GadbManagerFreeSyncOperation::leave,
      M==40?0x1404D6F69ULL:M==80?0x14081A992ULL:0x14081A4EAULL,f.body_rsp,v.backend.imports.leave_cs,b+0x378u);
    return {a,d};
  };
  const auto unlinkSmall=[&](Monstey_U64 x,Monstey_U64 index){
    a=m.Monstey_Read(x+0x10);const auto back=m.Monstey_Read(x+0x18);if constexpr(M==40)d=back;
    if(back==a){a=m.Monstey_Read(b,4);a&=~(Monstey_U64{1}<<(index&31));m.Monstey_Write(b,a,4);}
    else {m.Monstey_Write(a+0x18,back);m.Monstey_Write(back+0x10,a);}
  };
  if(!prevInUse){const auto prev=m.Monstey_Read(h);
    bool mapped;
    if constexpr(M==40){h-=prev;mapped=(a&3)==0;}else mapped=(m.Monstey_Read(h+8,1)&3)==0;
    if(mapped){if constexpr(M!=40)h-=prev;const auto total=prev+0x20u+size;
      const auto r=Monstey_FreeUnmap<M>(m,h,total,f.body_rsp-0x60u,s,v);a=r.rax;d=r.rdx;
      if(static_cast<Monstey_U32>(a)==0)m.Monstey_Sub(b+0x358u,total);return finish();
    }
    size+=prev;if constexpr(M!=40)h-=prev;
    if(h==m.Monstey_Read(b+0x20)){
      const auto bits=m.Monstey_Read(n+8,1)&3;
      if(bits==3){m.Monstey_Write(b+8,size);a=size;m.Monstey_And(n+8,~Monstey_U64{1});a|=1;
        m.Monstey_Write(h+8,a);m.Monstey_Write(h+size,size);return finish();}
    }else {const auto index=prev>>3;if(index<32)unlinkSmall(h,index);else Monstey_FreeUnlinkTree<M==40>(m,b,h,a,d);}
  }
  auto nextHeader=m.Monstey_Read(n+8);
  if((nextHeader&2)==0){
    if(n==m.Monstey_Read(b+0x28)){
      m.Monstey_Add(b+0x10,size);const auto merged=m.Monstey_Read(b+0x10);a=merged;m.Monstey_Write(b+0x28,h);a|=1;m.Monstey_Write(h+8,a);
      if(h==m.Monstey_Read(b+0x20)){if constexpr(M==40){m.Monstey_Write(b+0x20,0);m.Monstey_Write(b+8,0);}else {m.Monstey_ZeroRmw(b+0x20);m.Monstey_ZeroRmw(b+8);}}
      if(merged>m.Monstey_Read(b+0x30)){const auto r=Monstey_FreeTrim<M>(m,b,f.body_rsp-0x30u,d,s,v);a=r.rax;d=r.rdx;}return finish();
    }
    if(n==m.Monstey_Read(b+0x20)){
      m.Monstey_Add(b+8,size);const auto merged=m.Monstey_Read(b+8);m.Monstey_Write(b+0x20,h);a=merged|1;
      m.Monstey_Write(h+8,a);m.Monstey_Write(h+merged,merged);return finish();
    }
    const auto nextSize=nextHeader&~Monstey_U64{7};size+=nextSize;
    if((nextSize>>3)<32)unlinkSmall(n,nextSize>>3);else Monstey_FreeUnlinkTree<M==40>(m,b,n,a,d);
    a=size|1;m.Monstey_Write(h+8,a);m.Monstey_Write(h+size,size);
    if(h==m.Monstey_Read(b+0x20)){m.Monstey_Write(b+8,size);return finish();}
  }else {nextHeader&=~Monstey_U64{1};a=size;m.Monstey_Write(n+8,nextHeader);a|=1;m.Monstey_Write(h+8,a);m.Monstey_Write(h+size,size);}
  d=size>>3;
  if(d<32){auto map=m.Monstey_Read(b,4);const auto head=b+(static_cast<Monstey_U32>(d+d)+Monstey_U64{9})*8;a=head;
    Monstey_U64 forward=head;const auto bit=Monstey_U32{1}<<(d&31);
    if((map&bit)==0){map|=bit;m.Monstey_Write(b,map,4);}else forward=m.Monstey_Read(head+0x10);
    m.Monstey_Write(head+0x10,h);m.Monstey_Write(forward+0x18,h);m.Monstey_Write(h+0x10,forward);m.Monstey_Write(h+0x18,head);return finish();
  }
  const auto inserted=Monstey_FreeInsertLarge<M==40?40:80>(m,b,h,size);a=inserted.rax;d=inserted.rdx;
  if(m.Monstey_Sub(b+0x38,1)==0){const auto r=Monstey_FreePeriodic<M>(m,b,f.body_rsp-0x50u,d,s,v);a=r.rax;d=r.rdx;}
  return finish();
}
} // namespace
Monstey_GadbBufferedResult Monstey_GadbManagerFreeTrapReturn(Monstey_GadbBinderMemory memory,
    std::uint64_t entry,Monstey_GadbBufferedResult incoming,std::uint64_t r8,std::uint64_t r9){
  const Monstey_FreeImage m{memory};m.Monstey_Write(entry+0x10,incoming.rdx);
  m.Monstey_Write(entry+0x18,r8);m.Monstey_Write(entry+0x20,r9);m.Monstey_Write(0,0,4);return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbFreeManager(Monstey_GadbBinderMemory memory,std::uint64_t manager,
    std::uint64_t payload,const Monstey_GadbManagerFreeContext &context,const Monstey_GadbManagerFreeServices &services,
    Monstey_GadbManagerFreeBindings bindings){
  const Monstey_FreeImage m{memory};const auto backend=m.Monstey_Read(manager+0x10);
  if(payload==0)return {context.incoming_rax,0};
  const auto mode=m.Monstey_Read(backend+0x370,4)&0xC0000000u;
  if(mode==0x40000000u)return Monstey_GadbFreeManagerBackend40Thunk(memory,backend,payload,{context.body_rsp-8u,mode},services,bindings);
  if(mode==0x80000000u)return Monstey_GadbFreeManagerBackend80Thunk(memory,backend,payload,{context.body_rsp+0x28u,mode},services,bindings);
  if(mode==0xC0000000u)return Monstey_GadbFreeManagerBackendC0Thunk(memory,backend,payload,{context.body_rsp+0x28u,mode},services,bindings);
  return Monstey_GadbManagerFreeTrapReturn(memory,context.body_rsp+0x28u,{0,payload},0xC0000000u,context.incoming_r9);
}
#define Monstey_DefineFreeVariant(Suffix, Mode) \
Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend##Suffix##Thunk( \
    Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t p, \
    const Monstey_GadbManagerFreeEntry &e,const Monstey_GadbManagerFreeServices &s,Monstey_GadbManagerFreeBindings v){ \
  return Monstey_GadbFreeManagerBackend##Suffix(m,b,p,{e.entry_rsp-0x28u,e.incoming_rax},s,v); \
} \
Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend##Suffix( \
    Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t p, \
    const Monstey_GadbManagerFreeBody &f,const Monstey_GadbManagerFreeServices &s,Monstey_GadbManagerFreeBindings v){ \
  return Monstey_FreeBody<Mode>({m},b,p,f,s,v); \
} \
Monstey_GadbBufferedResult Monstey_GadbUnmapManagerRegion##Suffix( \
    Monstey_GadbBinderMemory m,std::uint64_t p,std::uint64_t n,std::uint64_t f, \
    const Monstey_GadbManagerFreeServices &s,Monstey_GadbManagerFreeBindings v){return Monstey_FreeUnmap<Mode>({m},p,n,f,s,v);} \
Monstey_GadbBufferedResult Monstey_GadbTrimManagerBackend##Suffix( \
    Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t f,std::uint64_t d, \
    const Monstey_GadbManagerFreeServices &s,Monstey_GadbManagerFreeBindings v){return Monstey_FreeTrim<Mode>({m},b,f,d,s,v);} \
Monstey_GadbBufferedResult Monstey_GadbReleaseManagerSegments##Suffix( \
    Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t f,std::uint64_t d, \
    const Monstey_GadbManagerFreeServices &s,Monstey_GadbManagerFreeBindings v){return Monstey_FreePeriodic<Mode>({m},b,f,d,s,v);} \
Monstey_GadbBufferedResult Monstey_GadbFindManagerSegment##Suffix(Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t q){return Monstey_FreeFind({m},b,q);} \
Monstey_GadbBufferedResult Monstey_GadbHasManagerSegmentLink##Suffix(Monstey_GadbBinderMemory m,std::uint64_t b,std::uint64_t g){return Monstey_FreeLink({m},b,g);}
Monstey_DefineFreeVariant(40,40)
Monstey_DefineFreeVariant(80,80)
Monstey_DefineFreeVariant(C0,120)
#undef Monstey_DefineFreeVariant
} // namespace Monstey
