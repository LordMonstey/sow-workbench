#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include "Monstey_Core.hpp"
#include <atomic>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
namespace
{
const auto Monstey_RealCreateFile=&::CreateFileW;
const auto Monstey_RealWriteFile=&::WriteFile;
const auto Monstey_RealFlush=&::FlushFileBuffers;
const auto Monstey_RealSetInfo=&::SetFileInformationByHandle;
const auto Monstey_RealMove=&::MoveFileExW;
const auto Monstey_RealClose=&::CloseHandle;
void Monstey_Check(bool v,const char *s){if(!v)throw std::runtime_error(s);}
enum class Monstey_Mode{None,RenamePause,LegacyClosePause,WriteError,FlushError,PostRenameFlushError,ShortWrites,ZeroProgress,CleanupError,CleanupClosePause};
struct Monstey_Context
{
    std::atomic<Monstey_Mode> mode{Monstey_Mode::None};
    HANDLE ready=nullptr,proceed=nullptr,stage_handle=INVALID_HANDLE_VALUE;
    std::filesystem::path stage;
    BY_HANDLE_FILE_INFORMATION identity{};
    unsigned creates=0,writes=0,flushes=0,renames=0,dispositions=0,closes=0;
    DWORD access=0,sharing=0,first_native_error=0,cleanup_native_error=0;
    bool close_pause_used=false;
    Monstey_Context(Monstey_Mode value):mode(value)
    {
        ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);Monstey_Check(ready!=nullptr,"Cannot create ready event");
        proceed=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!proceed){Monstey_RealClose(ready);throw std::runtime_error("Cannot create proceed event");}
    }
    ~Monstey_Context(){if(ready)Monstey_RealClose(ready);if(proceed)Monstey_RealClose(proceed);}
    void Pause(){SetEvent(ready);Monstey_Check(WaitForSingleObject(proceed,10000)==WAIT_OBJECT_0,"Observer did not release writer");}
};
thread_local Monstey_Context *Monstey_Current=nullptr;
HANDLE WINAPI Monstey_TestCreate(LPCWSTR name,DWORD access,DWORD sharing,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE template_file)
{
    const auto result=Monstey_RealCreateFile(name,access,sharing,security,disposition,flags,template_file);
    const auto saved=GetLastError();
    if(Monstey_Current && result!=INVALID_HANDLE_VALUE && disposition==CREATE_NEW)
    {
        auto &c=*Monstey_Current;c.stage=name;c.stage_handle=result;c.access=access;c.sharing=sharing;++c.creates;
        Monstey_Check(GetFileInformationByHandle(result,&c.identity)!=0,"Cannot capture stage file identity");
    }
    SetLastError(saved);return result;
}
BOOL WINAPI Monstey_TestWrite(HANDLE handle,LPCVOID data,DWORD count,LPDWORD written,LPOVERLAPPED overlapped)
{
    if(!Monstey_Current || handle!=Monstey_Current->stage_handle)return Monstey_RealWriteFile(handle,data,count,written,overlapped);
    auto &c=*Monstey_Current;++c.writes;
    if((c.mode==Monstey_Mode::WriteError || c.mode==Monstey_Mode::CleanupError) && c.writes==1)
    {
        const auto result=Monstey_RealWriteFile(INVALID_HANDLE_VALUE,data,count,written,overlapped);
        c.first_native_error=GetLastError();SetLastError(c.first_native_error);return result;
    }
    if(c.mode==Monstey_Mode::ZeroProgress){*written=0;return TRUE;}
    return Monstey_RealWriteFile(handle,data,c.mode==Monstey_Mode::ShortWrites?std::max<DWORD>(1,count/2):count,written,overlapped);
}
BOOL WINAPI Monstey_TestFlush(HANDLE handle)
{
    if(!Monstey_Current || handle!=Monstey_Current->stage_handle)return Monstey_RealFlush(handle);
    auto &c=*Monstey_Current;++c.flushes;
    if((c.mode==Monstey_Mode::FlushError && c.flushes==1) ||
        (c.mode==Monstey_Mode::PostRenameFlushError && c.flushes==2))
    {
        const auto result=Monstey_RealFlush(INVALID_HANDLE_VALUE);c.first_native_error=GetLastError();
        SetLastError(c.first_native_error);return result;
    }
    return Monstey_RealFlush(handle);
}
[[maybe_unused]] BOOL WINAPI Monstey_TestSetInfo(HANDLE handle,FILE_INFO_BY_HANDLE_CLASS type,LPVOID info,DWORD size)
{
    if(!Monstey_Current || handle!=Monstey_Current->stage_handle)return Monstey_RealSetInfo(handle,type,info,size);
    auto &c=*Monstey_Current;
    if(type==FileRenameInfo)
    {
        ++c.renames;Monstey_Check(!static_cast<FILE_RENAME_INFO*>(info)->ReplaceIfExists,"Rename unexpectedly permits overwrite");
        if(c.mode==Monstey_Mode::RenamePause)c.Pause();
    }
    if(type==FileDispositionInfo)
    {
        ++c.dispositions;Monstey_Check(static_cast<FILE_DISPOSITION_INFO*>(info)->DeleteFile!=FALSE,"Cleanup did not request delete disposition");
        if(c.mode==Monstey_Mode::CleanupError)
        {
            const auto result=Monstey_RealSetInfo(handle,type,info,0);c.cleanup_native_error=GetLastError();
            SetLastError(c.cleanup_native_error);return result;
        }
    }
    return Monstey_RealSetInfo(handle,type,info,size);
}
BOOL WINAPI Monstey_TestClose(HANDLE handle)
{
    const auto result=Monstey_RealClose(handle);const auto saved=GetLastError();
    if(Monstey_Current && handle==Monstey_Current->stage_handle)
    {
        auto &c=*Monstey_Current;++c.closes;
        if(c.mode==Monstey_Mode::LegacyClosePause && !c.close_pause_used)
        {c.close_pause_used=true;c.Pause();}
        if(c.mode==Monstey_Mode::CleanupClosePause && !c.close_pause_used)
        {c.close_pause_used=true;ResetEvent(c.proceed);c.Pause();}
        // Force a distinct last error after release to test initial-error restoration.
        if(c.mode==Monstey_Mode::WriteError || c.mode==Monstey_Mode::FlushError ||
            c.mode==Monstey_Mode::PostRenameFlushError || c.mode==Monstey_Mode::CleanupError || c.mode==Monstey_Mode::ZeroProgress)
        {SetLastError(ERROR_ACCESS_DENIED);return result;}
    }
    SetLastError(saved);return result;
}
[[maybe_unused]] BOOL WINAPI Monstey_TestMove(LPCWSTR source,LPCWSTR destination,DWORD flags)
{return Monstey_RealMove(source,destination,flags);}
}
#define CreateFileW Monstey_TestCreate
#define WriteFile Monstey_TestWrite
#define FlushFileBuffers Monstey_TestFlush
#define SetFileInformationByHandle Monstey_TestSetInfo
#define CloseHandle Monstey_TestClose
#define MoveFileExW Monstey_TestMove
#if defined(MONSTEY_LEGACY_WRITER)
#include "preimages/Monstey_Core.accepted016.cpp"
#else
#include "../src/Monstey_Core.cpp"
#endif
#undef CreateFileW
#undef WriteFile
#undef FlushFileBuffers
#undef SetFileInformationByHandle
#undef CloseHandle
#undef MoveFileExW
using namespace Monstey;
namespace
{
using J=Monstey_Json;using B=Monstey_Bytes;
void Monstey_RawWrite(const std::filesystem::path &p,const B &bytes)
{
    const auto h=Monstey_RealCreateFile(Monstey_WindowsFilePath(p).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    Monstey_Check(h!=INVALID_HANDLE_VALUE,"Cannot create observer fixture");
    DWORD n=0;const auto ok=Monstey_RealWriteFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&n,nullptr);
    const auto flushed=Monstey_RealFlush(h);Monstey_RealClose(h);
    Monstey_Check(ok && n==bytes.size() && flushed,"Cannot write observer fixture");
}
[[maybe_unused]] void Monstey_NoStages(const std::filesystem::path &directory)
{
    for(const auto &e:std::filesystem::recursive_directory_iterator(Monstey_WindowsFilePath(directory)))
        Monstey_Check(!e.path().filename().wstring().ends_with(L".partial"),"Owned stage survived successful cleanup");
}
struct Monstey_ErrorResult{bool failed=false;std::string code,message;DWORD last_error=0;};
Monstey_ErrorResult Monstey_Run(Monstey_Context *context,const std::filesystem::path &output,const B &bytes)
{
    Monstey_Current=context;
    try{Monstey_WriteNewFile(output,bytes);Monstey_Current=nullptr;return {};}
    catch(const Monstey_Error &e){const auto error=GetLastError();Monstey_Current=nullptr;return {true,e.code,e.what(),error};}
    catch(...){Monstey_Current=nullptr;throw;}
}
J Monstey_Race(const std::filesystem::path &directory,bool legacy,bool force_collision)
{
    std::filesystem::create_directories(directory);const auto output=directory/L"destination.bin";
    B payload(2*1024*1024+17,0x54);const B foreign{9,8,7,6,5};
    Monstey_Context context(legacy?Monstey_Mode::LegacyClosePause:Monstey_Mode::RenamePause);
    Monstey_ErrorResult result;std::exception_ptr writer_exception;
    std::thread writer([&]{try{result=Monstey_Run(&context,output,payload);}catch(...){writer_exception=std::current_exception();}});
    DWORD delete_error=0,rename_error=0,replace_error=0;bool deleted=false,renamed=false,replaced=false;
    const auto moved=directory/L"observer-moved-stage.bin";
    try
    {
        Monstey_Check(WaitForSingleObject(context.ready,10000)==WAIT_OBJECT_0,"Writer did not reach controlled race point");
        if(legacy)
        {
            deleted=DeleteFileW(context.stage.c_str())!=FALSE;delete_error=GetLastError();
            Monstey_Check(deleted,"Legacy closed stage could not be replaced");Monstey_RawWrite(context.stage,foreign);replaced=true;
        }
        else
        {
            deleted=DeleteFileW(context.stage.c_str())!=FALSE;delete_error=GetLastError();
            renamed=Monstey_RealMove(context.stage.c_str(),Monstey_WindowsFilePath(moved).c_str(),0)!=FALSE;rename_error=GetLastError();
            const auto replacement=Monstey_RealCreateFile(context.stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            replace_error=GetLastError();replaced=replacement!=INVALID_HANDLE_VALUE;if(replaced)Monstey_RealClose(replacement);
            Monstey_Check(!deleted && !renamed && !replaced,"Open exclusive stage was replaced/renamed/deleted");
        }
        if(force_collision)Monstey_RawWrite(output,foreign);
        SetEvent(context.proceed);writer.join();
    }
    catch(...){SetEvent(context.proceed);if(writer.joinable())writer.join();throw;}
    if(writer_exception)std::rethrow_exception(writer_exception);
    if(force_collision)
    {
        Monstey_Check(result.failed && result.code=="publish_failed","Destination collision was not rejected");
        Monstey_Check(Monstey_ReadFile(output)==foreign,"Foreign destination was overwritten/deleted");
    }
    else
    {
        Monstey_Check(!result.failed && Monstey_ReadFile(output)==payload,"Owned handle publication failed");
        const auto h=Monstey_RealCreateFile(Monstey_WindowsFilePath(output).c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Monstey_Check(h!=INVALID_HANDLE_VALUE,"Cannot inspect published file identity");BY_HANDLE_FILE_INFORMATION identity{};
        const auto ok=GetFileInformationByHandle(h,&identity);Monstey_RealClose(h);
        Monstey_Check(ok && identity.dwVolumeSerialNumber==context.identity.dwVolumeSerialNumber &&
            identity.nFileIndexHigh==context.identity.nFileIndexHigh && identity.nFileIndexLow==context.identity.nFileIndexLow,
            "Published file is not the original owned staging object");
    }
    Monstey_Check(!std::filesystem::exists(context.stage),"Stage pathname survived race cleanup/publication");
    if(!legacy)
    {
        Monstey_Check((context.access&DELETE)!=0 && context.sharing==0 && context.closes==1 && context.renames==1,
            "Exclusive handle lifetime differs");
        Monstey_Check(force_collision?context.dispositions==1:context.dispositions==0,"Unexpected object cleanup count");
    }
    return J::object({{"legacy_closed_stage",legacy},{"forced_destination_collision",force_collision},
        {"stage_delete_succeeded",deleted},{"stage_rename_succeeded",renamed},{"stage_replace_succeeded",replaced},
        {"delete_error",static_cast<std::uint64_t>(delete_error)},{"rename_error",static_cast<std::uint64_t>(rename_error)},{"replace_error",static_cast<std::uint64_t>(replace_error)},
        {"published_identity_matches_stage",!force_collision},{"foreign_destination_preserved",force_collision},
        {"legacy_competing_stage_deleted",legacy&&replaced&&!std::filesystem::exists(context.stage)},
        {"writer_failed",result.failed},{"writer_error",result.code},{"stage_closed_once",context.closes==1}});
}
[[maybe_unused]] J Monstey_AfterCloseRace(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    const auto output=directory/L"destination.bin";const B foreign{7,6,5,4},payload{1,2,3};
    Monstey_Context context(Monstey_Mode::RenamePause);Monstey_ErrorResult result;std::exception_ptr error;
    std::thread writer([&]{try{result=Monstey_Run(&context,output,payload);}catch(...){error=std::current_exception();}});
    bool absent_before_replacement=false;
    try
    {
        Monstey_Check(WaitForSingleObject(context.ready,10000)==WAIT_OBJECT_0,"Writer did not pause before rename");
        Monstey_RawWrite(output,foreign); // Race destination into the no-replace rename.
        ResetEvent(context.ready);context.mode=Monstey_Mode::CleanupClosePause;SetEvent(context.proceed);
        Monstey_Check(WaitForSingleObject(context.ready,10000)==WAIT_OBJECT_0,"Writer did not pause after cleanup close");
        absent_before_replacement=!std::filesystem::exists(context.stage);
        Monstey_Check(absent_before_replacement,"Owned source object was not deleted before close barrier");
        Monstey_RawWrite(context.stage,foreign); // A different file object now owns that pathname.
        SetEvent(context.proceed);writer.join();
    }
    catch(...){SetEvent(context.proceed);if(writer.joinable())writer.join();throw;}
    if(error)std::rethrow_exception(error);
    Monstey_Check(result.failed && result.code=="publish_failed" && context.dispositions==1 && context.closes==1,
        "After-close race did not retain the original publication failure/lifetime");
    Monstey_Check(Monstey_ReadFile(output)==foreign && Monstey_ReadFile(context.stage)==foreign,
        "Pathname cleanup deleted a competing post-close stage replacement");
    return J::object({{"owned_stage_absent_before_replacement",absent_before_replacement},
        {"post_close_foreign_stage_preserved",true},{"foreign_destination_preserved",true},
        {"original_error",result.code},{"competing_stage_path",Monstey_Utf8(context.stage.wstring())}});
}
J Monstey_Tests(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);auto tests=J::array();unsigned failed=0;
    const auto test=[&](const char *name,const std::function<J()> &f)
    {
        try{auto r=f();r["name"]=name;r["passed"]=true;tests.as_array().push_back(std::move(r));}
        catch(const std::exception &e){++failed;tests.as_array().push_back(J::object({{"name",name},{"passed",false},{"error",Monstey_DisplayUtf8(e.what())}}));}
    };
#if defined(MONSTEY_LEGACY_WRITER)
    test("accepted016_real_closed_stage_replacement_reproduces_foreign_delete",[&]{return Monstey_Race(directory/L"legacy-race",true,true);});
#else
    test("real_open_stage_replacement_attempts_blocked_on_success",[&]{return Monstey_Race(directory/L"race-success",false,false);});
    test("real_open_stage_replacement_blocked_and_foreign_destination_preserved",[&]{return Monstey_Race(directory/L"race-collision",false,true);});
    test("foreign_stage_replacement_after_real_cleanup_close_survives",[&]{return Monstey_AfterCloseRace(directory/L"after-close-race");});
    test("empty_multichunk_and_short_write_outputs_complete",[&]
    {
        const auto sub=directory/L"sizes";std::filesystem::create_directories(sub);B bytes(3*1024*1024+123);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(i%251);
        Monstey_WriteNewFile(sub/L"empty.bin",{});Monstey_Check(Monstey_ReadFile(sub/L"empty.bin").empty(),"Empty output differs");
        Monstey_Context c(Monstey_Mode::ShortWrites);const auto r=Monstey_Run(&c,sub/L"chunks.bin",bytes);
        Monstey_Check(!r.failed && Monstey_ReadFile(sub/L"chunks.bin")==bytes && c.writes>3 && c.flushes==2 && c.closes==1,"Short-write loop or flush lifetime differs");Monstey_NoStages(sub);
        return J::object({{"writes",c.writes},{"flushes",c.flushes}});
    });
    test("real_write_and_flush_failures_delete_owned_object_preserve_first_error",[&]
    {
        const auto sub=directory/L"io-errors";std::filesystem::create_directories(sub);J cases=J::array();unsigned i=0;
        for(auto mode:{Monstey_Mode::WriteError,Monstey_Mode::FlushError,Monstey_Mode::PostRenameFlushError})
        {
            Monstey_Context c(mode);const auto output=sub/(L"failure-"+std::to_wstring(i++)+L".bin");const auto r=Monstey_Run(&c,output,B{1,2,3});
            Monstey_Check(r.failed && r.code=="write_failed" && c.first_native_error==ERROR_INVALID_HANDLE &&
                r.last_error==c.first_native_error && r.message.find("Win32 6")!=std::string::npos && c.dispositions==1 && c.closes==1,
                "Initial real Win32 failure was lost during cleanup");
            Monstey_Check(!std::filesystem::exists(output) && !std::filesystem::exists(c.stage),"Failed write/flush left owned stage or destination");
            cases.as_array().push_back(J::object({{"first_error",static_cast<std::uint64_t>(c.first_native_error)},{"returned_last_error",static_cast<std::uint64_t>(r.last_error)},{"message",r.message}}));
        }
        Monstey_NoStages(sub);return J::object({{"cases",cases},{"fault_method","real_Win32_invalid_handle_at_private_call_boundary"}});
    });
    test("zero_progress_branch_has_bounded_failure_and_cleanup",[&]
    {
        const auto sub=directory/L"zero-progress";std::filesystem::create_directories(sub);Monstey_Context c(Monstey_Mode::ZeroProgress);
        const auto r=Monstey_Run(&c,sub/L"zero.bin",B{1,2,3});Monstey_Check(r.failed && r.code=="write_failed" && r.last_error==ERROR_WRITE_FAULT && c.writes==1 && c.dispositions==1 && c.closes==1,
            "Zero progress did not terminate with original error");Monstey_NoStages(sub);return J::object({{"synthetic_success_zero_bytes_control",true}});
    });
    test("cleanup_failure_preserves_initial_error_and_releases_handle",[&]
    {
        const auto sub=directory/L"cleanup-error";std::filesystem::create_directories(sub);Monstey_Context c(Monstey_Mode::CleanupError);
        const auto r=Monstey_Run(&c,sub/L"never-published.bin",B{1,2,3});
        Monstey_Check(r.failed && r.code=="write_failed" && c.first_native_error==ERROR_INVALID_HANDLE && r.last_error==ERROR_INVALID_HANDLE &&
            c.cleanup_native_error!=ERROR_SUCCESS && r.message.find("cleanup failed")!=std::string::npos && c.closes==1,"Cleanup failure replaced initial error or leaked handle");
        const auto reopened=Monstey_RealCreateFile(c.stage.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        Monstey_Check(reopened!=INVALID_HANDLE_VALUE,"Retained owned stage still has an open handle");Monstey_RealClose(reopened);
        // Retain this explicitly diagnosed own fixture for evidence; no pathname cleanup is attempted.
        return J::object({{"first_error",static_cast<std::uint64_t>(c.first_native_error)},{"cleanup_error",static_cast<std::uint64_t>(c.cleanup_native_error)},{"returned_last_error",static_cast<std::uint64_t>(r.last_error)},{"retained_stage",Monstey_Utf8(c.stage.wstring())},{"diagnostic",r.message}});
    });
    test("existing_file_directory_and_parent_obstruction_preserved",[&]
    {
        const auto sub=directory/L"obstructions";std::filesystem::create_directories(sub);const auto existing=sub/L"existing.bin";const B marker{9,8,7};Monstey_RawWrite(existing,marker);
        const auto a=Monstey_Run(nullptr,existing,B{1,2,3});Monstey_Check(a.failed && Monstey_ReadFile(existing)==marker,"Existing output overwritten");
        const auto folder=sub/L"existing-directory";std::filesystem::create_directory(folder);const auto b=Monstey_Run(nullptr,folder,B{1});Monstey_Check(b.failed && std::filesystem::is_directory(folder),"Existing directory altered");
        const auto blocked=sub/L"blocked-parent";Monstey_RawWrite(blocked,marker);bool rejected=false;try{Monstey_WriteNewFile(blocked/L"child.bin",B{1});}catch(const std::exception&){rejected=true;}
        Monstey_Check(rejected && Monstey_ReadFile(blocked)==marker,"Parent obstruction changed");Monstey_NoStages(sub);return J::object();
    });
    test("Unicode_spaces_and_long_directory_path_publish",[&]
    {
        auto sub=directory/L"Unicode \u00e9\u4f8b\u03a9";for(unsigned i=0;i<6;++i)sub/=L"long-directory-segment-0123456789-abcdefghij";
        const auto output=sub/L"result \u00e0\u6d4b\u8bd5.bin";const B bytes{1,2,3,4};Monstey_WriteNewFile(output,bytes);
        Monstey_Check(output.wstring().size()>260 && Monstey_ReadFile(output)==bytes,"Unicode/long path output differs");Monstey_NoStages(sub);
        return J::object({{"path_characters",static_cast<std::uint64_t>(output.wstring().size())}});
    });
    test("concurrent_atomic_no_replace_has_exactly_one_complete_winner",[&]
    {
        const auto sub=directory/L"parallel";std::filesystem::create_directories(sub);unsigned rounds=0;
        for(unsigned round=0;round<12;++round)
        {
            const auto output=sub/(L"winner-"+std::to_wstring(round)+L".bin");std::promise<void> start;const auto signal=start.get_future().share();
            std::array<unsigned,8> won{};std::array<std::thread,8> workers;
            try{for(std::size_t i=0;i<workers.size();++i)workers[i]=std::thread([&,i]{signal.wait();try{Monstey_WriteNewFile(output,B(65536,static_cast<std::uint8_t>(i+1)));won[i]=1;}catch(const std::exception&) {}});start.set_value();}
            catch(...){try{start.set_value();}catch(...){}for(auto &t:workers)if(t.joinable())t.join();throw;}
            for(auto &t:workers)t.join();unsigned count=0,winner=0;for(unsigned i=0;i<won.size();++i)if(won[i]){++count;winner=i+1;}
            Monstey_Check(count==1 && Monstey_ReadFile(output)==B(65536,static_cast<std::uint8_t>(winner)),"Concurrent publication mixed bytes or replaced a winner");++rounds;
        }
        Monstey_NoStages(sub);return J::object({{"rounds",rounds},{"writers_per_round",8}});
    });
    test("offline_game_root_and_source_protection_contract_unchanged",[&]
    {
        const auto sub=directory/L"guarded-game-root";std::filesystem::create_directories(sub);Monstey_RawWrite(sub/L"ShadowOfWar.exe",B{});
        const auto r=Monstey_Run(nullptr,sub/L"blocked.bin",B{1});Monstey_Check(r.failed && r.code=="validation_failed" && !std::filesystem::exists(sub/L"blocked.bin"),"Offline output guard changed");
        const auto source=directory/L"protected-input.bin";Monstey_RawWrite(source,B{1});const std::array<std::filesystem::path,1> protected_sources{source};bool rejected=false;
        try{Monstey_RequireOfflineOutput(source,protected_sources);}catch(const Monstey_Error&){rejected=true;}
        Monstey_Check(rejected && Monstey_ReadFile(source)==B{1},"Source path protection changed");return J::object();
    });
    test("repeated_writes_leave_no_owned_handle_leak",[&]
    {
        const auto sub=directory/L"handles";std::filesystem::create_directories(sub);Monstey_WriteNewFile(sub/L"warmup.bin",B{1});
        DWORD before=0,after=0;Monstey_Check(GetProcessHandleCount(GetCurrentProcess(),&before)!=0,"Cannot inspect initial handle count");
        for(unsigned i=0;i<64;++i)Monstey_WriteNewFile(sub/(L"file-"+std::to_wstring(i)+L".bin"),B{1,2,3});
        Monstey_Check(GetProcessHandleCount(GetCurrentProcess(),&after)!=0 && before==after,"Owned handles leaked across writes");Monstey_NoStages(sub);
        return J::object({{"before",static_cast<std::uint64_t>(before)},{"after",static_cast<std::uint64_t>(after)},{"writes",64}});
    });
#endif
    return J::object({{"schema","Monstey.CorePublicationTests/1"},{"groups",static_cast<std::uint64_t>(tests.size())},{"failed",failed},{"tests",tests},
#if defined(MONSTEY_LEGACY_WRITER)
        {"implementation","accepted016_legacy_negative_control"},
#else
        {"implementation","exclusive_handle_rename_candidate"},
#endif
        {"all_file_paths_are_owned_synthetic_fixtures",true},{"native_original_executed",false}});
}
}
int wmain(int argc,wchar_t **argv)
{
    try
    {
        Monstey_Check(argc==2,"Usage CorePublicationTests NEW-directory");const std::filesystem::path output=argv[1];
        Monstey_Check(!std::filesystem::exists(output),"Output directory must be new");Monstey_RequireOfflineOutput(output);
        const auto report=Monstey_Tests(output);Monstey_WriteNewText(output/L"report.json",report.dump(2));
        std::cout<<report.dump(2)<<'\n';return report["failed"].as_uint()?1:0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
