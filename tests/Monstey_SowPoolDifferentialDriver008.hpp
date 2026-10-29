#pragma once
#include "Monstey_SowPoolReference.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace Monstey_SowPoolDifferential {
using namespace Monstey_SowPoolReference;
struct Monstey_Arguments {
    std::uint64_t category,originalRdx,record,stream,frame;
};
std::uint64_t Monstey_RunProduct(Monstey_SowPoolMemory,const Monstey_Arguments&,const Monstey_SowPoolRead&);
std::uint64_t Monstey_PredicateProduct(std::uint32_t,std::uint64_t);
struct Monstey_ReadEvent {
    std::uint64_t stream,target,destination,site,result;std::uint32_t width,copied;
    bool operator==(const Monstey_ReadEvent&)const=default;
};
struct Monstey_ReadPlan {
    std::uint64_t result=0;std::uint32_t copyLimit=0xFFFFFFFFu;
};
struct Monstey_Case {
    static constexpr std::uint64_t Base=0x1234000000;
    std::string name;
    std::vector<std::uint8_t> initial=std::vector<std::uint8_t>(0x40000,0xA5);
    Monstey_Arguments args{Base+0x1700,0xAABBCCDDEEFF00C7ull,Base+0x1000,Base+0x2000,Base+0x1800};
    std::uint64_t pool=Base+0x30000,descriptors=Base+0x4000,words=Base+0x8000,cursor=Base+0x2200,table=Base+0x2100;
    std::vector<std::uint8_t> input;std::vector<Monstey_ReadPlan> plan;
    enum class Hook :std::uint8_t {None,ChangeTargetAfterHeader,ShrinkValuesAfterFirstPayload,ExpandValuesAfterFirstPayload,ChangeDescriptorBaseAfterHeader};
    Hook hook=Hook::None;
    Monstey_SowPoolMemory memory(){return {Base,initial,nullptr};}
    void append(std::uint64_t value,std::size_t width){for(std::size_t n=0;n<width;++n)input.push_back(static_cast<std::uint8_t>(value>>(8*n)));}
    void seed(std::uint16_t values,std::uint8_t descriptorCount,std::uint32_t descriptorIndex=0,std::uint8_t flags=0x80){
        auto m=memory();m.write(args.stream,8,table);m.write(table+8,8,0x140DEADC0);m.write(table+0x48,8,0x140DEADC8);
        m.write(cursor,8,words);m.write(args.frame+0x50,8,pool);m.write(args.frame+0x58,8,descriptors);m.write(args.frame+0x68,8,cursor);
        m.write(args.frame-0xC,4,8);m.write(args.frame-0x10,2,0);m.write(args.frame-8,4,0);m.write(args.frame+0x60,1,0xE7);
        const std::string model="Nazgul";for(std::size_t i=0;i<model.size();++i)m.write(pool+8+i,1,static_cast<std::uint8_t>(model[i]));m.write(pool+8+model.size(),1,0);
        append(8,4);append(values,2);append(descriptorCount,1);append(descriptorIndex,4);append(flags,1);
        for(std::uint32_t i=0;i<values;++i)append(0xBADC0000u+i,4);
    }
    void descriptor(std::uint32_t index,std::uint8_t type,std::uint16_t count,std::uint16_t first){auto m=memory();const auto at=descriptors+16ull*index;m.write(at+0xA,1,type);m.write(at+0xC,2,count);m.write(at+0xE,2,first);}
};
struct Monstey_Outcome {
    std::vector<std::uint8_t> bytes;std::vector<Monstey_ReadEvent> events;
    std::uint64_t result=0,faultAddress=0;std::size_t faultWidth=0,position=0;bool fault=false,writing=false;
};
inline Monstey_Outcome Monstey_Execute(const Monstey_Case& test,bool product){
    Monstey_Outcome out;out.bytes=test.initial;Monstey_SowPoolMemory memory{Monstey_Case::Base,out.bytes,nullptr};std::size_t ordinal=0;
    Monstey_SowPoolRead read=[&](Monstey_SowPoolMemory m,std::uint64_t object,std::uint64_t target,std::uint64_t destination,std::uint32_t width,std::uint64_t site)->std::uint64_t{
        const auto current=ordinal++;const auto plan=current<test.plan.size()?test.plan[current]:Monstey_ReadPlan{};
        const auto available=test.input.size()-std::min(out.position,test.input.size());
        const auto count=static_cast<std::uint32_t>(std::min<std::size_t>({width,plan.copyLimit,available}));
        // An explicit synthetic primitive profile. Nothing calls the native target.
        for(std::uint32_t n=0;n<count;++n)m.write(destination+n,1,test.input[out.position+n]);out.position+=count;
        auto result=plan.result;if(count<width&&current>=test.plan.size())result=0x1001;
        if(test.hook==Monstey_Case::Hook::ChangeTargetAfterHeader&&current==0)m.write(object,8,test.table+0x40);
        if(test.hook==Monstey_Case::Hook::ShrinkValuesAfterFirstPayload&&current==5)m.write(test.args.frame-0x10,2,1);
        if(test.hook==Monstey_Case::Hook::ExpandValuesAfterFirstPayload&&current==5)m.write(test.args.frame-0x10,2,4);
        if(test.hook==Monstey_Case::Hook::ChangeDescriptorBaseAfterHeader&&current==4)m.write(test.args.frame+0x58,8,test.descriptors+16);
        out.events.push_back({object,target,destination,site,result,width,count});return result;
    };
    try {
        if(product)out.result=Monstey_RunProduct(memory,test.args,read);
        else out.result=Monstey_RefLoadNamedRecord(memory,test.args.category,test.args.originalRdx,test.args.record,test.args.stream,test.args.frame,read);
    }catch(const Monstey_SowPoolFault& e){out.fault=true;out.faultAddress=e.address;out.faultWidth=e.width;out.writing=e.writing;}
    catch(const Monstey::Monstey_GadbValueFixupFault& e){out.fault=true;out.faultAddress=e.address;out.faultWidth=e.width;out.writing=e.writing;}
    return out;
}
struct Monstey_Totals {std::uint64_t cases=0,checks=0,faults=0,comparedBytes=0,primitiveEvents=0,predicates=0;};
inline void Monstey_Compare(const Monstey_Case& test,Monstey_Totals& totals){
    const auto reference=Monstey_Execute(test,false),product=Monstey_Execute(test,true);
    const auto check=[&](bool ok,const char* text){++totals.checks;if(!ok)throw std::runtime_error(test.name+": "+text);};
    check(reference.bytes==product.bytes,"entire arena/frame differs");totals.comparedBytes+=reference.bytes.size();
    check(reference.fault==product.fault,"fault status differs");
    check(reference.faultAddress==product.faultAddress&&reference.faultWidth==product.faultWidth&&reference.writing==product.writing,"fault identity/width/direction differs");
    check(reference.result==product.result,"raw RAX differs");check(reference.events==product.events,"primitive read events differ");
    check(reference.position==product.position,"primitive stream position differs");
    totals.faults+=reference.fault;totals.primitiveEvents+=reference.events.size();++totals.cases;
}
inline Monstey_Totals Monstey_DifferentialCampaign(){
    Monstey_Totals totals;
    for(std::uint32_t raw=0;raw<256;++raw){Monstey_Case t;t.name="all-type-flags-"+std::to_string(raw);t.seed(3,1,2,static_cast<std::uint8_t>(raw));t.descriptor(2,static_cast<std::uint8_t>(raw),3,0);Monstey_Compare(t,totals);}
    for(std::uint32_t n=0;n<256;++n){Monstey_Case t;t.name="descriptor-count-"+std::to_string(n);t.seed(3,static_cast<std::uint8_t>(n));for(std::uint32_t i=0;i<n;++i)t.descriptor(i,static_cast<std::uint8_t>(i%17),static_cast<std::uint16_t>(i%4),0);Monstey_Compare(t,totals);}
    for(std::uint32_t count=0;count<32;++count){Monstey_Case t;t.name="payload-count-"+std::to_string(count);t.seed(static_cast<std::uint16_t>(count),0);Monstey_Compare(t,totals);}
    for(std::size_t call=0;call<9;++call)for(const auto status:std::array<std::uint64_t,4>{1,0xAABBCCDD001234FFull,0x1122334400000000ull,0xFFFFFFFFFFFFFFFFull}){
        Monstey_Case t;t.name="read-status-"+std::to_string(call)+"-"+std::to_string(status);t.seed(4,0);t.plan.resize(call+1);t.plan[call].result=status;Monstey_Compare(t,totals);
    }
    for(std::size_t call=0;call<9;++call)for(std::uint32_t copied=0;copied<4;++copied){
        Monstey_Case t;t.name="partial-read-"+std::to_string(call)+"-"+std::to_string(copied);t.seed(4,0);t.plan.resize(call+1);t.plan[call]={0xFEED0001,copied};Monstey_Compare(t,totals);
    }
    for(const auto hook:std::array<Monstey_Case::Hook,4>{Monstey_Case::Hook::ChangeTargetAfterHeader,Monstey_Case::Hook::ShrinkValuesAfterFirstPayload,Monstey_Case::Hook::ExpandValuesAfterFirstPayload,Monstey_Case::Hook::ChangeDescriptorBaseAfterHeader}){
        Monstey_Case t;t.name="callback-hook-"+std::to_string(static_cast<int>(hook));t.seed(3,1);t.descriptor(0,4,3,0);t.descriptor(1,14,3,0);t.hook=hook;Monstey_Compare(t,totals);
    }
    for(const auto offset:std::array<std::uint64_t,17>{0,1,4,8,0xA,0xC,0xE,0x10,0x18,0x20,0x38,0x50,0x58,0x60,0x68,0x70,0x80})for(std::uint32_t kind=0;kind<7;++kind){
        Monstey_Case t;t.name="live-alias-"+std::to_string(kind)+"-"+std::to_string(offset);t.seed(3,2);t.descriptor(0,4,2,0);t.descriptor(1,14,2,0);auto m=t.memory();
        if(kind==0)t.args.record=t.words+offset;
        if(kind==1){t.words=t.descriptors+offset;m.write(t.cursor,8,t.words);}
        if(kind==2)t.args.record=t.args.frame+offset;
        if(kind==3){t.words=t.args.frame+offset;m.write(t.cursor,8,t.words);}
        if(kind==4){t.args.stream=t.words+offset;m.write(t.args.stream,8,t.table);}
        if(kind==5){t.cursor=t.words+offset;m.write(t.cursor,8,t.words);m.write(t.args.frame+0x68,8,t.cursor);}
        if(kind==6){t.descriptors=t.words+offset;m.write(t.args.frame+0x58,8,t.descriptors);t.descriptor(0,4,2,0);t.descriptor(1,14,2,0);}
        Monstey_Compare(t,totals);
    }
    for(std::uint64_t cut=0x7FF0;cut<0x8040;++cut){Monstey_Case t;t.name="payload-cut-"+std::to_string(cut);t.seed(4,0);t.initial.resize(static_cast<std::size_t>(cut));Monstey_Compare(t,totals);}
    for(std::uint64_t cut=0x1000;cut<0x1030;++cut){Monstey_Case t;t.name="publication-cut-"+std::to_string(cut);t.seed(0,0);t.args.record=Monstey_Case::Base+0x3FFF0+cut-0x1000;Monstey_Compare(t,totals);}
    for(const auto base:std::array<std::uint64_t,5>{0,1,0x100000000ull,0xFFFFFFFFFFFFFFFFull,0x8000000000000000ull}){
        Monstey_Case t;t.name="pool-base-"+std::to_string(base);t.seed(3,1);t.descriptor(0,14,3,0);t.memory().write(t.args.frame+0x50,8,base);Monstey_Compare(t,totals);
    }
    // Selected masked type/count aliases the known RDX home location.
    {Monstey_Case t;t.name="frame-descriptor-count-alias";t.pool+=0xFF;t.words=t.args.frame+0x38;t.seed(0,2);t.descriptor(0,4,1,0);t.descriptor(1,3,0,0);Monstey_Compare(t,totals);}
    for(std::uint32_t i=0;i<96;++i){
        Monstey_Case t;t.name="in-place-word-accumulator-"+std::to_string(i);t.words=t.descriptors;t.seed(0,1);t.descriptor(0,static_cast<std::uint8_t>(4+(i%6)),static_cast<std::uint16_t>(1+i%4),0);t.memory().write(t.args.frame+0x50,8,std::uint64_t{1}<<(i%64));Monstey_Compare(t,totals);
    }
    for(const auto type:std::array<std::uint32_t,13>{0,3,4,9,10,11,12,14,16,17,0x7FFFFFFF,0x80000000,0xFFFFFFFF})for(const auto incoming:std::array<std::uint64_t,4>{0,1,0xABCDEF0123456789ull,0xFFFFFFFFFFFFFFFFull}){
        const auto expected=Monstey_RefPoolTypeRaw(type,incoming),actual=Monstey_PredicateProduct(type,incoming);++totals.checks;++totals.predicates;
        if(expected!=actual)throw std::runtime_error("raw predicate result differs");
    }
    return totals;
}
} // namespace Monstey_SowPoolDifferential
