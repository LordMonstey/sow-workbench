#include "Monstey_Model.hpp"
#include "Monstey_Blend.hpp"
#include "Monstey_BlendGeometry.hpp"
#include "Monstey_BlendModel.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using B=Monstey_Bytes;
using J=Monstey_Json;
struct Monstey_Profile { unsigned uv=1, color=1; };
struct Monstey_Disk {
    std::vector<B> layouts;
    std::vector<std::array<std::uint32_t,10>> rows;
    B vertices,indices,palette;
};
void Monstey_Check(bool value,const char* message) {
    if(!value)throw Monstey_Error("test_failed",message);
}
void Monstey_F32(B& bytes,float value) {
    Monstey_AppendU32(bytes,std::bit_cast<std::uint32_t>(value));
}
std::uint32_t Monstey_Size(std::size_t value) {
    Monstey_Check(value<=UINT32_MAX,"Synthetic size overflow");
    return static_cast<std::uint32_t>(value);
}
B Monstey_Slice(const B& bytes,std::size_t at,std::size_t size) {
    Monstey_CheckRange(bytes,at,size);
    return B(bytes.begin()+static_cast<std::ptrdiff_t>(at),bytes.begin()+static_cast<std::ptrdiff_t>(at+size));
}
unsigned Monstey_Width(std::uint8_t type) {
    switch(type) {
    case 1: case 12:return 8;
    case 2:return 12;
    case 3:return 16;
    case 4:return 4;
    default:throw Monstey_Error("test_failed","Unexpected independent fixture storage");
    }
}
B Monstey_Declaration(Monstey_Profile profile) {
    Monstey_Check(profile.uv>=1&&profile.uv<=8&&profile.color>=1&&profile.color<=8,"Fixture profile range");
    B bytes{0,12,0,0,0,4,6,0,0,4,7,0};
    for(unsigned k=0;k<profile.uv;++k)
        for(const auto v:{1u,1u,4u,k})bytes.push_back(static_cast<std::uint8_t>(v));
    for(unsigned semantic=1;semantic<=3;++semantic)
        for(const auto v:{1u,2u,semantic,0u})bytes.push_back(static_cast<std::uint8_t>(v));
    for(unsigned k=0;k<profile.color;++k)
        for(const auto v:{1u,4u,5u,k})bytes.push_back(static_cast<std::uint8_t>(v));
    return bytes;
}
// Independent MMSH17 producer: one native connection/container, one HK bone,
// three vertices per draw, literal two-stream declarations and uncompressed spans.
B Monstey_Fixture(const std::vector<Monstey_Profile>& profiles) {
    Monstey_Check(!profiles.empty(),"Empty fixture");
    std::vector<B> layouts;
    std::vector<std::array<std::uint32_t,10>> rows;
    B vb,ib,pb;
    const std::uint16_t positions[3][4]={{0,0,0,0x3c00},{0x5640,0,0,0x3c00},{0,0x5640,0,0x3c00}};
    for(std::size_t part=0;part<profiles.size();++part) {
        const auto profile=profiles[part];
        layouts.push_back(Monstey_Declaration(profile));
        rows.push_back({Monstey_Size(vb.size()),Monstey_Size(part*3),3,Monstey_Size(ib.size()/2),3,
                        Monstey_Size(pb.size()),1,Monstey_Size(0x1200+part),0xabcdef01u,Monstey_Size(part)});
        for(const auto& position:positions) {
            for(const auto word:position)Monstey_AppendU16(vb,word);
            for(const auto word:{0u,0u,255u,0u,0u,0u,0u,0u})vb.push_back(static_cast<std::uint8_t>(word));
        }
        for(unsigned vertex=0;vertex<3;++vertex) {
            for(unsigned channel=0;channel<profile.uv;++channel) {
                Monstey_F32(vb,vertex==1?1.0f:0.0f);
                Monstey_F32(vb,vertex==2?1.0f:0.0f);
            }
            for(const auto word:{0.0f,0.0f,1.0f,1.0f,0.0f,0.0f,0.0f,1.0f,0.0f})Monstey_F32(vb,word);
            for(unsigned channel=0;channel<profile.color;++channel)
                for(const auto word:{11u+channel,33u+vertex,77u,255u})vb.push_back(static_cast<std::uint8_t>(word));
        }
        for(const auto index:{0u,1u,2u})Monstey_AppendU16(ib,static_cast<std::uint16_t>(index));
        pb.push_back(0);
    }
    B desc;
    for(const auto word:{0xebaec3fau,1u,1u,0u,0u,Monstey_Size(vb.size()),Monstey_Size(ib.size()),Monstey_Size(pb.size())})
        Monstey_AppendU32(desc,word);
    std::array<std::uint32_t,18> header{};
    header[0]=0x1a1541bc;header[2]=1;header[3]=1;header[4]=Monstey_Size(rows.size());
    header[5]=Monstey_Size(rows.size());header[9]=Monstey_Size(layouts.size());
    for(const auto& layout:layouts)header[10]+=Monstey_Size(layout.size()/4);
    header[16]=1;header[17]=40;
    B info;
    for(const auto word:header)Monstey_AppendU32(info,word);
    info.insert(info.end(),16,0); // one native connection record
    for(const auto word:{Monstey_Size(vb.size()),Monstey_Size(ib.size()),Monstey_Size(pb.size()),Monstey_Size(rows.size())})
        Monstey_AppendU32(info,word);
    for(unsigned k=0;k<16;++k)Monstey_AppendU16(info,1);
    for(const auto& row:rows)for(const auto word:row)Monstey_AppendU32(info,word);
    Monstey_AppendU32(info,0x8e3e068e);Monstey_AppendU32(info,0x37d749a6);
    for(const auto& layout:layouts) {
        Monstey_AppendU32(info,Monstey_Size(layout.size()/4));
        info.insert(info.end(),layout.begin(),layout.end());
    }
    Monstey_AppendU32(info,0x93d9a424);Monstey_AppendU32(info,0x1000);
    Monstey_AppendU16(info,0xffff);Monstey_AppendU16(info,0);
    for(const auto value:{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f,1.0f})Monstey_F32(info,value);
    B data;Monstey_AppendU32(data,0x95dbdb69);
    data.insert(data.end(),vb.begin(),vb.end());data.insert(data.end(),ib.begin(),ib.end());data.insert(data.end(),pb.begin(),pb.end());
    B bytes;
    for(const auto word:{0x48534d4du,17u,Monstey_Size(desc.size()),Monstey_Size(info.size()),Monstey_Size(data.size())})
        Monstey_AppendU32(bytes,word);
    bytes.insert(bytes.end(),desc.begin(),desc.end());bytes.insert(bytes.end(),info.begin(),info.end());bytes.insert(bytes.end(),data.begin(),data.end());
    return bytes;
}
Monstey_Disk Monstey_ReadDisk(const B& bytes) {
    Monstey_Check(Monstey_U32(bytes,0)==0x48534d4d&&Monstey_U32(bytes,4)==17,"Independent MMSH envelope");
    const std::size_t desc=20,info=desc+Monstey_U32(bytes,8),data=info+Monstey_U32(bytes,12);
    Monstey_Check(data+Monstey_U32(bytes,16)==bytes.size(),"Independent MMSH extent");
    Monstey_Check(Monstey_U32(bytes,info+12)==1,"Independent fixture expects one container");
    const auto connections=Monstey_U32(bytes,info+8),parts=Monstey_U32(bytes,info+16);
    std::size_t at=info+72+Monstey_U32(bytes,info+4)*4ull+connections*16ull+48;
    Monstey_Disk disk;
    for(std::uint32_t part=0;part<parts;++part) {
        std::array<std::uint32_t,10> row{};
        for(unsigned k=0;k<10;++k)row[k]=Monstey_U32(bytes,at+k*4);
        at+=40;disk.rows.push_back(row);
    }
    Monstey_Check(Monstey_U32(bytes,at)==0x8e3e068e,"Independent LOD marker");
    at+=4+Monstey_U32(bytes,info+24)*4ull+Monstey_U32(bytes,info+28)*8ull+Monstey_U32(bytes,info+32)*8ull;
    Monstey_Check(Monstey_U32(bytes,at)==0x37d749a6,"Independent declaration marker");at+=4;
    const auto layout_count=Monstey_U32(bytes,info+36);
    for(std::uint32_t k=0;k<layout_count;++k) {
        const auto count=Monstey_U32(bytes,at);at+=4;
        Monstey_Check(count>0&&count<=64,"Independent declaration count");
        disk.layouts.push_back(Monstey_Slice(bytes,at,static_cast<std::size_t>(count)*4));at+=static_cast<std::size_t>(count)*4;
    }
    Monstey_Check(Monstey_U32(bytes,at)==0x93d9a424,"Independent bone marker");
    const auto sizes=desc+12+connections*8ull;
    Monstey_Check(Monstey_U32(bytes,data)==0x95dbdb69,"Independent data marker");
    std::size_t offset=data+4;
    disk.vertices=Monstey_Slice(bytes,offset,Monstey_U32(bytes,sizes));offset+=disk.vertices.size();
    disk.indices=Monstey_Slice(bytes,offset,Monstey_U32(bytes,sizes+4));offset+=disk.indices.size();
    disk.palette=Monstey_Slice(bytes,offset,Monstey_U32(bytes,sizes+8));
    return disk;
}
B Monstey_DrawVertices(const Monstey_Disk& disk,std::size_t part) {
    const auto& row=disk.rows.at(part);const auto& layout=disk.layouts.at(row[9]);
    std::size_t stride=0;for(std::size_t at=0;at<layout.size();at+=4)stride+=Monstey_Width(layout[at+1]);
    return Monstey_Slice(disk.vertices,row[0],stride*row[2]);
}
J Monstey_AssertLayout(const B& bytes,std::size_t part,unsigned expected,unsigned position_type=12) {
    const auto disk=Monstey_ReadDisk(bytes);const auto& layout=disk.layouts.at(disk.rows.at(part)[9]);
    Monstey_Check(layout.size()==static_cast<std::size_t>(expected)*4,"Independent declaration count differs");
    Monstey_Check(layout[1]==position_type,"Independent position storage differs");
    auto raw=J::array();
    for(std::size_t at=0;at<layout.size();at+=4) {
        Monstey_Check(layout[at+2]!=8&&layout[at+2]!=9,"Writer unexpectedly emits explicit instance semantics");
        raw.as_array().push_back(J::array({layout[at],layout[at+1],layout[at+2],layout[at+3]}));
    }
    return J::object({{"count",expected},{"literal_attributes",raw},{"sha256",Monstey_HashSha256(bytes)}});
}
void Monstey_SetProfile(Monstey_ModelPart& part,unsigned uv,unsigned color) {
    const auto count=part.channels.at("POSITION").values.size()/3;
    for(unsigned k=0;k<uv;++k)
        if(!part.channels.contains("UV"+std::to_string(k)))
            part.channels["UV"+std::to_string(k)]={2,std::vector<double>(count*2,.25)};
    for(unsigned k=0;k<color;++k)
        if(!part.channels.contains("COLOR"+std::to_string(k)))
            part.channels["COLOR"+std::to_string(k)]={4,std::vector<double>(count*4,127)};
}
J Monstey_Options(bool preflight=false) { return J::object({{"preflight",preflight},{"triangle_band",0u}}); }
J Monstey_FloatOptions(bool preflight=false) {
    auto options=Monstey_Options(preflight);options["position_format"]="float";return options;
}
void Monstey_NoOutput(const std::filesystem::path& output) {
    Monstey_Check(!std::filesystem::exists(output)&&!std::filesystem::exists(std::filesystem::path(output.wstring()+L".report.json")),
                  "Rejected authoring published output or report");
}
J Monstey_LimitCase(bool baseline,const std::filesystem::path& output,const std::function<J()>& fn,bool preflight=false) {
    if(baseline) {
        const auto result=fn();
        if(preflight) { Monstey_NoOutput(output);return J::object({{"legacy_accepted",true},{"preflight",true},{"result",result}}); }
        return J::object({{"legacy_accepted",true},{"disk",Monstey_AssertLayout(Monstey_ReadFile(output),0,14,
                    static_cast<unsigned>(result.value("test_position_type",12u).as_uint()))}});
    }
    try { (void)fn(); }
    catch(const Monstey_Error& error) {
        Monstey_Check(error.code=="model_invalid"&&std::string(error.what()).find(
            "Authored vertex layout exceeds the native 13-attribute profile")!=std::string::npos,"Rejected by an unrelated error");
        Monstey_NoOutput(output);
        return J::object({{"rejected",true},{"error_code",error.code},{"message",error.what()},{"output_absent",true},{"report_absent",true}});
    }
    throw Monstey_Error("test_failed","14-attribute authoring was accepted");
}
struct Monstey_Glb {J doc;B bin;};
B Monstey_SaveGlb(Monstey_Glb glb) {
    glb.doc["buffers"]=J::array({J::object({{"byteLength",static_cast<std::uint64_t>(glb.bin.size())}})});
    auto text=glb.doc.dump();while(text.size()%4)text.push_back(' ');while(glb.bin.size()%4)glb.bin.push_back(0);
    B bytes;for(const auto word:{0x46546c67u,2u,Monstey_Size(28+text.size()+glb.bin.size()),Monstey_Size(text.size()),0x4e4f534au})
        Monstey_AppendU32(bytes,word);
    bytes.insert(bytes.end(),text.begin(),text.end());Monstey_AppendU32(bytes,Monstey_Size(glb.bin.size()));Monstey_AppendU32(bytes,0x004e4942);
    bytes.insert(bytes.end(),glb.bin.begin(),glb.bin.end());return bytes;
}
Monstey_Glb Monstey_ReadGlb(const std::filesystem::path& path) {
    const auto bytes=Monstey_ReadFile(path);const auto n=Monstey_U32(bytes,12);const auto at=20ull+n;
    return {J::Parse(std::string(reinterpret_cast<const char*>(bytes.data()+20),n)),
            Monstey_Slice(bytes,static_cast<std::size_t>(at+8),Monstey_U32(bytes,static_cast<std::size_t>(at)))};
}
B Monstey_CustomGlb(unsigned uv,unsigned color) {
    Monstey_Glb glb{J::object({{"asset",J::object({{"version","2.0"}})},{"accessors",J::array()},{"bufferViews",J::array()}}),{}};
    const auto add=[&](const std::vector<float>& values,unsigned width) {
        const auto start=glb.bin.size();for(auto value:values)Monstey_F32(glb.bin,value);
        const auto view=glb.doc["bufferViews"].size();
        glb.doc["bufferViews"].as_array().push_back(J::object({{"buffer",0u},{"byteOffset",static_cast<std::uint64_t>(start)},{"byteLength",static_cast<std::uint64_t>(values.size()*4)}}));
        const auto index=glb.doc["accessors"].size();
        glb.doc["accessors"].as_array().push_back(J::object({{"bufferView",static_cast<std::uint64_t>(view)},{"componentType",5126u},{"count",3u},{"type",width==2?"VEC2":width==3?"VEC3":"VEC4"}}));
        return static_cast<std::uint64_t>(index);
    };
    auto attrs=J::object({{"POSITION",add({0,0,0,1,0,0,0,1,0},3)},{"NORMAL",add({0,0,1,0,0,1,0,0,1},3)}});
    for(unsigned k=0;k<uv;++k)attrs["TEXCOORD_"+std::to_string(k)]=add({0,0,1,0,0,1},2);
    for(unsigned k=0;k<color;++k)attrs["COLOR_"+std::to_string(k)]=add({1,.5f,.25f,1,1,.5f,.25f,1,1,.5f,.25f,1},4);
    glb.doc["meshes"]=J::array({J::object({{"primitives",J::array({J::object({{"attributes",attrs},{"mode",4u}})})}})});
    glb.doc["nodes"]=J::array({J::object({{"mesh",0u}})});
    glb.doc["scenes"]=J::array({J::object({{"nodes",J::array({0u})}})});glb.doc["scene"]=0u;
    return Monstey_SaveGlb(std::move(glb));
}
void Monstey_EditGlb(Monstey_Glb& glb,const std::string& name) {
    const auto ai=static_cast<std::size_t>(glb.doc["meshes"][0u]["primitives"][0u]["attributes"][name].as_uint());
    const auto& accessor=glb.doc["accessors"][ai];const auto vi=static_cast<std::size_t>(accessor["bufferView"].as_uint());
    const auto offset=static_cast<std::size_t>(glb.doc["bufferViews"][vi].value("byteOffset",0u).as_uint()+accessor.value("byteOffset",0u).as_uint());
    const auto type=accessor["componentType"].as_uint();
    if(type==5126)Monstey_PutU32(glb.bin,offset,std::bit_cast<std::uint32_t>(.125f));
    else {Monstey_Check(type==5121,"Unexpected GLB edit storage");glb.bin.at(offset)=42;}
}
std::size_t Monstey_BlendMesh(Monstey_Blend& blend) {
    const auto rows=blend.Blocks("ME");
    for(const auto& row:rows["rows"].as_array()) {
        const auto index=static_cast<std::size_t>(row["index"].as_uint());
        if(blend.ReadField(index,"id.name").as_string()=="MESoW_C0_P0")return index;
    }
    throw Monstey_Error("test_failed","Imported Blender mesh absent");
}
void Monstey_EditBlend(Monstey_Blend& blend,std::size_t mesh,const std::string& wanted) {
    const auto pointer=blend.ReadField(mesh,"attribute_storage.dna_attributes")["pointer"].as_uint();
    const auto block=blend.BlockAtAddress(pointer,mesh);
    const auto count=blend.ReadField(mesh,"attribute_storage.dna_attributes_num").as_uint();
    for(std::size_t k=0;k<count;++k) {
        const auto np=blend.ReadField(block,"name",k)["pointer"].as_uint();
        const auto name_bytes=blend.BlockData(blend.BlockAtAddress(np,block));
        const std::string name(name_bytes.begin(),std::find(name_bytes.begin(),name_bytes.end(),0));
        if(name!=wanted)continue;
        const auto array=blend.BlockAtAddress(blend.ReadField(block,"data",k)["pointer"].as_uint(),block);
        const auto data=blend.BlockAtAddress(blend.ReadField(array,"data")["pointer"].as_uint(),array);
        auto bytes=blend.BlockData(data);
        if(wanted=="UVMap")Monstey_PutU32(bytes,0,std::bit_cast<std::uint32_t>(.125f));
        else bytes.at(0)=42;
        blend.ReplaceData(data,std::move(bytes));return;
    }
    throw Monstey_Error("test_failed","Imported Blender UV/color attribute absent");
}
class Monstey_SourcePins {
    std::map<std::filesystem::path,std::string> pins_;
    unsigned checks_=0;
public:
    void Pin(const std::filesystem::path& input) {
        const auto path=std::filesystem::absolute(input).lexically_normal();
        const auto sha=Monstey_HashSha256(path);
        const auto [it,inserted]=pins_.emplace(path,sha);
        Monstey_Check(inserted||it->second==sha,"Previously pinned source changed before use");
    }
    void Verify() {
        for(const auto& [path,sha]:pins_)
            if(Monstey_HashSha256(path)!=sha)
                throw Monstey_Error("test_failed","Source snapshot changed: "+Monstey_Utf8(path.wstring()));
        ++checks_;
    }
    J Report() const {
        auto rows=J::array();
        for(const auto& [path,sha]:pins_)
            rows.as_array().push_back(J::object({{"path",Monstey_Utf8(path.wstring())},{"sha256",sha}}));
        return rows;
    }
    unsigned Checks() const {return checks_;}
};
J Monstey_Run(const std::filesystem::path& directory,bool baseline,const std::filesystem::path& blend_template) {
    std::filesystem::create_directories(directory);
    auto tests=J::array();unsigned failed=0;
    Monstey_SourcePins sources;sources.Pin(blend_template);
    const auto test=[&](const char* name,const std::function<J()>& fn) {
        J row;std::string failure;
        try {row=fn();}
        catch(const std::exception& error) {failure=error.what();}
        try {sources.Verify();}
        catch(const std::exception& error) {
            if(!failure.empty())failure+="; ";
            failure+=error.what();
        }
        if(failure.empty()) {row["name"]=name;row["passed"]=true;tests.as_array().push_back(std::move(row));}
        else {++failed;tests.as_array().push_back(J::object({{"name",name},{"passed",false},{"error",failure}}));}
    };
    const auto bytes8=Monstey_Fixture({{1,1}}),bytes13=Monstey_Fixture({{6,1}}),bytes14=Monstey_Fixture({{7,1}});
    const auto source8=directory/L"source-8.mesh",source13=directory/L"source-13.mesh",source14=directory/L"source-14.mesh";
    Monstey_WriteNewFile(source8,bytes8);sources.Pin(source8);
    Monstey_WriteNewFile(source13,bytes13);sources.Pin(source13);
    Monstey_WriteNewFile(source14,bytes14);sources.Pin(source14);
    const auto bytes22=Monstey_Fixture({{8,8}});
    const auto source22=directory/L"source-22.mesh";Monstey_WriteNewFile(source22,bytes22);sources.Pin(source22);
    test("independent_raw_fixture_declarations_and_public_inspection",[&] {
        const auto inspect=Monstey_ModelInspectBytes(bytes14);
        Monstey_Check(inspect["allocation"]["safe"].as_bool(),"Raw14 allocation unsafe");
        Monstey_Check(Monstey_ModelReadSceneBytes(bytes14).parts.front().channels.size()==14,"Raw14 semantic channel count");
        return J::object({{"raw8",Monstey_AssertLayout(bytes8,0,8)},{"raw13",Monstey_AssertLayout(bytes13,0,13)},{"raw14",Monstey_AssertLayout(bytes14,0,14)}});
    });
    test("raw22_generic_profile_inspection_and_noops_remain_byte_exact",[&] {
        const auto inspection=Monstey_ModelInspectBytes(bytes22);
        const auto scene=Monstey_ModelReadScene(source22);
        Monstey_Check(inspection["allocation"]["safe"].as_bool()&&scene.parts[0].channels.size()==22,"Raw22 generic profile lost readable channels");
        const auto roundtrip=directory/L"raw22-roundtrip.mesh",noop=directory/L"raw22-scene-noop.mesh";
        (void)Monstey_ModelRoundtrip(source22,roundtrip);
        const auto report=Monstey_ModelWriteScene(scene,source22,noop);
        Monstey_Check(report["byte_exact_noop"].as_bool()&&Monstey_ReadFile(roundtrip)==bytes22&&Monstey_ReadFile(noop)==bytes22,
                      "Raw22 generic profile no-op changed bytes");
        return J::object({{"byte_exact",true},{"disk",Monstey_AssertLayout(bytes22,0,22)},{"public_channel_count",static_cast<std::uint64_t>(scene.parts[0].channels.size())}});
    });
    test("standard8_storage_rebuild_keeps_literal_eight_attribute_profile",[&] {
        auto scene=Monstey_ModelReadScene(source8);scene.parts[0].channels.at("POSITION").values[0]=25;
        const auto output=directory/L"authored-8.mesh";(void)Monstey_ModelWriteScene(scene,source8,output,Monstey_FloatOptions());
        return Monstey_AssertLayout(Monstey_ReadFile(output),0,8,3);
    });
    test("scene_new13_uv_boundary_is_accepted",[&] {
        auto scene=Monstey_ModelReadScene(source8);Monstey_SetProfile(scene.parts[0],6,1);
        const auto output=directory/L"scene-new13.mesh";(void)Monstey_ModelWriteScene(scene,source8,output,Monstey_Options());
        return Monstey_AssertLayout(Monstey_ReadFile(output),0,13);
    });
    test("scene_new14_uv_channel_overflow_is_guarded",[&] {
        auto scene=Monstey_ModelReadScene(source8);Monstey_SetProfile(scene.parts[0],7,1);
        const auto output=directory/L"scene-new14-uv.mesh";
        return Monstey_LimitCase(baseline,output,[&] {return Monstey_ModelWriteScene(scene,source8,output,Monstey_Options());});
    });
    test("scene_new14_color_channel_overflow_is_guarded",[&] {
        auto scene=Monstey_ModelReadScene(source8);Monstey_SetProfile(scene.parts[0],1,7);
        const auto output=directory/L"scene-new14-color.mesh";
        return Monstey_LimitCase(baseline,output,[&] {return Monstey_ModelWriteScene(scene,source8,output,Monstey_Options());});
    });
    test("scene_new14_preflight_rejects_without_output_or_report",[&] {
        auto scene=Monstey_ModelReadScene(source8);Monstey_SetProfile(scene.parts[0],7,1);
        const auto output=directory/L"scene-new14-preflight.mesh";
        return Monstey_LimitCase(baseline,output,[&] {return Monstey_ModelWriteScene(scene,source8,output,Monstey_Options(true));},true);
    });
    test("raw14_roundtrip_and_scene_noop_are_byte_exact",[&] {
        const auto a=directory/L"raw14-roundtrip.mesh",b=directory/L"raw14-noop.mesh";
        (void)Monstey_ModelRoundtrip(source14,a);const auto scene=Monstey_ModelReadScene(source14);
        const auto result=Monstey_ModelWriteScene(scene,source14,b);
        Monstey_Check(result["byte_exact_noop"].as_bool()&&Monstey_ReadFile(a)==bytes14&&Monstey_ReadFile(b)==bytes14,"Raw14 no-op changed bytes");
        return J::object({{"byte_exact",true},{"disk",Monstey_AssertLayout(Monstey_ReadFile(b),0,14)}});
    });
    test("raw14_compatible_same_topology_edit_retains_declaration",[&] {
        auto scene=Monstey_ModelReadScene(source14);scene.parts[0].channels.at("POSITION").values[0]=25;
        const auto output=directory/L"raw14-compatible.mesh";(void)Monstey_ModelWriteScene(scene,source14,output);
        const auto bytes=Monstey_ReadFile(output);const auto old=Monstey_ReadDisk(bytes14),now=Monstey_ReadDisk(bytes);
        Monstey_Check(bytes!=bytes14&&old.layouts==now.layouts&&old.indices==now.indices&&old.palette==now.palette,"Compatible raw14 edit lost native representation");
        Monstey_Check(Monstey_ModelReadScene(output).parts[0].channels.at("POSITION").values[0]==25,"Compatible edit absent");
        return Monstey_AssertLayout(bytes,0,14);
    });
    const auto mixed=directory/L"mixed14-8.mesh";const auto mixed_bytes=Monstey_Fixture({{7,1},{1,1}});Monstey_WriteNewFile(mixed,mixed_bytes);sources.Pin(mixed);
    for(const bool change_raw14:{false,true}) {
        test(change_raw14?"raw14_compatible_patch_survives_other_draw_channel_rebuild":"raw14_unchanged_survives_other_draw_channel_rebuild",[&] {
            auto scene=Monstey_ModelReadScene(mixed);
            if(change_raw14)scene.parts[0].channels.at("POSITION").values[0]=25;
            Monstey_SetProfile(scene.parts[1],2,1);
            const auto output=directory/(change_raw14?L"mixed-compatible14.mesh":L"mixed-unchanged14.mesh");
            (void)Monstey_ModelWriteScene(scene,mixed,output,Monstey_Options());
            const auto bytes=Monstey_ReadFile(output);const auto old=Monstey_ReadDisk(mixed_bytes),now=Monstey_ReadDisk(bytes);
            Monstey_Check(now.layouts[now.rows[0][9]]==old.layouts[old.rows[0][9]],"Rebuilt container discarded raw14 declaration");
            if(!change_raw14)Monstey_Check(Monstey_DrawVertices(old,0)==Monstey_DrawVertices(now,0),"Untouched raw14 vertex bytes changed");
            else Monstey_Check(Monstey_ModelReadScene(output).parts[0].channels.at("POSITION").values[0]==25,"Fallback raw14 patch missing");
            return J::object({{"retained_raw14",Monstey_AssertLayout(bytes,0,14)},{"rebuilt_draw",Monstey_AssertLayout(bytes,1,9)}});
        });
    }
    for(const auto profile:std::array<Monstey_Profile,3>{{{6,1},{7,1},{6,2}}}) {
        const auto count=6+profile.uv+profile.color;
        const auto name=count==13?"glb_new13_uv_boundary_accepted":profile.color==1?"glb_new14_uv_overflow_guarded":"glb_new14_color_overflow_guarded";
        test(name,[&] {
            const auto stem=L"generic-"+std::to_wstring(profile.uv)+L"-"+std::to_wstring(profile.color);
            const auto source=directory/(stem+L".glb"),output=directory/(stem+L".mesh");Monstey_WriteNewFile(source,Monstey_CustomGlb(profile.uv,profile.color));sources.Pin(source);
            auto options=Monstey_Options();options["binding"]="rigid";
            if(count==13) { (void)Monstey_ModelConvert(source,source8,output,options);return Monstey_AssertLayout(Monstey_ReadFile(output),0,13); }
            return Monstey_LimitCase(baseline,output,[&] {return Monstey_ModelConvert(source,source8,output,options);});
        });
    }
    const auto exported=directory/L"raw14.glb";(void)Monstey_ModelExportGlb(source14,exported);sources.Pin(exported);
    test("native_glb_raw14_noop_retains_all_bytes",[&] {
        const auto output=directory/L"raw14-glb-noop.mesh";
        (void)Monstey_ModelConvert(exported,source14,output,J::object({{"binding","native"}}));
        Monstey_Check(Monstey_ReadFile(output)==bytes14,"Native GLB raw14 no-op changed bytes");
        return J::object({{"byte_exact",true}});
    });
    for(const bool color:{false,true}) {
        test(color?"native_glb_color_edit_requiring_new_storage14_guarded":"native_glb_uv_edit_requiring_new_storage14_guarded",[&] {
            auto glb=Monstey_ReadGlb(exported);Monstey_EditGlb(glb,color?"COLOR_0":"TEXCOORD_0");
            const auto source=directory/(color?L"native-color-edit.glb":L"native-uv-edit.glb");
            const auto output=directory/(color?L"native-color-edit.mesh":L"native-uv-edit.mesh");Monstey_WriteNewFile(source,Monstey_SaveGlb(std::move(glb)));sources.Pin(source);
            auto options=Monstey_FloatOptions();options["binding"]="native";
            return Monstey_LimitCase(baseline,output,[&] {auto result=Monstey_ModelConvert(source,source14,output,options);result["test_position_type"]=3u;return result;});
        });
    }
    const auto import_options=J::object({{"blend_template",Monstey_Utf8(blend_template.wstring())}});
    for(const unsigned declarations:{13u,14u}) {
        test(declarations==13?"cpp_blender13_uv_edit_new_storage_accepted":"cpp_blender14_uv_edit_new_storage_guarded",[&] {
            const auto source=declarations==13?source13:source14;
            const auto blend_path=directory/(L"native-"+std::to_wstring(declarations)+L".blend");
            (void)Monstey_ModelImportNative(source,blend_path,{},import_options);sources.Pin(blend_path);
            Monstey_Blend blend(blend_path);Monstey_EditBlend(blend,Monstey_BlendMesh(blend),"UVMap");
            const auto edited=directory/(L"uv-edit-"+std::to_wstring(declarations)+L".blend");
            (void)blend.Write(edited);sources.Pin(edited);const auto parsed=Monstey_ModelReadBlendScene(edited);
            Monstey_Check(parsed.parts[0].channels.at("UV0").values[0]==.125,"Blender UV edit did not reach shared scene");
            const auto output=directory/(L"blender-uv-"+std::to_wstring(declarations)+L".mesh");
            if(declarations==13) { (void)Monstey_ModelExportNative(edited,{},output,Monstey_FloatOptions());return Monstey_AssertLayout(Monstey_ReadFile(output),0,13,3); }
            return Monstey_LimitCase(baseline,output,[&] {auto result=Monstey_ModelExportNative(edited,{},output,Monstey_FloatOptions());result["test_position_type"]=3u;return result;});
        });
    }
    test("cpp_blender14_color_edit_new_storage_guarded",[&] {
        const auto base=directory/L"native-14.blend";Monstey_Blend blend(base);Monstey_EditBlend(blend,Monstey_BlendMesh(blend),"_SOW_COLOR0");
        const auto edited=directory/L"color-edit14.blend",output=directory/L"blender-color14.mesh";(void)blend.Write(edited);sources.Pin(edited);
        Monstey_Check(Monstey_ModelReadBlendScene(edited).parts[0].channels.at("COLOR0").values[0]==42,"Blender byte color edit absent");
        return Monstey_LimitCase(baseline,output,[&] {auto result=Monstey_ModelExportNative(edited,{},output,Monstey_FloatOptions());result["test_position_type"]=3u;return result;});
    });
    test("cpp_blender14_noop_preserves_native_bytes",[&] {
        const auto output=directory/L"blender-raw14-noop.mesh";(void)Monstey_ModelExportNative(directory/L"native-14.blend",{},output);
        Monstey_Check(Monstey_ReadFile(output)==bytes14,"C++ Blender raw14 no-op changed bytes");return J::object({{"byte_exact",true}});
    });
    sources.Verify();
    return J::object({{"schema","Monstey.ModelLayoutAuthoringTests/1"},{"mode",baseline?"accepted016_baseline":"candidate"},{"groups",static_cast<std::uint64_t>(tests.size())},{"failed",failed},{"tests",tests},
        {"source_pins",sources.Report()},{"source_snapshot_checks",sources.Checks()},{"all_source_snapshots_unchanged",true},
        {"fixture_source","independent_synthetic_MMSH17_and_GLB2"},{"original_assets_copied",false},{"original_game_or_blender_executed",false},
        {"blend_template",Monstey_Utf8(blend_template.wstring())},{"blend_template_sha256",Monstey_HashSha256(blend_template)},
        {"blender_scope","Real C++ import/edit/export of existing baseline UV/color channels with half-to-float storage change. Arbitrary new Blender attributes are not claimed to become new MMSH channels."}});
}
} // namespace
int wmain(int argc,wchar_t** argv) {
    try {
        Monstey_Check(argc==3||argc==4,"Usage: LayoutTests NEW-output-directory --candidate|--baseline [template.blend]");
        const std::filesystem::path directory=argv[1];const std::wstring mode=argv[2];
        Monstey_Check(mode==L"--candidate"||mode==L"--baseline","Unknown mode");
        Monstey_Check(!std::filesystem::exists(directory),"Test output directory must be new");Monstey_RequireOfflineOutput(directory);
        const auto blend_template=argc==4?std::filesystem::path(argv[3]):Monstey_BlendGeometryTemplate();
        auto report=Monstey_Run(directory,mode==L"--baseline",blend_template);
        Monstey_WriteNewText(directory/L"report.json",report.dump(2));std::cout<<report.dump(2)<<'\n';
        return report["failed"].as_uint()==0?0:1;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}