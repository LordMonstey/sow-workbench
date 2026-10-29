#include "Monstey_GadbManagerPrimitives.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
std::size_t Monstey_Cases=0, Monstey_Checks=0, Monstey_Groups=0;
std::uint64_t Monstey_ComparedBytes=0, Monstey_Events=0;
std::vector<std::string> Monstey_Failures;
void Monstey_Check(bool Monstey_Value,const char* Monstey_Label) {
  ++Monstey_Checks;
  if(!Monstey_Value)throw std::runtime_error(Monstey_Label);
}
void Monstey_Raw(Monstey_GadbBufferedResult Monstey_Actual,
                 Monstey_GadbBufferedResult Monstey_Expected) {
  Monstey_Check(Monstey_Actual.rax==Monstey_Expected.rax,"raw RAX");
  Monstey_Check(Monstey_Actual.rdx==Monstey_Expected.rdx,"raw RDX");
}
void Monstey_Image(const std::vector<std::uint8_t>& Monstey_Actual,
                   const std::vector<std::uint8_t>& Monstey_Expected) {
  Monstey_ComparedBytes+=Monstey_Actual.size();
  Monstey_Check(Monstey_Actual==Monstey_Expected,"complete backing image");
}
void Monstey_Set(std::vector<std::uint8_t>& Monstey_Bytes,
    std::uint64_t Monstey_Base,std::uint64_t Monstey_At,
    std::uint64_t Monstey_Value,std::size_t Monstey_Width) {
  const auto Monstey_Offset=static_cast<std::size_t>(Monstey_At-Monstey_Base);
  for(std::size_t Monstey_I=0;Monstey_I<Monstey_Width;++Monstey_I)
    Monstey_Bytes[Monstey_Offset+Monstey_I]=static_cast<std::uint8_t>(Monstey_Value>>(8*Monstey_I));
}
std::uint64_t Monstey_Get(const std::vector<std::uint8_t>& Monstey_Bytes,
    std::uint64_t Monstey_Base,std::uint64_t Monstey_At,std::size_t Monstey_Width) {
  const auto Monstey_Offset=static_cast<std::size_t>(Monstey_At-Monstey_Base);
  std::uint64_t Monstey_Value=0;
  for(std::size_t Monstey_I=0;Monstey_I<Monstey_Width;++Monstey_I)
    Monstey_Value|=std::uint64_t(Monstey_Bytes[Monstey_Offset+Monstey_I])<<(8*Monstey_I);
  return Monstey_Value;
}
struct Monstey_Fixture {
  std::uint64_t Monstey_Base;
  std::vector<std::uint8_t> Monstey_Bytes;
  std::size_t Monstey_Visible;
  explicit Monstey_Fixture(std::uint64_t Monstey_BaseValue=0,std::size_t Monstey_Size=1024)
      :Monstey_Base(Monstey_BaseValue),Monstey_Bytes(Monstey_Size,0xA5),Monstey_Visible(Monstey_Size) {}
  std::uint64_t Monstey_At(std::uint64_t Monstey_Offset)const{return Monstey_Base+Monstey_Offset;}
  Monstey_GadbBinderMemory Monstey_Memory(){return {Monstey_Base,std::span<std::uint8_t>(Monstey_Bytes).first(Monstey_Visible)};}
  void Monstey_Put(std::uint64_t Monstey_Address,std::uint64_t Monstey_Value,std::size_t Monstey_Width=8){
    Monstey_Set(Monstey_Bytes,Monstey_Base,Monstey_Address,Monstey_Value,Monstey_Width);
  }
  Monstey_GadbManagerPrimitiveBindings Monstey_Bindings()const{
    Monstey_GadbManagerPrimitiveBindings Monstey_B;
    Monstey_B.gs58_cell_identity=Monstey_At(0x20);
    Monstey_B.enter_cell=Monstey_At(0x80);Monstey_B.leave_cell=Monstey_At(0x88);
    return Monstey_B;
  }
  void Monstey_Tls(std::uint32_t Monstey_Count){
    Monstey_Put(Monstey_At(0x20),Monstey_At(0x40));
    Monstey_Put(Monstey_At(0x40),Monstey_At(0x180));
    Monstey_Put(Monstey_At(0x1AC),Monstey_Count,4);
  }
};
template<class Monstey_F>void Monstey_Run(const std::string& Monstey_Name,Monstey_F Monstey_Function){
  ++Monstey_Cases;
  try{Monstey_Function();}
  catch(const std::exception& Monstey_Error){Monstey_Failures.push_back(Monstey_Name+": "+Monstey_Error.what());}
  catch(...){Monstey_Failures.push_back(Monstey_Name+": nonstandard exception");}
}
template<class Monstey_F>void Monstey_MemoryFault(Monstey_F Monstey_Function,
    std::uint64_t Monstey_Address,std::size_t Monstey_Width,bool Monstey_Writing){
  try{Monstey_Function();}
  catch(const Monstey_GadbBufferedMemoryFault& Monstey_Error){
    Monstey_Check(Monstey_Error.address==Monstey_Address,"first fault address");
    Monstey_Check(Monstey_Error.width==Monstey_Width,"first fault width");
    Monstey_Check(Monstey_Error.writing==Monstey_Writing,"first fault read/write");return;
  }
  throw std::runtime_error("expected ordinary memory fault");
}
template<class Monstey_F>void Monstey_Boundary(Monstey_F Monstey_Function,
    Monstey_GadbManagerPrimitiveBoundaryKind Monstey_Kind,std::uint64_t Monstey_Site,
    std::uint64_t Monstey_Address,std::uint64_t Monstey_Slot,
    std::uint64_t Monstey_Target,std::uint64_t Monstey_Argument){
  try{Monstey_Function();}
  catch(const Monstey_GadbManagerPrimitiveBoundaryFault& Monstey_Error){
    Monstey_Check(Monstey_Error.kind==Monstey_Kind,"boundary kind");
    Monstey_Check(Monstey_Error.native_site==Monstey_Site,"boundary native site");
    Monstey_Check(Monstey_Error.address==Monstey_Address,"boundary address");
    Monstey_Check(Monstey_Error.width==8,"boundary width");
    Monstey_Check(Monstey_Error.slot_identity==Monstey_Slot,"boundary slot");
    Monstey_Check(Monstey_Error.target_identity==Monstey_Target,"boundary live target");
    Monstey_Check(Monstey_Error.argument==Monstey_Argument,"boundary argument");return;
  }
  throw std::runtime_error("expected typed boundary fault");
}
void Monstey_Dto(const Monstey_GadbManagerSyncCall& Monstey_Call,
    Monstey_GadbManagerSyncOperation Monstey_Operation,std::uint64_t Monstey_Site,
    std::uint64_t Monstey_Slot,std::uint64_t Monstey_Target,
    std::uint64_t Monstey_Lock,Monstey_GadbBufferedResult Monstey_Incoming){
  ++Monstey_Events;
  Monstey_Check(Monstey_Call.operation==Monstey_Operation,"DTO operation");
  Monstey_Check(Monstey_Call.native_site==Monstey_Site,"DTO native site");
  Monstey_Check(Monstey_Call.slot_identity==Monstey_Slot,"DTO slot");
  Monstey_Check(Monstey_Call.target_identity==Monstey_Target,"DTO live target");
  Monstey_Check(Monstey_Call.critical_section==Monstey_Lock,"DTO lock");
  Monstey_Raw(Monstey_Call.incoming,Monstey_Incoming);
}
struct Monstey_ProviderFailure final:std::runtime_error{
  int Monstey_Tag;
  explicit Monstey_ProviderFailure(int Monstey_Value):std::runtime_error("declared partial provider exception"),Monstey_Tag(Monstey_Value){}
};
template<class Monstey_F>void Monstey_ProviderFault(Monstey_F Monstey_Function,int Monstey_Tag){
  try{Monstey_Function();}
  catch(const Monstey_ProviderFailure& Monstey_Error){Monstey_Check(Monstey_Error.Monstey_Tag==Monstey_Tag,"provider exception identity");return;}
  throw std::runtime_error("expected provider exception");
}
void Monstey_GuardCases(){
 ++Monstey_Groups;
 for(const std::uint64_t Monstey_Target:{0ULL,0x1122334455667788ULL,0xFFFFFFFFFFFFFFFFULL})
 Monstey_Run("ctor/raw/target/"+std::to_string(Monstey_Target),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;
  Monstey_F.Monstey_Put(Monstey_B.enter_cell,Monstey_Target);auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_Set(Monstey_Expected,0,0x100,0x2233445566778899ULL,8);std::size_t Monstey_Calls=0;
  Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){
   ++Monstey_Calls;Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::enter,0x14114BA9FULL,Monstey_B.enter_cell,Monstey_Target,0x2233445566778899ULL,{0xDEADBEEF12345678ULL,0x2233445566778899ULL});
   Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);return Monstey_GadbBufferedResult{0xBADF00D,0x9876543210012345ULL};};
  Monstey_Raw(Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x2233445566778899ULL,{0xDEADBEEF12345678ULL,0xCC},Monstey_B,Monstey_S),{0x100,0x9876543210012345ULL});
  Monstey_Check(Monstey_Calls==1,"one Enter primitive");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const int Monstey_Delta:{-7,-3,0,3,7})
 Monstey_Run("ctor/live-cell-alias/"+std::to_string(Monstey_Delta),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;
  Monstey_F.Monstey_Put(Monstey_B.enter_cell,0x8877665544332211ULL);const auto Monstey_G=Monstey_B.enter_cell+static_cast<std::uint64_t>(Monstey_Delta);
  auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,Monstey_G,0xFFEEDDCCBBAA9900ULL,8);
  const auto Monstey_Live=Monstey_Get(Monstey_Expected,0,Monstey_B.enter_cell,8);
  Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){
   Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::enter,0x14114BA9FULL,Monstey_B.enter_cell,Monstey_Live,0xFFEEDDCCBBAA9900ULL,{0x123,0xFFEEDDCCBBAA9900ULL});return Monstey_GadbBufferedResult{1,2};};
  Monstey_Raw(Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),Monstey_G,0xFFEEDDCCBBAA9900ULL,{0x123,0},Monstey_B,Monstey_S),{Monstey_G,2});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("dtor/live-changed-guard-and-leave-cell",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;std::vector<int> Monstey_Order;
  Monstey_F.Monstey_Put(Monstey_B.enter_cell,0x11);Monstey_F.Monstey_Put(Monstey_B.leave_cell,0x22);auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_Set(Monstey_Expected,0,0x100,0xAABBCCDDULL,8);Monstey_Set(Monstey_Expected,0,Monstey_B.leave_cell,0,8);
  Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){
   if(Monstey_Call.operation==Monstey_GadbManagerSyncOperation::enter){Monstey_Order.push_back(1);Monstey_F.Monstey_Put(0x100,0xAABBCCDDULL);Monstey_F.Monstey_Put(Monstey_B.leave_cell,0);return Monstey_GadbBufferedResult{0x31,0x32};}
   Monstey_Order.push_back(2);Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::leave,0x14114BA88ULL,Monstey_B.leave_cell,0,0xAABBCCDDULL,{0x71,0x72});return Monstey_GadbBufferedResult{0xF000000000000001ULL,0xE000000000000002ULL};};
  Monstey_Raw(Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x200,{0x55,0x66},Monstey_B,Monstey_S),{0x100,0x32});
  Monstey_Raw(Monstey_GadbDestroyManagerGuard(Monstey_F.Monstey_Memory(),0x100,{0x71,0x72},Monstey_B,Monstey_S),{0xF000000000000001ULL,0xE000000000000002ULL});
  Monstey_Check(Monstey_Order==std::vector<int>({1,2}),"ordered Enter/Leave");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("leave/live-target-across-calls-and-raw-VOID",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;
  for(const std::uint64_t Monstey_Target:{0x11ULL,0x22ULL,0ULL}){
   Monstey_F.Monstey_Put(Monstey_B.leave_cell,Monstey_Target);const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
   Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){
    Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::leave,0x14114BA88ULL,Monstey_B.leave_cell,Monstey_Target,0,{0x1111222233334444ULL,0xAAAABBBBCCCCDDDDULL});return Monstey_GadbBufferedResult{0xEEEEFFFFFFFF0001ULL,0x9999888877776666ULL};};
   Monstey_Raw(Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),0,{0x1111222233334444ULL,0xAAAABBBBCCCCDDDDULL},Monstey_B,Monstey_S),{0xEEEEFFFFFFFF0001ULL,0x9999888877776666ULL});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
  }
 });
 Monstey_Run("dtor/guard-equals-leave-cell/no-clear",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.leave_cell,0x12345678);const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::leave,0x14114BA88ULL,Monstey_B.leave_cell,0x12345678,0x12345678,{0x10,0x20});return Monstey_GadbBufferedResult{0x30,0x40};};
  Monstey_Raw(Monstey_GadbDestroyManagerGuard(Monstey_F.Monstey_Memory(),Monstey_B.leave_cell,{0x10,0x20},Monstey_B,Monstey_S),{0x30,0x40});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
}
void Monstey_FaultCases(){
 ++Monstey_Groups;
 Monstey_Run("ctor/first-guard-write-fault-before-missing",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_F.Monstey_Visible=0x47;
  Monstey_MemoryFault([&]{(void)Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x40,0x123,{1,2},Monstey_B,Monstey_S);},0x40,8,true);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(std::size_t Monstey_Cut=0;Monstey_Cut<8;++Monstey_Cut)
 Monstey_Run("ctor/cell-read-fault-after-store/"+std::to_string(Monstey_Cut),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0x20,0xABC,8);Monstey_F.Monstey_Visible=0x80+Monstey_Cut;
  Monstey_MemoryFault([&]{(void)Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x20,0xABC,{1,2},Monstey_B,Monstey_S);},0x80,8,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(std::size_t Monstey_Cut=0;Monstey_Cut<8;++Monstey_Cut)
 Monstey_Run("leave/read-before-missing/"+std::to_string(Monstey_Cut),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_F.Monstey_Visible=0x88+Monstey_Cut;
  Monstey_MemoryFault([&]{(void)Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),0x99,{1,2},Monstey_B,Monstey_S);},0x88,8,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("dtor/live-guard-read-first",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_F.Monstey_Visible=0x107;
  Monstey_MemoryFault([&]{(void)Monstey_GadbDestroyManagerGuard(Monstey_F.Monstey_Memory(),0x100,{1,2},Monstey_B,Monstey_S);},0x100,8,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const bool Monstey_Ctor:{false,true})
 Monstey_Run(Monstey_Ctor?"ctor/missing-sync-exact":"leave/missing-sync-exact",[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;const auto Monstey_Cell=Monstey_Ctor?Monstey_B.enter_cell:Monstey_B.leave_cell;
  Monstey_F.Monstey_Put(Monstey_Cell,0);auto Monstey_Expected=Monstey_F.Monstey_Bytes;if(Monstey_Ctor)Monstey_Set(Monstey_Expected,0,0x100,0x123,8);
  Monstey_Boundary([&]{if(Monstey_Ctor)(void)Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x123,{1,2},Monstey_B,Monstey_S);else (void)Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),0x123,{1,2},Monstey_B,Monstey_S);},Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,Monstey_Ctor?0x14114BA9FULL:0x14114BA88ULL,Monstey_Cell,Monstey_Cell,0,0x123);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const int Monstey_Operation:{0,1,2})
 Monstey_Run("sync/partial-provider-exception/"+std::to_string(Monstey_Operation),[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.enter_cell,1);Monstey_F.Monstey_Put(Monstey_B.leave_cell,2);Monstey_F.Monstey_Put(0x100,0x200);
  auto Monstey_Expected=Monstey_F.Monstey_Bytes;if(Monstey_Operation==0)Monstey_Set(Monstey_Expected,0,0x100,0x333,8);Monstey_Expected[0x10]=0xEB;
  Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall&)->Monstey_GadbBufferedResult{Monstey_F.Monstey_Bytes[0x10]=0xEB;throw Monstey_ProviderFailure(71);};
  Monstey_ProviderFault([&]{if(Monstey_Operation==0)(void)Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x333,{1,2},Monstey_B,Monstey_S);else if(Monstey_Operation==1)(void)Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),0x200,{1,2},Monstey_B,Monstey_S);else (void)Monstey_GadbDestroyManagerGuard(Monstey_F.Monstey_Memory(),0x100,{1,2},Monstey_B,Monstey_S);},71);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
}
void Monstey_TlsCases(){
 ++Monstey_Groups;
 for(const std::uint64_t Monstey_Base:{0ULL,0xFEDC000000000000ULL})
 for(const std::uint32_t Monstey_Count:{0u,1u,0x7FFFFFFFu,0xFFFFFFFEu,0xFFFFFFFFu})
 Monstey_Run("tls/inc/count-base/"+std::to_string(Monstey_Count)+"/"+std::to_string(Monstey_Base),[=]{
  Monstey_Fixture Monstey_F(Monstey_Base);auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_F.Monstey_Tls(Monstey_Count);auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_Set(Monstey_Expected,Monstey_Base,Monstey_F.Monstey_At(0x1AC),std::uint32_t(Monstey_Count+1u),4);
  Monstey_Raw(Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{Monstey_F.Monstey_At(0x40),0x28});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const std::uint64_t Monstey_Base:{0ULL,0xFEDC000000000000ULL})
 for(const std::uint32_t Monstey_Count:{1u,2u,0x80000000u,0xFFFFFFFFu})
 Monstey_Run("tls/dec/zext-count-base/"+std::to_string(Monstey_Count)+"/"+std::to_string(Monstey_Base),[=]{
  Monstey_Fixture Monstey_F(Monstey_Base);auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_F.Monstey_Tls(Monstey_Count);auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_Set(Monstey_Expected,Monstey_Base,Monstey_F.Monstey_At(0x1AC),Monstey_Count-1u,4);
  Monstey_Raw(Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{Monstey_Count-1u,0x28});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const bool Monstey_Inc:{false,true}){
  Monstey_Run(Monstey_Inc?"tls/inc/missing-GS":"tls/dec/missing-GS",[=]{
   Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity.reset();Monstey_F.Monstey_Visible=0;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
   Monstey_Boundary([&]{if(Monstey_Inc)(void)Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);else (void)Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);},Monstey_GadbManagerPrimitiveBoundaryKind::missing_gs58_binding,Monstey_Inc?0x1406AAFDCULL:0x1406AACFCULL,0x58,0,0,0);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
  });
  for(const int Monstey_Stage:{0,1,2})
  for(std::size_t Monstey_Cut=0;Monstey_Cut<(Monstey_Stage==2?4u:8u);++Monstey_Cut)
  Monstey_Run("tls/lazy-fault/"+std::to_string(Monstey_Inc)+"/"+std::to_string(Monstey_Stage)+"/"+std::to_string(Monstey_Cut),[=]{
   Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity=0;
   Monstey_F.Monstey_Put(0,0x40);Monstey_F.Monstey_Put(0x40,0x180);Monstey_F.Monstey_Put(0x1AC,0,4);
   const std::uint64_t Monstey_Address=Monstey_Stage==0?0:Monstey_Stage==1?0x40:0x1AC;
   Monstey_F.Monstey_Visible=static_cast<std::size_t>(Monstey_Address)+Monstey_Cut;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
   Monstey_MemoryFault([&]{if(Monstey_Inc)(void)Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);else (void)Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);},Monstey_Address,Monstey_Stage==2?4u:8u,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
  });
 }
 Monstey_Run("tls/present-zero-GS-binding",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity=0;Monstey_F.Monstey_Tls(3);Monstey_F.Monstey_Put(0,0x40);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0x1AC,4,4);
  Monstey_Raw(Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{0x40,0x28});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("tls/numeric-zero-array-is-owned",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity=8;Monstey_F.Monstey_Put(8,0);Monstey_F.Monstey_Put(0,0x180);Monstey_F.Monstey_Put(0x1AC,1,4);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0x1AC,2,4);
  Monstey_Raw(Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{0,0x28});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const bool Monstey_Inc:{false,true})
 Monstey_Run(Monstey_Inc?"tls/inc/wrapped-field-address":"tls/dec/wrapped-field-address",[=]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity=8;Monstey_F.Monstey_Put(8,0x40);Monstey_F.Monstey_Put(0x40,std::numeric_limits<std::uint64_t>::max()-0x2B);Monstey_F.Monstey_Put(0,5,4);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0,Monstey_Inc?6u:4u,4);
  Monstey_Raw(Monstey_Inc?Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B):Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{Monstey_Inc?0x40ULL:4ULL,0x28});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("tls/single-operand-U64-crossing",[]{
  const auto Monstey_A=std::numeric_limits<std::uint64_t>::max()-3;Monstey_Fixture Monstey_F(Monstey_A,16);auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_B.gs58_cell_identity=Monstey_A;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_MemoryFault([&]{(void)Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);},Monstey_A,8,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
}
void Monstey_ColdCases(){
 ++Monstey_Groups;
 for(const std::size_t Monstey_Visible:{4u,5u,16u})
 Monstey_Run("cold/mapped-zero/raw-and-four-bytes/"+std::to_string(Monstey_Visible),[=]{
  Monstey_Fixture Monstey_F;Monstey_F.Monstey_Visible=Monstey_Visible;auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0,0,4);
  Monstey_Raw(Monstey_GadbManagerNullAndReturn(Monstey_F.Monstey_Memory(),{0xFFEEDDCCBBAA9988ULL,0x123456789ABCDEF0ULL}),{0xFFEEDDCCBBAA9988ULL,0x123456789ABCDEF0ULL});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(std::size_t Monstey_Visible=0;Monstey_Visible<4;++Monstey_Visible)
 Monstey_Run("cold/zero-short-first-read/"+std::to_string(Monstey_Visible),[=]{
  Monstey_Fixture Monstey_F;Monstey_F.Monstey_Visible=Monstey_Visible;const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_MemoryFault([&]{(void)Monstey_GadbManagerNullAndReturn(Monstey_F.Monstey_Memory(),{1,2});},0,4,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("cold/unmapped-zero-not-boundary",[]{
  Monstey_Fixture Monstey_F(1);const auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_MemoryFault([&]{(void)Monstey_GadbManagerNullAndReturn(Monstey_F.Monstey_Memory(),{1,2});},0,4,false);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 for(const std::uint64_t Monstey_Base:{0ULL,0x100000000ULL})
 for(const std::uint32_t Monstey_Count:{0u,1u,0xFFFFFFFEu,0xFFFFFFFFu})
 Monstey_Run("tls/pair-or-cold/"+std::to_string(Monstey_Base)+"/"+std::to_string(Monstey_Count),[=]{
  Monstey_Fixture Monstey_F(Monstey_Base);auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_F.Monstey_Tls(Monstey_Count);auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  if(Monstey_Count==0){
   if(Monstey_Base==0){Monstey_Set(Monstey_Expected,0,0,0,4);Monstey_Raw(Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{0,0x28});}
   else Monstey_MemoryFault([&]{(void)Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);},0,4,false);
  }else{
   Monstey_Raw(Monstey_GadbIncrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{Monstey_F.Monstey_At(0x40),0x28});
   if(Monstey_Count==0xFFFFFFFFu){Monstey_Set(Monstey_Expected,Monstey_Base,Monstey_F.Monstey_At(0x1AC),0,4);if(Monstey_Base==0){Monstey_Set(Monstey_Expected,0,0,0,4);Monstey_Raw(Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{0,0x28});}else Monstey_MemoryFault([&]{(void)Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B);},0,4,false);}
   else Monstey_Raw(Monstey_GadbDecrementManagerTls(Monstey_F.Monstey_Memory(),Monstey_B),{Monstey_Count,0x28});
  }
  Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
}
struct Monstey_Token {
 std::vector<int>* Monstey_Order;std::vector<std::uint8_t>* Monstey_ImageBytes;
 Monstey_Token(std::vector<int>& Monstey_O,std::vector<std::uint8_t>& Monstey_B):Monstey_Order(&Monstey_O),Monstey_ImageBytes(&Monstey_B){}
 ~Monstey_Token(){Monstey_Order->push_back(9);(*Monstey_ImageBytes)[0x10]=0xD9;}
};
void Monstey_LifetimeCases(){
 ++Monstey_Groups;
 Monstey_Run("sync/mutable-current-target-state",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.leave_cell,1);const auto Monstey_Expected=Monstey_F.Monstey_Bytes;
  Monstey_S.sync=[Monstey_N=std::uint64_t{0}](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall&)mutable{return Monstey_GadbBufferedResult{++Monstey_N,0x55};};
  Monstey_Raw(Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),2,{3,4},Monstey_B,Monstey_S),{1,0x55});Monstey_Raw(Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),2,{3,4},Monstey_B,Monstey_S),{2,0x55});Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("sync/self-clear/current-active-retirement",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.leave_cell,1);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Expected[0x10]=0xD9;
  auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.sync=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall&){Monstey_Order.push_back(1);Monstey_S.sync={};Monstey_Check(!Monstey_Weak.expired(),"active target survives self-clear");Monstey_Order.push_back(2);return Monstey_GadbBufferedResult{0x31,0x32};};Monstey_T.reset();
  Monstey_Raw(Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),2,{3,4},Monstey_B,Monstey_S),{0x31,0x32});Monstey_Check(Monstey_Weak.expired(),"retirement after return");Monstey_Check(Monstey_Order==std::vector<int>({1,2,9}),"observable lifetime order");Monstey_Check(!Monstey_S.sync,"current target cleared");
  Monstey_Boundary([&]{(void)Monstey_GadbLeaveManagerCriticalSection(Monstey_F.Monstey_Memory(),2,{3,4},Monstey_B,Monstey_S);},Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,0x14114BA88ULL,Monstey_B.leave_cell,Monstey_B.leave_cell,1,2);Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("sync/self-replace/real-dtor-reentry",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.enter_cell,1);Monstey_F.Monstey_Put(Monstey_B.leave_cell,2);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0x100,0x444,8);Monstey_Expected[0x10]=0xD9;
  auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.sync=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall&){
   Monstey_Order.push_back(1);Monstey_F.Monstey_Put(0x100,0x444);
   Monstey_S.sync=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall& Monstey_Call){Monstey_Order.push_back(2);Monstey_Dto(Monstey_Call,Monstey_GadbManagerSyncOperation::leave,0x14114BA88ULL,Monstey_B.leave_cell,2,0x444,{0x51,0x52});return Monstey_GadbBufferedResult{0x61,0x62};};
   Monstey_Check(!Monstey_Weak.expired(),"active target survives replacement");Monstey_Raw(Monstey_GadbDestroyManagerGuard(Monstey_F.Monstey_Memory(),0x100,{0x51,0x52},Monstey_B,Monstey_S),{0x61,0x62});Monstey_Order.push_back(3);return Monstey_GadbBufferedResult{0x71,0x72};};Monstey_T.reset();
  Monstey_Raw(Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x333,{0x41,0x42},Monstey_B,Monstey_S),{0x100,0x72});Monstey_Check(Monstey_Weak.expired(),"old target retired");Monstey_Check(Monstey_Order==std::vector<int>({1,2,3,9}),"reentry/retirement order");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
 Monstey_Run("sync/self-clear/partial-exception-and-retirement",[]{
  Monstey_Fixture Monstey_F;auto Monstey_B=Monstey_F.Monstey_Bindings();std::vector<int> Monstey_Order;Monstey_Order.reserve(16);Monstey_GadbManagerPrimitiveServices Monstey_S;Monstey_F.Monstey_Put(Monstey_B.enter_cell,1);auto Monstey_Expected=Monstey_F.Monstey_Bytes;Monstey_Set(Monstey_Expected,0,0x100,0x333,8);Monstey_Expected[0x10]=0xD9;Monstey_Expected[0x11]=0xEB;
  auto Monstey_T=std::make_shared<Monstey_Token>(Monstey_Order,Monstey_F.Monstey_Bytes);std::weak_ptr<Monstey_Token> Monstey_Weak=Monstey_T;
  Monstey_S.sync=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerSyncCall&)->Monstey_GadbBufferedResult{Monstey_Order.push_back(1);Monstey_S.sync={};Monstey_Check(!Monstey_Weak.expired(),"active target during exception prefix");Monstey_F.Monstey_Bytes[0x11]=0xEB;throw Monstey_ProviderFailure(121);};Monstey_T.reset();
  Monstey_ProviderFault([&]{(void)Monstey_GadbConstructManagerGuard(Monstey_F.Monstey_Memory(),0x100,0x333,{1,2},Monstey_B,Monstey_S);},121);Monstey_Check(Monstey_Weak.expired(),"target retired on exception");Monstey_Check(Monstey_Order==std::vector<int>({1,9}),"exception lifetime order");Monstey_Image(Monstey_F.Monstey_Bytes,Monstey_Expected);
 });
}
std::string Monstey_Quote(const std::string& Monstey_Value){
 std::string Monstey_Result="\"";for(const char Monstey_C:Monstey_Value){if(Monstey_C=='"'||Monstey_C=='\\')Monstey_Result+='\\';if(Monstey_C=='\n')Monstey_Result+="\\n";else Monstey_Result+=Monstey_C;}return Monstey_Result+'"';
}
}
int main(){
 Monstey_GuardCases();Monstey_FaultCases();Monstey_TlsCases();Monstey_ColdCases();Monstey_LifetimeCases();
 std::cout<<"{\"status\":\""<<(Monstey_Failures.empty()?"PASS":"FAIL")<<"\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"compared_backing_bytes\":"<<Monstey_ComparedBytes<<",\"checked_sync_DTOs\":"<<Monstey_Events<<",\"failures\":[";
 for(std::size_t Monstey_I=0;Monstey_I<Monstey_Failures.size();++Monstey_I){if(Monstey_I)std::cout<<',';std::cout<<Monstey_Quote(Monstey_Failures[Monstey_I]);}
 std::cout<<"],\"actual_compiled_product\":true,\"original_OS_or_image_called\":false}\n";
 return Monstey_Failures.empty()?0:1;
}
