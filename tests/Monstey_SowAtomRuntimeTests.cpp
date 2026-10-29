#include "Monstey_Runtime.hpp"
#include "Monstey_AtomFixtures.hpp"
#include <algorithm>
#include <iostream>
using namespace Monstey;
namespace {
using J=Monstey_Json;
std::size_t Monstey_Checks=0;
void Monstey_Check(bool condition,const char*message) {
    ++Monstey_Checks;
    if(!condition)throw std::runtime_error(message);
}
J Monstey_Call(Monstey_Runtime &runtime,const char*operation,J args=J::object()) {
    auto reply=runtime.Call(J::object({{"operation",operation},{"arguments",std::move(args)}}));
    Monstey_Check(reply.at("ok").as_bool(),reply.dump().c_str());
    return reply;
}
void Monstey_ErrorReply(Monstey_Runtime &runtime,J args,const char*code) {
    const auto reply=runtime.Call(J::object({{"operation","database_atom_uses"},{"arguments",std::move(args)}}));
    Monstey_Check(!reply.at("ok").as_bool()&&reply.at("error").at("code")==J(code),reply.dump().c_str());
}
}
int wmain(int argc,wchar_t**argv) {
    try {
        Monstey_Require(argc==3,"Fresh output directory and baseline catalog required");
        const std::filesystem::path root=argv[1];
        Monstey_Require(!std::filesystem::exists(root),"Fresh output directory required");
        std::filesystem::create_directories(root);
        auto pool=Monstey_AtomFixturePool();
        const auto at=Monstey_AtomFixtureAppend(pool,{0xff,0});
        const auto fixture=Monstey_AtomFixture(pool,{{30,19,4,1,0},{10,7,2,1,1}},{at,0x7fc54321u});
        const auto source=root/std::filesystem::path(L"source_\u00e9_\u6d4b\u8bd5.gamedb");
        Monstey_WriteNewFile(source,fixture);
        const auto sourceHash=Monstey_HashSha256(source);
        Monstey_Runtime runtime({},root/L"absent-settings.json");
        const auto catalog=Monstey_Runtime::Catalog();
        const auto old=J::Parse(Monstey_ReadText(argv[2]));
        Monstey_Check(old.size()==226&&catalog.size()==227,"Additive catalog count");
        auto inherited=J::array();
        for(const auto &op:catalog.as_array())
            if(op.at("name")!=J("database_atom_uses"))inherited.as_array().push_back(op);
            else Monstey_Check(op.at("read_only").as_bool()&&
                op.at("annotations").at("readOnlyHint").as_bool()&&
                op.at("inputSchema").at("required")==J::array({"record"}),"New read-only descriptor");
        Monstey_Check(inherited==old,"Inherited descriptors changed");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"}}),"database_not_open");
        Monstey_Call(runtime,"open_database",J::object({{"path",Monstey_Utf8(source.wstring())}}));
        const auto before=Monstey_Call(runtime,"describe");
        const auto changes=Monstey_Call(runtime,"database_changes");
        const auto all=Monstey_Call(runtime,"database_atom_uses",J::object({{"record","0x0"}}));
        Monstey_Check(all.at("generation")==before.at("generation")&&
                      all.at("revision")==before.at("revision"),"Reader changed counters");
        const auto &q=all.at("result");
        Monstey_Check(q.at("total")==J(2)&&q.at("rows").size()==2&&
                      q.at("source_sha256")==J(sourceHash),"Wrong source/result");
        Monstey_Check(q.at("rows").at(0).at("atom").at("length_bytes")==J(2)&&
                      q.at("rows").at(1).at("raw_u32")==J(0x7fc54321u),"Raw source bits changed");
        const auto page=Monstey_Call(runtime,"database_atom_uses",J::object({{"record","0x0"},{"offset",1},{"limit",1}}));
        Monstey_Check(page.at("result").at("rows")==J::array({q.at("rows").at(1)}),"Paging altered a row");
        Monstey_ErrorReply(runtime,J::object(),"missing_argument");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"limit",0}}),"argument_out_of_range");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"limit",501}}),"argument_out_of_range");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"offset",-1}}),"argument_out_of_range");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"source","foreign"}}),"unknown_argument");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"expected_generation",before.at("generation").as_uint()+1}}),"stale_session");
        Monstey_ErrorReply(runtime,J::object({{"record","0x0"},{"expected_revision",before.at("revision").as_uint()+1}}),"stale_revision");
        Monstey_Check(Monstey_Call(runtime,"describe").at("result")==before.at("result")&&
                      Monstey_Call(runtime,"database_changes").at("result")==changes.at("result"),"Read changed project/history");
        const auto clone=Monstey_Call(runtime,"database_clone",J::object({{"source","0x0"},{"new_name","Monstey_AtomCopy"}}))
            .at("result").at("clone").at("selector").as_string();
        Monstey_Call(runtime,"database_add_field",J::object({{"record",clone},{"field","Owned"},{"native_type",3},{"value",7}}));
        const auto dirty=Monstey_Call(runtime,"describe");
        const auto dirtyChanges=Monstey_Call(runtime,"database_changes");
        const auto pending=Monstey_Call(runtime,"database_atom_uses",J::object({{"record",clone}}));
        bool found=false;
        for(const auto &row:pending.at("result").at("rows").as_array())
            if(row.at("introduced").as_bool()) {
                found=true;
                Monstey_Check(row.at("status")==J("not_yet_serialized")&&row.at("atom").is_null()&&
                              row.at("raw_u32").is_null()&&row.at("source_word_file_offset").is_null(),
                              "Pending edit fabricated source storage");
            }
        Monstey_Check(found,"Introduced field missing");
        Monstey_Check(pending.at("generation")==dirty.at("generation")&&
                      pending.at("revision")==dirty.at("revision")&&
                      Monstey_Call(runtime,"describe").at("result")==dirty.at("result")&&
                      Monstey_Call(runtime,"database_changes").at("result")==dirtyChanges.at("result"),
                      "Dirty project/history changed during read");
        Monstey_Call(runtime,"database_undo");
        Monstey_Check(Monstey_Call(runtime,"database_atom_uses",J::object({{"record",clone}}))
                      .at("result").at("total")==J(2),"Read consumed undo entry");
        Monstey_Check(Monstey_HashSha256(source)==sourceHash&&
                      !std::filesystem::exists(root/L"absent-settings.json"),"Source/settings changed");
        Monstey_WriteNewText(root/L"report.json",J::object({{"status","PASS"},{"checks",Monstey_Checks},
            {"catalog",227},{"inherited_descriptors_unchanged",226},{"SoW_GADB17",true},
            {"native_engine_or_SDK_executed",false},{"source_unchanged",true}}).dump(2)+"\n");
        std::cout<<"PASS "<<Monstey_Checks<<" Runtime checks\n";return 0;
    } catch(const std::exception&e) {std::cerr<<e.what()<<'\n';return 1;}
}
