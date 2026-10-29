#include "Monstey_FearConstraintTable.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_FearDbPrimitives.hpp"
#include "Monstey_Json.hpp"
#include <iostream>
#include <map>
#include <string>
namespace {
std::string Monstey_KeyHex(const std::string& key){constexpr char digits[]="0123456789abcdef";std::string value;for(unsigned char c:key){value.push_back(digits[c>>4]);value.push_back(digits[c&15]);}return value;}
}
int wmain(int argc,wchar_t** argv) {
 using namespace Monstey;
 auto out=Monstey_Json::object({{"schema","Monstey.FearConstraintInspect/1"},{"original_native_entrypoint_invoked",false},{"original_STL_ABI_qualified",false}});
 try {
  std::filesystem::path source;bool list=false;
  for(int i=1;i<argc;++i){const std::wstring flag=argv[i];if(flag==L"--help"){std::cout<<"Monstey_FearConstraintInspect --input DBConstraints.dat [--summary | --list]\n";return 0;}if(flag==L"--list"){list=true;continue;}if(flag==L"--summary"){list=false;continue;}if(flag!=L"--input"||++i>=argc)throw std::invalid_argument("Use --input FILE and optional --summary or --list");source=argv[i];}
  if(source.empty())throw std::invalid_argument("--input is required");source=std::filesystem::absolute(Monstey_WindowsFilePath(source));
  const auto before=Monstey_HashSha256(source);const auto bytes=Monstey_ReadFile(source);if(Monstey_HashSha256(bytes)!=before)throw std::runtime_error("Input changed while reading");
  const auto table=Monstey_FearDecodeConstraints(bytes);
  out["source_path"]=Monstey_Utf8(source.wstring());out["source_sha256"]=before;out["source_bytes"]=static_cast<std::uint64_t>(bytes.size());out["complete"]=table.complete;out["consumed_bytes"]=table.consumed;out["read_calls"]=table.read_calls;out["records"]=table.records;out["unique_keys"]=static_cast<std::uint64_t>(table.rows.size());out["duplicate_records"]=table.records-table.rows.size();
  std::map<std::uint32_t,std::uint64_t> histogram;for(const auto& row:table.rows)++histogram[row.type];auto types=Monstey_Json::array();for(const auto& [type,count]:histogram)types.as_array().push_back(Monstey_Json::object({{"type",type},{"unique_keys",count}}));out["types"]=std::move(types);
  if(list){auto rows=Monstey_Json::array();for(const auto& row:table.rows)rows.as_array().push_back(Monstey_Json::object({{"key_bytes_hex",Monstey_KeyHex(row.key)},{"type",row.type},{"word4_bits",Monstey_Hex(row.word4)},{"word8_bits",Monstey_Hex(row.word8)}}));out["rows"]=std::move(rows);}
  out["source_unchanged"]=Monstey_HashSha256(source)==before;if(!out["source_unchanged"].as_bool())throw std::runtime_error("Input changed while inspecting");out["ok"]=table.complete;std::cout<<out.dump(2)<<'\n';return table.complete?0:1;
 }catch(const Monstey_FearDbFault& e){out["ok"]=false;out["error"]=e.what();out["fault_identity"]=Monstey_Hex(e.address);out["fault_width"]=e.width;out["fault_writing"]=e.writing;}
 catch(const std::exception& e){out["ok"]=false;out["error"]=e.what();}
 std::cout<<out.dump(2)<<'\n';return 2;
}
