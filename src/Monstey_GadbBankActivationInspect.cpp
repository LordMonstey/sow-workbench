#include "Monstey_GadbBankActivation.hpp"
#include "Monstey_Core.hpp"
#include <array>
#include <iostream>
using namespace Monstey;
namespace {
std::uint64_t Monstey_ProfileAddress(const Monstey_Json &v){return std::stoull(v.as_string(),nullptr,16);}
}
int wmain(int argc,wchar_t**argv){try{
 Monstey_Require(argc==6,"Usage: BankActivationInspect PE PE_SHA REFERENCE_JSON REFERENCE_SHA NEW_REPORT");
 const std::filesystem::path imagePath(argv[1]),referencePath(argv[3]),output(argv[5]);const std::array inputs{imagePath,referencePath};
 Monstey_RequireOfflineOutput(output,inputs);Monstey_Require(!std::filesystem::exists(output),"New report required");
 const auto image=Monstey_ReadFile(imagePath),referenceBytes=Monstey_ReadFile(referencePath);
 const auto imageSha=Monstey_HashSha256(image),referenceSha=Monstey_HashSha256(referenceBytes);
 Monstey_Require(imageSha==Monstey_Utf8(argv[2])&&referenceSha==Monstey_Utf8(argv[4]),"Pinned source differs");
 const auto recipes=Monstey_GadbActivationRecipes(image);const auto reference=Monstey_Json::Parse(std::string(referenceBytes.begin(),referenceBytes.end()));
 const auto&banks=reference.at("banks").as_array();Monstey_Require(banks.size()==6,"Expected six reference banks");
 Monstey_Json::Array rows;std::size_t classes=0;
 for(std::size_t i=0;i<6;++i){const auto&r=recipes[i];const auto&b=banks[i];const auto&refs=b.at("classes").as_array();
  Monstey_Require(r.enumerator==Monstey_ProfileAddress(b.at("enumerator"))&&r.returned_array==Monstey_ProfileAddress(b.at("returned_array"))&&r.returned_count==b.at("returned_count").as_uint()&&r.constructor_classes.size()==refs.size(),"Native bank metadata differs");
  std::vector<std::uint8_t> constants;for(std::size_t j=0;j<refs.size();++j){Monstey_Require(r.constructor_classes[j]==Monstey_ProfileAddress(refs[j].at("class_pointer")),"Ordered constructor constant differs");for(unsigned n=0;n<8;++n)constants.push_back(static_cast<std::uint8_t>(r.constructor_classes[j]>>(8*n)));++classes;}
  rows.push_back(Monstey_Json::object({{"bank_index",i},{"class_count",r.returned_count},{"enumerator",r.enumerator},{"guard_identity",r.guard_identity},{"returned_array",r.returned_array},{"ordered_constants_sha256",Monstey_HashSha256(constants)}}));
 }
 Monstey_Require(classes==3151,"Expected3151 ordered constructor constants");
 Monstey_Require(Monstey_HashSha256(imagePath)==imageSha&&Monstey_HashSha256(referencePath)==referenceSha,"Source changed");
 Monstey_WriteNewText(output,Monstey_Json::object({{"schema","Monstey.GadbBankActivationRecipes/1"},{"passed",true},{"banks",std::move(rows)},{"ordered_class_constants",classes},{"PE_sha256",imageSha},{"reference_sha256",referenceSha},{"sources_unchanged",true},{"original_code_executed",false},{"live_guard_states_observed",false},{"field_states_reset",false},{"actual_runtime_BSS_inferred",false}}).dump(2)+"\n");
 std::cout<<"PASS six native recipes/3151 constants; no guard or live state inferred\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
