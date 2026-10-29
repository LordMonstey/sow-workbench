#include "BorrowedOwnerFixture.hpp"
#include <memory>
namespace {
std::uint64_t ownchecks=0;
void A(bool ok,const char *message){++ownchecks;Check(ok,message);}
std::vector<std::string> groups;
void G(const char *name){current_group=name;groups.emplace_back(name);}
struct Retire { std::function<void()> effect; ~Retire(){effect();} };
void Normal(){
 G("independent_complete_real_family_composition");
 for(const auto &key: std::array<std::string,3>{"C/R/A","Ab/Cd/Ef","LongCategoryName/R/A"}){
  Fixture f({{key}},key+"=11\n");const U root=f.Run();
  const auto p=key.find('/'),q=key.find('/',p+1);
  const U cat=R(f.memory,root+12),rec=R(f.memory,cat+12),attr=R(f.memory,rec+12);
  A(f.Kind("output").bytes==20+48+4+static_cast<U>(key.size())+5,"independent allocation formula");
  A(R(f.memory,root+16)==1&&R(f.memory,root+8)==1,"wrapper/root words");
  A(Cstr(f.memory,R(f.memory,cat))==key.substr(0,p)&&Cstr(f.memory,R(f.memory,rec))==key.substr(p+1,q-p-1)&&Cstr(f.memory,R(f.memory,attr))==key.substr(q+1),"independent names");
  A(R(f.memory,R(f.memory,attr+12))==11&&R(f.memory,attr+10,1)==3,"real callback/materializer type/scalar");
  A(f.validations==1&&f.Kind("text").released,"ignored validator AL/effects and released text");
  ++cases;
 }
 G("independent_category_discriminator_not_other_counts");
 {Fixture f({{"/R/A"}},"/R/A=11\n");const U root=f.Run();A(R(f.memory,f.frame+0x34)==0&&R(f.memory,f.frame+0x3C)==1,"synthetic distinct counts");A(root==f.Kind("empty-root").address&&f.Count("allocate",0x100063D2)==0,"6371 C is zero");++cases;}
 G("independent_empty_unused_boundaries_and_null_text");
 {Fixture f({},"");auto allocator=f.services.new_array;f.services.new_array=[allocator](Mem&m,U site,U n){return site==0x100062BA?0u:allocator(m,site,n);};
 f.services.callback={};f.services.constraints.read={};const U root=f.Run();A(root!=0&&R(f.memory,root+16)==1,"zero-length null text has no invented guard");bool del0=false;for(auto&e:f.events)if(e.kind=="delete-array"&&e.a==0&&e.site==0x10006346)del0=true;A(del0,"null zero-byte text delete preserved");++cases;}
}
void ZeroAndLateInput(){
 G("independent_mapped_zero_scalar_no_guard_and_no_wrapper_increment");
 std::vector<std::uint8_t> bytes(0x10000,0xA5);Mem m{0,bytes};const U frame=0x3000,wrapper=frame+0x58;Monstey_FearUnpackedOpenServices services;Monstey_FearUnpackedLogicalStrings strings;
 W(m,wrapper+4,0x100);W(m,wrapper+8,0x180);W(m,0x100,0x200);W(m,0x180,0x240);W(m,0x21C,0x111);W(m,0x204,0x222);W(m,0x25C,0x333);W(m,0x258,0x444);
 services.new_sentinel=[](Mem&,bool parsed,U,U){return parsed?0x5100u:0x5000u;};
 services.constraints.get_length=[](Monstey_FearConstraintCall){return std::uint64_t{0};};services.constraints.get_position=[](Monstey_FearConstraintCall){return std::uint64_t{0};};
 services.get_length=[](Mem&,Monstey_FearConstraintCall){return std::uint64_t{0};};services.read=[](Mem&,Monstey_FearConstraintReadCall c){A(c.destination==0&&c.bytes==0,"mapped zero text allocation");return 0u;};
 services.new_array=[](Mem&,U site,U n){A(site==0x100062BA&&n==0,"zero text exact allocation");return 0u;};services.new_scalar=[](Mem&,U site,U n){A(site==0x1000637E&&n==20,"mapped null scalar exact allocation");return 0u;};
 services.delete_array=[](Mem&,Monstey_FearUnpackedDeleteCall c){A(c.address==0&&c.native_site==0x10006346,"mapped zero text release");};
 U erases=0,deletes=0;services.cleanup.erase_range=[&](Mem&,Monstey_FearUnpackedEraseCall c){++erases;A(c.parsed==(erases==1),"mapped zero normal map order");return c.hidden_result;};services.cleanup.delete_scalar=[&](Mem&,Monstey_FearUnpackedDeleteCall){++deletes;};
 services.materializer.string=[&](Mem view,Op op,U self,U a,U b,U c){return strings.Invoke(view,op,self,a,b,c);};
 A(Monstey_FearOpenUnpacked(m,wrapper,services)==0&&erases==2&&deletes==2,"mapped address zero follows stores and cleanup then skips wrapper ref");for(U n=0;n<5;++n)A(R(m,n*4)==0,"all mapped zero root stores");++cases;
 G("independent_text_argument_read_after_concrete_constraints");
 Fixture f({},"");const U alternate=f.base+0x1600;f.streams[alternate]={};W(f.memory,alternate,f.textVtable);auto position=f.services.constraints.get_position;f.services.constraints.get_position=[&,position](Monstey_FearConstraintCall c){W(f.memory,f.frame+0x50,alternate);return position(c);};A(f.Run()!=0,"late text stream selected");bool seen=false;for(auto&e:f.events)if(e.kind=="text-length")seen=e.a==alternate;A(seen,"constraints effects precede text stream read");++cases;
}
void Epochs(){
 G("independent_constraint_failure_cleanup_live_head_and_hidden");
 {Fixture f({{"C/R/A"}},"");f.failConstraintRead=true;const U fresh=f.base+0x4000;U got=0;
 f.eraseHook=[&](Mem&m,Monstey_FearUnpackedEraseCall c){A(c.native_site==0x10006258&&c.map==f.frame+0x28&&c.hidden_result==f.frame+0x10,"constraint inline erase frame");A(R(m,f.frame+4)==c.last&&R(m,f.frame)==c.first&&R(m,f.frame-4)==c.hidden_result,"inline outgoing cells");W(m,c.map+4,fresh);};
 f.services.cleanup.delete_scalar=[&](Mem&,Monstey_FearUnpackedDeleteCall c){got=c.address;A(c.native_site==0x10006262,"inline scalar site");};
 A(f.Run()==0&&got==fresh,"live head after erase");A(f.Count("text-length")==0&&R(f.memory,f.frame+0x2C)==fresh,"no text read/no inline zero");++cases;}
 G("independent_Read_full_DWORD_failure_no_text_free");
 for(U status:std::array<U,5>{1,0x100,0x80000000,UINT32_MAX,0x10000}){Fixture f({},"x\n");f.textReadOverride=[status](Mem&,Monstey_FearConstraintReadCall){return status;};A(f.Run()==0,"Read EAX nonzero");A(!f.Kind("text").released&&f.Count("delete-array")==0,"no invented Read-fail text free");A(R(f.memory,f.frame+0x20)==0&&R(f.memory,f.frame+0x24)==0&&R(f.memory,f.frame+0x2C)!=0,"only parsed map inline zeroed");++cases;}
 G("independent_Read_target_sample_after_both_PUSH_stores");
 {Fixture f({},"");auto allocate=f.services.new_array;f.services.new_array=[&,allocate](Mem&m,U site,U n){const U p=allocate(m,site,n);if(site==0x100062BA)W(m,f.textStream,f.frame-0xC);return p;};
 f.services.read=[&](Mem&m,Monstey_FearConstraintReadCall c){A(c.target==c.destination&&R(m,f.frame-8)==c.destination&&R(m,f.frame-4)==0,"aliased Read target overwritten by second push");A(c.stream==f.textStream,"cached stream identity");return 0u;};A(f.Run()!=0,"post-PUSH target composition");++cases;}
 G("independent_Read_failure_head_capture_before_parsed_zero");
 {Fixture f({},"");f.services.read=[&](Mem&m,Monstey_FearConstraintReadCall){W(m,f.frame+0x2C,f.frame+0x20);return 1u;};U erases=0;
 f.services.cleanup.erase_range=[&](Mem&,Monstey_FearUnpackedEraseCall c){++erases;A(c.hidden_result==f.frame+0x10,"Read failure hidden word");if(erases==2)A(c.native_site==0x1000630D&&c.last==f.frame+0x20&&c.first==0,"capture old head identity then read first after parsed zero");return c.hidden_result;};
 f.services.cleanup.delete_scalar=[](Mem&,Monstey_FearUnpackedDeleteCall){};A(f.Run()==0&&erases==2,"aliased inline failure completion");++cases;}
 G("independent_Read_failure_fault_before_parsed_zero");
 {Fixture f({},"");f.textReadOverride=[](Mem&,Monstey_FearConstraintReadCall){return 1u;};auto del=f.services.cleanup.delete_scalar;
 f.services.cleanup.delete_scalar=[&,del](Mem&m,Monstey_FearUnpackedDeleteCall c){del(m,c);if(c.native_site==0x100062EC)m.bytes=m.bytes.first(f.frame+0x2C-f.base);};bool fault=false;
 try{(void)f.Run();}catch(const Monstey_FearDbFault&e){fault=!e.writing&&e.address==f.frame+0x2C&&e.width==4;}
 A(fault&&R(f.memory,f.frame+0x20)==f.Kind("parsed-head").address&&R(f.memory,f.frame+0x24)==0,"62F1 read fault precedes both zero stores");A(f.Count("erase-constraints")==0&&!f.Kind("text").released,"failure prefix retained");++cases;}
 G("independent_sentinel_target_retirement_before_argument_read");
 {Fixture f({},"");const U alternate=f.base+0x1600;f.streams[alternate]={};W(f.memory,alternate,f.constraintVtable);auto ordinary=f.services.new_sentinel;bool retired=false;
 auto token=std::make_shared<Retire>();token->effect=[&]{retired=true;W(f.memory,f.frame+0x54,alternate);};
 f.services.new_sentinel=[&,ordinary,token](Mem&m,bool p,U map,U site){auto ordinaryCopy=ordinary;f.services.new_sentinel=ordinaryCopy;return ordinaryCopy(m,p,map,site);};token.reset();
 A(f.Run()!=0&&retired,"self-replaced sentinel target retired");bool seen=false;for(auto&e:f.events)if(e.kind=="constraint-length")seen=e.a==alternate;A(seen,"post-retirement live constraint argument");++cases;}
 G("independent_scalar_target_retirement_before_zero_stores");
 {Fixture f({},"");auto ordinary=f.services.new_scalar;U root=0;bool retired=false;auto token=std::make_shared<Retire>();token->effect=[&]{retired=true;W(f.memory,root+16,0x12345678);};
 f.services.new_scalar=[&,ordinary,token](Mem&m,U site,U n){auto next=ordinary;f.services.new_scalar=next;root=next(m,site,n);return root;};token.reset();const U result=f.Run();A(result==root&&retired&&R(f.memory,root+16)==1,"retirement precedes final zero then wrapper increment");++cases;}
 G("independent_text_allocator_state_and_lifetime");
 {Fixture f({{"C/R/A"}},"C/R/A=11\n");auto ordinary=f.services.new_array;U last=0;f.services.new_array=[ordinary,count=U{0},&last](Mem&m,U site,U n)mutable{last=++count;return ordinary(m,site,n);};A(f.Run()!=0&&last==2,"mutable captured state persists both allocations");++cases;}
}
void Faults(){
 G("independent_empty_scalar_each_zero_store_fault_prefix");
 for(U stop=0;stop<5;++stop){Fixture f({},"");auto allocate=f.services.new_scalar;U root=0;
 f.services.new_scalar=[&,allocate](Mem&m,U site,U n){root=allocate(m,site,n);m.bytes=m.bytes.first(root+stop*4-f.base);return root;};bool fault=false;
 try{(void)f.Run();}catch(const Monstey_FearDbFault&e){fault=e.writing&&e.address==root+stop*4&&e.width==4;}
 Mem full{f.base,f.image};A(fault,"specific empty zero-store range fault");for(U n=0;n<5;++n)A(R(full,root+n*4)==(n<stop?0u:0xA5A5A5A5u),"ordered zero-store prefix");A(f.Count("erase-parsed")==0,"no invented fault cleanup");++cases;}
 G("independent_empty_null_guard_and_constraint_publication_fault");
 {Fixture f({},"");f.nullEmptyRoot=true;bool caught=false;try{(void)f.Run();}catch(const Monstey_FearDbFault&e){caught=e.writing&&e.address==0&&e.width==4;}A(caught&&f.Count("erase-parsed")==0,"no invented scalar null guard");++cases;}
 {Fixture f({},"");f.services.new_sentinel=[](Mem&,bool,U,U){return 0u;};bool caught=false;try{(void)f.Run();}catch(const Monstey_FearDbFault&e){caught=e.writing&&e.address==0x35&&e.width==1;}A(caught&&R(f.memory,f.frame+0x2C)==0,"head publication precedes nil byte fault");++cases;}
 G("independent_unknown_callback_known_two_PUSH_prefix");
 {Fixture f({{"C/R/A"}},"C/R/A=11\nC/R/A=22\n");U returns=0;f.stringHook=[&](Mem&m,Monstey_FearUnpackedCallbackStringCall c){if(c.native_site==0x10006009&&++returns==1){W(m,f.frame-8,0xC0DE1234);W(m,f.B()-4,0xDEAD1111);W(m,f.B()-8,0xDEAD2222);}};bool fault=false;
 try{(void)f.Run();}catch(const Monstey_FearUnpackedOpenBoundaryFault&e){fault=e.native_site==0x10009E0A&&e.target==0xC0DE1234;}
 A(fault&&R(f.memory,f.B()-4)==f.frame+0x14&&R(f.memory,f.B()-8)==f.B()+0x10,"unknown identity follows exact outgoing stores");A(f.validations==1&&!f.Kind("text").released,"no opaque second callback/fault cleanup");++cases;}
 G("independent_missing_Read_gate_after_outgoing_words");
 {Fixture f({},"a\n");f.services.read={};bool fault=false;try{(void)f.Run();}catch(const Monstey_FearUnpackedOpenBoundaryFault&e){fault=e.native_site==0x100062CA&&e.target==f.textVtable+0xA004;}
 A(fault&&R(f.memory,f.frame-4)==2&&R(f.memory,f.frame-8)==f.Kind("text").address,"missing read consumes pushed args/target");A(f.Count("erase-parsed")==0,"no gate exception cleanup");++cases;}
 G("independent_public_wrapper_argument_fault_order");
 {Fixture f({},"");f.memory.bytes=f.memory.bytes.first(f.wrapper+8-f.base);bool fault=false;try{(void)f.Run();}catch(const Monstey_FearDbFault&e){fault=!e.writing&&e.address==f.wrapper+8&&e.width==4;}A(fault&&R(f.memory,f.wrapper-4)==0xA5A5A5A5,"wrapper constraints read before outgoing store");A(f.Count("sentinel")==0,"no body before wrapper arg fault");++cases;}
 {Fixture f({},"");f.memory.bytes=f.memory.bytes.first(f.wrapper+4-f.base);bool fault=false;try{(void)f.Run();}catch(const Monstey_FearDbFault&e){fault=!e.writing&&e.address==f.wrapper+8&&e.width==4;}A(fault,"both wrapper input words missing still reads constraints first");++cases;}
 G("independent_wrapper_ref_fault_after_completed_cleanup");
 {Fixture f({},"");auto del=f.services.cleanup.delete_scalar;U root=0;f.services.cleanup.delete_scalar=[&,del](Mem&m,Monstey_FearUnpackedDeleteCall c){del(m,c);if(c.native_site==0x100061BB){root=f.Kind("empty-root").address;m.bytes=m.bytes.first(root+16-f.base);}};bool fault=false;
 try{(void)f.Run();}catch(const Monstey_FearDbFault&e){fault=!e.writing&&e.address==root+16&&e.width==4;}A(fault&&f.Count("delete-scalar")==2,"wrapper live read only after both cleanup returns");A(R(f.memory,f.frame+0x20)==0&&R(f.memory,f.frame+0x2C)==0,"normal zero stores precede wrapper fault");++cases;}
}
void RebindingsAndFormula(){
 G("independent_live_wrapper_ref_word_wrap_after_cleanup");
 {Fixture f({},"");f.deleteHook=[&](Mem&m,Monstey_FearUnpackedDeleteCall c){if(c.native_site==0x100061BB)W(m,f.Kind("empty-root").address+16,UINT32_MAX);};const U root=f.Run();A(R(f.memory,root+16)==0,"postcleanup live ref wraps");++cases;}
 G("independent_line_rebind_next_byte_and_all_later_stages");
 {Fixture f({{"C/R/A"}},"C/R/A=11\nC/R/A=22\n");U returns=0;f.stringHook=[&](Mem&m,Monstey_FearUnpackedCallbackStringCall c){if(c.native_site==0x10006009&&++returns==1){f.replacement=f.image;m={f.base,f.replacement};const U text=f.Kind("text").address;W(m,text+15,'3',1);W(m,text+16,'3',1);}};const U root=f.Run(),cat=R(f.memory,root+12),rec=R(f.memory,cat+12),attr=R(f.memory,rec+12);A(returns==2&&R(f.memory,R(f.memory,attr+12))==33,"next line reads replacement buffer");Mem old{f.base,f.image};A(R(old,root)==0xA5A5A5A5&&f.memory.bytes.data()==f.replacement.data(),"later output uses caller replacement view");++cases;}
 G("independent_sizing_end_rebind_observed_after_stable_stage");
 {Fixture f({},"");auto call=f.services.materializer.string;bool done=false;f.services.materializer.string=[&,call](Mem m,Op op,U self,U a,U b,U c){U result=call(m,op,self,a,b,c);if(op==Op::destroy&&self==f.frame-0xC4+0x64){f.replacement=f.image;f.memory={f.base,f.replacement};W(f.memory,f.frame+0x34,1);W(f.memory,f.frame+0x38,2);W(f.memory,f.frame+0x3C,3);W(f.memory,f.frame+0x40,4);W(f.memory,f.frame+0x44,5);done=true;}return result;};f.failOutputAllocation=true;A(f.Run()==0&&done,"last sizing primitive changes outer view with old backing retained");bool found=false;for(auto&e:f.events)if(e.kind=="null-output")found=e.b==137;A(found,"outer count/formula read after concrete stable-view stage");++cases;}
 G("independent_U32_formula_random_context_no_large_allocation");
 U seed=0xC83D9701;for(U k=0;k<256;++k){Fixture f({},"");std::array<U,5>w{};for(auto&n:w){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;n=seed;}if(w[0]==0)w[0]=1;auto call=f.services.materializer.string;f.services.materializer.string=[&,call](Mem m,Op op,U self,U a,U b,U c){U result=call(m,op,self,a,b,c);if(op==Op::destroy&&self==f.frame-0xC4+0x64)for(U n=0;n<5;++n)W(m,f.frame+0x34+n*4,w[n]);return result;};f.failOutputAllocation=true;A(f.Run()==0,"wrapped allocation null branch");const auto wide=std::uint64_t{20}+16ull*(std::uint64_t(w[0])+w[1]+w[2])+4ull*w[3]+w[4];bool found=false;for(auto&e:f.events)if(e.kind=="null-output")found=e.b==static_cast<U>(wide);A(found&&f.Count("erase-parsed")==1&&f.Count("erase-constraints")==1,"independent widened then narrow formula and real cleanup");++cases;}
 G("independent_cleanup_rebind_reaches_second_map_and_wrapper");
 {Fixture f({},"");bool changed=false;f.eraseHook=[&](Mem&m,Monstey_FearUnpackedEraseCall c){if(c.parsed){f.replacement=f.image;m={f.base,f.replacement};W(m,f.Kind("empty-root").address+16,91);changed=true;}};const U root=f.Run();A(changed&&R(f.memory,root+16)==92&&f.Count("erase-constraints")==1,"cleanup replacement retained");Mem old{f.base,f.image};A(R(old,root+16)==0,"old view not incremented");++cases;}
}
}
int wmain(int argc,wchar_t**argv){try{if(argc!=2)throw std::runtime_error("NEW_REPORT required");Normal();ZeroAndLateInput();Epochs();Faults();RebindingsAndFormula();auto names=Monstey_Json::array();for(auto&s:groups)names.as_array().push_back(s);Monstey_WriteNewText(argv[1],Monstey_Json::object({{"status","PASS"},{"groups",groups.size()},{"cases",cases},{"independent_assertions",ownchecks},{"checks_including_shared_fixture",checks},{"group_names",names},{"original_native_entrypoint_invoked",false},{"original_image_loaded",false},{"original_instruction_bytes_interpreted",false},{"scope","Top-level independent assertions derived from retained pre-CPP ASM reference; borrowed explicitly attributed fixture and qualified frozen concrete child families."}}).dump(2)+"\n");std::cout<<"PASS "<<groups.size()<<" groups "<<cases<<" cases "<<ownchecks<<" independent assertions "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<current_group<<": "<<e.what()<<'\n';return 1;}}
