#include "Monstey_SkelDefaultPose.hpp"
#include "Monstey_SkelDefaultNativePin.hpp"
#include "oracles/Monstey_FrozenDefaultOracle.hpp"
#include <bit>
#include <cmath>
namespace M=Monstey;
namespace {
using J=M::Monstey_Json;
void Monstey_TestRequire(bool v,const char* t){if(!v)throw std::runtime_error(t);}
std::uint32_t Monstey_Word(float v){return std::bit_cast<std::uint32_t>(v);}
float Monstey_Float(std::uint32_t v){return std::bit_cast<float>(v);}
void Monstey_RawPose(Monstey_Bytes& b,std::size_t s,const Monstey_Packed7& v){const auto p=Monstey_ParseSkel(b).boneStart+37*s+5;for(unsigned j=0;j<7;++j)Monstey_Put32(b,p+4*j,Monstey_Word(v[j]));}
M::Monstey_SkelDefaultPose Monstey_Compare(const Monstey_Bytes& bytes){
    const auto before=M::Monstey_HashSha256(bytes);const auto expected=Monstey_RigFromSkel(bytes);const auto parsed=Monstey_ParseSkel(bytes);
    auto actual=M::Monstey_SkelDefaultPoseRead(bytes);
    Monstey_TestRequire(actual.nodes.size()==expected.size()&&actual.hk_to_source==parsed.runtimeToSource,"Runtime permutation differs");
    for(std::size_t i=0;i<actual.nodes.size();++i){const auto& a=actual.nodes[i];const auto& b=expected[i];
        Monstey_TestRequire(a.hk_index==i&&a.source_index==parsed.runtimeToSource[i]&&actual.source_to_hk[a.source_index]==i,"HK/source inverse mapping differs");
        Monstey_TestRequire(a.parent_hk==(i?b.parent:-1)&&a.name_hash==b.hash&&a.raw_flag_byte==b.rawFlag,"Runtime node identity differs");
        for(unsigned j=0;j<3;++j)Monstey_TestRequire(a.local_words[j]==Monstey_Word(b.localPosition[j]),"Local position raw word differs");
        for(unsigned j=0;j<4;++j)Monstey_TestRequire(a.local_words[j+3]==Monstey_Word(b.localQuaternion[j]),"Local quaternion raw word differs");
        for(unsigned j=0;j<3;++j)Monstey_TestRequire(a.default_pose_words[j]==a.local_words[j],"Default XYZ transfer differs");
        for(unsigned j=0;j<4;++j)Monstey_TestRequire(a.default_pose_words[j+4]==a.local_words[j+3],"Default Q transfer differs");
        Monstey_TestRequire(a.default_pose_words[3]==0&&a.inverse_pose_words[3]==0x3f800000u,"Default/inverse W collapsed");
        Monstey_TestRequire(a.inverse_pose_words==b.inverseBind.words,"Raw inverse words differ");
    }
    Monstey_TestRequire(M::Monstey_HashSha256(bytes)==before&&actual.source_sha256==before,"Input changed or identity lost");return actual;
}
J Monstey_PoseJson(const M::Monstey_SkelDefaultPose& p){
    auto rows=J::array();for(const auto& n:p.nodes){auto l=J::array(),d=J::array(),inv=J::array();for(auto v:n.local_words)l.as_array().emplace_back(v);for(auto v:n.default_pose_words)d.as_array().emplace_back(v);for(auto v:n.inverse_pose_words)inv.as_array().emplace_back(v);
        rows.as_array().push_back(J::object({{"hk_index",static_cast<std::uint64_t>(n.hk_index)},{"source_index",static_cast<std::uint64_t>(n.source_index)},{"parent_hk",n.parent_hk},{"name",n.name},{"name_hash",n.name_hash},{"raw_flag_byte",n.raw_flag_byte},{"position_channel_enabled",(n.raw_flag_byte&1)==0},{"local_words",l},{"default_pose_words",d},{"inverse_pose_words",inv}}));}
    return J::object({{"schema","Monstey.SkelDefaultPose/1"},{"source_sha256",p.source_sha256},{"nodes",rows},{"native_y_up_cm_xyzw",true},{"quaternion_normalized",false},{"position_W",0},{"inverse_position_W",1},{"animation_applied",false},{"runtime_execution_verified",false}});
}
Monstey_Bytes Monstey_Chain256(){
    std::string names;std::vector<std::uint32_t> offsets;for(unsigned i=0;i<256;++i){offsets.push_back(static_cast<std::uint32_t>(names.size()));names+="node"+std::to_string(i);names.push_back(0);}
    Monstey_Bytes b(96);Monstey_Put32(b,0,0x4c454b53);Monstey_Put32(b,4,17);Monstey_Put32(b,12,static_cast<std::uint32_t>(names.size()));Monstey_Put32(b,40,256);
    M::Monstey_AppendU32(b,0xdcdcff00);b.insert(b.end(),names.begin(),names.end());
    for(unsigned i=0;i<256;++i){M::Monstey_AppendU32(b,offsets[i]);b.push_back(static_cast<std::uint8_t>(i));for(const auto v:{static_cast<float>(i),0.0f,0.0f,0.0f,0.0f,0.0f,1.0f})M::Monstey_AppendU32(b,Monstey_Word(v));M::Monstey_AppendU32(b,i==255?0u:1u);}return b;
}
J Monstey_RunTests(const std::vector<std::filesystem::path>& paths){
    auto rows=J::array();unsigned failed=0;const auto test=[&](const char* name,const std::function<void()>& body){try{body();rows.as_array().push_back(J::object({{"name",name},{"passed",true}}));}catch(const std::exception& e){++failed;rows.as_array().push_back(J::object({{"name",name},{"passed",false},{"error",e.what()}}));}};
    test("nontrivial_sibling_block_permutation",[]{auto p=Monstey_Compare(Monstey_SyntheticSkel());Monstey_TestRequire(p.hk_to_source==std::vector<std::size_t>{0,1,4,2,3,5,6},"Wrong expected permutation");});
    test("noncommuting_raw_parent_inverse_times_child",[]{auto b=Monstey_SyntheticSkel();Monstey_RawPose(b,0,{3,-4,5,0.3f,0.4f,0.5f,0.6f});Monstey_RawPose(b,1,{-8,12,1,-0.5f,0.25f,0.75f,0.125f});Monstey_RawPose(b,2,{9,-3,7,0.2f,-0.3f,0.4f,-0.5f});(void)Monstey_Compare(b);});
    test("nonunit_is_not_normalized_analytic",[]{auto b=Monstey_SyntheticSkel();Monstey_RawPose(b,0,{0,0,0,0,0,0,2});Monstey_RawPose(b,1,{1,2,3,0,0,0,3});auto p=Monstey_Compare(b);const auto& n=p.nodes[p.source_to_hk[1]];Monstey_TestRequire(Monstey_Float(n.local_words[0])==4&&Monstey_Float(n.local_words[1])==8&&Monstey_Float(n.local_words[2])==12&&Monstey_Float(n.local_words[6])==6,"Incorrect raw parent result");});
    test("root_identity_parent",[]{auto b=Monstey_SyntheticSkel();Monstey_RawPose(b,0,{12,-3,8,0,0,0,2});auto p=Monstey_Compare(b);Monstey_TestRequire(Monstey_Float(p.nodes[0].local_words[0])==12&&Monstey_Float(p.nodes[0].local_words[6])==2,"Root transform cancelled");});
    test("all256_raw_flags_keep_slots",[]{auto b=Monstey_SyntheticSkel();const auto at=Monstey_ParseSkel(b).boneStart;for(unsigned f=0;f<256;++f){b[at+4]=static_cast<std::uint8_t>(f);auto p=Monstey_Compare(b);Monstey_TestRequire(p.nodes.size()==7&&p.nodes[0].raw_flag_byte==f,"Flag changed node count");}});
    test("distinct_W_words",[]{auto p=Monstey_Compare(Monstey_SyntheticSkel());for(const auto& n:p.nodes)Monstey_TestRequire(n.default_pose_words[3]==0&&n.inverse_pose_words[3]==0x3f800000u,"W words differ");});
    test("global_signed_zero_bits_preserved",[]{auto b=Monstey_SyntheticSkel();Monstey_RawPose(b,0,{-0.0f,0,-0.0f,-0.0f,0,0,1});auto p=Monstey_Compare(b);Monstey_TestRequire(p.nodes[0].global_words[0]==0x80000000u&&p.nodes[0].global_words[3]==0x80000000u,"Signed-zero source bits lost");});
    test("multiple_roots_rejected",[]{auto b=Monstey_SyntheticSkel();Monstey_Put32(b,Monstey_ParseSkel(b).boneStart+33,0);Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});});
    test("truncated_bone_rejected",[]{auto b=Monstey_SyntheticSkel();b.pop_back();Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});});
    test("invalid_name_pool_offset_rejected",[]{auto b=Monstey_SyntheticSkel();Monstey_Put32(b,Monstey_ParseSkel(b).boneStart,Monstey_U32(b,12));Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});});
    test("nonfinite_raw_lanes_rejected",[]{for(auto w:{0x7fc12345u,0x7f800000u,0xff800000u}){auto b=Monstey_SyntheticSkel();Monstey_Put32(b,Monstey_ParseSkel(b).boneStart+5,w);Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});}});
    test("finite_input_overflow_rejected",[]{auto b=Monstey_SyntheticSkel();Monstey_RawPose(b,0,{1,2,3,0,0,0,1e30f});Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});});
    test("zero_and_over256_counts_rejected",[]{for(auto n:{0u,257u}){auto b=Monstey_SyntheticSkel();Monstey_Put32(b,40,n);Monstey_Rejects([&]{(void)M::Monstey_SkelDefaultPoseRead(b);});}});
    test("opaque_tail_not_interpreted",[]{auto b=Monstey_SyntheticSkel();auto a=Monstey_Compare(b);b.insert(b.end(),{0xa7,0xb2,0xc3});auto c=Monstey_Compare(b);for(std::size_t i=0;i<a.nodes.size();++i)Monstey_TestRequire(a.nodes[i].default_pose_words==c.nodes[i].default_pose_words,"Tail changed pose");});
    test("200_seeded_raw_F32_cases_match_frozen_oracle",[]{std::uint32_t seed=0x4e5b1234;for(unsigned c=0;c<200;++c){auto b=Monstey_SyntheticSkel();for(unsigned i=0;i<7;++i){Monstey_Packed7 p;for(auto& f:p){seed=seed*1664525u+1013904223u;f=(static_cast<float>((seed>>16)&65535u)-32768)/8192;}Monstey_RawPose(b,i,p);}(void)Monstey_Compare(b);}});
    test("count256_independent_chain_boundary",[]{auto b=Monstey_Chain256();auto p=M::Monstey_SkelDefaultPoseRead(b);Monstey_TestRequire(p.nodes.size()==256,"Missing node255");for(std::size_t i=0;i<256;++i){const auto& n=p.nodes[i];Monstey_TestRequire(n.hk_index==i&&n.source_index==i&&n.parent_hk==(i?static_cast<int>(i)-1:-1)&&n.raw_flag_byte==i,"Chain identity mismatch");Monstey_TestRequire(Monstey_Float(n.local_words[0])==(i?1.0f:0.0f)&&n.local_words[6]==0x3f800000u,"Chain local transform differs");}});
    for(const auto& path:paths)test("actual_SKEL_raw_words_and_input_immutability",[&]{const auto b=M::Monstey_ReadFile(path);const auto hash=M::Monstey_HashSha256(b);(void)Monstey_Compare(b);Monstey_TestRequire(M::Monstey_HashSha256(path)==hash,"Actual source changed");});
    return J::object({{"schema","Monstey.SkelDefaultPoseTests/1"},{"groups",static_cast<std::uint64_t>(rows.size())},{"failed",failed},{"tests",rows},{"exact_words_match_frozen_native_derived_oracle",failed==0},{"native_program_executed",false}});
}
}
int wmain(int argc,wchar_t** argv){try{
    Monstey_TestRequire(argc>=2,"Usage --test NEW.json [SKEL...] | --inspect SKEL NEW.json | --verify-native manifest first.exe second.exe NEW.json");
    const std::wstring command=argv[1];J report;std::filesystem::path output;
    if(command==L"--test"){Monstey_TestRequire(argc>=3,"Missing new report");output=argv[2];std::vector<std::filesystem::path> paths;for(int i=3;i<argc;++i)paths.emplace_back(argv[i]);report=Monstey_RunTests(paths);M::Monstey_WriteNewText(output,report.dump(2)+"\n");std::cout<<"Default-pose tests: "<<report["groups"].as_uint()<<" groups, failures="<<report["failed"].as_uint()<<'\n';return report["failed"].as_uint()?1:0;}
    if(command==L"--inspect"){Monstey_TestRequire(argc==4,"Expected SKEL and new report");output=argv[3];report=Monstey_PoseJson(M::Monstey_SkelDefaultPoseRead(M::Monstey_ReadFile(argv[2])));}
    else if(command==L"--verify-native"){Monstey_TestRequire(argc==6,"Expected manifest, pinned PEs and new report");output=argv[5];Monstey_TestRequire(M::Monstey_HashSha256(std::filesystem::path(argv[3]))=="6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455"&&M::Monstey_HashSha256(std::filesystem::path(argv[4]))=="ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef","Pinned PE identity differs");Monstey_TestRequire(M::Monstey_HashSha256(std::filesystem::path(argv[2]))==Monstey_DefaultPoseManifestSha256,"Native manifest identity differs");const auto count=Monstey_VerifyNative(argv[2],argv[3],argv[4]);Monstey_TestRequire(count==Monstey_DefaultPoseManifestRows,"Native manifest row coverage differs");report=J::object({{"schema","Monstey.SkelDefaultNativeCheck/1"},{"status","PASS"},{"rows",static_cast<std::uint64_t>(count)},{"two_full_PE_hashes_verified",true},{"original_executed",false}});}
    else throw std::runtime_error("Unknown default-pose probe command");
    M::Monstey_WriteNewText(output,report.dump(2)+"\n");std::cout<<"Default-pose probe PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}