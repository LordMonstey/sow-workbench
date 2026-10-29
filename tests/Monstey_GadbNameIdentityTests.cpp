#include "Monstey_Gadb.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B=Monstey_Bytes;using J=Monstey_Json;
B Fixture(){
 B pool(4);auto text=[&](const std::string& s){const auto at=static_cast<std::uint32_t>(pool.size());pool.insert(pool.end(),s.begin(),s.end());pool.push_back(0);return at;};
 const auto schema=text("Names"),name=text("Names"),path=text("");
 std::array<std::string,2> names{std::string("\xff",1)+"A","Plain"};
 std::sort(names.begin(),names.end(),[](const auto& a,const auto& b){return Monstey_NativeHash(a)<Monstey_NativeHash(b);});
 std::array<std::uint32_t,2> offsets{};for(std::size_t i=0;i<names.size();++i)offsets[i]=text(names[i]);
 B records;for(auto word:{schema,name,path,1u,2u,Monstey_NativeHash("Names")})Monstey_AppendU32(records,word);
 Monstey_AppendU32(records,0);Monstey_AppendU32(records,0);
 for(auto offset:offsets){Monstey_AppendU32(records,offset);Monstey_AppendU32(records,0);Monstey_AppendU32(records,0x08000000);}
 B b{'G','A','D','B'};for(auto word:{17u,static_cast<std::uint32_t>(pool.size()),1u,2u,1u,0u,0u,0u,0u,44u})Monstey_AppendU32(b,word);
 const std::string filename="names.gamedb";Monstey_AppendU16(b,static_cast<std::uint16_t>(filename.size()));b.insert(b.end(),filename.begin(),filename.end());b.insert(b.end(),pool.begin(),pool.end());Monstey_AppendU32(b,0);Monstey_AppendU32(b,1);b.insert(b.end(),records.begin(),records.end());Monstey_AppendU32(b,0);return b;
}
}
int wmain(int argc,wchar_t** argv){try{
 Monstey_Require(argc==2,"Need new report path");const std::filesystem::path report=argv[1];const auto dir=report.parent_path();const auto file=dir/L"names.gamedb";const auto raw=Fixture();Monstey_WriteNewFile(file,raw);Monstey_Gadb db(file);auto tests=J::array();
 auto check=[&](bool value,const char* label){Monstey_Require(value,label);tests.as_array().push_back(label);};
 const auto page=db.Records(0,"",0,10);check(page.at("total").as_uint()==3,"native named/anonymous pagination retained");
 const auto bad=std::string("\xff",1)+"A";const auto hash=Monstey_Hex(Monstey_NativeHash(bad));bool found=false;
 for(const auto& row:page.at("rows").as_array()){
   const auto metadata=db.Metadata(row.at("native_reference").as_string());
   check(row.at("name_hash")==metadata.at("name_hash"),"record page and metadata native hashes agree");
   if(!row.at("named").as_bool()){check(row.at("name_hash").is_null(),"anonymous record has no invented name hash");continue;}
   if(row.at("name_hash").as_string()==hash){found=true;check(row.at("name").as_string()!=bad&&Monstey_Hex(Monstey_NativeHash(row.at("name").as_string()))!=hash,"escaped display text cannot substitute native name identity");}
 }
 check(found,"nonUTF8 native named record identity remains accessible");
 const auto row=db.Records(0,"",0,1).at("rows").at(0);check(row.at("name_hash").is_null(),"paged anonymous identity remains null");
 const auto unchanged=db.Build(dir/L"identity.gamedb");check(Monstey_ReadFile(dir/L"identity.gamedb")==raw&&unchanged.at("sha256").as_string()==Monstey_HashSha256(file),"inspection hash metadata does not alter native bytes");
 std::string named;for(const auto& r:page.at("rows").as_array())if(r.at("named").as_bool()){named=r.at("native_reference").as_string();break;}
 const auto cloned=db.CloneRecord(named,"IdentityClone");(void)cloned;const auto clone=db.Metadata("Monstey_IdentityClone");check(clone.at("name_hash").as_string()==Monstey_Hex(Monstey_NativeHash("Monstey_IdentityClone")),"pending clone hash follows effective native name");
 const auto result=J::object({{"status","PASS"},{"checks",tests.size()},{"tests",tests},{"native_source_unchanged",Monstey_ReadFile(file)==raw},{"game_written",false}});Monstey_WriteNewText(report,result.dump(2)+"\n");std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
