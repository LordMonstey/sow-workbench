#pragma once
#include "Monstey_GadbValueFixup.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>
namespace Monstey_SowPoolReference {
struct Monstey_SowPoolFault : std::runtime_error {
    std::uint64_t address;std::size_t width;bool writing;
    Monstey_SowPoolFault(std::uint64_t a,std::size_t n,bool w):std::runtime_error("Independent SoW pool reference range fault"),address(a),width(n),writing(w){}
};
struct Monstey_SowPoolAccess {
    std::uint64_t address,value;std::size_t width;bool writing;
    bool operator==(const Monstey_SowPoolAccess&)const=default;
};
struct Monstey_SowPoolMemory {
    std::uint64_t base;std::span<std::uint8_t> bytes;std::vector<Monstey_SowPoolAccess>* trace=nullptr;
    std::size_t offset(std::uint64_t at,std::size_t n,bool store)const {
        if(at<base||at-base>bytes.size()||n>bytes.size()-static_cast<std::size_t>(at-base))throw Monstey_SowPoolFault(at,n,store);
        return static_cast<std::size_t>(at-base);
    }
    std::uint64_t read(std::uint64_t at,std::size_t n)const {
        const auto index=offset(at,n,false);std::uint64_t result=0;
        for(std::size_t i=0;i<n;++i)result|=static_cast<std::uint64_t>(bytes[index+i])<<(8*i);
        if(trace)trace->push_back({at,result,n,false});return result;
    }
    void write(std::uint64_t at,std::size_t n,std::uint64_t value)const {
        const auto index=offset(at,n,true);for(std::size_t i=0;i<n;++i)bytes[index+i]=static_cast<std::uint8_t>(value>>(8*i));
        if(trace)trace->push_back({at,value,n,true});
    }
    std::span<std::uint8_t> output(std::uint64_t at,std::size_t n)const {return bytes.subspan(offset(at,n,true),n);}
};
// Complete 1404BA364..382. The native result contract is AL only.
inline std::uint8_t Monstey_RefPoolType(std::uint32_t raw) {
    const auto type=std::bit_cast<std::int32_t>(raw);
    if(type<0||type<=3)return 0;
    if(type<=9)return 1;
    if(type<=11||type>16)return 0;
    return 1;
}
// Exact inner loop 4BA2B3..314, after header/payload reads. Offsets hexadecimal.
// The entry table/count/word-base/pool-base are captured by the enclosing body.
inline void Monstey_RefRelocateWords(Monstey_SowPoolMemory memory,std::uint64_t descriptors,
    std::uint8_t descriptorCount,std::uint64_t words,std::uint64_t poolBase) {
    if(!descriptorCount)return;std::uint32_t descriptorIndex=0;
    do {
        const auto descriptor=descriptors+static_cast<std::uint64_t>(descriptorIndex)*16;
        const auto raw=static_cast<std::uint32_t>(memory.read(descriptor+0xA,1));
        if(Monstey_RefPoolType(raw&0x3F)!=0) {
            const auto first=memory.read(descriptor+0xE,2);
            const auto start=words+first*8;std::uint32_t i=0;
            if(memory.read(descriptor+0xC,2)>0) {
                do {
                    const auto at=start+static_cast<std::uint64_t>(i)*8;++i;
                    const auto value=memory.read(at,8);memory.write(at,8,value+poolBase);
                    const auto count=memory.read(descriptor+0xC,2);if(i>=count)break;
                }while(true);
            }
        }
        ++descriptorIndex;
    }while(descriptorIndex<descriptorCount);
}
enum class Monstey_SowPoolReadDestination : std::uint8_t {NameOffset,ValueCount,DescriptorCount,DescriptorIndex,Flags,ValueWord};
// Synthetic fixture seed format only. The real reference below uses the supplied
// owned frame addresses, retaining every alias and all existing byte state.
struct Monstey_SowPoolHeader {
    std::array<std::uint8_t,12> bytes;
    std::uint64_t get(std::size_t at,std::size_t n)const {
        std::uint64_t value=0;for(std::size_t i=0;i<n;++i)value|=static_cast<std::uint64_t>(bytes[at+i])<<(8*i);return value;
    }
};
using Monstey_SowPoolRead=std::function<std::uint64_t(Monstey_SowPoolMemory,std::uint64_t stream,
    std::uint64_t target,std::uint64_t destination,std::uint32_t bytes,std::uint64_t nativeCallSite)>;
inline std::uint64_t Monstey_RefRead(Monstey_SowPoolMemory memory,std::uint64_t stream,
    std::uint64_t destination,std::uint32_t bytes,std::uint64_t site,const Monstey_SowPoolRead& reader) {
    const auto table=memory.read(stream,8);const auto target=memory.read(table+8,8);
    return reader(memory,stream,target,destination,bytes,site);
}
inline std::uint64_t Monstey_RefPoolTypeRaw(std::uint32_t type,std::uint64_t incomingRax) {
    return (incomingRax&0xFFFFFFFFFFFFFF00ull)|Monstey_RefPoolType(type);
}
// Complete known body 4BA1F0..363. frameRbp=entryRSP-28h. Only its known RDX
// home store is synthesized. Other saved registers are supplied opaque bytes.
// Three typed wrappers perform primitive Read and return stream, ignoring status.
// A service may partially write the actual owned frame or alias another range.
inline std::uint64_t Monstey_RefLoadNamedRecord(Monstey_SowPoolMemory memory,std::uint64_t category,
    std::uint64_t originalRdx,std::uint64_t record,std::uint64_t stream,std::uint64_t frameRbp,
    const Monstey_SowPoolRead& reader) {
    memory.write(frameRbp+0x38,8,originalRdx);
    Monstey_RefRead(memory,stream,frameRbp-0xC,4,0x14115FD5E,reader);
    Monstey_RefRead(memory,stream,frameRbp-0x10,2,0x14115FD3E,reader);
    Monstey_RefRead(memory,stream,frameRbp+0x38,1,0x14115FD1E,reader);
    Monstey_RefRead(memory,stream,frameRbp-8,4,0x14115FD5E,reader);
    Monstey_RefRead(memory,stream,frameRbp+0x60,1,0x14115FD1E,reader);
    const auto valueCursor=memory.read(frameRbp+0x68,8);
    const auto initialCount=memory.read(frameRbp-0x10,2),nameOffset=memory.read(frameRbp-0xC,4);
    const auto poolBase=memory.read(frameRbp+0x50,8),words=memory.read(valueCursor,8);
    const auto name=poolBase+nameOffset;
    memory.write(valueCursor,8,words+initialCount*8);
    std::uint32_t i=0;
    if(initialCount>0) {
        do {
            const auto word=words+static_cast<std::uint64_t>(i)*8;
            memory.write(word,8,0);
            const auto raw=Monstey_RefRead(memory,stream,word,4,0x1404BA297,reader);
            if(static_cast<std::uint32_t>(raw)!=0)return raw&0xFFFFFFFFFFFFFF00ull;
            const auto count=memory.read(frameRbp-0x10,2);++i;if(i>=count)break;
        }while(true);
    }
    const auto descriptorIndex=memory.read(frameRbp-8,4);
    auto descriptorCount=static_cast<std::uint8_t>(memory.read(frameRbp+0x38,1));
    const auto descriptors=(descriptorIndex<<4)+memory.read(frameRbp+0x58,8);
    std::uint32_t index=0;
    if(descriptorCount!=0) {
        do {
            const auto descriptor=descriptors+static_cast<std::uint64_t>(index)*16;
            const auto type=static_cast<std::uint32_t>(memory.read(descriptor+0xA,1))&0x3F;
            if(Monstey_RefPoolType(type)!=0) {
                const auto first=memory.read(descriptor+0xE,2),start=words+first*8;
                std::uint32_t n=0;
                if(memory.read(descriptor+0xC,2)!=0) {
                    do {
                        const auto word=start+static_cast<std::uint64_t>(n)*8;++n;
                        const auto value=memory.read(word,8);memory.write(word,8,value+poolBase);
                        const auto liveCount=memory.read(descriptor+0xC,2);if(n>=liveCount)break;
                    }while(true);
                    descriptorCount=static_cast<std::uint8_t>(memory.read(frameRbp+0x38,1));
                }
            }
            ++index;
        }while(index<descriptorCount);
    }
    memory.write(record+8,8,descriptors);memory.write(record+0x20,8,name);
    memory.write(record,1,descriptorCount);memory.write(record+0x18,8,category);memory.write(record+0x10,8,words);
    const auto hash=Monstey::Monstey_GadbHashCString({memory.base,memory.bytes},name,0);
    const auto flags=static_cast<std::uint8_t>(memory.read(frameRbp+0x60,1))|2;
    memory.write(record+4,4,hash);memory.write(record+1,1,flags);
    return (static_cast<std::uint64_t>(hash)&0xFFFFFFFFFFFFFF00ull)|1;
}
} // namespace Monstey_SowPoolReference
