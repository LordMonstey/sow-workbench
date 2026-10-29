#include "Monstey_GadbBanks.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include "Monstey_GadbStringPointer.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
template<class T>void Monstey_Put(std::vector<std::uint8_t>&b,std::size_t at,T value){std::memcpy(b.data()+at,&value,sizeof value);}
std::vector<std::uint8_t>Monstey_ParseHex(std::string_view s){std::vector<std::uint8_t>out;Monstey_Require((s.size()&1u)==0,"Odd metadata hex");auto nibble=[](char c)->unsigned{if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);if(c>='A'&&c<='F')return static_cast<unsigned>(c-'A'+10);throw std::runtime_error("Invalid metadata hex");};for(std::size_t i=0;i<s.size();i+=2)out.push_back(static_cast<std::uint8_t>((nibble(s[i])<<4)|nibble(s[i+1])));return out;}
}
int wmain(int argc,wchar_t**argv){try{
 Monstey_Require(argc==4,"Usage: IMAGE EXPECTED_SHA256 NEW_REPORT");const std::filesystem::path source=argv[1],out=argv[3];const auto expected=Monstey_Utf8(argv[2]);Monstey_RequireOfflineOutput(out,std::array{source});const auto image=Monstey_ReadFile(source,256ull*1024*1024);Monstey_Require(Monstey_HashSha256(image)==expected,"Source hash differs");
 const auto banks=Monstey_GadbBankRegistration(image);std::vector<std::uint64_t>pointers;std::set<std::uint64_t>targets;
 for(const auto&bank:banks.at("banks").as_array())for(const auto&row:bank.at("classes").as_array()){
  const auto target=std::stoull(row.at("descriptor_qword_plus_30").as_string(),nullptr,0);if(target){pointers.push_back(std::stoull(row.at("class_pointer").as_string(),nullptr,0));targets.insert(target);}}
 Monstey_Require(pointers.size()==34&&targets.size()==17,"Actual class30 bank identities differ");const auto declarations=Monstey_GadbInspectClassPointers(image,pointers,expected);Monstey_Require(declarations.at("classes").size()==39,"Actual39 identity count differs");std::uint64_t fields=0,strings=0,checks=0,unavailable=0;
 for(const auto&klass:declarations.at("classes").as_array()){
  if(klass.at("field_storage_status").as_string()=="UNAVAILABLE_FILE_RANGE")++unavailable;
  for(const auto&field:klass.at("fields").as_array()){
   const auto raw=Monstey_ParseHex(field.at("raw72_hex").as_string());Monstey_Require(raw.size()==72,"Raw field72 differs");std::uint32_t hash=0,type=0;std::memcpy(&hash,raw.data()+32,4);std::memcpy(&type,raw.data()+36,4);if((type&63u)==4u)++strings;++fields;
   for(unsigned layout=0;layout<2;++layout){std::vector<std::uint8_t>arena(512,0xa5);constexpr std::uint64_t base=0x100000000;std::copy(raw.begin(),raw.end(),arena.begin()+64);Monstey_Put(arena,0,std::uint8_t{1});Monstey_Put(arena,1,static_cast<std::uint8_t>(layout));Monstey_Put(arena,8,base+256);Monstey_Put(arena,16,base+384);Monstey_Put(arena,256+(layout?0:4),hash);Monstey_Put(arena,265,std::uint8_t{0});Monstey_Put(arena,266,static_cast<std::uint8_t>(type));Monstey_Put(arena,268,std::uint16_t{1});Monstey_Put(arena,270,std::uint16_t{0});constexpr std::uint64_t value=0xfeedcafed00dbeef,otherwise=0xdeadbeef0badc0de;Monstey_Put(arena,384,value);const auto prior=arena;Monstey_GadbBinderMemory memory{base,arena};Monstey_Require(Monstey_GadbFindRecordDescriptor(memory,base,base+96)==base+256,"Actual declaration hash did not resolve synthetic descriptor");Monstey_Require(Monstey_GadbReadStringPointer(memory,base,base+96,0,otherwise)==(((type&63u)==4u)?value:otherwise),"Actual declaration type did not select exact pointer/default");Monstey_Require(arena==prior,"Actual-backed synthetic record was mutated");checks+=3;}
  }
 }
 Monstey_Require(Monstey_HashSha256(source)==expected,"Source changed");auto report=Monstey_Json::object();report["status"]="PASS";report["source_sha256"]=expected;report["source_unchanged"]=true;report["registered_references"]=34;report["target_identities"]=17;report["classes_including_parents"]=39;report["raw_fields"]=static_cast<std::int64_t>(fields);report["type4_fields"]=static_cast<std::int64_t>(strings);report["virtual_only_field_classes"]=static_cast<std::int64_t>(unavailable);report["checks"]=static_cast<std::int64_t>(checks);report["scope"]="Actual fixed-profile PE bank+raw declaration reads. Each available raw field drives an explicitly synthetic owned16B runtime descriptor/value record, both layouts; no actual game.gamedb/runtime record or callback execution is claimed.";report["original_code_executed"]=false;Monstey_WriteNewText(out,report.dump(2)+"\n");std::cout<<"PASS39classes "<<fields<<" actual raw fields "<<checks<<" synthetic runtime-record checks\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
