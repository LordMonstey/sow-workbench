#pragma once
#include "Monstey_FearUnpackedLine.hpp"
#include <cstdint>
namespace Monstey_LineReference {
using U=std::uint32_t;using Memory=Monstey::Monstey_FearDbMutableMemory;
struct Boundary {U callback,line,context;};
inline U Read(const Memory& memory,U address,U width=4) {
 const std::uint64_t end=std::uint64_t(address)+width;
 if(address<memory.base||end>0x100000000ull||std::uint64_t(address)-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))throw Monstey::Monstey_FearDbFault(address,width,false);
 U value=0;for(U i=0;i<width;++i)value|=U(memory.bytes[address-memory.base+i])<<(i*8u);return value;
}
inline void Write(Memory& memory,U address,U value,U width=4) {
 const std::uint64_t end=std::uint64_t(address)+width;
 if(address<memory.base||end>0x100000000ull||std::uint64_t(address)-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))throw Monstey::Monstey_FearDbFault(address,width,true);
 for(U i=0;i<width;++i)memory.bytes[address-memory.base+i]=static_cast<std::uint8_t>(value>>(i*8u));
}
// Written from complete9D60..9E30 disassembly before inspecting v002 CPP.
// Models only public owned operands and return AL. CPU pushes/prologue are separate.
inline std::uint8_t Run(Memory& memory,U arguments,U scratch,const Monstey::Monstey_FearUnpackedLineDispatch& dispatch) {
 Write(memory,scratch,0,1);
 for(U n=0;n<255;++n)Write(memory,scratch+1u+4u*n,0,4);
 Write(memory,scratch+1021u,0,2);Write(memory,scratch+1023u,0,1);
 Write(memory,scratch-1u,0,1);
 U index=0,used=0;
 for(;;){
  U length=Read(memory,arguments+4u),skip=0;bool delimiter=index==length;
  U byte=0;
  if(!delimiter){byte=Read(memory,Read(memory,arguments)+index,1);delimiter=byte==0;}
  if(!delimiter&&byte==13){
   if(index+1u==length){skip=1;delimiter=true;}
   else if(Read(memory,Read(memory,arguments)+index+1u,1)==10){skip=2;delimiter=true;}
  }else if(!delimiter&&byte==10){skip=1;delimiter=true;}
  if(delimiter){
   if(Read(memory,scratch-1u,1)!=0)Write(memory,scratch-1u,0,1);
   else if(used!=0){
    Write(memory,scratch+used,0,1);
    const U context=Read(memory,arguments+12u);
    const U callback=Read(memory,arguments+8u);
    if(!dispatch)throw Boundary{callback,scratch,context};
    (void)dispatch(memory,callback,scratch,context);
    length=Read(memory,arguments+4u);
   }
   used=0;if(skip>1)index+=skip-1u;
  }else if(used==0&&byte==59){Write(memory,scratch-1u,1,1);}
  else {Write(memory,scratch+used,byte,1);++used;if(used==1024)return 0;}
  ++index;if(index>length)return 1;
 }
}
}
