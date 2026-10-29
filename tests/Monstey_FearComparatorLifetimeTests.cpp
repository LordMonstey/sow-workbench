#include "Monstey_Core.hpp"
#include "Monstey_FearDbOpen.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>
using namespace Monstey;
namespace {
using U=std::uint32_t;U checks=0;
void Need(bool x,const char*m){++checks;if(!x)throw std::runtime_error(m);}
void W(std::vector<std::uint8_t>&b,U at,U x){for(U n=0;n<4;++n)b.at(at+n)=static_cast<std::uint8_t>(x>>(n*8));}
U R(const Monstey_FearDbPackedImage&i,U address){const auto at=address-i.base;U x=0;for(U n=0;n<4;++n)x|=U(i.allocation.at(at+n))<<(n*8);return x;}
struct Arena{
 Monstey_FearDbResources resources;std::array<std::uint8_t,2> filename{'q',0};
 Arena(){resources.allocations.reserve(3);resources.cache.reserve(4);for(U k=0;k<3;++k){Monstey_FearDbPackedImage i;i.base=0x2000+k*0x100;i.database=i.base;i.allocation.resize(64,0xA5);W(i.allocation,0,i.base);W(i.allocation,4,i.base+0x20);W(i.allocation,16,17);resources.allocations.push_back(std::move(i));}resources.cache={0x2000,0x2100};}
 Monstey_FearDbMemory File(){return {0x1000,filename};}
 U Ref(U k){return R(resources.allocations.at(k),resources.allocations.at(k).base+16);}
};
struct State{
 Arena*arena=nullptr;std::function<void()> replace;std::vector<std::string>events;bool alive=true,alive_after_replace=true;U old_calls=0,new_calls=0;
};
struct Death{State*state;~Death(){state->alive=false;state->events.push_back("old-target-retired");state->arena->resources.cache.at(1)=0x2200;}};
// Same class/operator code for both registered instances. All closure fields used
// after assignment are copied into automatic locals BEFORE replacement.
struct Probe{
 State*state;U tag,calls=0;std::shared_ptr<Death>death;std::array<std::uint8_t,256>force_owned_heap{};
 std::int32_t operator()(U,U){
  auto*s=state;const U which=tag;const U ordinal=++calls;
  if(which==1)++s->old_calls;else ++s->new_calls;
  s->events.push_back((which==1?"old":"new")+std::string("-call-")+std::to_string(ordinal));
  if(which==1&&ordinal==1){s->replace();s->alive_after_replace=s->alive;s->events.push_back(s->alive?"after-replace-old-alive":"after-replace-old-retired");}
  return which==1?1:0;
 }
};
std::shared_ptr<Death> Token(State&s){auto p=std::make_shared<Death>();p->state=&s;return p;}
// Independent bounded native-identity reference: sample one existing target
// instance once, share its persistent state; never copy the callable target.
U FixedLookup(Arena&a,std::shared_ptr<Probe>&registry){
 auto sampled=registry;auto*backing=a.resources.cache.data();std::size_t index=0;
 if(a.resources.cache.empty())return 0;
 for(;;){Need(a.resources.cache.data()==backing,"reference backing stable");const U db=a.resources.cache.at(index);U candidate=0;for(const auto&i:a.resources.allocations)if(i.base==db)candidate=R(i,db+4);
  if(candidate&&(*sampled)(0x1000,candidate)==0){const U selected=a.resources.cache.at(index);Monstey_FearDbRetainSelected(a.resources,selected);return a.resources.cache.at(index);}
  const auto end=a.resources.cache.size();++index;if(index==end)return 0;
 }
}
Monstey_Json Events(const std::vector<std::string>&events){auto j=Monstey_Json::array();for(auto&s:events)j.as_array().push_back(s);return j;}
Monstey_Json ReplacementWitness(){
 Arena actual,reference;auto*actual_back=actual.resources.cache.data();auto*reference_back=reference.resources.cache.data();
 std::array<const std::uint8_t*,3> actual_images{},reference_images{};for(std::size_t k=0;k<3;++k){actual_images[k]=actual.resources.allocations[k].allocation.data();reference_images[k]=reference.resources.allocations[k].allocation.data();}
 State a{&actual,{},{}},b{&reference,{},{}};Monstey_FearDbNameCompare compare;auto token=Token(a);compare=Probe{&a,1,0,token,{}};token.reset();
 a.replace=[&]{compare=Probe{&a,2,0,{}, {}};};
 auto ref_token=Token(b);auto registry=std::make_shared<Probe>(Probe{&b,1,0,ref_token,{}});ref_token.reset();b.replace=[&]{registry=std::make_shared<Probe>(Probe{&b,2,0,{}, {}});};
 const U got=Monstey_FearDbOpenExisting(actual.resources,actual.File(),0x1000,compare);const U expected=FixedLookup(reference,registry);

 Need(got==0&&expected==0,"fixed target snapshot versus live stdFunction target differs");Need(a.alive_after_replace&&b.alive_after_replace,"target retirement epoch differs inside first call");Need(a.old_calls==2&&a.new_calls==0&&b.old_calls==2&&b.new_calls==0,"native target sampled once versus refreshed host target");Need(actual.Ref(2)==17&&reference.Ref(2)==17,"live selected refcount delta");
 Need(actual.resources.cache.data()==actual_back&&reference.resources.cache.data()==reference_back,"cache images/backing unchanged");for(std::size_t k=0;k<3;++k)Need(actual_images[k]==actual.resources.allocations[k].allocation.data()&&reference_images[k]==reference.resources.allocations[k].allocation.data(),"database backing remains alive");
 return Monstey_Json::object({{"actual_result",got},{"fixed_native_target_reference_result",expected},{"actual_old_calls",a.old_calls},{"actual_new_calls",a.new_calls},{"reference_old_calls",b.old_calls},{"reference_new_calls",b.new_calls},{"actual_old_target_alive_after_assignment",a.alive_after_replace},{"reference_old_target_alive_after_assignment",b.alive_after_replace},{"same_callable_class_and_operator_code",true},{"Cxx_target_instance_changed",true},{"images_and_cache_backing_stable",true},{"actual_third_refcount",actual.Ref(2)},{"reference_third_refcount",reference.Ref(2)},{"actual_events",Events(a.events)},{"reference_events",Events(b.events)},{"scope","Fixed callee-guaranteed sampled target and shared mutable state; caller replacement is permitted. Same class/operator code is separate from target-instance identity. No physical original ABI claim."}});
}
void ConformingControls(){
 {Arena a;Monstey_FearDbNameCompare compare;U last=0;compare=[&,n=U{0}](U,U)mutable{last=++n;if(n==1)a.resources.cache[1]=0x2200;return (n&1u)?1:0;};
 Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2200,"fixed target lookup1");Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2200,"fixed target lookup2");Need(last==4&&a.Ref(2)==19,"mutable value state persists without target reassignment");}
 {Arena a;Monstey_FearDbNameCompare compare=[&](U,U){a.resources.cache[0]=0x2200;return 0;};Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2200&&a.Ref(2)==18&&a.Ref(0)==17,"allowed match-row mutation observed on retain and return");}
 {Arena a;auto*back=a.resources.cache.data();U calls=0;Monstey_FearDbNameCompare compare=[&](U,U){if(++calls==1)a.resources.cache.push_back(0x2200);return calls==3?0:1;};Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2200&&calls==3&&a.resources.cache.data()==back,"allowed count growth within stable capacity");}
 {Arena a;U calls=0;Monstey_FearDbNameCompare compare=[&](U,U){++calls;a.resources.cache.resize(0);return 1;};bool fault=false;try{(void)Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare);}catch(const std::out_of_range&){fault=true;}Need(fault&&calls==1,"shrunk live end does not invent miss before consumed outside-owned cursor");}
}
Monstey_Json NamesLiveTargetControl(){
 Arena arena;State s{&arena,{},{}};Monstey_FearDbNameCompare compare;auto token=Token(s);compare=Probe{&s,1,0,token,{}};token.reset();s.replace=[&]{compare=Probe{&s,2,0,{}, {}};};std::vector<std::uint8_t>graph(0x200,0);for(U n=0;n<3;++n)W(graph,n*16,0x4000+n*0x20);
 const U row=Monstey_FearDbBinaryNameSearch({0x3000,graph},0x3000,3,0x1000,compare);Need(row==0x3020&&s.old_calls==1&&s.new_calls==1&&s.alive_after_replace,"7020 live IAT timing and active target retained through return");return Monstey_Json::object({{"result",row},{"old_calls",s.old_calls},{"new_calls",s.new_calls},{"native_site","10007056 reads the IAT for each comparison; do not apply Open EBX snapshot universally to names API."}});
}
}

