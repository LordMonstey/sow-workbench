#include "Monstey_Core.hpp"
#include <filesystem>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
using namespace Monstey;
namespace {
unsigned Monstey_Count=0;
void Monstey_Check(bool value,const char* label){if(!value)throw std::runtime_error(label);++Monstey_Count;}
void Monstey_Reject(const std::function<void()>& f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}Monstey_Check(failed,"Expected rejection");}
}
int wmain(int argc,wchar_t** argv){
 try {
 const auto root=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/L"Monstey-Cpp-CoreTests";std::filesystem::create_directories(root);
 const auto j=Monstey_Json::Parse(R"({"a":[true,false,null,-4,2.5,1e2],"text":"A\u03a9\ud83d\udd25","empty":{}})");
 Monstey_Check(j.at("a").size()==6,"Array");Monstey_Check(j.at("a").at(3).as_int()==-4,"Int");
 Monstey_Check(j.at("text").as_string()==Monstey_Utf8(L"A\u03a9\U0001f525"),"Surrogates");
 Monstey_Check(Monstey_Json::Parse(j.dump(2))==j,"Roundtrip");
 Monstey_Check(Monstey_Json::Parse("\xef\xbb\xbf{\"x\":1}").at("x").as_int()==1,"BOM");
 for(const auto* invalid:{"","{\"x\":1,\"x\":2}","[1,]","{\"x\":1,}","01","NaN","Infinity","1e999","\"\\ud800\"","\"\\udc00\"","9223372036854775808","true false","\"\\x01\""})
  Monstey_Reject([&]{(void)Monstey_Json::Parse(invalid);});
 Monstey_Reject([]{(void)Monstey_Json::Parse(std::string("\"")+static_cast<char>(-1)+"\"");});
 Monstey_Reject([]{(void)Monstey_Json(std::numeric_limits<double>::infinity());});
 Monstey_Reject([]{(void)Monstey_Json::Parse(std::string(130,'[')+std::string(130,']'));});
 Monstey_Bytes b;Monstey_AppendU16(b,0x1234);Monstey_AppendU32(b,0x89abcdef);Monstey_AppendU64(b,0x123456789abcdef0);
 Monstey_Check(Monstey_U16(b,0)==0x1234&&Monstey_U32(b,2)==0x89abcdef,"LE16/32");
 Monstey_Check(Monstey_U64(b,6)==0x123456789abcdef0,"LE64");
 Monstey_Reject([&]{(void)Monstey_U32(b,b.size()-1);});Monstey_Reject([&]{(void)Monstey_U64(b,SIZE_MAX);});
 Monstey_Check(Monstey_HashSha256(std::span<const std::uint8_t>{})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256 empty");
 const std::string abc="abc";Monstey_Check(Monstey_HashSha256(std::span(reinterpret_cast<const std::uint8_t*>(abc.data()),abc.size()))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc");
 Monstey_Check(Monstey_NativeHash("A")==1&&Monstey_NativeHash("a")==1,"Case");
 Monstey_Check(Monstey_NativeHash("abc")==((1u*919u+2u)*919u+3u),"Hash397");
 Monstey_Check(Monstey_NativeHash("/")!=Monstey_NativeHash("\\"),"Raw slash");
 Monstey_Check(Monstey_PathHash("folder/file")==Monstey_PathHash("FOLDER\\FILE"),"Path slash");
 Monstey_Reject([]{(void)Monstey_PathHash(std::string("a\0b",3));});
 Monstey_Bytes chunked(2*1024*1024+7,0x37);std::uint64_t calls=0,peak=0;
 const auto chunkHash=Monstey_HashSha256Chunks(chunked.size(),[&](std::uint64_t at,std::uint64_t n) {
   ++calls;peak=std::max(peak,n);
   return Monstey_Bytes(chunked.begin()+static_cast<std::ptrdiff_t>(at),chunked.begin()+static_cast<std::ptrdiff_t>(at+n));
 });
 Monstey_Check(chunkHash==Monstey_HashSha256(chunked)&&calls==3&&peak==1024*1024,"Bounded reader SHA256");
 Monstey_Check(Monstey_HashSha256Chunks(0,{})==Monstey_HashSha256(std::span<const std::uint8_t>{}),"Empty reader digest");
 Monstey_Reject([&]{Monstey_HashSha256Chunks(2,[](std::uint64_t,std::uint64_t){return Monstey_Bytes{1};});});
 const auto output=root/L"source.bin";if(std::filesystem::exists(output))std::filesystem::remove(output);
 Monstey_WriteNewFile(output,b);Monstey_Check(Monstey_ReadFile(output)==b,"Readback");
 Monstey_Check(Monstey_HashSha256(output)==Monstey_HashSha256(b),"File hash");
 Monstey_Reject([&]{Monstey_WriteNewFile(output,{});});Monstey_Check(Monstey_ReadFile(output)==b,"No overwrite");
 const std::filesystem::path sources[]{output};Monstey_Reject([&]{Monstey_RequireOfflineOutput(output,sources);});
 const auto longRoot=root/(L"long-paths-\u03a9-"+std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()))/std::wstring(90,L'a')/std::wstring(90,L'b')/std::wstring(90,L'c');
 const auto longFile=longRoot/(std::wstring(235,L'd')+L".bin");
 Monstey_WriteNewFile(longFile,b);
 Monstey_Check(Monstey_ReadFile(longFile)==b,"Extended path read/write");
 Monstey_Check(Monstey_HashSha256(longFile)==Monstey_HashSha256(b),"Extended path hash");
 Monstey_Reject([&]{Monstey_WriteNewFile(longFile,{});});
 Monstey_Check(Monstey_ReadFile(longFile)==b,"Extended path existing file preserved");
 const std::filesystem::path longSources[]{longFile};
 const auto prefixed=std::filesystem::path(L"\\\\?\\"+std::filesystem::absolute(longFile).wstring());
 Monstey_Reject([&]{Monstey_RequireOfflineOutput(prefixed,longSources);});
 for(const auto &entry:std::filesystem::directory_iterator(Monstey_WindowsFilePath(longRoot)))
   Monstey_Check(entry.path().extension()!=L".partial","Staging file leaked");
 const auto game=root/L"game";std::filesystem::create_directories(game);
 if(!std::filesystem::exists(game/L"ShadowOfWar.exe"))Monstey_WriteNewFile(game/L"ShadowOfWar.exe",{});
 Monstey_Reject([&]{Monstey_WriteNewFile(game/L"changed.bin",{});});
 Monstey_Check(Monstey_DisplayUtf8(std::string("x")+static_cast<char>(-1))=="x\\xff","Raw byte display");
 std::cout<<Monstey_Json::object({{"status","PASS"},{"checks",Monstey_Count},{"language","C++20"},{"python_runtime",false}}).dump()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
