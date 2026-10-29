#include "Monstey_FearUnpackedCleanup.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U32=std::uint32_t;using U8=std::uint8_t;
U32 Monstey_Cases=0,Monstey_Checks=0;
void Need(bool x,const char* reason){++Monstey_Checks;if(!x)throw std::runtime_error(reason);}
struct Fixture {
 static constexpr U32 Base=0x1000,Frame=0x3000,Map=0x4000,Head=0x5000,First=0x6000,Replacement=0x7000;
 std::vector<U8> bytes=std::vector<U8>(0x9000,0xCD),prior;
 Monstey_FearDbMutableMemory memory{Base,bytes};
 Monstey_FearUnpackedCleanupServices services;
 std::vector<Monstey_FearUnpackedEraseCall> erases;
 std::vector<Monstey_FearUnpackedDeleteCall> deletes;
 Monstey_FearDbCallable<void(Fixture&)> on_erase,on_delete;
 U32 map=Map,frame=Frame,expected_head=Head;bool parsed=false;
 void Put(U32 at,U32 value){for(U32 i=0;i<4;++i)bytes.at(at-Base+i)=static_cast<U8>(value>>(i*8u));}
 U32 Get(U32 at)const{U32 v=0;for(U32 i=0;i<4;++i)v|=static_cast<U32>(bytes.at(at-Base+i))<<(i*8u);return v;}
 Fixture(){Put(map+4,Head);Put(map+8,0x1234);Put(Head,First);
  services.erase_range=[this](auto& active,auto c){Need(&active==&memory,"same mutable memory identity");Need(c.parsed==parsed&&c.native_site==(parsed?0x100061E2u:0x100061B2u),"selected native erase callsite");Need(c.map==map&&c.hidden_result==frame+4&&c.first==First&&c.last==Head,"actual outgoing argument cells");erases.push_back(c);if(on_erase)on_erase(*this);return 0xDEADBEEFu;};
  services.delete_scalar=[this](auto& active,auto c){Need(&active==&memory,"delete uses rebound view reference");Need(c.native_site==(parsed?0x100061EBu:0x100061BBu)&&c.address==expected_head,"fresh head selected after erase");Need(Get(map+4)==expected_head&&Get(map+8)!=0,"zeros must follow delete");deletes.push_back(c);if(on_delete)on_delete(*this);};
 }
 void Run(){Monstey_FearUnpackedCleanup(memory,map,frame,parsed,services);}
 void Finished(){Need(erases.size()==1&&deletes.size()==1,"one erase and one delete");Need(Get(map+4)==0&&Get(map+8)==0,"head then count zero");}
};
void Basics(){
 for(bool parsed:{false,true}){Fixture f;f.parsed=parsed;f.Run();f.Finished();Need(f.Get(f.frame-12)==f.frame+4&&f.Get(f.frame-8)==f.First&&f.Get(f.frame-4)==f.Head,"known outgoing stack stores retained");++Monstey_Cases;}
 for(int offset=-16;offset<=16;offset+=4){Fixture f;f.map=static_cast<U32>(static_cast<int>(f.frame)+offset);f.Put(f.map+4,f.Head);f.Put(f.map+8,0x1234);f.services.delete_scalar=[&f](auto&,auto c){Need(c.address==f.Get(f.map+4),"alias delete consumes actual late head");f.deletes.push_back(c);};f.Run();f.Finished();++Monstey_Cases;}
 for(int offset=-16;offset<=16;offset+=4){Fixture f;f.on_erase=[offset](auto& x){x.map=Fixture::Map;x.Put(x.map+4,Fixture::Replacement);x.expected_head=Fixture::Replacement;x.Put(static_cast<U32>(static_cast<int>(x.frame)+offset),0xAABBCCDD);x.Put(x.map+8,0x1234);};f.Run();f.Finished();++Monstey_Cases;}
 {Fixture f;f.on_erase=[](auto& x){x.Put(x.map+4,x.Replacement);x.expected_head=x.Replacement;};f.on_delete=[](auto& x){x.Put(x.map+4,0xABCDEF00);x.Put(x.map+8,0xABCDEF01);};f.Run();f.Finished();++Monstey_Cases;}
 {Fixture f;f.on_erase=[](auto& x){x.prior=std::move(x.bytes);x.bytes=x.prior;x.memory.bytes=x.bytes;x.Put(x.map+4,x.Replacement);x.expected_head=x.Replacement;};f.on_delete=[](auto& x){x.prior=std::move(x.bytes);x.bytes=x.prior;x.memory.bytes=x.bytes;};f.Run();f.Finished();Need(f.Get(f.map+4)==0&&f.prior[f.map+5-f.Base]==0x70,"subsequent stores use current view");++Monstey_Cases;}
}
void Failures(){
 {Fixture f;f.Put(f.map+4,0xFFFF0000);bool fault=false;try{f.Run();}catch(const Monstey_FearDbFault& e){fault=e.address==0xFFFF0000&&e.width==4&&!e.writing;}Need(fault&&f.erases.empty()&&f.Get(f.frame-4)==0xCDCDCDCD,"head.first consumed before any push");++Monstey_Cases;}
 for(U32 available:{0u,4u,8u}){Fixture f;f.frame=f.Base+available;bool fault=false;try{f.Run();}catch(const Monstey_FearDbFault& e){fault=e.writing&&e.width==4;}Need(fault&&f.erases.empty(),"outgoing argument consumed fault");if(available>=4)Need(f.Get(f.frame-4)==f.Head,"preceding push retained");if(available>=8)Need(f.Get(f.frame-8)==f.First,"second push retained");++Monstey_Cases;}
 {Fixture f;f.services.erase_range={};bool fault=false;try{f.Run();}catch(const Monstey_FearUnpackedCleanupBoundaryFault&){fault=true;}Need(fault&&f.Get(f.frame-12)==f.frame+4&&f.deletes.empty()&&f.Get(f.map+4)==f.Head,"missing erase after argument prefix");++Monstey_Cases;}
 {Fixture f;f.services.delete_scalar={};bool fault=false;try{f.Run();}catch(const Monstey_FearUnpackedCleanupBoundaryFault&){fault=true;}Need(fault&&f.erases.size()==1&&f.Get(f.frame-4)==f.Head&&f.Get(f.map+8)==0x1234,"missing delete after head reload and push");++Monstey_Cases;}
 {Fixture f;f.on_erase=[](auto&){throw std::runtime_error("erase-failure");};bool fault=false;try{f.Run();}catch(const std::runtime_error& e){fault=std::string(e.what())=="erase-failure";}Need(fault&&f.deletes.empty()&&f.Get(f.map+4)==f.Head,"erase exception retains prefix");++Monstey_Cases;}
 {Fixture f;f.on_delete=[](auto&){throw std::runtime_error("delete-failure");};bool fault=false;try{f.Run();}catch(const std::runtime_error& e){fault=std::string(e.what())=="delete-failure";}Need(fault&&f.deletes.size()==1&&f.Get(f.map+8)==0x1234,"delete exception precedes zeros");++Monstey_Cases;}
 {Fixture f;f.map=0x9000;f.Put(f.map+4,f.Head);f.Put(f.map+8,0x1234);f.memory.bytes=std::span<U8>(f.bytes).first(f.map+8-f.Base);f.services.delete_scalar=[&f](auto&,auto c){Need(c.address==f.Head,"selected before trailing write fault");f.deletes.push_back(c);};bool fault=false;try{f.Run();}catch(const Monstey_FearDbFault& e){fault=e.address==f.map+8&&e.writing&&e.width==4;}Need(fault&&f.Get(f.map+4)==0&&f.deletes.size()==1,"first zero retained before second range fault");++Monstey_Cases;}
}
void Lifetimes(){
 {Fixture f;unsigned calls=0;f.services.erase_range=[n=0,&calls](auto&,auto)mutable{calls=++n;return 0u;};for(int i=0;i<3;++i){f.Put(f.map+4,f.Head);f.Put(f.map+8,9);f.Run();}Need(calls==3,"erase target mutable state persists");++Monstey_Cases;}
 for(int mode=0;mode<3;++mode){Fixture f;auto life=std::make_shared<int>(7);std::weak_ptr<int> weak=life;bool alive=false;
  f.services.erase_range=[life,&f,&alive,weak,mode](auto&,auto){auto observed=weak;auto output=&alive;auto* owner=&f;if(mode==0)owner->services.erase_range=[](auto&,auto){return 2u;};else owner->services.erase_range={};*output=!observed.expired();if(mode==2)throw std::runtime_error("self-throw");return 1u;};life.reset();bool threw=false;try{f.Run();}catch(const std::runtime_error& e){threw=std::string(e.what())=="self-throw";}Need(alive&&weak.expired(),"active erase target retained and then retired");Need(mode==2?threw:f.deletes.size()==1,"active original call finishes");++Monstey_Cases;
 }
 for(int mode=0;mode<3;++mode){Fixture f;auto life=std::make_shared<int>(9);std::weak_ptr<int> weak=life;bool alive=false;
  f.services.delete_scalar=[life,&f,&alive,weak,mode](auto&,auto){auto observed=weak;auto output=&alive;auto* owner=&f;if(mode==0)owner->services.delete_scalar=[](auto&,auto){};else owner->services.delete_scalar={};*output=!observed.expired();if(mode==2)throw std::runtime_error("delete-self-throw");};life.reset();bool threw=false;try{f.Run();}catch(const std::runtime_error& e){threw=std::string(e.what())=="delete-self-throw";}Need(alive&&weak.expired(),"active delete target retained and retired");Need(mode==2?threw:f.Get(f.map+4)==0,"active delete return precedes zero stores");++Monstey_Cases;
 }

}
}
int main(){try{Basics();Failures();Lifetimes();std::cout<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
