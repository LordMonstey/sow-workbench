#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace Monstey;
namespace {
std::uint32_t Monstey_Cases=0,Monstey_Checks=0;
void Monstey_CheckCallable(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
struct Monstey_Signal {};
void Monstey_SharedState() {
 ++Monstey_Cases;
 Monstey_FearDbCallable<unsigned(unsigned)> first=[count=0u](unsigned step) mutable{return count+=step;};
 auto second=first;
 Monstey_CheckCallable(first(2)==2,"first state increment");Monstey_CheckCallable(second(3)==5,"copy cloned mutable state");
 Monstey_CheckCallable(first(4)==9,"state did not persist");
 second=[](unsigned x){return x+100;};
 Monstey_CheckCallable(second(7)==107&&first(1)==10,"replacement mutated other holder target");
}
void Monstey_SelfReplacement() {
 ++Monstey_Cases;Monstey_FearDbCallable<unsigned(unsigned)> target;
 auto lifetime=std::make_shared<unsigned>(17);std::weak_ptr<unsigned> weak=lifetime;bool alive=false;
 target=[lifetime,&target,&weak,&alive](unsigned x) {
  auto* to=&target;auto* w=&weak;auto* observation=&alive;
  *to=[](unsigned y){return y+100;};*observation=!w->expired();return x+1;
 };
 lifetime.reset();Monstey_CheckCallable(target(7)==8&&alive,"active target died during direct replacement");
 Monstey_CheckCallable(weak.expired(),"retired target leaked");Monstey_CheckCallable(target(7)==107,"replacement not published");
}
void Monstey_SelfClearException() {
 ++Monstey_Cases;Monstey_FearDbCallable<unsigned()> target;
 auto lifetime=std::make_shared<unsigned>(19);std::weak_ptr<unsigned> weak=lifetime;bool alive=false,caught=false;
 target=[lifetime,&target,&weak,&alive]() -> unsigned {
  auto* to=&target;auto* w=&weak;auto* observation=&alive;*to={};*observation=!w->expired();throw Monstey_Signal{};
 };
 lifetime.reset();try{(void)target();}catch(const Monstey_Signal&){caught=true;}
 Monstey_CheckCallable(caught&&alive,"exception/active lifetime changed");
 Monstey_CheckCallable(!target&&weak.expired(),"self-clear not retained after exception");
 bool empty_fault=false;try{(void)target();}catch(const std::bad_function_call&){empty_fault=true;}
 Monstey_CheckCallable(empty_fault,"empty callable did not fault");
}
void Monstey_ReentrantReplacement() {
 ++Monstey_Cases;Monstey_FearDbCallable<unsigned(unsigned)> target;std::vector<unsigned> events;
 auto lifetime=std::make_shared<unsigned>(23);std::weak_ptr<unsigned> weak=lifetime;bool alive=false;
 target=[lifetime,&target,&weak,&alive,&events](unsigned x) {
  auto* to=&target;auto* log=&events;auto* w=&weak;auto* observed=&alive;
  log->push_back(1);*to=[log](unsigned y){log->push_back(2);return y+100;};
  const auto value=(*to)(x);*observed=!w->expired();log->push_back(3);return value+1;
 };
 lifetime.reset();const auto value=target(7);
 Monstey_CheckCallable(value==108&&events==std::vector<unsigned>{1,2,3},"reentrant call did not select new target");
 Monstey_CheckCallable(alive&&weak.expired(),"reentrant old target lifetime mismatch");
}
void Monstey_EmptyAndExceptionState() {
 ++Monstey_Cases;
 Monstey_FearDbCallable<unsigned()> empty=std::function<unsigned()>{};
 Monstey_CheckCallable(!empty,"empty erased target reported present");
 bool fault=false;try{(void)empty();}catch(const std::bad_function_call&){fault=true;}
 Monstey_CheckCallable(fault,"empty erased target invocation did not fault");
 Monstey_FearDbCallable<unsigned(bool)> target=[count=0u](bool fail) mutable {++count;if(fail)throw Monstey_Signal{};return count;};
 Monstey_CheckCallable(target(false)==1,"initial state");
 bool caught=false;try{(void)target(true);}catch(const Monstey_Signal&){caught=true;}
 auto copy=target;Monstey_CheckCallable(caught&&copy(false)==3&&target(false)==4,"exception reset or cloned state");
}
}
int main(int argc,char** argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");
  Monstey_SharedState();Monstey_SelfReplacement();Monstey_SelfClearException();Monstey_ReentrantReplacement();Monstey_EmptyAndExceptionState();
  std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":5,\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<"}\n";
  std::cout<<"PASS callable "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}