#include "Monstey_GadbRegistry.hpp"
#include <cstring>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using B=Monstey_Bytes;using J=Monstey_Json;
constexpr std::uint64_t base=0x140000000ull;
std::uint64_t Ea(std::size_t local){return base+0x1000+local;}
void Q(B& b,std::size_t local,std::uint64_t n){for(unsigned i=0;i<8;++i)b[512+local+i]=static_cast<std::uint8_t>(n>>(i*8));}
void U(B& b,std::size_t at,std::uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=static_cast<std::uint8_t>(n>>(i*8));}
void T(B& b,std::size_t local,std::string_view value){std::memcpy(b.data()+512+local,value.data(),value.size());b[512+local+value.size()]=0;}
B Fixture(){
 B b(512+8192,0);b[0]='M';b[1]='Z';U(b,60,128);U(b,128,0x4550);b[132]=0x64;b[133]=0x86;b[134]=1;b[148]=240;
 b[152]=0x0b;b[153]=2;for(unsigned i=0;i<8;++i)b[176+i]=static_cast<std::uint8_t>(base>>(i*8));
 const std::size_t section=392;std::memcpy(b.data()+section,".data",5);U(b,section+8,8192);U(b,section+12,0x1000);U(b,section+16,8192);U(b,section+20,512);U(b,section+36,0xe0000040);
 T(b,0x1000,"Parent");T(b,0x1020,"Test.Parent");T(b,0x1040,"Child");T(b,0x1060,"Test.Parent.Child");T(b,0x1080,"Value");T(b,0x10a0,"Other");
 Q(b,0x100,Ea(0x500));Q(b,0x108,2);Q(b,0x110,Ea(0x1000));Q(b,0x120,Ea(0x1020));Q(b,0x138,8);
 Q(b,0x180,0);Q(b,0x188,0);Q(b,0x190,Ea(0x1040));Q(b,0x1a0,Ea(0x1060));Q(b,0x1a8,Ea(0x100));Q(b,0x1b8,8);
 for(unsigned i=0;i<2;++i){const std::size_t f=0x500+i*72;Q(b,f,Ea(0x1080+i*0x20));U(b,512+f+16,0x3f800000);U(b,512+f+32,Monstey_NativeHash(i?"Other":"Value"));U(b,512+f+36,2);U(b,512+f+40,1);U(b,512+f+44,2);Q(b,f+56,Ea(0x800+i*16));b[512+f+64]=static_cast<std::uint8_t>(250+i);b[512+0x800+i*16]=0xb0;b[512+0x801+i*16]=static_cast<std::uint8_t>(i);b[512+0x802+i*16]=0xc3;}
 return b;
}
J Read(const B& b){return Monstey_GadbDiscoverNativeDeclarations(b,Monstey_HashSha256(b));}
void Fails(const std::function<void()>& f){bool failed=false;try{f();}catch(const Monstey_Error&){failed=true;}Monstey_Require(failed,"Expected rejection");}
}
int wmain(int argc,wchar_t** argv){
 try{Monstey_Require(argc==2,"Need new report output");auto names=J::array();
 auto test=[&](const char* name,const std::function<void()>& fn){fn();names.as_array().push_back(name);};
 test("Complete native declarations preserve raw cache and callback identity",[]{const auto b=Fixture();const auto r=Read(b);Monstey_Require(r.at("categories").size()==2&&r.at("unknown").empty(),"Lost declarations");const auto& f=r.at("categories").at("Test.Parent").at("fields").at(1);Monstey_Require(f.at("native_ordinal").as_uint()==1&&f.at("cached_ordinal").as_uint()==251&&f.at("native_ordinal_source").as_string()=="native_index_callback","Cache conflated with identity");});
 test("Parent declaration stays unflattened",[]{const auto r=Read(Fixture());const auto& c=r.at("categories").at("Test.Parent.Child");Monstey_Require(c.at("field_count").as_uint()==0&&c.at("fields").empty()&&c.at("parent").as_string()=="Test.Parent","Flattened parent table");});
 test("Native descriptor provenance matches actual72bytes",[]{const auto b=Fixture();const auto r=Read(b);const auto& f=r.at("categories").at("Test.Parent").at("fields").at(0);Monstey_Require(f.at("native_descriptor_sha256").as_string()==Monstey_HashSha256(std::span(b).subspan(512+0x500,72)),"Bad field provenance");});
 test("Unsupported callback remains unknown with dependent parent boundary",[]{auto b=Fixture();b[512+0x800]=0xcc;const auto r=Read(b);Monstey_Require(r.at("categories").empty()&&r.at("unknown").size()==2,"Guessed callback");});
 test("Callback identity mismatch is not replaced by cache or position",[]{auto b=Fixture();b[512+0x811]=42;const auto r=Read(b);Monstey_Require(r.at("categories").empty()&&r.at("unknown").size()==2,"Guessed identity");});
 test("Index callback data bytes are not executable proof",[]{auto b=Fixture();U(b,392+36,0xc0000040);const auto r=Read(b);Monstey_Require(r.at("categories").empty()&&r.at("unknown").size()==2,"Treated data as executable callback");});
 test("Null callback explicitly lacks callback qualification",[]{auto b=Fixture();Q(b,0x500+56,0);const auto r=Read(b);Monstey_Require(!r.at("categories").at("Test.Parent").at("native_index_callbacks_qualified").as_bool(),"Null callback considered proven");});
 test("Duplicate full names remain ambiguous",[]{auto b=Fixture();std::memcpy(b.data()+512+0x200,b.data()+512+0x100,64);const auto r=Read(b);Monstey_Require(r.at("ambiguous").size()==1&&!r.at("categories").contains("Test.Parent"),"Picked duplicate candidate");});
 test("Missing parent remains unknown",[]{auto b=Fixture();Q(b,0x1a8,Ea(0x700));const auto r=Read(b);Monstey_Require(r.at("categories").size()==1&&r.at("unknown").size()==1,"Guessed parent");});
 test("Parent cycles terminate without recursion escape",[]{auto b=Fixture();Q(b,0x128,Ea(0x180));const auto r=Read(b);Monstey_Require(r.at("categories").empty()&&r.at("unknown").size()==2,"Accepted parent cycle");});
 test("Wrong source hash rejects",[]{const auto b=Fixture();Fails([&]{(void)Monstey_GadbDiscoverNativeDeclarations(b,std::string(64,'0'));});});
 test("Truncated section rejects before traversal",[]{auto b=Fixture();b.resize(1024);Fails([&]{(void)Read(b);});});
 test("Wrong PE machine rejects",[]{auto b=Fixture();b[132]=0x4c;b[133]=1;Fails([&]{(void)Read(b);});});
 test("Malformed name hash is not a declaration",[]{auto b=Fixture();U(b,512+0x500+32,0);const auto r=Read(b);Monstey_Require(r.at("categories").empty(),"Accepted false field shape");});
 test("Fields beyondUInt8 envelope remain unsupported",[]{auto b=Fixture();Q(b,0x108,257);const auto r=Read(b);Monstey_Require(r.at("categories").empty()&&r.at("unknown").size()==2,"Accepted oversized identity table");});
 test("All prefix truncations reject or produce no invented native declarations",[]{const auto original=Fixture();for(std::size_t n=0;n<512;++n){B b(original.begin(),original.begin()+static_cast<std::ptrdiff_t>(n));Fails([&]{(void)Read(b);});}});
 auto report=J::object({{"status","PASS"},{"groups",names.size()},{"tests",names},{"original_executed",false},{"components",J::object({{"PE_mapping","synthetic boundaries verified"},{"declaration_shape","synthetic discovery verified, runtime registration notclaimed"},{"field_identity","callback and mutable cache distinguished"},{"parent_tables","unflattened and uncertain chains explicit"}})}});
 Monstey_WriteNewText(argv[1],report.dump(2)+"\n");std::cout<<report.dump()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
