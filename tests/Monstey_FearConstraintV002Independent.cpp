#define main Monstey_PreparedIndependentMain
#include "Monstey_FearConstraintIndependent.cpp"
#undef main
#include <memory>
namespace {
using Monstey_Trace=std::array<std::vector<unsigned>,9>;
template<class Function>
void Monstey_SequenceProvider(Function& slot,std::vector<unsigned>& trace) {
 auto original=slot;
 slot=[original,ordinal=0u,&trace](auto...args) mutable -> decltype(auto) {
  trace.push_back(ordinal++);return original(args...);
 };
}
void Monstey_InitialHighEOF() {
 ++Monstey_Counts.groups;
 for(const auto high_length:std::array<std::uint32_t,3>{0,1,0xFFFFFFFFu})
  for(const auto high_position:std::array<std::uint32_t,3>{0,1,0xFFFFFFFFu}) {
   Monstey_Config cfg;cfg.empty=true;cfg.length_high=high_length;cfg.position_high=high_position;
   Monstey_Compare(cfg,{});
  }
}
void Monstey_StatefulProviders() {
 ++Monstey_Counts.groups;++Monstey_Counts.cases;
 Monstey_Fixture fixture({},Monstey_Serialize(Monstey_Rows()));auto services=fixture.Services();Monstey_Trace trace;
 Monstey_SequenceProvider(services.get_length,trace[0]);Monstey_SequenceProvider(services.get_position,trace[1]);
 Monstey_SequenceProvider(services.read,trace[2]);Monstey_SequenceProvider(services.new_array,trace[3]);
 Monstey_SequenceProvider(services.delete_array,trace[4]);Monstey_SequenceProvider(services.string_default,trace[5]);
 Monstey_SequenceProvider(services.string_assign_cstr,trace[6]);Monstey_SequenceProvider(services.string_destroy,trace[7]);
 Monstey_SequenceProvider(services.map_value,trace[8]);
 for(unsigned run=0;run<2;++run) {
  fixture.position=0;Monstey_Memory(fixture.view).Write(Monstey_Frame+0x48,Monstey_Stream);
  Monstey_Check(Monstey_FearReadConstraints(fixture.view,Monstey_Frame,services),"stateful sequence complete record failure");
 }
 const std::array<unsigned,9> expected{2,6,24,4,4,4,4,4,4};
 for(std::size_t kind=0;kind<trace.size();++kind) {
  Monstey_Check(trace[kind].size()==expected[kind],"unexpected provider consumption count");
  for(std::size_t i=0;i<trace[kind].size();++i)Monstey_Check(trace[kind][i]==i,"supplied target state was cloned/reset");
 }
 Monstey_Check(fixture.maps==4&&fixture.reads==24&&!fixture.array_live&&!fixture.string_live,"stateful protocol lifetime mismatch");
}
template<class Function>
void Monstey_InstallReplacement(Function& slot,std::weak_ptr<unsigned>& weak,std::vector<bool>& alive,bool clear) {
 auto lifetime=std::make_shared<unsigned>(0x60020u);weak=lifetime;
 auto original=slot;auto* destination=&slot;auto* weak_pointer=&weak;auto* observations=&alive;
 slot=[lifetime,original,destination,weak_pointer,observations,clear](auto...args) mutable -> decltype(auto) {
  // All state used after replacement is moved to automatic locals first. The
  // weak observation therefore does not read the destroyed target in a wrong
  // direct-reference implementation; it detects the premature destruction.
  auto next=original;auto* to=destination;auto* w=weak_pointer;auto* log=observations;
  if(clear)*to={};else *to=next;
  log->push_back(!w->expired());return next(args...);
 };
}
void Monstey_ReplaceService(Monstey_FearConstraintServices& services,unsigned index,
 std::weak_ptr<unsigned>& weak,std::vector<bool>& alive,bool clear) {
 switch(index) {
 case 0:Monstey_InstallReplacement(services.get_length,weak,alive,clear);break;
 case 1:Monstey_InstallReplacement(services.get_position,weak,alive,clear);break;
 case 2:Monstey_InstallReplacement(services.read,weak,alive,clear);break;
 case 3:Monstey_InstallReplacement(services.new_array,weak,alive,clear);break;
 case 4:Monstey_InstallReplacement(services.delete_array,weak,alive,clear);break;
 case 5:Monstey_InstallReplacement(services.string_default,weak,alive,clear);break;
 case 6:Monstey_InstallReplacement(services.string_assign_cstr,weak,alive,clear);break;
 case 7:Monstey_InstallReplacement(services.string_destroy,weak,alive,clear);break;
 case 8:Monstey_InstallReplacement(services.map_value,weak,alive,clear);break;
 default:throw std::runtime_error("Invalid provider index");
 }
}
Monstey_Outcome Monstey_RunPrepared(Monstey_Fixture& fixture,Monstey_FearConstraintServices& services,bool reference) {
 Monstey_Outcome result;
 try {result.result=reference?Monstey_ReferenceConstraints(fixture.view,Monstey_Frame,services):Monstey_FearReadConstraints(fixture.view,Monstey_Frame,services);}
 catch(const Monstey_RefFault& f){result={1,false,f.address,f.width,f.writing};}
 catch(const Monstey_FearDbFault& f){result={1,false,f.address,f.width,f.writing};}
 catch(const Monstey_RefBoundary&){result.kind=2;}
 catch(const Monstey_FearConstraintBoundaryFault&){result.kind=2;}
 return result;
}
void Monstey_ReplacingProviders(bool clear) {
 ++Monstey_Counts.groups;
 for(unsigned index=0;index<9;++index) {
  ++Monstey_Counts.cases;const auto input=Monstey_Serialize(Monstey_Rows());
  Monstey_Fixture expected({},input),actual({},input);auto es=expected.Services(),as=actual.Services();
  std::weak_ptr<unsigned> ew,aw;std::vector<bool> ea,aa;
  Monstey_ReplaceService(es,index,ew,ea,clear);Monstey_ReplaceService(as,index,aw,aa,clear);
  const auto want=Monstey_RunPrepared(expected,es,true),got=Monstey_RunPrepared(actual,as,false);
  if(want.kind)++Monstey_Counts.faults;
  Monstey_Check(want.kind==got.kind&&want.result==got.result&&want.address==got.address&&want.width==got.width&&want.writing==got.writing,"self-replacement failure boundary mismatch");
  Monstey_Check(ea==std::vector<bool>{true}&&aa==ea,"active target died during its own replacement");
  Monstey_Check(ew.expired()&&aw.expired(),"retired target retained after invocation");
  Monstey_Check(expected.events==actual.events&&expected.images==actual.images,"self-replacement event/memory prefix mismatch");
  Monstey_Check(expected.position==actual.position&&expected.reads==actual.reads&&expected.maps==actual.maps&&expected.array_live==actual.array_live&&expected.string_live==actual.string_live,"self-replacement retained lifetime mismatch");
  Monstey_Check(clear?(index==0?want.kind==0&&want.result:want.kind==2):want.kind==0&&want.result,"independent replacement fixture outcome invalid");
 }
}
}
int main(int argc,char**argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report path required");
  Monstey_ProtocolCases();Monstey_FaultCuts();Monstey_WidthsAndTypes();Monstey_ConcreteTable();
  Monstey_InitialHighEOF();Monstey_StatefulProviders();Monstey_ReplacingProviders(false);Monstey_ReplacingProviders(true);
  std::ofstream out(argv[1]);
  out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Counts.groups<<",\"cases\":"<<Monstey_Counts.cases<<",\"checks\":"<<Monstey_Counts.checks<<",\"fault_cases\":"<<Monstey_Counts.faults<<",\"all_nine_provider_kinds_tested\":true}\n";
  std::cout<<"PASS "<<Monstey_Counts.cases<<" cases "<<Monstey_Counts.checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}