#include "Monstey_GadbConcreteRecords.hpp"
#include "Monstey_GadbValueFixup.hpp"
#include <array>
#include <bit>
#include <limits>
namespace Monstey {
namespace {
using Monstey_ConcreteAddress=std::uint64_t;
std::size_t Monstey_ConcreteOffset(Monstey_GadbBinderMemory m,
                                  Monstey_ConcreteAddress at,std::size_t n,bool writing) {
    if(at<m.base || n==0 || at>std::numeric_limits<Monstey_ConcreteAddress>::max()-(n-1) ||
       at-m.base>m.bytes.size() || n>m.bytes.size()-static_cast<std::size_t>(at-m.base))
        throw Monstey_GadbPoolRelocationFault(at,n,writing);
    return static_cast<std::size_t>(at-m.base);
}
Monstey_ConcreteAddress Monstey_ConcreteRead(Monstey_GadbBinderMemory m,
                                            Monstey_ConcreteAddress at,std::size_t n) {
    const auto p=Monstey_ConcreteOffset(m,at,n,false);Monstey_ConcreteAddress v=0;
    for(std::size_t k=0;k<n;++k)v|=Monstey_ConcreteAddress(m.bytes[p+k])<<(8*k);
    return v;
}
void Monstey_ConcreteWrite(Monstey_GadbBinderMemory m,Monstey_ConcreteAddress at,
                           Monstey_ConcreteAddress v,std::size_t n) {
    const auto p=Monstey_ConcreteOffset(m,at,n,true);
    for(std::size_t k=0;k<n;++k)m.bytes[p+k]=static_cast<std::uint8_t>(v>>(8*k));
}
std::uint8_t Monstey_ConcreteHashByte(Monstey_GadbBinderMemory m,Monstey_ConcreteAddress at) {
    if(at<m.base || at-m.base>=m.bytes.size())throw Monstey_GadbValueFixupFault(at,1,false);
    return m.bytes[static_cast<std::size_t>(at-m.base)];
}
constexpr std::array<std::uint8_t,256> Monstey_ConcreteCharacterMap={
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x25,0x3a,0x33,0x3c,0x3d,0x3e,0x40,0x32,0x42,0x43,0x41,0x28,0x36,0x27,0x37,0x34,
 0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x31,0x30,0x38,0x29,0x39,0x35,
 0x3b,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2c,0x2a,0x2d,0x3f,0x26,
 0x44,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2e,0x2b,0x2f,0x45,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
Monstey_GadbBufferedResult Monstey_ConcreteStreamRead(
    Monstey_GadbBinderMemory m,std::uint64_t stream,std::uint64_t destination,
    std::uint32_t count,std::uint64_t outer_body_rsp,std::uint64_t native_site,
    const Monstey_GadbBufferedServices& services,Monstey_GadbBufferedCells cells) {
    const auto table=Monstey_ConcreteRead(m,stream,8);
    const auto target=Monstey_ConcreteRead(m,table+8,8);
    if(target!=0x141B50DE8ULL)throw Monstey_GadbPoolReadBoundaryFault(native_site,target);
    return Monstey_GadbBufferedRead(m,stream,destination,count,outer_body_rsp,services,cells);
}
Monstey_GadbBufferedResult Monstey_ConcreteTypedRead(
    Monstey_GadbBinderMemory m,std::uint64_t stream,std::uint64_t destination,
    std::uint32_t count,std::uint64_t caller_body_rsp,std::uint64_t site,
    const Monstey_GadbBufferedServices& services,Monstey_GadbBufferedCells cells) {
    auto result=Monstey_ConcreteStreamRead(m,stream,destination,count,caller_body_rsp-0x60,site,services,cells);
    result.rax=stream;
    return result;
}
Monstey_GadbBufferedResult Monstey_ConcreteAL(Monstey_GadbBufferedResult result,bool value) {
    result.rax=(result.rax&~std::uint64_t{255})|static_cast<std::uint64_t>(value);return result;
}
} // namespace
Monstey_GadbBufferedResult Monstey_GadbHashCStringRegisters(
    Monstey_GadbBinderMemory m,std::uint64_t identity,std::uint32_t seed,std::uint64_t incoming_rdx) {
    auto byte=Monstey_ConcreteHashByte(m,identity);std::uint64_t rdx=incoming_rdx;
    while(byte!=0) {
        ++identity;
        const auto signed_value=std::bit_cast<std::int8_t>(Monstey_ConcreteCharacterMap[byte]);
        const auto translated=static_cast<std::uint32_t>(static_cast<std::int32_t>(signed_value));
        rdx=translated;seed=seed*0x397u+translated;
        byte=Monstey_ConcreteHashByte(m,identity);
    }
    return {seed,rdx};
}
Monstey_GadbBufferedResult Monstey_GadbLoadNamedRecordFromBuffered(
    Monstey_GadbBinderMemory m,std::uint64_t category,std::uint64_t original_rdx,
    std::uint64_t record,std::uint64_t stream,std::uint64_t frame,
    const Monstey_GadbBufferedServices& services,Monstey_GadbBufferedCells cells) {
    Monstey_ConcreteWrite(m,frame+0x38,original_rdx,8);
    const auto body=frame-0x30;
    (void)Monstey_ConcreteTypedRead(m,stream,frame-0x0C,4,body,0x14115FD5EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame-0x10,2,body,0x14115FD3EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x38,1,body,0x14115FD1EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame-8,4,body,0x14115FD5EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x60,1,body,0x14115FD1EULL,services,cells);
    auto rdx=Monstey_ConcreteRead(m,frame+0x68,8);
    const auto initial=static_cast<std::uint16_t>(Monstey_ConcreteRead(m,frame-0x10,2));
    const auto name_offset=static_cast<std::uint32_t>(Monstey_ConcreteRead(m,frame-0x0C,4));
    const auto pool=Monstey_ConcreteRead(m,frame+0x50,8);
    const auto words=Monstey_ConcreteRead(m,rdx,8);const auto name=pool+name_offset;
    Monstey_ConcreteWrite(m,rdx,words+std::uint64_t(initial)*8,8);
    Monstey_GadbBufferedResult result{stream,rdx};
    std::uint32_t index=0;
    if(initial!=0)do {
        const auto slot=words+std::uint64_t(index)*8;Monstey_ConcreteWrite(m,slot,0,8);
        result=Monstey_ConcreteStreamRead(m,stream,slot,4,body-0x30,0x1404BA297ULL,services,cells);
        if(static_cast<std::uint32_t>(result.rax)!=0)return Monstey_ConcreteAL(result,false);
        result.rax=Monstey_ConcreteRead(m,frame-0x10,2);++index;
    }while(index<result.rax);
    const auto first=static_cast<std::uint32_t>(Monstey_ConcreteRead(m,frame-8,4));
    auto count=static_cast<std::uint8_t>(Monstey_ConcreteRead(m,frame+0x38,1));
    const auto descriptors=(std::uint64_t(first)<<4)+Monstey_ConcreteRead(m,frame+0x58,8);index=0;
    if(count!=0)do {
        result.rdx=descriptors+(std::uint64_t(index)<<4);
        const auto type=static_cast<std::uint32_t>(Monstey_ConcreteRead(m,result.rdx+0x0A,1))&0x3Fu;
        result.rax=Monstey_GadbPoolRelativeTypeRaw(type,result.rax);
        if(static_cast<std::uint8_t>(result.rax)!=0) {
            result.rax=Monstey_ConcreteRead(m,result.rdx+0x0E,2);
            const auto first_word=words+result.rax*8;result.rax=0;std::uint32_t word_index=0;
            if(Monstey_ConcreteRead(m,result.rdx+0x0C,2)!=0) {
                do {
                    const auto slot=first_word+std::uint64_t(word_index++)*8;
                    const auto offset=Monstey_ConcreteRead(m,slot,8);Monstey_ConcreteWrite(m,slot,offset+pool,8);
                    result.rax=Monstey_ConcreteRead(m,result.rdx+0x0C,2);
                }while(word_index<result.rax);
                count=static_cast<std::uint8_t>(Monstey_ConcreteRead(m,frame+0x38,1));
            }
        }
        ++index;
    }while(index<count);
    Monstey_ConcreteWrite(m,record+8,descriptors,8);
    Monstey_ConcreteWrite(m,record+0x20,name,8);
    Monstey_ConcreteWrite(m,record,count,1);
    Monstey_ConcreteWrite(m,record+0x18,category,8);
    Monstey_ConcreteWrite(m,record+0x10,words,8);
    result=Monstey_GadbHashCStringRegisters(m,name,0,result.rdx);
    const auto flags=static_cast<std::uint8_t>(Monstey_ConcreteRead(m,frame+0x60,1))|2u;
    Monstey_ConcreteWrite(m,record+4,result.rax,4);Monstey_ConcreteWrite(m,record+1,flags,1);
    return Monstey_ConcreteAL(result,true);
}
Monstey_GadbBufferedResult Monstey_GadbLoadAnonymousRecordFromBuffered(
    Monstey_GadbBinderMemory m,std::uint64_t category,std::uint64_t original_rdx,
    std::uint64_t record,std::uint64_t stream,std::uint64_t frame,
    const Monstey_GadbBufferedServices& services,Monstey_GadbBufferedCells cells) {
    Monstey_ConcreteWrite(m,frame+0x58,original_rdx,8);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x20,2,frame,0x14115FD3EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x58,1,frame,0x14115FD1EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x24,4,frame,0x14115FD5EULL,services,cells);
    (void)Monstey_ConcreteTypedRead(m,stream,frame+0x80,1,frame,0x14115FD1EULL,services,cells);
    const auto initial=static_cast<std::uint16_t>(Monstey_ConcreteRead(m,frame+0x20,2));
    const auto cursor=Monstey_ConcreteRead(m,frame+0x88,8);
    Monstey_GadbBufferedResult result{initial,cursor};const auto words=Monstey_ConcreteRead(m,cursor,8);
    Monstey_ConcreteWrite(m,cursor,words+std::uint64_t(initial)*8,8);
    std::uint32_t index=0;
    if(initial!=0)do {
        const auto slot=words+std::uint64_t(index)*8;Monstey_ConcreteWrite(m,slot,0,8);
        result=Monstey_ConcreteStreamRead(m,stream,slot,4,frame-0x30,0x14176662BULL,services,cells);
        if(static_cast<std::uint32_t>(result.rax)!=0)return Monstey_ConcreteAL(result,false);
        result.rax=Monstey_ConcreteRead(m,frame+0x20,2);++index;
    }while(index<result.rax);
    const auto first=static_cast<std::uint32_t>(Monstey_ConcreteRead(m,frame+0x24,4));
    auto count=static_cast<std::uint8_t>(Monstey_ConcreteRead(m,frame+0x58,1));
    const auto descriptors=(std::uint64_t(first)<<4)+Monstey_ConcreteRead(m,frame+0x78,8);index=0;
    if(count!=0) {
        const auto pool=Monstey_ConcreteRead(m,frame+0x70,8);
        do {
            result.rdx=descriptors+(std::uint64_t(index)<<4);
            const auto type=static_cast<std::uint32_t>(Monstey_ConcreteRead(m,result.rdx+0x0A,1))&0x3Fu;
            result.rax=Monstey_GadbPoolRelativeTypeRaw(type,result.rax);
            if(static_cast<std::uint8_t>(result.rax)!=0) {
                result.rax=Monstey_ConcreteRead(m,result.rdx+0x0E,2);const auto first_word=words+result.rax*8;
                std::uint32_t word_index=0;
                if(Monstey_ConcreteRead(m,result.rdx+0x0C,2)!=0) {
                    do {
                        const auto slot=first_word+std::uint64_t(word_index++)*8;
                        const auto value=Monstey_ConcreteRead(m,slot,8);Monstey_ConcreteWrite(m,slot,value+pool,8);
                        result.rax=Monstey_ConcreteRead(m,result.rdx+0x0C,2);
                    }while(word_index<result.rax);
                    count=static_cast<std::uint8_t>(Monstey_ConcreteRead(m,frame+0x58,1));
                }
            }
            ++index;
        }while(index<count);
    }
    const auto flags=Monstey_ConcreteRead(m,frame+0x80,1);result=Monstey_ConcreteAL(result,true);
    Monstey_ConcreteWrite(m,record+1,flags,1);Monstey_ConcreteWrite(m,record,count,1);
    Monstey_ConcreteWrite(m,record+8,descriptors,8);Monstey_ConcreteWrite(m,record+0x18,category,8);
    Monstey_ConcreteWrite(m,record+0x10,words,8);return result;
}
} // namespace Monstey
