#include "Monstey_GadbAudioNextKey.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace Monstey;
namespace {
using U64=std::uint64_t; using U32=std::uint32_t;
U64 Monstey_Cases=0,Monstey_Checks=0,Monstey_Groups=0,Monstey_RangeCases=0;
void Check(bool ok,const char *why){++Monstey_Checks;if(!ok)throw std::runtime_error(why);}
struct Read {U64 address;std::size_t width;};
struct Fixture {
    static constexpr U64 base=0x10000, wrapper=base+0x20, ac=base+0x100,
      kc=base+0x180, af=base+0x1000,kf=base+0x2000,ar=base+0x3000,
      kr=base+0x3100,ad=base+0x4000,kd=base+0x5000,av=base+0x6000,
      kv=base+0x7000,ao=base+0x8000,ko=base+0x8100,ag=base+0x8200,
      kg=base+0x8300,at=base+0x9000,kt=base+0xb000,aa=base+0xd000,
      ka=base+0xe000,field=af+20*72;
    static constexpr U32 ah=0x223477a3,kh=0x4ae9f81e;
    static constexpr U64 answer=0xfedcba9876543210;
    std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x12000,0xa5);
    void Put(U64 address,U64 value,std::size_t width){
        Check(address>=base && address-base+width<=bytes.size(),"fixture write range");
        for(std::size_t i=0;i<width;++i)bytes[static_cast<std::size_t>(address-base)+i]=static_cast<std::uint8_t>(value>>(8*i));
    }
    Monstey_GadbBinderMemory Memory(){return {base,bytes};}
    U64 Run(){return Monstey_GadbReadAudioNextKey(Memory(),wrapper,ac,kc);}
    void Decl(bool key,U32 index,U32 h,U32 type,U32 count,U32 flags){
        const U64 row=(key?kt:at)+static_cast<U64>(index)*72;
        Put(row+32,h,4);Put(row+36,type,4);Put(row+40,count,4);Put(row+44,flags,4);
    }
    void Configure(std::int32_t as=2,std::int32_t ks=2){
        Put(ac,af,8);Put(kc,kf,8);Put(wrapper,ar,8);
        Put(field+32,ah,4);Put(field+48,std::bit_cast<U32>(as),4);Put(field+64,7,1);
        Put(kf+32,kh,4);Put(kf+48,std::bit_cast<U32>(ks),4);Put(kf+64,9,1);
        Put(ar,1,1);Put(ar+1,1,1);Put(ar+8,ad,8);Put(ar+16,av,8);Put(ar+24,ao,8);
        Put(kr,1,1);Put(kr+1,1,1);Put(kr+8,kd,8);Put(kr+16,kv,8);Put(kr+24,ko,8);
        for(auto [d,index]:{std::pair{ad,7u},std::pair{kd,9u}}){
            Put(d+index*16+10,0,1);Put(d+index*16+12,1,2);Put(d+index*16+14,index==7?13:17,2);
        }
        Put(av+13*8,kr,8);Put(kv+17*8,answer,8);
        for(auto [d,h]:{std::pair{ad,ah},std::pair{kd,kh}}){
            Put(d,h,4);Put(d+10,0xca,1);Put(d+12,1,2);Put(d+14,d==ad?13:17,2);
        }
        Put(ao+32,ag,8);Put(ko+32,kg,8);Put(ag,at,8);Put(kg,kt,8);
        Put(ag+8,1,4);Put(kg+8,1,4);Decl(false,0,ah,10,1,0);Decl(true,0,kh,10,1,0);
    }
    // Runtime rows are sorted by logical hash; indirect physical metadata is
    // intentionally elsewhere, exercising remapping rather than ordinal reads.
    void Rows(bool key,U32 count,U32 selected,bool indirect){
        const auto d=key?kd:ad;const auto r=key?kr:ar;const auto f=key?kf:field;
        Put(r,count,1);Put(r+1,indirect?0:1,1);
        for(U32 i=0;i<count;++i){
            const U32 h=0x80000000u+i*0x10001u;
            Put(d+16ull*i+(indirect?4:0),h,4);
            if(indirect)Put(d+16ull*i+9,(i*73u+11u)%256u,1);
        }
        Put(f+32,0x80000000u+selected*0x10001u,4);
        const U32 p=indirect?(selected*73u+11u)%256u:selected;
        Put(d+16ull*p+10,0xca,1);Put(d+16ull*p+12,1,2);Put(d+16ull*p+14,key?17:13,2);
    }
};
template<class F> void Range(F call,U64 expected){
    ++Monstey_Cases;++Monstey_RangeCases;bool caught=false;
    try{(void)call();}catch(const Monstey_GadbBinderFault &e){
        caught=true;Check(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"fault kind");
        Check(e.address==expected,"fault address/read order");
    }Check(caught,"missing consumed fault");
}
void TestCached(){
    ++Monstey_Groups;
    for(const auto as:{INT32_MIN,-1,0,1,2,3,INT32_MAX})
      for(const auto ks:{INT32_MIN,-1,0,1,2,3,INT32_MAX}){
        Fixture f;f.Configure(as,ks);const auto before=f.bytes;++Monstey_Cases;
        Check(f.Run()==f.answer,"cached/slow mix answer");Check(f.bytes==before,"normal memory modified");
    }
    for(bool key:{false,true})for(U32 count:{0u,1u,65535u}){
        Fixture f;f.Configure();const auto d=key?f.kd+9*16:f.ad+7*16;
        f.Put(d+12,count,2);f.Put(d+10,0xff,1);++Monstey_Cases;
        Check(f.Run()==(count==0?0:f.answer),"cached count/type behavior");
    }
    for(const U64 raw:{0ull,1ull,0x8000000000000000ull,0xffffffffffffffffull}){
        Fixture f;f.Configure();f.Put(f.kv+17*8,raw,8);++Monstey_Cases;
        Check(f.Run()==raw,"returned raw word was interpreted");
    }
}
void TestLookup(){
    ++Monstey_Groups;
    for(bool key:{false,true})for(bool indirect:{false,true})
      for(U32 n=1;n<=255;++n)for(U32 pos:{0u,n/2,n-1}){
        Fixture f;f.Configure(key?2:0,key?0:2);f.Rows(key,n,pos,indirect);
        ++Monstey_Cases;const auto got=f.Run();if(got!=f.answer)std::cerr<<"lookup key="<<key<<" indirect="<<indirect<<" n="<<n<<" pos="<<pos<<" got="<<got<<" expected="<<f.answer<<"\n";Check(got==f.answer,"unsigned sorted lookup/remap");
    }
    for(bool key:{false,true})for(bool indirect:{false,true}){
        Fixture f;f.Configure(key?2:0,key?0:2);f.Rows(key,7,3,indirect);
        const auto r=key?f.kr:f.ar,d=key?f.kd:f.ad,field=key?f.kf:f.field;
        for(U32 i=0;i<7;++i)f.Put(d+16ull*i+(indirect?4:0),0xf1234567,4);
        f.Put(field+32,0xf1234567,4);
        const U32 p=indirect?(3*73+11)%256:3;
        f.Put(d+16ull*p+14,key?19:21,2);
        f.Put((key?f.kv:f.av)+8ull*(key?19:21),key?0x0123456789abcdef:f.kr,8);
        ++Monstey_Cases;Check(f.Run()==(key?0x0123456789abcdef:f.answer),"duplicate midpoint selection");
        f.Put(r,0,1);++Monstey_Cases;Check(f.Run()==0,"empty slow search");
    }
    for(bool key:{false,true})for(U32 type=0;type<256;++type){
        Fixture f;f.Configure(key?2:0,key?0:2);f.Put((key?f.kd:f.ad)+10,type,1);
        ++Monstey_Cases;Check(f.Run()==((type&63u)==10?f.answer:0),"type10 mask");
    }
}
U32 PrefixCost(U32 type,U32 count,U32 flags){
    if(flags&0x280u)return 0;
    U32 width=count==1?(type==1?5u:type==8?12u:type==9?16u:
        (type>=2&&type<=17?8u:0u)):16u;
    return (width+7u)/8u*8u;
}
void TestGenerated(){
    ++Monstey_Groups;
    for(bool key:{false,true})for(bool indirect:{false,true})
      for(U32 type:{0u,1u,2u,7u,8u,9u,10u,17u,18u,UINT32_MAX})
      for(U32 count:{0u,1u,2u,UINT32_MAX})for(U32 flags:{0u,0x80u,0x200u,0x280u,0x40u}){
        Fixture f;f.Configure(key?2:0,key?0:2);
        const auto r=key?f.kr:f.ar,d=key?f.kd:f.ad,c=key?f.kg:f.ag,v=key?f.kv:f.av;
        const U32 h=key?f.kh:f.ah;f.Put(r+1,indirect?0x10:0x11,1);
        if(indirect){f.Put(d+4,h,4);f.Put(d+9,0,1);}
        f.Put(c+8,4,4);f.Decl(key,0,h+1,type,count,flags);
        f.Decl(key,1,h+2,8,1,0);f.Decl(key,2,h+3,1,1,0);
        f.Decl(key,3,h,0,0,0);const U32 offset=PrefixCost(type,count,flags)+16+8;
        f.Put(v+offset,key?f.answer:f.kr,8);
        ++Monstey_Cases;Check(f.Run()==f.answer,"generated size/alignment/fixed count0");
    }
    for(bool key:{false,true})for(U32 dynamicCount:{1u,2u,0x80000000u,UINT32_MAX}){
        Fixture f;f.Configure(key?2:0,key?0:2);const auto r=key?f.kr:f.ar;
        f.Put(r+1,0x11,1);f.Decl(key,0,key?f.kh:f.ah,9,UINT32_MAX,0);
        const auto v=key?f.kv:f.av, a=key?f.ka:f.aa;
        f.Put(v,a,8);f.Put(v+8,dynamicCount,4);f.Put(a,key?f.answer:f.kr,8);
        ++Monstey_Cases;Check(f.Run()==f.answer,"dynamic first element unsigned count");
        f.Put(v+8,0,4);bool caught=false;++Monstey_Cases;
        try{(void)f.Run();}catch(const Monstey_GadbScalarFault&e){caught=true;
            Check(e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion&&e.address==v+8,"dynamic assertion details");}
        Check(caught,"empty dynamic assertion missing");
    }
}
void TestEarly(){
    ++Monstey_Groups;
    for(bool key:{false,true})for(U32 gate=0;gate<7;++gate){
        Fixture f;f.Configure(key?2:0,key?0:2);const auto r=key?f.kr:f.ar,d=key?f.kd:f.ad;
        const auto field=key?f.kf:f.field;f.Put(r+1,0x11,1);
        if(gate==0)f.Put(r,0,1);
        if(gate==1)f.Put(field+32,0xffffffff,4);
        if(gate==2)f.Put(d+10,2,1);
        if(gate==3)f.Put(d+12,0,2);
        if(gate==4)f.Put(r+24,0,8);
        if(gate==5)f.Put((key?f.ko:f.ao)+32,0,8);
        if(gate==6)f.Put((key?f.kg:f.ag)+8,0,4);
        ++Monstey_Cases;Check(f.Run()==0,"missing/zero generated/slow gate");
    }
    Fixture f;f.Configure();f.Put(f.wrapper,0,8);++Monstey_Cases;Check(f.Run()==0,"null action");
    f.Put(f.kc,0xfffffff000000000,8);Range([&]{return f.Run();},0xfffffff000000030);
    f.Configure();f.Put(f.av+13*8,0,8);++Monstey_Cases;Check(f.Run()==0,"null raw child");
}
// Independent consumed-range oracle: explicit address/width lists from the
// selected native instruction trace, not a second implementation of the getter.
void Prefixes(Fixture &f,const std::vector<Read>& trace,U64 answer){
    const auto before=f.bytes;
    for(std::size_t end=0;end<=f.bytes.size();++end){
        U64 bad=0;bool invalid=false;
        for(const auto &read:trace)if(read.address<f.base||read.address-f.base>end||read.width>end-static_cast<std::size_t>(read.address-f.base)){
            bad=read.address;invalid=true;break;
        }
        const auto call=[&]{return Monstey_GadbReadAudioNextKey({f.base,std::span(f.bytes).first(end)},f.wrapper,f.ac,f.kc);};
        if(invalid)Range(call,bad);else{++Monstey_Cases;Check(call()==answer,"prefix successful result");}
    }
    Check(f.bytes==before,"fault/success path modified source");
}
void TestReadOrder(){
    ++Monstey_Groups;Fixture f;f.Configure();
    std::vector<Read> cached={{f.ac,8},{f.wrapper,8},{f.field+48,4},{f.field+64,1},{f.ar+8,8},
      {f.ad+7*16+12,2},{f.ad+7*16+14,2},{f.ar+16,8},{f.av+13*8,8},
      {f.kc,8},{f.kf+48,4},{f.kf+64,1},{f.kr+8,8},{f.kd+9*16+12,2},
      {f.kd+9*16+14,2},{f.kr+16,8},{f.kv+17*8,8}};
    Prefixes(f,cached,f.answer);
    f.Configure(0,0);
    std::vector<Read> slow={{f.ac,8},{f.wrapper,8},{f.field+48,4},{f.ar+1,1},{f.ar+8,8},{f.ar,1},
      {f.field+32,4},{f.ad,4},{f.ad+10,1},{f.ad+12,2},{f.ad+14,2},{f.ar+16,8},{f.av+13*8,8},
      {f.kc,8},{f.kf+48,4},{f.kr+1,1},{f.kr+8,8},{f.kr,1},{f.kf+32,4},{f.kd,4},
      {f.kd+10,1},{f.kd+12,2},{f.kd+14,2},{f.kr+16,8},{f.kv+17*8,8}};
    Prefixes(f,slow,f.answer);
    // A one-row generated selection has no prefix-size rows. This verifies the
    // category/class/count/hash/table and values-before-declaration-count order.
    f.Configure(0,2);f.Put(f.ar+1,0x11,1);f.Put(f.av,f.kr,8);
    std::vector<Read> generated={{f.ac,8},{f.wrapper,8},{f.field+48,4},{f.ar+1,1},{f.ar+8,8},
      {f.ar,1},{f.field+32,4},{f.ad,4},{f.ad+10,1},{f.ad+12,2},{f.ar+24,8},{f.ao+32,8},
      {f.ag+8,4},{f.field+32,4},{f.ag,8},{f.at+32,4},{f.at+32,4},{f.ar+16,8},{f.at+40,4},{f.av,8},
      {f.kc,8},{f.kf+48,4},{f.kf+64,1},{f.kr+8,8},{f.kd+9*16+12,2},
      {f.kd+9*16+14,2},{f.kr+16,8},{f.kv+17*8,8}};
    Prefixes(f,generated,f.answer);
}
void TestSpecificFaults(){
    ++Monstey_Groups;Fixture f;f.Configure();
    Range([&]{return Monstey_GadbReadAudioNextKey(f.Memory(),0,0,f.kc);},0);
    Range([&]{return Monstey_GadbReadAudioNextKey(f.Memory(),0,f.ac,f.kc);},0);
    f.Put(f.wrapper,0,8);Range([&]{return Monstey_GadbReadAudioNextKey(f.Memory(),f.wrapper,f.ac,0);},0);
    f.Configure(0,2);f.Put(f.ar+8,UINT64_MAX,8);f.Put(f.ar,0,1);++Monstey_Cases;Check(f.Run()==0,"empty table pointer eagerly used");
    f.Put(f.ar,1,1);Range([&]{return f.Run();},UINT64_MAX);
    f.Configure(0,2);f.Put(f.ar+8,UINT64_MAX-8,8);f.Put(f.ar+1,0,1);
    Range([&]{return f.Run();},UINT64_MAX-4);
    f.Configure();f.Put(f.ad+7*16+12,0,2);f.Put(f.ar+16,0,8);++Monstey_Cases;Check(f.Run()==0,"zero count used missing values");
}
void TestStateIsolation(){
    ++Monstey_Groups;
    for(const auto as:{INT32_MIN,-1,0,1,2,3,INT32_MAX})
      for(const auto ks:{INT32_MIN,-1,0,1,2,3,INT32_MAX}){
        Fixture f;f.Configure(as,ks);
        if(as<2)f.Put(f.ad+7*16+12,0,2);else f.Put(f.ad+10,0,1);
        if(ks<2)f.Put(f.kd+9*16+12,0,2);else f.Put(f.kd+10,0,1);
        ++Monstey_Cases;Check(f.Run()==f.answer,"signed state branch must select its only valid route");
    }
}
void TestWrapAndAliases(){
    ++Monstey_Groups;
    for(bool key:{false,true})for(U32 physical=0;physical<256;++physical){
        Fixture f;f.Configure();const auto field=key?f.kf:f.field;
        const auto d=key?f.kd:f.ad;f.Put(field+64,physical,1);
        f.Put(d+16ull*physical+12,1,2);f.Put(d+16ull*physical+14,65535,2);
        const auto valueAddress=key?f.kv+17*8:f.av+13*8;
        f.Put((key?f.kr:f.ar)+16,valueAddress-8ull*65535,8);
        const auto before=f.bytes;++Monstey_Cases;
        Check(f.Run()==f.answer,"cache byte/u16 word/wrapped virtual address");
        Check(f.bytes==before,"wrapped source modified");
    }
    for(bool key:{false,true})for(U32 pos:{0u,1u,2u}){
        Fixture f;f.Configure(key?2:0,key?0:2);const auto d=key?f.kd:f.ad;
        const auto field=key?f.kf:f.field;const auto r=key?f.kr:f.ar;
        f.Put(r,3,1);constexpr std::array<U32,3> hashes={0u,0x80000000u,UINT32_MAX};
        for(U32 i=0;i<3;++i){f.Put(d+16ull*i,hashes[i],4);f.Put(d+16ull*i+10,10,1);
            f.Put(d+16ull*i+12,1,2);f.Put(d+16ull*i+14,key?17:13,2);}
        f.Put(field+32,hashes[pos],4);++Monstey_Cases;
        Check(f.Run()==f.answer,"mixed signedness hash ordering");
    }
    // The final word may borrow bytes of the class descriptor. It is returned
    // raw and does not trigger an invented dereference or string operation.
    Fixture f;f.Configure();f.Put(f.kr+16,f.kc-17*8,8);++Monstey_Cases;
    Check(f.Run()==f.kf,"metadata/value alias raw result");
    f.Configure(0,2);const U64 tail=f.base+f.bytes.size()-2;
    f.Put(f.wrapper,tail,8);f.Put(tail,0,1);f.Put(tail+1,1,1);
    Range([&]{return f.Run();},tail+8);
    Range([&]{return Monstey_GadbReadAudioNextKey(f.Memory(),2,1,f.kc);},1);
}
} // namespace
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("Usage: NEXTKEY_TEST NEW_REPORT");
    TestCached();TestLookup();TestGenerated();TestEarly();TestReadOrder();TestSpecificFaults();TestWrapAndAliases();TestStateIsolation();
    const std::string json="{\"status\":\"PASS\",\"groups\":"+std::to_string(Monstey_Groups)+
      ",\"cases\":"+std::to_string(Monstey_Cases)+",\"checks\":"+std::to_string(Monstey_Checks)+
      ",\"consumed_range_cases\":"+std::to_string(Monstey_RangeCases)+"}\n";
    std::ofstream out(argv[1],std::ios::binary);out<<json;Check(static_cast<bool>(out),"write report");std::cout<<json;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
