#include "Monstey_AnimationProject.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_ECheck(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
void Monstey_EReject(const std::function<void()> &action) {
    try { action(); } catch (const Monstey_Error &) { return; }
    throw std::runtime_error("Expected bounded validation failure");
}
std::string Monstey_EHex(std::string_view bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const unsigned char value : bytes) { result.push_back(digits[value >> 4]); result.push_back(digits[value & 15]); }
    return result;
}
std::string Monstey_EBytes(const J &event) {
    if (!event.contains("text_bytes_hex")) return event.at("text").as_string();
    const auto &hex = event.at("text_bytes_hex").as_string();
    std::string bytes;
    auto digit = [](char value) -> unsigned { return value >= '0' && value <= '9' ? static_cast<unsigned>(value-'0') : static_cast<unsigned>(value-'a'+10); };
    Monstey_ECheck(hex.size() % 2 == 0, "Output hex has odd width");
    for (std::size_t i = 0; i < hex.size(); i += 2) bytes.push_back(static_cast<char>((digit(hex[i]) << 4) | digit(hex[i+1])));
    return bytes;
}
J Monstey_EEvent(unsigned tick, std::string_view bytes, bool mirror = true) {
    auto event = J::object({{"tick", tick}});
    if (std::any_of(bytes.begin(), bytes.end(), [](unsigned char value) { return value >= 128; })) {
        event["text_bytes_hex"] = Monstey_EHex(bytes);
        if (mirror) event["text"] = Monstey_DisplayUtf8(bytes);
    } else event["text"] = bytes;
    return event;
}
J Monstey_EScene(const J &events = J::array()) {
    auto clip = [](const char *name) {
        return J::object({{"name", name}, {"duration_ticks", 100}, {"ticks", J::array({0,100})},
            {"tracks", J::array({J::object({{"channel_index", 0}, {"values", J::array({J::array({0,0,0,1}),J::array({0,0,0,1})})}}),
                J::object({{"channel_index", 1}, {"values", J::array({J::array({0,0,0}),J::array({10,0,0})})}})})}, {"events", J::array()}});
    };
    auto walk = clip("walk"); walk["events"] = events;
    return J::object({{"library_path", "synthetic/event-bytes.anix"},
        {"channels", J::array({J::object({{"type_hash", "27436396"}, {"bone_hash", "88baa7e7"}}),
            J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "88baa7e7"}}),
            J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "12345678"}})})},
        {"clips", J::array({clip("bind"), walk})}});
}
J Monstey_EOperation(const J &events, std::string_view clip = "walk") {
    return J::array({J::object({{"op", "events"}, {"clip", clip}, {"events", events}})});
}
B Monstey_EFooterPool(const B &bytes) {
    const auto meta = static_cast<std::size_t>(Monstey_U32(bytes,32));
    const auto footer = meta + Monstey_U32(bytes,meta+12);
    const auto first = footer+24+Monstey_U32(bytes,footer+16), end=footer+24+Monstey_U32(bytes,footer);
    Monstey_ECheck(first <= end, "Footer string extent reversed"); Monstey_CheckRange(bytes,first,end-first);
    return B(bytes.begin()+first,bytes.begin()+end);
}
std::string Monstey_EString(const B &bytes, std::size_t first, std::size_t end) {
    Monstey_CheckRange(bytes,first,end-first);
    std::string text;
    while (first < end && bytes[first]) text.push_back(static_cast<char>(bytes[first++]));
    Monstey_ECheck(first < end, "Raw test string is unterminated"); return text;
}
struct Monstey_EDescriptor { std::string name; std::size_t at=0,end=0; };
std::vector<Monstey_EDescriptor> Monstey_EDescriptors(const B &bytes) {
    const auto meta=static_cast<std::size_t>(Monstey_U32(bytes,32)); const auto count=Monstey_U32(bytes,meta+4);
    const auto footer=meta+Monstey_U32(bytes,meta+12); std::vector<std::size_t> positions;
    for (std::size_t i=0;i<count;++i) positions.push_back(meta+Monstey_U32(bytes,meta+20+8*i));
    auto sorted=positions; std::sort(sorted.begin(),sorted.end()); std::vector<Monstey_EDescriptor> out;
    for (const auto at:positions) { auto next=std::upper_bound(sorted.begin(),sorted.end(),at); const auto end=next==sorted.end()?footer:*next;
        out.push_back({Monstey_EString(bytes,at,at+48),at,end}); }
    return out;
}
struct Monstey_ERawEvent { std::uint32_t tick=0; std::string bytes; std::uint16_t offset=0; };
std::vector<Monstey_ERawEvent> Monstey_ERawEvents(const B &bytes, const Monstey_EDescriptor &descriptor) {
    const auto at=descriptor.at; const auto nc=Monstey_U16(bytes,at+56), nt=Monstey_U16(bytes,at+58), ns=Monstey_U16(bytes,at+52), ne=Monstey_U16(bytes,at+54);
    std::size_t code=nc;
    for(std::size_t k=0;k<nc;++k) if(std::string_view(reinterpret_cast<const char*>(bytes.data()+at+124+4*k),4)=="STRI") code=k;
    if(code==nc || ns==0) return {};
    const auto ids=(124+4*static_cast<std::size_t>(nc)+nt+3)/4*4;
    const auto segments=(ids+2*nt+3)/4*4, extras=segments+16*ns; std::size_t pool=0;
    for(std::size_t k=0;k<ne;++k) if(Monstey_U32(bytes,at+extras+8*k)==0xb9010be0) pool=at+Monstey_U32(bytes,at+extras+8*k+4);
    Monstey_ECheck(pool>=at && pool<descriptor.end,"Missing bounded KeyString");
    const auto segment=static_cast<std::size_t>(Monstey_U32(bytes,24))+Monstey_U32(bytes,at+segments);
    const auto segment_size=Monstey_U32(bytes,at+segments+4); Monstey_CheckRange(bytes,segment,segment_size);
    const auto block=segment+Monstey_U16(bytes,segment+4+2*code);
    const auto pool_size=Monstey_U16(bytes,block+2); Monstey_ECheck(pool_size<=descriptor.end-pool,"Declared pool exceeds descriptor");
    const auto count=Monstey_U32(bytes,block+4); Monstey_CheckRange(bytes,block+10,8ull*count);
    std::vector<Monstey_ERawEvent> events;
    for(std::size_t k=0;k<count;++k) {const auto entry=block+10+8*k;const auto offset=Monstey_U16(bytes,entry+4),length=Monstey_U16(bytes,entry+6);
        Monstey_ECheck(length>0 && offset<=pool_size && length<=pool_size-offset,"Event string exceeds declared pool");
        Monstey_ECheck(bytes[pool+offset+length-1]==0,"Raw event lost final NUL");
        events.push_back({Monstey_U32(bytes,entry),Monstey_EString(bytes,pool+offset,pool+offset+length),offset}); }
    return events;
}
std::vector<Monstey_ERawEvent> Monstey_ERawEvents(const B &bytes, std::string_view name) {
    for(const auto &descriptor:Monstey_EDescriptors(bytes)) if(descriptor.name==name) return Monstey_ERawEvents(bytes,descriptor);
    throw std::runtime_error("Raw clip not found");
}
void Monstey_ECompare(const J &events, std::vector<Monstey_ERawEvent> raw) {
    std::stable_sort(raw.begin(),raw.end(),[](const auto &a,const auto &b){return a.tick<b.tick;});
    Monstey_ECheck(events.size()==raw.size(),"Event count changed");
    for(std::size_t i=0;i<raw.size();++i) {
        Monstey_ECheck(events[i]["tick"].as_uint()==raw[i].tick && Monstey_EBytes(events[i])==raw[i].bytes,"Event native bytes/tick changed");
        const bool high=std::any_of(raw[i].bytes.begin(),raw[i].bytes.end(),[](unsigned char value){return value>=128;});
        Monstey_ECheck(events[i].contains("text_bytes_hex")==high,"Canonical raw metadata/ASCII shape changed");
        Monstey_ECheck(events[i]["text"].as_string()==Monstey_DisplayUtf8(raw[i].bytes),"Raw display mirror is not canonical");
    }
}
}
int wmain(int argc,wchar_t **argv) {
    if(argc<2) return 2;
    const auto output=std::filesystem::path(argv[1])/("EventBytes_"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(output);
    J checks=J::array(),actual=J::array();std::size_t failures=0;
    auto test=[&](std::string name,const std::function<void()> &action){try{action();checks.as_array().push_back(J::object({{"name",name},{"pass",true}}));}
        catch(const std::exception &error){++failures;checks.as_array().push_back(J::object({{"name",name},{"pass",false},{"error",error.what()}}));std::cerr<<name<<": "<<error.what()<<"\n";}};
    std::vector<std::string> samples{std::string(1,static_cast<char>(0xa0)),std::string("\xc2\xa0"),std::string("\\xa0"),std::string("ASCII; exact  \t"),std::string("\xe2\x82\xac"),std::string("\xed\xa0\x80"),std::string("\xff\xfe"),{}};
    for(unsigned value=1;value<=255;++value) samples.back().push_back(static_cast<char>(value));
    J all=J::array(); for(std::size_t i=0;i<samples.size();++i) all.as_array().push_back(Monstey_EEvent(static_cast<unsigned>(i*10),samples[i]));
    test("eight-payloads-native-json-semantic-roundtrip",[&]{
        auto library=Monstey_Anix::Create(Monstey_EScene(all));const auto original=library.Build();
        Monstey_Anix reopened(original);Monstey_ECheck(reopened.Build()==original,"No-op build changed raw bytes");
        auto scene=J::Parse(reopened.ExportClip("walk").dump());Monstey_ECompare(scene["events"],Monstey_ERawEvents(original,"walk"));
        Monstey_ECheck(scene["events"][0]["text"]==scene["events"][2]["text"] && Monstey_EBytes(scene["events"][0])!=Monstey_EBytes(scene["events"][2]),"Display collision was not exercised");
        Monstey_ECheck(Monstey_EBytes(scene["events"][0])!=Monstey_EBytes(scene["events"][1]),"A0 and UTF8 C2A0 collapsed");
        scene["name"]="copy";library.Edit(J::array({J::object({{"op","import"},{"clip",scene}})}));
        Monstey_ECompare(library.EventInfo("copy")["events"],Monstey_ERawEvents(original,"walk"));
        const auto info=J::Parse(library.EventInfo("copy").dump());Monstey_ECompare(info["events"],Monstey_ERawEvents(original,"walk"));
        (void)J::Parse(library.Export().dump());
    });
    test("ascii-output-shape-and-global-json-path-contracts",[&]{
        auto library=Monstey_Anix::Create(Monstey_EScene(J::array({Monstey_EEvent(0,"ASCII; text  ")})));
        Monstey_ECheck(library.EventInfo("walk")["events"]==J::array({J::object({{"tick",0},{"text","ASCII; text  "}})}),"ASCII event JSON shape changed");
        Monstey_EReject([&]{(void)J(samples[0]).dump();});
        auto scene=Monstey_EScene();scene["library_path"]=samples[1];Monstey_EReject([&]{(void)Monstey_Anix::Create(scene);});
        scene=Monstey_EScene();scene["clips"][1]["name"]=samples[1];Monstey_EReject([&]{(void)Monstey_Anix::Create(scene);});
        Monstey_EReject([&]{library.Edit(Monstey_EOperation(J::array({J::object({{"tick",0},{"text",samples[1]}})})));});
    });
    test("raw-input-types-mirrors-digits-nul-and-width-transactional",[&]{
        auto library=Monstey_Anix::Create(Monstey_EScene(all));const auto baseline=library.Build();
        std::vector<J> invalid{J::object({{"tick",0},{"text_bytes_hex",""}}),J::object({{"tick",0},{"text_bytes_hex","a"}}),J::object({{"tick",0},{"text_bytes_hex","zz"}}),J::object({{"tick",0},{"text_bytes_hex","0xa0"}}),J::object({{"tick",0},{"text_bytes_hex","a0  "}}),J::object({{"tick",0},{"text_bytes_hex","00"}}),J::object({{"tick",0},{"text_bytes_hex","610062"}}),J::object({{"tick",0},{"text_bytes_hex",5}}),J::object({{"tick",0},{"text_bytes_hex",nullptr}}),J::object({{"tick",0},{"text_bytes_hex","a0"},{"text","changed display"}}),J::object({{"tick",0},{"text_bytes_hex","a0"},{"text",nullptr}}),J::object({{"tick",0},{"text_bytes_hex","a0"},{"text",5}}),J::object({{"tick",0},{"text",""}}),J::object({{"tick",0},{"text",5}}),J::object({{"tick",0},{"text",std::string("a\0b",3)}}),J::object({{"tick",0},{"text",std::string(65535,'A')}}),J::object({{"tick",0},{"text_bytes_hex",std::string(131070,'a')}})};
        for(const auto &event:invalid){Monstey_EReject([&]{library.Edit(Monstey_EOperation(J::array({event})));});Monstey_ECheck(library.Build()==baseline,"Rejected raw event changed state");}
        auto raw=J::object({{"tick",1},{"text_bytes_hex","A0"}});library.Edit(Monstey_EOperation(J::array({raw})));
        const auto canonical=library.EventInfo("walk")["events"][0];Monstey_ECheck(canonical["text_bytes_hex"].as_string()=="a0" && canonical["text"].as_string()=="\\xa0","Raw-only/uppercase hex did not canonicalize");
        raw["tick"]=2;library.Edit(Monstey_EOperation(J::array({raw})));Monstey_ECheck(Monstey_EBytes(library.EventInfo("walk")["events"][0])==samples[0],"Raw-only tick edit lost source byte");
    });
    test("native-u16-string-and-pool-boundaries",[&]{
        auto library=Monstey_Anix::Create(Monstey_EScene());const std::string maximum(65534,static_cast<char>(0xab));
        library.Edit(Monstey_EOperation(J::array({Monstey_EEvent(0,maximum,false)})));
        Monstey_ECheck(Monstey_EBytes(library.EventInfo("walk")["events"][0])==maximum,"Largest native string truncated");
        const auto before=library.Build();
        const auto overflow=J::array({Monstey_EEvent(0,std::string(32767,'A')),Monstey_EEvent(1,std::string(32767,'B'))});
        Monstey_EReject([&]{library.Edit(Monstey_EOperation(overflow));});Monstey_ECheck(library.Build()==before,"Pool overflow changed state");
    });
    test("raw-byte-dedup-and-display-collision-stay-distinct",[&]{
        auto events=J::array({Monstey_EEvent(0,samples[0]),Monstey_EEvent(1,samples[0]),Monstey_EEvent(2,samples[2])});
        const auto bytes=Monstey_Anix::Create(Monstey_EScene(events)).Build();const auto raw=Monstey_ERawEvents(bytes,"walk");
        Monstey_ECheck(raw[0].offset==raw[1].offset && raw[2].offset!=raw[0].offset && raw[0].bytes!=raw[2].bytes,"Dedup used display instead of native bytes");
    });
    test("unused-raw-footer-strings-survive-removal-and-rename",[&]{
        auto library=Monstey_Anix::Create(Monstey_EScene(all));const auto original=library.Build();const auto pool=Monstey_EFooterPool(original);
        library.Edit(Monstey_EOperation(J::array()));const auto removed=library.Build();
        Monstey_ECheck(Monstey_EFooterPool(removed)==pool,"Removing events discarded unused raw footer strings");
        library.Edit(J::array({J::object({{"op","rename"},{"clip","walk"},{"name","renamed"}})}));
        Monstey_ECheck(Monstey_EFooterPool(library.Build())==pool,"Rename changed unused raw footer strings");
        Monstey_Anix reopened(library.Build());Monstey_ECheck(reopened.EventInfo("renamed")["events"].empty(),"Removed event binding returned");
    });
    test("timeline-preserves-raw-event-objects-and-changes-only-ticks",[&]{
        const auto events=J::array({Monstey_EEvent(10,samples[0]),Monstey_EEvent(60,samples[1]),Monstey_EEvent(90,samples[2])});
        const std::vector<J> ops{J::object({{"op","rate"},{"clip","walk"},{"factor",2.0}}),J::object({{"op","length"},{"clip","walk"},{"duration_ticks",200}}),J::object({{"op","delay"},{"clip","walk"},{"at_tick",50},{"duration_ticks",20}}),J::object({{"op","trim"},{"clip","walk"},{"start_tick",20},{"end_tick",100}})};
        for(std::size_t k=0;k<ops.size();++k){auto library=Monstey_Anix::Create(Monstey_EScene(events));library.Edit(J::array({ops[k]}));const auto after=library.EventInfo("walk")["events"];
            const auto begin=k==3?1u:0u;Monstey_ECheck(after.size()==events.size()-begin,"Timeline event count differs");
            for(std::size_t e=begin;e<events.size();++e){Monstey_ECheck(Monstey_EBytes(after[e-begin])==Monstey_EBytes(events[e]),"Timeline changed raw bytes");const auto old=events[e]["tick"].as_uint();const auto expected=k==0?old/2:k==1?old*2:k==2?old+(old>=50?20:0):old-20;Monstey_ECheck(after[e-begin]["tick"].as_uint()==expected,"Timeline event tick differs");}}
    });
    test("project-journal-export-import-undo-redo-replay-preserve-raw",[&]{
        const auto source=output/L"synthetic-source.anix";Monstey_WriteNewFile(source,Monstey_Anix::Create(Monstey_EScene()).Build());const auto source_hash=Monstey_HashSha256(source);
        Monstey_AnimationProject project(source);auto raw_only=all;for(auto &event:raw_only.as_array()) if(event.contains("text_bytes_hex")) event.as_object().erase("text");project.Edit(Monstey_EOperation(raw_only));const auto changed=project.Clip("walk")["events"];
        Monstey_ECompare(changed,Monstey_ERawEvents(Monstey_Anix::Create(Monstey_EScene(all)).Build(),"walk"));
        project.Undo();Monstey_ECheck(project.Clip("walk")["events"].empty(),"Undo failed");project.Redo();Monstey_ECheck(project.Clip("walk")["events"]==changed,"Redo lost raw metadata");
        const auto saved=output/L"project.json";project.SaveProject(saved);auto replay=Monstey_AnimationProject::LoadProject(saved);
        Monstey_ECheck(replay->Clip("walk")["events"]==changed,"Project replay changed raw events");
        const auto a=output/L"project-output.anix",b=output/L"replay-output.anix";project.Build(a);replay->Build(b);Monstey_ECheck(Monstey_ReadFile(a)==Monstey_ReadFile(b),"Replay build differs");
        const auto scene=output/L"walk.json";project.Export(scene,"walk");project.ImportClip(scene,"walk","raw_import_copy");
        Monstey_ECheck(project.Clip("raw_import_copy")["events"]==changed,"Project semantic import lost raw metadata");
        Monstey_ECheck(Monstey_HashSha256(source)==source_hash,"Project changed baseline source");
    });
    if(argc>=3) test("two-actual-libraries-all-event-bytes-json-edits-and-prefixes",[&]{
        const auto evidence=std::filesystem::path(argv[2]);const auto facts=J::Parse(Monstey_ReadText(evidence/L"actual-source-event-bytes.json"));
        const auto &sources=facts["non_ascii_footer_events"]["identical_source_copies"];
        Monstey_ECheck(sources.size()==2,"Two actual sources required");
        for(std::size_t source_index=0;source_index<sources.size();++source_index){const auto path=std::filesystem::path(Monstey_Wide(sources[source_index]["path"].as_string()));const auto bytes=Monstey_ReadFile(path);const auto sha=Monstey_HashSha256(bytes);
            Monstey_ECheck(sha=="7586ea246e4589d82f80b3035815f5f88ef431429ff66670114f1e57db76ed22" && bytes.size()==575000,"Pinned actual source differs");
            Monstey_Anix library(bytes);Monstey_ECheck(library.Build()==bytes,"Actual no-op build changed bytes");const auto descriptors=Monstey_EDescriptors(bytes);const auto footer=Monstey_EFooterPool(bytes);
            std::set<std::string> strings;std::size_t entries=0,raw_entries=0,raw_clips=0,blocks=0;std::string selected;J selected_scene;
            for(const auto &descriptor:descriptors){const auto raw=Monstey_ERawEvents(bytes,descriptor);if(!raw.empty())++blocks;const auto info=J::Parse(library.EventInfo(descriptor.name).dump());Monstey_ECheck(info["event_edit_supported"].as_bool(),"Actual EventInfo rejected raw text");Monstey_ECompare(info["events"],raw);
                const auto scene=J::Parse(library.ExportClip(descriptor.name).dump());Monstey_ECompare(scene["events"],raw);bool high_clip=false;
                for(const auto &event:raw){++entries;strings.insert(event.bytes);if(std::any_of(event.bytes.begin(),event.bytes.end(),[](unsigned char value){return value>=128;})){++raw_entries;high_clip=true;}}
                if(high_clip){++raw_clips;if(selected.empty()){selected=descriptor.name;selected_scene=scene;}}
            }
            Monstey_ECheck(descriptors.size()==41 && entries==383 && strings.size()==63 && raw_entries==11 && raw_clips==11 && blocks==40,"Actual event coverage differs");
            (void)J::Parse(library.Export().dump());const auto events=library.EventInfo(selected)["events"];library.Edit(Monstey_EOperation(events,selected));
            const auto edited=library.Build();Monstey_ECompare(library.EventInfo(selected)["events"],Monstey_ERawEvents(bytes,selected));Monstey_ECheck(Monstey_EFooterPool(edited)==footer,"Actual event edit changed source footer strings");
            selected_scene["name"]="RAW_BYTE_IMPORT_COPY";Monstey_ECheck(selected_scene["semantic_reimport_supported"].as_bool(),"Selected actual scene cannot reimport");library.Edit(J::array({J::object({{"op","import"},{"clip",selected_scene}})}));
            Monstey_ECompare(library.EventInfo("RAW_BYTE_IMPORT_COPY")["events"],Monstey_ERawEvents(bytes,selected));
            library.Edit(J::array({J::object({{"op","rename"},{"clip",selected},{"name","RAW_BYTE_RENAME"}})}));const auto built=library.Build();Monstey_Anix reopened(built);
            Monstey_ECompare(reopened.EventInfo("RAW_BYTE_RENAME")["events"],Monstey_ERawEvents(bytes,selected));
            Monstey_ECheck(Monstey_EFooterPool(built)==footer,"Actual footer source bytes/unused strings changed");const auto old_meta=Monstey_U32(bytes,32);Monstey_ECheck(built.size()>=old_meta && std::equal(bytes.begin()+48,bytes.begin()+old_meta,built.begin()+48),"Original channels/payload prefix changed");
            for(const auto &descriptor:descriptors){const auto name=descriptor.name==selected?std::string("RAW_BYTE_RENAME"):descriptor.name;Monstey_ECompare(reopened.EventInfo(name)["events"],Monstey_ERawEvents(bytes,descriptor));}
            const auto authored=output/("heldattack-byte-candidate-"+std::to_string(source_index)+".anix");Monstey_WriteNewFile(authored,built);Monstey_ECheck(Monstey_HashSha256(path)==sha,"Actual source modified");
            actual.as_array().push_back(J::object({{"path",Monstey_Utf8(path.wstring())},{"source_sha256",sha},{"clips",41},{"stri_blocks",40},{"event_entries",383},{"unique_raw_strings",63},{"raw_entries",11},{"raw_clips",11},{"byte_exact_noop",true},{"footer_event_pool_preserved",true},{"original_payload_prefix_preserved",true},{"source_unchanged",true},{"authored_path",Monstey_Utf8(authored.wstring())},{"authored_sha256",Monstey_HashSha256(built)}}));
        }
    });
    const auto report=J::object({{"schema","Monstey.AnixEventByteTests/1"},{"synthetic_payloads",8},{"checks",checks},{"failures",static_cast<std::uint64_t>(failures)},{"actual_sources",actual},{"product_runtime_verified",false}});
    Monstey_WriteNewText(output/L"report.json",report.dump(2)+"\n");std::cout<<report.dump(2)<<"\n";return failures?1:0;
}