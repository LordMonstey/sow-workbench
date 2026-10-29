#include "Monstey_GadbRemainingContracts.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <random>
using namespace Monstey;
namespace {
using J = Monstey_Json;
std::uint64_t cases = 0, checks = 0, groups = 0;
void Check(bool value, const char* message) { ++checks; Monstey_Require(value, message); }
template<class F> void Reject(F f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } Check(rejected, "Consumed-invalid storage was accepted"); }
J Issue(const char* name = "AudioHookNextKey", unsigned ordinal = 20) {
 return J::object({{"category","AudioHook.Action"},{"field",name},{"native_ordinal",ordinal},{"physical_index",ordinal},
  {"classification","unknown_value_contract"},{"mismatches",J::array({"count"})},{"record","0x00100001"},{"first_record","0x00100000"},
  {"declaration",J::object({{"hash","0x223477a3"},{"native_type",11},{"native_ordinal",ordinal},{"count",1}})},
  {"first_descriptor",J::object({{"hash","0x223477a3"},{"native_type",11},{"native_ordinal",ordinal},{"count",1}})},
  {"actual_descriptor",J::object({{"hash","0x223477a3"},{"native_type",11},{"native_ordinal",ordinal},{"count",0}})}});
}
J Audit() {
 auto irrelevant = J::object({{"name","Unrelated duplicate"}});
 auto excluded = Issue(); excluded["category"]="Combat.Buff";excluded["field"]="ConditionalBuff";
 return J::object({{"format","Monstey_GadbClassCacheAudit"},{"version",1},{"diagnostics_truncated",false},{"status","unknown"},
  {"database_sha256",std::string(64,'a')},{"totals",J::object({{"unqualified_value_contracts",120},{"unknown_categories",656},{"cached_slot_hazards",0}})},
  {"categories",J::array({J::object({{"name","AudioHook.Action"},{"schema","AudioHooks/Actions"},{"native_entry",J::object({{"entry_ea","0x14264c660"}})}}),irrelevant,irrelevant})},
  {"diagnostics",J::array({Issue(),Issue("Other",21),excluded})}});
}
J Native() {
 auto field=J::object({{"declaration_index",20},{"name",J::object({{"status","FILE_BACKED"},{"value","AudioHookNextKey"}})},
  {"name_hash_u32","0x223477a3"},{"type_u32",11},{"count_u32",1},{"field_pointer","0x142863da0"},
  {"index_callback_pointer","0x141689c10"},{"captured_cache_byte_u8",0},{"captured_binding_state_u32",0}});
 auto other=field;other["declaration_index"]=21;other["name"]["value"]="Other";
 other["field_pointer"]="0x142863de8";other["index_callback_pointer"]="0x14168c204";
 return J::object({{"schema","Monstey.GadbExplicitClassDeclarations/1"},
  {"classes",J::array({J::object({{"class_pointer","0x14264c660"},{"fields",J::array({field,other})}})})}});
}
}
int wmain(int argc, wchar_t** argv) {
 try {
  Monstey_Require(argc==2,"Need new report path");
  const std::array<std::int32_t,8> states{INT32_MIN,-2,-1,0,1,2,3,INT32_MAX};
  for(auto state:states)for(unsigned cache=0;cache<256;++cache){
   auto result=Monstey_GadbProbeNextKeyAbsence(nullptr,state,static_cast<std::uint8_t>(cache));
   Check(result.gate==Monstey_NextKeyProbeGate::ReturnedZero&&result.raw_next_key==0&&!result.descriptor_count_consumed,"Null action did not return zero lazily");++cases;
  }++groups;
  Monstey_GadbCachedRecordView noStorage{{},{}};
  for(auto state:states){
   if(state<2){auto result=Monstey_GadbProbeNextKeyAbsence(&noStorage,state,255);Check(result.gate==Monstey_NextKeyProbeGate::NamedActionLookupRequired&&!result.descriptor_count_consumed,"Slow action gate touched storage or fabricated success");}
   else Reject([&]{(void)Monstey_GadbProbeNextKeyAbsence(&noStorage,state,255);});++cases;
  }++groups;
  for(unsigned cache=0;cache<256;++cache){
   std::vector<std::uint8_t> bytes(cache*16+14,0xff);bytes[cache*16+12]=0;bytes[cache*16+13]=0;
   Monstey_GadbCachedRecordView view{bytes,{}};
   auto result=Monstey_GadbProbeNextKeyAbsence(&view,2,static_cast<std::uint8_t>(cache));
   Check(result.gate==Monstey_NextKeyProbeGate::ReturnedZero&&result.raw_next_key==0&&result.descriptor_count_consumed,"Empty descriptor consumed missing offset/values or wrong physical byte");++cases;
  }++groups;
  std::mt19937_64 random(0x6d6f6e73746579ull);
  for(unsigned count=1;count<=65535;++count){
   const auto cache=static_cast<std::uint8_t>(random());const auto offset=static_cast<std::uint16_t>(random()%32);
   std::vector<std::uint8_t> bytes(static_cast<std::size_t>(cache)*16+16,0xad);
   const auto at=static_cast<std::size_t>(cache)*16;
   bytes[at+12]=static_cast<std::uint8_t>(count);bytes[at+13]=static_cast<std::uint8_t>(count>>8);
   bytes[at+14]=static_cast<std::uint8_t>(offset);bytes[at+15]=static_cast<std::uint8_t>(offset>>8);
   std::vector<std::uint64_t> values(offset+1);const auto word=count%7==0?0:random();values[offset]=word;
   Monstey_GadbCachedRecordView view{bytes,values};const auto result=Monstey_GadbProbeNextKeyAbsence(&view,INT32_MAX,cache);
   Check(result.raw_next_key==word&&result.descriptor_count_consumed,"Cached raw next-key word changed");
   Check(result.gate==(word==0?Monstey_NextKeyProbeGate::ReturnedZero:Monstey_NextKeyProbeGate::NonzeroNextKeyRequiresOwnedChild),"Nonzero child fabricated a pointer/return outcome");++cases;
  }++groups;
  std::array<std::uint8_t,16> bytes{};bytes[12]=1;bytes[14]=255;bytes[15]=255;
  std::vector<std::uint64_t> values(65536);values[65535]=0xfff8000000001234ull;
  Monstey_GadbCachedRecordView high{bytes,values};auto result=Monstey_GadbProbeNextKeyAbsence(&high,2,0);
  Check(result.raw_next_key==values[65535]&&result.gate==Monstey_NextKeyProbeGate::NonzeroNextKeyRequiresOwnedChild,"Full-width offset/raw opaque word changed");++cases;
  high.values={};Reject([&]{(void)Monstey_GadbProbeNextKeyAbsence(&high,2,0);});++cases;++groups;
  auto audit=Audit(),native=Native();auto out=Monstey_GadbInspectRemainingCountContracts(audit,native);
  Check(out.at("remaining_rows").as_uint()==2&&out.at("selected_cached_absence_rows").as_uint()==1&&out.at("excluded_ConditionalBuff_rows").as_uint()==1,"Inventory scope/grouping differs");
  Check(out.at("status")==audit.at("status")&&out.at("original_totals")==audit.at("totals"),"Original unknown classifications/counters were requalified");
  Check(out.at("rows").at(0).at("original_contract")==audit.at("diagnostics").at(0),"Original contract bytes changed");
  Check(out.at("rows").at(1).at("consumer").at("status")==J("NATIVE_CONSUMER_NOT_QUALIFIED"),"Another type11 field got blanket Optional semantics");++cases;++groups;
  for(unsigned which=0;which<12;++which){
   auto changed=audit;auto changedNative=native;
   if(which==0)changed["diagnostics"].at(0)["physical_index"]=19;
   if(which==1)changed["diagnostics"].at(0)["native_ordinal"]=19;
   if(which==2)changed["diagnostics"].at(0)["declaration"]["count"]=2;
   if(which==3)changed["diagnostics"].at(0)["actual_descriptor"]["count"]=1;
   if(which==4)changed["diagnostics"].at(0)["first_descriptor"]["native_type"]=10;
   if(which==5)changed["diagnostics"].at(0)["mismatches"].as_array().push_back("hash");
   auto& f=changedNative["classes"].at(0)["fields"].at(0);
   if(which==6)f["index_callback_pointer"]="0x141689c14";
   if(which==7)f["field_pointer"]="0x142863de8";
   if(which==8)f["count_u32"]=2;
   if(which==9)f["type_u32"]=10;
   if(which==10)f["name"]["value"]="Other";
   if(which==11)f["name"]["status"]="VIRTUAL_ONLY";
   auto rejected=Monstey_GadbInspectRemainingCountContracts(changed,changedNative);
   Check(rejected.at("selected_cached_absence_rows").as_uint()==0,"Wrong selected native/physical contract accepted");++cases;
  }++groups;
  auto duplicate=native;duplicate["classes"].as_array().push_back(duplicate["classes"].at(0));Reject([&]{(void)Monstey_GadbInspectRemainingCountContracts(audit,duplicate);});++cases;
  auto truncated=audit;truncated["diagnostics_truncated"]=true;Reject([&]{(void)Monstey_GadbInspectRemainingCountContracts(truncated,native);});++cases;
  auto missing=native;missing["classes"]=J::array();auto unresolved=Monstey_GadbInspectRemainingCountContracts(audit,missing);
  Check(unresolved.at("selected_cached_absence_rows").as_uint()==0&&unresolved.at("remaining_rows").as_uint()==2,"Missing native evidence was discarded or qualified");++cases;++groups;
  Monstey_WriteNewText(argv[1],J::object({{"pass",true},{"groups",groups},{"cases",cases},{"assertions",checks},{"synthetic_only",true}}).dump(2)+"\n");
  std::cout<<"PASS "<<groups<<" groups "<<cases<<" cases "<<checks<<" assertions\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
