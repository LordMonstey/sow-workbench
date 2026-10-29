#include "Monstey_GadbManagerPlacement.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
std::size_t Monstey_Cases=0,Monstey_Checks=0,Monstey_Groups=0;
std::uint64_t Monstey_ComparedBytes=0,Monstey_Dtos=0;
std::vector<std::string> Monstey_Failures;
void Monstey_Check(bool Monstey_Value,const char* Monstey_Label){++Monstey_Checks;if(!Monstey_Value)throw std::runtime_error(Monstey_Label);}
void Monstey_Raw(Monstey_GadbBufferedResult Monstey_A,Monstey_GadbBufferedResult Monstey_E){
 Monstey_Check(Monstey_A.rax==Monstey_E.rax,"raw RAX");Monstey_Check(Monstey_A.rdx==Monstey_E.rdx,"raw RDX");
}
void Monstey_Image(const std::vector<std::uint8_t>& Monstey_A,const std::vector<std::uint8_t>& Monstey_E){
 Monstey_ComparedBytes+=Monstey_A.size();Monstey_Check(Monstey_A==Monstey_E,"complete backing image");
}
void Monstey_Set(std::vector<std::uint8_t>& Monstey_B,std::uint64_t Monstey_Base,std::uint64_t Monstey_At,std::uint64_t Monstey_Value,std::size_t Monstey_Width){
 const auto Monstey_O=static_cast<std::size_t>(Monstey_At-Monstey_Base);
 for(std::size_t Monstey_I=0;Monstey_I<Monstey_Width;++Monstey_I)Monstey_B[Monstey_O+Monstey_I]=static_cast<std::uint8_t>(Monstey_Value>>(8*Monstey_I));
}
std::uint64_t Monstey_Get(const std::vector<std::uint8_t>& Monstey_B,std::uint64_t Monstey_Base,std::uint64_t Monstey_At,std::size_t Monstey_Width){
 const auto Monstey_O=static_cast<std::size_t>(Monstey_At-Monstey_Base);std::uint64_t Monstey_Value=0;
 for(std::size_t Monstey_I=0;Monstey_I<Monstey_Width;++Monstey_I)Monstey_Value|=std::uint64_t(Monstey_B[Monstey_O+Monstey_I])<<(8*Monstey_I);
 return Monstey_Value;
}
struct Monstey_Fixture{
 std::uint64_t Monstey_Base;std::vector<std::uint8_t> Monstey_Bytes;std::size_t Monstey_Visible;
 explicit Monstey_Fixture(std::uint64_t Monstey_B=0,std::size_t Monstey_N=4096):Monstey_Base(Monstey_B),Monstey_Bytes(Monstey_N,0xA5),Monstey_Visible(Monstey_N){}
 std::uint64_t Monstey_At(std::uint64_t Monstey_O)const{return Monstey_Base+Monstey_O;}
 Monstey_GadbBinderMemory Monstey_Memory(){return {Monstey_Base,std::span<std::uint8_t>(Monstey_Bytes).first(Monstey_Visible)};}
 void Monstey_Put(std::uint64_t Monstey_A,std::uint64_t Monstey_V,std::size_t Monstey_W=8){Monstey_Set(Monstey_Bytes,Monstey_Base,Monstey_A,Monstey_V,Monstey_W);}
 Monstey_GadbManagerPlacementCells Monstey_Cells()const{return {Monstey_At(0x80),Monstey_At(0x88),Monstey_At(0x90),Monstey_At(0xA0)};}
 void Monstey_Cas(std::uint32_t Monstey_Selector=5,std::uint32_t Monstey_Policy=5,std::uint32_t Monstey_Compare=7,std::uint32_t Monstey_Old=7){
  const auto Monstey_C=Monstey_Cells();for(std::uint64_t Monstey_I=0;Monstey_I<6;++Monstey_I)Monstey_Put(Monstey_C.order_table+4*Monstey_I,Monstey_Policy,4);
  Monstey_Put(Monstey_At(0x228),Monstey_Selector,4);Monstey_Put(Monstey_At(0x300),Monstey_Compare,4);Monstey_Put(Monstey_At(0x340),Monstey_Old,4);
 }
};
template<class Monstey_F>void Monstey_Run(const std::string& Monstey_Name,Monstey_F Monstey_Function){
 ++Monstey_Cases;try{Monstey_Function();}catch(const std::exception& Monstey_E){Monstey_Failures.push_back(Monstey_Name+": "+Monstey_E.what());}catch(...){Monstey_Failures.push_back(Monstey_Name+": nonstandard exception");}
}
template<class Monstey_F>void Monstey_MemoryFault(Monstey_F Monstey_Function,std::uint64_t Monstey_A,std::size_t Monstey_W,bool Monstey_Writing){
 try{Monstey_Function();}catch(const Monstey_GadbBufferedMemoryFault& Monstey_E){Monstey_Check(Monstey_E.address==Monstey_A,"first fault address");Monstey_Check(Monstey_E.width==Monstey_W,"first fault width");Monstey_Check(Monstey_E.writing==Monstey_Writing,"first fault read/write");return;}throw std::runtime_error("expected ordinary memory fault");
}
template<class Monstey_F>void Monstey_Missing(Monstey_F Monstey_Function,std::uint64_t Monstey_Site,std::uint64_t Monstey_Target,std::uint32_t Monstey_Compare,std::uint32_t Monstey_Desired){
 try{Monstey_Function();}catch(const Monstey_GadbManagerCompareExchange32Fault& Monstey_E){Monstey_Check(Monstey_E.native_site==Monstey_Site,"missing native site");Monstey_Check(Monstey_E.address==Monstey_Target,"missing target");Monstey_Check(Monstey_E.width==4,"missing width");Monstey_Check(Monstey_E.comparand==Monstey_Compare,"missing comparand");Monstey_Check(Monstey_E.desired==Monstey_Desired,"missing desired");return;}throw std::runtime_error("expected missing primitive fault");
}
void Monstey_CasResult(Monstey_GadbManagerCasResult Monstey_A,std::uint64_t Monstey_Status,std::uint64_t Monstey_Expected,std::uint64_t Monstey_Desired,std::uint64_t Monstey_Target){
 Monstey_Raw(Monstey_A.raw,{Monstey_Status,Monstey_Expected});Monstey_Check(Monstey_A.desired_register==Monstey_Desired,"full desired register");Monstey_Check(Monstey_A.target_identity==Monstey_Target,"produced target identity");
}
void Monstey_Dto(const Monstey_GadbManagerCompareExchange32Call& Monstey_C,std::uint64_t Monstey_Site,std::uint64_t Monstey_Target,std::uint32_t Monstey_Compare,std::uint32_t Monstey_Desired){
 ++Monstey_Dtos;Monstey_Check(Monstey_C.native_site==Monstey_Site,"DTO native site");Monstey_Check(Monstey_C.address==Monstey_Target,"DTO target");Monstey_Check(Monstey_C.comparand==Monstey_Compare,"DTO comparand");Monstey_Check(Monstey_C.desired==Monstey_Desired,"DTO desired");
}
std::uint32_t Monstey_Atomic(Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_C){
 if(Monstey_C.address<Monstey_M.base||Monstey_C.address>std::numeric_limits<std::uint64_t>::max()-3||Monstey_C.address-Monstey_M.base>Monstey_M.bytes.size()||4>Monstey_M.bytes.size()-static_cast<std::size_t>(Monstey_C.address-Monstey_M.base))throw Monstey_GadbBufferedMemoryFault(Monstey_C.address,4,false);
 const auto Monstey_O=static_cast<std::size_t>(Monstey_C.address-Monstey_M.base);std::uint32_t Monstey_Old=0;
 for(std::size_t Monstey_I=0;Monstey_I<4;++Monstey_I)Monstey_Old|=std::uint32_t(Monstey_M.bytes[Monstey_O+Monstey_I])<<(8*Monstey_I);
 if(Monstey_Old==Monstey_C.comparand)for(std::size_t Monstey_I=0;Monstey_I<4;++Monstey_I)Monstey_M.bytes[Monstey_O+Monstey_I]=static_cast<std::uint8_t>(Monstey_C.desired>>(8*Monstey_I));
 return Monstey_Old;
}
struct Monstey_ProviderFailure final:std::runtime_error{int Monstey_Tag;explicit Monstey_ProviderFailure(int Monstey_T):std::runtime_error("declared primitive prefix exception"),Monstey_Tag(Monstey_T){}};
template<class Monstey_F>void Monstey_ProviderFault(Monstey_F Monstey_Function,int Monstey_Tag){
 try{Monstey_Function();}catch(const Monstey_ProviderFailure& Monstey_E){Monstey_Check(Monstey_E.Monstey_Tag==Monstey_Tag,"provider exception identity");return;}throw std::runtime_error("expected provider exception");
}
using Monstey_ResetFunction=Monstey_GadbBufferedResult(*)(Monstey_GadbBinderMemory,std::uint64_t,std::uint64_t);
const std::array<Monstey_ResetFunction,3> Monstey_Resets={Monstey_GadbResetManagerBackend40,Monstey_GadbResetManagerBackend80,Monstey_GadbResetManagerBackendC0};
using Monstey_TailFunction=Monstey_GadbManagerTailResult(*)(Monstey_GadbBinderMemory,std::uint64_t,std::uint64_t,std::uint64_t,Monstey_GadbManagerPlacementCells);
const std::array<Monstey_TailFunction,3> Monstey_Tails={Monstey_GadbInitializeManagerBackendTail40,Monstey_GadbInitializeManagerBackendTail80,Monstey_GadbInitializeManagerBackendTailC0};
std::uint64_t& Monstey_GlobalCell(Monstey_GadbManagerPlacementCells& Monstey_C,std::size_t Monstey_V){return Monstey_V==0?Monstey_C.tail_40:Monstey_V==1?Monstey_C.tail_80:Monstey_C.tail_c0;}
void Monstey_ResetPrefix(std::vector<std::uint8_t>& Monstey_E,std::uint64_t Monstey_Base,std::uint64_t Monstey_O,std::uint32_t Monstey_Count){
 for(std::uint32_t Monstey_I=0;Monstey_I<Monstey_Count;++Monstey_I){const auto Monstey_L=Monstey_O+0x58u+std::uint64_t(Monstey_I)*16u;const auto Monstey_H=Monstey_L-0x10u;Monstey_Set(Monstey_E,Monstey_Base,Monstey_L+8u,Monstey_H,8);Monstey_Set(Monstey_E,Monstey_Base,Monstey_L,Monstey_H,8);}
}
void Monstey_ResetCases(){
 ++Monstey_Groups;
 for(std::size_t Monstey_V=0;Monstey_V<3;++Monstey_V){
  for(const std::uint64_t Monstey_O:{0ULL,0x100ULL,0xFEDC000000000100ULL})
  Monstey_Run("reset/complete/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_O),[=]{
   const auto Monstey_Base=Monstey_O>0x100?0xFEDC000000000000ULL:0ULL;Monstey_Fixture Monstey_F(Monstey_Base);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_ResetPrefix(Monstey_E,Monstey_Base,Monstey_O,32);
   Monstey_Raw(Monstey_Resets[Monstey_V](Monstey_F.Monstey_Memory(),Monstey_O,0xFEDCBA9876543210ULL),{Monstey_O+0x238u,0xFEDCBA9876543210ULL});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
   Monstey_Check(Monstey_Get(Monstey_F.Monstey_Bytes,Monstey_Base,Monstey_O+0x58,8)==Monstey_O+0x48,"literal first low store");Monstey_Check(Monstey_Get(Monstey_F.Monstey_Bytes,Monstey_Base,Monstey_O+0x250,8)==Monstey_O+0x238,"literal last high store");
  });
  for(const std::uint32_t Monstey_I:{0u,1u,7u,31u})
  Monstey_Run("reset/high-store-first-fault/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_I),[=]{
   Monstey_Fixture Monstey_F;const std::uint64_t Monstey_O=0x100;const auto Monstey_High=Monstey_O+0x60+std::uint64_t(Monstey_I)*16;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_ResetPrefix(Monstey_E,0,Monstey_O,Monstey_I);Monstey_F.Monstey_Visible=static_cast<std::size_t>(Monstey_High)+7;
   Monstey_MemoryFault([&]{(void)Monstey_Resets[Monstey_V](Monstey_F.Monstey_Memory(),Monstey_O,0x77);},Monstey_High,8,true);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  Monstey_Run("reset/second-low-fault-after-high/"+std::to_string(Monstey_V),[=]{
   Monstey_Fixture Monstey_F(0x160);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0x160,0x160,0x148,8);
   Monstey_MemoryFault([&]{(void)Monstey_Resets[Monstey_V](Monstey_F.Monstey_Memory(),0x100,0x77);},0x158,8,true);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  Monstey_Run("reset/wrapped-object-complete/"+std::to_string(Monstey_V),[=]{
   Monstey_Fixture Monstey_F;const auto Monstey_O=std::numeric_limits<std::uint64_t>::max()-0x40;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_ResetPrefix(Monstey_E,0,Monstey_O,32);
   Monstey_Raw(Monstey_Resets[Monstey_V](Monstey_F.Monstey_Memory(),Monstey_O,0x77),{Monstey_O+0x238,0x77});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  Monstey_Run("reset/wrapped-first-then-U64-crossing/"+std::to_string(Monstey_V),[=]{
   Monstey_Fixture Monstey_F;const auto Monstey_O=std::numeric_limits<std::uint64_t>::max()-0x5B;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,4,Monstey_O+0x48,8);
   Monstey_MemoryFault([&]{(void)Monstey_Resets[Monstey_V](Monstey_F.Monstey_Memory(),Monstey_O,0x77);},std::numeric_limits<std::uint64_t>::max()-3,8,true);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
 }
}
void Monstey_TailCases(){
 ++Monstey_Groups;const std::array<std::uint64_t,8> Monstey_Pads={0,7,6,5,4,3,2,1};
 for(std::size_t Monstey_V=0;Monstey_V<3;++Monstey_V){
  for(std::uint64_t Monstey_R=0;Monstey_R<8;++Monstey_R)
  for(const std::uint64_t Monstey_N:{0ULL,3ULL,0x80ULL,0xFFFFFFFFFFFFFFFFULL})
  Monstey_Run("tail/alignment-underflow-wrap/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_R)+"/"+std::to_string(Monstey_N),[=]{
   Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();const auto Monstey_G=Monstey_GlobalCell(Monstey_C,Monstey_V);Monstey_F.Monstey_Put(Monstey_G,0x1234567890ABCDEFULL);
   const auto Monstey_Start=0x500+Monstey_R;const auto Monstey_Q=Monstey_Start+Monstey_Pads[static_cast<std::size_t>(Monstey_R)];const auto Monstey_Rem=Monstey_N-Monstey_Pads[static_cast<std::size_t>(Monstey_R)];auto Monstey_E=Monstey_F.Monstey_Bytes;
   Monstey_Set(Monstey_E,0,0x128,Monstey_Q,8);Monstey_Set(Monstey_E,0,0x110,Monstey_Rem,8);Monstey_Set(Monstey_E,0,Monstey_Q+8,Monstey_Rem|1,8);Monstey_Set(Monstey_E,0,Monstey_Start+Monstey_N+8,0x48,8);Monstey_Set(Monstey_E,0,0x130,0x1234567890ABCDEFULL,8);
   const auto Monstey_A=Monstey_Tails[Monstey_V](Monstey_F.Monstey_Memory(),0x100,Monstey_Start,Monstey_N,Monstey_C);Monstey_Raw(Monstey_A.raw,{0x1234567890ABCDEFULL,Monstey_Q});Monstey_Check(Monstey_A.remaining==Monstey_Rem,"full outgoing remaining");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  for(const std::uint64_t Monstey_G:{0x128ULL,0x110ULL,0x508ULL,0x588ULL})
  Monstey_Run("tail/late-global-alias/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_G),[=]{
   Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_GlobalCell(Monstey_C,Monstey_V)=Monstey_G;Monstey_F.Monstey_Put(Monstey_G,0x9876543210ABCDEFULL);auto Monstey_E=Monstey_F.Monstey_Bytes;
   Monstey_Set(Monstey_E,0,0x128,0x500,8);Monstey_Set(Monstey_E,0,0x110,0x80,8);Monstey_Set(Monstey_E,0,0x508,0x81,8);Monstey_Set(Monstey_E,0,0x588,0x48,8);const auto Monstey_Live=Monstey_Get(Monstey_E,0,Monstey_G,8);Monstey_Set(Monstey_E,0,0x130,Monstey_Live,8);
   const auto Monstey_A=Monstey_Tails[Monstey_V](Monstey_F.Monstey_Memory(),0x100,0x500,0x80,Monstey_C);Monstey_Raw(Monstey_A.raw,{Monstey_Live,0x500});Monstey_Check(Monstey_A.remaining==0x80,"remaining after aliases");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  for(const std::uint64_t Monstey_Start:{0x101ULL,0x118ULL,0x12BULL})
  Monstey_Run("tail/object-chunk-partial-alias/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_Start),[=]{
   Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();const auto Monstey_G=Monstey_GlobalCell(Monstey_C,Monstey_V);Monstey_F.Monstey_Put(Monstey_G,0x33);const auto Monstey_Pad=Monstey_Pads[static_cast<std::size_t>(Monstey_Start&7)];const auto Monstey_Q=Monstey_Start+Monstey_Pad;const auto Monstey_Rem=std::uint64_t{0x10}-Monstey_Pad;auto Monstey_E=Monstey_F.Monstey_Bytes;
   Monstey_Set(Monstey_E,0,0x128,Monstey_Q,8);Monstey_Set(Monstey_E,0,0x110,Monstey_Rem,8);Monstey_Set(Monstey_E,0,Monstey_Q+8,Monstey_Rem|1,8);Monstey_Set(Monstey_E,0,Monstey_Start+0x10+8,0x48,8);Monstey_Set(Monstey_E,0,0x130,0x33,8);
   const auto Monstey_A=Monstey_Tails[Monstey_V](Monstey_F.Monstey_Memory(),0x100,Monstey_Start,0x10,Monstey_C);Monstey_Raw(Monstey_A.raw,{0x33,Monstey_Q});Monstey_Check(Monstey_A.remaining==Monstey_Rem,"cached remaining under overlap");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
  for(int Monstey_Stage=0;Monstey_Stage<6;++Monstey_Stage)
  Monstey_Run("tail/ordered-first-fault/"+std::to_string(Monstey_V)+"/"+std::to_string(Monstey_Stage),[=]{
   const std::uint64_t Monstey_Base=Monstey_Stage==1?0x120:0;Monstey_Fixture Monstey_F(Monstey_Base);auto Monstey_C=Monstey_F.Monstey_Cells();const std::uint64_t Monstey_Q=Monstey_Stage==5?0x40:0x700;const std::uint64_t Monstey_N=Monstey_Stage==5?0x20:0x80;
   Monstey_GlobalCell(Monstey_C,Monstey_V)=Monstey_Stage==4?0x900:Monstey_Stage==5?0x80:Monstey_F.Monstey_At(0x80);
   const std::array<std::uint64_t,6> Monstey_FaultAt={0x128,0x110,Monstey_Q+8,Monstey_Q+Monstey_N+8,Monstey_GlobalCell(Monstey_C,Monstey_V),0x130};
   if(Monstey_Stage==1){}else Monstey_F.Monstey_Visible=static_cast<std::size_t>(Monstey_FaultAt[static_cast<std::size_t>(Monstey_Stage)])+7;
   auto Monstey_E=Monstey_F.Monstey_Bytes;
   if(Monstey_Stage>=1)Monstey_Set(Monstey_E,Monstey_Base,0x128,Monstey_Q,8);
   if(Monstey_Stage>=2)Monstey_Set(Monstey_E,Monstey_Base,0x110,Monstey_N,8);
   if(Monstey_Stage>=3)Monstey_Set(Monstey_E,Monstey_Base,Monstey_Q+8,Monstey_N|1,8);
   if(Monstey_Stage>=4)Monstey_Set(Monstey_E,Monstey_Base,Monstey_Q+Monstey_N+8,0x48,8);
   Monstey_MemoryFault([&]{(void)Monstey_Tails[Monstey_V](Monstey_F.Monstey_Memory(),0x100,Monstey_Q,Monstey_N,Monstey_C);},Monstey_FaultAt[static_cast<std::size_t>(Monstey_Stage)],8,Monstey_Stage!=4);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  });
 }
}
void Monstey_CasCases(){
 ++Monstey_Groups;
 const std::array<std::uint64_t,6> Monstey_Sites={0x14059D608ULL,0x14059D608ULL,0x14059D608ULL,0x14059D608ULL,0x14059D5F3ULL,0x14059D5F3ULL};
 const std::array<std::uint32_t,4> Monstey_Olds={0,1,0x80000000u,0xFFFFFFFFu};
 for(std::uint32_t Monstey_Selector=0;Monstey_Selector<6;++Monstey_Selector)
 for(std::uint32_t Monstey_Policy=0;Monstey_Policy<6;++Monstey_Policy)
 for(const bool Monstey_Success:{false,true})
 Monstey_Run("cas/policy-site-observed-old/"+std::to_string(Monstey_Selector)+"/"+std::to_string(Monstey_Policy)+"/"+std::to_string(Monstey_Success),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();const auto Monstey_Old=Monstey_Olds[static_cast<std::size_t>((Monstey_Selector+Monstey_Policy)%4)];const auto Monstey_Compare=Monstey_Success?Monstey_Old:Monstey_Old^0x12345678u;
  const std::uint64_t Monstey_Desired=0xFEDCBA98AABBCCDDULL;Monstey_F.Monstey_Cas(Monstey_Selector,Monstey_Policy,Monstey_Compare,Monstey_Old);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,Monstey_Success?0x340:0x300,Monstey_Success?0xAABBCCDDu:Monstey_Old,4);
  Monstey_GadbManagerPlacementServices Monstey_S;std::size_t Monstey_Calls=0;Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){++Monstey_Calls;Monstey_Dto(Monstey_Call,Monstey_Sites[Monstey_Policy],0x340,Monstey_Compare,0xAABBCCDDu);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,Monstey_Desired,0x200,Monstey_S,Monstey_C),Monstey_Success?1:0,0x300,Monstey_Desired,0x340);Monstey_Check(Monstey_Calls==1,"one consumed CMPXCHG");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 for(const std::uint32_t Monstey_Selector:{6u,0xFFFFFFFFu})
 Monstey_Run("cas/selector-above5-no-table/"+std::to_string(Monstey_Selector),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(Monstey_Selector,0,0,0);Monstey_C.order_table=std::numeric_limits<std::uint64_t>::max();auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,0x77,4);
  Monstey_GadbManagerPlacementServices Monstey_S;Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,0,0x77);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,0x123400000077ULL,0x200,Monstey_S,Monstey_C),1,0x300,0x123400000077ULL,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 for(const std::uint32_t Monstey_Policy:{0xFFFFFFFFu,0x80000000u,6u,0x7FFFFFFFu})
 for(const std::uint32_t Monstey_Selector:{0u,5u})
 Monstey_Run("cas/invalid-policy-skips-expected-and-service/"+std::to_string(Monstey_Policy)+"/"+std::to_string(Monstey_Selector),[=]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(Monstey_Selector,Monstey_Policy);const auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_GadbManagerPlacementServices Monstey_S;const auto Monstey_Expected=std::numeric_limits<std::uint64_t>::max()-1;
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),std::numeric_limits<std::uint64_t>::max(),Monstey_Expected,0xFFEEDDCC11223344ULL,0x200,Monstey_S,Monstey_C),0,Monstey_Expected,0xFFEEDDCC11223344ULL,std::numeric_limits<std::uint64_t>::max());Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 for(const int Monstey_Delta:{-3,-1,0,1,3})
 Monstey_Run("cas/expected-target-partial-alias/"+std::to_string(Monstey_Delta),[=]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas();const std::uint64_t Monstey_Target=0x340;const auto Monstey_Expected=Monstey_Target+static_cast<std::uint64_t>(Monstey_Delta);Monstey_F.Monstey_Put(Monstey_Expected,0x7C6B5A49u,4);
  const auto Monstey_Compare=static_cast<std::uint32_t>(Monstey_Get(Monstey_F.Monstey_Bytes,0,Monstey_Expected,4));const auto Monstey_Old=static_cast<std::uint32_t>(Monstey_Get(Monstey_F.Monstey_Bytes,0,Monstey_Target,4));const bool Monstey_Success=Monstey_Old==Monstey_Compare;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,Monstey_Success?Monstey_Target:Monstey_Expected,Monstey_Success?0x11223344u:Monstey_Old,4);
  Monstey_GadbManagerPlacementServices Monstey_S;Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,Monstey_Target,Monstey_Compare,0x11223344u);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),Monstey_Target,Monstey_Expected,0x8899AABB11223344ULL,0x200,Monstey_S,Monstey_C),Monstey_Success?1:0,Monstey_Expected,0x8899AABB11223344ULL,Monstey_Target);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("cas/zero-numeric-target-is-dispatched",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,9,0);Monstey_F.Monstey_Put(0,9,4);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0,0xAA55,4);Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0,9,0xAA55);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0,0x300,0xAA55,0x200,Monstey_S,Monstey_C),1,0x300,0xAA55,0);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
}
void Monstey_FrameCases(){
 ++Monstey_Groups;
 for(const std::uint64_t Monstey_FBody:{0x200ULL,0x900ULL,0xFFFFFFFFFFFFFFEFULL})
 Monstey_Run("wrapper/known-frame-and-child/"+std::to_string(Monstey_FBody),[=]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(0,5,5,5);Monstey_F.Monstey_Put(Monstey_C.order_table,6,4);if(Monstey_FBody==0x200)Monstey_F.Monstey_Put(0x228,0,4);
  auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,Monstey_FBody+0x20,5,4);Monstey_Set(Monstey_E,0,0x340,0xFFFFFFFFu,4);Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,5,0xFFFFFFFFu);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerState(Monstey_F.Monstey_Memory(),0x340,0x300,Monstey_FBody,Monstey_S,Monstey_C),1,0x300,0xFFFFFFFFULL,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("wrapper/frame-slot-aliases-expected",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(0,5,0,5);Monstey_F.Monstey_Put(0x220,0x99,4);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x220,5,4);Monstey_Set(Monstey_E,0,0x340,0xFFFFFFFFu,4);Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,5,0xFFFFFFFFu);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerState(Monstey_F.Monstey_Memory(),0x340,0x220,0x200,Monstey_S,Monstey_C),1,0x220,0xFFFFFFFFULL,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("wrapper/frame-slot-aliases-table5",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(0,0,5,5);const auto Monstey_FBody=Monstey_C.order_table+20-0x20;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,Monstey_FBody+0x20,5,4);Monstey_Set(Monstey_E,0,0x340,0xFFFFFFFFu,4);Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,5,0xFFFFFFFFu);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerState(Monstey_F.Monstey_Memory(),0x340,0x300,Monstey_FBody,Monstey_S,Monstey_C),1,0x300,0xFFFFFFFFULL,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("wrapper/frame-first-fault",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_GadbManagerPlacementServices Monstey_S;const auto Monstey_E=Monstey_F.Monstey_Bytes;
  Monstey_MemoryFault([&]{(void)Monstey_GadbCompareExchangeManagerState(Monstey_F.Monstey_Memory(),0,0,0x1200,Monstey_S,Monstey_C);},0x1220,4,true);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
}
void Monstey_CasFaultCases(){
 ++Monstey_Groups;
 for(int Monstey_Stage=0;Monstey_Stage<3;++Monstey_Stage)
 for(std::size_t Monstey_Cut=0;Monstey_Cut<4;++Monstey_Cut)
 Monstey_Run("cas/consumed-fault-before-missing/"+std::to_string(Monstey_Stage)+"/"+std::to_string(Monstey_Cut),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas();std::uint64_t Monstey_At=0x228;
  if(Monstey_Stage==1){Monstey_C.order_table=0x500;Monstey_At=0x514;}else if(Monstey_Stage==2)Monstey_At=0x300;
  Monstey_F.Monstey_Visible=static_cast<std::size_t>(Monstey_At)+Monstey_Cut;const auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_MemoryFault([&]{(void)Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,0x77,0x200,Monstey_S,Monstey_C);},Monstey_At,4,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 for(const std::uint32_t Monstey_Policy:{0u,5u})
 Monstey_Run("cas/missing-opcode-no-target-preflight/"+std::to_string(Monstey_Policy),[=]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,Monstey_Policy,0x12345678u,0);const auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_GadbManagerPlacementServices Monstey_S;const auto Monstey_Target=std::numeric_limits<std::uint64_t>::max();
  Monstey_Missing([&]{(void)Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),Monstey_Target,0x300,0xAABBCCDD87654321ULL,0x200,Monstey_S,Monstey_C);},Monstey_Policy==0?0x14059D608ULL:0x14059D5F3ULL,Monstey_Target,0x12345678u,0x87654321u);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("cas/present-provider-no-component-target-probe",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas();auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_E[0x10]=0xEB;Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call)->std::uint32_t{Monstey_Dto(Monstey_Call,0x14059D5F3ULL,std::numeric_limits<std::uint64_t>::max(),7,0x77);Monstey_F.Monstey_Bytes[0x10]=0xEB;throw Monstey_ProviderFailure(71);};
  Monstey_ProviderFault([&]{(void)Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),std::numeric_limits<std::uint64_t>::max(),0x300,0x77,0x200,Monstey_S,Monstey_C);},71);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("cas/partial-provider-exception-no-followup-write",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,7,3);auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,0xEEFFAA55u,4);Monstey_E[0x10]=0xEB;Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call&)->std::uint32_t{Monstey_F.Monstey_Put(0x340,0xEEFFAA55u,4);Monstey_F.Monstey_Bytes[0x10]=0xEB;throw Monstey_ProviderFailure(121);};
  Monstey_ProviderFault([&]{(void)Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,0x77,0x200,Monstey_S,Monstey_C);},121);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
}
struct Monstey_Token{
 std::vector<int>* Monstey_Order;std::vector<std::uint8_t>* Monstey_Bytes;bool Monstey_WriteExpected;
 Monstey_Token(std::vector<int>& Monstey_O,std::vector<std::uint8_t>& Monstey_B,bool Monstey_W):Monstey_Order(&Monstey_O),Monstey_Bytes(&Monstey_B),Monstey_WriteExpected(Monstey_W){}
 ~Monstey_Token(){Monstey_Order->push_back(9);(*Monstey_Bytes)[0x10]=0xD9;if(Monstey_WriteExpected)Monstey_Set(*Monstey_Bytes,0,0x300,0xDEADBEEFu,4);}
};
void Monstey_LifetimeCases(){
 ++Monstey_Groups;
 Monstey_Run("callable/mutable-state-across-real-operations",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,1,1);std::vector<std::uint64_t> Monstey_Sequence;Monstey_GadbManagerPlacementServices Monstey_S;
  Monstey_S.compare_exchange32=[&,Monstey_N=std::uint64_t{0}](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call)mutable{Monstey_Sequence.push_back(++Monstey_N);return Monstey_Atomic(Monstey_M,Monstey_Call);};
  auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,2,4);
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,2,0x200,Monstey_S,Monstey_C),1,0x300,2,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  Monstey_F.Monstey_Put(0x300,2,4);Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,3,4);
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,3,0x200,Monstey_S,Monstey_C),1,0x300,3,0x340);Monstey_Check(Monstey_Sequence==std::vector<std::uint64_t>({1,2}),"persistent mutable closure state");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("callable/self-clear-retirement-before-real-mismatch-store",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,7,3);std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPlacementServices Monstey_S;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x300,3,4);Monstey_E[0x10]=0xD9;
  auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes,true);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.compare_exchange32=[&,Monstey_T](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){
   Monstey_Order.push_back(1);Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,7,2);const auto Monstey_Old=Monstey_Atomic(Monstey_M,Monstey_Call);Monstey_S.compare_exchange32={};Monstey_Check(!Monstey_Weak.expired(),"active target survives clear");return Monstey_Old;};Monstey_T.reset();
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,2,0x200,Monstey_S,Monstey_C),0,0x300,2,0x340);Monstey_Check(Monstey_Weak.expired(),"target retired after call");Monstey_Check(Monstey_Order==std::vector<int>({1,9}),"observable target lifetime order");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
  Monstey_Missing([&]{(void)Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,2,0x200,Monstey_S,Monstey_C);},0x14059D5F3ULL,0x340,3,2);
 });
 Monstey_Run("callable/replacement-and-real-nested-leaf",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,1,1);Monstey_F.Monstey_Put(0x320,7,4);Monstey_F.Monstey_Put(0x360,8,4);std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPlacementServices Monstey_S;
  auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,2,4);Monstey_Set(Monstey_E,0,0x320,8,4);Monstey_E[0x10]=0xD9;auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes,false);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.compare_exchange32=[&,Monstey_T](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){
   Monstey_Order.push_back(1);Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,1,2);const auto Monstey_Old=Monstey_Atomic(Monstey_M,Monstey_Call);
   Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_N,const Monstey_GadbManagerCompareExchange32Call& Monstey_Nested){Monstey_Order.push_back(2);Monstey_Dto(Monstey_Nested,0x14059D5F3ULL,0x360,7,3);return Monstey_Atomic(Monstey_N,Monstey_Nested);};
   Monstey_Check(!Monstey_Weak.expired(),"active target survives replacement");Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_M,0x360,0x320,0x8877665500000003ULL,0x200,Monstey_S,Monstey_C),0,0x320,0x8877665500000003ULL,0x360);Monstey_Order.push_back(3);return Monstey_Old;};Monstey_T.reset();
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,0xAABBCCDD00000002ULL,0x200,Monstey_S,Monstey_C),1,0x300,0xAABBCCDD00000002ULL,0x340);Monstey_Check(Monstey_Weak.expired(),"old target retired");Monstey_Check(Monstey_Order==std::vector<int>({1,2,3,9}),"replacement reentry order");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("callable/partial-exception-retirement-and-wrapper-prefix",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,7,3);std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPlacementServices Monstey_S;
  auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x220,5,4);Monstey_Set(Monstey_E,0,0x340,0x778899AAu,4);Monstey_Set(Monstey_E,0,0x300,0xDEADBEEFu,4);Monstey_E[0x10]=0xD9;Monstey_E[0x11]=0xEB;
  auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes,true);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.compare_exchange32=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call&)->std::uint32_t{Monstey_Order.push_back(1);Monstey_S.compare_exchange32={};Monstey_Check(!Monstey_Weak.expired(),"active target during exception prefix");Monstey_F.Monstey_Put(0x340,0x778899AAu,4);Monstey_F.Monstey_Bytes[0x11]=0xEB;throw Monstey_ProviderFailure(181);};Monstey_T.reset();
  Monstey_ProviderFault([&]{(void)Monstey_GadbCompareExchangeManagerState(Monstey_F.Monstey_Memory(),0x340,0x300,0x200,Monstey_S,Monstey_C);},181);Monstey_Check(Monstey_Weak.expired(),"retirement after exception");Monstey_Check(Monstey_Order==std::vector<int>({1,9}),"exception retirement order");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
 Monstey_Run("callable/nested-RMW-no-component-readback",[]{
  Monstey_Fixture Monstey_F;const auto Monstey_C=Monstey_F.Monstey_Cells();Monstey_F.Monstey_Cas(5,5,1,1);Monstey_F.Monstey_Put(0x320,2,4);Monstey_GadbManagerPlacementServices Monstey_S;auto Monstey_E=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_E,0,0x340,3,4);
  Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_M,const Monstey_GadbManagerCompareExchange32Call& Monstey_Call){
   Monstey_Dto(Monstey_Call,0x14059D5F3ULL,0x340,1,2);const auto Monstey_Old=Monstey_Atomic(Monstey_M,Monstey_Call);
   Monstey_S.compare_exchange32=[&](Monstey_GadbBinderMemory Monstey_N,const Monstey_GadbManagerCompareExchange32Call& Monstey_Inner){Monstey_Dto(Monstey_Inner,0x14059D5F3ULL,0x340,2,3);return Monstey_Atomic(Monstey_N,Monstey_Inner);};
   Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_M,0x340,0x320,3,0x200,Monstey_S,Monstey_C),1,0x320,3,0x340);return Monstey_Old;};
  Monstey_CasResult(Monstey_GadbCompareExchangeManagerWord(Monstey_F.Monstey_Memory(),0x340,0x300,2,0x200,Monstey_S,Monstey_C),1,0x300,2,0x340);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_E);
 });
}
std::string Monstey_Quote(const std::string& Monstey_V){
 std::string Monstey_R="\"";for(const char Monstey_C:Monstey_V){if(Monstey_C=='"'||Monstey_C=='\\')Monstey_R+='\\';if(Monstey_C=='\n')Monstey_R+="\\n";else Monstey_R+=Monstey_C;}return Monstey_R+'"';
}
}
int main(){
 Monstey_ResetCases();Monstey_TailCases();Monstey_CasCases();Monstey_FrameCases();Monstey_CasFaultCases();Monstey_LifetimeCases();
 std::cout<<"{\"status\":\""<<(Monstey_Failures.empty()?"PASS":"FAIL")<<"\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"compared_backing_bytes\":"<<Monstey_ComparedBytes<<",\"checked_CMPXCHG_DTOs\":"<<Monstey_Dtos<<",\"failures\":[";
 for(std::size_t Monstey_I=0;Monstey_I<Monstey_Failures.size();++Monstey_I){if(Monstey_I)std::cout<<',';std::cout<<Monstey_Quote(Monstey_Failures[Monstey_I]);}
 std::cout<<"],\"actual_compiled_product\":true,\"per_read_observer\":false,\"physical_atomic_or_original_OS_called\":false}\n";
 return Monstey_Failures.empty()?0:1;
}
