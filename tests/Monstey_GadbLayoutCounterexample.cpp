#include "Monstey_FxWorkflow.hpp"
#include <filesystem>
#include <iostream>
using namespace Monstey;
namespace {
using J=Monstey_Json;
J Field(const char* name,unsigned type,J value,unsigned flags=0) {
 return J::object({{"name",name},{"type_code",type},{"default",value},{"array",1},{"flags",flags},{"ordinal",99}});
}
J Schema() {
 return J::object({{"categories",J::object({{"ClientFX.NativeLayout",J::object({{"field_count",6},{"unreadable_fields",0},{"flags","0x100000000"},{"fields",J::array({Field("KeyName",4,""),Field("StartTime",2,0.0),Field("BeginGroup",4,nullptr,0x80),Field("EndTime",2,1.0),Field("Value",3,8),Field("EndGroup",4,nullptr,0x200)})}})}})}});
}
std::string Element(Monstey_Gadb& db,const char* name) {
 return Monstey_FxCreateElement(db,J::object({{"name",name},{"class_name","ClientFX.NativeLayout"}}))["result"]["selector"].as_string();
}
J Run(const std::filesystem::path& out,const std::filesystem::path& schema,const char* name,unsigned mutation) {
 auto db=Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(),schema);
 const auto a=Element(*db,"Monstey_First"),b=Element(*db,"Monstey_Later");
 db->SetField(a,"Value",123); db->SetField(b,"Value",456);
 const auto before=Monstey_FxVerifyNativeBindings(*db);
 Monstey_Require(before["all_records_compatible"].as_bool(),"Control must start compatible");
 const auto first=before["categories"][0u]["first_record"].as_string();
 const auto later=first==a?b:a;
 if(mutation==1) db->MoveField(later,"Value",0);
 if(mutation==2) db->DeleteField(later,"Value");
 if(mutation==3) db->MoveField(first,"Value",0);
 const auto expected=Monstey_FxVerifyNativeBindings(*db);
 const auto dst=out/(std::string(name)+".fxp");
 const auto build=db->Build(dst);
 Monstey_Gadb reopened(dst,schema);
 const auto after=Monstey_FxVerifyNativeBindings(reopened);
 J row=J::object({{"case",name},{"before",before},{"requested",expected},{"build",build},{"published",after},{"engine_executed",false}});
 auto cache=J::array();
 const auto& cat=after["categories"][0u];
 const auto& firstRow=cat["records"][0u];
 for(const auto& record:cat["records"].as_array()) {
  const auto metadata=reopened.Metadata(record["record"].as_string());
  for(const auto& field:firstRow["fields"].as_array()) {
   if(!field["native_fast_predicates_pass"].as_bool()) continue;
   const auto i=static_cast<std::size_t>(field["physical_index"].as_uint());
   const bool present=i<metadata["fields"].size();
   cache.as_array().push_back(J::object({{"record",record["record"]},{"requested_field",field["field"]},{"physical_index",static_cast<std::uint64_t>(i)},{"inside_descriptor_array",present},{"actual_field",present?metadata["fields"][i]["name"]:J(nullptr)},{"actual_hash",present?metadata["fields"][i]["hash"]:J(nullptr)}}));
  }
 }
 row["modeled_cached_index_observations"]=cache;
 Monstey_WriteNewText(out/(std::string(name)+".json"),row.dump(2)+"\n");
 Monstey_Require(build["warnings"].empty(),"Unexpected writer warning");
 if(mutation==1||mutation==2) Monstey_Require(after["later_record_cache_conflicts"].as_uint()>0,"Counterexample lost conflict");
 if(mutation==0) Monstey_Require(after["all_records_compatible"].as_bool(),"Control changed");
 return J::object({{"case",name},{"writer_status",build["status"]},{"writer_warning_count",build["warnings"].size()},{"later_conflicts",after["later_record_cache_conflicts"]},{"all_compatible",after["all_records_compatible"]},{"sha256",build["sha256"]}});
}
}
int wmain(int argc,wchar_t** argv) {
 try {
  Monstey_Require(argc==2,"Pass new output directory"); const std::filesystem::path out(argv[1]);
  Monstey_Require(!std::filesystem::exists(out),"Output already exists"); std::filesystem::create_directories(out);
  const auto schema=out/L"synthetic-schema.json"; Monstey_WriteNewText(schema,Schema().dump(2));
  auto cases=J::array(); cases.as_array().push_back(Run(out,schema,"control",0)); cases.as_array().push_back(Run(out,schema,"later-reorder",1)); cases.as_array().push_back(Run(out,schema,"later-delete",2)); cases.as_array().push_back(Run(out,schema,"first-reorder",3));
  const auto report=J::object({{"status","CONFIRMED_BUILD_PUBLICATION_GAP"},{"cases",cases},{"originals_executed",false},{"source_or_game_modified",false}});
  Monstey_WriteNewText(out/L"report.json",report.dump(2)+"\n"); std::cout<<report.dump(2)<<"\n"; return 0;
 } catch(const std::exception& e) { std::cerr<<e.what()<<"\n";return 1; }
}
