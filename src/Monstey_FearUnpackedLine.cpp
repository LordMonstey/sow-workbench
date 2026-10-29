#include "Monstey_FearUnpackedLine.hpp"
#include <limits>
namespace Monstey {
Monstey_FearUnpackedLineBoundaryFault::Monstey_FearUnpackedLineBoundaryFault(
 std::uint32_t callback,std::uint32_t line,std::uint32_t context)
 :std::runtime_error("Consumed unpacked-line callback boundary unavailable"),
 callback_identity(callback),line_identity(line),context_identity(context){}
namespace {
std::size_t Monstey_LineOffset(Monstey_FearDbMutableMemory m,std::uint32_t at,std::uint32_t width,bool writing) {
 if(at<m.base||static_cast<std::uint64_t>(at)+width>0x100000000ull||
    static_cast<std::uint64_t>(at)-m.base>m.bytes.size()||
    width>m.bytes.size()-static_cast<std::size_t>(static_cast<std::uint64_t>(at)-m.base))
  throw Monstey_FearDbFault(at,width,writing);
 return static_cast<std::size_t>(at-m.base);
}
std::uint32_t Monstey_LineRead(Monstey_FearDbMutableMemory m,std::uint32_t at,std::uint32_t width) {
 const auto offset=Monstey_LineOffset(m,at,width,false);std::uint32_t value=0;
 for(std::uint32_t i=0;i<width;++i)value|=std::uint32_t(m.bytes[offset+i])<<(8*i);
 return value;
}
void Monstey_LineWrite(Monstey_FearDbMutableMemory m,std::uint32_t at,std::uint32_t value,std::uint32_t width) {
 const auto offset=Monstey_LineOffset(m,at,width,true);
 for(std::uint32_t i=0;i<width;++i)m.bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::uint32_t Monstey_KeyRfind(const std::string& input,std::uint32_t from) {
 const auto position=input.rfind('/',from==UINT32_MAX?std::string::npos:static_cast<std::size_t>(from));
 return static_cast<std::uint32_t>(position);
}
std::string Monstey_KeySubstr(const std::string& input,std::uint32_t start,std::uint32_t count) {
 return input.substr(start,count==UINT32_MAX?std::string::npos:static_cast<std::size_t>(count));
}
}
std::uint8_t Monstey_FearUnpackedFrameLines(Monstey_FearDbMutableMemory& memory,
 std::uint32_t arguments,std::uint32_t scratch,const Monstey_FearUnpackedLineDispatch& dispatch) {
 Monstey_LineWrite(memory,scratch,0,1);
 for(std::uint32_t i=0;i<255;++i)Monstey_LineWrite(memory,scratch+1u+4u*i,0,4);
 Monstey_LineWrite(memory,scratch+1021u,0,2);
 Monstey_LineWrite(memory,scratch+1023u,0,1);
 Monstey_LineWrite(memory,scratch-1u,0,1);
 std::uint32_t index=0,line_length=0;
 for(;;) {
  auto length=Monstey_LineRead(memory,arguments+4u,4);
  std::uint32_t skip=0;std::uint8_t value=0;
  if(index==length)goto Monstey_LineBoundary;
  value=static_cast<std::uint8_t>(Monstey_LineRead(memory,Monstey_LineRead(memory,arguments,4)+index,1));
  if(value==0)goto Monstey_LineBoundary;
  if(value==13) {
   if(index+1u==length){skip=1;goto Monstey_LineBoundary;}
   if(Monstey_LineRead(memory,Monstey_LineRead(memory,arguments,4)+index+1u,1)==10) {
    skip=2;goto Monstey_LineBoundary;
   }
  } else if(value==10) {skip=1;goto Monstey_LineBoundary;}
  if(line_length==0&&value==';') {
   Monstey_LineWrite(memory,scratch-1u,1,1);goto Monstey_LineAdvance;
  }
  Monstey_LineWrite(memory,scratch+line_length,value,1);++line_length;
  if(line_length==1024)return 0;
  goto Monstey_LineAdvance;
Monstey_LineBoundary:
  if(Monstey_LineRead(memory,scratch-1u,1)!=0)Monstey_LineWrite(memory,scratch-1u,0,1);
  else if(line_length!=0) {
   Monstey_LineWrite(memory,scratch+line_length,0,1);
   const auto context=Monstey_LineRead(memory,arguments+12u,4);
   const auto callback=Monstey_LineRead(memory,arguments+8u,4);
   if(!dispatch)throw Monstey_FearUnpackedLineBoundaryFault(callback,scratch,context);
   (void)dispatch(memory,callback,scratch,context);
   length=Monstey_LineRead(memory,arguments+4u,4);
  }
  line_length=0;
  if(skip>1)index+=skip-1u;
Monstey_LineAdvance:
  ++index;
  if(index>length)return 1;
 }
}
void Monstey_FearUnpackedSplitKey(const std::string& input,std::string& category,
 std::string& record,std::string& attribute) {
 const auto last=Monstey_KeyRfind(input,UINT32_MAX);
 {const auto temporary=Monstey_KeySubstr(input,last+1u,UINT32_MAX);attribute=temporary;}
 const auto previous=Monstey_KeyRfind(input,last-1u);
 {const auto temporary=Monstey_KeySubstr(input,previous+1u,last-previous-1u);record=temporary;}
 {const auto temporary=Monstey_KeySubstr(input,0,previous);category=temporary;}
}
}
