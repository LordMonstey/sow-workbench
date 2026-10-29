#include "Monstey_GadbConcreteRecords.hpp"
#include "Monstey_GadbAnonymousRecord.hpp"
#include "Monstey_GadbValueFixup.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace Monstey_Test {
using namespace Monstey;
constexpr std::uint64_t Monstey_Base=0x100000000ULL;
constexpr std::uint64_t Monstey_Stream=Monstey_Base+0x200,Monstey_Frame=Monstey_Base+0x30000;
constexpr std::uint64_t Monstey_Pool=Monstey_Base+0x8000,Monstey_Descriptors=Monstey_Base+0xA000;
constexpr std::uint64_t Monstey_Record=Monstey_Base+0x9000,Monstey_Category=Monstey_Base+0xB000;
constexpr std::uint64_t Monstey_Words=Monstey_Base+0xC000,Monstey_Cursor=Monstey_Base+0xD000;
constexpr Monstey_GadbBufferedCells Monstey_Cells{Monstey_Base+0x100,Monstey_Base+0x108,Monstey_Base+0x110};
struct Monstey_ExternalFault {std::uint64_t at;std::size_t width;bool writing;};
std::size_t Monstey_At(Monstey_GadbBinderMemory m,std::uint64_t at,std::size_t n,bool w) {
    if(at<m.base||at-m.base>m.bytes.size()||n>m.bytes.size()-static_cast<std::size_t>(at-m.base))throw Monstey_ExternalFault{at,n,w};
    return static_cast<std::size_t>(at-m.base);
}
std::uint64_t Monstey_Read(Monstey_GadbBinderMemory m,std::uint64_t at,std::size_t n) {
    const auto p=Monstey_At(m,at,n,false);std::uint64_t v=0;for(std::size_t k=0;k<n;++k)v|=std::uint64_t(m.bytes[p+k])<<(8*k);return v;
}
void Monstey_Write(Monstey_GadbBinderMemory m,std::uint64_t at,std::uint64_t v,std::size_t n) {
    const auto p=Monstey_At(m,at,n,true);for(std::size_t k=0;k<n;++k)m.bytes[p+k]=static_cast<std::uint8_t>(v>>(8*k));
}
struct Monstey_Case {
    std::string name;bool named;unsigned values,descriptors,type,mode;std::string text;
    std::uint64_t record=Monstey_Record;std::size_t visible=0x40000;
    std::function<void(Monstey_GadbBinderMemory)> alter;
    std::function<void(Monstey_GadbBinderMemory,std::size_t)> after_copy;
    bool analytical=true;
};
struct Monstey_Event {std::array<std::uint64_t,10> v{};bool operator==(const Monstey_Event&)const=default;};
struct Monstey_Fault {unsigned kind{};std::uint64_t at{};std::size_t width{};bool writing{};bool operator==(const Monstey_Fault&)const=default;};
struct Monstey_Outcome {
    std::vector<std::uint8_t> arena;std::vector<Monstey_Event> events;
    Monstey_GadbBufferedResult result{};Monstey_Fault fault;
    std::uint64_t last_primitive_rdx{};
};
void Monstey_Append(std::vector<std::uint8_t>& v,std::uint32_t n,unsigned width) {
    for(unsigned k=0;k<width;++k)v.push_back(static_cast<std::uint8_t>(n>>(k*8)));
}
Monstey_Outcome Monstey_Run(const Monstey_Case& c,bool product) {
    Monstey_Outcome out;out.arena.assign(0x40000,0xA5);Monstey_GadbBinderMemory m{Monstey_Base,out.arena};
    std::vector<std::uint8_t> source;
    if(c.named)Monstey_Append(source,0,4);
    Monstey_Append(source,c.values,2);Monstey_Append(source,c.descriptors,1);Monstey_Append(source,0,4);Monstey_Append(source,0xA7,1);
    const auto header=source.size();for(unsigned k=0;k<c.values;++k)Monstey_Append(source,k==0?0xFFFFFFFFu:k*13,4);
    for(std::size_t k=0;k<=c.text.size();++k)Monstey_Write(m,Monstey_Pool+k,k==c.text.size()?0:static_cast<std::uint8_t>(c.text[k]),1);
    Monstey_Write(m,Monstey_Stream,Monstey_Base+0x600,8);Monstey_Write(m,Monstey_Base+0x608,0x141B50DE8ULL,8);
    Monstey_Write(m,Monstey_Stream+8,Monstey_Base+0x700,8);Monstey_Write(m,Monstey_Base+0x718,0x141204C2CULL,8);
    Monstey_Write(m,Monstey_Stream+0x10,0x4321,8);Monstey_Write(m,Monstey_Stream+0x18,Monstey_Base+0x2000,8);
    Monstey_Write(m,Monstey_Stream+0x20,0,4);Monstey_Write(m,Monstey_Stream+0x28,0,1);
    const auto cached=c.mode==0?source.size():c.mode==1?0:header;
    Monstey_Write(m,Monstey_Stream+0x24,cached,4);
    for(std::size_t k=0;k<cached;++k)Monstey_Write(m,Monstey_Base+0x2000+k,source[k],1);
    Monstey_Write(m,Monstey_Cells.copy,0x1111,8);Monstey_Write(m,Monstey_Cells.fill,0x2222,8);Monstey_Write(m,Monstey_Cells.read_file,0x3333,8);
    Monstey_Write(m,Monstey_Cursor,Monstey_Words,8);
    Monstey_Write(m,Monstey_Frame+(c.named?0x50:0x70),Monstey_Pool,8);
    Monstey_Write(m,Monstey_Frame+(c.named?0x58:0x78),Monstey_Descriptors,8);
    Monstey_Write(m,Monstey_Frame+(c.named?0x68:0x88),Monstey_Cursor,8);
    for(unsigned k=0;k<c.descriptors;++k) {
        Monstey_Write(m,Monstey_Descriptors+k*16+0x0A,c.type,1);
        Monstey_Write(m,Monstey_Descriptors+k*16+0x0C,c.values,2);
        Monstey_Write(m,Monstey_Descriptors+k*16+0x0E,0,2);
    }
    if(c.alter)c.alter(m);m.bytes=m.bytes.first(c.visible);
    std::size_t position=cached,copy_calls=0;
    Monstey_GadbBufferedServices services;
    services.copy=[&](Monstey_GadbBinderMemory v,const Monstey_GadbBufferedCopyCall& q) {
        out.events.push_back({{0,q.slot,q.target,q.destination,q.source,q.count,q.native_site,0,0,0}});
        for(std::uint64_t k=0;k<q.count;++k)Monstey_Write(v,q.destination+k,Monstey_Read(v,q.source+k,1),1);
        ++copy_calls;if(c.after_copy)c.after_copy(v,copy_calls);
        out.last_primitive_rdx=0xD123456700000000ULL+out.events.size();return Monstey_GadbBufferedResult{0xABCD012345678901ULL,out.last_primitive_rdx};
    };
    services.fill=[&](Monstey_GadbBinderMemory v,const Monstey_GadbBufferedFillCall& q) {
        out.events.push_back({{1,q.slot,q.target,q.destination,0,q.count,q.native_site,q.value,0,0}});
        for(std::uint64_t k=0;k<q.count;++k)Monstey_Write(v,q.destination+k,q.value,1);
        out.last_primitive_rdx=0xD234567800000000ULL+out.events.size();return Monstey_GadbBufferedResult{0xABCD123456789012ULL,out.last_primitive_rdx};
    };
    services.read_file=[&](Monstey_GadbBinderMemory v,const Monstey_GadbBufferedReadFileCall& q) {
        out.events.push_back({{2,q.slot,q.target,q.destination,q.handle,q.count,q.native_site,q.bytes_read_address,q.overlapped,q.stack_argument_address}});
        const auto count=static_cast<std::uint32_t>(std::min<std::size_t>(q.count,source.size()-position));
        Monstey_Write(v,q.bytes_read_address,count,4);
        for(std::uint32_t k=0;k<count;++k)Monstey_Write(v,q.destination+k,source[position++],1);
        out.last_primitive_rdx=0xD345678900000000ULL+out.events.size();
        return Monstey_GadbBufferedResult{c.mode==2?0xEEFF000000000002ULL:0xEEFF000000000001ULL,out.last_primitive_rdx};
    };
    try {
        if(product)out.result=c.named?
            Monstey_GadbLoadNamedRecordFromBuffered(m,Monstey_Category,0xFEEDAB1234567890ULL,c.record,Monstey_Stream,Monstey_Frame,services,Monstey_Cells):
            Monstey_GadbLoadAnonymousRecordFromBuffered(m,Monstey_Category,0xFEEDAB1234567890ULL,c.record,Monstey_Stream,Monstey_Frame,services,Monstey_Cells);
        else {
            const auto body=c.named?Monstey_Frame-0x30:Monstey_Frame;
            Monstey_GadbPoolReadService read=[&](Monstey_GadbBinderMemory& v,const Monstey_GadbPoolReadCall& q) {
                if(q.target!=0x141B50DE8ULL)throw Monstey_GadbPoolReadBoundaryFault(q.native_site,q.target);
                const bool payload=q.native_site==0x1404BA297ULL||q.native_site==0x14176662BULL;
                return Monstey_GadbBufferedRead(v,q.stream,q.destination,q.byte_count,body-(payload?0x30:0x60),services,Monstey_Cells).rax;
            };
            out.result.rax=c.named?Monstey_GadbLoadPoolRelativeRecord(m,Monstey_Category,0xFEEDAB1234567890ULL,c.record,Monstey_Stream,Monstey_Frame,read):
                Monstey_GadbLoadAnonymousRecord(m,Monstey_Category,0xFEEDAB1234567890ULL,c.record,Monstey_Stream,Monstey_Frame,read);
        }
    } catch(const Monstey_GadbPoolRelocationFault& q) {out.fault={1,q.address,q.width,q.writing};}
      catch(const Monstey_GadbBufferedMemoryFault& q) {out.fault={2,q.address,q.width,q.writing};}
      catch(const Monstey_GadbValueFixupFault& q) {out.fault={3,q.address,q.width,q.writing};}
      catch(const Monstey_GadbPoolReadBoundaryFault& q) {out.fault={4,q.native_site,static_cast<std::size_t>(q.target),false};}
      catch(const Monstey_ExternalFault& q) {out.fault={5,q.at,q.width,q.writing};}
    return out;
}
} // namespace Monstey_Test
int main() {
    using namespace Monstey_Test;std::size_t cases=0,checks=0,failed=0,faults=0,events=0,bytes=0;
    const auto require=[&](bool v,const std::string& name){++checks;if(!v){++failed;std::cerr<<name<<'\n';}};
    const auto test=[&](const Monstey_Case& c) {
        ++cases;const auto a=Monstey_Run(c,false),p=Monstey_Run(c,true);
        require(a.fault==p.fault,c.name+": fault");require(a.arena==p.arena,c.name+": full arena");require(a.events==p.events,c.name+": events");
        if(a.fault.kind==0)require(a.result.rax==p.result.rax,c.name+": RAX");
        if(p.fault.kind!=0)++faults;
        else if(c.analytical) {
            auto expected=Monstey_Cursor;
            if((p.result.rax&255)==0)expected=p.last_primitive_rdx;
            else {
                if(c.values!=0)expected=p.last_primitive_rdx;
                if(c.descriptors!=0)expected=Monstey_Descriptors+(c.descriptors-1)*16;
                if(c.named&&!c.text.empty())expected=c.text.back()=='A'?1:0;
            }
            require(p.result.rdx==expected,c.name+": independent raw RDX rule");
        }
        events+=p.events.size();bytes+=p.arena.size();
    };
    for(bool named:{false,true})for(unsigned values:{0u,1u,2u,5u})for(unsigned desc:{0u,1u,2u})
      for(unsigned type:{3u,5u,10u,14u})for(unsigned mode:{0u,1u,2u})for(const auto& text:{std::string{},std::string("A"),std::string(1,std::bit_cast<char>(std::uint8_t{255}))}) {
        Monstey_Case c{"matrix-"+std::to_string(cases),named,values,desc,type,mode,text};test(c);
    }
    for(bool named:{false,true}) {
        for(std::uint64_t record:{Monstey_Frame+0x58,Monstey_Words,Monstey_Descriptors,Monstey_Cursor}) {
            Monstey_Case c{"output-alias-"+std::to_string(cases),named,2,2,5,0,"A"};c.record=record;c.analytical=false;test(c);
        }
        for(std::size_t visible:{std::size_t(0x30040),std::size_t(0x30100),std::size_t(0xA00F),std::size_t(0xD004)}) {
            Monstey_Case c{"range-"+std::to_string(cases),named,2,2,5,0,""};c.visible=visible;c.analytical=false;test(c);
        }
        Monstey_Case c{"bad-vtable-"+std::to_string(cases),named,2,2,5,0,""};c.alter=[](auto m){Monstey_Write(m,Monstey_Base+0x608,0x141B50DE9ULL,8);};c.analytical=false;test(c);
        c={"live-header-count-"+std::to_string(cases),named,2,2,5,0,""};c.after_copy=[named](auto m,std::size_t k){if(k==(named?5u:4u))Monstey_Write(m,Monstey_Frame+(named?0x38:0x58),1,1);};c.analytical=false;test(c);
        c={"live-end-during-copy-"+std::to_string(cases),named,2,2,5,0,""};c.after_copy=[](auto m,std::size_t k){if(k==1)Monstey_Write(m,Monstey_Stream+0x24,2,4);};c.analytical=false;test(c);
    }
    for(unsigned byte=0;byte<256;++byte)for(std::uint32_t seed:{0u,1u,0x7FFFFFFFu,0xFFFFFFFFu})for(std::uint64_t incoming:{0ULL,0xFEDCBA9876543210ULL}) {
        ++cases;std::vector<std::uint8_t> arena(32,0);Monstey_GadbBinderMemory m{Monstey_Base,arena};arena[0]=static_cast<std::uint8_t>(byte);
        const auto raw=Monstey_GadbHashCStringRegisters(m,Monstey_Base,seed,incoming);
        require(raw.rax==Monstey_GadbHashCString(m,Monstey_Base,seed),"hash legacy");
        require(raw.rdx==(byte==0?incoming:Monstey_GadbHashCString(m,Monstey_Base,0)),"hash raw RDX");
    }
    std::cout<<"{\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"faults\":"<<faults<<",\"events\":"<<events<<",\"arena_bytes\":"<<bytes<<",\"failed\":"<<failed<<"}\n";
    return failed==0?0:1;
}
