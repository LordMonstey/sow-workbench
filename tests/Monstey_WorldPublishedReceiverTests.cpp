#include "Monstey_WorldRuntimeProbe.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <utility>
using namespace Monstey;
namespace {
using A=std::uint64_t;using B=Monstey_Bytes;using J=Monstey_Json;
constexpr A Base=0x140000000,Root=0x500000,Receiver=0x600000,MaxUser=0x00007fffffffffffULL;
const std::string Profile="6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455";
const std::string Installed="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef";
void Check(bool c){Monstey_Require(c,"Published receiver independent assertion failed");}
void Status(const J &j,const char *s){Monstey_Require(j["status"].as_string()==s,j.dump());}
bool Has(const J &j,std::string_view kind){for(const auto &d:j["diagnostics"].as_array())if(d["kind"].as_string()==kind)return true;return false;}
B Word(A v){B b(8);Monstey_PutU64(b,0,v);return b;}
class Monstey_MockMemory final:public Monstey_WorldMemory {
public:
    std::map<A,B> regions;std::map<A,unsigned> hits;
    std::vector<std::pair<A,std::size_t>> trace;
    std::function<void(A,unsigned,Monstey_WorldRead&)> modify;
    Monstey_WorldRead Read(A a,std::size_t n) override {
        trace.emplace_back(a,n);const unsigned hit=++hits[a];
        auto it=regions.upper_bound(a);Monstey_WorldRead out{{},299,false};
        if(it!=regions.begin()) {--it;const A offset=a-it->first;
            if(offset<=it->second.size()&&n<=it->second.size()-static_cast<std::size_t>(offset))
                out={B(it->second.begin()+static_cast<std::ptrdiff_t>(offset),it->second.begin()+static_cast<std::ptrdiff_t>(offset+n)),0,true};}
        if(modify)modify(a,hit,out);return out;
    }
};
Monstey_MockMemory Fixture(A base=Base,A root=Root,A receiver=Receiver){
    Monstey_MockMemory m;m.regions[base+0x26ff050]=Word(root);
    m.regions[root+0xae8]=Word(receiver);m.regions[receiver]=Word(base+0x1e6c2c0);
    B slots(16);Monstey_PutU64(slots,0,base+0x483b44);Monstey_PutU64(slots,8,base+0xc30a90);
    m.regions[base+0x1e6c2c0+0xc8]=std::move(slots);return m;
}
J Shot(Monstey_MockMemory &m,const Monstey_WorldLimits &l={}){return Monstey_WorldPublishedReceiverSnapshot(m,Base,Profile,l);}
std::array<A,4> Reads(A base=Base,A root=Root,A receiver=Receiver){return {base+0x26ff050,root+0xae8,receiver,base+0x1e6c2c0+0xc8};}
void Conditional(const J &j){
    Check(!j["profile_verified_against_process"].as_bool()&&!j["snapshot_atomic"].as_bool());
    Check(!j["receiver_lifetime_retained"].as_bool()&&!j["concurrent_lifetime_proved"].as_bool());
    Check(!j["initial_activation_verified"].as_bool()&&!j["registration_verified"].as_bool());
    Check(!j["row_or_resource_association_verified"].as_bool()&&!j["native_functions_called"].as_bool()&&!j["process_memory_written"].as_bool());
}
B HexBytes(const std::string &s){B b;Check(s.size()%2==0);for(std::size_t i=0;i<s.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return b;}
}
int wmain(int argc,wchar_t **argv){try{
    Monstey_Require(argc==2||argc==3,"Usage: PublishedReceiverTests NEW-REPORT [paired-native-proof.json]");
    const bool source_profile_qualified=argc==3;
    if(source_profile_qualified)
        Monstey_RequireOfflineOutput(argv[1],std::array<std::filesystem::path,1>{argv[2]});
    else
        Monstey_RequireOfflineOutput(argv[1]);
    J::Array groups;auto test=[&](const char *n,const std::function<void()> &f){try{f();}catch(const std::exception &e){throw Monstey_Error("receiver_test",std::string(n)+": "+e.what());}groups.push_back(n);};
    if(source_profile_qualified){
    test("paired_native_profile_literals_and_chain_sites",[&]{
        Check(Monstey_HashSha256(std::filesystem::path(argv[2]))=="c1bc590b8416f0a1a5cd43672e5fbfd41ca73e1c36f41fba690935c57a424e6c");
        const auto p=J::Parse(Monstey_ReadText(argv[2]));B slots;
        for(const auto &r:p["ranges"].as_array())if(r["ea"].as_string()=="0x141e6c388")slots=HexBytes(r["hex"].as_string());
        Check(slots.size()==16&&Monstey_U64(slots,0)==Base+0x483b44&&Monstey_U64(slots,8)==Base+0xc30a90);
        const auto code=[&](A address,std::string_view expected){for(const auto &r:p["ranges"].as_array()){
            const A start=std::stoull(r["ea"].as_string(),nullptr,16);const B b=HexBytes(r["hex"].as_string());
            if(address>=start&&address-start<b.size()){const B x=HexBytes(std::string(expected));const auto off=static_cast<std::size_t>(address-start);return x.size()<=b.size()-off&&std::equal(x.begin(),x.end(),b.begin()+static_cast<std::ptrdiff_t>(off));}}
            return false;};
        Check(code(0x14117cf55,"488b3df4205801")&&code(0x14117d252,"488d9fe80a0000")&&code(0x14117d264,"488b0b"));
        Check(code(0x14118c4be,"488d0dfbfdcd00")&&code(0x14118c49f,"48895118"));
        auto m=Fixture();const auto j=Shot(m);Check(j["expected_C8"].as_string()==Monstey_Hex(Monstey_U64(slots,0),16)&&j["expected_D0"].as_string()==Monstey_Hex(Monstey_U64(slots,8),16));
    });
    }
    test("both_exact_profiles_are_conditional_observations",[]{for(const auto &profile:{Profile,Installed}){auto m=Fixture();const auto j=Monstey_WorldPublishedReceiverSnapshot(m,Base,profile);Status(j,"RECEIVER_OBSERVED");Check(j["receiver_profile_match"].as_bool());Conditional(j);}});
    test("relocated_image_base_not_fixed_virtual_addresses",[]{constexpr A relocated=0x180000000;auto m=Fixture(relocated);const auto j=Monstey_WorldPublishedReceiverSnapshot(m,relocated,Profile);Status(j,"RECEIVER_OBSERVED");Check(j["expected_vptr"].as_string()==Monstey_Hex(relocated+0x1e6c2c0,16));Conditional(j);});
    test("unsupported_profiles_never_read",[]{for(const auto &profile:{std::string{},std::string(64,'0'),std::string("6B8DB778135C082C934463B6524E6B220478FB6987F082AB37CA48A870ACB455"),std::string("unknown")}){Monstey_MockMemory m;Monstey_WorldLimits l;l.read_calls=0;const auto j=Monstey_WorldPublishedReceiverSnapshot(m,UINT64_MAX,profile,l);Status(j,"UNKNOWN");Check(m.trace.empty()&&j["reason"].as_string()=="unsupported_code_profile");}});
    test("four_exact_regions_no_object_or_resource_scan",[]{auto m=Fixture();const auto j=Shot(m);Status(j,"RECEIVER_OBSERVED");const auto a=Reads();Check(m.trace.size()==8);for(std::size_t i=0;i<a.size();++i)Check(m.trace[i]==std::make_pair(a[i],i==3?std::size_t{16}:std::size_t{8}));Check(j["read_calls"].as_uint()==4&&j["requested_read_bytes"].as_uint()==40&&j["stability_read_calls"].as_uint()==4&&j["stability_read_bytes"].as_uint()==40&&j["captured_region_hashes"].size()==4);});
    test("null_root_unknown_without_following_slot",[]{auto m=Fixture();m.regions[Base+0x26ff050]=Word(0);const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["reason"].as_string()=="root_pointer_null"&&m.trace.size()==2&&j["receiver_address"].is_null());});
    test("null_receiver_unknown_without_vptr_read",[]{auto m=Fixture();m.regions[Root+0xae8]=Word(0);const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["reason"].as_string()=="receiver_pointer_null"&&m.trace.size()==4&&m.hits[0]==0);});
    test("wrong_vptr_unknown_and_no_speculative_vtable_reads",[]{auto m=Fixture();m.regions[Receiver]=Word(Base+0x1e6c2c0+8);const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["reason"].as_string()=="receiver_vptr_mismatch"&&m.trace.size()==6&&m.hits[Base+0x1e6c2c0+0xc8]==0);});
    test("both_slot_words_must_match",[]{for(unsigned which=0;which<3;++which){auto m=Fixture();auto &s=m.regions[Base+0x1e6c2c0+0xc8];if(which!=1)Monstey_PutU64(s,0,Base+0x483b45);if(which!=0)Monstey_PutU64(s,8,Base+0xc30a91);const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["reason"].as_string()=="receiver_vtable_slot_mismatch"&&j["receiver_profile_match"].is_null());}});
    test("invalid_root_values_do_not_create_derived_reads",[]{for(A a:{A{1},A{0xffff},A{0x800000000000},UINT64_MAX}){auto m=Fixture();m.regions[Base+0x26ff050]=Word(a);const auto j=Shot(m);Status(j,"UNKNOWN");Check(m.trace.size()==2&&j["reason"].as_string()=="invalid_root_address");}});
    test("root_slot_crossing_canonical_end_rejects",[]{auto m=Fixture();m.regions[Base+0x26ff050]=Word(MaxUser-0xae8-6);const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"invalid_user_range")&&m.trace.size()==2);});
    test("invalid_receiver_ranges_not_read",[]{for(A a:{A{1},A{0xffff},MaxUser-6,A{0x800000000000},UINT64_MAX}){auto m=Fixture();m.regions[Root+0xae8]=Word(a);const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"invalid_user_range")&&m.trace.size()==4);}});
    test("image_base_overflow_and_canonical_ranges_no_reads",[]{for(A a:{A{0},A{0xffff},MaxUser-0x26ff050-6,A{0x800000000000},UINT64_MAX}){Monstey_MockMemory m;const auto j=Monstey_WorldPublishedReceiverSnapshot(m,a,Profile);Status(j,"UNKNOWN");Check(m.trace.empty()&&j["reason"].as_string()=="invalid_or_overflowing_image_range");}});
    test("each_initial_partial_transfer_is_unknown",[]{for(A a:Reads()){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit==1){r.bytes.resize(r.bytes.size()-1);r.complete=false;r.error=299;}};const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"partial_or_failed_read")&&j["receiver_profile_match"].is_null()&&m.hits[a]==1);}});
    test("complete_flag_false_with_full_bytes_is_unknown",[]{for(A a:Reads()){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit==1){r.complete=false;r.error=5;}};const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"partial_or_failed_read"));}});
    test("oversized_transfer_is_not_silently_truncated",[]{for(A a:Reads()){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit==1)r.bytes.push_back(0);};const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"partial_or_failed_read"));}});
    test("each_stability_partial_invalidates_initial_match",[]{for(A a:Reads()){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit>1){r.bytes.pop_back();r.complete=false;r.error=299;}};const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["observation_before_capture_validation"]["status"].as_string()=="RECEIVER_OBSERVED"&&!j["repeated_regions_unchanged"].as_bool()&&j["observed_changes"].size()==1);}});
    test("each_stability_change_invalidates_initial_match",[]{for(A a:Reads()){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit>1)r.bytes[0]^=1;};const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["observed_changes"].size()==1&&j["receiver_profile_match"].is_null());}});
    test("null_transition_in_global_or_slot_is_unknown",[]{for(A a:{Base+0x26ff050,Root+0xae8}){auto m=Fixture();m.modify=[a](A read,unsigned hit,Monstey_WorldRead &r){if(read==a&&hit>1)r.bytes=Word(0);};const auto j=Shot(m);Status(j,"UNKNOWN");Check(!j["repeated_regions_unchanged"].as_bool());}});
    test("verification_never_retargets_changed_global",[]{constexpr A otherRoot=0x800000,otherReceiver=0x900000;auto m=Fixture();m.regions[otherRoot+0xae8]=Word(otherReceiver);m.regions[otherReceiver]=Word(Base+0x1e6c2c0);m.modify=[](A a,unsigned hit,Monstey_WorldRead &r){if(a==Base+0x26ff050&&hit>1)r.bytes=Word(otherRoot);};const auto j=Shot(m);Status(j,"UNKNOWN");Check(m.hits[otherRoot+0xae8]==0&&m.hits[otherReceiver]==0&&j["root_address"].as_string()==Monstey_Hex(Root,16));});
    test("root_receiver_alias_has_no_guessed_class_rejection",[]{auto m=Fixture(Base,Root,Root);const auto j=Shot(m);Status(j,"RECEIVER_OBSERVED");Check(j["root_address"]==j["receiver_address"]);});
    test("exact_global_slot_alias_is_capture_cached",[]{constexpr A same=Base+0x26ff050-0xae8;auto m=Fixture(Base,same,same);const auto j=Shot(m);Status(j,"RECEIVER_OBSERVED");Check(j["read_calls"].as_uint()==3&&j["requested_read_bytes"].as_uint()==32&&m.hits[Base+0x26ff050]==2);});
    test("read_byte_budget_boundary_is_exact",[]{auto m=Fixture();Monstey_WorldLimits l;l.read_bytes=39;const auto j=Shot(m,l);Status(j,"UNKNOWN");Check(Has(j,"capture_budget_exceeded")&&j["read_calls"].as_uint()==3&&m.trace.size()==6);auto n=Fixture();l.read_bytes=40;Status(Shot(n,l),"RECEIVER_OBSERVED");});
    test("read_call_budget_boundary_is_exact",[]{auto m=Fixture();Monstey_WorldLimits l;l.read_calls=3;const auto j=Shot(m,l);Status(j,"UNKNOWN");Check(Has(j,"capture_budget_exceeded")&&m.trace.size()==6);auto n=Fixture();l.read_calls=4;Status(Shot(n,l),"RECEIVER_OBSERVED");});
    test("invalid_limits_throw_before_any_reads",[]{for(unsigned which=0;which<4;++which){auto m=Fixture();Monstey_WorldLimits l;if(which==0)l.read_calls=0;if(which==1)l.read_calls=100001;if(which==2)l.read_bytes=0;if(which==3)l.read_bytes=32*1024*1024+1;bool caught=false;try{(void)Shot(m,l);}catch(const Monstey_Error &e){caught=e.code=="world_receiver_limits";}Check(caught&&m.trace.empty());}});
    test("unused_world_limits_do_not_control_selected_chain",[]{auto m=Fixture();Monstey_WorldLimits l;l.scenes=0;l.nodes=0;l.groups=0;l.links_per_node=0;l.culls_per_group=0;Status(Shot(m,l),"RECEIVER_OBSERVED");});
    test("source_owned_regions_are_not_written",[]{auto m=Fixture();const auto before=m.regions;const auto j=Shot(m);Status(j,"RECEIVER_OBSERVED");Check(m.regions==before);Conditional(j);});
    test("failed_read_error_and_no_stability_region",[]{Monstey_MockMemory m;const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"partial_or_failed_read")&&j["diagnostics"].at(0)["win32_error"].as_uint()==299&&j["stability_read_calls"].as_uint()==0&&m.trace.size()==1);});
    test("last_complete_user_qword_boundaries",[]{auto m=Fixture(Base,Root,MaxUser-7);Status(Shot(m),"RECEIVER_OBSERVED");auto n=Fixture(Base,MaxUser-0xae8-7,Receiver);Status(Shot(n),"RECEIVER_OBSERVED");constexpr A lastBase=MaxUser-0x26ff050-7;auto x=Fixture(lastBase);Status(Monstey_WorldPublishedReceiverSnapshot(x,lastBase,Profile),"RECEIVER_OBSERVED");});
    test("mismatch_still_rechecks_observed_source_regions",[]{auto m=Fixture();m.regions[Receiver]=Word(0);m.modify=[](A a,unsigned hit,Monstey_WorldRead&r){if(a==Root+0xae8&&hit>1)r.bytes=Word(Receiver+0x1000);};const auto j=Shot(m);Status(j,"UNKNOWN");Check(j["observation_before_capture_validation"]["reason"].as_string()=="receiver_vptr_mismatch"&&!j["repeated_regions_unchanged"].as_bool());});
    test("partial_later_region_preserves_earlier_change_diagnostic",[]{auto m=Fixture();m.modify=[](A a,unsigned hit,Monstey_WorldRead&r){if(a==Receiver&&hit==1){r.bytes.pop_back();r.complete=false;}if(a==Base+0x26ff050&&hit>1)r.bytes[0]^=1;};const auto j=Shot(m);Status(j,"UNKNOWN");Check(Has(j,"partial_or_failed_read")&&j["observed_changes"].size()==1&&j["captured_region_hashes"].size()==2);});
    const std::size_t expected_groups=source_profile_qualified?std::size_t{31}:std::size_t{30};
    Monstey_Require(groups.size()==expected_groups,"Published receiver group count mismatch");
    Monstey_WriteNewText(argv[1],J::object({{"schema","Monstey.WorldPublishedReceiverTests/1"},{"status","PASS"},{"groups",groups},{"count",static_cast<A>(groups.size())},{"source_profile_qualified",source_profile_qualified},{"original_calls",false},{"runtime_verified",false}}).dump(2)+"\n");
    std::cout<<"PASS "<<groups.size()<<" published receiver groups\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}