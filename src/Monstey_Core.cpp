#include "Monstey_Core.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cwctype>
#include <fstream>
#include <cstring>
#include <exception>
#include <new>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>

namespace Monstey {
void Monstey_Require(bool v, std::string_view message) { if (!v) throw Monstey_Error("validation_failed", std::string(message)); }
void Monstey_CheckRange(std::span<const std::uint8_t> d, std::size_t p, std::size_t n) { Monstey_Require(p <= d.size() && n <= d.size() - p, "Binary range exceeds input"); }
std::uint16_t Monstey_U16(std::span<const std::uint8_t> d, std::size_t p) { Monstey_CheckRange(d,p,2); return static_cast<std::uint16_t>(d[p] | (static_cast<unsigned>(d[p+1]) << 8)); }
std::uint32_t Monstey_U32(std::span<const std::uint8_t> d, std::size_t p) { Monstey_CheckRange(d,p,4); std::uint32_t v=0; for (unsigned i=0;i<4;++i) v |= static_cast<std::uint32_t>(d[p+i]) << (i*8); return v; }
std::uint64_t Monstey_U64(std::span<const std::uint8_t> d, std::size_t p) { Monstey_CheckRange(d,p,8); std::uint64_t v=0; for (unsigned i=0;i<8;++i) v |= static_cast<std::uint64_t>(d[p+i]) << (i*8); return v; }
void Monstey_PutU16(Monstey_Bytes& d,std::size_t p,std::uint16_t v) { Monstey_CheckRange(d,p,2); for (unsigned i=0;i<2;++i)d[p+i]=static_cast<std::uint8_t>(v>>(i*8)); }
void Monstey_PutU32(Monstey_Bytes& d,std::size_t p,std::uint32_t v) { Monstey_CheckRange(d,p,4); for (unsigned i=0;i<4;++i)d[p+i]=static_cast<std::uint8_t>(v>>(i*8)); }
void Monstey_PutU64(Monstey_Bytes& d,std::size_t p,std::uint64_t v) { Monstey_CheckRange(d,p,8); for (unsigned i=0;i<8;++i)d[p+i]=static_cast<std::uint8_t>(v>>(i*8)); }
void Monstey_AppendU16(Monstey_Bytes& d,std::uint16_t v) { for(unsigned i=0;i<2;++i)d.push_back(static_cast<std::uint8_t>(v>>(i*8))); }
void Monstey_AppendU32(Monstey_Bytes& d,std::uint32_t v) { for(unsigned i=0;i<4;++i)d.push_back(static_cast<std::uint8_t>(v>>(i*8))); }
void Monstey_AppendU64(Monstey_Bytes& d,std::uint64_t v) { for(unsigned i=0;i<8;++i)d.push_back(static_cast<std::uint8_t>(v>>(i*8))); }
std::filesystem::path Monstey_WindowsFilePath(const std::filesystem::path &input) {
    Monstey_Require(!input.empty() && input.wstring().find(L'\0')==std::wstring::npos,
                    "Filesystem path must be nonempty without NUL");
    auto path=std::filesystem::absolute(input).lexically_normal();
    path.make_preferred();
    const auto value=path.wstring();
    if(value.starts_with(L"\\\\?\\"))return path;
    if(value.starts_with(L"\\\\"))return std::filesystem::path(L"\\\\?\\UNC\\"+value.substr(2));
    return std::filesystem::path(L"\\\\?\\"+value);
}
Monstey_Bytes Monstey_ReadFile(const std::filesystem::path& p,std::uint64_t maximum) {
    std::ifstream f(Monstey_WindowsFilePath(p),std::ios::binary|std::ios::ate);
    if(!f)throw Monstey_Error("read_failed","Cannot open input: "+Monstey_Utf8(p.wstring()));
    const auto end=f.tellg();
    Monstey_Require(end>=0 && static_cast<std::uint64_t>(end)<=maximum && static_cast<std::uint64_t>(end)<=SIZE_MAX,"Input exceeds configured size limit");
    Monstey_Bytes data(static_cast<std::size_t>(end));f.seekg(0);
    if(!data.empty())f.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()));
    if(!f)throw Monstey_Error("read_failed","Incomplete input read");
    return data;
}
std::string Monstey_ReadText(const std::filesystem::path& p,std::uint64_t maximum) {
    const auto b=Monstey_ReadFile(p,maximum);return std::string(b.begin(),b.end());
}
namespace {
std::wstring Monstey_CanonicalKey(const std::filesystem::path &p) {
    auto value=std::filesystem::weakly_canonical(Monstey_WindowsFilePath(p)).wstring();
    if (value.starts_with(L"\\\\?\\UNC\\")) value=L"\\\\"+value.substr(8);
    else if(value.starts_with(L"\\\\?\\")) value=value.substr(4);
    std::transform(value.begin(),value.end(),value.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return value;
}
class Monstey_Sha {
public:
    Monstey_Sha() {
        try {
            Check(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0),"Open SHA256");
            DWORD used=0,length=0;
            Check(BCryptGetProperty(algorithm_,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&used,0),"SHA256 object length");
            storage_.resize(length);
            Check(BCryptCreateHash(algorithm_,&hash_,storage_.data(),static_cast<ULONG>(storage_.size()),nullptr,0,0),"Create SHA256");
        } catch(...) { Release();throw; }
    }
    ~Monstey_Sha(){Release();}
    Monstey_Sha(const Monstey_Sha&)=delete;
    Monstey_Sha& operator=(const Monstey_Sha&)=delete;
    void Add(std::span<const std::uint8_t> b) {
        while(!b.empty()) { const auto count=static_cast<ULONG>(std::min<std::size_t>(b.size(),0x7fffffff));Check(BCryptHashData(hash_,const_cast<PUCHAR>(b.data()),count,0),"Hash data");b=b.subspan(count); }
    }
    std::string Finish() {
        std::array<std::uint8_t,32> output{};Check(BCryptFinishHash(hash_,output.data(),static_cast<ULONG>(output.size()),0),"Finish SHA256");
        std::string s;constexpr char h[]="0123456789abcdef";for(auto c:output){s.push_back(h[c>>4]);s.push_back(h[c&15]);}return s;
    }
private:
    BCRYPT_ALG_HANDLE algorithm_=nullptr;BCRYPT_HASH_HANDLE hash_=nullptr;Monstey_Bytes storage_;
    static void Check(NTSTATUS status,const char* where){if(status<0)throw Monstey_Error("crypto_failed",std::string(where)+": "+Monstey_Hex(static_cast<std::uint32_t>(status)));}
    void Release() noexcept {if(hash_)BCryptDestroyHash(hash_);if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);hash_=nullptr;algorithm_=nullptr;}
};
}
void Monstey_RequireOfflineOutput(const std::filesystem::path& p,std::span<const std::filesystem::path> sources) {
    Monstey_Require(!p.empty(),"Output path must be explicit");
    const auto key=Monstey_CanonicalKey(p);
    for(const auto& s:sources)Monstey_Require(key!=Monstey_CanonicalKey(s),"Output cannot overwrite a source");
    auto parent=std::filesystem::absolute(p).parent_path();
    while(!parent.empty()) {
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(parent/L"ShadowOfWar.exe")) && !std::filesystem::exists(Monstey_WindowsFilePath(parent/L"x64"/L"ShadowOfWar.exe")),"Write an offline output outside the game installation");
        const auto next=parent.parent_path();if(next==parent)break;parent=next;
    }
}
void Monstey_WriteNewFile(const std::filesystem::path &input, std::span<const std::uint8_t> data) {
    Monstey_RequireOfflineOutput(input);
    const auto path=Monstey_WindowsFilePath(input);
    Monstey_Require(!std::filesystem::exists(path),"Output already exists");
    std::filesystem::create_directories(path.parent_path());
    const auto target=path.wstring();
    constexpr auto rename_prefix=offsetof(FILE_RENAME_INFO,FileName);
    Monstey_Require(target.size()<=(MAXDWORD-rename_prefix-sizeof(wchar_t))/sizeof(wchar_t),
        "Output rename path exceeds Win32 buffer limit");
    const auto name_bytes=target.size()*sizeof(wchar_t);
    const auto rename_bytes=rename_prefix+name_bytes+sizeof(wchar_t);
    static_assert(alignof(FILE_RENAME_INFO)<=alignof(std::uint64_t));
    std::vector<std::uint64_t> rename_storage(rename_bytes/sizeof(std::uint64_t)+(rename_bytes%sizeof(std::uint64_t)!=0),0);
    auto* rename=::new(static_cast<void*>(rename_storage.data())) FILE_RENAME_INFO{};
    rename->ReplaceIfExists=FALSE;
    rename->RootDirectory=nullptr;
    rename->FileNameLength=static_cast<DWORD>(name_bytes);
    std::memcpy(reinterpret_cast<std::uint8_t*>(rename_storage.data())+rename_prefix,target.data(),name_bytes);
    std::array<std::uint8_t,16> random{};
    if(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
        throw Monstey_Error("crypto_failed","Cannot create staging nonce");
    std::wstring nonce=L".Monstey-";
    for(const auto v:random) {
        nonce.push_back(L"0123456789abcdef"[v>>4]);
        nonce.push_back(L"0123456789abcdef"[v&15]);
    }
    const auto stage=path.parent_path()/(nonce+L".partial");
    // Exclusive ownership lasts through rename/flush or object-based cleanup.
    HANDLE handle=CreateFileW(stage.c_str(),GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE) {
        const auto error=GetLastError();
        throw Monstey_Error("write_failed","Cannot create output staging file (Win32 "+std::to_string(error)+")");
    }
    DWORD first_error=ERROR_SUCCESS;
    try {
        std::size_t offset=0;
        while(offset<data.size()) {
            const auto count=static_cast<DWORD>(std::min<std::size_t>(data.size()-offset,1024*1024));
            DWORD written=0;
            const auto success=WriteFile(handle,data.data()+offset,count,&written,nullptr);
            if(!success||written==0) {
                first_error=success?ERROR_WRITE_FAULT:GetLastError();
                throw Monstey_Error("write_failed","Partial output write (Win32 "+std::to_string(first_error)+")");
            }
            offset+=written;
        }
        if(!FlushFileBuffers(handle)) {
            first_error=GetLastError();
            throw Monstey_Error("write_failed","Cannot flush staged output (Win32 "+std::to_string(first_error)+")");
        }
        if(!SetFileInformationByHandle(handle,FileRenameInfo,rename,static_cast<DWORD>(rename_bytes))) {
            first_error=GetLastError();
            throw Monstey_Error("publish_failed","Output exists or cannot be published (Win32 "+std::to_string(first_error)+")");
        }
        // Flush the same owned object after its directory-entry change as well.
        if(!FlushFileBuffers(handle)) {
            first_error=GetLastError();
            throw Monstey_Error("write_failed","Cannot flush published output (Win32 "+std::to_string(first_error)+")");
        }
        CloseHandle(handle);
    } catch(...) {
        const auto original=std::current_exception();
        FILE_DISPOSITION_INFO disposition{TRUE};
        const auto deleted=SetFileInformationByHandle(handle,FileDispositionInfo,&disposition,sizeof(disposition));
        const auto cleanup_error=deleted?ERROR_SUCCESS:GetLastError();
        CloseHandle(handle);
        if(first_error!=ERROR_SUCCESS)SetLastError(first_error);
        if(!deleted) {
            try {std::rethrow_exception(original);}
            catch(const Monstey_Error& error) {
                throw Monstey_Error(error.code,std::string(error.what())+
                    "; owned file-object cleanup failed (Win32 "+std::to_string(cleanup_error)+")");
            }
            catch(...) {std::rethrow_exception(original);}
        }
        std::rethrow_exception(original);
    }
}
void Monstey_WriteNewText(const std::filesystem::path& p,std::string_view text) {Monstey_WriteNewFile(p,std::span(reinterpret_cast<const std::uint8_t*>(text.data()),text.size()));}
std::string Monstey_HashSha256(std::span<const std::uint8_t> data) {Monstey_Sha h;h.Add(data);return h.Finish();}
std::string Monstey_HashSha256(const std::filesystem::path& p) {
    std::ifstream f(Monstey_WindowsFilePath(p),std::ios::binary);if(!f)throw Monstey_Error("read_failed","Cannot hash source");
    Monstey_Sha hash;Monstey_Bytes block(1024*1024);
    while(f){f.read(reinterpret_cast<char*>(block.data()),static_cast<std::streamsize>(block.size()));const auto count=f.gcount();if(count>0)hash.Add(std::span(block.data(),static_cast<std::size_t>(count)));}
    Monstey_Require(f.eof(),"Input read failed while hashing");return hash.Finish();
}
std::string Monstey_HashSha256Chunks(std::uint64_t length,
    const std::function<Monstey_Bytes(std::uint64_t,std::uint64_t)> &reader) {
    Monstey_Require(length==0||static_cast<bool>(reader),"Hash reader is missing");
    Monstey_Sha hash;
    for(std::uint64_t offset=0;offset<length;) {
        const auto count=std::min<std::uint64_t>(1024*1024,length-offset);
        const auto bytes=reader(offset,count);
        Monstey_Require(bytes.size()==count,"Hash reader returned an incomplete chunk");
        hash.Add(bytes);offset+=count;
    }
    return hash.Finish();
}
std::uint32_t Monstey_NativeHash(std::string_view b,std::uint32_t seed) {
    constexpr std::string_view table=
        "0000000000000000000000000000000000000000000000000000000000000000"
        "253a333c3d3e403242434128362737341b1c1d1e1f2021222324313038293935"
        "3b0102030405060708090a0b0c0d0e0f101112131415161718191a2c2a2d3f26"
        "440102030405060708090a0b0c0d0e0f101112131415161718191a2e2b2f4500"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000";
    static_assert(table.size()==512);
    auto hex=[](char c){return static_cast<unsigned>(c<='9'?c-'0':c-'a'+10);};
    auto value=seed;for(const auto raw:b){const auto c=static_cast<unsigned char>(raw);value=value*0x397u+((hex(table[c*2])<<4)|hex(table[c*2+1]));}return value;
}
std::uint32_t Monstey_PathHash(std::string_view bytes,std::uint32_t seed) {
    std::string normalized(bytes);for(auto& c:normalized){Monstey_Require(c!='\0' && static_cast<unsigned char>(c)<128,"Native resource path must be ASCII without NUL");if(c=='/')c='\\';}return Monstey_NativeHash(normalized,seed);
}
std::wstring Monstey_Wide(std::string_view s) {
    if(s.empty())return {};
    Monstey_Require(s.size()<=INT_MAX,"UTF8 text exceeds Windows conversion limit");
    const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    Monstey_Require(n>0,"Invalid UTF8 text");std::wstring out(static_cast<std::size_t>(n),L'\0');
    Monstey_Require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n)==n,"UTF8 conversion failed");return out;
}
std::string Monstey_Utf8(std::wstring_view s) {
    if(s.empty())return {};
    Monstey_Require(s.size()<=INT_MAX,"Wide text exceeds Windows conversion limit");
    const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    Monstey_Require(n>0,"Invalid UTF16 text");std::string out(static_cast<std::size_t>(n),'\0');
    Monstey_Require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr)==n,"UTF16 conversion failed");return out;
}
std::string Monstey_DisplayUtf8(std::string_view s) {
    std::string out;constexpr char hex[]="0123456789abcdef";
    for(std::size_t i=0;i<s.size();) {
        const auto c=static_cast<unsigned char>(s[i]);std::size_t width=c<0x80?1:c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;
        bool valid=width!=0 && width<=s.size()-i;
        if(valid){try{(void)Monstey_Wide(s.substr(i,width));}catch(const Monstey_Error&){valid=false;}}
        if(valid){out.append(s.substr(i,width));i+=width;}
        else{out+="\\x";out.push_back(hex[c>>4]);out.push_back(hex[c&15]);++i;}
    }return out;
}
std::string Monstey_Hex(std::uint64_t n,unsigned width) {std::ostringstream s;s<<"0x"<<std::hex<<std::setfill('0')<<std::setw(static_cast<int>(width))<<n;return s.str();}
}
