#include "Monstey_WorldRuntimeProbe.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using A = std::uint64_t;
struct Node { A address{}, child{}, sibling{}, parent{}, primary{}; std::uint32_t scene_hash{}; std::uint8_t scene{}, flags{}; B raw; std::vector<A> groups; bool inside{}; };
struct Scene { A address{}, node_pointer{}, loose{}, prefix{}; std::uint32_t count{}, loose_count{}, prefix_count{}, hash{}; };
std::string Hex(A a) { return Monstey_Hex(a, 16); }
std::string RawHex(std::span<const std::uint8_t> b) { constexpr char d[]="0123456789abcdef"; std::string s; s.reserve(b.size()*2); for(auto x:b){s+=d[x>>4];s+=d[x&15];} return s; }
std::optional<A> Add(A a, A b) { if(b>std::numeric_limits<A>::max()-a) return {}; return a+b; }
J FloatWords(std::span<const std::uint8_t> b, std::size_t count) {
    J::Array a; for(std::size_t i=0;i<count;++i) { const auto word=Monstey_U32(b,i*4); const auto f=std::bit_cast<float>(word); a.push_back(J::object({{"word",Monstey_Hex(word)},{"finite",std::isfinite(f)},{"value",std::isfinite(f)?J(static_cast<double>(f)):J()}})); } return a;
}
B CullBounds(std::span<const std::uint8_t> b) { B out(24);std::copy_n(b.begin(),12,out.begin());std::copy_n(b.begin()+16,12,out.begin()+12);return out; }
bool Contains(std::span<const std::uint8_t> b,const std::array<float,3>& p) {
    for(std::size_t i=0;i<3;++i) { const auto lo=std::bit_cast<float>(Monstey_U32(b,i*4)),hi=std::bit_cast<float>(Monstey_U32(b,(i+3)*4)); if(!std::isfinite(lo)||!std::isfinite(hi)||lo>hi||p[i]<lo||p[i]>hi) return false; } return true;
}
class Capture {
    Monstey_WorldMemory& memory_; const Monstey_WorldLimits& limits_;
    std::map<std::pair<A,std::size_t>,B> captured_;
public:
    J::Array diagnostics, changes; std::size_t read_bytes{},read_calls{},stable_bytes{},stable_calls{};
    Capture(Monstey_WorldMemory& m,const Monstey_WorldLimits& l):memory_(m),limits_(l){}
    void Issue(std::string_view kind,A a,std::size_t requested,std::size_t got=0,std::uint32_t error=0) { diagnostics.push_back(J::object({{"kind",kind},{"address",Hex(a)},{"requested_bytes",static_cast<A>(requested)},{"transferred_bytes",static_cast<A>(got)},{"win32_error",error}})); }
    const B* Get(A a,std::size_t n) {
        const auto key=std::make_pair(a,n); if(auto it=captured_.find(key);it!=captured_.end()) return &it->second;
        if(!n) { return &captured_.emplace(key,B{}).first->second; }
        if(!a||a<0x10000||a>0x00007fffffffffffULL||n-1>0x00007fffffffffffULL-a){Issue("invalid_user_range",a,n);return nullptr;}
        if(read_calls>=limits_.read_calls||n>limits_.read_bytes-read_bytes){Issue("capture_budget_exceeded",a,n);return nullptr;}
        ++read_calls;read_bytes+=n; auto r=memory_.Read(a,n);
        if(!r.complete||r.bytes.size()!=n){Issue("partial_or_failed_read",a,n,r.bytes.size(),r.error);return nullptr;}
        return &captured_.emplace(key,std::move(r.bytes)).first->second;
    }
    const B* At(A a,A offset,std::size_t n) { auto p=Add(a,offset); if(!p){Issue("address_overflow",a,n);return nullptr;} return Get(*p,n); }
    void Verify() {
        for(const auto& [key,b]:captured_) {if(!key.second)continue; ++stable_calls;stable_bytes+=key.second;const auto r=memory_.Read(key.first,key.second);if(!r.complete||r.bytes.size()!=key.second||r.bytes!=b)changes.push_back(J::object({{"address",Hex(key.first)},{"bytes",static_cast<A>(key.second)},{"complete",r.complete},{"transferred_bytes",static_cast<A>(r.bytes.size())},{"win32_error",r.error},{"before_sha256",Monstey_HashSha256(b)},{"after_sha256",Monstey_HashSha256(r.bytes)}})); }
    }
    J Regions() const { J::Array a;for(const auto& [k,b]:captured_)a.push_back(J::object({{"address",Hex(k.first)},{"bytes",static_cast<A>(k.second)},{"sha256",Monstey_HashSha256(b)}}));return a; }
};
J ReadContext(Capture& c,A context) {
    J out=J::object({{"address",Hex(context)},{"resolver_14114BBBC_executed",false},{"native_model_ready",J()},{"native_material_ready",J()}});
    if(!context)return out;
    const auto* raw=c.Get(context,56); if(!raw)return out;
    out["raw_56_bytes"]=RawHex(*raw);out["raw_sha256"]=Monstey_HashSha256(*raw);
    const A wrapper=Monstey_U64(*raw,0);out["first_member_wrapper_pointer"]=Hex(wrapper);
    // This is a literal observed qword chain; it is not a resolver/readiness result.
    J::Array chain; A address=wrapper; for(unsigned level=0;level<3&&address;++level) {
        const auto* bytes=c.At(address,level==0?0:8,8);if(!bytes)break;
        const A next=Monstey_U64(*bytes,0);chain.push_back(J::object({{"read_address",Hex(address)},{"offset",level==0?0:8},{"qword",Hex(next)}}));address=next;
    } out["diagnostic_pointer_chain_only"]=chain;return out;
}
}
J Monstey_WorldRuntimeSnapshot(Monstey_WorldMemory& memory,A base,const std::array<float,3>& point,std::uint32_t target,const Monstey_WorldLimits& limits) {
    for(auto x:point)if(!std::isfinite(x))throw Monstey_Error("world_probe_point","Point must be finite");
    if(!limits.scenes||limits.scenes>20000||!limits.nodes||limits.nodes>100000||!limits.groups||limits.groups>65536||!limits.links_per_node||limits.links_per_node>4096||!limits.culls_per_group||limits.culls_per_group>65535||limits.read_bytes<1024||limits.read_bytes>32*1024*1024||!limits.read_calls||limits.read_calls>100000)throw Monstey_Error("world_probe_limits","Invalid bounded limits");
    const auto reg=Add(base,0x2778220);if(!reg)throw Monstey_Error("world_probe_base","Image base overflow");
    Capture cap(memory,limits);std::map<A,Scene> scenes;std::map<A,Node> nodes;J::Array vectors,candidates,groupRows,holderRows;std::map<A,std::set<std::string>> targetGroups;std::size_t nodeTotal=0;
    for(const auto offset:{768U,792U}) {
        J::Array entries;const auto* header=cap.At(*reg,offset,16);J v=J::object({{"offset",offset}});
        if(header) {const A begin=Monstey_U64(*header,0),end=Monstey_U64(*header,8);v["begin"]=Hex(begin);v["end"]=Hex(end);
            if(end<begin||(end-begin)%32||(end-begin)/32>limits.scenes){cap.Issue("invalid_or_capped_scene_vector",begin,16);} else if(const auto* data=cap.Get(begin,static_cast<std::size_t>(end-begin))) {
                for(std::size_t i=0;i<data->size()/32;++i){const auto row=std::span(*data).subspan(i*32,32);const A address=Monstey_U64(row,24);J e=J::object({{"index",static_cast<A>(i)},{"scene",Hex(address)},{"bounds",FloatWords(row,6)},{"contains_point_exact_AABB",Contains(row,point)}});
                    if(address) { if(auto it=scenes.find(address);it!=scenes.end()){e["path_hash"]=Monstey_Hex(it->second.hash);}else if(const auto* s=cap.Get(address,176)) {Scene scene{address,Monstey_U64(*s,8),Monstey_U64(*s,56),Monstey_U64(*s,88),Monstey_U32(*s,16),Monstey_U32(*s,64),Monstey_U32(*s,96),Monstey_U32(*s,168)};scenes.emplace(address,scene);e["path_hash"]=Monstey_Hex(scene.hash);}}
                    entries.push_back(std::move(e));}
            }
        }v["entries"]=entries;vectors.push_back(std::move(v));
    }
    auto AddGroup=[&](A address,const std::string& origin){if(!address)return;if(targetGroups.size()>=limits.groups&&!targetGroups.contains(address)){cap.Issue("target_group_limit",address,72);return;}targetGroups[address].insert(origin);};
    J::Array sceneRows,nodeRows;
    for(const auto& [address,s]:scenes) {
        const bool isTarget=s.hash==target;J row=J::object({{"address",Hex(address)},{"path_hash",Monstey_Hex(s.hash)},{"target",isTarget},{"nodes",Hex(s.node_pointer)},{"node_count",s.count},{"loose_groups",Hex(s.loose)},{"loose_count",s.loose_count},{"prefix_groups",Hex(s.prefix)},{"prefix_count",s.prefix_count}});
        if(const auto* sceneBytes=cap.Get(address,176)){row["raw_176_bytes"]=RawHex(*sceneBytes);row["raw_sha256"]=Monstey_HashSha256(*sceneBytes);}
        if(isTarget)for(unsigned arrayIndex=0;arrayIndex<2;++arrayIndex) {const A pointer=arrayIndex?s.prefix:s.loose;const auto count=arrayIndex?s.prefix_count:s.loose_count;if(count>limits.groups){cap.Issue("target_group_count_limit",pointer,72);continue;}if(count&&!pointer){cap.Issue("null_group_array",0,72);continue;}for(std::uint32_t i=0;i<count;++i){auto p=Add(pointer,static_cast<A>(i)*72);if(!p){cap.Issue("group_address_overflow",pointer,72);break;}AddGroup(*p,arrayIndex?"scene_prefix":"scene_loose");}}
        if(s.count>limits.nodes-nodeTotal){cap.Issue("aggregate_node_limit",s.node_pointer,160);sceneRows.push_back(std::move(row));continue;}
        nodeTotal+=s.count;if(s.count&&!s.node_pointer){cap.Issue("null_node_array",0,160);sceneRows.push_back(std::move(row));continue;}
        for(std::uint32_t i=0;i<s.count;++i) {
            const auto a=Add(s.node_pointer,static_cast<A>(i)*160);if(!a){cap.Issue("node_address_overflow",s.node_pointer,160);break;}const auto* raw=cap.Get(*a,160);if(!raw)continue;
            Node n; n.address=*a;n.raw=*raw;n.scene_hash=s.hash;n.primary=Monstey_U64(*raw,72);n.child=Monstey_U64(*raw,136);n.sibling=Monstey_U64(*raw,144);n.parent=Monstey_U64(*raw,152);n.flags=(*raw)[113];n.scene=(*raw)[114];n.inside=Contains(*raw,point);
            const A begin=Monstey_U64(*raw,80),end=Monstey_U64(*raw,88);
            if(end<begin||(end-begin)%8||(end-begin)/8>limits.links_per_node)cap.Issue("invalid_or_capped_node_groups",begin,8);else if(const auto* gs=cap.Get(begin,static_cast<std::size_t>(end-begin)))for(std::size_t k=0;k<gs->size();k+=8)n.groups.push_back(Monstey_U64(*gs,k));
            const unsigned planeCount=(*raw)[112];if(n.inside&&planeCount) {const auto* planes=cap.Get(Monstey_U64(*raw,120),planeCount*20U);if(!planes)n.inside=false;else for(unsigned k=0;k<planeCount;++k){const auto pl=std::span(*planes).subspan(k*20U,20);float f[4]{};for(unsigned z=0;z<4;++z){f[z]=std::bit_cast<float>(Monstey_U32(pl,z*4));if(!std::isfinite(f[z]))n.inside=false;}if(n.inside&&!(static_cast<double>(f[0])*point[0]+static_cast<double>(f[1])*point[1]+static_cast<double>(f[2])*point[2]-f[3]>=0))n.inside=false;}}
            if(isTarget){AddGroup(n.primary,"target_node_primary");for(auto g:n.groups)AddGroup(g,"target_node_vector");}
            nodes.emplace(n.address,std::move(n));
        }sceneRows.push_back(std::move(row));
    }
    for(const auto& [a,n]:nodes) {
        J::Array observedGroups;for(auto g:n.groups)observedGroups.push_back(Hex(g));nodeRows.push_back(J::object({{"address",Hex(a)},{"scene_hash",Monstey_Hex(n.scene_hash)},{"raw_160_bytes",RawHex(n.raw)},{"raw_sha256",Monstey_HashSha256(n.raw)},{"bounds",FloatWords(n.raw,6)},{"primary_group",Hex(n.primary)},{"group_vector",observedGroups},{"child",Hex(n.child)},{"sibling",Hex(n.sibling)},{"parent",Hex(n.parent)},{"flags",n.flags},{"scene_byte",n.scene},{"diagnostic_cell_contains_point",n.inside}}));
        if(n.inside)candidates.push_back(J::object({{"node",Hex(a)},{"scene_hash",Monstey_Hex(n.scene_hash)},{"scene_byte",n.scene},{"flags",n.flags},{"leaf",!n.child},{"engine_start_selection_verified",false}}));
        J::Array links;if(targetGroups.contains(n.primary))links.push_back(J::object({{"group",Hex(n.primary)},{"via","primary"}}));for(auto g:n.groups)if(targetGroups.contains(g))links.push_back(J::object({{"group",Hex(g)},{"via","group_vector"}}));
        if(!links.empty()) {J ancestry=J::array();std::set<A> seen;A parent=a;bool complete=false;while(parent&&seen.size()<nodes.size()+1){if(!seen.insert(parent).second){cap.Issue("parent_cycle",parent,160);break;}ancestry.as_array().push_back(Hex(parent));const auto it=nodes.find(parent);if(it==nodes.end())break;parent=it->second.parent;if(!parent)complete=true;}holderRows.push_back(J::object({{"node",Hex(a)},{"scene_hash",Monstey_Hex(n.scene_hash)},{"flags",n.flags},{"parent_chain",ancestry},{"parent_chain_complete_in_snapshot",complete},{"links",links}}));}
    }
    for(const auto& [address,origins]:targetGroups) {
        J::Array originRows;for(const auto& o:origins)originRows.push_back(o);J group=J::object({{"address",Hex(address)},{"origins",originRows}});const auto* raw=cap.Get(address,72);if(!raw){groupRows.push_back(group);continue;}
        const auto eligible=Monstey_U16(*raw,2),total=Monstey_U16(*raw,4);const A statics=Monstey_U64(*raw,16);group["raw_72_bytes"]=RawHex(*raw);group["raw_sha256"]=Monstey_HashSha256(*raw);group["eligible_prefix_count"]=eligible;group["total_static_count"]=total;group["static_pointer"]=Hex(statics);group["bounds"]=FloatWords(std::span(*raw).subspan(24),6);J::Array culls;
        if(eligible>total||total>limits.culls_per_group)cap.Issue("invalid_or_capped_static_counts",statics,48);else if(const auto* data=cap.Get(statics,static_cast<std::size_t>(total)*48))for(std::size_t i=0;i<total;++i){const auto record=std::span(*data).subspan(i*48,48);const A context=Monstey_U64(record,32);const B bounds=CullBounds(record);J item=J::object({{"index",static_cast<A>(i)},{"address",Hex(*Add(statics,i*48))},{"raw_48_bytes",RawHex(record)},{"raw_sha256",Monstey_HashSha256(record)},{"bounds",FloatWords(bounds,6)},{"bounds_layout","minXYZ_at_0_maxXYZ_at_0x10_two_padded_rows"},{"bounds_contains_point",Contains(bounds,point)},{"in_eligible_prefix",i<eligible},{"flag_at_0x2B",record[43]},{"flag_nonzero_2BDC38",record[43]!=0},{"draw_U16_at_0x28",Monstey_U16(record,40)},{"context_at_0x20",ReadContext(cap,context)},{"later_resource_material_draw_gates","UNVERIFIED"},{"rendered",J()}});culls.push_back(std::move(item));}
        group["static_culls"]=culls;groupRows.push_back(std::move(group));
    }
    cap.Verify();
    J::Array p;for(auto x:point)p.push_back(static_cast<double>(x));
    return J::object({{"schema","Monstey.WorldRuntimeSnapshot/1"},{"target_hash",Monstey_Hex(target)},{"point",p},{"image_base",Hex(base)},{"registry",Hex(*reg)},{"registry_vectors",vectors},{"scenes",sceneRows},{"decoded_nodes",static_cast<A>(nodes.size())},{"node_records",nodeRows},{"target_groups",groupRows},{"nodes_linking_target",holderRows},{"diagnostic_cell_candidates",candidates},{"engine_camera_start_or_frustum_occlusion_executed",false},{"gather_execution_observed",false},{"rendering_verified",false},{"snapshot_atomic",false},{"repeated_regions_unchanged",cap.changes.empty()},{"complete_within_profile",cap.diagnostics.empty()&&cap.changes.empty()},{"diagnostics",cap.diagnostics},{"observed_changes",cap.changes},{"read_calls",static_cast<A>(cap.read_calls)},{"requested_read_bytes",static_cast<A>(cap.read_bytes)},{"stability_read_calls",static_cast<A>(cap.stable_calls)},{"stability_read_bytes",static_cast<A>(cap.stable_bytes)},{"captured_region_hashes",cap.Regions()},{"process_memory_written",false},{"native_functions_called",false}});
}

