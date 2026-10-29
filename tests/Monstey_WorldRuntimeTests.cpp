#include "Monstey_WorldRuntimeProbe.hpp"
#include <bit>
#include <functional>
#include <iostream>
#include <map>
#include <limits>
using namespace Monstey;
namespace {
using A=std::uint64_t;using B=Monstey_Bytes;using J=Monstey_Json;
constexpr A Base=0x140000000,Reg=Base+0x2778220,Vec=0x100000,Scene=0x200000,Host=0x210000,Nodes=0x300000,Links=0x310000,Groups=0x400000,Culls=0x500000;
constexpr std::uint32_t Target=0xE5CCFB22;
class Memory final:public Monstey_WorldMemory {
public:
 std::map<A,B> regions;std::map<A,unsigned> hits;A partial{},changing{};std::size_t calls{};
 B& Make(A a,std::size_t n){return regions[a]=B(n);}
 Monstey_WorldRead Read(A a,std::size_t n)override{++calls;++hits[a];auto it=regions.upper_bound(a);if(it==regions.begin())return {{},299,false};--it;if(a-it->first>it->second.size()||n>it->second.size()-static_cast<std::size_t>(a-it->first))return {{},299,false};B b(it->second.begin()+static_cast<std::ptrdiff_t>(a-it->first),it->second.begin()+static_cast<std::ptrdiff_t>(a-it->first+n));if(a==partial&&!b.empty()){b.pop_back();return {b,299,false};}if(a==changing&&hits[a]>1&&!b.empty())b[0]^=1;return {b,0,true};}
};
void Bounds(B& b,std::size_t offset=0){for(unsigned i=0;i<3;++i){Monstey_PutU32(b,offset+i*4,std::bit_cast<std::uint32_t>(-100.f));Monstey_PutU32(b,offset+(i+3)*4,std::bit_cast<std::uint32_t>(100.f));}}
void CullBox(B& b,std::size_t offset){for(unsigned i=0;i<3;++i){Monstey_PutU32(b,offset+i*4,std::bit_cast<std::uint32_t>(-100.f));Monstey_PutU32(b,offset+16+i*4,std::bit_cast<std::uint32_t>(100.f));}Monstey_PutU32(b,offset+12,0x7FC00000);Monstey_PutU32(b,offset+28,0x80000000);}
Memory Fixture(){Memory m;auto& reg=m.Make(Reg,816);for(auto o:{768U,792U}){Monstey_PutU64(reg,o,Vec);Monstey_PutU64(reg,o+8,Vec+64);}auto& v=m.Make(Vec,64);Bounds(v);Bounds(v,32);Monstey_PutU64(v,24,Scene);Monstey_PutU64(v,56,Host);
 auto& s=m.Make(Scene,176);Monstey_PutU32(s,168,Target);Monstey_PutU64(s,56,Groups);Monstey_PutU32(s,64,1);Monstey_PutU64(s,88,Groups+72);Monstey_PutU32(s,96,1);auto& h=m.Make(Host,176);Monstey_PutU64(h,8,Nodes);Monstey_PutU32(h,16,2);Monstey_PutU32(h,168,0xCBB9F0CF);
 auto& ns=m.Make(Nodes,320);Bounds(ns);Bounds(ns,160);Monstey_PutU64(ns,72,Groups);Monstey_PutU64(ns,80,Links);Monstey_PutU64(ns,88,Links+16);ns[113]=7;Monstey_PutU64(ns,136,Nodes+160);Monstey_PutU64(ns,160+152,Nodes);auto& gs=m.Make(Links,16);Monstey_PutU64(gs,0,Groups);Monstey_PutU64(gs,8,Groups+72);
 auto& groups=m.Make(Groups,144);Bounds(groups,24);Bounds(groups,72+24);Monstey_PutU16(groups,2,1);Monstey_PutU16(groups,4,2);Monstey_PutU64(groups,16,Culls);
 auto& c=m.Make(Culls,96);CullBox(c,0);CullBox(c,48);Monstey_PutU64(c,32,0x600000);Monstey_PutU16(c,40,65535);c[43]=1;Monstey_PutU16(c,48+40,300);c[48+43]=0;
 auto& context=m.Make(0x600000,56);Monstey_PutU64(context,0,0x610000);auto& wrapper=m.Make(0x610000,8);Monstey_PutU64(wrapper,0,0x620000);auto& handle=m.Make(0x620000,16);Monstey_PutU64(handle,8,0x630000);auto& slot=m.Make(0x630000,16);Monstey_PutU64(slot,8,0x640000);return m;
}
J Snap(Memory& m,const Monstey_WorldLimits& l={}){return Monstey_WorldRuntimeSnapshot(m,Base,{0,0,0},Target,l);}
void Check(bool c){if(!c)throw std::runtime_error("Independent World runtime assertion failed");}
bool Has(const J& j,std::string_view kind){for(const auto& d:j.at("diagnostics").as_array())if(d.at("kind").as_string()==kind)return true;return false;}
}
int wmain(int argc,wchar_t** argv){try{if(argc!=2)throw std::runtime_error("Usage: WorldRuntimeTests NEW-REPORT.json");Monstey_RequireOfflineOutput(argv[1]);J::Array passed;auto Test=[&](const char* name,const std::function<void()>& f){f();passed.push_back(name);};
Test("loose_and_prefix_groups_without_owned_nodes_and_host_links",[]{auto m=Fixture();auto j=Snap(m);Check(j.at("complete_within_profile").as_bool());Check(j.at("target_groups").size()==2&&j.at("nodes_linking_target").size()==1);Check(j.at("decoded_nodes").as_uint()==2&&j.at("node_records").size()==2&&j.at("node_records").at(0).at("raw_160_bytes").as_string().size()==320);Check(j.at("scenes").at(0).at("raw_176_bytes").as_string().size()==352);Check(!j.at("rendering_verified").as_bool()&&!j.at("gather_execution_observed").as_bool());});
Test("cull_raw_flags_full_U16_draw_and_prefix_suffix",[]{auto m=Fixture();const auto j=Snap(m);const auto& c=j.at("target_groups").at(0).at("static_culls");Check(c.size()==2&&c.at(0).at("draw_U16_at_0x28").as_uint()==65535&&c.at(1).at("draw_U16_at_0x28").as_uint()==300);Check(c.at(0).at("flag_nonzero_2BDC38").as_bool()&&!c.at(1).at("flag_nonzero_2BDC38").as_bool());Check(c.at(0).at("in_eligible_prefix").as_bool()&&!c.at(1).at("in_eligible_prefix").as_bool());Check(c.at(0).at("rendered").is_null());});
Test("raw_context_chain_has_no_readiness_claim",[]{auto m=Fixture();const auto j=Snap(m);const auto& c=j.at("target_groups").at(0).at("static_culls").at(0).at("context_at_0x20");Check(c.at("diagnostic_pointer_chain_only").size()==3&&c.at("native_model_ready").is_null()&&!c.at("resolver_14114BBBC_executed").as_bool());});
Test("target_node_primary_is_included_with_empty_group_vector",[]{auto m=Fixture();Monstey_PutU64(m.regions[Scene],8,Nodes);Monstey_PutU32(m.regions[Scene],16,1);Monstey_PutU64(m.regions[Nodes],72,Groups+144);m.Make(Groups+144,72);const auto j=Snap(m);Check(j.at("target_groups").size()==3);Check(j.at("target_groups").at(2).at("origins").at(0).as_string()=="target_node_primary");});
Test("same_loose_prefix_pointer_retains_both_origins",[]{auto m=Fixture();Monstey_PutU64(m.regions[Scene],88,Groups);const auto j=Snap(m);const auto& o=j.at("target_groups").at(0).at("origins");Check(o.size()==2&&o.at(0).as_string()=="scene_loose"&&o.at(1).as_string()=="scene_prefix");});
Test("cull_padded_bounds_ignore_W_and_keep_raw_W",[]{auto m=Fixture();const auto j=Snap(m);const auto& c=j.at("target_groups").at(0).at("static_culls").at(0);Check(c.at("bounds_contains_point").as_bool());Check(c.at("bounds").at(3).at("value").as_number()==100.0);Check(c.at("raw_48_bytes").as_string().substr(24,8)=="0000c07f");});
Test("missing_target_does_not_mean_invisible",[]{auto m=Fixture();Monstey_PutU32(m.regions[Scene],168,0);const auto j=Snap(m);Check(j.at("target_groups").empty()&&!j.at("rendering_verified").as_bool());});
Test("partial_transfer_is_explicit_and_never_decoded",[]{auto m=Fixture();m.partial=Culls;const auto j=Snap(m);Check(Has(j,"partial_or_failed_read")&&!j.at("complete_within_profile").as_bool());Check(j.at("target_groups").at(0).at("static_culls").empty());});
Test("post_read_change_marks_non_atomic_snapshot",[]{auto m=Fixture();m.changing=Reg+768;const auto j=Snap(m);Check(!j.at("repeated_regions_unchanged").as_bool()&&!j.at("complete_within_profile").as_bool()&&!j.at("snapshot_atomic").as_bool());});
Test("malformed_registry_range_is_bounded",[]{auto m=Fixture();Monstey_PutU64(m.regions[Reg],776,Vec-1);const auto j=Snap(m);Check(Has(j,"invalid_or_capped_scene_vector"));});
Test("aggregate_node_cap_and_read_budget",[]{auto m=Fixture();Monstey_WorldLimits l;l.nodes=1;const auto j=Snap(m,l);Check(Has(j,"aggregate_node_limit"));auto n=Fixture();l={};l.read_bytes=1024;const auto k=Snap(n,l);Check(Has(k,"capture_budget_exceeded")&&k.at("requested_read_bytes").as_uint()<=1024);});
Test("malformed_group_range_and_counts",[]{auto m=Fixture();Monstey_PutU64(m.regions[Nodes],88,Links+7);Monstey_PutU16(m.regions[Groups],2,3);const auto j=Snap(m);Check(Has(j,"invalid_or_capped_node_groups")&&Has(j,"invalid_or_capped_static_counts"));});
Test("NaN_and_negative_zero_keep_raw_words",[]{auto m=Fixture();Monstey_PutU32(m.regions[Culls],0,0xFFC00000);Monstey_PutU32(m.regions[Culls],4,0x80000000);const auto j=Snap(m);const auto& b=j.at("target_groups").at(0).at("static_culls").at(0).at("bounds");Check(b.at(0).at("word").as_string()=="0xFFC00000"||b.at(0).at("word").as_string()=="0xffc00000");Check(b.at(0).at("value").is_null()&&b.at(1).at("word").as_string()=="0x80000000");});
Test("parent_cycle_is_diagnostic_not_an_infinite_walk",[]{auto m=Fixture();Monstey_PutU64(m.regions[Nodes],152,Nodes+160);const auto j=Snap(m);Check(Has(j,"parent_cycle"));});
Test("planes_finite_guard_and_point_domain",[]{auto m=Fixture();m.regions[Nodes][112]=1;Monstey_PutU64(m.regions[Nodes],120,0x700000);auto& pl=m.Make(0x700000,20);Monstey_PutU32(pl,0,0x7F800000);const auto j=Snap(m);Check(j.at("diagnostic_cell_candidates").size()==1);bool threw=false;try{(void)Monstey_WorldRuntimeSnapshot(m,Base,{std::numeric_limits<float>::quiet_NaN(),0,0},Target);}catch(const Monstey_Error&){threw=true;}Check(threw);});
Test("invalid_user_pointer_is_not_read",[]{auto m=Fixture();Monstey_PutU64(m.regions[Groups],16,0xFFFFFFFFFFFFFFF0);const auto j=Snap(m);Check(Has(j,"invalid_user_range"));});
Test("invalid_limit_and_image_base_overflow_reject_before_reads",[]{auto m=Fixture();Monstey_WorldLimits l;l.nodes=0;bool threw=false;try{(void)Snap(m,l);}catch(const Monstey_Error&){threw=true;}Check(threw&&m.calls==0);threw=false;try{(void)Monstey_WorldRuntimeSnapshot(m,std::numeric_limits<A>::max(),{0,0,0},Target);}catch(const Monstey_Error&){threw=true;}Check(threw&&m.calls==0);});
Monstey_WriteNewText(argv[1],J::object({{"schema","Monstey.WorldRuntimeSyntheticTests/1"},{"pass",true},{"groups",passed},{"count",static_cast<A>(passed.size())},{"process_writes",false},{"game_functions_called",false}}).dump(2)+"\n");std::cout<<"PASS "<<passed.size()<<" World runtime groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}