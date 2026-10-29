#include "Monstey_FearDbCallable.hpp"
#include <iostream>
#include <string>
#include <vector>
using Monstey::Monstey_FearDbCallable;
namespace {
unsigned Monstey_Checks=0,Monstey_Cases=0;
void Monstey_Check(bool value,const char* why){++Monstey_Checks;if(!value)throw std::runtime_error(why);}
struct Monstey_Life{};
struct Monstey_Throwing {Monstey_Throwing()=default;Monstey_Throwing(const Monstey_Throwing&){throw std::bad_alloc();}int operator()(){return 0;}};
}
int main(){try{
 Monstey_FearDbCallable<int()> count=[n=0]()mutable{return ++n;};
 Monstey_Check(count()==1&&count()==2,"mutable target state persists");++Monstey_Cases;
 auto copied=count;Monstey_Check(copied()==3&&count()==4,"copies share target identity/state");++Monstey_Cases;
 count=[](){return 99;};Monstey_Check(count()==99&&copied()==5,"replacement leaves prior snapshots intact");++Monstey_Cases;
 Monstey_FearDbCallable<void()> empty;Monstey_Check(!empty,"empty bool");bool failed=false;try{empty();}catch(const std::bad_function_call&){failed=true;}Monstey_Check(failed,"empty invocation");empty=[](){};empty();empty={};Monstey_Check(!empty,"brace reset");empty=nullptr;Monstey_Check(!empty,"nullptr reset");++Monstey_Cases;
 std::function<int()> normal=[n=40]()mutable{return ++n;};Monstey_FearDbCallable<int()> bound=normal;Monstey_Check(bound()==41&&bound()==42&&normal()==41,"binding copies initial stdFunction once only");++Monstey_Cases;
 for(int mode=0;mode<3;++mode){
  auto token=std::make_shared<Monstey_Life>();std::weak_ptr<Monstey_Life> weak=token;bool alive=false;
  Monstey_FearDbCallable<int()> self;
  self=[token,&self,&alive,weak,mode](){auto observed=weak;auto result=&alive;if(mode==0)self=[](){return 77;};else self={};*result=!observed.expired();if(mode==2)throw std::runtime_error("intentional");return 17;};token.reset();
  bool threw=false;int value=0;try{value=self();}catch(const std::runtime_error& e){threw=std::string(e.what())=="intentional";}
  Monstey_Check(alive&&weak.expired(),"active target retained and retired exactly after return/exception");Monstey_Check(mode==2?threw:value==17,"active call returns original value");if(mode==0)Monstey_Check(self()==77,"subsequent call uses replacement");else Monstey_Check(!self,"self clear remains visible");++Monstey_Cases;
 }
 Monstey_FearDbCallable<int()> nested;nested=[&nested,n=0]()mutable{if(++n==1)return 10+nested();return n;};Monstey_Check(nested()==12&&nested()==3,"reentrant target state shared");++Monstey_Cases;
 auto original=bound;Monstey_Throwing throwing;bool allocation=false;try{bound=throwing;}catch(const std::bad_alloc&){allocation=true;}Monstey_Check(allocation&&bound()==43&&original()==44,"failed target construction preserves prior shared target");++Monstey_Cases;
 std::cout<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
