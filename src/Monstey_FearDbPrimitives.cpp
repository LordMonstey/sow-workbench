#include "Monstey_FearDbPrimitives.hpp"
#include <bit>
namespace Monstey {
Monstey_FearDbFault::Monstey_FearDbFault(std::uint32_t at,std::uint32_t count,bool write):std::runtime_error(write?"Consumed FEAR writer range unavailable":"Consumed FEAR reader range unavailable"),address(at),width(count),writing(write){}
namespace {
using Monstey_U32=std::uint32_t;
void Monstey_FearRange(std::uint32_t base,std::size_t size,Monstey_U32 at,Monstey_U32 width,bool write){
 if(at<base||static_cast<std::uint64_t>(at)+width>0x100000000ull||static_cast<std::uint64_t>(at)-base>size||width>size-static_cast<std::size_t>(static_cast<std::uint64_t>(at)-base))throw Monstey_FearDbFault(at,width,write);
}
Monstey_U32 Monstey_FearRead(Monstey_FearDbMemory m,Monstey_U32 at,Monstey_U32 width){
 Monstey_FearRange(m.base,m.bytes.size(),at,width,false);const auto offset=static_cast<std::size_t>(at-m.base);Monstey_U32 value=0;for(Monstey_U32 i=0;i<width;++i)value|=static_cast<Monstey_U32>(m.bytes[offset+i])<<(i*8);return value;
}
void Monstey_FearStore(Monstey_FearDbMutableMemory m,Monstey_U32 at,Monstey_U32 value){
 Monstey_FearRange(m.base,m.bytes.size(),at,4,true);const auto offset=static_cast<std::size_t>(at-m.base);for(unsigned i=0;i<4;++i)m.bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8));
}
Monstey_U32 Monstey_FearScalar(Monstey_FearDbMemory m,Monstey_U32 h,Monstey_U32 index,Monstey_U32 type,Monstey_U32 fallback){
 if(h==0)return fallback;
 if(Monstey_FearRead(m,h+10u,1)!=type)return fallback;
 if(index>=Monstey_FearRead(m,h+8u,2))return fallback;
 const auto values=Monstey_FearRead(m,h+12u,4);
 return Monstey_FearRead(m,values+index*4u,4);
}
Monstey_U32 Monstey_FearVector(Monstey_FearDbMutableMemory m,Monstey_U32 h,Monstey_U32 index,Monstey_U32 fallback,Monstey_U32 output,Monstey_U32 type,Monstey_U32 count){
 const Monstey_FearDbMemory read{m.base,m.bytes};Monstey_U32 source=fallback;
 if(h!=0&&Monstey_FearRead(read,h+10u,1)==type&&index<Monstey_FearRead(read,h+8u,2)){
  const auto values=Monstey_FearRead(read,h+12u,4);source=Monstey_FearRead(read,values+index*4u,4);
 }
 for(Monstey_U32 i=0;i<count;++i){const auto word=Monstey_FearRead(read,source+i*4u,4);Monstey_FearStore(m,output+i*4u,word);}
 return output;
}
}
std::uint32_t Monstey_FearDbNodeName(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h,4):0;}
std::uint32_t Monstey_FearDbNodeParentOrDatabaseName(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h+4u,4):0;}
std::uint32_t Monstey_FearDbChildCount(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h+8u,4):0;}
std::uint32_t Monstey_FearDbChildByIndex(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i){if(h==0)return 0;if(i>=Monstey_FearRead(m,h+8u,4))return 0;return Monstey_FearRead(m,h+12u,4)+i*16u;}
std::uint32_t Monstey_FearDbAttributeByIndex(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i){if(h==0)return 0;return Monstey_FearRead(m,h+12u,4)+i*16u;}
std::uint32_t Monstey_FearDbHandleIndex(Monstey_FearDbMemory m,std::uint32_t h){if(h==0)return UINT32_MAX;const auto parent=Monstey_FearRead(m,h+4u,4);const auto first=Monstey_FearRead(m,parent+12u,4);return std::bit_cast<Monstey_U32>(std::bit_cast<std::int32_t>(h-first)>>4);}
std::uint32_t Monstey_FearDbValueCount(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h+8u,2):0;}
std::uint32_t Monstey_FearDbAttributeType(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h+10u,1):0;}
std::uint32_t Monstey_FearDbAttributeUsage(Monstey_FearDbMemory m,std::uint32_t h){return h?Monstey_FearRead(m,h+11u,1):0;}
std::uint32_t Monstey_FearDbInt32Bits(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f){return Monstey_FearScalar(m,h,i,3,f);}
std::uint32_t Monstey_FearDbFloat32Bits(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f){return Monstey_FearScalar(m,h,i,2,f);}
std::uint32_t Monstey_FearDbStringIdentity(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f){return Monstey_FearScalar(m,h,i,4,f);}
std::uint32_t Monstey_FearDbWStringIdentity(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f){return Monstey_FearScalar(m,h,i,5,f);}
std::uint32_t Monstey_FearDbRecordIdentity(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f){return Monstey_FearScalar(m,h,i,9,f);}
std::uint8_t Monstey_FearDbBoolByte(Monstey_FearDbMemory m,std::uint32_t h,std::uint32_t i,std::uint8_t f){if(h==0)return f;if(Monstey_FearRead(m,h+10u,1)!=1)return f;if(i>=Monstey_FearRead(m,h+8u,2))return f;const auto values=Monstey_FearRead(m,h+12u,4);return static_cast<std::uint8_t>(Monstey_FearRead(m,values+i*4u,1));}
std::uint32_t Monstey_FearDbVector2(Monstey_FearDbMutableMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f,std::uint32_t o){return Monstey_FearVector(m,h,i,f,o,6,2);}
std::uint32_t Monstey_FearDbVector3(Monstey_FearDbMutableMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f,std::uint32_t o){return Monstey_FearVector(m,h,i,f,o,7,3);}
std::uint32_t Monstey_FearDbVector4(Monstey_FearDbMutableMemory m,std::uint32_t h,std::uint32_t i,std::uint32_t f,std::uint32_t o){return Monstey_FearVector(m,h,i,f,o,8,4);}
} //namespace Monstey
