#include "Monstey_WorldRuntimeProbe.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <utility>
using namespace Monstey;
namespace {
using A=std::uint64_t;using B=Monstey_Bytes;using J=Monstey_Json;
constexpr A Receiver=0x500000,First=0x600000,Second=0x700000,MaxUser=0x00007fffffffffffULL;
const std::string Profile="6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455";
const std::string Installed="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef";
void Need(bool v){Monstey_Require(v,"Receiver row test assertion failed");}
void Status(const J&j,const char*s){Monstey_Require(j["status"].as_string()==s,j.dump());}
B Words(std::initializer_list<A> values){B b(values.size()*8);std::size_t i=0;for(A v:values)Monstey_PutU64(b,i++*8,v);return b;}
B HexBytes(const std::string&s){Need(s.size()%2==0);B b;for(std::size_t i=0;i<s.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return b;}
class Monstey_RowMemory final:public Monstey_WorldMemory {
public:
 std::map<A,B> regions;std::map<A,unsigned> hits;
 std::vector<std::pair<A,std::size_t>> trace;
 std::function<void(A,unsigned,Monstey_WorldRead&)> modify;
 Monstey_WorldRead Read(A a,std::size_t n) override {
  trace.emplace_back(a,n);const auto hit=++hits[a];Monstey_WorldRead r{{},299,false};
  auto it=regions.upper_bound(a);if(it!=regions.begin()){--it;const A offset=a-it->first;
   if(offset<=it->second.size()&&n<=it->second.size()-static_cast<std::size_t>(offset))
    r={B(it->second.begin()+static_cast<std::ptrdiff_t>(offset),it->second.begin()+static_cast<std::ptrdiff_t>(offset+n)),0,true};}
  if(modify)modify(a,hit,r);return r;
 }
};
Monstey_RowMemory Fixture(A receiver=Receiver){Monstey_RowMemory m;
 m.regions[receiver+0x5c8]=Words({First,First+16});m.regions[First]=Words({0x900000,0x910000});
 m.regions[receiver+0x5f8]=Words({Second,Second+8});m.regions[Second]=Words({0x920000});return m;}
J Shot(Monstey_RowMemory&m,const Monstey_WorldReceiverRowLimits&l={}){return Monstey_WorldReceiverRowsSnapshot(m,Receiver,Profile,l);}
void Scope(const J&j){for(const char*k:{"row_pointers_dereferenced","row_pointer_validity_verified","receiver_profile_authenticated","profile_verified_against_process","snapshot_atomic","receiver_lifetime_retained","concurrent_lifetime_proved","native_preceding_mutations_executed","call_gates_evaluated","initial_activation_verified","registration_verified","typed_navigation_association_verified","native_functions_called","process_memory_written","runtime_verified"})Need(!j[k].as_bool());Need(j["row_pointer_values_only"].as_bool());}
bool Has(const J&j,std::string_view kind){for(const auto&d:j["diagnostics"].as_array())if(d["kind"].as_string()==kind)return true;return false;}
}
int wmain(int argc,wchar_t**argv){try{
 Monstey_Require(argc==2||argc==3,"Usage: ReceiverRowsTests NEW_REPORT [paired-consumer-proof.json]");
 const bool source_profile_qualified=argc==3;
 if(source_profile_qualified)
  Monstey_RequireOfflineOutput(argv[1],std::array<std::filesystem::path,1>{argv[2]});
 else
  Monstey_RequireOfflineOutput(argv[1]);
 J::Array groups;
 const auto test=[&](const char*n,const std::function<void()>&f){try{f();}catch(const std::exception&e){throw Monstey_Error("receiver_rows_test",std::string(n)+": "+e.what());}groups.push_back(n);};
 if(source_profile_qualified){
 test("paired_native_arrays_steps_and_distinct_descriptor_lanes",[&]{
  Need(Monstey_HashSha256(std::filesystem::path(argv[2]))=="c389fe8762cc7ba61ff0e6fa55fbcfc676c7456103dc1bf7a0985a1f5db504e3");const J p=J::Parse(Monstey_ReadText(argv[2]));
  Need(p["inputs"].size()==2&&p["inputs"][0u]["sha256"].as_string()==Profile&&p["inputs"][1u]["sha256"].as_string()==Installed);
  B raw;for(const auto&r:p["ranges"].as_array())if(r["ea"].as_string()=="0x140c2fdc0")raw=HexBytes(r["hex"].as_string());Need(raw.size()==1234);
  const auto code=[&](A va,const char*h){const B expected=HexBytes(h);const A o=va-0x140c2fdc0;Need(o<=raw.size()&&expected.size()<=raw.size()-o&&std::equal(expected.begin(),expected.end(),raw.begin()+static_cast<std::ptrdiff_t>(o)));};
  code(0x140c2ffbd,"498b9dc8050000");code(0x140c2ffd0,"493b9dd0050000");code(0x140c2ffdd,"488b33");code(0x140c3011a,"4883c308");
  code(0x140c30123,"498b9df8050000");code(0x140c30130,"493b9d00060000");code(0x140c3013d,"488b3b");code(0x140c30278,"4883c308");
  code(0x140c30044,"498b44c908");code(0x140c301a4,"498b04c9");
 });
 }
 test("both_arrays_preserve_native_order_and_pointer_values",[]{auto m=Fixture();const J j=Shot(m);Status(j,"ROWS_OBSERVED");Scope(j);Need(j["vectors"].size()==2);const auto&a=j["vectors"][0u];const auto&b=j["vectors"][1u];Need(a["begin_offset"].as_string()==Monstey_Hex(0x5c8)&&a["end_offset"].as_string()==Monstey_Hex(0x5d0)&&b["begin_offset"].as_string()==Monstey_Hex(0x5f8)&&b["end_offset"].as_string()==Monstey_Hex(0x600));Need(a["entries"].size()==2&&b["entries"].size()==1&&a["entries"][0u]["row_pointer"].as_string()==Monstey_Hex(0x900000,16)&&b["entries"][0u]["row_pointer"].as_string()==Monstey_Hex(0x920000,16));Need(j["read_calls"].as_uint()==4&&j["requested_read_bytes"].as_uint()==56&&j["stability_read_calls"].as_uint()==4&&j["stability_read_bytes"].as_uint()==56);});
 test("paired_installed_profile_same_observations",[]{auto a=Fixture(),b=Fixture();J x=Shot(a),y=Monstey_WorldReceiverRowsSnapshot(b,Receiver,Installed);y["image_sha256"]=Profile;Need(x==y);});
 test("no_row_dereference_or_other_receiver_table",[]{auto m=Fixture();Status(Shot(m),"ROWS_OBSERVED");for(const auto&[a,n]:m.trace)Need((a==Receiver+0x5c8&&n==16)||(a==Receiver+0x5f8&&n==16)||(a==First&&n==16)||(a==Second&&n==8));});
 test("null_and_high_raw_pointers_are_not_certified_as_rows",[]{auto m=Fixture();m.regions[First]=Words({0,std::numeric_limits<A>::max()});const J j=Shot(m);Status(j,"ROWS_OBSERVED");Scope(j);Need(j["vectors"][0u]["entries"][0u]["row_pointer"].as_string()==Monstey_Hex(0,16)&&j["vectors"][0u]["entries"][1u]["row_pointer"].as_string()==Monstey_Hex(std::numeric_limits<A>::max(),16));});
 test("duplicates_and_alias_arrays_preserved",[]{auto m=Fixture();m.regions[First]=Words({0x900000,0x900000});m.regions[Receiver+0x5f8]=Words({First,First+16});const J j=Shot(m);Status(j,"ROWS_OBSERVED");Need(j["read_calls"].as_uint()==3&&j["vectors"][0u]["entries"]==j["vectors"][1u]["entries"]);});
 test("empty_null_vectors_need_only_headers",[]{auto m=Fixture();m.regions[Receiver+0x5c8]=Words({0,0});m.regions[Receiver+0x5f8]=Words({0,0});const J j=Shot(m);Status(j,"ROWS_OBSERVED");Need(j["read_calls"].as_uint()==2&&j["requested_read_bytes"].as_uint()==32&&j["vectors"][0u]["entries"].empty());});
 test("empty_noncanonical_equal_endpoints_not_dereferenced",[]{auto m=Fixture();m.regions[Receiver+0x5c8]=Words({std::numeric_limits<A>::max(),std::numeric_limits<A>::max()});Status(Shot(m),"ROWS_OBSERVED");Need(!m.hits.contains(std::numeric_limits<A>::max()));});
 test("unaligned_array_base_is_not_rejected",[]{auto m=Fixture();m.regions[First+1]=Words({1,2});m.regions[Receiver+0x5c8]=Words({First+1,First+17});Status(Shot(m),"ROWS_OBSERVED");});
 test("unsupported_profile_no_reads_even_with_bad_limits",[]{auto m=Fixture();Monstey_WorldReceiverRowLimits l;l.rows_per_vector=65537;l.capture.read_bytes=0;Status(Monstey_WorldReceiverRowsSnapshot(m,0,"unknown",l),"UNKNOWN");Need(m.trace.empty());});
 test("profile_embedded_nul_is_not_a_supported_prefix",[]{auto m=Fixture();const std::string p=Profile+std::string(1,'\0')+"suffix";Status(Monstey_WorldReceiverRowsSnapshot(m,Receiver,p),"UNKNOWN");Need(m.trace.empty());});
 test("supported_profile_bad_limits_throw_before_reads",[]{for(unsigned i=0;i<5;++i){auto m=Fixture();Monstey_WorldReceiverRowLimits l;if(i==0)l.rows_per_vector=65537;if(i==1)l.capture.read_bytes=0;if(i==2)l.capture.read_bytes=32*1024*1024+1;if(i==3)l.capture.read_calls=0;if(i==4)l.capture.read_calls=100001;bool caught=false;try{(void)Shot(m,l);}catch(const Monstey_Error&e){caught=e.code=="world_receiver_rows_limits";}Need(caught&&m.trace.empty());}});
 test("irrelevant_world_limits_unused",[]{auto m=Fixture();Monstey_WorldReceiverRowLimits l;l.capture.scenes=0;l.capture.nodes=std::numeric_limits<std::size_t>::max();l.capture.groups=0;l.capture.links_per_node=0;l.capture.culls_per_group=0;Status(Shot(m,l),"ROWS_OBSERVED");});
 test("zero_row_budget_allows_only_empty_vectors",[]{auto m=Fixture();Monstey_WorldReceiverRowLimits l;l.rows_per_vector=0;J j=Shot(m,l);Status(j,"UNKNOWN");Need(Has(j,"receiver_row_limit")&&m.hits[First]==0);m=Fixture();m.regions[Receiver+0x5c8]=Words({0,0});m.regions[Receiver+0x5f8]=Words({0,0});Status(Shot(m,l),"ROWS_OBSERVED");});
 test("row_count_limit_precedes_array_read",[]{auto m=Fixture();m.regions[Receiver+0x5c8]=Words({First,First+65537*8});Monstey_WorldReceiverRowLimits l;l.rows_per_vector=65536;const J j=Shot(m,l);Status(j,"UNKNOWN");Need(Has(j,"receiver_row_limit")&&m.hits[First]==0&&j["vectors"][0u]["count"].as_uint()==65537);});
 test("maximum_row_count_is_bounded_and_supported",[]{auto m=Fixture();m.regions[Receiver+0x5c8]=Words({First,First+65536*8});m.regions[First]=B(65536*8);m.regions[Receiver+0x5f8]=Words({0,0});Monstey_WorldReceiverRowLimits l;l.rows_per_vector=65536;const J j=Shot(m,l);Status(j,"ROWS_OBSERVED");Need(j["vectors"][0u]["entries"].size()==65536&&j["read_calls"].as_uint()==3);});
 test("reversed_and_non_stride_ranges_are_unknown",[]{for(auto pair:{std::pair<A,A>{First+8,First},std::pair<A,A>{First,First+9}}){auto m=Fixture();m.regions[Receiver+0x5c8]=Words({pair.first,pair.second});const J j=Shot(m);Status(j,"UNKNOWN");Need(Has(j,"invalid_receiver_row_vector")&&m.hits[First]==0);}});
 test("invalid_receiver_no_reads",[]{for(A a:{A{0},A{1},A{0xffff},MaxUser+1,std::numeric_limits<A>::max()}){auto m=Fixture();Status(Monstey_WorldReceiverRowsSnapshot(m,a,Profile),"UNKNOWN");Need(m.trace.empty());}});
 test("nonempty_null_and_noncanonical_arrays_not_read",[]{for(auto pair:{std::pair<A,A>{0,8},std::pair<A,A>{MaxUser+1,MaxUser+9}}){auto m=Fixture();m.regions[Receiver+0x5c8]=Words({pair.first,pair.second});const J j=Shot(m);Status(j,"UNKNOWN");Need(Has(j,"invalid_user_range")&&m.hits[pair.first]==0);}});
 test("exact_capture_budgets",[]{auto m=Fixture();Monstey_WorldReceiverRowLimits l;l.capture.read_calls=4;l.capture.read_bytes=56;Status(Shot(m,l),"ROWS_OBSERVED");});
 test("short_capture_budgets_preserve_unknown",[]{for(unsigned i=0;i<2;++i){auto m=Fixture();Monstey_WorldReceiverRowLimits l;if(i==0)l.capture.read_bytes=55;else l.capture.read_calls=3;const J j=Shot(m,l);Status(j,"UNKNOWN");Need(Has(j,"capture_budget_exceeded")&&j["read_calls"].as_uint()==3);}});
 test("each_initial_transfer_failure_unknown_without_decoding",[]{for(A target:{Receiver+0x5c8,First,Receiver+0x5f8,Second})for(unsigned variant=0;variant<3;++variant){auto m=Fixture();m.modify=[=](A a,unsigned hit,Monstey_WorldRead&r){if(a==target&&hit==1){if(variant==0){r.complete=false;r.error=299;}else if(variant==1){r.bytes.pop_back();}else{r.bytes.clear();r.complete=false;}}};const J j=Shot(m);Status(j,"UNKNOWN");Need(Has(j,"partial_or_failed_read"));}});
 test("each_verification_failure_or_change_unknown",[]{for(A target:{Receiver+0x5c8,First,Receiver+0x5f8,Second})for(unsigned variant=0;variant<3;++variant){auto m=Fixture();m.modify=[=](A a,unsigned hit,Monstey_WorldRead&r){if(a==target&&hit==2){if(variant==0)r.bytes[0]^=1;else if(variant==1)r.complete=false;else r.bytes.pop_back();}};const J j=Shot(m);Status(j,"UNKNOWN");Need(!j["observed_changes"].empty()&&j["observation_before_capture_validation"]["status"].as_string()=="ROWS_OBSERVED");}});
 test("changed_header_never_retargets_new_array",[]{auto m=Fixture();m.modify=[](A a,unsigned hit,Monstey_WorldRead&r){if(a==Receiver+0x5c8&&hit==2)r.bytes=Words({0x800000,0x800010});};Status(Shot(m),"UNKNOWN");Need(!m.hits.contains(0x800000));});
 test("successful_first_vector_retained_on_second_failure",[]{auto m=Fixture();m.regions.erase(Receiver+0x5f8);const J j=Shot(m);Status(j,"UNKNOWN");Need(j["vectors"].size()==2&&j["vectors"][0u]["entries"].size()==2&&j["vectors"][1u]["entries"].empty());});
 test("canonical_last_array_byte_valid_exclusive_end",[]{auto m=Fixture();m.regions[Receiver+0x5c8]=Words({MaxUser-7,MaxUser+1});m.regions[MaxUser-7]=Words({0x900000});Status(Shot(m),"ROWS_OBSERVED");});
 test("canonical_header_boundary_rejected_before_provider",[]{const A receiver=MaxUser-0x606;auto m=Fixture(receiver);m.regions[receiver+0x5c8]=Words({0,0});const J j=Monstey_WorldReceiverRowsSnapshot(m,receiver,Profile);Status(j,"UNKNOWN");Need(Has(j,"invalid_user_range")&&!m.hits.contains(receiver+0x5f8));});
 test("cache_owned_per_call",[]{auto m=Fixture();Status(Shot(m),"ROWS_OBSERVED");m.regions[First]=Words({0x930000,0x940000});const J j=Shot(m);Status(j,"ROWS_OBSERVED");Need(j["vectors"][0u]["entries"][0u]["row_pointer"].as_string()==Monstey_Hex(0x930000,16));});
 test("row_lists_are_not_direct_gate_or_navigation_results",[]{auto m=Fixture();const J j=Shot(m);Scope(j);Need(!j.contains("call_gate_pass")&&!j.contains("native_model_ready"));});
 const std::size_t expected_groups=source_profile_qualified?std::size_t{29}:std::size_t{28};
 Monstey_Require(groups.size()==expected_groups,"Receiver rows group count mismatch");
 J report=J::object({{"status","PASS"},{"groups",groups},{"count",static_cast<A>(groups.size())},{"source_profile_qualified",source_profile_qualified},{"original_functions_executed",false},{"game_process_opened",false},{"runtime_verified",false}});
 Monstey_WriteNewText(argv[1],report.dump(2)+"\n");std::cout<<"PASS "<<groups.size()<<" receiver row groups\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}