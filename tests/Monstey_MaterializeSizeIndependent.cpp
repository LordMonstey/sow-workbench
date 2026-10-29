#include "Monstey_MaterializeFixture.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace Monstey;using namespace Monstey_IndependentMaterialize;
namespace {
std::uint64_t Monstey_Cases=0,Monstey_Checks=0;
void Monstey_Check(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
void Monstey_Case(std::uint32_t type,bool high,bool names,std::array<std::uint32_t,5> initial={}) {
 ++Monstey_Cases;Monstey_StringFixture f(names?"Cat/Rec/A":"//A","ab;c",type);f.high_bool=high;auto imports=f.Monstey_Imports();const Monstey_RefMemory m(f.Monstey_Memory());
 for(unsigned i=0;i<5;++i)m.Monstey_Write(f.context+4*i,initial[i]);
 Monstey_FearUnpackedSize(f.Monstey_Memory(),f.context,f.frame,imports);
 const auto extra=type==4?5u:type==6?16u:type==7?24u:type==8?32u:8u;
 const std::array<std::uint32_t,5> expected{initial[0]+(names?1u:0u),initial[1]+(names?1u:0u),initial[2]+1u,initial[3]+2u,initial[4]+(names?10u:2u)+extra};
 for(unsigned i=0;i<5;++i)Monstey_Check(m.Monstey_Read(f.context+4*i)==expected[i],"sizing native counters/AL/type dispatch/wrap mismatch");
 Monstey_Check(f.Monstey_Get(f.node+0x2C)=="ab;c;","sizing failed to append missing live semicolon");
}
}
int main(int argc,char**argv){
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");
  for(std::uint32_t type=0;type<256;++type)for(bool high:{false,true})for(bool names:{false,true})Monstey_Case(type,high,names);
  for(const auto type:std::array<std::uint32_t,4>{0x104u,0x106u,0xFFFFFFFFu,0x80000004u})Monstey_Case(type,true,true,{0xFFFFFFFFu,0xFFFFFFFEu,0xFFFFFFFFu,0xFFFFFFFEu,0xFFFFFFFCu});
  std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":2,\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<"}\n";
  std::cout<<"PASS size "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}