#include "Monstey_GadbScalarFloat.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Monstey;
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
U64 Monstey_Checks = 0, Monstey_Cases = 0, Monstey_Groups = 0;
void Monstey_Check(bool condition, const char* message) {
    ++Monstey_Checks;
    if (!condition) throw std::runtime_error(message);
}
struct Monstey_Fixture {
    static constexpr U64 base = 0x10000, record = base + 0x20,
        key = base + 0x80, fallback = base + 0x90, descriptor = base + 0x100,
        owner = base + 0x180, cl = base + 0x200, declarations = base + 0x300,
        values = base + 0x2000, dynamic = base + 0x4000, output = base + 0x5000;
    static constexpr U32 hash = 0x90200123;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x10000);
    Monstey_GadbBinderMemory Memory() { return {base, bytes}; }
    void Put(U64 address, U64 value, std::size_t width) {
        const auto offset = static_cast<std::size_t>(address - base);
        Monstey_Check(address >= base && offset + width <= bytes.size(), "fixture range");
        for (std::size_t i=0; i<width; ++i)
            bytes[offset+i] = static_cast<std::uint8_t>(value >> (8*i));
    }
    U64 Get(U64 address, std::size_t width) const {
        const auto offset = static_cast<std::size_t>(address-base); U64 value=0;
        for(std::size_t i=0;i<width;++i) value |= static_cast<U64>(bytes[offset+i])<<(8*i);
        return value;
    }
    void Configure(bool indirect=false, bool generated=false) {
        Put(record,1,1); Put(record+1,(indirect?0u:1u)|(generated?0x10u:0u),1);
        Put(record+8,descriptor,8); Put(record+16,values,8); Put(record+24,owner,8);
        Put(key,hash,4); Put(fallback,0x7fa12345,4);
        Put(descriptor+(indirect?4u:0u),hash,4);
        Put(descriptor+9,0,1); Put(descriptor+10,2,1);
        Put(descriptor+12,8,2); Put(descriptor+14,3,2);
        Put(owner+32,cl,8); Put(cl,declarations,8); Put(cl+8,1,4);
        Decl(0,hash,2,1,0);
    }
    void Decl(U32 i,U32 h,U32 type,U32 count,U32 flags) {
        const auto address=declarations+static_cast<U64>(i)*72;
        Put(address+32,h,4); Put(address+36,type,4);
        Put(address+40,count,4); Put(address+44,flags,4);
    }
};
template<class F> void Monstey_RangeFault(F call,U64 address) {
    bool found=false;
    try{call();}catch(const Monstey_GadbBinderFault& e){
        found=e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable &&
            e.address==address;
    }
    Monstey_Check(found,"consumed range fault/order");
}
template<class F> void Monstey_IndexFault(F call,U64 address) {
    bool found=false;
    try{call();}catch(const Monstey_GadbScalarFault& e){
        found=e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion &&
            e.address==address;
    }
    Monstey_Check(found,"dynamic assertion boundary");
}
void Monstey_SizeTable() {
    ++Monstey_Groups;
    for(U32 t=0;t<40;++t) for(U32 count:{0u,1u,2u,UINT32_MAX})
        for(U32 flags=0;flags<1024;++flags){
            ++Monstey_Cases;
            U32 expected=0;
            if((flags&0x280u)==0){
                if(count!=1||t==9)expected=16;
                else if(t==8)expected=12;
                else if(t==1)expected=5;
                else if(t>=2&&t<=17)expected=8;
            }
            Monstey_Check(Monstey_GadbGeneratedFieldSize(t,count,flags)==expected,"native size table");
        }
    Monstey_Check(Monstey_GadbGeneratedFieldSize(UINT32_MAX,1,0)==0,"unknown type1");
    Monstey_Check(Monstey_GadbGeneratedFieldSize(UINT32_MAX,0,0)==16,"unknown type0");
}
void Monstey_ClassOrder() {
    ++Monstey_Groups;
    Monstey_Fixture f; f.Configure();
    for(U32 count=0;count<128;++count) {
        f.Put(f.cl+8,count,4); U32 expected_offset=0;
        for(U32 i=0;i<count;++i){
            const auto t=i%19;const auto n=i%4;const auto flags=(i%11==0)?0x200u:0u;
            f.Decl(i,10000-i,t,n,flags);
        }
        for(U32 i=0;i<count;++i){
            ++Monstey_Cases;
            Monstey_Check(Monstey_GadbFindGeneratedField(f.Memory(),f.cl,10000-i)==f.declarations+72ull*i,"declaration first order");
            Monstey_Check(Monstey_GadbGeneratedFieldOffset(f.Memory(),f.cl,10000-i)==expected_offset,"aligned prefix offset");
            const auto size=Monstey_GadbGeneratedFieldSize(i%19,i%4,(i%11==0)?0x200u:0u);
            expected_offset=(expected_offset+size+7)&~7u;
        }
        Monstey_Check(Monstey_GadbFindGeneratedField(f.Memory(),f.cl,999999)==0,"missing generated field");
        Monstey_Check(Monstey_GadbGeneratedFieldOffset(f.Memory(),f.cl,999999)==UINT32_MAX,"missing offset sentinel");
    }
    f.Put(f.cl+8,2,4);f.Decl(0,9,2,1,0);f.Decl(1,9,8,1,0);
    Monstey_Check(Monstey_GadbFindGeneratedField(f.Memory(),f.cl,9)==f.declarations,"duplicate first identity");
    Monstey_Check(Monstey_GadbGeneratedFieldOffset(f.Memory(),f.cl,9)==0,"duplicate first offset");
}
void Monstey_RawBits() {
    ++Monstey_Groups;
    Monstey_Fixture f;
    for(bool indirect:{false,true}) {
        f.Configure(indirect,false);
        U32 random=0x7f800001;
        for(U32 i=0;i<10000;++i){
            ++Monstey_Cases; random=random*1664525u+1013904223u;
            const auto element=i%8;
            f.Put(f.values+8ull*(3+element),0xa5a5a5a500000000ull|random,8);
            f.Put(f.descriptor+10,2u|((i&3u)<<6),1);
            Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,element,UINT64_MAX)==random,"raw payload/stride/masked type");
        }
    }
    for(U32 bits:{0u,0x80000000u,0x7f800000u,0xff800000u,0x7f800001u,0x7fc00001u,UINT32_MAX}){
        ++Monstey_Cases;f.Put(f.values+24,bits,8);
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,0,UINT64_MAX)==bits,"special IEEE payload");
        f.Put(f.fallback,bits,4);
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),0,UINT64_MAX,UINT32_MAX,f.fallback)==bits,"default IEEE payload");
    }
}
void Monstey_RawBounds() {
    ++Monstey_Groups; Monstey_Fixture f; f.Configure();
    for(U32 type=0;type<256;++type){
        ++Monstey_Cases;f.Put(f.descriptor+10,type,1);
        f.Put(f.values+24,0x3f800000,4);
        const auto expected=(type&63u)==2?0x3f800000u:0x7fa12345u;
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,0,f.fallback)==expected,"type mask");
    }
    f.Put(f.descriptor+10,2,1);
    for(U32 n:{0u,1u,7u,65535u}){
        ++Monstey_Cases;f.Put(f.descriptor+12,n,2);
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,n,f.fallback)==0x7fa12345,"unsigned count equality");
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,UINT32_MAX,f.fallback)==0x7fa12345,"unsigned index max");
    }
    f.Put(f.record,0,1);
    Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,UINT64_MAX,0,f.fallback)==0x7fa12345,"zero descriptor count skips field");
}
void Monstey_GeneratedInline() {
    ++Monstey_Groups;Monstey_Fixture f;f.Configure(false,true);
    f.Put(f.cl+8,4,4);
    f.Decl(0,8,1,1,0);f.Decl(1,9,8,1,0);f.Decl(2,10,9,1,0);f.Decl(3,f.hash,2,1,0);
    for(U32 type:{2u,8u,9u,10u,UINT32_MAX}){
        f.Decl(3,f.hash,type,1,0);
        for(U32 i=0;i<8;++i){
            ++Monstey_Cases;const auto index=(type==8||type==9)?i*2:i;
            const auto value=0x1122334400000000ull|static_cast<U64>(0x80000000u+i);
            f.Put(f.values+40+8ull*index,value,8);
            const auto result=Monstey_GadbEvaluateGeneratedWord(f.Memory(),f.cl,f.key,f.record,i);
            Monstey_Check(result.found&&result.value==value,"generated inline and double index");
            Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,i,UINT64_MAX)==static_cast<U32>(value),"complete generated float route");
        }
    }
    f.Decl(3,f.hash,8,1,0);f.Put(f.values+40,0x123456789abcdef0,8);
    const auto wrapped=Monstey_GadbEvaluateGeneratedWord(f.Memory(),f.cl,f.key,f.record,0x80000000);
    Monstey_Check(wrapped.found&&wrapped.value==0x123456789abcdef0,"double U32 wrapping");
}
void Monstey_GeneratedDynamic() {
    ++Monstey_Groups;Monstey_Fixture f;f.Configure(false,true);
    f.Decl(0,f.hash,2,UINT32_MAX,0);f.Put(f.values,f.dynamic,8);f.Put(f.values+8,8,4);
    for(U32 i=0;i<8;++i){
        ++Monstey_Cases;const auto value=0x7f800001ull+i;
        f.Put(f.dynamic+8ull*i,value,8);
        Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,i,UINT64_MAX)==value,"dynamic float");
    }
    f.Put(f.output,0xfeeddead12345678,8);
    Monstey_IndexFault([&]{Monstey_GadbFetchGeneratedWord(f.Memory(),f.cl,f.key,f.record,8,f.output);},f.values+8);
    Monstey_Check(f.Get(f.output,8)==0xfeeddead12345678,"assert output unchanged");
    f.Put(f.values,UINT64_MAX,8);f.Put(f.values+8,0,4);
    Monstey_IndexFault([&]{Monstey_GadbEvaluateGeneratedWord(f.Memory(),f.cl,f.key,f.record,0);},f.values+8);
    f.Decl(0,f.hash,9,UINT32_MAX,0);f.Put(f.values,f.dynamic,8);f.Put(f.values+8,3,4);
    f.Put(f.dynamic+16,0xabcdef1234567890,8);
    Monstey_Check(Monstey_GadbEvaluateGeneratedWord(f.Memory(),f.cl,f.key,f.record,1).value==0xabcdef1234567890,"dynamic doubled index");
    Monstey_IndexFault([&]{Monstey_GadbEvaluateGeneratedWord(f.Memory(),f.cl,f.key,f.record,2);},f.values+8);
}
void Monstey_Fallbacks() {
    ++Monstey_Groups;Monstey_Fixture f;f.Configure(false,true);
    f.Put(f.record+24,0,8);
    Monstey_Check(Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,0,f.fallback)==0x7fa12345,"missing generated owner");
    f.Put(f.record+24,f.owner,8);f.Put(f.owner+32,0,8);
    Monstey_Check(Monstey_GadbReadGeneratedFloatBits(f.Memory(),f.record,UINT64_MAX,0,f.fallback)==0x7fa12345,"missing generated class");
    f.Put(f.owner+32,f.cl,8);f.Put(f.cl+8,0,4);
    Monstey_Check(Monstey_GadbReadGeneratedFloatBits(f.Memory(),f.record,f.key,0,f.fallback)==0x7fa12345,"missing generated field");
    Monstey_Check(Monstey_GadbFetchGeneratedWord(f.Memory(),f.cl,f.key,UINT64_MAX,0,UINT64_MAX)==0,"missing skips record/output");
    Monstey_Check(Monstey_GadbReadGeneratedFloatBits(f.Memory(),0,UINT64_MAX,UINT32_MAX,f.fallback)==0x7fa12345,"null generated record");
    Monstey_Cases+=5;
}
void Monstey_LazyReads() {
    ++Monstey_Groups;Monstey_Fixture f;f.Configure();
    auto m=f.Memory();m.bytes=m.bytes.first(static_cast<std::size_t>(f.descriptor+11-f.base));
    f.Put(f.descriptor+10,3,1);
    Monstey_Check(Monstey_GadbReadScalarFloatBits(m,f.record,f.key,0,f.fallback)==0x7fa12345,"wrong type does not read count");
    f.Put(f.descriptor+10,2,1);
    Monstey_RangeFault([&]{Monstey_GadbReadScalarFloatBits(m,f.record,f.key,0,f.fallback);},f.descriptor+12);
    m=f.Memory();m.bytes=m.bytes.first(static_cast<std::size_t>(f.descriptor+14-f.base));
    f.Put(f.descriptor+12,0,2);
    Monstey_Check(Monstey_GadbReadScalarFloatBits(m,f.record,f.key,0,f.fallback)==0x7fa12345,"empty does not read offset");
    f.Put(f.descriptor+12,1,2);
    Monstey_RangeFault([&]{Monstey_GadbReadScalarFloatBits(m,f.record,f.key,0,f.fallback);},f.descriptor+14);
    Monstey_RangeFault([&]{Monstey_GadbReadScalarFloatBits(f.Memory(),0,0,0,UINT64_MAX);},UINT64_MAX);
    Monstey_RangeFault([&]{Monstey_GadbEvaluateGeneratedWord(f.Memory(),UINT64_MAX,UINT64_MAX,f.record,0);},UINT64_MAX);
    f.Put(f.cl+8,0,4);m=f.Memory();m.bytes=m.bytes.first(static_cast<std::size_t>(f.cl+12-f.base));
    f.Put(f.cl,UINT64_MAX,8);
    Monstey_Check(Monstey_GadbFindGeneratedField(m,f.cl,9)==0,"empty class skips table");
    Monstey_Check(Monstey_GadbGeneratedFieldOffset(m,f.cl,9)==UINT32_MAX,"empty offset skips table");
    Monstey_Cases+=8;
}
void Monstey_OutputAlias() {
    ++Monstey_Groups;
    for(U64 target:{Monstey_Fixture::values,Monstey_Fixture::key,Monstey_Fixture::record+16,Monstey_Fixture::cl,Monstey_Fixture::output}){
        Monstey_Fixture f;f.Configure(false,true);f.Put(f.values,0xa1b2c3d4e5f60718,8);
        const auto before=f.bytes;
        Monstey_Check(Monstey_GadbFetchGeneratedWord(f.Memory(),f.cl,f.key,f.record,0,target)==1,"aliased fetch succeeds");
        auto expected=before;
        for(std::size_t i=0;i<8;++i)expected[static_cast<std::size_t>(target-f.base)+i]=static_cast<std::uint8_t>(0xa1b2c3d4e5f60718ull>>(8*i));
        Monstey_Check(f.bytes==expected,"read before aliased output store");
        ++Monstey_Cases;
    }
    Monstey_Fixture f;f.Configure(false,true);f.Put(f.values,0xabcdef,8);
    Monstey_RangeFault([&]{Monstey_GadbFetchGeneratedWord(f.Memory(),f.cl,f.key,f.record,0,UINT64_MAX);},UINT64_MAX);
    const auto before=f.bytes;f.Put(f.key,7,4);const auto missing=f.bytes;
    Monstey_Check(Monstey_GadbFetchGeneratedWord(f.Memory(),f.cl,f.key,UINT64_MAX,0,UINT64_MAX)==0&&f.bytes==missing,"missing never writes");
    f.bytes=before;
    const auto result=Monstey_GadbReadScalarFloatBits(f.Memory(),f.record,f.key,0,UINT64_MAX);
    Monstey_Check(result==0xabcdef&&f.bytes==before,"float accessor no source writes");
}
void Monstey_OffsetFaults() {
    ++Monstey_Groups;Monstey_Fixture f;f.Configure();
    auto m=f.Memory();m.bytes=m.bytes.first(static_cast<std::size_t>(f.declarations+36-f.base));
    Monstey_Check(Monstey_GadbGeneratedFieldOffset(m,f.cl,f.hash)==0,"matched hash skips metadata");
    Monstey_RangeFault([&]{Monstey_GadbGeneratedFieldOffset(m,f.cl,7);},f.declarations+44);
    Monstey_RangeFault([&]{Monstey_GadbFindGeneratedField(m,UINT64_MAX,7);},7);
    Monstey_Cases+=3;
}
}
int main() {
    try {
        Monstey_SizeTable();Monstey_ClassOrder();Monstey_RawBits();Monstey_RawBounds();
        Monstey_GeneratedInline();Monstey_GeneratedDynamic();Monstey_Fallbacks();
        Monstey_LazyReads();Monstey_OutputAlias();Monstey_OffsetFaults();
        std::cout<<"PASS scalar_float groups="<<Monstey_Groups<<" cases="<<Monstey_Cases<<" checks="<<Monstey_Checks<<"\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL after "<<Monstey_Checks<<": "<<e.what()<<"\n";return 1;}
}