#include "Monstey_GadbRegistry.hpp"
#include <iostream>
#include <array>
using namespace Monstey;
int wmain(int argc,wchar_t** argv) {
 try {
    Monstey_Require(argc==4||argc==5,"Usage: RegistryTool image expected_sha256 new_output [coverage_schema]");
    const std::filesystem::path source=argv[1],output=argv[3];
    Monstey_RequireOfflineOutput(output,std::array{source});
    const auto bytes=Monstey_ReadFile(source);
    auto result=Monstey_GadbDiscoverNativeDeclarations(bytes,Monstey_Utf8(argv[2]));
    if(argc==5) {
        const auto schema=Monstey_Json::Parse(Monstey_ReadText(argv[4],128*1024*1024));
        auto missing=Monstey_Json::array();std::uint64_t matched=0;
        for(const auto& [name,value]:schema.at("categories").as_object()) {
            (void)value;if(result.at("categories").contains(name))++matched;
            else missing.as_array().push_back(name);
        }
        result["coverage"]=Monstey_Json::object({{"source",Monstey_Utf8(std::filesystem::path(argv[4]).wstring())},
            {"schema_sha256",Monstey_HashSha256(argv[4])},{"matched",matched},{"missing",missing},
            {"schema_used_for_discovery",false}});
    }
    Monstey_Require(Monstey_HashSha256(source)==result.at("source_sha256").as_string(),"Input changed during discovery");
    Monstey_WriteNewText(output,result.dump(2)+"\n");
    std::cout<<Monstey_Json::object({{"status","PASS"},{"categories",result.at("categories").size()},
        {"unknown",result.at("unknown").size()},{"ambiguous",result.at("ambiguous").size()},
        {"output_sha256",Monstey_HashSha256(output)}}).dump()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
