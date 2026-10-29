#include "Monstey_GadbPublication.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
struct Monstey_TestError : std::runtime_error { using std::runtime_error::runtime_error; };
struct Monstey_ProviderError { int marker; };
unsigned Monstey_Checks = 0;
void Monstey_Check(bool value, const char *message) {
  ++Monstey_Checks;
  if (!value) throw Monstey_TestError(message);
}
std::size_t Monstey_TestOffset(Monstey_GadbBinderMemory m, Monstey_U64 at,
                              std::size_t n, bool writing) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbBufferedMemoryFault(at, n, writing);
  return static_cast<std::size_t>(at - m.base);
}
Monstey_U64 Monstey_Read(Monstey_GadbBinderMemory m, Monstey_U64 at,
                        std::size_t n = 8) {
  const auto p = Monstey_TestOffset(m, at, n, false);
  Monstey_U64 value = 0;
  for (std::size_t i = 0; i < n; ++i) value |= Monstey_U64(m.bytes[p + i]) << (i * 8u);
  return value;
}
void Monstey_Write(Monstey_GadbBinderMemory m, Monstey_U64 at,
                   Monstey_U64 value, std::size_t n = 8) {
  const auto p = Monstey_TestOffset(m, at, n, true);
  for (std::size_t i = 0; i < n; ++i) m.bytes[p + i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
struct Monstey_Event {
  Monstey_U64 site, address, target, rax, rdx;
};
struct Monstey_Fixture {
  static constexpr Monstey_U64 base = 0x100000;
  static constexpr Monstey_U64 db = base + 0x100, control = base + 0x400,
      pair = base + 0x500, owner = base + 0x600, vt = base + 0x700,
      q = base + 0x800;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x2000, 0);
  std::vector<Monstey_Event> events;
  Monstey_GadbPublicationServices services;
  Monstey_GadbBinderMemory memory() { return {base, bytes}; }
  Monstey_U64 read(Monstey_U64 at, std::size_t n = 8) { return Monstey_Read(memory(), at, n); }
  void write(Monstey_U64 at, Monstey_U64 value, std::size_t n = 8) { Monstey_Write(memory(), at, value, n); }
  Monstey_U32 atomic(Monstey_GadbBinderMemory m, const Monstey_GadbPublicationAtomicCall &call) {
    events.push_back({call.native_site, call.address, call.addend, 0, 0});
    const auto old = static_cast<Monstey_U32>(Monstey_Read(m, call.address, 4));
    Monstey_Write(m, call.address, old + call.addend, 4);
    return old;
  }
  void bind_atomic() {
    services.atomic32 = [this](Monstey_GadbBinderMemory m, const Monstey_GadbPublicationAtomicCall &call) {
      return atomic(m, call);
    };
  }
  void init_control(Monstey_U32 primary, Monstey_U32 secondary) {
    write(control, vt); write(control + 8, primary, 4); write(control + 12, secondary, 4);
    write(vt, 0xABC0); write(vt + 8, 0xABC8);
  }
  void init_main() {
    write(q + 0x60, 9, 4); write(q + 0x68, 7, 4);
    write(q + 0x70, 0xCAFE); write(q + 0x78, pair);
    write(q + 0x80, 0xD00D); write(q + 0x88, 0xB000);
    write(q + 0x90, 0xA1, 4); write(q + 0x98, 0xB1, 4);
    write(q + 0xA0, 0xA000); write(q + 0xA8, owner);
    write(pair, 0x12345678); write(pair + 8, 0); write(owner, 0xF00D);
    write(q + 0x20, 0xDEAD); write(q + 0x28, 0xBEEF);
  }
  Monstey_GadbBufferedResult publish() {
    return Monstey_GadbPublishDatabaseStorage(memory(), db, 0x11223344, 0x55667788,
                                              0xBABA, q, services);
  }
};
void Monstey_Raw(Monstey_GadbBufferedResult r, Monstey_U64 a, Monstey_U64 d) {
  Monstey_Check(r.rax == a, "raw RAX mismatch"); Monstey_Check(r.rdx == d, "raw RDX mismatch");
}
void Monstey_Fault(const Monstey_GadbPublicationBoundaryFault &f,
                   Monstey_GadbPublicationBoundaryKind kind, Monstey_U64 site,
                   Monstey_U64 caller, Monstey_U64 address, Monstey_U32 width,
                   Monstey_U64 slot, Monstey_U64 target, Monstey_U64 message) {
  Monstey_Check(f.kind == kind, "boundary kind"); Monstey_Check(f.native_site == site, "boundary site");
  Monstey_Check(f.call_site == caller, "boundary call site"); Monstey_Check(f.address == address, "boundary address");
  Monstey_Check(f.width == width, "boundary width"); Monstey_Check(f.slot == slot, "boundary slot");
  Monstey_Check(f.target == target, "boundary target"); Monstey_Check(f.message_identity == message, "boundary message");
}
using Monstey_Case = std::pair<const char *, std::function<void()>>;
std::vector<Monstey_Case> Monstey_Cases() {
  return {
    {"main-distinct-owner-normal", [] {
      Monstey_Fixture f; f.init_main(); const auto r = f.publish();
      Monstey_Raw(r, 0, 0xF00D);
      Monstey_Check(f.read(f.db) == 0xBABA && f.read(f.db + 8) == 0xD00D, "bank/descriptor stores");
      Monstey_Check(f.read(f.db + 0x10) == 0xB000 && f.read(f.db + 0x40) == 0xA000, "arena stores");
      Monstey_Check(f.read(f.db + 0x28, 4) == 0x11223344 && f.read(f.db + 0x2C, 4) == 0x55667788, "header stores");
      Monstey_Check(f.read(f.db + 0x30, 4) == 0xA1 && f.read(f.db + 0x34, 4) == 0xB1, "size stores");
      Monstey_Check(f.read(f.db + 0x48, 4) == 9 && f.read(f.db + 0x4C, 4) == 7, "count stores");
      Monstey_Check(f.read(f.db + 0x50) == 0xCAFE && f.read(f.db + 0x58) == 0x12345678, "category/pool stores");
      Monstey_Check(f.read(f.db + 0x60) == 0 && f.read(f.db + 0x20) == 0xF00D && f.read(f.owner) == 0, "owner move");
      Monstey_Check(f.read(f.pair) == 0x12345678 && f.read(f.pair + 8) == 0, "source pair not cleared");
      Monstey_Check(f.events.empty(), "unused empty service bindings allowed");
    }},
    {"main-equal-owner-preserves-last-raw", [] {
      Monstey_Fixture f; f.init_main(); f.write(f.q + 0xA8, f.db + 0x20);
      Monstey_Raw(f.publish(), 7, 0x12345678);
    }},
    {"release-pair-minimum-null-read", [] {
      std::array<std::uint8_t, 8> bytes{}; Monstey_GadbPublicationServices s;
      const auto before = bytes;
      Monstey_Raw(Monstey_GadbReleasePublicationPair({0x1008, bytes}, 0x1000,
                                                     {0xFEDCBA9876543210ULL, 0x123456789ABCDEF0ULL}, s),
                  0xFEDCBA9876543210ULL, 0x123456789ABCDEF0ULL);
      Monstey_Check(bytes == before, "null release changed pair");
    }},
    {"release-pair-read-first-fault", [] {
      Monstey_Fixture f;
      try { (void)Monstey_GadbReleasePublicationPair(f.memory(), f.base - 16, {3,4}, f.services); throw Monstey_TestError("missing fault"); }
      catch (const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address == f.base - 8 && e.width == 8 && !e.writing, "pair minimum range fault"); }
    }},
    {"assign-null-old-preserves-rax-and-caches-args", [] {
      Monstey_Fixture f;
      Monstey_Raw(Monstey_GadbAssignPublicationPair(f.memory(), f.pair, 0xDADA, 0xCACA, {0xAAAAAAAA11111111ULL, 0xBBBB}, f.services), 0xAAAAAAAA11111111ULL, 0xDADA);
      Monstey_Check(f.read(f.pair) == 0xDADA && f.read(f.pair + 8) == 0xCACA, "assign data/control");
    }},
    {"assign-consumed-write-fault", [] {
      std::array<std::uint8_t,8> bytes{}; Monstey_GadbPublicationServices s;
      try { (void)Monstey_GadbAssignPublicationPair({0x1008,bytes},0x1000,9,8,{1,2},s); throw Monstey_TestError("missing write fault"); }
      catch(const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address == 0x1000 && e.width == 8 && e.writing, "assign exact write fault"); }
    }},
    {"copy-null-counted-source-not-cleared", [] {
      Monstey_Fixture f; f.write(f.pair,0xDADA); f.write(f.pair+8,0); f.write(f.db,8); f.write(f.db+8,9);
      Monstey_Raw(Monstey_GadbCopyPublicationPair(f.memory(),f.db,f.pair,{0xFAFA,3},f.services),f.db,0xDADA);
      Monstey_Check(f.read(f.pair)==0xDADA && f.read(f.pair+8)==0 && f.read(f.db)==0xDADA && f.read(f.db+8)==0,"copy values");
    }},
    {"copy-self-alias", [] {
      Monstey_Fixture f; f.write(f.pair,0xDADA); f.write(f.pair+8,f.control);
      Monstey_Raw(Monstey_GadbCopyPublicationPair(f.memory(),f.pair,f.pair,{9,8},f.services),f.pair,0);
      Monstey_Check(f.read(f.pair)==0 && f.read(f.pair+8)==0,"self alias clears before reads");
    }},
    {"copy-partial-alias", [] {
      Monstey_Fixture f; f.write(f.pair-8,0xDADA); f.write(f.pair,0xEEEE); f.write(f.pair+8,0xFFFF);
      Monstey_Raw(Monstey_GadbCopyPublicationPair(f.memory(),f.pair,f.pair-8,{9,8},f.services),f.pair,0xDADA);
      Monstey_Check(f.read(f.pair)==0xDADA && f.read(f.pair+8)==0,"partial alias native order");
    }},
    {"copy-wrap-increment-and-raw", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(0xFFFFFFFFu,8); f.write(f.pair,0xDADA); f.write(f.pair+8,f.control);
      Monstey_Raw(Monstey_GadbCopyPublicationPair(f.memory(),f.db,f.pair,{0xFEDCBA9876543210ULL,9},f.services),f.db,0xDADA);
      Monstey_Check(f.read(f.control+8,4)==0 && f.read(f.pair+8)==f.control,"increment modular counted copy");
      Monstey_Check(f.events.size()==1 && f.events[0].site==0x1404BAEA6ULL && f.events[0].target==1,"INC narrow primitive");
    }},
    {"copy-counter-alias-activates-real-assign", [] {
      Monstey_Fixture f; f.bind_atomic(); f.write(f.pair,0xDADA); f.write(f.pair+8,f.db);
      try { (void)Monstey_GadbCopyPublicationPair(f.memory(),f.db,f.pair,{9,8},f.services); throw Monstey_TestError("missing aliased fault"); }
      catch(const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address==9 && e.width==4 && !e.writing,"counter alias current old control read"); }
      Monstey_Check(f.read(f.db)==0 && f.read(f.db+8)==1,"alias committed prefix");
      Monstey_Check(f.events.size()==2 && f.events[1].address==9,"real assign release consumed");
    }},
    {"copy-missing-atomic-after-clear", [] {
      Monstey_Fixture f; f.write(f.pair,0xDADA); f.write(f.pair+8,f.control); f.write(f.db,9); f.write(f.db+8,8);
      try { (void)Monstey_GadbCopyPublicationPair(f.memory(),f.db,f.pair,{9,8},f.services); throw Monstey_TestError("missing atomic fault"); }
      catch(const Monstey_GadbPublicationBoundaryFault &e) { Monstey_Fault(e,Monstey_GadbPublicationBoundaryKind::missing_atomic32,0x1404BAEA6ULL,0x1404BAEA6ULL,f.control+8,4,0,0,0); }
      Monstey_Check(f.read(f.db)==0 && f.read(f.db+8)==0 && f.read(f.pair)==0xDADA,"clear before missing primitive");
    }},
    {"copy-and-is-read-before-write", [] {
      std::array<std::uint8_t,8> bytes{}; Monstey_GadbPublicationServices s;
      try { (void)Monstey_GadbCopyPublicationPair({0x1008,bytes},0x1000,0,{1,2},s); throw Monstey_TestError("missing AND fault"); }
      catch(const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address==0x1000 && e.width==8 && !e.writing,"AND reports read first"); }
    }},
    {"primary-and-secondary-non1-count-matrix", [] {
      for(const auto count: std::array<Monstey_U32,3>{0,2,0xFFFFFFFFu}) {
        for(const bool primary: {false,true}) {
          Monstey_Fixture f; f.bind_atomic(); f.init_control(count,count); const auto before=f.bytes;
          const auto r=primary ? Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{0xAAAAAAAAFFFFFFFFULL,0xBABA},f.services)
                               : Monstey_GadbReleasePublicationSecondary(f.memory(),f.control,{0xAAAAAAAAFFFFFFFFULL,0xBABA},f.services);
          Monstey_Raw(r,count,0xBABA);
          Monstey_Check(f.read(f.control+(primary?8:12),4)==Monstey_U32(count-1),"XADD wrapping");
          auto expected=before; const auto p=static_cast<std::size_t>(f.control+(primary?8:12)-f.base); const auto value=Monstey_U32(count-1);
          for(std::size_t i=0;i<4;++i) expected[p+i]=static_cast<std::uint8_t>(value>>(i*8u));
          Monstey_Check(f.bytes==expected && f.events.size()==1,"only consumed counter changed");
        }
      }
    }},
    {"live-vptr-slot0-then-slot8-and-target0", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(1,1); f.write(f.vt,0); f.write(f.vt+0x28,0xDEAD);
      f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationVirtualCall &c) {
        f.events.push_back({c.native_site,c.this_identity,c.target_identity,c.incoming.rax,c.incoming.rdx});
        if(c.native_site==0x1403DD6FBULL) {
          Monstey_Check(c.target_identity==0 && c.incoming.rax==f.vt && c.incoming.rdx==0xBABA,"live slot0 raw/zero target");
          Monstey_Write(m,f.control,f.vt+0x20); return Monstey_GadbBufferedResult{0x1111,0x2222};
        }
        Monstey_Check(c.vtable_identity==f.vt+0x20 && c.slot_identity==f.vt+0x28 && c.target_identity==0xDEAD && c.incoming.rdx==0x2222,"live slot8 after mutation");
        return Monstey_GadbBufferedResult{0xFFFFFFFF12345678ULL,0xEEEECCCCAAAA8888ULL};
      };
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{9,0xBABA},f.services),0xFFFFFFFF12345678ULL,0xEEEECCCCAAAA8888ULL);
      Monstey_Check(f.events.size()==4 && f.read(f.control+8,4)==0 && f.read(f.control+12,4)==0,"both real decrements");
    }},
    {"primary-slot0-raw-flows-into-non1-secondary", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(1,2);
      f.services.virtual_call=[](Monstey_GadbBinderMemory,const Monstey_GadbPublicationVirtualCall &) { return Monstey_GadbBufferedResult{0xFFFF,0xABCD}; };
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{9,8},f.services),2,0xABCD);
    }},
    {"missing-virtual-after-live-target-read", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(1,1);
      try { (void)Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{9,8},f.services); throw Monstey_TestError("missing virtual fault"); }
      catch(const Monstey_GadbPublicationBoundaryFault &e) { Monstey_Fault(e,Monstey_GadbPublicationBoundaryKind::missing_virtual_call,0x1403DD6FBULL,0x1403DD6FBULL,f.control,8,f.vt,0xABC0,0); }
      Monstey_Check(f.read(f.control+8,4)==0 && f.read(f.control+12,4)==1,"decrement prefix before virtual fault");
    }},
    {"missing-virtual-does-not-hide-slot-range-fault", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(1,1); f.write(f.control,0xBAD);
      try { (void)Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{9,8},f.services); throw Monstey_TestError("missing slot fault"); }
      catch(const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address==0xBAD && e.width==8 && !e.writing,"live target before missing binding"); }
      Monstey_Check(f.read(f.control+8,4)==0,"committed primary decrement");
    }},
    {"provider-partial-atomic-effect-propagates", [] {
      Monstey_Fixture f; f.init_control(2,3);
      f.services.atomic32=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c)->Monstey_U32 {
        Monstey_Write(m,c.address,0xDEAD,4); throw Monstey_ProviderError{67};
      };
      try { (void)Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{9,8},f.services); throw Monstey_TestError("missing provider exception"); }
      catch(const Monstey_ProviderError &e) { Monstey_Check(e.marker==67 && f.read(f.control+8,4)==0xDEAD,"provider partial effect preserved"); }
    }},
    {"assign-cache-survives-virtual-mutation", [] {
      Monstey_Fixture f; f.bind_atomic(); f.init_control(1,2); f.write(f.pair+8,f.control);
      f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationVirtualCall &) {
        Monstey_Write(m,f.pair,0x1111); Monstey_Write(m,f.pair+8,0x2222); return Monstey_GadbBufferedResult{0x3333,0x4444};
      };
      Monstey_Raw(Monstey_GadbAssignPublicationPair(f.memory(),f.pair,0xDADA,0xCACA,{9,8},f.services),2,0x4444);
      Monstey_Check(f.read(f.pair)==0xDADA && f.read(f.pair+8)==0xCACA,"cached arguments published after actual release");
    }},
    {"main-late-homes-and-final-pair-reread", [] {
      Monstey_Fixture f; f.init_main(); f.bind_atomic(); f.init_control(1,2); f.write(f.db+0x60,f.control);
      f.write(f.pair+8,f.control+0x40); f.write(f.control+0x48,2,4);
      f.write(f.pair+0x20,0x9999); f.write(f.pair+0x28,0);
      f.write(f.owner+0x20,0xFAFA);
      f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationVirtualCall &) {
        Monstey_Write(m,f.q+0x88,0xBB11); Monstey_Write(m,f.q+0x90,0xAA22,4);
        Monstey_Write(m,f.q+0x98,0xBB33,4); Monstey_Write(m,f.q+0xA0,0xAA44);
        Monstey_Write(m,f.q+0x68,0x55,4); Monstey_Write(m,f.q+0xA8,f.owner+0x20);
        Monstey_Write(m,f.q+0x78,f.pair+0x20); return Monstey_GadbBufferedResult{0x66,0x77};
      };
      Monstey_Raw(f.publish(),0,0xFAFA);
      Monstey_Check(f.read(f.db+0x10)==0xBB11 && f.read(f.db+0x30,4)==0xAA22 && f.read(f.db+0x34,4)==0xBB33,"late arenas");
      Monstey_Check(f.read(f.db+0x40)==0xAA44 && f.read(f.db+0x4C,4)==0x55,"late scalar homes");
      Monstey_Check(f.read(f.owner)==0xF00D && f.read(f.owner+0x20)==0 && f.read(f.db+0x20)==0xFAFA,"late source owner identity");
      Monstey_Check(f.read(f.control+0x48,4)==3,"reread Q+78 skipped original incoming release");
    }},
    {"main-final-cmp-preserves-xadd-raw", [] {
      Monstey_Fixture f; f.init_main(); f.bind_atomic(); f.init_control(1,7); f.write(f.pair+8,f.control);
      Monstey_Raw(f.publish(),2,0xF00D);
      Monstey_Check(f.read(f.control+8,4)==1,"copy increment then incoming release");
    }},
    {"main-overlap-late-descriptor-home", [] {
      Monstey_Fixture f; f.init_main(); f.bind_atomic();
      const auto db=f.q+0x18; f.write(f.q+0xA8,f.owner);
      try { (void)Monstey_GadbPublishDatabaseStorage(f.memory(),db,0x11223344,0x55667788,0xBABA,f.q,f.services); throw Monstey_TestError("missing final pair fault"); }
      catch(const Monstey_GadbBufferedMemoryFault &e) { Monstey_Check(e.address==8 && e.width==8 && !e.writing,"late final pair read fault"); }
      Monstey_Check(f.read(db+0x4C,4)==0xCAFE,"interleaved frame/database descriptor count alias");
    }},
    {"main-first-trap-missing-after-move", [] {
      Monstey_Fixture f; f.init_main(); f.write(f.db+0x20,0xABAB);
      try { (void)f.publish(); throw Monstey_TestError("missing first trap"); }
      catch(const Monstey_GadbPublicationBoundaryFault &e) { Monstey_Fault(e,Monstey_GadbPublicationBoundaryKind::invariant_trap,0x140819113ULL,0x1410ED7D7ULL,0,4,0,0x140819104ULL,0x141F65A38ULL); }
      Monstey_Check(f.read(f.owner)==0 && f.read(f.db+0x20)==0xF00D && f.read(f.db+0x4C,4)==7,"trap exact prior move prefix");
    }},
    {"main-second-trap-equal-owner-returning-provider", [] {
      Monstey_Fixture f; f.init_main(); f.write(f.q+0xA8,f.db+0x20); f.write(f.db+0x20,0xABAB);
      f.services.trap=[&](Monstey_GadbBinderMemory,const Monstey_GadbPublicationTrapCall &c) {
        Monstey_Check(c.call_site==0x1410ED7E9ULL && c.trap_entry==0x140819104ULL && c.zero_store_site==0x140819113ULL,"trap narrow metadata");
        Monstey_Check(c.message_identity==0x141F65A38ULL && c.address==0 && c.value==0,"trap message/store metadata");
        Monstey_Raw(c.incoming,7,0x12345678);
      };
      try { (void)f.publish(); throw Monstey_TestError("returning trap continued"); }
      catch(const Monstey_GadbPublicationBoundaryFault &e) { Monstey_Fault(e,Monstey_GadbPublicationBoundaryKind::trap_returned,0x140819113ULL,0x1410ED7E9ULL,0,4,0,0x140819104ULL,0x141F65A38ULL); }
    }},
    {"main-trap-provider-throws-unchanged", [] {
      Monstey_Fixture f; f.init_main(); f.write(f.db+0x20,3);
      f.services.trap=[](Monstey_GadbBinderMemory,const Monstey_GadbPublicationTrapCall &c) {
        Monstey_Raw(c.incoming,3,0xF00D); throw Monstey_ProviderError{99};
      };
      try { (void)f.publish(); throw Monstey_TestError("missing trap provider exception"); }
      catch(const Monstey_ProviderError &e) { Monstey_Check(e.marker==99 && f.read(f.db+0x20)==0xF00D,"trap exception and prefix"); }
    }},
    {"main-cached-final-owner-cell-despite-home-mutation", [] {
      Monstey_Fixture f; f.init_main(); f.bind_atomic(); f.init_control(1,2); f.write(f.db+0x60,f.control);
      const auto second=f.control+0x40; f.write(second,f.vt); f.write(second+8,1,4); f.write(second+12,2,4);
      f.write(f.pair+0x20,0); f.write(f.pair+0x28,second); f.write(f.owner+0x20,0xBBBB);
      f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationVirtualCall &c) {
        if(c.this_identity==f.control) Monstey_Write(m,f.q+0x78,f.pair+0x20);
        else Monstey_Write(m,f.q+0xA8,f.owner+0x20);
        return Monstey_GadbBufferedResult{0xCC,0xDD};
      };
      Monstey_Raw(f.publish(),2,0xDD);
      Monstey_Check(f.read(f.owner)==0 && f.read(f.owner+0x20)==0xBBBB,"final cached owner identity");
    }},
    {"callable-persistent-mutable-state", [] {
      Monstey_Fixture f; f.init_control(4,4);
      f.services.atomic32=[&,calls=0u](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c) mutable {
        ++calls; Monstey_Check(calls==static_cast<unsigned>(5-f.read(f.control+8,4)),"persistent mutable closure state"); return f.atomic(m,c);
      };
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{8,9},f.services),4,9);
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{8,9},f.services),3,9);
    }},
    {"atomic-self-replacement-retirement-before-slot0", [] {
      Monstey_Fixture f; f.init_control(1,2); auto token=std::make_shared<int>(7); std::weak_ptr<int> weak=token;
      f.services.atomic32=[&,keep=token](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c) {
        Monstey_Check(*keep==7,"active atomic target alive");
        const auto old=f.atomic(m,c); f.services.atomic32=[&](Monstey_GadbBinderMemory mm,const Monstey_GadbPublicationAtomicCall &cc){ return f.atomic(mm,cc); };
        Monstey_Check(!weak.expired(),"active atomic target survives self replacement"); return old;
      }; token.reset();
      f.services.virtual_call=[&](Monstey_GadbBinderMemory,const Monstey_GadbPublicationVirtualCall &) {
        Monstey_Check(weak.expired(),"retired atomic target destroyed before virtual call"); return Monstey_GadbBufferedResult{4,5};
      };
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{8,9},f.services),2,5);
    }},
    {"virtual-self-reset-retirement-before-secondary", [] {
      Monstey_Fixture f; f.init_control(1,2); auto token=std::make_shared<int>(8); std::weak_ptr<int> weak=token;
      f.services.atomic32=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationAtomicCall &c) {
        if(c.native_site==0x1403DD713ULL) Monstey_Check(weak.expired(),"retired virtual target destroyed before secondary atomic");
        return f.atomic(m,c);
      };
      f.services.virtual_call=[&,keep=token](Monstey_GadbBinderMemory,const Monstey_GadbPublicationVirtualCall &) {
        Monstey_Check(*keep==8,"active virtual target alive"); f.services.virtual_call={};
        Monstey_Check(!weak.expired(),"active virtual target survives self reset"); return Monstey_GadbBufferedResult{4,5};
      }; token.reset();
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{8,9},f.services),2,5);
    }},
    {"reentrant-call-uses-current-target", [] {
      Monstey_Fixture f; f.init_control(1,2); f.bind_atomic(); const auto other=f.control+0x40; f.write(other+8,2,4);
      f.services.virtual_call=[&](Monstey_GadbBinderMemory m,const Monstey_GadbPublicationVirtualCall &) {
        const auto r=Monstey_GadbReleasePublicationPrimary(m,other,{0x11,0x22},f.services);
        Monstey_Raw(r,2,0x22); return Monstey_GadbBufferedResult{0x33,0x44};
      };
      Monstey_Raw(Monstey_GadbReleasePublicationPrimary(f.memory(),f.control,{8,9},f.services),2,0x44);
      Monstey_Check(f.read(other+8,4)==1,"reentrant effect retained");
    }}
  };
}
} // namespace
int main() {
  unsigned passed=0, failed=0; const auto cases=Monstey_Cases();
  std::cout << "{\"schema\":\"Monstey.SoWPublicationOwnerTests/1\",\"cases\":[";
  bool first=true;
  for(const auto &test:cases) {
    if(!first) std::cout << ','; first=false;
    std::cout << "{\"name\":\"" << test.first << "\",\"pass\":";
    try { test.second(); ++passed; std::cout << "true}"; }
    catch(const std::exception &e) { ++failed; std::cout << "false,\"error\":\"" << e.what() << "\"}"; }
    catch(...) { ++failed; std::cout << "false,\"error\":\"unexpected provider or unknown exception\"}"; }
  }
  std::cout << "],\"passed\":" << passed << ",\"failed\":" << failed << ",\"checks\":" << Monstey_Checks << "}\n";
  return failed==0?0:1;
}
