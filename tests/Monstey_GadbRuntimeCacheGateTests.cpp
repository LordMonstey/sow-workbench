#include "Monstey_Runtime.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_SowCatalogProjection.hpp"
#include "Monstey_RuntimeGateFixtures.inc"
#define wmain Monstey_FxLibraryOriginalEntry
#include "Monstey_FxLibraryTests.cpp"
#undef wmain
using namespace Monstey;
namespace {
using J=Monstey_Json;
J Monstey_RuntimeRequest(const char*op,const J&a){return J::object({{"id",37},{"operation",op},{"arguments",a}});}
J Monstey_Good(Monstey_Runtime&r,const char*op,J a=J::object()){
    const auto reply=r.Call(Monstey_RuntimeRequest(op,a));Monstey_Need(reply["ok"].as_bool(),reply.dump().c_str());return reply;}
J Monstey_Bad(Monstey_Runtime&r,const char*op,const J&a,const char*code=nullptr){
    const auto before=Monstey_Good(r,"describe");const auto reply=r.Call(Monstey_RuntimeRequest(op,a));
    Monstey_Need(!reply["ok"].as_bool(),"Unexpected success");if(code)Monstey_Need(reply["error"]["code"].as_string()==code,reply.dump().c_str());
    Monstey_Need(reply["generation"]==before["generation"]&&reply["revision"]==before["revision"],"Failed guard changed session counters");return reply;}
void Monstey_Load(Monstey_Runtime&r,Monstey_Gadb&db,const std::filesystem::path&d,bool fx=false){
    const auto file=d/L"project.json";db.SaveProject(file);Monstey_Good(r,fx?"fx_load_project":"database_load_project",J::object({{"path",Monstey_Utf8(file.wstring())}}));}
J Monstey_Profile(const std::filesystem::path&d,const std::filesystem::path&out,const J&reflection){
    const auto file=d/L"native-declarations.json";Monstey_WriteNewText(file,reflection.dump(2));return J::object({
        {"output",Monstey_Utf8(out.wstring())},{"native_declarations",Monstey_Utf8(file.wstring())},
        {"expected_native_declarations_sha256",Monstey_HashSha256(file)}});}
void Monstey_Absent(const std::filesystem::path&file){Monstey_Need(!std::filesystem::exists(file)&&
    !std::filesystem::exists(file.wstring()+L".namemap.json")&&!std::filesystem::exists(file.wstring()+L".report.json"),"Rejected guard published output");}
std::unique_ptr<Monstey_Gadb> Monstey_Effects(){
    auto f=Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix());f->CreateCategory("NativeSchema","Native");
    std::array<std::string,2> names{"EffectA","EffectB"};std::sort(names.begin(),names.end(),[](const auto&a,const auto&b){return Monstey_NativeHash(a)<Monstey_NativeHash(b);});
    for(const auto&name:names){const auto ref=f->CreateRecord(0,name)["record"]["selector"].as_string();
        f->AddField(ref,"A",3,1,J::object({{"native_ordinal",0}}));f->AddField(ref,"B",3,2,J::object({{"native_ordinal",1}}));}return f;
}
J Monstey_McpCall(Monstey_Mcp&m,const char*op,const J&a){
    const auto reply=m.Handle(J::object({{"jsonrpc","2.0"},{"id",42},{"method","tools/call"},
        {"params",J::object({{"name",op},{"arguments",a}})}}));Monstey_Need(reply.has_value()&&reply->contains("result"),"MCP transport failed");
    const auto&result=reply->at("result");const auto response=J::Parse(result["content"][0u]["text"].as_string());
    Monstey_Need(result["isError"].as_bool()==!response["ok"].as_bool(),"MCP isError lost native failure");return response;
}
}
int wmain(int argc,wchar_t**argv){try{
    if(argc==2&&std::wstring_view(argv[1])==L"--catalog"){std::cout<<Monstey_Runtime::Catalog().dump(2);return 0;}
    Monstey_Need(argc==3,"Pass a new output directory and baseline catalog");const std::filesystem::path out=argv[1];
    Monstey_Need(!std::filesystem::exists(out),"Output exists");std::filesystem::create_directories(out);
    auto result=J::object({{"status","PASS"},{"groups",J::array()},{"runtime_verified",false}});
    const auto test=[&](const char*name,const std::function<void(const std::filesystem::path&)>&fn){const auto d=out/Monstey_Wide(name);std::filesystem::create_directory(d);
        try{fn(d);}catch(const std::exception&e){throw Monstey_Error("test",std::string(name)+": "+e.what());}result["groups"].as_array().push_back(name);};
    test("catalog_only_two_additive_paired_descriptors",[&](const auto&){auto actual=Monstey_HistoricalCatalogWithoutAtomUses(Monstey_Runtime::Catalog());const auto baseline=J::Parse(Monstey_ReadText(argv[2]));
        Monstey_Need(actual.size()==226,"Expected trace, bank and static-tree read-only operations");J projected=J::array();for(const auto&row:actual.as_array()){if(row["name"]!=J("database_native_class")&&row["name"]!=J("database_cache_trace")&&row["name"]!=J("database_native_banks")&&row["name"]!=J("sow_world_navigation_static_tree"))projected.as_array().push_back(row);}actual=std::move(projected);
        unsigned changed=0;for(auto&row:actual.as_array()){if(row["name"]==J("database_build")||row["name"]==J("fx_build")){
            auto&schema=row["inputSchema"];Monstey_Need(schema["properties"]["native_declarations"]["format"].as_string()=="path"&&
                schema["properties"]["expected_native_declarations_sha256"]["minLength"].as_uint()==64&&schema.contains("dependentRequired"),"Profile schema incomplete");
            schema["properties"].as_object().erase("native_declarations");schema["properties"].as_object().erase("expected_native_declarations_sha256");schema.as_object().erase("dependentRequired");++changed;}}
        Monstey_Need(changed==2&&actual==baseline,"Unrelated operation descriptor changed");result["catalog_operations"]=actual.size();});
    test("direct_guarded_qualified_profile_provenance",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto a=Monstey_Profile(d,d/L"safe.gamedb",Monstey_Reflection());const auto before=Monstey_Good(r,"describe");const auto reply=Monstey_Good(r,"database_build",a);const auto&b=reply["result"];
        Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified"&&reply["generation"]==before["generation"]&&
            reply["revision"].as_uint()==before["revision"].as_uint()+1,"Session/build classification changed");
        Monstey_Need(b["native_declaration_profile"]["sha256"]==a["expected_native_declarations_sha256"]&&
            b["native_class_cache"]["reflection_source_sha256"].as_string()==Monstey_ProofSha&&
            b["native_declaration_profile"]["sha256"]!=b["native_class_cache"]["reflection_source_sha256"]&&
            b["native_declaration_profile"]["input_contract"].as_string()=="loaded_pinned_snapshot"&&
            !b["native_declaration_profile"]["path_rechecked_after_publication"].as_bool(),"Profile-file and image provenance conflated");});
    test("direct_unknown_is_success_not_silent_qualified",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto a=Monstey_Profile(d,d/L"unknown.gamedb",J::object());const auto b=Monstey_Good(r,"database_build",a)["result"];
        Monstey_Need(b["native_class_cache"]["status"].as_string()=="unknown"&&std::filesystem::exists(d/L"unknown.gamedb"),"UNKNOWN rejected or renamed");});
    test("direct_unsafe_no_artifacts_no_revision_or_project_change",[&](const auto&d){auto f=Monstey_Create();f.db->MoveField(f.refs[1],"B",0);Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto a=Monstey_Profile(d,d/L"unsafe.gamedb",Monstey_Reflection());const auto changes=Monstey_Good(r,"database_changes")["result"];
        (void)Monstey_Bad(r,"database_build",a,"gadb_class_cache_unsafe");Monstey_Absent(d/L"unsafe.gamedb");Monstey_Need(Monstey_Good(r,"database_changes")["result"]==changes,"Guard changed database state");});
    test("legacy_arguments_and_bytes_unchanged",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto a=Monstey_Profile(d,d/L"safe.gamedb",Monstey_Reflection());(void)Monstey_Good(r,"database_build",a);
        const auto b=Monstey_Good(r,"database_build",J::object({{"output",Monstey_Utf8((d/L"legacy.gamedb").wstring())}}))["result"];
        Monstey_Need(!b.contains("native_class_cache")&&!b.contains("native_declaration_profile")&&Monstey_ReadFile(d/L"legacy.gamedb")==Monstey_ReadFile(d/L"safe.gamedb"),"Legacy contract changed");});
    test("paired_optional_arguments_both_directions",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto a=Monstey_Profile(d,d/L"bad.gamedb",Monstey_Reflection());auto onlyPath=a;onlyPath.as_object().erase("expected_native_declarations_sha256");
        (void)Monstey_Bad(r,"database_build",onlyPath,"invalid_arguments");auto onlyHash=a;onlyHash.as_object().erase("native_declarations");(void)Monstey_Bad(r,"database_build",onlyHash,"invalid_arguments");Monstey_Absent(d/L"bad.gamedb");});
    test("profile_pin_failures_and_malformed_hash",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        auto a=Monstey_Profile(d,d/L"bad.gamedb",Monstey_Reflection());a["expected_native_declarations_sha256"]=std::string(64,'0');(void)Monstey_Bad(r,"database_build",a,"source_hash_mismatch");
        a["expected_native_declarations_sha256"]=std::string(64,'z');(void)Monstey_Bad(r,"database_build",a,"invalid_arguments");a["expected_native_declarations_sha256"]="123";(void)Monstey_Bad(r,"database_build",a,"argument_out_of_range");Monstey_Absent(d/L"bad.gamedb");});
    test("profile_json_failure_and_nonpath_rejected",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto path=d/L"bad.json";Monstey_WriteNewText(path,"{ malformed");auto a=J::object({{"output",Monstey_Utf8((d/L"bad.gamedb").wstring())},
            {"native_declarations",Monstey_Utf8(path.wstring())},{"expected_native_declarations_sha256",Monstey_HashSha256(path)}});
        (void)Monstey_Bad(r,"database_build",a);a["native_declarations"]=nullptr;(void)Monstey_Bad(r,"database_build",a,"invalid_argument_type");Monstey_Absent(d/L"bad.gamedb");});
    test("profile_cannot_be_output_or_settings_overwrite",[&](const auto&d){auto f=Monstey_Create();const auto profile=d/L"profile.gamedb";Monstey_Runtime r({},profile);Monstey_Load(r,*f.db,d);
        Monstey_WriteNewText(profile,Monstey_Reflection().dump(2));const auto h=Monstey_HashSha256(profile);auto a=J::object({{"output",Monstey_Utf8(profile.wstring())},
            {"native_declarations",Monstey_Utf8(profile.wstring())},{"expected_native_declarations_sha256",h}});(void)Monstey_Bad(r,"database_build",a);
        a["output"]=Monstey_Utf8((d/L"safe.gamedb").wstring());(void)Monstey_Good(r,"database_build",a);
        (void)Monstey_Bad(r,"configure",J::object({{"save",true}}));Monstey_Need(Monstey_HashSha256(profile)==h,"Profile overwritten");});
    test("stale_session_revision_precedes_profile_read",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);const auto current=Monstey_Good(r,"describe");
        auto a=J::object({{"output",Monstey_Utf8((d/L"bad.gamedb").wstring())},{"native_declarations",Monstey_Utf8((d/L"does-not-exist.json").wstring())},
            {"expected_native_declarations_sha256",std::string(64,'0')},{"expected_generation",current["generation"].as_uint()+1}});
        (void)Monstey_Bad(r,"database_build",a,"stale_session");a.as_object().erase("expected_generation");a["expected_revision"]=current["revision"].as_uint()+1;
        (void)Monstey_Bad(r,"database_build",a,"stale_revision");Monstey_Absent(d/L"bad.gamedb");});
    test("json_reply_unknown_and_unsafe",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        auto a=Monstey_Profile(d,d/L"unknown.gamedb",J::object());const auto reply=r.Reply(Monstey_RuntimeRequest("database_build",a).dump());
        Monstey_Need(reply["ok"].as_bool()&&reply["result"]["native_class_cache"]["status"].as_string()=="unknown","JSON UNKNOWN failed");
        (void)Monstey_Good(r,"database_move_field",J::object({{"record",f.refs[1]},{"field","B"},{"ordinal",0}}));
        const auto actual=d/L"qualified.json";Monstey_WriteNewText(actual,Monstey_Reflection().dump());a["native_declarations"]=Monstey_Utf8(actual.wstring());a["expected_native_declarations_sha256"]=Monstey_HashSha256(actual);a["output"]=Monstey_Utf8((d/L"unsafe.gamedb").wstring());
        const auto failed=r.Reply(Monstey_RuntimeRequest("database_build",a).dump());Monstey_Need(!failed["ok"].as_bool()&&failed["error"]["code"].as_string()=="gadb_class_cache_unsafe","JSON failure lost");Monstey_Absent(d/L"unsafe.gamedb");});
    test("mcp_tools_list_call_success_unknown_unsafe",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);Monstey_Mcp m(r);
        Monstey_Need(m.Handle(J::object({{"jsonrpc","2.0"},{"id",1},{"method","initialize"}})).has_value(),"MCP initialization failed");
        const auto list=m.Handle(J::object({{"jsonrpc","2.0"},{"id",2},{"method","tools/list"}}));Monstey_Need(list->at("result")["tools"]==Monstey_Runtime::Catalog(),"MCP schema not updated");
        auto a=Monstey_Profile(d,d/L"safe.gamedb",Monstey_Reflection());auto reply=Monstey_McpCall(m,"database_build",a);Monstey_Need(reply["ok"].as_bool()&&reply["result"]["native_class_cache"]["status"].as_string()=="qualified","MCP qualified failed");
        const auto unknown=d/L"unknown.json";Monstey_WriteNewText(unknown,"{}");a["native_declarations"]=Monstey_Utf8(unknown.wstring());a["expected_native_declarations_sha256"]=Monstey_HashSha256(unknown);a["output"]=Monstey_Utf8((d/L"unknown.gamedb").wstring());
        reply=Monstey_McpCall(m,"database_build",a);Monstey_Need(reply["ok"].as_bool()&&reply["result"]["native_class_cache"]["status"].as_string()=="unknown","MCP UNKNOWN treated error");
        (void)Monstey_Good(r,"database_move_field",J::object({{"record",f.refs[1]},{"field","B"},{"ordinal",0}}));a["native_declarations"]=Monstey_Utf8((d/L"native-declarations.json").wstring());a["expected_native_declarations_sha256"]=Monstey_HashSha256(d/L"native-declarations.json");a["output"]=Monstey_Utf8((d/L"unsafe.gamedb").wstring());
        reply=Monstey_McpCall(m,"database_build",a);Monstey_Need(!reply["ok"].as_bool()&&reply["error"]["code"].as_string()=="gadb_class_cache_unsafe","MCP unsafe accepted");Monstey_Absent(d/L"unsafe.gamedb");});
    test("fx_shared_guarded_route_qualified_and_legacy",[&](const auto&d){auto f=Monstey_Effects();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f,d,true);
        const auto a=Monstey_Profile(d,d/L"safe.fxp",Monstey_Reflection());const auto b=Monstey_Good(r,"fx_build",a)["result"];Monstey_Need(b["native_class_cache"]["status"].as_string()=="qualified","FX guard not reached");
        const auto old=Monstey_Good(r,"fx_build",J::object({{"output",Monstey_Utf8((d/L"legacy.fxp").wstring())}}))["result"];
        Monstey_Need(!old.contains("native_class_cache")&&Monstey_ReadFile(d/L"safe.fxp")==Monstey_ReadFile(d/L"legacy.fxp"),"Legacy FX changed");});
    test("fx_unsafe_and_unpaired_profile_no_artifacts",[&](const auto&d){auto f=Monstey_Effects();f->MoveField("0x00000001","B",0);Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f,d,true);
        auto a=Monstey_Profile(d,d/L"unsafe.fxp",Monstey_Reflection());(void)Monstey_Bad(r,"fx_build",a,"gadb_class_cache_unsafe");a.as_object().erase("expected_native_declarations_sha256");(void)Monstey_Bad(r,"fx_build",a,"invalid_arguments");Monstey_Absent(d/L"unsafe.fxp");});
    test("fx_prefab_materialization_is_preserved",[&](const auto&d){const auto schema=d/L"schema.json";Monstey_WriteNewText(schema,Monstey_Schema().dump());auto f=Monstey_IntegratedFixture(schema);
        f.db->SetField(f.template_record,"Strength",66.0);Monstey_Need(Monstey_Value(*f.db,f.child,"Strength").as_number()!=66.0,"Control already materialized");
        Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d,true);const auto a=Monstey_Profile(d,d/L"prefab.fxp",J::object());
        const auto b=Monstey_Good(r,"fx_build",a)["result"];Monstey_Need(b["native_class_cache"]["status"].as_string()=="unknown","Missing FX evidence not explicit");
        const auto meta=Monstey_Good(r,"fx_record",J::object({{"record",f.child}}))["result"];bool strength=false;for(const auto&field:meta["fields"].as_array())
            if(field["name"].as_string()=="Strength")strength=field["value"].as_number()==66.0;Monstey_Need(strength,"Guard bypassed native prefab transaction");});
    test("relative_unicode_profile_has_exact_absolute_provenance",[&](const auto&d){auto f=Monstey_Create();Monstey_Runtime r({},d/L"settings.json");Monstey_Load(r,*f.db,d);
        const auto profile=d/L"declarations-épreuve.json";Monstey_WriteNewText(profile,Monstey_Reflection().dump());
        struct Monstey_CurrentDirectory {std::filesystem::path before=std::filesystem::current_path();
            ~Monstey_CurrentDirectory(){std::error_code e;std::filesystem::current_path(before,e);}} current;
        std::filesystem::current_path(d);const auto reply=Monstey_Good(r,"database_build",J::object({{"output","relative.gamedb"},
            {"native_declarations",Monstey_Utf8(profile.filename().wstring())},{"expected_native_declarations_sha256",Monstey_HashSha256(profile)}}));
        const std::filesystem::path loaded(Monstey_Wide(reply["result"]["native_declaration_profile"]["path"].as_string()));
        Monstey_Need(loaded.is_absolute()&&std::filesystem::equivalent(loaded,profile)&&
            std::filesystem::exists(d/L"relative.gamedb"),"Relative profile did not retain exact loaded absolute path");});
    test("legacy_fx_path_validation_precedes_missing_session",[&](const auto&d){Monstey_Runtime r({},d/L"settings.json");
        (void)Monstey_Bad(r,"fx_build",J::object({{"output",std::string(1,char{})}}),"invalid_path");});
    result["count"]=result["groups"].size();Monstey_WriteNewText(out/L"result.json",result.dump(2)+"\n");std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
