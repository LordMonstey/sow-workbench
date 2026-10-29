#include "Monstey_GadbClassDeclaration.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
std::uint64_t checks=0,cases=0,groups=0,actualFields=0;
void Check(bool p,const char* why){++checks;Monstey_Require(p,why);}
std::uint64_t Number(const J& j){return std::stoull(j.as_string(),nullptr,0);}
std::string Hex(std::span<const std::uint8_t> b){static constexpr char d[]="0123456789abcdef";std::string out;for(auto x:b){out+=d[x/16];out+=d[x%16];}return out;}
struct Mapping {
    std::span<const std::uint8_t> b; std::uint64_t base=0;std::size_t table=0;std::uint16_t count=0;
    explicit Mapping(std::span<const std::uint8_t> input):b(input){const auto nt=Monstey_U32(b,60);Check(Monstey_U32(b,nt)==0x4550,"Expected PE fixture");count=Monstey_U16(b,nt+6);table=static_cast<std::size_t>(nt)+24+Monstey_U16(b,nt+20);base=Monstey_U64(b,static_cast<std::size_t>(nt)+48);}
    std::span<const std::uint8_t> Get(std::uint64_t va,std::size_t n)const{
        Monstey_Require(va>=base,"Independent address below base");const auto rva=va-base;
        for(std::size_t i=0;i<count;++i){const auto p=table+i*40;const auto section=Monstey_U32(b,p+12),rawSize=Monstey_U32(b,p+16),raw=Monstey_U32(b,p+20);if(rva<section||rva-section>=rawSize)continue;const auto delta=rva-section;Monstey_Require(n<=rawSize-delta,"Independent range crosses raw end");return b.subspan(static_cast<std::size_t>(raw+delta),n);}
        throw std::runtime_error("Independent virtual-only/unmapped range");
    }
    std::string Text(std::uint64_t va)const{std::string s;for(std::size_t i=0;i<4096;++i){const auto c=Get(va+i,1)[0];if(!c)return s;s.push_back(static_cast<char>(c));}throw std::runtime_error("Independent text bound");}
};
constexpr std::uint64_t base=0x140000000,va=base+0x1000;
Monstey_Bytes Image(std::size_t raw=0x4000,std::size_t virt=0x8000){
    Monstey_Bytes b(0x200+raw,0);Monstey_PutU16(b,0,0x5a4d);Monstey_PutU32(b,60,0x80);Monstey_PutU32(b,0x80,0x4550);Monstey_PutU16(b,0x84,0x8664);Monstey_PutU16(b,0x86,1);Monstey_PutU16(b,0x94,0xf0);Monstey_PutU16(b,0x98,0x20b);Monstey_PutU64(b,0xb0,base);Monstey_PutU32(b,0x190,static_cast<std::uint32_t>(virt));Monstey_PutU32(b,0x194,0x1000);Monstey_PutU32(b,0x198,static_cast<std::uint32_t>(raw));Monstey_PutU32(b,0x19c,0x200);return b;
}
void Q(Monstey_Bytes& b,std::uint64_t p,std::uint64_t x,unsigned width=8){const auto o=static_cast<std::size_t>(p-va)+0x200;for(unsigned i=0;i<width;++i)b.at(o+i)=static_cast<std::uint8_t>(x>>(8*i));}
void Text(Monstey_Bytes& b,std::uint64_t p,const char* text){const auto n=std::strlen(text)+1;const auto at=static_cast<std::size_t>(p-va)+0x200;std::copy_n(reinterpret_cast<const std::uint8_t*>(text),n,b.begin()+static_cast<std::ptrdiff_t>(at));}
J Read(const Monstey_Bytes& b,const std::vector<std::uint64_t>& roots){return Monstey_GadbInspectClassPointers(b,roots);}
template<class F>void Reject(F&& f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}Check(threw,"Expected bounded reader rejection");}
const J& Row(const J& j,std::uint64_t p){for(const auto& row:j.at("classes").as_array())if(Number(row.at("class_pointer"))==p)return row;throw std::runtime_error("Missing class identity");}
void RawRow(const Mapping& m,const J& row){
    const auto p=Number(row.at("class_pointer"));const auto raw=m.Get(p,64);++cases;
    Check(row.at("raw64_hex").as_string()==Hex(raw),"Full class raw64 differs from independent section map");
    Check(row.at("raw64_sha256").as_string()==Monstey_HashSha256(raw),"Class hash differs");
    Check(Number(row.at("field_table_pointer"))==Monstey_U64(raw,0),"Field pointer differs");
    Check(row.at("field_count_u32").as_uint()==Monstey_U32(raw,8),"Class U32 count differs");
    Check(Number(row.at("parent_pointer"))==Monstey_U64(raw,40),"Parent pointer differs");
    Check(Number(row.at("opaque_class_pointer_30"))==Monstey_U64(raw,48),"Opaque class30 differs");
    Check(row.at("flags_u32").as_uint()==Monstey_U32(raw,56),"Flags conflated with initialized byte");
    Check(row.at("captured_initialized_u8").as_uint()==raw[60],"Initialized byte differs");
    Check(row.at("class_30_role").as_string()=="NOT_INFERRED","Class30 role invented");
    bool available=true;try{if(Monstey_U32(raw,8)) (void)m.Get(Monstey_U64(raw,0),static_cast<std::size_t>(Monstey_U32(raw,8))*72);}catch(const std::exception&){available=false;}
    Check(row.at("field_storage_status").as_string()==(available?"COMPLETE_FILE_BACKED":"UNAVAILABLE_FILE_RANGE"),"Raw storage availability differs");
    Check(row.at("fields").size()==(available?Monstey_U32(raw,8):0),"Field rows fabricated or lost");
    for(std::size_t i=0;i<row.at("fields").size();++i){const auto address=Monstey_U64(raw,0)+72*i;const auto f=m.Get(address,72);const auto& field=row.at("fields").at(i);++actualFields;
        Check(field.at("declaration_index").as_uint()==i&&Number(field.at("field_pointer"))==address,"Field identity or order differs");
        Check(field.at("raw72_hex").as_string()==Hex(f),"Full field raw72 differs");
        Check(field.at("raw72_sha256").as_string()==Monstey_HashSha256(f),"Field hash differs");
        Check(field.at("default16_hex").as_string()==Hex(f.subspan(16,16)),"Default bytes converted/truncated");
        Check(Number(field.at("name_pointer"))==Monstey_U64(f,0)&&Number(field.at("constraint_pointer"))==Monstey_U64(f,8),"Text pointer differs");
        Check(Number(field.at("name_hash_u32"))==Monstey_U32(f,32),"Name hash truncated");
        Check(field.at("type_u32").as_uint()==Monstey_U32(f,36)&&field.at("count_u32").as_uint()==Monstey_U32(f,40),"Raw field type/count differ");
        Check(field.at("flags_u32").as_uint()==Monstey_U32(f,44),"Raw field flags differ");
        Check(field.at("captured_binding_state_u32").as_uint()==Monstey_U32(f,48),"Binding state truncated");
        Check(Number(field.at("index_callback_pointer"))==Monstey_U64(f,56),"Callback pointer changed/read");
        Check(field.at("captured_cache_byte_u8").as_uint()==f[64],"Captured cache differs");
        Check(field.at("group_marker").as_bool()==((Monstey_U32(f,44)&0x280u)!=0),"Marker mask differs");
        Check(field.at("index_callback_execution").as_string()=="NOT_PERFORMED","Callback executed/inferred");
    }
}
void Synthetic(){
    ++groups;{auto b=Image();Text(b,va+0x100,"Same.Name");Text(b,va+0x140,"GetsReflected");for(unsigned k=0;k<2;++k){const auto p=va+k*64;Q(b,p,va+0x400+k*0x400);Q(b,p+8,3,4);Q(b,p+12,0x80000000u+k,4);Q(b,p+16,va+0x100);Q(b,p+32,va+0x100);Q(b,p+48,UINT64_MAX-k);Q(b,p+56,0x80000040u,4);Q(b,p+60,0x81+k,1);for(unsigned i=0;i<3;++i){const auto f=va+0x400+k*0x400+i*72;Q(b,f,va+0x140);Q(b,f+32,Monstey_NativeHash("GetsReflected"),4);Q(b,f+36,0x80000000u+i,4);Q(b,f+40,0xffffffffu-i,4);Q(b,f+44,i?0x40:0x280,4);Q(b,f+48,0x80000000u+i,4);Q(b,f+56,UINT64_MAX-i);Q(b,f+64,255-i,1);Q(b,f+65,0x81726354453627ull);}}const auto result=Read(b,{va,va+64,va,va+64});Check(result.at("requested_class_pointers").size()==4&&result.at("classes").size()==2,"Exact pointer/request identities merged");Mapping m(b);for(const auto& row:result.at("classes").as_array())RawRow(m,row);}
    ++groups;{std::uint32_t rng=0x41ab09c5;for(unsigned run=0;run<1024;++run){auto b=Image(0x800,0x1000);for(unsigned pad=0;pad<64;++pad){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;b.at(0x200+pad)=static_cast<std::uint8_t>(rng);}Q(b,va,va+0x200);Q(b,va+8,1,4);Q(b,va+16,0);Q(b,va+24,0);Q(b,va+32,0);Q(b,va+40,0);const auto f=va+0x200;for(unsigned i=0;i<72;++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;b.at(0x400+i)=static_cast<std::uint8_t>(rng);}Q(b,f,0);Q(b,f+8,0);Q(b,va+48,UINT64_MAX-run);Q(b,va+56,rng,4);Q(b,va+60,run%256,1);Mapping m(b);RawRow(m,Row(Read(b,{va}),va));}}
    ++groups;{for(const auto count:{0u,1u,2u,31u,255u,256u,4096u}){auto b=Image(0x1000+static_cast<std::size_t>(count)*72,0x100000);Q(b,va,va+0x1000);Q(b,va+8,count,4);Q(b,va+12,UINT32_MAX,4);Mapping m(b);RawRow(m,Row(Read(b,{va}),va));}auto b=Image();Q(b,va+8,4097,4);Reject([&]{(void)Read(b,{va});});}
    ++groups;{for(unsigned remaining=1;remaining<=143;++remaining){auto b=Image(0x1000,0x2000);Q(b,va,va+0x1000-remaining);Q(b,va+8,2,4);Mapping m(b);RawRow(m,Row(Read(b,{va}),va));}auto b=Image();Q(b,va,UINT64_MAX);Q(b,va+8,0,4);Mapping m(b);RawRow(m,Row(Read(b,{va}),va));Q(b,va+8,1,4);Reject([&]{(void)Read(b,{va});});}
    ++groups;{auto b=Image();Q(b,va,va+0x200);Q(b,va+8,1,4);Q(b,va+16,va+0x100);b[0x300]=0xff;Q(b,va+0x200,va+0x100);const auto j=Read(b,{va});Check(Row(j,va).at("short_name").at("status").as_string()=="UNAVAILABLE_OR_INVALID","Invalid UTF8 fabricated");Check(Row(j,va).at("fields").at(0).at("name_hash_matches").is_null(),"Unavailable text used as hash identity");Mapping m(b);RawRow(m,Row(j,va));}
    ++groups;{for(unsigned depth=1;depth<=65;++depth){auto b=Image();for(unsigned i=0;i+1<depth;++i)Q(b,va+i*64+40,va+(i+1)*64);if(depth>64){Reject([&]{(void)Read(b,{va});});continue;}const auto j=Read(b,{va,va});Check(j.at("classes").size()==depth,"Parent chain identity count differs");for(unsigned i=0;i<depth;++i)Check(Number(j.at("classes").at(i).at("class_pointer"))==va+(depth-i-1)*64,"Parent-before-child order differs");}auto b=Image();Q(b,va+40,va+64);Q(b,va+64+40,va);Reject([&]{(void)Read(b,{va});});Reject([&]{(void)Read(b,{0});});}
    ++groups;{const auto run=[](std::size_t total,std::size_t roots,std::size_t tail){auto b=Image(total*64,total*64);std::vector<std::uint64_t> pointers;for(std::size_t i=0;i<roots;++i)pointers.push_back(va+i*64);if(tail){pointers.push_back(va+roots*64);for(std::size_t i=roots;i+1<total;++i)Q(b,va+i*64+40,va+(i+1)*64);}const auto j=Read(b,pointers);Check(j.at("classes").size()==total,"Exact boundary identities lost");};run(4096,4096,0);run(4096,4032,64);Reject([&]{run(4097,4033,64);});Reject([&]{run(4159,4095,64);});}
}
void Actual(const std::filesystem::path& imagePath,const std::filesystem::path& expectedPath,const char* expectedHash,bool backed){
    ++groups;const auto bytes=Monstey_ReadFile(imagePath);Check(Monstey_HashSha256(bytes)==expectedHash,"Actual PE hash differs");const auto old=J::Parse(Monstey_ReadText(expectedPath,32ull*1024*1024));std::vector<std::uint64_t> roots;for(const auto& p:old.at("requested_class_pointers").as_array())roots.push_back(Number(p));Check(roots.size()==34,"Expected34 explicit static-bank requests");const auto j=Monstey_GadbInspectClassPointers(bytes,roots,expectedHash);Check(j.at("classes").dump()==old.at("classes").dump(),"Actual reader output differs from frozen report");Check(j.at("classes").size()==39,"Expected39 exact class identities");Mapping m(bytes);std::set<std::uint64_t> ids,opaque;std::map<std::string,unsigned> names;std::size_t fields=0,missing=0;for(const auto& row:j.at("classes").as_array()){RawRow(m,row);ids.insert(Number(row.at("class_pointer")));const auto p=Number(row.at("opaque_class_pointer_30"));if(p)opaque.insert(p);++names[row.at("full_name").at("value").as_string()];fields+=row.at("fields").size();missing+=row.at("field_storage_status").as_string()=="UNAVAILABLE_FILE_RANGE";}Check(ids.size()==39&&opaque.size()==17,"Exact identities or opaque+30 set differ");unsigned duplicateNames=0;for(const auto& [name,n]:names){(void)name;if(n==2)++duplicateNames;else Check(n==1,"Unexpected name multiplicity");}Check(duplicateNames==17,"Duplicate full names merged/lost");Check(fields==(backed?598u:0u)&&missing==(backed?0u:39u),"Actual field availability differs");Check(Monstey_HashSha256(imagePath)==expectedHash,"Actual PE changed");
}
}
int wmain(int argc,wchar_t** argv){try{Monstey_Require(argc==2||argc==6,"Usage: Independent NEW_REPORT [SNAPSHOT INSTALLED owner_snapshot_report owner_installed_report]");Synthetic();if(argc==6){Actual(argv[2],argv[4],"6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455",true);Actual(argv[3],argv[5],"ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef",false);}Monstey_WriteNewText(argv[1],J::object({{"status","PASS"},{"groups",groups},{"cases",cases},{"assertions",checks},{"raw_field_rows_checked",actualFields},{"original_programs_executed",false}}).dump(2)+"\n");std::cout<<"PASS groups="<<groups<<" cases="<<cases<<" checks="<<checks<<" fields="<<actualFields<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
