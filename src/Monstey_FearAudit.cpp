#include "Monstey_FearAudit.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string_view>

namespace Monstey {
namespace {
using Monstey_Span = std::span<const std::uint8_t>;
using Monstey_J = Monstey_Json;
void Monstey_AuditCheck(bool condition,const char* message) {Monstey_Require(condition,message);}
std::string Monstey_ByteHex(Monstey_Span bytes) {
    constexpr char table[]="0123456789abcdef"; std::string out;out.reserve(bytes.size()*2);
    for(auto b:bytes) {out.push_back(table[b>>4]);out.push_back(table[b&15]);}return out;
}
std::string Monstey_Cstr(Monstey_Span b,std::size_t p) {
    Monstey_CheckRange(b,p,1);const auto first=p;while(p<b.size()&&b[p]) ++p;
    Monstey_AuditCheck(p<b.size(),"FEAR: unterminated string");return {reinterpret_cast<const char*>(b.data()+first),p-first};
}
std::string Monstey_Fold(std::string text) {for(char& c:text) if(c>='A'&&c<='Z') c=static_cast<char>(c+('a'-'A'));return text;}
std::string Monstey_ResourceText(Monstey_Span b,std::size_t& p,bool terminated=true,std::size_t count=0) {
    std::wstring result;std::size_t units=0;
    while(terminated||units<count) {const auto ch=Monstey_U16(b,p);p+=2;++units;if(terminated&&ch==0) return Monstey_Utf8(result);result.push_back(static_cast<wchar_t>(ch));Monstey_AuditCheck(units<=1024*1024,"FEAR: resource text limit");}
    return Monstey_Utf8(result);
}
struct Monstey_Section {std::uint32_t va=0,virtualSize=0,raw=0,size=0,flags=0;};
struct Monstey_Pe {
    Monstey_Span bytes;std::uint64_t imageBase=0;std::uint32_t resourceRva=0,resourceSize=0,exportRva=0,exportSize=0,importRva=0,importSize=0;
    std::uint16_t machine=0;bool pe64=false;std::vector<Monstey_Section> sections;
    explicit Monstey_Pe(Monstey_Span b):bytes(b) {
        Monstey_AuditCheck(Monstey_U16(b,0)==0x5a4d,"FEAR: missing MZ");const auto nt=Monstey_U32(b,0x3c);
        Monstey_AuditCheck(Monstey_U32(b,nt)==0x4550,"FEAR: missing PE");machine=Monstey_U16(b,static_cast<std::size_t>(nt)+4);
        const auto count=Monstey_U16(b,static_cast<std::size_t>(nt)+6),optionalSize=Monstey_U16(b,static_cast<std::size_t>(nt)+20);const auto opt=static_cast<std::size_t>(nt)+24;
        Monstey_CheckRange(b,opt,optionalSize);const auto magic=Monstey_U16(b,opt);pe64=magic==0x20b;
        Monstey_AuditCheck(magic==0x10b||pe64,"FEAR: unsupported PE header");const std::size_t dir=pe64?112:96;
        Monstey_AuditCheck(optionalSize>=dir+24,"FEAR: truncated PE directories");
        imageBase=pe64?Monstey_U64(b,opt+24):Monstey_U32(b,opt+28);exportRva=Monstey_U32(b,opt+dir);exportSize=Monstey_U32(b,opt+dir+4);
        importRva=Monstey_U32(b,opt+dir+8);importSize=Monstey_U32(b,opt+dir+12);resourceRva=Monstey_U32(b,opt+dir+16);resourceSize=Monstey_U32(b,opt+dir+20);
        Monstey_AuditCheck(count>0&&count<=96,"FEAR: invalid section count");const auto table=opt+optionalSize;
        for(std::size_t i=0;i<count;++i) {const auto p=table+i*40;Monstey_CheckRange(b,p,40);Monstey_Section s;
            s.virtualSize=Monstey_U32(b,p+8);s.va=Monstey_U32(b,p+12);s.size=Monstey_U32(b,p+16);s.raw=Monstey_U32(b,p+20);s.flags=Monstey_U32(b,p+36);
            Monstey_CheckRange(b,s.raw,s.size);sections.push_back(s);}
    }
    std::size_t Offset(std::uint32_t rva,std::size_t count=1) const {
        for(const auto& s:sections) if(rva>=s.va) {const std::uint64_t delta=static_cast<std::uint64_t>(rva)-s.va;
            if(delta<s.size&&count<=s.size-delta) {const auto p=static_cast<std::size_t>(s.raw)+static_cast<std::size_t>(delta);Monstey_CheckRange(bytes,p,count);return p;}}
        throw Monstey_Error("fear_invalid_rva","RVA is not backed by file bytes: "+Monstey_Hex(rva));
    }
    Monstey_Span At(std::uint32_t rva,std::size_t count) const {return bytes.subspan(Offset(rva,count),count);}
    bool Executable(std::uint32_t rva) const {for(const auto& s:sections) if(rva>=s.va&&static_cast<std::uint64_t>(rva)-s.va<std::max(s.virtualSize,s.size)) return (s.flags&0x20000000)!=0;return false;}
};
Monstey_J Monstey_ResourceValue(Monstey_Span b,std::size_t& p) {
    const auto first=Monstey_U16(b,p);if(first==0) {p+=2;return nullptr;}
    if(first==0xffff) {p+=4;return Monstey_J(static_cast<std::uint64_t>(Monstey_U16(b,p-2)));}
    return Monstey_J(Monstey_ResourceText(b,p));
}
std::string Monstey_ResourceKey(Monstey_Span b,std::uint32_t key) {
    if((key&0x80000000)==0) return std::to_string(key);
    std::size_t p=key&0x7fffffff;const auto count=Monstey_U16(b,p);p+=2;return Monstey_ResourceText(b,p,false,count);
}
void Monstey_MenuItems(Monstey_Span b,std::size_t& p,Monstey_J& items,unsigned depth) {
    Monstey_AuditCheck(depth<32,"FEAR: menu depth limit");
    for(std::size_t count=0;count<10000;++count) {const auto at=p;const auto flags=Monstey_U16(b,p);p+=2;auto item=Monstey_J::object();
        item["offset"]=static_cast<std::uint64_t>(at);item["flags"]=static_cast<std::uint64_t>(flags);
        const bool popup=(flags&0x10)!=0;if(!popup) {item["command_id"]=static_cast<std::uint64_t>(Monstey_U16(b,p));p+=2;}
        item["text"]=Monstey_ResourceText(b,p);if(popup) {item["children"]=Monstey_J::array();Monstey_MenuItems(b,p,item["children"],depth+1);}
        items.as_array().push_back(item);if(flags&0x80) return;}
    throw Monstey_Error("fear_resource_limit","Menu item limit");
}
Monstey_J Monstey_Menu(Monstey_Span b) {
    auto result=Monstey_J::object();const auto version=Monstey_U16(b,0);result["template_version"]=static_cast<std::uint64_t>(version);
    if(version!=0) {result["supported"]=false;return result;}
    std::size_t p=4+Monstey_U16(b,2);result["supported"]=true;result["items"]=Monstey_J::array();Monstey_MenuItems(b,p,result["items"],0);result["parsed_bytes"]=static_cast<std::uint64_t>(p);return result;
}
void Monstey_Rect(Monstey_J& out,Monstey_Span b,std::size_t& p) {
    for(const char* key:{"x","y","width","height"}) {out[key]=static_cast<std::int64_t>(static_cast<std::int16_t>(Monstey_U16(b,p)));p+=2;}
}
Monstey_J Monstey_Dialog(Monstey_Span b) {
    auto d=Monstey_J::object();const bool extended=Monstey_U16(b,0)==1&&Monstey_U16(b,2)==0xffff;std::size_t p=0;std::uint32_t style=0;std::uint16_t count=0;
    if(extended) {d["template_version"]=1u;d["help_id"]=Monstey_U32(b,4);d["extended_style"]=Monstey_U32(b,8);style=Monstey_U32(b,12);count=Monstey_U16(b,16);p=18;}
    else {d["template_version"]=0u;style=Monstey_U32(b,0);d["extended_style"]=Monstey_U32(b,4);count=Monstey_U16(b,8);p=10;}
    d["style"]=style;Monstey_Rect(d,b,p);d["menu"]=Monstey_ResourceValue(b,p);d["class"]=Monstey_ResourceValue(b,p);d["title"]=Monstey_ResourceValue(b,p);
    if(style&0x40) {auto font=Monstey_J::object();font["points"]=static_cast<std::uint64_t>(Monstey_U16(b,p));p+=2;
        if(extended) {font["weight"]=static_cast<std::uint64_t>(Monstey_U16(b,p));p+=2;Monstey_CheckRange(b,p,2);font["italic"]=static_cast<std::uint64_t>(b[p++]);font["charset"]=static_cast<std::uint64_t>(b[p++]);}
        font["face"]=Monstey_ResourceText(b,p);d["font"]=font;}
    d["controls"]=Monstey_J::array();
    for(std::size_t i=0;i<count;++i) {p=(p+3)&~std::size_t(3);auto c=Monstey_J::object();
        if(extended) {c["help_id"]=Monstey_U32(b,p);c["extended_style"]=Monstey_U32(b,p+4);c["style"]=Monstey_U32(b,p+8);p+=12;}
        else {c["style"]=Monstey_U32(b,p);c["extended_style"]=Monstey_U32(b,p+4);p+=8;}
        Monstey_Rect(c,b,p);c["id"]=extended?Monstey_U32(b,p):Monstey_U16(b,p);p+=extended?4:2;
        c["class"]=Monstey_ResourceValue(b,p);c["title"]=Monstey_ResourceValue(b,p);
        const auto extra=Monstey_U16(b,p);p+=2;Monstey_CheckRange(b,p,extra);p+=extra;d["controls"].as_array().push_back(c);}
    d["control_count"]=static_cast<std::uint64_t>(count);d["parsed_bytes"]=static_cast<std::uint64_t>(p);return d;
}
Monstey_J Monstey_Resources(const Monstey_Pe& pe) {
    auto result=Monstey_J::array();if(pe.resourceRva==0||pe.resourceSize==0) return result;const auto b=pe.At(pe.resourceRva,pe.resourceSize);
    std::vector<std::string> keys;std::vector<std::uint32_t> stack;
    const auto visit=[&](auto&& self,std::uint32_t at,unsigned depth)->void {
        Monstey_AuditCheck(depth<4,"FEAR: resource depth limit");Monstey_AuditCheck(std::find(stack.begin(),stack.end(),at)==stack.end(),"FEAR: resource cycle");stack.push_back(at);
        const std::uint32_t count=static_cast<std::uint32_t>(Monstey_U16(b,static_cast<std::size_t>(at)+12))+Monstey_U16(b,static_cast<std::size_t>(at)+14);Monstey_AuditCheck(count<=10000,"FEAR: resource entry limit");
        Monstey_CheckRange(b,static_cast<std::size_t>(at)+16,static_cast<std::size_t>(count)*8);
        for(std::uint32_t i=0;i<count;++i) {const auto p=static_cast<std::size_t>(at)+16+static_cast<std::size_t>(i)*8;const auto name=Monstey_U32(b,p),target=Monstey_U32(b,p+4);keys.push_back(Monstey_ResourceKey(b,name));
            if(target&0x80000000) self(self,target&0x7fffffff,depth+1);
            else {Monstey_AuditCheck(keys.size()==3,"FEAR: wrong resource leaf depth");Monstey_AuditCheck(result.size()<100000,"FEAR: resource leaf limit");
                const auto rva=Monstey_U32(b,target),size=Monstey_U32(b,static_cast<std::size_t>(target)+4);const auto payload=pe.At(rva,size);auto r=Monstey_J::object();
                r["type"]=keys[0];r["name"]=keys[1];r["language"]=keys[2];r["rva"]=Monstey_Hex(rva);r["bytes"]=static_cast<std::uint64_t>(size);
                if(keys[0]=="4") r["menu"]=Monstey_Menu(payload);
                else if(keys[0]=="5") r["dialog"]=Monstey_Dialog(payload);
                else if(keys[0]=="6") {auto strings=Monstey_J::array();std::size_t s=0;for(std::size_t n=0;n<16;++n) {const auto len=Monstey_U16(payload,s);s+=2;if(len) {auto entry=Monstey_J::object();entry["index"]=static_cast<std::uint64_t>(n);entry["text"]=Monstey_ResourceText(payload,s,false,len);strings.as_array().push_back(entry);}}r["strings"]=strings;}
                result.as_array().push_back(r);}
            keys.pop_back();}
        stack.pop_back();
    };visit(visit,0,0);return result;
}
Monstey_J Monstey_Exports(const Monstey_Pe& pe) {
    auto out=Monstey_J::object();if(pe.exportRva==0) return out;const auto p=pe.Offset(pe.exportRva,40);
    const auto base=Monstey_U32(pe.bytes,p+16),fn=Monstey_U32(pe.bytes,p+20),names=Monstey_U32(pe.bytes,p+24);
    Monstey_AuditCheck(fn<=100000&&names<=fn,"FEAR: export count");const auto f=pe.Offset(Monstey_U32(pe.bytes,p+28),static_cast<std::size_t>(fn)*4);
    const auto n=pe.Offset(Monstey_U32(pe.bytes,p+32),static_cast<std::size_t>(names)*4),o=pe.Offset(Monstey_U32(pe.bytes,p+36),static_cast<std::size_t>(names)*2);
    for(std::size_t i=0;i<names;++i) {const auto ordinal=Monstey_U16(pe.bytes,o+i*2);Monstey_AuditCheck(ordinal<fn,"FEAR: export ordinal");
        const auto name=Monstey_Cstr(pe.bytes,pe.Offset(Monstey_U32(pe.bytes,n+i*4)));const auto rva=Monstey_U32(pe.bytes,f+static_cast<std::size_t>(ordinal)*4);auto row=Monstey_J::object();row["ordinal"]=static_cast<std::uint64_t>(base+ordinal);row["rva"]=Monstey_Hex(rva);row["executable"]=pe.Executable(rva);out[name]=row;}
    return out;
}
Monstey_J Monstey_MfcCommands(const Monstey_Pe& pe,const Monstey_J& resources) {
    std::map<std::uint32_t,std::string> labels;
    const auto collect=[&](auto&& self,const Monstey_J& items,const std::string& prefix)->void {
        for(const auto& item:items.as_array()) {const auto name=prefix+"/"+item["text"].as_string();
            if(item.contains("children")) self(self,item["children"],name);
            else if(item.contains("command_id")&&item["command_id"].as_uint()!=0) labels[static_cast<std::uint32_t>(item["command_id"].as_uint())]=name;}
    };
    for(const auto& r:resources.as_array()) if(r.contains("menu")&&r["menu"]["supported"].as_bool()) collect(collect,r["menu"]["items"],"");
    auto result=Monstey_J::array();
    for(const auto& section:pe.sections) {
        if((section.flags&0x20000000)!=0||section.size<24) continue;
        for(std::size_t delta=0;delta<=static_cast<std::size_t>(section.size)-24;delta+=4) {const auto p=static_cast<std::size_t>(section.raw)+delta;
            if(Monstey_U32(pe.bytes,p)!=0x111) continue;
            const auto code=Monstey_U32(pe.bytes,p+4),id=Monstey_U32(pe.bytes,p+8),last=Monstey_U32(pe.bytes,p+12),sig=Monstey_U32(pe.bytes,p+16),fn=Monstey_U32(pe.bytes,p+20);
            if(!labels.contains(id)||id!=last||!((code==0&&sig==0x35)||(code==0xffffffff&&sig==0x3d))||fn<pe.imageBase||!pe.Executable(static_cast<std::uint32_t>(fn-pe.imageBase))) continue;
            auto row=Monstey_J::object();row["entry_rva"]=Monstey_Hex(static_cast<std::uint64_t>(section.va)+delta);row["message"]=0x111u;row["command_id"]=id;row["label"]=labels.at(id);
            row["notification_code"]=Monstey_Hex(code);row["signature"]=Monstey_Hex(sig);row["handler_rva"]=Monstey_Hex(fn-pe.imageBase);
            row["route"]=code==0?"command":"update_command_ui";row["entry_bytes"]=Monstey_ByteHex(pe.bytes.subspan(p,24));result.as_array().push_back(row);
        }
    }return result;
}
Monstey_J Monstey_Imports(const Monstey_Pe& pe) {
    auto result=Monstey_J::array();if(pe.importRva==0||pe.importSize==0) return result;
    const auto directory=pe.At(pe.importRva,pe.importSize);
    for(std::size_t d=0;d+20<=directory.size();d+=20) {const auto original=Monstey_U32(directory,d),name=Monstey_U32(directory,d+12),iat=Monstey_U32(directory,d+16);
        if(original==0&&name==0&&iat==0) return result;
        const auto library=Monstey_Cstr(pe.bytes,pe.Offset(name));const auto table=original?original:iat;const std::size_t width=pe.pe64?8:4;
        bool ended=false;
        for(std::uint32_t i=0;i<100000;++i) {const std::uint64_t tr=static_cast<std::uint64_t>(table)+static_cast<std::uint64_t>(i)*width;
            Monstey_AuditCheck(tr<=std::numeric_limits<std::uint32_t>::max(),"FEAR: import RVA overflow");const auto thunk=pe.At(static_cast<std::uint32_t>(tr),width);const auto value=pe.pe64?Monstey_U64(thunk,0):Monstey_U32(thunk,0);
            if(value==0) {ended=true;break;}auto row=Monstey_J::object();row["library"]=library;row["iat_rva"]=Monstey_Hex(static_cast<std::uint64_t>(iat)+static_cast<std::uint64_t>(i)*width);
            if(value&(pe.pe64?0x8000000000000000ull:0x80000000ull)) row["ordinal"]=static_cast<std::uint64_t>(value&0xffff);
            else {Monstey_AuditCheck(value<=std::numeric_limits<std::uint32_t>::max(),"FEAR: import name RVA overflow");const auto at=pe.Offset(static_cast<std::uint32_t>(value),3);row["symbol"]=Monstey_Cstr(pe.bytes,at+2);}
            result.as_array().push_back(row);}
        Monstey_AuditCheck(ended,"FEAR: unbounded import thunk array");
    }
    throw Monstey_Error("fear_import_directory","Import directory has no terminator");
}
Monstey_J Monstey_Image(Monstey_Span bytes) {Monstey_Pe pe(bytes);auto r=Monstey_J::object();r["machine"]=Monstey_Hex(pe.machine);r["pe64"]=pe.pe64;r["image_base"]=Monstey_Hex(pe.imageBase);r["sections"]=static_cast<std::uint64_t>(pe.sections.size());r["exports"]=Monstey_Exports(pe);r["imports"]=Monstey_Imports(pe);r["resources"]=Monstey_Resources(pe);r["mfc_command_routes"]=Monstey_MfcCommands(pe,r["resources"]);return r;}
struct Monstey_LegacyAttribute {std::uint32_t name=0,type=0,usage=0;std::vector<std::uint32_t> values;};
struct Monstey_LegacyRecord {std::uint32_t name=0;std::vector<Monstey_LegacyAttribute> attributes;};
struct Monstey_LegacyCategory {std::uint32_t name=0;std::vector<Monstey_LegacyRecord> records;};
}
std::span<const std::uint8_t> Monstey_FearPeRange(std::span<const std::uint8_t> rawPe,
                                               std::uint64_t virtualAddress, std::size_t count) {
    const Monstey_Pe pe(rawPe);
    Monstey_Require(virtualAddress >= pe.imageBase && virtualAddress - pe.imageBase <= UINT32_MAX,
                    "Virtual address lies outside the PE image RVA space");
    return pe.At(static_cast<std::uint32_t>(virtualAddress - pe.imageBase), count);
}
Monstey_Json Monstey_FearInspectPe(const std::filesystem::path& path) {
    const auto bytes=Monstey_ReadFile(path);auto result=Monstey_Image(bytes);result["path"]=Monstey_Utf8(path.wstring());result["bytes"]=static_cast<std::uint64_t>(bytes.size());result["sha256"]=Monstey_HashSha256(bytes);result["executed"]=false;return result;
}
Monstey_Json Monstey_FearInspectDatabaseBytes(std::span<const std::uint8_t> bytes,bool includeRecords) {
    Monstey_AuditCheck(Monstey_U32(bytes,0)==0x42444147&&Monstey_U32(bytes,4)==3,"FEAR: expected GADB version 3");
    const auto poolBytes=Monstey_U32(bytes,8),catCount=Monstey_U32(bytes,12),recordCount=Monstey_U32(bytes,16),attributeCount=Monstey_U32(bytes,20),valueCount=Monstey_U32(bytes,24);
    const std::uint64_t expected=28ull+poolBytes+4ull+8ull*catCount+8ull*recordCount+16ull*attributeCount+4ull*valueCount;
    Monstey_AuditCheck(expected==bytes.size(),"FEAR: GADB3 count-derived file length mismatch");Monstey_CheckRange(bytes,28,poolBytes);const auto pool=bytes.subspan(28,poolBytes);std::size_t p=28+static_cast<std::size_t>(poolBytes);
    Monstey_AuditCheck(Monstey_U32(bytes,p)==catCount,"FEAR: GADB3 root count mismatch");p+=4;
    const auto word=[&](){const auto v=Monstey_U32(bytes,p);p+=4;return v;};
    std::vector<Monstey_LegacyCategory> categories;std::uint64_t actualRecords=0,actualAttributes=0,actualValues=0;std::array<std::uint64_t,11> typeCounts{};
    const auto sortedName=[&](const std::string& previous,std::uint32_t offset,bool first){const auto name=Monstey_Fold(Monstey_Cstr(pool,offset));Monstey_AuditCheck(first||previous<name,"FEAR: GADB3 named array is not strictly case-insensitive sorted");return name;};
    std::string prevCategory;
    for(std::uint32_t ci=0;ci<catCount;++ci) {Monstey_LegacyCategory c;c.name=word();const auto records=word();prevCategory=sortedName(prevCategory,c.name,ci==0);
        Monstey_AuditCheck(records<=recordCount-actualRecords,"FEAR: GADB3 record count exceeds header");std::string prevRecord;
        for(std::uint32_t ri=0;ri<records;++ri) {Monstey_LegacyRecord r;r.name=word();const auto attributes=word();prevRecord=sortedName(prevRecord,r.name,ri==0);
            Monstey_AuditCheck(attributes<=attributeCount-actualAttributes,"FEAR: GADB3 attribute count exceeds header");std::string prevAttribute;
            for(std::uint32_t ai=0;ai<attributes;++ai) {Monstey_LegacyAttribute a;a.name=word();a.type=word();a.usage=word();const auto count=word();prevAttribute=sortedName(prevAttribute,a.name,ai==0);
                Monstey_AuditCheck(a.type>=1&&a.type<=10,"FEAR: invalid GADB3 attribute type");Monstey_AuditCheck(count<=65535&&count<=valueCount-actualValues,"FEAR: GADB3 value count exceeds header or UINT16");
                ++typeCounts[a.type];for(std::uint32_t vi=0;vi<count;++vi) {const auto v=word();a.values.push_back(v);if(a.type>=4&&a.type<=8) {
                    if(a.type==4) (void)Monstey_Cstr(pool,v);else if(a.type==5) {std::size_t at=v;(void)Monstey_ResourceText(pool,at);}
                    else Monstey_CheckRange(pool,v,static_cast<std::size_t>(a.type-4)*4);}}
                actualValues+=count;r.attributes.push_back(std::move(a));}
            actualAttributes+=attributes;c.records.push_back(std::move(r));}
        actualRecords+=records;categories.push_back(std::move(c));}
    Monstey_AuditCheck(actualRecords==recordCount&&actualAttributes==attributeCount&&actualValues==valueCount&&p==bytes.size(),"FEAR: GADB3 nested totals mismatch");
    auto result=Monstey_J::object();result["version"]=3u;result["header_bytes"]=28u;result["pool_bytes"]=poolBytes;result["categories_count"]=catCount;result["records_count"]=recordCount;result["attributes_count"]=attributeCount;result["values_count"]=valueCount;result["bytes"]=static_cast<std::uint64_t>(bytes.size());result["reference_encoding"]="category << 16 | record";result["name_lookup"]="case-insensitive binary search (_stricmp), not hash397";
    result["types"]=Monstey_J::object();for(std::size_t i=1;i<typeCounts.size();++i) if(typeCounts[i]) result["types"][std::to_string(i)]=typeCounts[i];result["categories"]=Monstey_J::array();std::uint64_t refs=0,invalidRefs=0,nullRefs=0;
    for(std::size_t ci=0;ci<categories.size();++ci) {const auto& c=categories[ci];auto category=Monstey_J::object();category["index"]=static_cast<std::uint64_t>(ci);category["name"]=Monstey_Cstr(pool,c.name);category["record_count"]=static_cast<std::uint64_t>(c.records.size());if(includeRecords) category["records"]=Monstey_J::array();
        for(std::size_t ri=0;ri<c.records.size();++ri) {const auto& r=c.records[ri];auto record=Monstey_J::object();record["index"]=static_cast<std::uint64_t>(ri);record["name"]=Monstey_Cstr(pool,r.name);record["attributes"]=Monstey_J::array();
            for(const auto& a:r.attributes) {auto attr=Monstey_J::object();attr["name"]=Monstey_Cstr(pool,a.name);attr["type"]=a.type;attr["usage"]=a.usage;attr["raw_words"]=Monstey_J::array();
                for(const auto v:a.values) {if(includeRecords) attr["raw_words"].as_array().push_back(static_cast<std::uint64_t>(v));if(a.type==9) {++refs;const auto cat=v>>16,rec=v&0xffffu;if(v==0xffffffff) ++nullRefs;else if(cat>=categories.size()||rec>=categories[cat].records.size()) ++invalidRefs;}}
                if(includeRecords) record["attributes"].as_array().push_back(attr);}
            if(includeRecords) category["records"].as_array().push_back(record);}
        result["categories"].as_array().push_back(category);}
    result["record_link_words"]=refs;result["null_record_links"]=nullRefs;result["invalid_non_null_record_links"]=invalidRefs;result["null_or_out_of_range_links"]=nullRefs+invalidRefs;result["validated"]=true;result["runtime_verified"]=false;return result;
}
Monstey_Json Monstey_FearInspectDatabase(const std::filesystem::path& path,bool includeRecords) {
    const auto bytes=Monstey_ReadFile(path);auto result=Monstey_FearInspectDatabaseBytes(bytes,includeRecords);result["path"]=Monstey_Utf8(path.wstring());result["sha256"]=Monstey_HashSha256(bytes);return result;
}
Monstey_Json Monstey_FearAudit(const std::filesystem::path& sdkRoot) {
    auto result=Monstey_J::object();result["method"]="C++ static PE, resources and GADB3 parsing. No original image executed.";result["originals"]=Monstey_J::array();
    const auto database=sdkRoot/L"Dev/Runtime/GameDatabase.dll",archive=sdkRoot/L"Dev/Tools/ArchiveEdit.exe";
    Monstey_AuditCheck(Monstey_HashSha256(database)=="d1e9fa7df74fd4d1bcbb52ce7fe7e757e41d5732dafcbbf4adb58426df10e7f6","FEAR: database DLL hash is not qualified");
    Monstey_AuditCheck(Monstey_HashSha256(archive)=="05fce451b444d6fe1c0add2f95d175cac9af4e21223887255f144d4125cb4a13","FEAR: archive editor hash is not qualified");
    const auto raw=Monstey_ReadFile(database);const Monstey_Pe pe(raw);Monstey_AuditCheck(pe.machine==0x14c&&!pe.pe64,"FEAR: expected x86 database DLL");
    const auto exported=Monstey_Exports(pe),imported=Monstey_Imports(pe);
    Monstey_AuditCheck(exported["GetIDatabaseMgr"]["rva"].as_string()==Monstey_Hex(0x9d10)&&exported["GetIDatabaseCreatorMgr"]["rva"].as_string()==Monstey_Hex(0x9c90)&&exported["GetIDatabaseCreatorUnpackedMgr"]["rva"].as_string()==Monstey_Hex(0x9cd0),"FEAR: factory export mismatch");
    bool comparator=false;for(const auto& row:imported.as_array()) if(row["iat_rva"].as_string()==Monstey_Hex(0xb0f8)&&row.contains("symbol")&&row["symbol"].as_string()=="_stricmp"&&row["library"].as_string()=="MSVCR71.dll") comparator=true;
    Monstey_AuditCheck(comparator,"FEAR: named comparator import mismatch");    struct Monstey_Predicate {const char* name;std::uint32_t rva;const char* hex;};
    constexpr Monstey_Predicate checks[]={
        {"header_read_28",0x39aa,"6a1c"},{"header_gadb",0x39be,"817c240447414442"},{"header_version3",0x39cc,"837c240803"},
        {"pool_relative_type_low4",0x6e84,"83f804"},{"pool_relative_type_high8",0x6e89,"83f808"},
        {"reference_category_shift16",0x6f4b,"c1e810"},{"reference_record_mask16",0x6f4e,"81e2ffff0000"},
        {"named_binarysearch_stride16",0x7040,"c1e004"},{"named_comparator_iat",0x7056,"ff15f8b00010"},
        {"writer_magic",0x729b,"c744243047414442"},{"writer_version3",0x72a3,"c744243403000000"},
        {"category_disk_header8",0x37f8,"6a08"},{"record_disk_header8",0x3758,"6a08"},{"attribute_disk_header16",0x3688,"6a10"},
        {"overload_record_slot13_calls_category_slot7",0x665c,"ff501c"},{"overload_attribute_slot20_calls_record_slot14",0x66ec,"ff5038"},
        {"overload_attribute_slot21_calls_record_slot13",0x66c1,"ff5034"}};
    result["instruction_checks"]=Monstey_J::array();
    for(const auto& check:checks) {const auto observed=Monstey_ByteHex(pe.At(check.rva,std::strlen(check.hex)/2));Monstey_AuditCheck(observed==check.hex,"FEAR: original instruction predicate failed");auto row=Monstey_J::object();row["name"]=check.name;row["rva"]=Monstey_Hex(check.rva);row["bytes"]=observed;row["pass"]=true;result["instruction_checks"].as_array().push_back(row);}
    constexpr std::array<std::uint32_t,3> tables={0xb330,0xb150,0xb20c};constexpr std::array<unsigned,3> counts={40,16,15};constexpr std::array<const char*,3> interfaces={"IDatabaseMgr","IDatabaseCreatorMgr","IDatabaseCreatorUnpackedMgr"};
    result["vtables"]=Monstey_J::array();
    for(std::size_t i=0;i<tables.size();++i) {auto table=Monstey_J::object();table["interface"]=interfaces[i];table["rva"]=Monstey_Hex(tables[i]);table["slots"]=Monstey_J::array();const auto block=pe.At(tables[i],counts[i]*4);
        for(unsigned n=0;n<counts[i];++n) {const auto va=Monstey_U32(block,n*4);Monstey_AuditCheck(va>=pe.imageBase&&pe.Executable(static_cast<std::uint32_t>(va-pe.imageBase)),"FEAR: non-code vtable target");auto slot=Monstey_J::object();slot["slot"]=n;slot["rva"]=Monstey_Hex(va-pe.imageBase);table["slots"].as_array().push_back(slot);}result["vtables"].as_array().push_back(table);}
    constexpr std::array<const wchar_t*,6> tools={L"ArchiveEdit.exe",L"GDBEdit.exe",L"ModelEdit.exe",L"WorldEdit.exe",L"FXEdit.exe",L"StringEdit.exe"};
    constexpr std::array<const char*,6> hashes={
        "05fce451b444d6fe1c0add2f95d175cac9af4e21223887255f144d4125cb4a13",
        "83137f4210dc2e985e72f9bef6c6df2924c40ab8b5d867083007aa262a6ddaaa",
        "7d35ea6ce0b31166f4600075370ce673bcda5f694053df433cd78892f06249e3",
        "18576c1014bc9cff2340b7cc00bf84c5f7d47f0492b901087f0872397af81f32",
        "392ee341cd1a356b1c5b92d78bd1e8f3ff04e0da95db3e29cd6bc027a317ade9",
        "e0b7d4a04f5290205dc6a474acb945e1efa4c1397ed87060e241afb756194733"};
    for(std::size_t i=0;i<tools.size();++i) {auto original=Monstey_FearInspectPe(sdkRoot/L"Dev/Tools"/tools[i]);Monstey_AuditCheck(original["sha256"].as_string()==hashes[i],"FEAR: editor hash is not qualified");result["originals"].as_array().push_back(original);}
    result["originals"].as_array().push_back(Monstey_FearInspectPe(database));result["databases"]=Monstey_J::array();
    for(const wchar_t* relative:{L"Database/FEAR.Gamdb00p",L"AnimationDatabase/FEAR_anim.Gamdb00p",L"databaselocalized/FEARL.Gamdb00p",L"profiledatabase/Defaults.Gamdb00p"}) result["databases"].as_array().push_back(Monstey_FearInspectDatabase(sdkRoot/L"Dev/Runtime/Game"/relative));
    result["status"]="PASS";result["input_modified"]=false;result["runtime_verified"]=false;return result;
}
}
