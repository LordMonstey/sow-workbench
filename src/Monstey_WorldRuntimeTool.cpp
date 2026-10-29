#include "Monstey_WorldRuntimeProbe.hpp"
#include <Windows.h>
#include <TlHelp32.h>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
namespace {
using namespace Monstey;
using A=std::uint64_t;
struct Handle { HANDLE value{}; explicit Handle(HANDLE h=nullptr):value(h){} ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);} Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete; };
std::string WinError(std::string_view text){return std::string(text)+" (Win32 "+std::to_string(GetLastError())+")";}
A Number(const wchar_t* text){std::string s=Monstey_Utf8(text);int radix=10;if(s.starts_with("0x")){s.erase(0,2);radix=16;}A v{};const auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v,radix);if(e!=std::errc{}||p!=s.data()+s.size())throw Monstey_Error("world_probe_argument","Invalid unsigned integer");return v;}
A Created(HANDLE p){FILETIME c{},e{},k{},u{};if(!GetProcessTimes(p,&c,&e,&k,&u))throw Monstey_Error("world_probe_identity",WinError("GetProcessTimes"));return (static_cast<A>(c.dwHighDateTime)<<32)|c.dwLowDateTime;}
std::wstring ImagePath(HANDLE p){std::wstring s(32768,L'\0');DWORD n=static_cast<DWORD>(s.size());if(!QueryFullProcessImageNameW(p,0,s.data(),&n))throw Monstey_Error("world_probe_identity",WinError("QueryFullProcessImageNameW"));s.resize(n);return s;}
void SamePath(std::wstring_view a,std::wstring_view b){if(CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)!=CSTR_EQUAL)throw Monstey_Error("world_probe_identity","Unexpected process image path");}
MODULEENTRY32W Module(DWORD pid){Handle h(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));if(h.value==INVALID_HANDLE_VALUE)throw Monstey_Error("world_probe_identity",WinError("Module snapshot"));MODULEENTRY32W m{};m.dwSize=sizeof(m);if(!Module32FirstW(h.value,&m))throw Monstey_Error("world_probe_identity",WinError("Module32FirstW"));return m;}
class ProcessMemory final:public Monstey_WorldMemory {
    HANDLE process_;
public:
    explicit ProcessMemory(HANDLE p):process_(p){}
    Monstey_WorldRead Read(A address,std::size_t count) override {Monstey_WorldRead r;r.bytes.resize(count);SIZE_T got=0;SetLastError(ERROR_SUCCESS);const BOOL ok=ReadProcessMemory(process_,reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)),r.bytes.data(),count,&got);r.error=ok?0:GetLastError();if(got>count)throw Monstey_Error("world_probe_read","ReadProcessMemory returned excessive length");r.bytes.resize(got);r.complete=ok&&got==count;return r;}
};
void ImageHeader(ProcessMemory& m,A base,DWORD imageSize) {const auto raw=m.Read(base,4096);Monstey_Require(raw.complete&&raw.bytes.size()==4096,"Incomplete module header read");const auto& b=raw.bytes;Monstey_Require(Monstey_U16(b,0)==0x5A4D,"Invalid DOS header");const auto nt=Monstey_U32(b,0x3C);Monstey_Require(nt<=4096-88,"PE header outside selected bounded header");Monstey_Require(Monstey_U32(b,nt)==0x4550&&Monstey_U16(b,nt+4)==0x8664&&Monstey_U16(b,nt+24)==0x20B,"Unexpected loaded PE profile");Monstey_Require(Monstey_U32(b,nt+24+56)==imageSize,"Module extent differs from loaded PE header");}
}
namespace {
constexpr std::wstring_view Monstey_HkaiActivationSwitch=L"--hkai-activation";
constexpr std::wstring_view Monstey_HkaiActivationHelp=L"--help-hkai-activation";
constexpr const char* Monstey_HkaiActivationUsage=
    "WorldRuntimeProbe --hkai-activation PID CREATION_FILETIME ROW_ADDRESS RESOURCE_ADDRESS STRING_SLOT_BEGIN STRING_SLOT_COUNT NEW-REPORT.json";
void Monstey_PrintHkaiActivationUsage() {
    std::cout << Monstey_HkaiActivationUsage << '\n'
              << "Unsigned integers accept decimal or 0x hexadecimal.\n"
              << "String slots describe an explicit caller read-safety range, not a native capacity.\n"
              << "Use zero for an unknown resource or string extent; later unobserved stages remain UNKNOWN.\n"
              << "Only a selected activation call gate is observed; queue execution and completed registration remain unverified.\n";
}
int Monstey_HkaiActivationMain(int argc,wchar_t** argv) {
    if(argc!=9)throw Monstey_Error("world_hkai_argument",Monstey_HkaiActivationUsage);
    const auto id=Number(argv[2]);
    if(!id||id>std::numeric_limits<DWORD>::max())throw Monstey_Error("world_hkai_argument","PID outside DWORD");
    const auto creationExpected=Number(argv[3]);
    if(!creationExpected)throw Monstey_Error("world_hkai_argument","Creation time must be pinned");
    const auto rowAddress=Number(argv[4]),resourceAddress=Number(argv[5]);
    const auto slotBegin=Number(argv[6]),slotCount=Number(argv[7]);
    const Monstey_HkaiActivationLimits limits{};
    if(slotCount>std::numeric_limits<std::uint32_t>::max())
        throw Monstey_Error("world_hkai_argument","String slot count exceeds U32");
    const Monstey_HkaiStringExtent extent{slotBegin,static_cast<std::uint32_t>(slotCount)};
    const DWORD pid=static_cast<DWORD>(id);
    const std::filesystem::path output=argv[8];
    Monstey_RequireOfflineOutput(output);
    if(std::filesystem::exists(output))throw Monstey_Error("world_probe_output","Output already exists");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!process.value)throw Monstey_Error("world_probe_identity",WinError("OpenProcess read-only"));
    const A creation=Created(process.value);
    if(creation!=creationExpected)throw Monstey_Error("world_probe_stale","PID creation time changed");
    const auto image=ImagePath(process.value);
    const std::filesystem::path Monstey_ImageFile(image);Monstey_RequireOfflineOutput(output,std::array{Monstey_ImageFile});
    const auto hash=Monstey_HashSha256(Monstey_ImageFile);
    Monstey_Require(hash=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef","Unexpected installed PE full hash");
    const auto mod=Module(pid);
    SamePath(image,mod.szExePath);
    const A base=reinterpret_cast<std::uintptr_t>(mod.modBaseAddr);
    Monstey_Require(base==0x140000000,"Module base differs from selected native profile");
    ProcessMemory memory(process.value);
    ImageHeader(memory,base,mod.modBaseSize);
    auto report=Monstey_WorldHkaiActivationSnapshot(memory,rowAddress,resourceAddress,hash,extent,limits);
    const auto afterMod=Module(pid);
    DWORD exitCode{};
    Monstey_Require(GetExitCodeProcess(process.value,&exitCode)&&exitCode==STILL_ACTIVE,"Process ended during snapshot");
    Monstey_Require(Created(process.value)==creation,"Stale process creation identity");
    SamePath(ImagePath(process.value),image);
    Monstey_Require(afterMod.modBaseAddr==mod.modBaseAddr&&afterMod.modBaseSize==mod.modBaseSize,"Module changed during snapshot");
    ImageHeader(memory,base,mod.modBaseSize);
    Monstey_Require(Monstey_HashSha256(Monstey_ImageFile)==hash,"Installed PE changed during snapshot");
    report["process"]=Monstey_Json::object({
        {"pid",static_cast<std::uint32_t>(pid)},{"creation_filetime",creation},
        {"image",Monstey_Utf8(image)},{"image_sha256",hash},
        {"module_base",Monstey_Hex(base,16)},{"module_bytes",static_cast<std::uint32_t>(mod.modBaseSize)},
        {"identity_revalidated_after",true},
        {"open_access",static_cast<std::uint32_t>(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ)},
        {"loaded_code_bytes_hashed",false}});
    report["cli"]=Monstey_Json::object({
        {"mode","explicit_hkai_activation"},
        {"caller_row_and_resource_addresses",true},
        {"caller_string_extent_is_read_safety_only",true},
        {"process_identity_scope","PID creation, image path/file SHA256, module base/size and loaded header before/after; not a hash of loaded code bytes"},
        {"native_functions_called",false},{"game_memory_written",false}});
    Monstey_WriteNewText(output,report.dump(2)+"\n");
    std::cout<<"HKAI activation observation: "<<report.at("status").as_string()
             <<"; queue execution and completed registration unverified\n";
    return 0;
}
}
namespace {
constexpr std::wstring_view Monstey_PublishedReceiverSwitch=L"--published-receiver";
constexpr std::wstring_view Monstey_PublishedReceiverHelp=L"--help-published-receiver";
constexpr const char* Monstey_PublishedReceiverUsage=
    "WorldRuntimeProbe --published-receiver PID CREATION_FILETIME NEW-REPORT.json";
void Monstey_PrintPublishedReceiverUsage() {
    std::cout << Monstey_PublishedReceiverUsage << '\n'
              << "Unsigned integers accept decimal or 0x hexadecimal.\n"
              << "Read-only published receiver pointer/vptr/C8-D0 observation only.\n"
              << "No row or resource discovery, original game calls or process-memory writes.\n"
              << "Initial activation, registration and receiver lifetime remain unverified.\n";
}
int Monstey_PublishedReceiverMain(int argc,wchar_t** argv) {
    if(argc!=5)throw Monstey_Error("world_receiver_argument",Monstey_PublishedReceiverUsage);
    const auto id=Number(argv[2]);
    if(!id||id>std::numeric_limits<DWORD>::max())throw Monstey_Error("world_receiver_argument","PID outside DWORD");
    const auto creationExpected=Number(argv[3]);
    if(!creationExpected)throw Monstey_Error("world_receiver_argument","Creation time must be pinned");
    const DWORD pid=static_cast<DWORD>(id);
    const std::filesystem::path output=argv[4];
    Monstey_RequireOfflineOutput(output);
    if(std::filesystem::exists(output))throw Monstey_Error("world_probe_output","Output already exists");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!process.value)throw Monstey_Error("world_probe_identity",WinError("OpenProcess read-only"));
    const A creation=Created(process.value);
    if(creation!=creationExpected)throw Monstey_Error("world_probe_stale","PID creation time changed");
    const auto image=ImagePath(process.value);
    const std::filesystem::path Monstey_ImageFile(image);Monstey_RequireOfflineOutput(output,std::array{Monstey_ImageFile});
    const auto hash=Monstey_HashSha256(Monstey_ImageFile);
    Monstey_Require(hash=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef","Unexpected installed PE full hash");
    const auto mod=Module(pid);
    SamePath(image,mod.szExePath);
    const A base=reinterpret_cast<std::uintptr_t>(mod.modBaseAddr);
    Monstey_Require(base==0x140000000,"Module base differs from selected native profile");
    ProcessMemory memory(process.value);
    ImageHeader(memory,base,mod.modBaseSize);
    auto report=Monstey_WorldPublishedReceiverSnapshot(memory,base,hash);
    const auto afterMod=Module(pid);
    DWORD exitCode{};
    Monstey_Require(GetExitCodeProcess(process.value,&exitCode)&&exitCode==STILL_ACTIVE,"Process ended during snapshot");
    Monstey_Require(Created(process.value)==creation,"Stale process creation identity");
    SamePath(ImagePath(process.value),image);
    Monstey_Require(afterMod.modBaseAddr==mod.modBaseAddr&&afterMod.modBaseSize==mod.modBaseSize,"Module changed during snapshot");
    ImageHeader(memory,base,mod.modBaseSize);
    Monstey_Require(Monstey_HashSha256(Monstey_ImageFile)==hash,"Installed PE changed during snapshot");
    report["process"]=Monstey_Json::object({
        {"pid",static_cast<std::uint32_t>(pid)},{"creation_filetime",creation},
        {"image",Monstey_Utf8(image)},{"image_sha256",hash},
        {"module_base",Monstey_Hex(base,16)},{"module_bytes",static_cast<std::uint32_t>(mod.modBaseSize)},
        {"identity_revalidated_after",true},
        {"open_access",static_cast<std::uint32_t>(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ)},
        {"loaded_code_bytes_hashed",false}});
    report["cli"]=Monstey_Json::object({
        {"mode","explicit_published_receiver"},{"row_or_resource_discovery",false},
        {"process_identity_scope","PID creation, image path/file SHA256, module base/size and loaded header before/after; not a hash of loaded code bytes"},
        {"native_functions_called",false},{"game_memory_written",false}});
    Monstey_WriteNewText(output,report.dump(2)+"\n");
    std::cout<<"Published receiver observation: "<<report.at("status").as_string()
             <<"; initial activation, registration and receiver lifetime unverified\n";
    return 0;
}
}
namespace {
constexpr std::wstring_view Monstey_ReceiverRowsSwitch=L"--receiver-rows";
constexpr std::wstring_view Monstey_ReceiverRowsHelp=L"--help-receiver-rows";
constexpr const char* Monstey_ReceiverRowsUsage=
    "Monstey_WorldRuntimeProbe.exe --receiver-rows PID CREATION_FILETIME RECEIVER NEW-REPORT.json";
void Monstey_PrintReceiverRowsUsage() {
    std::cout<<Monstey_ReceiverRowsUsage<<'\n'
             <<"Unsigned integers accept decimal or 0x hexadecimal.\n"
             <<"Read-only raw pointers from receiver +5C8/+5D0 and +5F8/+600 only.\n"
             <<"Receiver address is explicit; no row dereference, native call or process-memory write.\n"
             <<"Receiver identity, row lifetime, call eligibility and initial registration remain unverified.\n";
}
int Monstey_ReceiverRowsMain(int argc,wchar_t**argv) {
    if(argc!=6)throw Monstey_Error("world_receiver_rows_argument",Monstey_ReceiverRowsUsage);
    const auto id=Number(argv[2]);
    if(!id||id>std::numeric_limits<DWORD>::max())throw Monstey_Error("world_receiver_rows_argument","PID outside DWORD");
    const auto creationExpected=Number(argv[3]);
    if(!creationExpected)throw Monstey_Error("world_receiver_rows_argument","Creation time must be pinned");
    const auto receiver=Number(argv[4]);
    if(receiver<0x10000||receiver>0x00007fffffffffffULL)
        throw Monstey_Error("world_receiver_rows_argument","Receiver outside user address range");
    const DWORD pid=static_cast<DWORD>(id);
    const std::filesystem::path output=argv[5];
    Monstey_RequireOfflineOutput(output);
    if(std::filesystem::exists(output))throw Monstey_Error("world_probe_output","Output already exists");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!process.value)throw Monstey_Error("world_probe_identity",WinError("OpenProcess read-only"));
    const A creation=Created(process.value);
    if(creation!=creationExpected)throw Monstey_Error("world_probe_stale","PID creation time changed");
    const auto image=ImagePath(process.value);
    const std::filesystem::path Monstey_ImageFile(image);Monstey_RequireOfflineOutput(output,std::array{Monstey_ImageFile});
    const auto hash=Monstey_HashSha256(Monstey_ImageFile);
    Monstey_Require(hash=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef","Unexpected installed PE full hash");
    const auto mod=Module(pid);SamePath(image,mod.szExePath);
    const A base=reinterpret_cast<std::uintptr_t>(mod.modBaseAddr);
    Monstey_Require(base==0x140000000,"Module base differs from selected native profile");
    ProcessMemory memory(process.value);ImageHeader(memory,base,mod.modBaseSize);
    auto report=Monstey_WorldReceiverRowsSnapshot(memory,receiver,hash);
    const auto afterMod=Module(pid);DWORD exitCode{};
    Monstey_Require(GetExitCodeProcess(process.value,&exitCode)&&exitCode==STILL_ACTIVE,"Process ended during snapshot");
    Monstey_Require(Created(process.value)==creation,"Stale process creation identity");
    SamePath(ImagePath(process.value),image);
    Monstey_Require(afterMod.modBaseAddr==mod.modBaseAddr&&afterMod.modBaseSize==mod.modBaseSize,"Module changed during snapshot");
    ImageHeader(memory,base,mod.modBaseSize);
    Monstey_Require(Monstey_HashSha256(Monstey_ImageFile)==hash,"Installed PE changed during snapshot");
    report["process"]=Monstey_Json::object({{"pid",static_cast<std::uint32_t>(pid)},
        {"creation_filetime",creation},{"image",Monstey_Utf8(image)},{"image_sha256",hash},
        {"module_base",Monstey_Hex(base,16)},{"module_bytes",static_cast<std::uint32_t>(mod.modBaseSize)},
        {"identity_revalidated_after",true},
        {"open_access",static_cast<std::uint32_t>(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ)},
        {"loaded_code_bytes_hashed",false}});
    report["cli"]=Monstey_Json::object({{"mode","explicit_receiver_rows"},
        {"receiver_address_supplied_by_caller",true},{"receiver_vptr_checked",false},
        {"process_identity_scope","PID creation, image path/file SHA256, module base/size and loaded header before/after; not a hash of loaded code bytes"},
        {"native_functions_called",false},{"game_memory_written",false}});
    Monstey_WriteNewText(output,report.dump(2)+"\n");
    std::cout<<"Receiver row pointer observation: "<<report.at("status").as_string()
             <<"; row validity, call eligibility and initial registration unverified\n";return 0;
}
}
int wmain(int argc,wchar_t** argv){try{
    if(argc==2&&std::wstring_view(argv[1])==Monstey_ReceiverRowsHelp){Monstey_PrintReceiverRowsUsage();return 0;}
    if(argc>1&&std::wstring_view(argv[1])==Monstey_ReceiverRowsSwitch)return Monstey_ReceiverRowsMain(argc,argv);
    if(argc==2&&std::wstring_view(argv[1])==Monstey_PublishedReceiverHelp){Monstey_PrintPublishedReceiverUsage();return 0;}
    if(argc>1&&std::wstring_view(argv[1])==Monstey_PublishedReceiverSwitch)return Monstey_PublishedReceiverMain(argc,argv);
    if(argc==2&&std::wstring_view(argv[1])==Monstey_HkaiActivationHelp){Monstey_PrintHkaiActivationUsage();return 0;}
    if(argc>1&&std::wstring_view(argv[1])==Monstey_HkaiActivationSwitch)return Monstey_HkaiActivationMain(argc,argv);
    using namespace Monstey;
    if(argc!=4&&argc!=8)throw Monstey_Error("world_probe_argument","Usage: WorldRuntimeProbe PID EXPECTED_CREATION_FILETIME NEW-REPORT.json [TARGET_HASH X Y Z]");
    const auto id=Number(argv[1]);if(!id||id>std::numeric_limits<DWORD>::max())throw Monstey_Error("world_probe_argument","PID outside DWORD");const DWORD pid=static_cast<DWORD>(id);const auto expectedCreation=Number(argv[2]);if(!expectedCreation)throw Monstey_Error("world_probe_argument","Creation time must be pinned");
    const std::filesystem::path output=argv[3];Monstey_RequireOfflineOutput(output);if(std::filesystem::exists(output))throw Monstey_Error("world_probe_output","Output already exists");
    std::uint32_t target=0xE5CCFB22;std::array<float,3> point{-1789.002197265625f,1952.0042724609375f,21298.009765625f};if(argc==8){const auto x=Number(argv[4]);if(x>0xFFFFFFFF)throw Monstey_Error("world_probe_argument","Target hash exceeds U32");target=static_cast<std::uint32_t>(x);for(int i=0;i<3;++i){std::size_t consumed=0;const std::wstring s=argv[5+i];point[static_cast<std::size_t>(i)]=std::stof(s,&consumed);if(consumed!=s.size()||!std::isfinite(point[static_cast<std::size_t>(i)]))throw Monstey_Error("world_probe_argument","Point must be finite F32");}}
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));if(!process.value)throw Monstey_Error("world_probe_identity",WinError("OpenProcess read-only"));const A creation=Created(process.value);if(creation!=expectedCreation)throw Monstey_Error("world_probe_stale","PID creation time changed");
    const auto image=ImagePath(process.value);const std::filesystem::path Monstey_ImageFile(image);Monstey_RequireOfflineOutput(output,std::array{Monstey_ImageFile});const auto hash=Monstey_HashSha256(Monstey_ImageFile);Monstey_Require(hash=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef","Unexpected installed PE full hash");
    const auto mod=Module(pid);SamePath(image,mod.szExePath);const A base=reinterpret_cast<std::uintptr_t>(mod.modBaseAddr);Monstey_Require(base==0x140000000,"Module base differs from selected native profile");ProcessMemory memory(process.value);ImageHeader(memory,base,mod.modBaseSize);
    auto report=Monstey_WorldRuntimeSnapshot(memory,base,point,target);
    const auto afterMod=Module(pid);DWORD exitCode{};Monstey_Require(GetExitCodeProcess(process.value,&exitCode)&&exitCode==STILL_ACTIVE,"Process ended during snapshot");Monstey_Require(Created(process.value)==creation,"Stale process creation identity");SamePath(ImagePath(process.value),image);Monstey_Require(afterMod.modBaseAddr==mod.modBaseAddr&&afterMod.modBaseSize==mod.modBaseSize,"Module changed during snapshot");ImageHeader(memory,base,mod.modBaseSize);Monstey_Require(Monstey_HashSha256(Monstey_ImageFile)==hash,"Installed PE changed during snapshot");
    report["process"]=Monstey_Json::object({{"pid",static_cast<std::uint32_t>(pid)},{"creation_filetime",creation},{"image",Monstey_Utf8(image)},{"image_sha256",hash},{"module_base",Monstey_Hex(base,16)},{"module_bytes",static_cast<std::uint32_t>(mod.modBaseSize)},{"identity_revalidated_after",true},{"open_access",static_cast<std::uint32_t>(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ)}});
    Monstey_WriteNewText(output,report.dump(2)+"\n");std::cout<<"PASS identity-pinned read-only snapshot: "<<report.at("scenes").size()<<" scenes, "<<report.at("target_groups").size()<<" target groups; rendered is unverified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}