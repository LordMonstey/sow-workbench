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
}
