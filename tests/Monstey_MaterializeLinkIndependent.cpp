#include "Monstey_MaterializeNativeReference.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Monstey;using namespace Monstey_IndependentMaterialize;
namespace {
constexpr std::uint32_t Monstey_Base=0x10000000u,Monstey_Root=Monstey_Base+0x100,Monstey_Categories=Monstey_Base+0x200,
 Monstey_Records=Monstey_Base+0x400,Monstey_Attributes=Monstey_Base+0x800,Monstey_Cells=Monstey_Base+0x1800;
std::uint64_t Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
struct Monstey_Result {bool fault=false;std::uint32_t at=0,width=0;bool write=false;bool operator==(const Monstey_Result&)const=default;};
void Monstey_Check(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
std::vector<std::uint8_t> Monstey_Fixture(unsigned seed) {
 std::vector<std::uint8_t> data(0x8000,0);const Monstey_RefMemory m({Monstey_Base,data});std::mt19937 random(seed);
 m.Monstey_Write(Monstey_Root+8,2);m.Monstey_Write(Monstey_Root+12,Monstey_Categories);
 for(std::uint32_t c=0;c<2;++c) {
  const auto cat=Monstey_Categories+16*c,records=Monstey_Records+0x100*c;
  m.Monstey_Write(cat+4,Monstey_Root);m.Monstey_Write(cat+8,2);m.Monstey_Write(cat+12,records);
  for(std::uint32_t r=0;r<2;++r) {
   const auto record=records+16*r,attributes=Monstey_Attributes+0x400*c+0x100*r;
   m.Monstey_Write(record+4,cat);m.Monstey_Write(record+8,3);m.Monstey_Write(record+12,attributes);
   for(std::uint32_t a=0;a<3;++a) {
    const auto attribute=attributes+16*a,cells=Monstey_Cells+0x400*c+0x100*r+0x20*a;
    m.Monstey_Write(attribute+4,record);m.Monstey_Write(attribute+8,4,2);m.Monstey_Write(attribute+10,9,1);m.Monstey_Write(attribute+12,cells);
    for(unsigned i=0;i<4;++i)m.Monstey_Write(cells+4*i,((random()%4u)<<16u)|(random()%4u));
   }
  }
 }
 return data;
}
void Monstey_Compare(std::vector<std::uint8_t> input,std::size_t cut=0x8000) {
 ++Monstey_Cases;auto expected=input,actual=input;Monstey_Result e,a;
 try{Monstey_RefFixLinks({Monstey_Base,std::span(expected).first(cut)},Monstey_Root);}catch(const Monstey_RefFault& f){e={true,f.address,f.width,f.writing};}
 try{Monstey_FearUnpackedFixLinks({Monstey_Base,std::span(actual).first(cut)},Monstey_Root);}catch(const Monstey_FearDbFault& f){a={true,f.address,f.width,f.writing};}
 Monstey_Check(e==a,"fixlink first consumed fault mismatch");Monstey_Check(expected==actual,"fixlink whole memory/live metadata mismatch");if(e.fault)++Monstey_Faults;
}
void Monstey_CaseFamilies() {
 for(unsigned seed=0;seed<512;++seed)Monstey_Compare(Monstey_Fixture(seed));
 for(unsigned type=0;type<256;++type) {
  auto data=Monstey_Fixture(type);const Monstey_RefMemory m({Monstey_Base,data});
  m.Monstey_Write(Monstey_Attributes+10,type,1);m.Monstey_Write(Monstey_Attributes+4,Monstey_Base-4);
  m.Monstey_Write(Monstey_Attributes+8,0,2);Monstey_Compare(std::move(data));
 }
 for(std::size_t cut=0;cut<=0x2050;++cut)Monstey_Compare(Monstey_Fixture(7),cut);
 for(const auto destination:std::vector<std::uint32_t>{Monstey_Root+8,Monstey_Root+12,Monstey_Categories+8,Monstey_Categories+12,Monstey_Records+8,Monstey_Records+12,Monstey_Attributes+4,Monstey_Attributes+8,Monstey_Attributes+12}) {
  auto data=Monstey_Fixture(1);const Monstey_RefMemory m({Monstey_Base,data});m.Monstey_Write(Monstey_Attributes+12,destination);m.Monstey_Write(Monstey_Attributes+8,1,2);Monstey_Compare(std::move(data));
 }
}
}
int main(int argc,char** argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");Monstey_CaseFamilies();
  std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":4,\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"fault_cases\":"<<Monstey_Faults<<"}\n";
  std::cout<<"PASS fixlinks "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}