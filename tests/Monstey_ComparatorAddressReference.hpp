#pragma once
#include "Monstey_FearDbOpen.hpp"
#include <bit>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
namespace {
using Monstey_U32=std::uint32_t;using Monstey_I32=std::int32_t;using Monstey_Resources=Monstey::Monstey_FearDbResources;
struct Monstey_ReferenceFault{Monstey_U32 address,width;bool writing;};
struct Monstey_ReferenceTarget{
 using Function=std::function<Monstey_I32(Monstey_U32,Monstey_U32)>;std::shared_ptr<Function>current;
 Monstey_ReferenceTarget()=default;Monstey_ReferenceTarget(const Monstey_ReferenceTarget&)=default;Monstey_ReferenceTarget&operator=(const Monstey_ReferenceTarget&)=default;
 template<class F>Monstey_ReferenceTarget&operator=(F f){current=std::make_shared<Function>(std::move(f));return *this;}
 explicit operator bool()const{return current&&static_cast<bool>(*current);}Monstey_I32 operator()(Monstey_U32 a,Monstey_U32 b)const{const auto active=current;if(!active||!*active)throw std::bad_function_call();return (*active)(a,b);}
};
Monstey_U32 Monstey_ReferenceRead(Monstey::Monstey_FearDbMemory m,Monstey_U32 p,unsigned width=4){if(p<m.base||static_cast<std::uint64_t>(p)+width>0x100000000ull||static_cast<std::uint64_t>(p-m.base)+width>m.bytes.size())throw Monstey_ReferenceFault{p,width,false};Monstey_U32 v=0;for(unsigned i=0;i<width;++i)v|=static_cast<Monstey_U32>(m.bytes[p-m.base+i])<<(8*i);return v;}
void Monstey_ReferenceWrite(Monstey_Resources&r,Monstey_U32 /*db*/,Monstey_U32 p,Monstey_U32 value){for(auto&i:r.allocations){if(p>=i.base&&static_cast<std::uint64_t>(p)+4<=0x100000000ull&&static_cast<std::uint64_t>(p-i.base)+4<=i.allocation.size()){for(unsigned n=0;n<4;++n)i.allocation[p-i.base+n]=static_cast<std::uint8_t>(value>>(8*n));return;}}throw Monstey_ReferenceFault{p,4,true};}
Monstey_U32 Monstey_ReferenceDbRead(Monstey_Resources&r,Monstey_U32 /*db*/,Monstey_U32 p){for(const auto&i:r.allocations)if(p>=i.base&&static_cast<std::uint64_t>(p)+4<=0x100000000ull&&static_cast<std::uint64_t>(p-i.base)+4<=i.allocation.size())return Monstey_ReferenceRead(i.Memory(),p);throw Monstey_ReferenceFault{p,4,false};}
Monstey_U32 Monstey_ReferenceOpen(Monstey_Resources&r,Monstey::Monstey_FearDbMemory names,Monstey_U32 filename,const Monstey_ReferenceTarget&holder){
 if(filename==0)return 0;if(Monstey_ReferenceRead(names,filename,1)==0)return 0;if(r.cache.empty())return 0;
 const auto selected=holder.current;const auto*backing=r.cache.data();std::size_t cursor=0;
 auto row=[&](){if(r.cache.data()!=backing||cursor>=r.cache.size())throw std::runtime_error("consumed unavailable cache slot");return r.cache[cursor];};
 for(;;){const auto db=row();const auto candidate=Monstey_ReferenceDbRead(r,db,db+4);if(candidate!=0){if(!selected||!*selected)throw std::bad_function_call();const auto cmp=(*selected)(filename,candidate);if(cmp==0){const auto live=row();const auto count=Monstey_ReferenceDbRead(r,live,live+16);Monstey_ReferenceWrite(r,live,live+16,count+1u);return row();}}
  const auto end=r.cache.size();++cursor;if(cursor==end)return 0;
 }
}
Monstey_U32 Monstey_ReferenceNames(Monstey::Monstey_FearDbMemory m,Monstey_U32 children,Monstey_U32 count,Monstey_U32 query,const Monstey_ReferenceTarget&holder){
 Monstey_I32 lo=0,hi=std::bit_cast<Monstey_I32>(count-1u);while(lo<=hi){const auto sum=static_cast<Monstey_U32>(lo)+static_cast<Monstey_U32>(hi);const auto mid=std::bit_cast<Monstey_I32>(sum)/2;const auto row=children+16u*static_cast<Monstey_U32>(mid);const auto candidate=Monstey_ReferenceRead(m,row);Monstey_I32 cmp=0;if(query==0)cmp=candidate==0?0:-1;else if(candidate==0){lo=std::bit_cast<Monstey_I32>(static_cast<Monstey_U32>(mid)+1u);continue;}else cmp=holder(query,candidate);if(cmp==0)return children+16u*static_cast<Monstey_U32>(mid);if(cmp<0)hi=std::bit_cast<Monstey_I32>(static_cast<Monstey_U32>(mid)-1u);else lo=std::bit_cast<Monstey_I32>(static_cast<Monstey_U32>(mid)+1u);}
 return 0;
}
}
