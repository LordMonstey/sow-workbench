#include "Monstey_MaterializeFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace Monstey;using namespace Monstey_IndependentMaterialize;
namespace {struct Monstey_Stop {}; unsigned Monstey_Checks=0; void Monstey_Check(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}}
int main(int argc,char**argv){
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");
  Monstey_StringFixture fixture("Cat/Rec/A","x;",3);auto imports=fixture.Monstey_Imports();const Monstey_RefMemory m(fixture.Monstey_Memory());
  Monstey_FearUnpackedSize(fixture.Monstey_Memory(),fixture.context,fixture.frame,imports);
  constexpr auto frame=fixture.base+0x6000u,cursor=fixture.base+0x700u,root=fixture.base+0x2000u;
  const auto initial_cursor=frame-0x80u,attribute=frame-0x60u,values=frame-0x50u,pool=frame-0x4Cu;
  m.Monstey_Write(cursor,initial_cursor);m.Monstey_Write(fixture.context+20,pool+m.Monstey_Read(fixture.context+16));
  m.Monstey_Write(frame+0x130,root);m.Monstey_Write(frame+0x134,fixture.context);m.Monstey_Write(frame+0x138,fixture.map);m.Monstey_Write(frame+0x13C,root);
  imports.atoi_bits=[](auto,auto){return std::uint32_t{0x11111111};};
  imports.strncpy=[](Monstey_FearDbMutableMemory memory,std::uint32_t destination,std::uint32_t source,std::uint32_t count){
   const Monstey_RefMemory copy(memory);bool padding=false;
   for(std::uint32_t i=0;i<count;++i){const auto byte=padding?0u:copy.Monstey_Read(source+i,1);padding=padding||byte==0;copy.Monstey_Write(destination+i,byte,1);}
  };
  auto primitive=imports.string;unsigned name_calls=0;std::uint32_t observed_value=0,observed_pool=0;bool stopped=false;
  imports.string=[&](Monstey_FearDbMutableMemory memory,Monstey_FearUnpackedStringOp op,std::uint32_t receiver,std::uint32_t a,std::uint32_t b,std::uint32_t c){
   const auto result=primitive(memory,op,receiver,a,b,c);
   if(op==Monstey_FearUnpackedStringOp::c_str&&receiver==frame-0x38u&&++name_calls==2) {
    const Monstey_RefMemory live(memory);observed_value=live.Monstey_Read(values);observed_pool=live.Monstey_Read(pool);throw Monstey_Stop{};
   }
   return result;
  };
  try{(void)Monstey_FearUnpackedMaterialize(fixture.Monstey_Memory(),cursor,frame,imports);}catch(const Monstey_Stop&){stopped=true;}
  Monstey_Check(stopped&&name_calls==2,"Expected post-attribute-constructor import checkpoint missing");
  // Native3E1D/3E22 push values atframe-4C and count atframe-50.
  // The16-byte output ends atframe-50, so6EF0 does not replace either word.
  Monstey_Check(observed_value==1u,"Native6EF0 count PUSH missing at frame-50");
  Monstey_Check(observed_pool==values,"Native6EF0 values PUSH missing at frame-4C");
  const bool attribute_correct=m.Monstey_Read(attribute)==pool&&m.Monstey_Read(attribute+4)==initial_cursor+16&&m.Monstey_Read(attribute+8,2)==1&&m.Monstey_Read(attribute+10,1)==3&&m.Monstey_Read(attribute+11,1)==0&&m.Monstey_Read(attribute+12)==values;
  Monstey_Check(attribute_correct,"Bounded fixture attribute metadata differs before isolated missing pushes");
  std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":1,\"cases\":1,\"checks\":"<<Monstey_Checks<<"}\n";
  std::cout<<"PASS native constructor-argument borrowed-storage regression\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}