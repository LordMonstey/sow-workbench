#include "Monstey_GadbCachedValue.hpp"
#include <array>
#include <iostream>
#include <random>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;using J=Monstey_Json;
U64 cases=0,checks=0,errors=0;unsigned groups=0;
void Check(bool p,const char*m){++checks;Monstey_Require(p,m);}
void Put16(Monstey_Bytes&b,std::size_t p,unsigned v){b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8);}
U64 Next(U64&x){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
void GetCase(const Monstey_GadbCachedRecordView*view,std::uint8_t p,U32 i,U64 fallback,bool reject,U64 expected){bool threw=false;U64 result=0;try{result=Monstey_GadbReadCachedQword(view,p,i,fallback);}catch(const Monstey_Error&){threw=true;}Check(threw==reject,"Consumed range/fallback partition differs");if(!threw)Check(result==expected,"Raw QWORD or fallback bits differ");else ++errors;++cases;}
void NullAndAllWords(){++groups;
 constexpr std::array<U64,8> bits{0,1,~U64{0},0x8000000000000000ull,0x7ff0000000000000ull,0x7ff8000012345678ull,0xfff00000fedcba98ull,0x0123456789abcdefull};
 for(unsigned p=0;p<256;++p)for(U64 f:bits)for(U32 i:{0u,1u,65534u,65535u,65536u,0x80000000u,0xffff0001u,UINT32_MAX})GetCase(nullptr,static_cast<std::uint8_t>(p),i,f,false,f);
 std::vector<U64> values(131072);U64 random=0x529b3bc823f11965ull;for(auto&v:values)v=Next(random);const auto before=values;Monstey_Bytes desc(4096,0x5a);
 for(U32 count=0;count<65536;++count){const auto p=static_cast<std::uint8_t>(count*73u);const auto offset=static_cast<std::uint16_t>(count*997u);const auto at=static_cast<std::size_t>(p)*16;Put16(desc,at+12,count);Put16(desc,at+14,offset);const Monstey_GadbCachedRecordView view{desc,values};
  for(U32 i:{0u,count/2,count?count-1:0u,count,count+65536u,UINT32_MAX}){const auto fallback=Next(random);const U64 expected=i<count?values[static_cast<std::size_t>(offset)+i]:fallback;GetCase(&view,p,i,fallback,false,expected);}
 }
 Check(values==before,"Read-only getter modified values");
}
void PrefixMatrix(){++groups;Monstey_Bytes all(4096,0xa5);std::vector<U64> values(65540);U64 random=0x85231091fdec8176ull;for(auto&v:values)v=Next(random);
 for(unsigned p=0;p<256;++p)for(unsigned count:{0u,1u,65535u})for(unsigned tail:{12u,13u,14u,15u,16u})for(U32 i:{0u,1u,65534u,65535u,65536u,0xffff0001u,UINT32_MAX})for(unsigned available:{0u,1u,65540u}){
  const auto at=static_cast<std::size_t>(p)*16;Put16(all,at+12,count);Put16(all,at+14,5);const auto before=all;const Monstey_GadbCachedRecordView view{std::span<const std::uint8_t>(all).first(at+tail),std::span<const U64>(values).first(available)};const U64 fallback=Next(random);
  const bool in_count=i<count;const bool reject=tail<14||(in_count&&(tail<16||5ull+i>=available));const U64 expected=in_count&&!reject?values[5ull+i]:fallback;GetCase(&view,static_cast<std::uint8_t>(p),i,fallback,reject,expected);Check(all==before,"Read-only getter modified descriptor/prefix bytes");
 }
}
J Audit(){const auto d=J::object({{"hash","0x5b57a121"},{"native_type",11},{"native_ordinal",5},{"count",1}});auto z=d;z["count"]=0;const auto row=J::object({{"category","Combat.Buff"},{"field","ConditionalBuff"},{"record","synthetic"},{"classification","unknown_value_contract"},{"physical_index",4},{"native_ordinal",5},{"mismatches",J::array({"count"})},{"first_descriptor",d},{"actual_descriptor",z},{"declaration",d}});return J::object({{"format","Monstey_GadbClassCacheAudit"},{"version",1},{"status","unknown"},{"database_sha256","synthetic"},{"diagnostics_truncated",false},{"totals",J::object({{"unqualified_value_contracts",120},{"unknown_categories",656},{"unsafe_categories",0},{"slot_hazards",0}})},{"diagnostics",J::array({row})}});}
void WitnessBoundaries(){++groups;
 for(unsigned change=0;change<25;++change){auto a=Audit();auto&row=a["diagnostics"][0];bool selected=true,qualified=false,fail=false;
  if(change==0)qualified=true;
  else if(change<=12){const std::array<const char*,3> owner{"first_descriptor","actual_descriptor","declaration"};const std::array<const char*,4> property{"hash","native_type","native_ordinal","count"};const auto n=change-1;const auto key=property[n%4];if(n%4==0)row[owner[n/4]][key]="0x5b57a122";else row[owner[n/4]][key]=row[owner[n/4]][key].as_uint()+1;}
  else if(change==13)row["physical_index"]=5;
  else if(change==14)row["native_ordinal"]=4;
  else if(change==15)row["mismatches"]=J::array({"type"});
  else if(change==16)row["mismatches"]=J::array({"count","type"});
  else if(change==17)row["classification"]="qualified";
  else if(change==18){row["category"]="Different";selected=false;}
  else if(change==19){row["field"]="Different";selected=false;}
  else if(change==20){a["diagnostics_truncated"]=true;fail=true;}
  else if(change==21){a["format"]="different";fail=true;}
  else if(change==22){a["version"]=2;fail=true;}
  else if(change==23){a["totals"]["unqualified_value_contracts"]=0;fail=true;}
  else{row["mismatches"]=J::array();}
  const auto before=a;bool threw=false;J out;try{out=Monstey_GadbConditionalBuffAbsence(a);}catch(const Monstey_Error&){threw=true;}Check(threw==fail,"Witness accept/reject boundary differs");Check(a==before,"Witness changed the original audit");if(!threw){Check(out["original_checker_status"]==a["status"]&&out["original_checker_totals"]==a["totals"],"Witness rewrote original counters/status");Check(out["selected_count_diagnostics"].as_uint()==(selected?1u:0u),"Witness changed selected scope");Check(out["qualified_selected_absences"].as_uint()==(qualified?1u:0u),"Witness qualified a wider field contract");Check(out["other_or_unqualified_contracts"].as_uint()==120-(qualified?1:0),"Witness removed original contracts");if(selected)Check(out["rows"][0]["original_classification"]==row["classification"],"Witness changed classification");}++cases;
 }
 // Reorder74 selected rows among46 unrelated diagnostics. The original120
 // diagnostics remain byte-for-byte unchanged; only a separate witness is added.
 auto a=Audit();const auto seed=a["diagnostics"][0];a["diagnostics"]=J::array();for(unsigned n=0;n<120;++n){auto row=seed;row["record"]="record"+std::to_string(n);if(n<46){row["category"]="Different";row["field"]="AnotherType11";}a["diagnostics"].as_array().push_back(row);}std::reverse(a["diagnostics"].as_array().begin(),a["diagnostics"].as_array().end());const auto before=a;const auto out=Monstey_GadbConditionalBuffAbsence(a);Check(a==before&&a["diagnostics"].size()==120,"Original120 diagnostics changed");Check(out["original_checker_totals"]==a["totals"]&&out["original_checker_status"].as_string()=="unknown","Original unknown/counters lost");Check(out["selected_count_diagnostics"].as_uint()==74&&out["qualified_selected_absences"].as_uint()==74&&out["other_or_unqualified_contracts"].as_uint()==46,"Selected74 witness totals differ");Check(out["requires_correct_physical_field_cache"].as_bool()&&!out["field_state_or_locale_simulated"].as_bool()&&!out["named_lookup_qualified"].as_bool()&&!out["all_gameplay_consumers_claimed"].as_bool(),"Conditional witness limits were promoted");++cases;
}
}
int wmain(int argc,wchar_t**argv){try{Monstey_Require(argc==2,"Usage: IndependentCachedValue NEW_REPORT");NullAndAllWords();PrefixMatrix();WitnessBoundaries();const auto report=J::object({{"schema","Monstey.IndependentCachedQwordReview/1"},{"status","PASS"},{"groups",groups},{"cases",cases},{"checks",checks},{"matched_consumed_errors",errors},{"all_U16_counts",65536},{"all_U8_caches",256},{"original_code_executed",false},{"private_input_required",false}});Monstey_WriteNewText(argv[1],report.dump(2)+"\n");std::cout<<report.dump()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"case "<<cases<<": "<<e.what()<<'\n';return 1;}}
