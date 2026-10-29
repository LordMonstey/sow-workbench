#include <Windows.h>
#include "Monstey_Core.hpp"
#include "Monstey_Json.hpp"
#include "Monstey_ConstraintFixtures.hpp"
#include <iostream>
using namespace Monstey;
namespace {
std::uint64_t Monstey_Checks=0,Monstey_Cases=0;
void Monstey_Check(bool good,const char* label){++Monstey_Checks;if(!good)throw std::runtime_error(label);}
struct Monstey_Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Monstey_Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
};
std::wstring Monstey_Quote(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (auto ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += ch; slashes = 0; continue; }
        result.append(slashes, L'\\'); slashes = 0; result += ch;
    }
    result.append(slashes * 2, L'\\'); result += L'\"'; return result;
}
struct Monstey_RunResult { DWORD code; Monstey_Json json; std::string text; };
Monstey_RunResult Monstey_Run(const std::filesystem::path& exe, const std::filesystem::path& root,
                             const std::vector<std::wstring>& arguments) {
    ++Monstey_Cases;
    const auto stdout_path = root / (L"stdout-" + std::to_wstring(Monstey_Cases) + L".json");
    const auto stderr_path = root / (L"stderr-" + std::to_wstring(Monstey_Cases) + L".txt");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Monstey_Handle stdout_handle{CreateFileW(stdout_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Monstey_Handle stderr_handle{CreateFileW(stderr_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Monstey_Check(stdout_handle.value != INVALID_HANDLE_VALUE && stderr_handle.value != INVALID_HANDLE_VALUE, "test output handle creation");
    auto command = Monstey_Quote(exe.wstring());
    for (const auto& argument : arguments) { command += L" "; command += Monstey_Quote(argument); }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_handle.value; startup.hStdError = stderr_handle.value; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    Monstey_Check(CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "test CLI process creation");
    Monstey_Handle process_handle{process.hProcess}, thread_handle{process.hThread};
    const auto wait = WaitForSingleObject(process_handle.value, 30000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process_handle.value, 99); throw std::runtime_error("CLI test timed out"); }
    DWORD code = 99; Monstey_Check(GetExitCodeProcess(process_handle.value, &code) != FALSE, "test CLI process exit");
    const auto text = Monstey_ReadText(stdout_path);
    Monstey_Check(Monstey_ReadText(stderr_path).empty(), "CLI stderr must be empty; stdout holds JSON errors");
    return {code, Monstey_Json::Parse(text), text};
}
}
int wmain(int argc,wchar_t** argv){
 try{
  if(argc!=3)throw std::invalid_argument("ConstraintCliTests <CLI> <NEW directory>");
  const std::filesystem::path exe=argv[1],root=argv[2];Monstey_Check(std::filesystem::create_directory(root),"new output directory");
  auto result=Monstey_Run(exe,root,{});Monstey_Check(result.code==2&&!result.json.at("ok").as_bool(),"missing-input JSON failure");
  result=Monstey_Run(exe,root,{L"--unknown"});Monstey_Check(result.code==2&&!result.json.at("ok").as_bool(),"unknown-option JSON failure");
  const auto empty=root/L"empty.dat";Monstey_WriteNewFile(empty,{});const auto emptyHash=Monstey_HashSha256(empty);
  result=Monstey_Run(exe,root,{L"--input",empty.wstring(),L"--summary"});Monstey_Check(result.code==0&&result.json.at("complete").as_bool()&&result.json.at("records").as_uint()==0&&result.json.at("read_calls").as_uint()==0,"empty original protocol success");Monstey_Check(result.json.at("source_sha256").as_string()==emptyHash&&Monstey_HashSha256(empty)==emptyHash,"empty source remains readonly");
  std::vector<std::uint8_t> bytes;Monstey_Test::Record(bytes,"A/B",1,0x12345678,3,4);Monstey_Test::Record(bytes,std::string("A/B\0ignored",11),255,0,0x80000000,0x7FC00001);
  const auto input=root/L"synthetic-\xe9-\u773C constraints.dat";Monstey_WriteNewFile(input,bytes);const auto hash=Monstey_HashSha256(input);
  result=Monstey_Run(exe,root,{L"--input",input.wstring(),L"--summary"});Monstey_Check(result.code==0&&result.json.at("records").as_uint()==2&&result.json.at("unique_keys").as_uint()==1&&result.json.at("duplicate_records").as_uint()==1&&result.json.at("read_calls").as_uint()==12,"summary counts and duplicates");Monstey_Check(!result.json.contains("rows")&&result.json.at("source_unchanged").as_bool()&&Monstey_HashSha256(input)==hash,"summary omits original keys and preserves source");
  result=Monstey_Run(exe,root,{L"--input",input.wstring(),L"--list"});const auto& rows=result.json.at("rows");Monstey_Check(result.code==0&&rows.size()==1&&rows.at(0).at("key_bytes_hex").as_string()=="412f42"&&rows.at(0).at("type").as_uint()==255,"list uses first NUL and last duplicate");Monstey_Check(rows.at(0).at("word4_bits").as_string()==Monstey_Hex(0x80000000u)&&rows.at(0).at("word8_bits").as_string()==Monstey_Hex(0x7FC00001u),"raw words preserved without numeric conversion");Monstey_Check(!result.json.at("original_native_entrypoint_invoked").as_bool()&&!result.json.at("original_STL_ABI_qualified").as_bool(),"qualification scope explicit");
  const auto broken=root/L"truncated.dat";bytes.resize(7);Monstey_WriteNewFile(broken,bytes);const auto brokenHash=Monstey_HashSha256(broken);
  result=Monstey_Run(exe,root,{L"--input",broken.wstring()});Monstey_Check(result.code==1&&!result.json.at("complete").as_bool()&&result.json.at("consumed_bytes").as_uint()==7&&result.json.at("read_calls").as_uint()==3&&result.json.at("records").as_uint()==0,"truncated type failure and partial cursor");Monstey_Check(Monstey_HashSha256(broken)==brokenHash,"failed input readonly");
  result=Monstey_Run(exe,root,{L"--input",(root/L"missing.dat").wstring()});Monstey_Check(result.code==2&&!result.json.at("ok").as_bool(),"missing file JSON failure");
  Monstey_Check(Monstey_HashSha256(input)==hash,"all CLI calls preserve full source");
  std::cout<<Monstey_Json::object({{"status","PASS"},{"cases",Monstey_Cases},{"checks",Monstey_Checks}}).dump()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
