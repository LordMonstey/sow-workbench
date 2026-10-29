#include <chrono>
#include "Monstey_Runtime.hpp"
#include "Monstey_ModelPreviewApi.hpp"
#include <algorithm>
#include <iostream>
using namespace Monstey;
int wmain(int argc,wchar_t** argv) {
 try {
  Monstey_Require(argc==3,"Usage RuntimePreviewTests NEW-directory synthetic.mesh");
  const std::filesystem::path directory=std::filesystem::path(argv[1]) / (L"run-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())),mesh=argv[2];Monstey_Require(!std::filesystem::exists(directory),"Directory exists");std::filesystem::create_directories(directory);
  const auto catalog=Monstey_Runtime::Catalog();unsigned found=0;
  for(const auto& row:catalog.as_array())if(row["name"].as_string()=="model_preview_render") {
   ++found;Monstey_Require(!row["read_only"].as_bool()&&row["implementation"].as_string()=="C++20","Runtime preview descriptor differs");
   Monstey_Require(row["inputSchema"]["required"]==Monstey_Json::array({"source","output"})&&row["inputSchema"]["properties"].contains("expected_generation"),"Runtime schema/guard optionality differs");
  }
  Monstey_Require(found==1,"Runtime preview catalog missing/duplicate");
  Monstey_Runtime runtime(directory/L"unused-schema.json",directory/L"settings.json");
  const auto output=directory/L"runtime.bmp";
  auto args=Monstey_Json::object({{"source",Monstey_Utf8(mesh.wstring())},{"output",Monstey_Utf8(output.wstring())},{"width",64},{"height",64},{"expected_generation",0},{"expected_revision",0}});
  const auto request=[&](const Monstey_Json& value){return Monstey_Json::object({{"operation","model_preview_render"},{"arguments",value}});};
  const auto sourceHash=Monstey_HashSha256(mesh);
  const auto success=runtime.Call(request(args));
  Monstey_Require(success["ok"].as_bool()&&success["revision"].as_uint()==1&&success["result"]["host_bind_geometry_only"].as_bool(),"Runtime route failed");
  Monstey_Require(Monstey_U16(Monstey_ReadFile(output),0)==0x4d42,"Runtime output is not BMP");
  const auto bad=directory/L"bad.bmp";args["output"]=Monstey_Utf8(bad.wstring());args["expected_revision"]=1;args["expected_source_sha256"]=std::string(64,'0');
  const auto rejected=runtime.Call(request(args));
  Monstey_Require(!rejected["ok"].as_bool()&&rejected["error"]["code"].as_string()=="source_hash_mismatch"&&rejected["revision"].as_uint()==1&&!std::filesystem::exists(bad),"Runtime hash rejection changed revision or published output");
  args.as_object().erase("expected_source_sha256");args["width"]="64";const auto wrongType=runtime.Call(request(args));
  Monstey_Require(!wrongType["ok"].as_bool()&&wrongType["error"]["code"].as_string()=="invalid_argument_type"&&!std::filesystem::exists(bad),"Runtime typed schema not enforced");
  Monstey_Require(Monstey_HashSha256(mesh)==sourceHash,"Runtime preview changed mesh");
  const auto report=Monstey_Json::object({{"schema","Monstey.ModelPreviewRuntimeTests/1"},{"catalog_operations",static_cast<std::uint64_t>(catalog.size())},{"preview_descriptors",found},{"groups",4},{"failed",0},{"success",success},{"hash_rejected",rejected},{"type_rejected",wrongType},{"source_unchanged",true}});
  Monstey_WriteNewText(directory/L"report.json",report.dump(2)+"\n");std::cout<<"Runtime preview4 groups PASS; catalog="<<catalog.size()<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
