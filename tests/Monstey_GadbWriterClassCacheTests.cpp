#include "Monstey_Gadb.hpp"
#include "Monstey_GadbClassCache.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#define wmain Monstey_NameIdentityOriginalEntry
#include "Monstey_GadbNameIdentityTests.cpp"
#undef wmain
using namespace Monstey;
namespace {
using J = Monstey_Json;
const std::string Monstey_ProofSha(64, 'a');
void Monstey_Need(bool b, const char *what) { Monstey_Require(b, what); }
J Monstey_NativeField(std::string name, unsigned ordinal, std::int64_t count = 1) {
    char code[17]{};
    std::snprintf(code, sizeof(code), "b0%02xc3cccccccccc", ordinal);
    return J::object({{"name", name}, {"hash", Monstey_Hex(Monstey_NativeHash(name))},
        {"type_code", 3}, {"array", count}, {"flags", 0}, {"native_ordinal", ordinal},
        {"native_ordinal_source", "native_index_callback"}, {"native_index_callback_ea", "0x140010000"},
        {"native_index_callback_hex", code}, {"native_descriptor_ea", "0x142000000"},
        {"native_descriptor_sha256", Monstey_ProofSha}});
}
J Monstey_Reflection(std::string name = "Native", J fields = J::array({Monstey_NativeField("A",0),Monstey_NativeField("B",1)})) {
    return J::object({{"source_sha256", Monstey_ProofSha}, {"entries", J::array({J::object({
        {"name",name},{"entry_ea","0x142100000"},{"entry_sha256",Monstey_ProofSha},
        {"fields_ea","0x142000000"},{"count",fields.size()},{"flags",0},{"parent_ea","0x0"}})})},
        {"categories", J::object({{name,J::object({{"name",name},{"flags",0},{"field_count",fields.size()},
            {"unreadable_fields",0},{"fields",fields}})}})}});
}
struct Monstey_WriterFixture {
    std::unique_ptr<Monstey_Gadb> db;
    std::array<std::string,3> refs;
    std::array<std::string,3> names;
};
Monstey_WriterFixture Monstey_Create(bool variable = false, bool third = false) {
    Monstey_WriterFixture f; f.db=Monstey_Gadb::New();
    f.names={"WriterA","WriterB","WriterC"};
    std::sort(f.names.begin(),f.names.end(),[](const auto&a,const auto&b){return Monstey_NativeHash(a)<Monstey_NativeHash(b);});
    const auto cat=static_cast<std::uint32_t>(f.db->CreateCategory("NativeSchema","Native")["category"]["index"].as_uint());
    for(std::size_t i=0;i<(third?3u:2u);++i){
        f.refs[i]=f.db->CreateRecord(cat,f.names[i])["record"]["selector"].as_string();
        f.db->AddField(f.refs[i],"A",3,variable?J::array({1}):J(1),J::object({{"array",variable?-1:1},{"native_ordinal",0}}));
        f.db->AddField(f.refs[i],"B",3,2,J::object({{"native_ordinal",1}}));
    }
    return f;
}
J Monstey_Capture(const Monstey_Gadb &db) {
    auto cs=J::array();const auto categories=db.Categories();
    for(const auto&c:categories["rows"].as_array()){
        auto records=J::array();
        for(std::size_t offset=0;;){
            auto page=db.DescriptorLayouts(static_cast<std::uint32_t>(c["index"].as_uint()),offset,10000);
            for(auto &r:page["rows"].as_array())records.as_array().push_back(std::move(r));
            offset+=page["rows"].size();if(offset>=page["total"].as_uint())break;
        }
        cs.as_array().push_back(J::object({{"schema",c["schema"]},{"name",c["name"]},{"records",records}}));
    }
    return J::object({{"format","Monstey_GadbClassCacheLayout"},{"version",1},
        {"source_sha256",db.Describe()["source_sha256"]},{"categories",cs}});
}
J Monstey_Unsafe(Monstey_Gadb&db,const std::filesystem::path&out,const Monstey_GadbClassCache&checker){
    try {(void)db.Build(out,checker);}catch(const Monstey_Error&e){
        Monstey_Need(e.code=="gadb_class_cache_unsafe","Wrong rejection classification");
        const std::string message=e.what();const auto at=message.find('{');
        Monstey_Need(at!=std::string::npos,"Missing bounded audit in exception");
        const auto audit=J::Parse(message.substr(at));
        Monstey_Need(audit["status"].as_string()=="unsafe","Unsafe result lost");
        Monstey_Need(!std::filesystem::exists(out)&&!std::filesystem::exists(out.wstring()+L".report.json")&&
            !std::filesystem::exists(out.wstring()+L".namemap.json"),"Unsafe plan published an artifact");
        return audit;
    }
    throw Monstey_Error("test","Expected unsafe writer rejection");
}
void Monstey_VerifyPublished(const J&build,const Monstey_GadbClassCache&checker){
    Monstey_Gadb reopened(Monstey_Wide(build["path"].as_string()));
    const auto independent=checker.Check(Monstey_Capture(reopened));
    const auto&audit=build["native_class_cache"];
    for(const auto&[key,value]:independent.as_object())
        Monstey_Need(audit.at(key)==value,("Writer/native readback checker mismatch: "+key).c_str());
    Monstey_Need(audit["layout_source_sha256"]==build["sha256"]&&audit["layout_stage"].as_string()=="final_native_writer_plan",
        "Plan qualification not tied to exact candidate");
    const auto sidecar=J::Parse(Monstey_ReadText(Monstey_Wide(build["report_path"].as_string())));
    Monstey_Need(sidecar["native_class_cache"]==audit,"Persisted guarded audit changed");
    Monstey_Need(!audit["runtime_verified"].as_bool()&&!build["runtime_verified"].as_bool(),"Engine claim invented");
}
}
int wmain(int argc,wchar_t**argv){try{
    Monstey_Need(argc==2,"Pass a new result directory");const std::filesystem::path out=argv[1];
    Monstey_Need(!std::filesystem::exists(out),"Result directory exists");std::filesystem::create_directories(out);
    static_assert(sizeof(Monstey_Gadb)==sizeof(void*));
    auto result=J::object({{"status","PASS"},{"groups",J::array()},{"engine_executed",false}});
    const auto test=[&](const char*name,const std::function<void(const std::filesystem::path&)>&fn){
        auto dir=out/Monstey_Wide(name);std::filesystem::create_directory(dir);
        try{fn(dir);}catch(const std::exception&e){throw Monstey_Error("test",std::string(name)+": "+e.what());}
        result["groups"].as_array().push_back(name);
    };
    const Monstey_GadbClassCache checker(Monstey_Reflection());
    test("safe_exact_final_projection",[&](const auto&d){auto f=Monstey_Create();const auto b=f.db->Build(d/L"safe.gamedb",checker);
        Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified","Safe plan unqualified");Monstey_VerifyPublished(b,checker);});
    test("later_reorder_rejected_before_any_publication",[&](const auto&d){auto f=Monstey_Create();f.db->MoveField(f.refs[1],"B",0);
        const auto a=Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);Monstey_Need(a["cached_slot_hazards"].as_uint()==2,"Later reordered slots not proved");
        const auto changes=f.db->Changes();(void)f.db->Build(d/L"legacy.gamedb");Monstey_Need(changes==f.db->Changes(),"Guard mutated project");});
    test("later_delete_rejected",[&](const auto&d){auto f=Monstey_Create();f.db->DeleteField(f.refs[1],"A");
        const auto a=Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);Monstey_Need(a["cached_slot_hazards"].as_uint()==2,"Deleted slot not caught");});
    test("first_reorder_hash_fallback_is_not_normalized",[&](const auto&d){auto f=Monstey_Create();f.db->MoveField(f.refs[0],"B",0);
        const auto b=f.db->Build(d/L"fallback.gamedb",checker);Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified","Fallback rejected");
        for(const auto&field:b["native_class_cache"]["categories"][0u]["fields"].as_array())Monstey_Need(field["direct_initial_mode"].as_string()=="hash_fallback","Fallback forced into cache");
        (void)f.db->Build(d/L"legacy.gamedb");Monstey_Need(Monstey_ReadFile(d/L"fallback.gamedb")==Monstey_ReadFile(d/L"legacy.gamedb"),"Guard rewrote descriptors");Monstey_VerifyPublished(b,checker);});
    test("first_delete_hash_fallback",[&](const auto&d){auto f=Monstey_Create();f.db->DeleteField(f.refs[0],"A");Monstey_VerifyPublished(f.db->Build(d/L"safe.gamedb",checker),checker);});
    test("typed_resize_uses_final_count_not_snapshot",[&](const auto&d){auto f=Monstey_Create(true);f.db->SetField(f.refs[1],"A",J::array({1,2,3}));
        const auto snapshot=f.db->DescriptorLayouts(0);Monstey_Need(snapshot["rows"][1u]["fields"][0u]["count"].as_uint()==1,"Control did not retain snapshot count");
        const auto b=f.db->Build(d/L"resized.gamedb",checker);const auto&a=b["native_class_cache"];
        Monstey_Need(a["status"].as_string()=="unknown"&&a["unqualified_value_contracts"].as_uint()==1&&a["cached_slot_hazards"].as_uint()==0,"Resize mislabeled structural/safe");
        Monstey_Gadb opened(d/L"resized.gamedb");Monstey_Need(opened.DescriptorLayouts(0)["rows"][1u]["fields"][0u]["count"].as_uint()==3,"Final resized count missing");Monstey_VerifyPublished(b,checker);});
    test("first_resize_final_fallback",[&](const auto&d){auto f=Monstey_Create(true);f.db->SetField(f.refs[0],"A",J::array({1,2}));
        const auto b=f.db->Build(d/L"resized.gamedb",checker);Monstey_Need(b["native_class_cache"]["categories"][0u]["fields"][0u]["direct_initial_mode"].as_string()=="hash_fallback","Final first count not used");Monstey_VerifyPublished(b,checker);});
    test("rename_changes_actual_first_record_and_detects_hazard",[&](const auto&d){auto f=Monstey_Create(false,true);f.db->MoveField(f.refs[0],"B",0);
        // Renaming old first beyond its successor makes the good successor initialize cache slots.
        std::string high;for(unsigned i=0;i<100000;++i){auto n="Last"+std::to_string(i);if(Monstey_NativeHash(n)>Monstey_NativeHash(f.names[2])){high=n;break;}}
        Monstey_Need(!high.empty(),"No high name found");f.db->RenameRecord(f.refs[0],high);
        const auto a=Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);Monstey_Need(a["cached_slot_hazards"].as_uint()==2,"Final sorted first not used");});
    test("deleted_first_record_does_not_initialize_cache",[&](const auto&d){auto f=Monstey_Create();f.db->MoveField(f.refs[1],"B",0);f.db->DeleteRecord(f.refs[0]);
        const auto b=f.db->Build(d/L"fallback.gamedb",checker);Monstey_Need(b["native_class_cache"]["records_checked"].as_uint()==1,"Deleted record validated");Monstey_VerifyPublished(b,checker);});
    test("deleted_bad_record_is_excluded",[&](const auto&d){auto f=Monstey_Create();f.db->MoveField(f.refs[1],"B",0);f.db->DeleteRecord(f.refs[1]);Monstey_VerifyPublished(f.db->Build(d/L"safe.gamedb",checker),checker);});
    test("clone_published_name_and_remap",[&](const auto&d){auto f=Monstey_Create();f.db->CloneRecord(f.refs[0],"FriendlyClone");
        const auto b=f.db->Build(d/L"clone.gamedb",checker);Monstey_Need(b["native_class_cache"]["records_checked"].as_uint()==3,"Clone omitted");Monstey_VerifyPublished(b,checker);});
    test("clone_later_reorder_hazard",[&](const auto&d){auto f=Monstey_Create();f.db->CloneRecord(f.refs[0],"FriendlyClone");f.db->MoveField("Monstey_FriendlyClone","B",0);
        (void)Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);});
    test("clone_rename_and_deleted_clone_final_order",[&](const auto&d){auto f=Monstey_Create();f.db->CloneRecord(f.refs[0],"ToDelete");f.db->CloneRecord(f.refs[0],"ToRename");
        f.db->DeleteRecord("Monstey_ToDelete");f.db->RenameRecord("Monstey_ToRename","FinalRenamedClone");Monstey_VerifyPublished(f.db->Build(d/L"safe.gamedb",checker),checker);});
    test("missing_native_evidence_unknown_not_blocked",[&](const auto&d){auto f=Monstey_Create();const Monstey_GadbClassCache none(J::object());
        const auto b=f.db->Build(d/L"unknown.gamedb",none);Monstey_Need(b["native_class_cache"]["unknown_categories"].as_uint()==1,"Missing evidence silently qualified");Monstey_VerifyPublished(b,none);});
    test("duplicate_binding_context_complete",[&](const auto&d){auto f=Monstey_Create();const auto cat=static_cast<std::uint32_t>(f.db->CreateCategory("OtherSchema","Native")["category"]["index"].as_uint());
        const auto ref=f.db->CreateRecord(cat,"OtherRecord")["record"]["selector"].as_string();f.db->AddField(ref,"A",3,1,J::object({{"native_ordinal",0}}));f.db->AddField(ref,"B",3,1,J::object({{"native_ordinal",1}}));
        const auto b=f.db->Build(d/L"duplicate.gamedb",checker);Monstey_Need(b["native_class_cache"]["unknown_categories"].as_uint()==2,"Streaming lost global duplicate context");Monstey_VerifyPublished(b,checker);});
    test("native_parent_shared_state_unknown",[&](const auto&d){auto f=Monstey_Create();auto r=Monstey_Reflection();auto parent=Monstey_Reflection("Parent");
        r["entries"].as_array().push_back(parent["entries"][0u]);r["entries"][1u]["entry_ea"]="0x142200000";r["entries"][0u]["parent_ea"]="0x142200000";
        r["categories"]["Native"]["parent"]="Parent";r["categories"]["Parent"]=parent["categories"]["Parent"];
        const Monstey_GadbClassCache inherited(r);const auto b=f.db->Build(d/L"inherited.gamedb",inherited);Monstey_Need(b["native_class_cache"]["status"].as_string()=="unknown","Inherited state guessed");Monstey_VerifyPublished(b,inherited);});
    test("empty_plan_explicit_evidence_retained",[&](const auto&d){const auto db=Monstey_Gadb::New();const auto b=db->Build(d/L"empty.gamedb",checker);
        Monstey_Need(b["native_class_cache"]["category_count"].as_uint()==0&&b["native_class_cache"]["reflection_source_sha256"].as_string()==Monstey_ProofSha,"Empty evidence lost");Monstey_VerifyPublished(b,checker);});
    test("raw_native_name_bytes_do_not_hash_display",[&](const auto&d){const auto file=d/L"raw.gamedb";Monstey_WriteNewFile(file,Fixture());Monstey_Gadb db(file);
        const Monstey_GadbClassCache names(Monstey_Reflection("Names",J::array()));const auto b=db.Build(d/L"output.gamedb",names);
        Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified","Raw names escaped before hashing");Monstey_Need(Monstey_ReadFile(file)==Monstey_ReadFile(d/L"output.gamedb"),"Raw name bytes changed");Monstey_VerifyPublished(b,names);});
    test("guarded_existing_output_preserves_file",[&](const auto&d){auto f=Monstey_Create();Monstey_WriteNewText(d/L"existing.gamedb","sentinel");bool bad=false;
        try{(void)f.db->Build(d/L"existing.gamedb",checker);}catch(const std::exception&){bad=true;}Monstey_Need(bad&&Monstey_ReadText(d/L"existing.gamedb")=="sentinel","Existing output overwritten");});
    test("legacy_mode_has_no_added_report_keys",[&](const auto&d){auto f=Monstey_Create();const auto b=f.db->Build(d/L"legacy.gamedb");Monstey_Need(!b.contains("native_class_cache"),"Old report contract changed");});
    test("diagnostics_cap_does_not_cap_rejection_totals",[&](const auto&d){auto f=Monstey_Create();
        for(unsigned i=0;i<300;++i){const auto ref=f.db->CreateRecord(0,"Extra"+std::to_string(i))["record"]["selector"].as_string();
            f.db->AddField(ref,"A",3,1,J::object({{"native_ordinal",0}}));f.db->AddField(ref,"B",3,2,J::object({{"native_ordinal",1}}));
            f.db->MoveField(ref,"B",0);}
        // Keep one good record at the lowest raw hash, without relying on insertion order.
        // Native hash is verified from the same native helper; a one-character name is deterministic.
        Monstey_Need(Monstey_NativeHash("A")==1u,"Native hash fixture assumption changed");
        f.db->RenameRecord(f.refs[0],"A");
        const auto a=Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);
        Monstey_Need(a["records_checked"].as_uint()==302&&a["cached_slot_hazards"].as_uint()==600&&
            a["diagnostics_total"].as_uint()==600&&a["diagnostics"].size()==256&&a["diagnostics_truncated"].as_bool(),"Diagnostic cap hid full unsafe totals");});
    test("prepared_native_evidence_immutable_during_build",[&](const auto&d){auto f=Monstey_Create();auto r=Monstey_Reflection();const Monstey_GadbClassCache prepared(r);
        r["categories"]=J::object();r["entries"]=J::array();r["source_sha256"]="modified caller text";
        const auto b=f.db->Build(d/L"safe.gamedb",prepared);Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified"&&
            b["native_class_cache"]["reflection_source_sha256"].as_string()==Monstey_ProofSha,"Build retained mutable caller declarations");Monstey_VerifyPublished(b,prepared);});
    test("failure_leaves_no_mutation_and_undo_restores_safe_plan",[&](const auto&d){auto f=Monstey_Create();const auto before=f.db->Changes();f.db->MoveField(f.refs[1],"B",0);
        const auto changed=f.db->Changes();(void)Monstey_Unsafe(*f.db,d/L"bad.gamedb",checker);Monstey_Need(f.db->Changes()==changed,"Guard altered history/state");
        (void)f.db->Undo();Monstey_Need(f.db->Changes()==before,"Guard consumed undo step");Monstey_VerifyPublished(f.db->Build(d/L"restored.gamedb",checker),checker);});
    result["count"]=result["groups"].size();Monstey_WriteNewText(out/L"result.json",result.dump(2)+"\n");std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