namespace {
U total_cases=6;std::vector<std::string>group_names{"rebound_frozen_replacement_witness","unchanged_fixed_target_cache_controls","Names_live_target_active_lifetime"};
void Group(const char*name){group_names.emplace_back(name);}
struct LifetimeEffect{std::function<void()> effect;~LifetimeEffect(){if(effect)effect();}};
std::shared_ptr<LifetimeEffect> Life(std::function<void()> effect){auto p=std::make_shared<LifetimeEffect>();p->effect=std::move(effect);return p;}
struct CopyFailure{
 bool*fail;explicit CopyFailure(bool*f):fail(f){};CopyFailure(const CopyFailure&o):fail(o.fail){if(*fail)throw std::runtime_error("owned copy failure");}CopyFailure(CopyFailure&&)=default;std::int32_t operator()(U,U){return 19;}
};
void Extras(){
 Group("shared_copy_move_and_stdFunction_configuration");
 {U seen=0;std::function<std::int32_t(U,U)> source=[n=U{0},&seen](U,U)mutable{seen=++n;return static_cast<std::int32_t>(n);};Monstey_FearDbNameCompare first=source,copy=first;Need(copy(1,2)==1&&first(1,2)==2&&seen==2,"holder copies share persistent mutable target");auto moved=std::move(copy);Need(!copy&&moved(1,2)==3&&seen==3,"holder move keeps same target");Need(source(1,2)==1,"new stdFunction configuration is an explicit target copy, not a holder copy");++total_cases;}
 {bool fail=false;std::function<std::int32_t(U,U)> source=CopyFailure(&fail);Monstey_FearDbNameCompare target=[](U,U){return 7;};fail=true;bool caught=false;try{target=source;}catch(const std::runtime_error&e){caught=std::string(e.what())=="owned copy failure";}Need(caught&&target(1,2)==7,"failed target construction does not replace existing target");++total_cases;}
 Group("Open_clear_keeps_sampled_target_then_resamples_next_lookup");
 {Arena a;Monstey_FearDbNameCompare compare;U visits=0;bool alive=true;auto token=Life([&]{alive=false;});compare=[&,token](U,U){auto*slot=&compare;auto*live=&alive;auto*count=&visits;++*count;if(*count==1){*slot={};Need(*live,"Open sampled target alive inside clearing callback");}return 1;};token.reset();Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0&&visits==2&&!alive&&!compare,"cleared outer slot cannot affect current lookup snapshot");bool missing=false;try{(void)Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare);}catch(const std::invalid_argument&){missing=true;}Need(missing,"next lookup samples cleared target");++total_cases;}
 {Arena a;State st{&a,{},{}};Monstey_FearDbNameCompare compare;auto token=Token(st);compare=Probe{&st,1,0,token,{}};token.reset();st.replace=[&]{compare=Probe{&st,2,0,{}, {}};};Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0,"first lookup retains old target");Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2000&&st.old_calls==2&&st.new_calls==1&&a.Ref(0)==18,"next lookup samples replacement and retains selected live row");++total_cases;}
 Group("Names_clear_is_consumed_at_next_nonnull_comparison");
 {std::vector<std::uint8_t> graph(48);for(U n=0;n<3;++n)W(graph,n*16,0x4000+n*16);Monstey_FearDbNameCompare compare;bool alive=true;U visits=0;auto token=Life([&]{alive=false;});compare=[&,token](U,U){auto*slot=&compare;auto*live=&alive;auto*count=&visits;++*count;*slot={};Need(*live,"Names active target alive until primitive return");return 1;};token.reset();bool missing=false;try{(void)Monstey_FearDbBinaryNameSearch({0x3000,graph},0x3000,3,0x1000,compare);}catch(const std::invalid_argument&){missing=true;}Need(missing&&visits==1&&!alive,"Names reloads cleared current target and preserves retirement prefix");++total_cases;}
 Group("Open_nested_lookup_uses_new_target_outer_keeps_old");
 {Arena a;State st{&a,{},{}};Monstey_FearDbNameCompare compare;auto token=Token(st);compare=Probe{&st,1,0,token,{}};token.reset();U nested=0;st.replace=[&]{compare=Probe{&st,2,0,{}, {}};nested=Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare);};Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0&&nested==0x2000&&st.old_calls==2&&st.new_calls==1&&a.Ref(0)==18,"recursive lookup snapshots current new target independently");++total_cases;}
 Group("Open_target_retirement_precedes_OpenNew_filename_recheck");
 {Arena a;Monstey_FearDbNameCompare compare;bool retired=false;auto token=Life([&]{retired=true;a.filename[0]=0;});compare=[&,token](U,U){auto*slot=&compare;*slot=[](U,U){return 1;};return 1;};token.reset();Monstey_FearDbPackedInput input{{},7,77};Need(Monstey_FearDbOpenNew(a.resources,a.File(),0x1000,input,0x5000,compare)==0&&retired&&input.position==7&&input.read_calls==77,"lookup holder retires before OpenNew guard, no extra function-scope retention");++total_cases;}
 Group("Open_return_word_is_read_before_sampled_target_retirement");
 {Arena a;Monstey_FearDbNameCompare compare;bool retired=false;auto token=Life([&]{retired=true;a.resources.cache[0]=0x2200;});compare=[&,token](U,U){auto*slot=&compare;*slot=[](U,U){return 1;};return 0;};token.reset();const U result=Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare);Need(retired&&result==0x2000&&a.resources.cache[0]==0x2200&&a.Ref(0)==18&&a.Ref(2)==17,"live matched retain/return finish before snapshot holder destruction");++total_cases;}
 Group("null_target_consumed_only_after_original_guards_and_name_read");
 {Arena a;Need(Monstey_FearDbOpenExisting(a.resources,{0,{}},0,{})==0,"null filename skips all boundaries");++total_cases;}
 {Arena a;a.filename[0]=0;Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,{})==0,"empty filename skips comparator");++total_cases;}
 {Arena a;a.resources.cache.clear();Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,{})==0,"empty cache skips null target");++total_cases;}
 {Arena a;for(auto&i:a.resources.allocations)W(i.allocation,4,0);Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,{})==0,"nonnull cache with only null names never requires sampled comparator");++total_cases;}
 {Arena a;bool missing=false;try{(void)Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,{});}catch(const std::invalid_argument&){missing=true;}Need(missing&&a.Ref(0)==17,"first consumed nonnull comparison requires sampled target");++total_cases;}
 {Arena a;bool fault=false;try{(void)Monstey_FearDbOpenExisting(a.resources,{0,{}},0x1000,{});}catch(const Monstey_FearDbFault&e){fault=e.address==0x1000&&e.width==1&&!e.writing;}Need(fault,"filename first-byte fault precedes missing target gate");++total_cases;}
 Group("Open_full_signed_EAX_zero_not_low_byte");
 for(auto result:std::array<std::int32_t,5>{0,256,-256,INT32_MIN,INT32_MAX}){Arena a;U calls=0;Monstey_FearDbNameCompare compare=[&](U,U){return ++calls==1?result:0;};const U expected=result==0?0x2000u:0x2100u;Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==expected&&calls==(result==0?1u:2u),"full signed EAX zero consumed");++total_cases;}
 Group("exception_keeps_prefix_and_retires_sampled_target_on_unwind");
 {Arena a;Monstey_FearDbNameCompare compare;bool alive=true;auto token=Life([&]{alive=false;});compare=[&,token](U,U)->std::int32_t{auto*slot=&compare;auto*live=&alive;auto*arena=&a;*slot=[](U,U){return 0;};Need(*live,"old sampled target alive before throwing");arena->resources.cache[1]=0x2200;throw std::runtime_error("owned comparator throw");};token.reset();bool caught=false;try{(void)Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare);}catch(const std::runtime_error&e){caught=std::string(e.what())=="owned comparator throw";}Need(caught&&!alive&&a.Ref(0)==17&&a.resources.cache[1]==0x2200,"exception preserves preceding mutation and releases snapshot");Need(Monstey_FearDbOpenExisting(a.resources,a.File(),0x1000,compare)==0x2000&&a.Ref(0)==18,"published replacement remains usable after exception");++total_cases;}
}
}
int wmain(int argc,wchar_t**argv){try{if(argc!=2)throw std::runtime_error("NEW_REPORT required");ConformingControls();auto witness=ReplacementWitness();auto names=NamesLiveTargetControl();Extras();auto groups=Monstey_Json::array();for(auto&s:group_names)groups.as_array().push_back(s);Monstey_WriteNewText(argv[1],Monstey_Json::object({{"status","PASS"},{"groups",group_names.size()},{"cases",total_cases},{"checks",checks},{"group_names",groups},{"fixed_Open_witness",witness},{"live_Names_control",names},{"native_original_entrypoint_invoked",false},{"original_image_loaded",false},{"original_instruction_bytes_interpreted",false},{"provider_contract","Callee guarantees sampled shared target for one Open lookup; Names reads current holder at each comparison. Caller replacement/clear and mutable value state supported."}}).dump(2)+"\n");std::cout<<"PASS "<<group_names.size()<<" groups "<<total_cases<<" cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
