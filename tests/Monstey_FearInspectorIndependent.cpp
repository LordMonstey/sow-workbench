#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include "Monstey_Core.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
using namespace Monstey;
namespace {
std::uint64_t Monstey_Checks=0,Monstey_Cases=0;
Monstey_Json Monstey_Findings=Monstey_Json::array();
void Monstey_Check(bool ok,const char* why){++Monstey_Checks;if(!ok)throw std::runtime_error(why);}
struct Monstey_Handle{HANDLE value=INVALID_HANDLE_VALUE;~Monstey_Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::wstring Monstey_Quote(std::wstring_view value){std::wstring out=L"\"";std::size_t slashes=0;for(auto c:value){if(c==L'\\'){++slashes;continue;}out.append(slashes*(c==L'\"'?2u:1u)+(c==L'\"'?1u:0u),L'\\');slashes=0;out+=c;}out.append(slashes*2,L'\\');return out+L'\"';}
struct Monstey_Result{DWORD code;Monstey_Json json;std::string bytes;};
Monstey_Result Monstey_Run(const std::filesystem::path& exe,const std::filesystem::path& root,const std::vector<std::wstring>& args){
 ++Monstey_Cases;const auto stem=L"case-"+std::to_wstring(Monstey_Cases);
 const auto out=root/(stem+L".json"),err=root/(stem+L".stderr");SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
 Monstey_Handle oh{CreateFileW(out.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)},eh{CreateFileW(err.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};
 Monstey_Check(oh.value!=INVALID_HANDLE_VALUE&&eh.value!=INVALID_HANDLE_VALUE,"owned stdout/stderr creation");
 auto command=Monstey_Quote(exe.wstring());for(const auto& a:args)command+=L" "+Monstey_Quote(a);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=oh.value;startup.hStdError=eh.value;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION process{};
 Monstey_Check(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"owned CLI creation");
 Monstey_Handle ph{process.hProcess},th{process.hThread};if(WaitForSingleObject(ph.value,30000)!=WAIT_OBJECT_0){TerminateProcess(ph.value,99);throw std::runtime_error("owned CLI timeout");}
 DWORD code=99;Monstey_Check(GetExitCodeProcess(ph.value,&code)!=FALSE,"owned CLI exit read");const auto text=Monstey_ReadText(out);
 Monstey_Check(Monstey_ReadText(err).empty(),"CLI errors belong to JSON stdout");Monstey_Check(!text.empty()&&text.back()=='\n'&&text.find('\r')==std::string::npos,"CLI UTF8 LF bytes");
 return{code,Monstey_Json::Parse(text),text};
}
struct Monstey_Attribute{std::uint32_t type,usage;std::vector<std::uint32_t> words;};
struct Monstey_Fixture{
 Monstey_Bytes pool{0,'s','a','m','e',0};std::vector<Monstey_Attribute> attributes;std::uint32_t categories=3;
 std::uint32_t add(std::initializer_list<std::uint8_t> bytes){const auto at=static_cast<std::uint32_t>(pool.size());pool.insert(pool.end(),bytes);return at;}
 Monstey_Bytes bytes()const{
  Monstey_Bytes out;std::uint32_t values=0;for(const auto&a:attributes)values+=static_cast<std::uint32_t>(a.words.size());
  for(auto word:std::array<std::uint32_t,7>{0x42444147u,3u,static_cast<std::uint32_t>(pool.size()),categories,categories,static_cast<std::uint32_t>(attributes.size()),values})Monstey_AppendU32(out,word);
  out.insert(out.end(),pool.begin(),pool.end());Monstey_AppendU32(out,categories);
  for(std::uint32_t c=0;c<categories;++c){Monstey_AppendU32(out,1);Monstey_AppendU32(out,1);Monstey_AppendU32(out,1);Monstey_AppendU32(out,c==0?static_cast<std::uint32_t>(attributes.size()):0);
   if(c==0)for(const auto&a:attributes){for(auto word:std::array<std::uint32_t,4>{1,a.type,a.usage,static_cast<std::uint32_t>(a.words.size())})Monstey_AppendU32(out,word);for(auto word:a.words)Monstey_AppendU32(out,word);}}
  return out;
 }
};
void Monstey_Success(const Monstey_Result&r){Monstey_Check(r.code==0&&r.json["ok"].as_bool(),"CLI success");Monstey_Check(r.json["schema"].as_string()=="Monstey.FearDbInspection/1"&&r.json["source_unchanged"].as_bool(),"inspection schema and immutability");}
}
int wmain(int argc,wchar_t**argv){try{
 Monstey_Check(argc==3,"IndependentInspector <frozen CLI> <NEW synthetic directory>");const std::filesystem::path exe=argv[1],root=argv[2];Monstey_Check(std::filesystem::create_directory(root),"new independent output directory");
 Monstey_Fixture f;const auto raw=f.add({0x22,0x5c,0x09,0x01,0xff,0}),wide=f.add({0x00,0xd8,0,0});
 const auto vector=static_cast<std::uint32_t>(f.pool.size());for(auto bits:std::array<std::uint32_t,4>{0x00000001,0xff800001,0x80000000,0x7f7fffff})Monstey_AppendU32(f.pool,bits);
 const auto pool_size=static_cast<std::uint32_t>(f.pool.size());
 const std::vector<std::uint32_t> float_bits{0,0x80000000,1,0x80000001,0x00800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7f800001,0xff800001,0x7fc12345};
 f.attributes={{1,0xdeadbeef,{0x44332280,0x88776655,0xccbbaa99,0x00ffeedd}},{2,0,float_bits},{3,0,{0x80000000,0xffffffff,0,0x7fffffff}},{4,0,{raw,pool_size,pool_size+30u,0x7ffffff0u}},{5,0,{wide}},{8,0,{vector,pool_size-3u,pool_size+20u,0x70000000}},{9,0,{0,0x00020000,0x00030000,0x00000001}},{0x104,0xabcdef23,{0x24000000u+raw}},{0,0,{0xdeadbeef}},{0xff,0,{0xcafebabe}},{0x109,0,{0x00010000}},{3,0,{}}};
 const auto input=root/L"independent-\u03A9-\u6F22-\U0001F9EA.gadb";auto source=f.bytes();source.insert(source.end(),{0xa5,0x5a,0x33,0xcc});Monstey_WriteNewFile(input,source);const auto initial_hash=Monstey_HashSha256(input);
 auto run=[&](std::vector<std::wstring>args){args.insert(args.begin(),input.wstring());args.insert(args.begin(),L"--input");return Monstey_Run(exe,root,args);};
 auto values=[&](unsigned a,std::vector<std::wstring>tail=std::vector<std::wstring>{}){std::vector<std::wstring>args{L"--category",L"0",L"--record",L"0",L"--attribute",std::to_wstring(a)};args.insert(args.end(),tail.begin(),tail.end());auto r=run(args);Monstey_Success(r);return r;};
 for(std::uint32_t offset:std::array<std::uint32_t,6>{0,1,2,3,4,0xffffffffu})for(std::uint32_t limit:std::array<std::uint32_t,3>{1,2,10000}){
  auto r=run({L"--offset",std::to_wstring(offset),L"--limit",std::to_wstring(limit)});Monstey_Success(r);const auto begin=std::min(offset,3u),end=std::min(begin+limit,3u);const auto&p=r.json["inspection"]["page"];
  Monstey_Check(p["total"].as_uint()==3&&p["offset_requested"].as_uint()==offset&&p["offset_emitted"].as_uint()==begin&&p["emitted"].as_uint()==end-begin,"independent pagination matrix");
  Monstey_Check(p["has_more"].as_bool()==(end<3)&&((end<3&&p["next_offset"].as_uint()==end)||(end==3&&p["next_offset"].is_null())),"next offset and terminal page");
  Monstey_Check(r.json["source_sha256"].as_string()==initial_hash&&r.json["consumed_bytes"].as_uint()==source.size()-4&&r.json["trailing_bytes"].as_uint()==4,"exact source provenance and trailing bytes");
  const auto&items=r.json["inspection"]["items"];for(std::uint32_t i=begin;i<end;++i)Monstey_Check(items[i-begin]["ordinal"].as_uint()==i&&items[i-begin]["name"]["bytes_hex"].as_string()=="73616d65","duplicates preserve ordinal identity");
 }
 auto r=values(0);for(unsigned i=0;i<4;++i){const auto&v=r.json["inspection"]["items"][i];Monstey_Check(v["byte_u8"].as_uint()==std::array<unsigned,4>{128,0x55,0x99,0xdd}[i],"accepted bool getter preserves each word low byte");Monstey_Check(v["runtime_word_u32"].as_uint()==f.attributes[0].words[i],"complete runtime bool word retained independently");}
 Monstey_Check(r.json["inspection"]["selected_attribute"]["runtime_usage_u8"].as_uint()==0xef,"usage u8 narrowing");
 r=values(1);for(std::size_t i=0;i<float_bits.size();++i){const auto&v=r.json["inspection"]["items"][i];Monstey_Check(v["runtime_word_u32"].as_uint()==float_bits[i]&&v["float32"]["bits_u32"].as_uint()==float_bits[i],"raw F32 bits including signalling NaN and subnormal");const bool nonfinite=(float_bits[i]&0x7f800000u)==0x7f800000u;Monstey_Check(v["float32"]["value"].is_null()==nonfinite,"finite JSON value versus explicit nonfinite null");}
 r=values(2);for(unsigned i=0;i<4;++i)Monstey_Check(r.json["inspection"]["items"][i]["signed_value"].as_int()==std::array<std::int64_t,4>{INT32_MIN,-1,0,INT32_MAX}[i],"I32 signed endpoints");
 r=values(3);const auto&strings=r.json["inspection"]["items"];Monstey_Check(strings[0]["runtime_word_u32"].as_uint()==0x24000000u+raw&&strings[0]["string"]["bytes_hex"].as_string()=="225c0901ff","relocated runtime word and raw nonlocale byte string");
 Monstey_Check(strings[1]["string"]["status"].as_string()=="outside_pool_not_decoded"&&strings[1]["string"]["identity"]["pointer_region"].as_string()=="runtime_graph_or_diagnostic_caller_name","pool boundary runtime graph label");
 Monstey_Check(strings[2]["string"]["status"].as_string()=="outside_pool_not_decoded"&&strings[3]["string"]["status"].as_string()=="unavailable_outside_owned_allocation","inside owned versus outside owned string labels");
 r=values(4);Monstey_Check(r.json["inspection"]["items"][0]["string"]["bytes_hex"].as_string()=="00d8"&&r.json["inspection"]["items"][0]["string"]["status"].as_string()=="terminated","unpaired UTF16 code unit preserved without conversion");
 r=values(5);const auto&vectors=r.json["inspection"]["items"];Monstey_Check(vectors[0]["components"][1]["bits_u32"].as_uint()==0xff800001&&vectors[0]["components"].size()==4,"vector pool raw F32 components");Monstey_Check(vectors[1]["status"].as_string()=="pool_span_crosses_boundary_not_decoded"&&vectors[2]["status"].as_string()=="outside_pool_not_decoded"&&vectors[3]["status"].as_string()=="unavailable_outside_owned_allocation","three vector boundary distinctions");
 r=values(6);const auto&links=r.json["inspection"]["items"];Monstey_Check(links[0]["target_category_ordinal"].as_uint()==0&&links[1]["target_category_ordinal"].as_uint()==2,"link ordinals resolved after fixup");for(unsigned i=2;i<4;++i)Monstey_Check(links[i]["runtime_word_u32"].as_uint()==0&&links[i]["status"].as_string()=="null_after_loader_fixup_original_target_unavailable","invalid physical link bits unavailable after runtime fixup");
 r=values(7);Monstey_Check(r.json["inspection"]["selected_attribute"]["runtime_type_u8"].as_uint()==4&&r.json["inspection"]["selected_attribute"]["runtime_usage_u8"].as_uint()==0x23&&r.json["inspection"]["items"][0]["runtime_word_u32"].as_uint()==0x24000000u+raw&&r.json["inspection"]["items"][0]["string"]["bytes_hex"].as_string()=="225c0901ff","full physical type outside4..8 is not relocated, low u8 still selects typed getter");
 for(unsigned a=8;a<=9;++a){r=values(a);Monstey_Check(r.json["inspection"]["items"][0]["kind"].as_string()=="unknown_runtime_type"&&r.json["inspection"]["items"][0]["runtime_word_u32"].as_uint()==f.attributes[a].words[0],"unknown full raw runtime word labels");}
 r=values(10);Monstey_Check(r.json["inspection"]["selected_attribute"]["runtime_type_u8"].as_uint()==9&&r.json["inspection"]["items"][0]["target_category_ordinal"].as_uint()==1,"link fixup keyed on narrowed runtime type9");r=values(11);Monstey_Check(r.json["inspection"]["page"]["total"].as_uint()==0&&r.json["inspection"]["items"].empty(),"empty selected attribute");
 const auto output=root/L"output-\u6F22.json";r=run({L"--summary",L"--output",output.wstring()});Monstey_Success(r);Monstey_Check(Monstey_ReadText(output)==r.bytes&&!r.json["inspection"].contains("items"),"Unicode output UTF8/LF exact and summary no items");const auto output_hash=Monstey_HashSha256(output);
 for(const auto&alias:std::array<std::filesystem::path,3>{output,input,root/L"sub"/L".."/input.filename()}){r=run({L"--output",alias.wstring(),L"--summary"});Monstey_Check(r.code==1&&!r.json["ok"].as_bool()&&Monstey_HashSha256(input)==initial_hash&&Monstey_HashSha256(output)==output_hash,"existing output/source/canonical alias unchanged");}
 const auto hardlink=root/L"source-hardlink.gadb";Monstey_Check(CreateHardLinkW(hardlink.c_str(),input.c_str(),nullptr)!=FALSE,"owned hardlink fixture creation");r=run({L"--summary",L"--output",hardlink.wstring()});Monstey_Check(r.code==1&&Monstey_HashSha256(input)==initial_hash,"hardlink alias cannot overwrite input");
 for(const auto&args:std::vector<std::vector<std::wstring>>{{},{L"--input"},{L"--help",L"--input",input.wstring()},{L"--input",input.wstring(),L"--summary",L"--summary"},{L"--input",input.wstring(),L"--limit",L"+1"},{L"--input",input.wstring(),L"--offset",L"4294967296"},{L"--input",input.wstring(),L"--string-bytes",L"1"},{L"--input",input.wstring(),L"--string-bytes",L"1048577"},{L"--input",input.wstring(),L"--attribute",L"0"},{L"--input",input.wstring(),L"--record",L"0"},{L"--input",input.wstring(),L"--summary",L"--category",L"0"}}){r=Monstey_Run(exe,root,args);Monstey_Check(r.code==2&&!r.json["ok"].as_bool()&&r.json.contains("error"),"invalid command JSON exit2");}
 for(const auto&args:std::vector<std::vector<std::wstring>>{{L"--category",L"3"},{L"--category",L"0",L"--record",L"1"},{L"--category",L"0",L"--record",L"0",L"--attribute",L"12"}}){r=run(args);Monstey_Check(r.code==1&&!r.json["ok"].as_bool(),"invalid ordinal JSON exit1");}
 r=Monstey_Run(exe,root,{L"--help"});Monstey_Check(r.code==0&&r.json["schema"].as_string()=="Monstey.FearDbInspectorHelp/1","JSON help schema");
 Monstey_Fixture narrow;narrow.categories=1;narrow.attributes={{3,0,std::vector<std::uint32_t>(65537,0x11223344)}};const auto narrow_path=root/L"u16-wrap-plus-one.gadb";const auto narrow_bytes=narrow.bytes();Monstey_WriteNewFile(narrow_path,narrow_bytes);r=Monstey_Run(exe,root,{L"--input",narrow_path.wstring(),L"--category",L"0",L"--record",L"0",L"--attribute",L"0"});Monstey_Success(r);Monstey_Check(r.json["consumed_bytes"].as_uint()==narrow_bytes.size()&&r.json["inspection"]["page"]["total"].as_uint()==1&&r.json["inspection"]["items"].size()==1,"65537 physical values consumed, only one runtime visible value");
 for(const auto cut:std::array<std::size_t,8>{0,1,27,28,28+f.pool.size()-1,28+f.pool.size(),28+f.pool.size()+4,source.size()-5}){const auto path=root/(L"cut-"+std::to_wstring(cut)+L".gadb");Monstey_WriteNewFile(path,std::span(source).first(cut));r=Monstey_Run(exe,root,{L"--input",path.wstring()});Monstey_Check(r.code==1&&!r.json["ok"].as_bool()&&r.json.contains("consumed_bytes"),"bounded truncated fixture gives JSON consumed boundary");}
 // Permanent D001 repro: pool has one complete nonzero UTF16 unit and one odd
 // final byte. With cap >= remaining pool bytes, only pool end limits decoding.
 Monstey_Fixture odd;odd.categories=1;const auto odd_at=odd.add({0x41,0,0x7f});odd.attributes={{5,0,{odd_at}}};const auto odd_path=root/L"D001-odd-wide-pool.gadb";Monstey_WriteNewFile(odd_path,odd.bytes());
 for(unsigned cap:std::array<unsigned,3>{2,3,256}){r=Monstey_Run(exe,root,{L"--input",odd_path.wstring(),L"--category",L"0",L"--record",L"0",L"--attribute",L"0",L"--string-bytes",std::to_wstring(cap)});Monstey_Success(r);const auto&s=r.json["inspection"]["items"][0]["string"];Monstey_Check(s["bytes_hex"].as_string()=="4100"&&!s["terminator_observed"].as_bool(),"odd-wide complete units and absent terminator");const std::string expected=cap==2?"truncated_at_display_limit":"unavailable_terminator_in_pool";const auto actual=s["status"].as_string();++Monstey_Checks;if(actual!=expected)Monstey_Findings.as_array().push_back(Monstey_Json::object({{"id","D001"},{"case",Monstey_Cases},{"cap",cap},{"remaining_pool_bytes",3},{"expected",expected},{"actual",actual},{"source","src/Monstey_FearDbInspector.cpp:maximum/status in Monstey_PoolString"}}));}
 Monstey_Check(Monstey_HashSha256(input)==initial_hash,"independent source unchanged at teardown");
 const auto report=Monstey_Json::object({{"schema","Monstey.FearInspectorIndependentCli/1"},{"cli_cases",Monstey_Cases},{"checks",Monstey_Checks},{"non_discrepancy_checks_passed",true},{"candidate_accepted",Monstey_Findings.empty()},{"findings",Monstey_Findings},{"input_assets","C++ synthetic fixtures only"},{"original_native_entrypoint_invoked",false}});Monstey_WriteNewText(root/L"independent-result.json",report.dump(2)+"\n");std::cout<<report.dump(2)<<'\n';return Monstey_Findings.empty()?0:1;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