namespace {
bool Monstey_HkaiProfile(std::string_view hash) {
    return hash=="6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455" ||
           hash=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef";
}
bool Monstey_HkaiAddress(A address) { return address>=0x10000 && address<=0x00007fffffffffffULL; }
bool Monstey_HkaiRange(A address,A bytes) { return bytes && Monstey_HkaiAddress(address) && bytes-1<=0x00007fffffffffffULL-address; }
}
J Monstey_WorldHkaiActivationGate(const Monstey_HkaiActivationInput &input) {
    J result=J::object({{"schema","Monstey.HkaiSelectedActivationGate/1"},
        {"scope","C2FDC0_selected_row_C30115_call_gate_only"},{"image_sha256",input.image_sha256},
        {"status","UNKNOWN"},{"call_gate_pass",J()},{"queue_request_executed",false},
        {"queue_allocation_succeeded",J()},{"completed_registration",J()},
        {"manager_list_membership_verified",false},{"profile_verified_against_process",false},
        {"complete_sorted_table_verified",false},{"native_sorted_order_precondition_assumed",true},
        {"caller_string_extent_is_read_safety_only",true},{"native_string_capacity_verified",false},
        {"opaque_pre_call_helpers_assumed_to_return_normally",true},
        {"native_functions_called",false},{"runtime_verified",false},{"visited_indices",J::array()}});
    const auto finish=[&](const char *status,const char *reason)->J {
        result["status"]=status;result["reason"]=reason;
        result["call_gate_pass"]=std::string_view(status)=="UNKNOWN" ? J() : J(std::string_view(status)=="GATE_PASS");
        return result;
    };
    if(!Monstey_HkaiProfile(input.image_sha256)) return finish("UNKNOWN","unsupported_code_profile");
    if(!input.row) return finish("UNKNOWN","row_not_observed");
    const auto &row=*input.row;
    const auto rowName=Add(row.address,0x10);
    if(!Monstey_HkaiRange(row.address,0x58)||!rowName||!Monstey_HkaiAddress(*rowName)) return finish("UNKNOWN","invalid_row_address");
    if(row.value_50 && !row.value_49) return finish("BLOCKED","row_pending_without_ready_byte");
    if(row.value_48) return finish("BLOCKED","row_queue_byte_nonzero");
    if(!row.identity_3c) return finish("UNKNOWN","row_identity_not_observed");
    if(!input.resource) return finish("UNKNOWN","resource_not_observed");
    const auto &resource=*input.resource;
    if(!Monstey_HkaiRange(resource.address,0x1c)) return finish("UNKNOWN","invalid_resource_address");
    if(*row.identity_3c!=resource.identity_18) return finish("BLOCKED","resource_identity_mismatch");
    if(!resource.table_78||!resource.count_80) return finish("UNKNOWN","table_header_not_observed");
    if(!Monstey_HkaiRange(resource.address,0x84)) return finish("UNKNOWN","invalid_resource_table_header_range");
    if(!row.key_30) return finish("UNKNOWN","row_key_not_observed");
    bool invalidTableRow=false;
    const auto entry=[&](std::uint32_t index)->const Monstey_HkaiActivationEntry* {
        const auto address=Add(*resource.table_78,static_cast<A>(index)*16);
        if(!address||!Monstey_HkaiRange(*address,16)) {invalidTableRow=true;return nullptr;}
        const auto it=input.entries.find(index);
        if(it==input.entries.end()||!it->second.descriptor||(*it->second.descriptor && !it->second.hash_1c)) {
            result["required_table_index"]=index;return nullptr;
        }
        if(*it->second.descriptor && !Monstey_HkaiRange(*it->second.descriptor,0x20)) return nullptr;
        return &it->second;
    };
    std::uint64_t low=0,high=*resource.count_80;
    while(low<high) {
        const auto mid=static_cast<std::uint32_t>(low+(high-low)/2);
        result["visited_indices"].as_array().push_back(mid);
        const auto *candidate=entry(mid);
        if(!candidate) return finish("UNKNOWN",invalidTableRow?"invalid_table_row_address":"table_entry_not_observed");
        const std::uint32_t hash=*candidate->descriptor ? *candidate->hash_1c : 0;
        if(hash<*row.key_30) low=static_cast<std::uint64_t>(mid)+1;
        else high=mid;
    }
    if(low==*resource.count_80) return finish("BLOCKED","hash_not_found");
    const auto index=static_cast<std::uint32_t>(low);
    const auto *candidate=entry(index);
    if(!candidate) return finish("UNKNOWN",invalidTableRow?"invalid_table_row_address":"table_entry_not_observed");
    const std::uint32_t hash=*candidate->descriptor ? *candidate->hash_1c : 0;
    if(hash!=*row.key_30) return finish("BLOCKED","hash_not_found");
    result["matched_table_index"]=index;
    const auto matchedAddress=Add(*resource.table_78,static_cast<A>(index)*16);
    if(!matchedAddress||!Monstey_HkaiRange(*matchedAddress,16))
        return finish("UNKNOWN","invalid_table_row_address");
    result["matched_table_row_address"]=Hex(*matchedAddress);
    if(*candidate->descriptor && !candidate->index_18) {
        result["required_table_index"]=index;return finish("UNKNOWN","descriptor_index_not_observed");
    }
    const std::int32_t stringIndex=*candidate->descriptor ? *candidate->index_18 : 0;
    result["signed_string_index"]=stringIndex;
    // These are tool input bounds, not a native guard. The native code uses MOVSXD.
    if(stringIndex<0) return finish("UNKNOWN","negative_index_outside_supplied_safety_extent");
    if(!input.string_extent.begin||!input.string_extent.slots) return finish("UNKNOWN","string_safety_extent_not_supplied");
    if(static_cast<std::uint32_t>(stringIndex)>=input.string_extent.slots) return finish("UNKNOWN","index_outside_supplied_safety_extent");
    if(!resource.string_slots) return finish("UNKNOWN","string_pool_not_observed");
    if(*resource.string_slots!=input.string_extent.begin) return finish("UNKNOWN","string_safety_extent_base_mismatch");
    const auto slot=Add(*resource.string_slots,static_cast<A>(static_cast<std::uint32_t>(stringIndex))*8);
    if(!slot||!Monstey_HkaiRange(*slot,8)) return finish("UNKNOWN","invalid_string_slot_address");
    result["string_slot_address"]=Hex(*slot);
    const auto name=input.names.find(static_cast<std::uint32_t>(stringIndex));
    if(name==input.names.end()||!name->second.tagged_pointer) {
        result["required_string_index"]=static_cast<std::uint32_t>(stringIndex);return finish("UNKNOWN","string_slot_not_observed");
    }
    const auto stringAddress=*name->second.tagged_pointer & ~A{1};
    result["tagged_string_pointer"]=Hex(*name->second.tagged_pointer);result["untagged_string_pointer"]=Hex(stringAddress);
    if(!stringAddress) return finish("BLOCKED","resolved_name_is_null");
    if(!Monstey_HkaiAddress(stringAddress)) return finish("UNKNOWN","invalid_resolved_string_address");
    if(!row.name_at_10) return finish("UNKNOWN","row_name_not_observed");
    if(!name->second.bytes) {
        result["required_string_index"]=static_cast<std::uint32_t>(stringIndex);return finish("UNKNOWN","resolved_name_not_observed");
    }
    const auto &a=*row.name_at_10,&b=*name->second.bytes;
    for(std::size_t i=0;i<std::min({a.size(),b.size(),std::size_t{4096}});++i) {
        if(!Monstey_HkaiRange(*rowName,i+1)||!Monstey_HkaiRange(stringAddress,i+1)) return finish("UNKNOWN","name_observation_range_invalid");
        if(a[i]!=b[i]) {result["name_mismatch_offset"]=static_cast<A>(i);return finish("BLOCKED","native_name_bytes_differ");}
        if(!a[i]) {
            const auto sourceFlag=Add(row.address,0x64);
            if(!sourceFlag||!Monstey_HkaiRange(*sourceFlag,1)) return finish("UNKNOWN","invalid_source_flag_address");
            if(!row.value_64) return finish("UNKNOWN","source_flag_not_observed");
            result["name_bytes_compared_including_NUL"]=static_cast<A>(i+1);
            result["queue_arguments"]=J::object({{"manager",J()},{"row",Hex(row.address)},
                {"resource",Hex(resource.address)},{"matched_table_row",Hex(*matchedAddress)},
                {"identity_U32",*row.identity_3c},{"source_flag_U8",*row.value_64}});
            return finish("GATE_PASS","selected_row_call_gate_passes_conditionally");
        }
    }
    return finish("UNKNOWN","name_observation_ends_before_equal_NUL_or_mismatch");
}
J Monstey_WorldHkaiActivationSnapshot(Monstey_WorldMemory &memory,A rowAddress,A resourceAddress,
    std::string_view profile,const Monstey_HkaiStringExtent &extent,const Monstey_HkaiActivationLimits &limits) {
    if(!limits.table_entries||limits.table_entries>1048576||!limits.string_slots||limits.string_slots>1048576||
       !limits.name_bytes||limits.name_bytes>4096||!limits.capture.read_bytes||limits.capture.read_bytes>32*1024*1024||
       !limits.capture.read_calls||limits.capture.read_calls>100000)
        throw Monstey_Error("hkai_activation_limits","Invalid bounded activation limits");
    Capture cap(memory,limits.capture);
    Monstey_HkaiActivationInput input;input.image_sha256=profile;input.string_extent=extent;
    J result;
    for(unsigned step=0;step<80;++step) {
        result=Monstey_WorldHkaiActivationGate(input);
        if(result["status"].as_string()!="UNKNOWN") break;
        const auto reason=result["reason"].as_string();
        if(reason=="row_not_observed") {
            const auto *pending=cap.At(rowAddress,0x50,8);if(!pending)break;
            Monstey_HkaiActivationRow row;row.address=rowAddress;row.value_50=Monstey_U64(*pending,0);
            if(row.value_50) {const auto *ready=cap.At(rowAddress,0x49,1);if(!ready)break;row.value_49=(*ready)[0];}
            if(!row.value_50||row.value_49) {const auto *queued=cap.At(rowAddress,0x48,1);if(!queued)break;row.value_48=(*queued)[0];}
            input.row=std::move(row);continue;
        }
        if(reason=="row_identity_not_observed") {
            const auto *raw=cap.At(rowAddress,0x3c,4);if(!raw)break;input.row->identity_3c=Monstey_U32(*raw,0);continue;
        }
        if(reason=="resource_not_observed") {
            const auto *raw=cap.At(resourceAddress,0x18,4);if(!raw)break;
            Monstey_HkaiActivationResource resource;resource.address=resourceAddress;resource.identity_18=Monstey_U32(*raw,0);input.resource=resource;continue;
        }
        if(reason=="table_header_not_observed") {
            const auto *raw=cap.At(resourceAddress,0x78,12);if(!raw)break;
            input.resource->table_78=Monstey_U64(*raw,0);input.resource->count_80=Monstey_U32(*raw,8);continue;
        }
        if(reason=="row_key_not_observed") {
            const auto *raw=cap.At(rowAddress,0x30,4);if(!raw)break;input.row->key_30=Monstey_U32(*raw,0);continue;
        }
        if(result.contains("required_table_index")) {
            if(!input.resource||!input.resource->count_80||*input.resource->count_80>limits.table_entries) {cap.Issue("table_entry_limit",resourceAddress,limits.table_entries);break;}
            const auto index=static_cast<std::uint32_t>(result["required_table_index"].as_uint());
            if(reason=="descriptor_index_not_observed") {
                auto &entry=input.entries.at(index);const auto *raw=cap.At(*entry.descriptor,0x18,4);if(!raw)break;
                entry.index_18=std::bit_cast<std::int32_t>(Monstey_U32(*raw,0));continue;
            }
            const auto *pointer=cap.At(*input.resource->table_78,static_cast<A>(index)*16+8,8);if(!pointer)break;
            Monstey_HkaiActivationEntry entry;entry.descriptor=Monstey_U64(*pointer,0);
            if(*entry.descriptor) {const auto *raw=cap.At(*entry.descriptor,0x1c,4);if(!raw)break;entry.hash_1c=Monstey_U32(*raw,0);}
            input.entries[index]=std::move(entry);continue;
        }
        if(reason=="string_pool_not_observed") {
            if(extent.slots>limits.string_slots) {cap.Issue("string_slot_limit",extent.begin,limits.string_slots);break;}
            const auto *container=cap.At(resourceAddress,0x40,8);if(!container)break;
            const auto *raw=cap.At(Monstey_U64(*container,0),0x20,8);if(!raw)break;
            input.resource->string_slots=Monstey_U64(*raw,0);continue;
        }
        if(reason=="string_slot_not_observed") {
            const auto index=static_cast<std::uint32_t>(result["required_string_index"].as_uint());
            const auto *raw=cap.At(*input.resource->string_slots,static_cast<A>(index)*8,8);if(!raw)break;
            input.names[index].tagged_pointer=Monstey_U64(*raw,0);continue;
        }
        if(reason=="row_name_not_observed"||reason=="resolved_name_not_observed") {
            const auto index=static_cast<std::uint32_t>(result["signed_string_index"].as_int());
            const auto address=*input.names[index].tagged_pointer & ~A{1};
            B lhs,rhs;bool done=false,failed=false;
            for(std::uint32_t offset=0;offset<limits.name_bytes;++offset) {
                const auto first=Add(rowAddress,0x10);if(!first){cap.Issue("address_overflow",rowAddress,1);failed=true;break;}
                const auto *a=cap.At(*first,offset,1);if(!a){failed=true;break;}
                const auto *b=cap.At(address,offset,1);if(!b){failed=true;break;}
                lhs.push_back((*a)[0]);rhs.push_back((*b)[0]);
                if((*a)[0]!=(*b)[0]||!(*a)[0]) {done=true;break;}
            }
            input.row->name_at_10=std::move(lhs);input.names[index].bytes=std::move(rhs);
            if(!done&&!failed)cap.Issue("name_byte_limit",address,limits.name_bytes);
            if(failed||!done)break;continue;
        }
        if(reason=="source_flag_not_observed") {
            const auto *raw=cap.At(rowAddress,0x64,1);if(!raw)break;input.row->value_64=(*raw)[0];continue;
        }
        break;
    }
    result=Monstey_WorldHkaiActivationGate(input);cap.Verify();
    if(!cap.diagnostics.empty()||!cap.changes.empty()) {
        const auto before=result;result["observed_gate_before_capture_validation"]=before;
        result["status"]="UNKNOWN";result["reason"]="capture_incomplete_limited_or_changed";result["call_gate_pass"]=J();
    }
    result["snapshot_atomic"]=false;result["repeated_regions_unchanged"]=cap.changes.empty();
    result["diagnostics"]=cap.diagnostics;result["observed_changes"]=cap.changes;
    result["read_calls"]=static_cast<A>(cap.read_calls);result["requested_read_bytes"]=static_cast<A>(cap.read_bytes);
    result["stability_read_calls"]=static_cast<A>(cap.stable_calls);result["captured_region_hashes"]=cap.Regions();
    result["table_entries_observed"]=static_cast<A>(input.entries.size());
    result["row_address"]=Hex(rowAddress);result["resource_address"]=Hex(resourceAddress);
    result["caller_string_safety_extent"]=J::object({{"begin",Hex(extent.begin)},{"slots",extent.slots}});
    return result;
}
J Monstey_WorldPublishedReceiverSnapshot(Monstey_WorldMemory &memory,A base,
    std::string_view profile,const Monstey_WorldLimits &limits) {
    Capture cap(memory,limits);
    J result=J::object({{"schema","Monstey.WorldPublishedReceiverSnapshot/1"},
        {"scope","published_root_AE8_receiver_vptr_and_C8_D0_literals_only"},
        {"image_base",Hex(base)},{"image_sha256",profile},{"status","UNKNOWN"},
        {"reason","not_observed"},{"receiver_profile_match",J()},
        {"root_global_address",J()},{"root_address",J()},{"receiver_slot_address",J()},
        {"receiver_address",J()},{"receiver_vptr",J()},{"C8_slot_value",J()},{"D0_slot_value",J()},
        {"expected_vptr",J()},{"expected_C8",J()},{"expected_D0",J()},
        {"profile_verified_against_process",false},{"snapshot_atomic",false},
        {"receiver_lifetime_retained",false},{"concurrent_lifetime_proved",false},
        {"initial_activation_verified",false},{"registration_verified",false},
        {"row_or_resource_association_verified",false},{"native_functions_called",false},
        {"process_memory_written",false}});
    const auto finish=[&](const char *reason,bool observed=false)->J {
        result["reason"]=reason;
        result["status"]=observed?"RECEIVER_OBSERVED":"UNKNOWN";
        result["receiver_profile_match"]=observed?J(true):J();
        cap.Verify();
        if(!cap.diagnostics.empty()||!cap.changes.empty()) {
            const auto before=result;result["observation_before_capture_validation"]=before;
            result["status"]="UNKNOWN";result["reason"]="capture_incomplete_limited_or_changed";
            result["receiver_profile_match"]=J();
        }
        result["repeated_regions_unchanged"]=cap.changes.empty();
        result["diagnostics"]=cap.diagnostics;result["observed_changes"]=cap.changes;
        result["read_calls"]=static_cast<A>(cap.read_calls);
        result["requested_read_bytes"]=static_cast<A>(cap.read_bytes);
        result["stability_read_calls"]=static_cast<A>(cap.stable_calls);
        result["stability_read_bytes"]=static_cast<A>(cap.stable_bytes);
        result["captured_region_hashes"]=cap.Regions();
        return result;
    };
    if(!Monstey_HkaiProfile(profile))return finish("unsupported_code_profile");
    if(!limits.read_bytes||limits.read_bytes>32*1024*1024||!limits.read_calls||limits.read_calls>100000)
        throw Monstey_Error("world_receiver_limits","Invalid bounded receiver capture limits");
    const auto global=Add(base,0x26ff050),vtable=Add(base,0x1e6c2c0),
        expectedC8=Add(base,0x483b44),expectedD0=Add(base,0xc30a90);
    if(!Monstey_HkaiAddress(base)||!global||!vtable||!expectedC8||!expectedD0||
       !Monstey_HkaiRange(*global,8)||!Monstey_HkaiRange(*vtable,0xd8)||
       !Monstey_HkaiAddress(*expectedC8)||!Monstey_HkaiAddress(*expectedD0))
        return finish("invalid_or_overflowing_image_range");
    result["root_global_address"]=Hex(*global);result["expected_vptr"]=Hex(*vtable);
    result["expected_C8"]=Hex(*expectedC8);result["expected_D0"]=Hex(*expectedD0);
    const auto *rootWord=cap.Get(*global,8);if(!rootWord)return finish("root_pointer_not_observed");
    const A root=Monstey_U64(*rootWord,0);result["root_address"]=Hex(root);
    if(!root)return finish("root_pointer_null");
    if(!Monstey_HkaiAddress(root))return finish("invalid_root_address");
    const auto slot=Add(root,0xae8);
    if(!slot)return finish("receiver_slot_address_overflow");
    result["receiver_slot_address"]=Hex(*slot);
    const auto *receiverWord=cap.Get(*slot,8);if(!receiverWord)return finish("receiver_pointer_not_observed");
    const A receiver=Monstey_U64(*receiverWord,0);result["receiver_address"]=Hex(receiver);
    if(!receiver)return finish("receiver_pointer_null");
    const auto *vptrWord=cap.Get(receiver,8);if(!vptrWord)return finish("receiver_vptr_not_observed");
    const A vptr=Monstey_U64(*vptrWord,0);result["receiver_vptr"]=Hex(vptr);
    if(vptr!=*vtable)return finish("receiver_vptr_mismatch");
    const auto *slots=cap.At(vptr,0xc8,16);if(!slots)return finish("receiver_vtable_slots_not_observed");
    const A c8=Monstey_U64(*slots,0),d0=Monstey_U64(*slots,8);
    result["C8_slot_value"]=Hex(c8);result["D0_slot_value"]=Hex(d0);
    if(c8!=*expectedC8||d0!=*expectedD0)return finish("receiver_vtable_slot_mismatch");
    return finish("published_receiver_profile_observed",true);
}
J Monstey_WorldReceiverRowsSnapshot(Monstey_WorldMemory &memory,A receiver,
    std::string_view profile,const Monstey_WorldReceiverRowLimits &limits) {
    Capture cap(memory,limits.capture);
    J result=J::object({{"schema","Monstey.WorldReceiverRowsSnapshot/1"},
        {"scope","C2FDC0_receiver_5C8_5D0_and_5F8_600_pointer_arrays_only"},
        {"receiver_address",Hex(receiver)},{"image_sha256",profile},
        {"status","UNKNOWN"},{"reason","not_observed"},{"vectors",J::array()},
        {"row_pointer_values_only",true},{"row_pointers_dereferenced",false},
        {"row_pointer_validity_verified",false},{"receiver_profile_authenticated",false},
        {"profile_verified_against_process",false},{"snapshot_atomic",false},
        {"receiver_lifetime_retained",false},{"concurrent_lifetime_proved",false},
        {"native_preceding_mutations_executed",false},{"call_gates_evaluated",false},
        {"initial_activation_verified",false},{"registration_verified",false},
        {"typed_navigation_association_verified",false},{"native_functions_called",false},
        {"process_memory_written",false},{"runtime_verified",false}});
    const auto finish=[&](const char *reason,bool observed=false)->J {
        result["reason"]=reason;result["status"]=observed?"ROWS_OBSERVED":"UNKNOWN";
        cap.Verify();
        if(!cap.diagnostics.empty()||!cap.changes.empty()) {
            const auto before=result;result["observation_before_capture_validation"]=before;
            result["status"]="UNKNOWN";result["reason"]="capture_incomplete_limited_or_changed";
        }
        result["repeated_regions_unchanged"]=cap.changes.empty();
        result["diagnostics"]=cap.diagnostics;result["observed_changes"]=cap.changes;
        result["read_calls"]=static_cast<A>(cap.read_calls);
        result["requested_read_bytes"]=static_cast<A>(cap.read_bytes);
        result["stability_read_calls"]=static_cast<A>(cap.stable_calls);
        result["stability_read_bytes"]=static_cast<A>(cap.stable_bytes);
        result["captured_region_hashes"]=cap.Regions();return result;
    };
    if(!Monstey_HkaiProfile(profile))return finish("unsupported_code_profile");
    if(limits.rows_per_vector>65536||!limits.capture.read_bytes||
       limits.capture.read_bytes>32*1024*1024||!limits.capture.read_calls||
       limits.capture.read_calls>100000)
        throw Monstey_Error("world_receiver_rows_limits","Invalid bounded receiver row limits");
    if(!Monstey_HkaiAddress(receiver))return finish("invalid_receiver_address");
    constexpr std::array<A,2> offsets{0x5c8,0x5f8};
    for(std::size_t vectorIndex=0;vectorIndex<offsets.size();++vectorIndex) {
        const A offset=offsets[vectorIndex];
        const auto headerAddress=Add(receiver,offset);
        if(!headerAddress)return finish("row_header_address_overflow");
        J vector=J::object({{"native_order",static_cast<A>(vectorIndex)},
            {"begin_offset",Monstey_Hex(offset)},{"end_offset",Monstey_Hex(offset+8)},
            {"header_address",Hex(*headerAddress)},{"pointer_stride_bytes",8},
            {"entries",J::array()},{"status","UNKNOWN"}});
        const auto *header=cap.Get(*headerAddress,16);
        if(!header){result["vectors"].as_array().push_back(std::move(vector));return finish("row_header_not_observed");}
        const A begin=Monstey_U64(*header,0),end=Monstey_U64(*header,8);
        vector["begin"]=Hex(begin);vector["end"]=Hex(end);
        if(end<begin||(end-begin)%8) {
            cap.Issue("invalid_receiver_row_vector",*headerAddress,16);
            result["vectors"].as_array().push_back(std::move(vector));return finish("invalid_receiver_row_vector");
        }
        const A count=(end-begin)/8;vector["count"]=count;
        if(count>limits.rows_per_vector) {
            vector["configured_row_limit"]=limits.rows_per_vector;cap.Issue("receiver_row_limit",*headerAddress,16);
            result["vectors"].as_array().push_back(std::move(vector));return finish("receiver_row_limit");
        }
        if(count) {
            const auto *entries=cap.Get(begin,static_cast<std::size_t>(count)*8);
            if(!entries){result["vectors"].as_array().push_back(std::move(vector));return finish("row_pointer_array_not_observed");}
            for(std::size_t index=0;index<static_cast<std::size_t>(count);++index)
                vector["entries"].as_array().push_back(J::object({
                    {"index",static_cast<A>(index)},{"slot_address",Hex(begin+static_cast<A>(index)*8)},
                    {"row_pointer",Hex(Monstey_U64(*entries,index*8))}}));
        }
        vector["status"]="POINTERS_OBSERVED";
        result["vectors"].as_array().push_back(std::move(vector));
    }
    return finish("both_native_row_pointer_arrays_observed",true);
}
}
