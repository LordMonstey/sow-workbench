#include "Monstey_ModelPreviewApi.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
namespace Monstey {
namespace {
using J=Monstey_Json;
void Monstey_PreviewApiRequire(bool valid,const std::string& message) {
    if(!valid)throw Monstey_Error("model_preview",message);
}
J Monstey_PreviewApiSchema() {
    const auto path=J::object({{"type","string"},{"format","path"},{"minLength",1}});
    const auto hash=J::object({{"type","string"},{"pattern","^[a-fA-F0-9]{64}$"}});
    const auto boolean=J::object({{"type","boolean"}});
    const auto dimension=J::object({{"type","integer"},{"minimum",32},{"maximum",4096}});
    auto properties=J::object({{"source",path},{"output",path},{"skeleton",path},
        {"expected_source_sha256",hash},{"expected_skeleton_sha256",hash},
        {"width",dimension},{"height",dimension},
        {"container",J::object({{"type","integer"},{"minimum",0}})}});
    for(const auto key:{"all_containers","mesh","bones","bones_in_front","wireframe","vertex_colors"})
        properties[key]=boolean;
    for(const auto key:{"yaw_degrees","pitch_degrees"})properties[key]=J::object({{"type","number"}});
    properties["zoom"]=J::object({{"type","number"},{"minimum",0.05},{"maximum",20}});
    for(const auto key:{"pan_x","pan_y"})properties[key]=J::object({{"type","number"},{"minimum",-8},{"maximum",8}});
    return J::object({{"type","object"},{"properties",properties},
        {"required",J::array({"source","output"})},{"additionalProperties",false},
        {"dependentRequired",J::object({{"expected_skeleton_sha256",J::array({"skeleton"})}})}});
}
std::filesystem::path Monstey_PreviewApiPath(const J& a,const char* key) {
    Monstey_PreviewApiRequire(a.contains(key)&&a.at(key).is_string()&&!a.at(key).as_string().empty(),
        std::string("Missing or invalid preview path ")+key);
    const auto& value=a.at(key).as_string();
    Monstey_PreviewApiRequire(value.find('\0')==std::string::npos,std::string("NUL in preview path ")+key);
    return std::filesystem::path(Monstey_Wide(value));
}
std::string Monstey_PreviewApiExpected(const J& a,const char* key) {
    if(!a.contains(key))return {};
    Monstey_PreviewApiRequire(a.at(key).is_string(),std::string("Invalid preview SHA256 type ")+key);
    auto value=a.at(key).as_string();
    Monstey_PreviewApiRequire(value.size()==64&&std::all_of(value.begin(),value.end(),[](unsigned char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}),
        std::string("Invalid preview SHA256 precondition ")+key);
    for(auto& c:value)if(c>='A'&&c<='F')c=static_cast<char>(c-'A'+'a');
    return value;
}
Monstey_ModelPreviewOptions Monstey_PreviewApiOptions(const J& a) {
    Monstey_ModelPreviewOptions o;
    const auto dimension=[&](const char* key,std::uint32_t fallback) {
        if(!a.contains(key))return fallback;
        Monstey_PreviewApiRequire(a.at(key).is_integer(),std::string("Expected integer preview option ")+key);
        const auto v=a.at(key).as_int();
        Monstey_PreviewApiRequire(v>=32&&v<=4096,std::string("Preview dimension outside 32..4096: ")+key);
        return static_cast<std::uint32_t>(v);
    };
    const auto boolean=[&](const char* key,bool fallback) {
        if(!a.contains(key))return fallback;
        Monstey_PreviewApiRequire(a.at(key).is_bool(),std::string("Expected Boolean preview option ")+key);
        return a.at(key).as_bool();
    };
    const auto number=[&](const char* key,double fallback) {
        if(!a.contains(key))return fallback;
        Monstey_PreviewApiRequire(a.at(key).is_number(),std::string("Expected numeric preview option ")+key);
        const auto v=a.at(key).as_number();
        Monstey_PreviewApiRequire(std::isfinite(v),std::string("Nonfinite preview option ")+key);
        return v;
    };
    o.width=dimension("width",o.width);o.height=dimension("height",o.height);
    Monstey_PreviewApiRequire(static_cast<std::uint64_t>(o.width)*o.height<=8388608,"Preview pixel budget exceeds 8388608");
    if(a.contains("container")) {
        Monstey_PreviewApiRequire(a.at("container").is_integer()&&a.at("container").as_int()>=0,"Invalid preview container");
        const auto v=a.at("container").as_uint();
        Monstey_PreviewApiRequire(v<=std::numeric_limits<std::size_t>::max(),"Preview container overflows native size");
        o.container=static_cast<std::size_t>(v);
    }
    o.all_containers=boolean("all_containers",o.all_containers);o.mesh=boolean("mesh",o.mesh);
    o.bones=boolean("bones",o.bones);o.bones_in_front=boolean("bones_in_front",o.bones_in_front);
    o.wireframe=boolean("wireframe",o.wireframe);o.vertex_colors=boolean("vertex_colors",o.vertex_colors);
    o.yaw_degrees=number("yaw_degrees",o.yaw_degrees);o.pitch_degrees=number("pitch_degrees",o.pitch_degrees);
    o.zoom=number("zoom",o.zoom);o.pan_x=number("pan_x",o.pan_x);o.pan_y=number("pan_y",o.pan_y);
    Monstey_PreviewApiRequire(o.zoom>=0.05&&o.zoom<=20,"Preview zoom outside 0.05..20");
    Monstey_PreviewApiRequire(std::abs(o.pan_x)<=8&&std::abs(o.pan_y)<=8,"Preview pan outside -8..8");
    return o;
}
J Monstey_PreviewApiOptionsJson(const Monstey_ModelPreviewOptions& o) {
    return J::object({{"width",o.width},{"height",o.height},{"container",static_cast<std::uint64_t>(o.container)},
        {"all_containers",o.all_containers},{"mesh",o.mesh},{"bones",o.bones},{"bones_in_front",o.bones_in_front},
        {"wireframe",o.wireframe},{"vertex_colors",o.vertex_colors},{"yaw_degrees",o.yaw_degrees},
        {"pitch_degrees",o.pitch_degrees},{"zoom",o.zoom},{"pan_x",o.pan_x},{"pan_y",o.pan_y}});
}
struct Monstey_PreviewApiSnapshot {
    std::filesystem::path path;
    std::string hash;
    std::uint64_t bytes=0;
    bool expected=false;
};
void Monstey_PreviewApiUnchanged(std::span<const Monstey_PreviewApiSnapshot> inputs) {
    for(const auto& input:inputs)
        if(Monstey_HashSha256(input.path)!=input.hash)
            throw Monstey_Error("source_changed","Preview input changed after snapshot: "+Monstey_Utf8(input.path.wstring()));
}
bool Monstey_PreviewApiRollback(const std::filesystem::path& path,const std::string& expected) noexcept {
    try {
        const auto native=Monstey_WindowsFilePath(path);
        if(!std::filesystem::exists(native))return true;
        if(Monstey_HashSha256(path)!=expected)return false;
        std::error_code error;std::filesystem::remove(native,error);
        return !error&&!std::filesystem::exists(native);
    } catch(...) {return false;}
}
} // namespace
J Monstey_ModelPreviewCatalog() {
    return J::array({J::object({{"name","model_preview_render"},{"read_only",false},
        {"description","Render a new offline BMP of host bind geometry and qualified skeleton links. Native materials and animation playback are not rendered. Optional SHA256 guards pin supplied sources."},
        {"inputSchema",Monstey_PreviewApiSchema()}})});
}
J Monstey_ModelPreviewRenderFile(const J& a) {
    Monstey_PreviewApiRequire(a.is_object(),"Preview arguments must be an object");
    const auto schema=Monstey_PreviewApiSchema();
    for(const auto& [key,value]:a.as_object()) {
        (void)value;Monstey_PreviewApiRequire(schema.at("properties").contains(key),"Unknown preview argument "+key);
    }
    const auto source=Monstey_PreviewApiPath(a,"source"),output=Monstey_PreviewApiPath(a,"output");
    const auto skeleton=a.contains("skeleton")?Monstey_PreviewApiPath(a,"skeleton"):std::filesystem::path{};
    Monstey_PreviewApiRequire(!skeleton.empty()||!a.contains("expected_skeleton_sha256"),"Skeleton SHA256 requires an explicit skeleton");
    const auto sourceExpected=Monstey_PreviewApiExpected(a,"expected_source_sha256");
    const auto skeletonExpected=Monstey_PreviewApiExpected(a,"expected_skeleton_sha256");
    const auto options=Monstey_PreviewApiOptions(a);
    std::vector<std::filesystem::path> protectedInputs{source};
    if(!skeleton.empty())protectedInputs.push_back(skeleton);
    Monstey_RequireOfflineOutput(output,protectedInputs);
    if(std::filesystem::exists(Monstey_WindowsFilePath(output)))
        throw Monstey_Error("output_exists","Preview output already exists");
    for(const auto& path:protectedInputs)
        Monstey_PreviewApiRequire(std::filesystem::is_regular_file(Monstey_WindowsFilePath(path)),"Preview input must be a regular file");
    auto meshBytes=Monstey_ReadFile(source);
    std::vector<Monstey_PreviewApiSnapshot> snapshots{{source,Monstey_HashSha256(meshBytes),static_cast<std::uint64_t>(meshBytes.size()),!sourceExpected.empty()}};
    if(!sourceExpected.empty()&&sourceExpected!=snapshots.front().hash)
        throw Monstey_Error("source_hash_mismatch","Preview source SHA256 differs");
    if(!skeleton.empty()) {
        const auto bytes=Monstey_ReadFile(skeleton);const auto hash=Monstey_HashSha256(bytes);
        if(!skeletonExpected.empty()&&skeletonExpected!=hash)
            throw Monstey_Error("source_hash_mismatch","Preview skeleton SHA256 differs");
        snapshots.push_back({skeleton,hash,static_cast<std::uint64_t>(bytes.size()),!skeletonExpected.empty()});
    }
    const auto scene=Monstey_ModelReadSceneBytes(meshBytes,skeleton);
    Monstey_Bytes{}.swap(meshBytes);
    Monstey_PreviewApiRequire(scene.source_sha256==snapshots.front().hash,"Preview decoder source identity differs");
    if(!skeleton.empty()&&scene.skeleton_sha256!=snapshots.back().hash)
        throw Monstey_Error("source_changed","Skeleton changed while decoding preview");
    const auto frame=Monstey_ModelPreviewRender(scene,options);
    const auto bitmap=Monstey_ModelPreviewBitmap(frame);
    const auto outputHash=Monstey_HashSha256(bitmap);
    auto inputs=J::object({{"source",J::object({{"path",Monstey_Utf8(source.wstring())},
        {"sha256",snapshots.front().hash},{"bytes",snapshots.front().bytes},{"expected_hash_guard",snapshots.front().expected}})},
        {"skeleton",nullptr}});
    if(!skeleton.empty())inputs["skeleton"]=J::object({{"path",Monstey_Utf8(skeleton.wstring())},
        {"sha256",snapshots.back().hash},{"bytes",snapshots.back().bytes},{"expected_hash_guard",snapshots.back().expected}});
    auto result=J::object({{"schema","Monstey.ModelPreviewRenderFile/1"},{"operation","model_preview_render"},
        {"inputs",inputs},{"viewport",Monstey_PreviewApiOptionsJson(options)},{"preview",frame.report},
        {"output",Monstey_Utf8(output.wstring())},{"output_format","BMP"},{"output_sha256",outputHash},
        {"output_bytes",static_cast<std::uint64_t>(bitmap.size())},{"source_unchanged",true},
        {"host_bind_geometry_only",true},{"native_materials",false},{"animation_playback",false}});
    bool published=false;
    try {
        Monstey_PreviewApiUnchanged(snapshots);
        Monstey_WriteNewFile(output,bitmap);published=true;
        Monstey_PreviewApiRequire(Monstey_ReadFile(output)==bitmap,"Preview bitmap failed exact readback");
        Monstey_PreviewApiUnchanged(snapshots);
    } catch(...) {
        const auto original=std::current_exception();
        if(published&&!Monstey_PreviewApiRollback(output,outputHash)) {
            try {std::rethrow_exception(original);}
            catch(const Monstey_Error& e){throw Monstey_Error(e.code,std::string(e.what())+"; preview rollback incomplete; externally changed output preserved");}
            catch(const std::exception& e){throw Monstey_Error("model_preview_rollback",std::string(e.what())+"; preview rollback incomplete; externally changed output preserved");}
            catch(...){throw Monstey_Error("model_preview_rollback","Preview publication failed; rollback incomplete; externally changed output preserved");}
        }
        std::rethrow_exception(original);
    }
    return result;
}
J Monstey_ModelPreviewCall(std::string_view operation,const J& arguments) {
    if(operation!="model_preview_render")throw Monstey_Error("unknown_operation","Unknown native preview operation");
    return Monstey_ModelPreviewRenderFile(arguments);
}
} // namespace Monstey
