#include "Monstey_LogicalMapsFixture.hpp"
namespace {
void Monstey_Sequences(bool parsed){
 const auto keys=Monstey_Keys();for(unsigned profile=0;profile<12;++profile){Monstey_Fixture f;f.Init(parsed);Monstey_Oracle o;std::uint32_t seed=0x98761u+profile;std::map<std::string,std::uint32_t,Monstey_Less>identities;
  for(unsigned step=0;step<240;++step){const auto j=profile<3?(profile==0?step%keys.size():profile==1?keys.size()-1-step%keys.size():(step*7)%keys.size()):Monstey_Random(seed)%keys.size();const auto&key=keys[j];Monstey_Value v{Monstey_Random(seed),Monstey_Random(seed),Monstey_Random(seed),step%3==0?std::string(31,static_cast<char>('A'+step%20)):step%3==1?std::string("v\0x",3):""};f.Input(key,v);
   if(parsed){f.W(f.hidden,0xBAD01234);const auto result=f.maps.InsertParsed(f.mem,f.map,f.hidden,step%2==0?0:0xFFFFFFFFu,f.pair);Monstey_Check(result==f.hidden,"hidden result identity");const auto node=f.R(f.hidden);if(o.contains(key)){Monstey_Check(node==identities.at(key),"duplicate node retained");}else{o.emplace(key,v);identities.emplace(key,node);}}else{f.maps.PutConstraint(f.mem,f.map,f.pair,v.type,v.w4,v.w8);o[key]=v;}f.Verify(parsed,o);++Monstey_cases;
  }
 }
}
void Monstey_MutableAllocator(){
 Monstey_Fixture f;f.Init(true);unsigned calls=0;f.maps.allocate=[&](Monstey_Memory,std::uint32_t n){++calls;Monstey_Check(n==80,"node allocator width");f.maps.allocate=[next=f.base+0x9000u](Monstey_Memory,std::uint32_t n2)mutable{const auto out=next;next+=n2+16;return out;};return f.base+0x8000;};Monstey_Oracle o;
 for(unsigned i=0;i<3;++i){const std::string key(1,static_cast<char>('a'+i));Monstey_Value v{i,0,i+77,"value"};f.Input(key,v);f.maps.InsertParsed(f.mem,f.map,f.hidden,0,f.pair);o.emplace(key,v);f.Verify(true,o);++Monstey_cases;}Monstey_Check(calls==1,"allocator replacement persisted");
 f.maps.allocate={};f.Input("b",{3,0,5,"replacement"});f.maps.InsertParsed(f.mem,f.map,f.hidden,0,f.pair);f.Verify(true,o);++Monstey_cases;
 const auto before=f.image;bool threw=false;try{f.Input("d",{1,0,9,"new"});f.maps.InsertParsed(f.mem,f.map,f.hidden,0,f.pair);}catch(const std::exception&){threw=true;}Monstey_Check(threw,"missing consumed allocator");f.Verify(true,o);(void)before;++Monstey_cases;
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("report argument required");Monstey_Sequences(false);Monstey_Sequences(true);Monstey_MutableAllocator();std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":3,\"cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<"}\n";std::cout<<"PASS "<<Monstey_cases<<" cases "<<Monstey_checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
