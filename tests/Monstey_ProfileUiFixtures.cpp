#include "Monstey_RuntimeGateFixtures.inc"
#include "Monstey_FxWorkflow.hpp"
#include <iostream>
using namespace Monstey;
namespace {
std::unique_ptr<Monstey_Gadb> Monstey_FxFixture(){auto f=Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix());f->CreateCategory("NativeSchema","Native");
    std::array<std::string,2> names{"EffectA","EffectB"};std::sort(names.begin(),names.end(),[](const auto&a,const auto&b){return Monstey_NativeHash(a)<Monstey_NativeHash(b);});
    for(const auto&name:names){const auto ref=f->CreateRecord(0,name)["record"]["selector"].as_string();f->AddField(ref,"A",3,1,Monstey_Json::object({{"native_ordinal",0}}));f->AddField(ref,"B",3,2,Monstey_Json::object({{"native_ordinal",1}}));}return f;}
}
int wmain(int argc,wchar_t**argv){try{Monstey_Need(argc==2,"Need a new fixture directory");const std::filesystem::path out=argv[1];Monstey_Need(!std::filesystem::exists(out),"Fixture root exists");std::filesystem::create_directories(out);
    Monstey_WriteNewText(out/L"qualified-profile.json",Monstey_Reflection().dump(2));Monstey_WriteNewText(out/L"unknown-profile.json","{}\n");auto cases=Monstey_Json::array();
    for(const bool effects:{false,true})for(const std::string mode:{"qualified","unknown","unsafe","changed"}){
        auto f=Monstey_Create();if(effects)f.db=Monstey_FxFixture();if(mode=="unsafe")f.db->MoveField("0x00000001","B",0);
        const auto name=std::string(effects?"effects-":"database-")+mode;const auto project=out/Monstey_Wide(name+".project.json");f.db->SaveProject(project);
        const auto args=out/Monstey_Wide(name+".args.json");Monstey_WriteNewText(args,Monstey_Json::object({{"domain",effects?"effects":"database"},{"expect",mode},
            {"profile",Monstey_Utf8((out/(mode=="unknown"?L"unknown-profile.json":L"qualified-profile.json")).wstring())}}).dump(2));
        cases.as_array().push_back(Monstey_Json::object({{"name",name},{"project",Monstey_Utf8(project.wstring())},{"arguments",Monstey_Utf8(args.wstring())}}));}
    Monstey_WriteNewText(out/L"cases.json",cases.dump(2));std::cout<<cases.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
