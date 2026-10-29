#include "Monstey_FearAudit.hpp"
#include <iostream>
#include <functional>
using namespace Monstey;
namespace {
Monstey_Bytes Monstey_Fixture() {
    Monstey_Bytes pool;
    const auto string=[&](std::string_view s){const auto at=static_cast<std::uint32_t>(pool.size());pool.insert(pool.end(),s.begin(),s.end());pool.push_back(0);return at;};
    const auto category=string("Synthetic"),record=string("Example");
    const std::uint32_t names[]={string("Alpha"),string("Float"),string("Link"),string("String"),string("Vector"),string("Wide")};
    const auto text=string("test");
    const auto vector=static_cast<std::uint32_t>(pool.size());Monstey_AppendU32(pool,0x3f800000);Monstey_AppendU32(pool,0x40000000);Monstey_AppendU32(pool,0x40400000);
    const auto wide=static_cast<std::uint32_t>(pool.size());Monstey_AppendU16(pool,0x03b1);Monstey_AppendU16(pool,0);
    Monstey_Bytes b;for(const auto v:{0x42444147u,3u,static_cast<std::uint32_t>(pool.size()),1u,1u,6u,6u}) Monstey_AppendU32(b,v);
    b.insert(b.end(),pool.begin(),pool.end());for(const auto v:{1u,category,1u,record,6u}) Monstey_AppendU32(b,v);
    constexpr std::uint32_t types[]={1,2,9,4,7,5};const std::uint32_t values[]={1,0x3fc00000,0,text,vector,wide};
    for(std::size_t i=0;i<6;++i) for(const auto v:{names[i],types[i],0u,1u,values[i]}) Monstey_AppendU32(b,v);return b;
}
Monstey_Bytes Monstey_PeFixture() {
    Monstey_Bytes b(0x600,0);Monstey_PutU16(b,0,0x5a4d);Monstey_PutU32(b,0x3c,0x80);Monstey_PutU32(b,0x80,0x4550);
    Monstey_PutU16(b,0x84,0x14c);Monstey_PutU16(b,0x86,1);Monstey_PutU16(b,0x94,0xe0);Monstey_PutU16(b,0x98,0x10b);
    Monstey_PutU32(b,0xb4,0x400000);Monstey_PutU32(b,0x98+96+16,0x1000);Monstey_PutU32(b,0x98+96+20,0x400);
    Monstey_PutU32(b,0x178+8,0x400);Monstey_PutU32(b,0x178+12,0x1000);Monstey_PutU32(b,0x178+16,0x400);Monstey_PutU32(b,0x178+20,0x200);Monstey_PutU32(b,0x178+36,0x40000040);
    const auto word=[&](std::size_t p,std::uint16_t v){Monstey_PutU16(b,0x200+p,v);};
    const auto dword=[&](std::size_t p,std::uint32_t v){Monstey_PutU32(b,0x200+p,v);};
    word(14,1);dword(16,4);dword(20,0x80000020);word(0x20+14,1);dword(0x20+16,101);dword(0x20+20,0x80000040);
    word(0x40+14,1);dword(0x40+16,1033);dword(0x40+20,0x60);dword(0x60,0x1080);
    Monstey_Bytes menu;Monstey_AppendU16(menu,0);Monstey_AppendU16(menu,0);Monstey_AppendU16(menu,0x90);
    for(wchar_t c:std::wstring(L"File")) Monstey_AppendU16(menu,static_cast<std::uint16_t>(c));Monstey_AppendU16(menu,0);
    Monstey_AppendU16(menu,0x80);Monstey_AppendU16(menu,32771);for(wchar_t c:std::wstring(L"Open")) Monstey_AppendU16(menu,static_cast<std::uint16_t>(c));Monstey_AppendU16(menu,0);
    dword(0x64,static_cast<std::uint32_t>(menu.size()));std::copy(menu.begin(),menu.end(),b.begin()+0x280);return b;
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        const std::filesystem::path directory=argc>1?argv[1]:L"fear-audit-test-output";std::filesystem::create_directories(directory);
        unsigned passed=0;auto cases=Monstey_Json::array();
        const auto test=[&](const char* name,const std::function<void()>& fn){fn();++passed;cases.as_array().push_back(name);};
        const auto reject=[&](const Monstey_Bytes& data){bool failed=false;try{(void)Monstey_FearInspectDatabaseBytes(data);}catch(const std::exception&){failed=true;}Monstey_Require(failed,"Malformed legacy database accepted");};
        const auto fixture=Monstey_Fixture();
        test("synthetic_all_supported_storage_classes",[&]{const auto r=Monstey_FearInspectDatabaseBytes(fixture,true);Monstey_Require(r["categories_count"].as_uint()==1&&r["attributes_count"].as_uint()==6,"Wrong fixture counts");Monstey_Require(r["record_link_words"].as_uint()==1&&r["null_or_out_of_range_links"].as_uint()==0,"Wrong ref decoding");});
        test("truncated_legacy_header",[&]{reject(Monstey_Bytes(4));});
        test("version_does_not_silently_upgrade",[&]{auto b=fixture;Monstey_PutU32(b,4,17);reject(b);});
        test("declared_count_overflow",[&]{auto b=fixture;Monstey_PutU32(b,24,0xffffffff);reject(b);});
        test("trailing_bytes_rejected",[&]{auto b=fixture;b.push_back(0);reject(b);});
        test("pool_offset_out_of_bounds",[&]{auto b=fixture;const auto root=28+Monstey_U32(b,8);Monstey_PutU32(b,root+4,0xffffffff);reject(b);});
        test("unsorted_attributes_rejected",[&]{auto b=fixture;const auto first=28+Monstey_U32(b,8)+20;const auto x=Monstey_U32(b,first),y=Monstey_U32(b,first+20);Monstey_PutU32(b,first,y);Monstey_PutU32(b,first+20,x);reject(b);});
        test("pool_vector_range_checked",[&]{auto b=fixture;const auto first=28+Monstey_U32(b,8)+20;Monstey_PutU32(b,first+4*20+16,Monstey_U32(b,8)-1);reject(b);});
        const auto pePath=directory/L"synthetic-menu.exe";const auto peBytes=Monstey_PeFixture();
        if(std::filesystem::exists(pePath)) std::filesystem::remove(pePath);Monstey_WriteNewFile(pePath,peBytes);
        test("synthetic_pe_menu_command_mapping",[&]{const auto r=Monstey_FearInspectPe(pePath);const auto& items=r["resources"][0u]["menu"]["items"];Monstey_Require(items[0u]["text"].as_string()=="File"&&items[0u]["children"][0u]["command_id"].as_uint()==32771,"Menu ID not recovered");});
        test("malformed_pe_resource_cycle",[&]{auto b=peBytes;Monstey_PutU32(b,0x214,0x80000000);const auto path=directory/L"cyclic-resource.exe";if(std::filesystem::exists(path)) std::filesystem::remove(path);Monstey_WriteNewFile(path,b);bool failed=false;try{(void)Monstey_FearInspectPe(path);}catch(const std::exception&){failed=true;}Monstey_Require(failed,"Resource cycle accepted");});
        auto report=Monstey_Json::object();report["synthetic_cases"]=cases;report["synthetic_fixture_hex"]=[] (const Monstey_Bytes& b){std::string out;constexpr char digits[]="0123456789abcdef";for(auto v:b){out.push_back(digits[v>>4]);out.push_back(digits[v&15]);}return out;}(fixture);
        if(argc>2) {
            test("original_sdk_static_audit_all_editors_and_four_databases",[&]{report["original_audit"]=Monstey_FearAudit(argv[2]);});
            test("original_full_database_counts_and_links",[&] {
                const auto& databases=report["original_audit"]["databases"];Monstey_Require(databases.size()==4,"Missing original database");
                constexpr std::uint64_t counts[]={3271,1582,143,48};
                for(std::size_t i=0;i<databases.size();++i) {Monstey_Require(databases[i]["records_count"].as_uint()==counts[i],"Wrong full original record count");Monstey_Require(databases[i]["invalid_non_null_record_links"].as_uint()==0,"Original non-null link failed resolution");}
            });
            test("original_command_routes_resolve_expected_handlers",[&] {
                const auto& originals=report["original_audit"]["originals"];Monstey_Require(originals.size()==7,"Missing original editor");
                bool extract=false,edit=false;
                for(const auto& route:originals[0u]["mfc_command_routes"].as_array()) if(route["command_id"].as_uint()==32783&&route["handler_rva"].as_string()==Monstey_Hex(0x13de0)&&route["route"].as_string()=="command") extract=true;
                for(const auto& route:originals[1u]["mfc_command_routes"].as_array()) if(route["command_id"].as_uint()==32784&&route["handler_rva"].as_string()==Monstey_Hex(0x1f190)&&route["route"].as_string()=="command") edit=true;
                Monstey_Require(extract&&edit,"Original action route mismatch");
            });
        }
        report["passed"]=passed;report["status"]="PASS";const auto out=directory/L"fear-audit-tests.json";if(std::filesystem::exists(out)) std::filesystem::remove(out);Monstey_WriteNewText(out,report.dump(2)+"\n");
        std::cout<<"FEAR native audit tests: "<<passed<<" passed\n";return 0;
    }catch(const std::exception& e) {std::cerr<<"FEAR audit failed: "<<e.what()<<"\n";return 1;}
}
