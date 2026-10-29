#include "Monstey_Anix.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using J=Monstey_Json;
using B=Monstey_Bytes;
constexpr std::string_view Monstey_GEventClip="event_only";
constexpr std::array<unsigned,6> Monstey_GTicks{0,166,333,500,666,833};
void Monstey_GCheck(bool value,std::string_view message) {
    if(!value)throw std::runtime_error(std::string(message));
}
std::size_t Monstey_GAlign4(std::size_t value) {return (value+3)/4*4;}
std::string Monstey_GString(const B& bytes,std::size_t first,std::size_t end) {
    Monstey_GCheck(first<end && end<=bytes.size(),"Invalid bounded test string extent");
    const auto stop=std::find(bytes.begin()+first,bytes.begin()+end,0);
    Monstey_GCheck(stop!=bytes.begin()+end,"Missing test string terminator");
    return std::string(bytes.begin()+first,stop);
}
struct Monstey_GFooter {
    std::size_t metadata=0,footer=0,body=0;
    std::uint32_t event_at=0,stream_at=0;
};
Monstey_GFooter Monstey_GFooterAt(const B& bytes) {
    Monstey_GFooter layout;
    layout.metadata=Monstey_U32(bytes,32);
    layout.footer=layout.metadata+Monstey_U32(bytes,layout.metadata+12);
    layout.body=layout.footer+24;
    layout.event_at=Monstey_U32(bytes,layout.footer+16);
    layout.stream_at=Monstey_U32(bytes,layout.footer);
    Monstey_CheckRange(bytes,layout.body,layout.stream_at);
    return layout;
}
B Monstey_GNames(const B& bytes) {
    const auto layout=Monstey_GFooterAt(bytes);
    Monstey_CheckRange(bytes,layout.body,layout.event_at);
    return B(bytes.begin()+layout.body,bytes.begin()+layout.body+layout.event_at);
}
B Monstey_GGap(const B& bytes) {
    const auto first=static_cast<std::size_t>(Monstey_U32(bytes,24))+Monstey_U32(bytes,28);
    const auto last=static_cast<std::size_t>(Monstey_U32(bytes,32));
    Monstey_GCheck(first<=last && last<=bytes.size(),"Invalid test gap extent");
    return B(bytes.begin()+first,bytes.begin()+last);
}
std::map<std::string,std::uint32_t> Monstey_GNameOffsets(const B& bytes) {
    const auto layout=Monstey_GFooterAt(bytes);
    const auto names=Monstey_GNames(bytes);
    std::size_t cursor=layout.body+layout.stream_at+41;
    std::map<std::string,std::uint32_t> offsets;
    const auto count=Monstey_U32(bytes,layout.metadata+4);
    for(std::size_t i=0;i<count;++i) {
        const auto offset=Monstey_U32(bytes,cursor+12);
        offsets.emplace(Monstey_GString(names,offset,names.size()),offset);
        const auto markers=Monstey_U32(bytes,cursor+36);
        Monstey_CheckRange(bytes,cursor+40,8ull*markers);
        cursor+=40+8ull*markers;
    }
    return offsets;
}
void Monstey_GPreserved(const B& before,const B& after) {
    const auto first=static_cast<std::size_t>(Monstey_U32(before,24));
    const auto length=static_cast<std::size_t>(Monstey_U32(before,28));
    const auto next=static_cast<std::size_t>(Monstey_U32(after,24));
    Monstey_CheckRange(before,first,length);Monstey_CheckRange(after,next,length);
    Monstey_GCheck(std::equal(before.begin()+first,before.begin()+first+length,after.begin()+next),
                   "Original payload prefix changed");
    Monstey_GCheck(Monstey_GGap(before)==Monstey_GGap(after),"Original gap changed");
    const auto names_before=Monstey_GNames(before),names_after=Monstey_GNames(after);
    Monstey_GCheck(names_after.size()>=names_before.size() &&
        std::equal(names_before.begin(),names_before.end(),names_after.begin()),"Opaque names prefix changed");
    Monstey_GCheck(Monstey_GString(names_before,0,names_before.size())==
        Monstey_GString(names_after,0,names_after.size()),"Library path identity changed");
    const auto offsets_before=Monstey_GNameOffsets(before),offsets_after=Monstey_GNameOffsets(after);
    for(const auto& [clip_name,offset]:offsets_before) {
        const auto found=offsets_after.find(clip_name);
        if(found!=offsets_after.end())Monstey_GCheck(found->second==offset,"Existing footer name offset changed");
    }
}
J Monstey_GEvents() {
    J events=J::array();
    for(std::size_t i=0;i<Monstey_GTicks.size();++i)
        events.as_array().push_back(J::object({{"tick",Monstey_GTicks[i]},
            {"text","PORTABLE_EVENT_"+std::to_string(i)+";  "}}));
    return events;
}
J Monstey_GPoseClip(std::size_t rotation) {
    return J::object({{"name","bind"},{"duration_ticks",1000},{"ticks",J::array({0,1000})},
        {"tracks",J::array({
            J::object({{"channel_index",0},{"values",J::array({J::array({0,0,0}),J::array({0,0,0})})}}),
            J::object({{"channel_index",rotation},{"values",J::array({J::array({0,0,0,1}),J::array({0,0,0,1})})}})})},
        {"events",J::array()}});
}
J Monstey_GScene(unsigned event_channels=1,bool event_clip=true) {
    J channels=J::array({J::object({{"type_hash","00ce66fc"},{"bone_hash","88baa7e7"}})});
    if(event_channels)channels.as_array().push_back(J::object({{"type_hash","210c8f5a"},{"bone_hash","88baa7e7"}}));
    const auto rotation=channels.size();
    channels.as_array().push_back(J::object({{"type_hash","27436396"},{"bone_hash","88baa7e7"}}));
    if(event_channels==2)channels.as_array().push_back(J::object({{"type_hash","210c8f5a"},{"bone_hash","12345678"}}));
    J clips=J::array({Monstey_GPoseClip(rotation)});
    if(event_clip)clips.as_array().push_back(J::object({{"name",Monstey_GEventClip},
        {"duration_ticks",1000},{"ticks",J::array({0,1000})},{"tracks",J::array()},
        {"event_channel_index",1},{"events",Monstey_GEvents()}}));
    return J::object({{"library_path","synthetic/portable-gap.anix"},{"channels",channels},{"clips",clips}});
}
B Monstey_GFixture(std::size_t gap_size) {
    auto bytes=Monstey_Anix::Create(Monstey_GScene()).Build();
    const auto payload_end=static_cast<std::size_t>(Monstey_U32(bytes,24))+Monstey_U32(bytes,28);
    const auto old_metadata=Monstey_U32(bytes,32),old_payload_size=Monstey_U32(bytes,28);
    Monstey_GCheck(payload_end==old_metadata,"Create fixture unexpectedly has gap");
    B added{0xa7};
    for(std::size_t i=0;i<gap_size;++i)added.push_back(static_cast<std::uint8_t>(0x70+(i*13)%128));
    bytes.insert(bytes.begin()+payload_end,added.begin(),added.end());
    Monstey_PutU32(bytes,12,static_cast<std::uint32_t>(bytes.size()));
    Monstey_PutU32(bytes,28,old_payload_size+1);
    Monstey_PutU32(bytes,32,old_metadata+static_cast<std::uint32_t>(added.size()));
    const auto layout=Monstey_GFooterAt(bytes);
    const std::string unused="UNUSED_PORTABLE_NAME";
    B suffix(unused.begin(),unused.end());
    suffix.insert(suffix.end(),{0,0xde,0xad,0xbe,0xef,0,0x7f,0x01,0xff});
    const auto old_metadata_size=Monstey_U32(bytes,36);
    bytes.insert(bytes.begin()+layout.body+layout.event_at,suffix.begin(),suffix.end());
    const auto extra=static_cast<std::uint32_t>(suffix.size());
    Monstey_PutU32(bytes,12,static_cast<std::uint32_t>(bytes.size()));
    Monstey_PutU32(bytes,36,old_metadata_size+extra);
    Monstey_PutU32(bytes,layout.footer,layout.stream_at+extra);
    Monstey_PutU32(bytes,layout.footer+16,layout.event_at+extra);
    Monstey_PutU32(bytes,layout.body+layout.stream_at+extra,layout.stream_at+extra-5);
    Monstey_GCheck(Monstey_U32(bytes,28)%4==1,"Payload is not deliberately byte-sized");
    Monstey_GCheck(Monstey_GGap(bytes).size()==gap_size,"Synthetic gap size changed");
    return bytes;
}
void Monstey_GAssertEvents(const Monstey_Anix& library,std::string_view clip,
                          const J& expected,const std::vector<unsigned>& ticks,std::uint64_t binding=1) {
    const auto info=library.EventInfo(clip);
    Monstey_GCheck(info.at("event_edit_supported").as_bool() &&
        info.at("event_channel_index").as_uint()==binding,"Event binding/support changed");
    const auto& events=info.at("events");
    Monstey_GCheck(events.size()==expected.size() && events.size()==ticks.size(),"Event count changed");
    for(std::size_t i=0;i<events.size();++i)
        Monstey_GCheck(events[i].at("text")==expected[i].at("text") &&
            events[i].at("tick").as_uint()==ticks[i],"Event bytes/order/ticks changed");
}
B Monstey_GEdit(const B& source,const J& operations,const std::filesystem::path& output) {
    Monstey_Anix library(source);library.Edit(operations);
    const auto bytes=library.Build();
    Monstey_Anix reopened(bytes);(void)reopened.Describe();
    Monstey_GPreserved(source,bytes);
    Monstey_WriteNewFile(output,bytes);
    return bytes;
}
J Monstey_GOperation(std::string_view operation,std::string_view clip=Monstey_GEventClip) {
    return J::object({{"op",operation},{"clip",clip}});
}
void Monstey_GRejectImport(Monstey_Anix& library,const J& scene) {
    const auto before=library.Build();bool rejected=false;
    try {library.Edit(J::array({J::object({{"op","import"},{"clip",scene}})}));}
    catch(const Monstey_Error&) {rejected=true;}
    Monstey_GCheck(rejected && library.Build()==before,"Invalid semantic import committed changes");
}
void Monstey_GRejectBytes(const B& bytes) {
    bool rejected=false;
    try {Monstey_Anix library(bytes);(void)library.Describe();}
    catch(const Monstey_Error&) {rejected=true;}
    Monstey_GCheck(rejected,"Malformed profile was accepted");
}
}
int wmain(int argc,wchar_t** argv) {
    const std::filesystem::path base=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path();
    const auto output=base/("AnixGap_"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(output);
    J checks=J::array(),optional=J::array();std::size_t failures=0;
    const auto test=[&](std::string label,const std::function<void()>& action) {
        try {action();checks.as_array().push_back(J::object({{"name",label},{"passed",true}}));}
        catch(const std::exception& error) {++failures;checks.as_array().push_back(J::object({{"name",label},{"passed",false},{"message",error.what()}}));}
    };
    const auto events=Monstey_GEvents();
    const std::vector<unsigned> original_ticks(Monstey_GTicks.begin(),Monstey_GTicks.end());
    for(const std::size_t gap_size:std::array<std::size_t,5>{0,1,2,3,17}) {
        const auto source=Monstey_GFixture(gap_size);
        const auto tag="gap-"+std::to_string(gap_size);
        test(tag+"-read-noop",[&] {
            Monstey_Anix library(source);const auto description=library.Describe();
            Monstey_GCheck(description.at("clip_count").as_uint()==2 &&
                description.at("qualified_codec_blocks").as_uint()==3,"Fixture codec qualification changed");
            Monstey_GCheck(library.Build()==source,"Read-only no-op changed byte-sized source");
            Monstey_GAssertEvents(library,Monstey_GEventClip,events,original_ticks);
        });
        test(tag+"-rename-fidelity",[&] {
            auto operation=Monstey_GOperation("rename");operation["name"]="renamed";
            const auto bytes=Monstey_GEdit(source,J::array({operation}),output/(tag+"-rename.anix"));
            Monstey_GAssertEvents(Monstey_Anix(bytes),"renamed",events,original_ticks);
        });
        test(tag+"-duplicate-fidelity",[&] {
            auto operation=Monstey_GOperation("duplicate");operation["name"]="duplicate";
            const auto bytes=Monstey_GEdit(source,J::array({operation}),output/(tag+"-duplicate.anix"));
            Monstey_GAssertEvents(Monstey_Anix(bytes),Monstey_GEventClip,events,original_ticks);
            Monstey_GAssertEvents(Monstey_Anix(bytes),"duplicate",events,original_ticks);
        });
        test(tag+"-held-fidelity",[&] {
            const auto operation=J::object({{"op","held"},{"source","bind"},{"name","held"},{"duration_ticks",1000}});
            const auto bytes=Monstey_GEdit(source,J::array({operation}),output/(tag+"-held.anix"));
            Monstey_GAssertEvents(Monstey_Anix(bytes),Monstey_GEventClip,events,original_ticks);
        });
        test(tag+"-events-fidelity",[&] {
            auto replacement=events;replacement[0]["text"]="EDITED_EVENT;  ";
            auto operation=Monstey_GOperation("events");operation["events"]=replacement;
            const auto bytes=Monstey_GEdit(source,J::array({operation}),output/(tag+"-events.anix"));
            Monstey_GAssertEvents(Monstey_Anix(bytes),Monstey_GEventClip,replacement,original_ticks);
        });
    }
    const auto source=Monstey_GFixture(2);
    const auto exported=Monstey_Anix(source).ExportClip(Monstey_GEventClip);
    const J timing=J::array({
        J::object({{"op","rate"},{"clip",Monstey_GEventClip},{"factor",2.0}}),
        J::object({{"op","length"},{"clip",Monstey_GEventClip},{"duration_ticks",1500}}),
        J::object({{"op","delay"},{"clip",Monstey_GEventClip},{"at_tick",500},{"duration_ticks",100}}),
        J::object({{"op","trim"},{"clip",Monstey_GEventClip},{"start_tick",0},{"end_tick",900}})});
    const std::array<std::vector<unsigned>,4> expected{{
        {0,83,166,250,333,416},{0,249,500,750,999,1250},{0,166,333,600,766,933},{0,166,333,500,666,833}}};
    for(std::size_t i=0;i<timing.size();++i) {
        const auto label="event-only-"+timing[i].at("op").as_string();
        test(label,[&] {
            const auto bytes=Monstey_GEdit(source,J::array({timing[i]}),output/(label+".anix"));
            Monstey_GAssertEvents(Monstey_Anix(bytes),Monstey_GEventClip,events,expected[i]);
        });
    }
    test("event-only-trim-discards-only-outside-events",[&] {
        auto operation=Monstey_GOperation("trim");operation["start_tick"]=100;operation["end_tick"]=900;
        const auto bytes=Monstey_GEdit(source,J::array({operation}),output/"trim-boundaries.anix");
        auto retained=events;retained.as_array().erase(retained.as_array().begin());
        Monstey_GAssertEvents(Monstey_Anix(bytes),Monstey_GEventClip,retained,{66,233,400,566,733});
    });
    test("event-only-semantic-copy",[&] {
        auto scene=exported;scene["name"]="semantic_copy";
        Monstey_GCheck(scene.at("tracks").empty() && scene.at("semantic_reimport_supported").as_bool(),
                       "Event-only scene is not qualified for import");
        const auto bytes=Monstey_GEdit(source,J::array({J::object({{"op","import"},{"clip",scene}})}),output/"semantic-copy.anix");
        Monstey_GAssertEvents(Monstey_Anix(bytes),"semantic_copy",events,original_ticks);
    });
    test("event-only-explicit-empty-binding",[&] {
        auto scene=exported;scene["name"]="empty_binding";scene["events"]=J::array();
        const auto bytes=Monstey_GEdit(source,J::array({J::object({{"op","import"},{"clip",scene}})}),output/"empty-binding.anix");
        Monstey_GAssertEvents(Monstey_Anix(bytes),"empty_binding",J::array(),{});
    });
    test("event-only-inferred-unique-binding",[&] {
        auto scene=exported;scene["name"]="inferred_binding";scene.as_object().erase("event_channel_index");
        const auto bytes=Monstey_GEdit(source,J::array({J::object({{"op","import"},{"clip",scene}})}),output/"inferred-binding.anix");
        Monstey_GAssertEvents(Monstey_Anix(bytes),"inferred_binding",events,original_ticks);
    });
    std::vector<std::pair<std::string,J>> invalid;
    auto scene=exported;scene["name"]="invalid_import";scene["events"]=J::array();scene.as_object().erase("event_channel_index");
    invalid.emplace_back("no-events-no-binding",scene);
    scene=exported;scene["name"]="invalid_import";scene["event_channel_index"]=0;invalid.emplace_back("non-Event-binding",scene);
    scene["event_channel_index"]=99;invalid.emplace_back("out-of-bounds-binding",scene);
    scene=exported;scene["name"]="invalid_import";scene["events"]=J::array({J::object({{"tick",0},{"text",std::string("a\0b",3)}})});
    invalid.emplace_back("NUL-event-bytes",scene);
    scene=exported;scene["name"]="invalid_import";scene["empty_native_clip"]=true;invalid.emplace_back("specialempty-structural-only",scene);
    for(const auto& entry:invalid)test("reject-import-"+entry.first,[&] {
        Monstey_Anix library(source);Monstey_GRejectImport(library,entry.second);
    });
    test("reject-absent-and-ambiguous-Event-then-explicit-selection",[&] {
        auto candidate=exported;candidate["name"]="test_binding";candidate.as_object().erase("event_channel_index");
        auto absent=Monstey_Anix::Create(Monstey_GScene(0,false));Monstey_GRejectImport(absent,candidate);
        auto ambiguous=Monstey_Anix::Create(Monstey_GScene(2,false));Monstey_GRejectImport(ambiguous,candidate);
        candidate["event_channel_index"]=1;
        ambiguous.Edit(J::array({J::object({{"op","import"},{"clip",candidate}})}));
        Monstey_GAssertEvents(ambiguous,"test_binding",events,original_ticks,1);
    });
    const auto malformed=[&](std::string label,const std::function<void(B&)>& mutate) {
        test("reject-profile-"+label,[&] {auto bytes=source;mutate(bytes);Monstey_GRejectBytes(bytes);});
    };
    malformed("overlap",[](B& bytes) {Monstey_PutU32(bytes,32,Monstey_U32(bytes,24)+Monstey_U32(bytes,28)-1);});
    malformed("payload-size-overflow",[](B& bytes) {Monstey_PutU32(bytes,28,UINT32_MAX);});
    malformed("metadata-size-overflow",[](B& bytes) {Monstey_PutU32(bytes,36,UINT32_MAX);});
    malformed("metadata-offset-overflow",[](B& bytes) {Monstey_PutU32(bytes,32,UINT32_MAX);});
    malformed("declared-file-size",[](B& bytes) {Monstey_PutU32(bytes,12,static_cast<std::uint32_t>(bytes.size()-1));});
    malformed("truncated-metadata",[](B& bytes) {bytes.pop_back();Monstey_PutU32(bytes,12,static_cast<std::uint32_t>(bytes.size()));});
    malformed("descriptor-relative-plus-one",[](B& bytes) {
        const auto metadata=static_cast<std::size_t>(Monstey_U32(bytes,32));
        const auto field=metadata+Monstey_U32(bytes,metadata)+4;
        Monstey_PutU32(bytes,field,Monstey_U32(bytes,field)+1);
    });
    malformed("segment-relative-plus-one",[](B& bytes) {
        const auto metadata=static_cast<std::size_t>(Monstey_U32(bytes,32));
        const auto descriptor=metadata+Monstey_U32(bytes,metadata+Monstey_U32(bytes,metadata)+4);
        const auto codecs=Monstey_U16(bytes,descriptor+56),tracks=Monstey_U16(bytes,descriptor+58);
        const auto ids=Monstey_GAlign4(124+4ull*codecs+tracks);
        const auto field=descriptor+Monstey_GAlign4(ids+2ull*tracks);
        Monstey_PutU32(bytes,field,Monstey_U32(bytes,field)+1);
    });
    if(argc>2)test("optional-real-source-qualification",[&] {
        const std::filesystem::path path(argv[2]);const auto real=Monstey_ReadFile(path);const auto digest=Monstey_HashSha256(real);
        Monstey_Anix library(real);Monstey_GCheck(library.Build()==real,"Optional source no-op differs");
        const auto description=library.Describe();std::string selected;
        for(const auto& clip:description.at("clips").as_array())
            if(clip.at("codecs").size()==1 && clip.at("codecs")[0].as_string()=="STRI") {selected=clip.at("name").as_string();break;}
        Monstey_GCheck(!selected.empty(),"Optional source has no event-only STRI clip");
        const auto real_scene=library.ExportClip(selected);const auto real_events=real_scene.at("events");
        std::vector<unsigned> ticks;
        for(const auto& event:real_events.as_array())ticks.push_back(static_cast<unsigned>(event.at("tick").as_uint()));
        const auto binding=library.EventInfo(selected).at("event_channel_index").as_uint();
        auto operation=Monstey_GOperation("rename",selected);operation["name"]="portable_real_copy";
        const auto bytes=Monstey_GEdit(real,J::array({operation}),output/"optional-real-rename.anix");
        Monstey_GAssertEvents(Monstey_Anix(bytes),"portable_real_copy",real_events,ticks,binding);
        Monstey_GCheck(Monstey_HashSha256(path)==digest,"Optional source file changed");
        optional.as_array().push_back(J::object({{"path",Monstey_Utf8(path.wstring())},{"source_sha256",digest},{"source_unchanged",true}}));
    });
    const auto report=J::object({{"schema","Monstey.AnixGapPortableTests/1"},{"status",failures?"FAIL":"PASS"},
        {"tests",checks},{"test_count",checks.size()},{"failures",failures},{"default_synthetic_only",argc<=2},
        {"gap_sizes",J::array({0,1,2,3,17})},{"optional_actual_sources",optional},
        {"output_directory",Monstey_Utf8(output.wstring())},{"original_executable_executed",false},{"runtime_playback_verified",false}});
    Monstey_WriteNewText(output/"anix-gap-tests.json",report.dump(2)+"\n");
    std::cout<<report.dump(2)<<'\n';
    return failures?1:0;
}
