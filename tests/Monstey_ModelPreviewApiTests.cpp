#include <chrono>
#include "Monstey_ModelPreviewApi.hpp"
#include "Monstey_ModelPreviewApiFixture.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
using namespace Monstey;
namespace {
using J=Monstey_Json;
void Monstey_Check(bool valid,const char* text){if(!valid)throw Monstey_Error("preview_api_test",text);}
std::string Monstey_PathText(const std::filesystem::path& p){return Monstey_Utf8(p.wstring());}
J Monstey_Args(const std::filesystem::path& source,const std::filesystem::path& output) {
 return J::object({{"source",Monstey_PathText(source)},{"output",Monstey_PathText(output)},{"width",64},{"height",64}});
}
void Monstey_NoPartials(const std::filesystem::path& directory) {
 for(const auto& entry:std::filesystem::recursive_directory_iterator(directory))
  Monstey_Check(!entry.path().filename().wstring().ends_with(L".partial"),"Staging file survived API call");
}
void Monstey_Failure(const std::function<void()>& action,const std::filesystem::path& output,const std::string& code={}) {
 bool failed=false;
 try{action();}catch(const Monstey_Error& e){failed=true;Monstey_Check(code.empty()||e.code==code,"Unexpected native error code");}
 catch(const std::exception&){failed=true;Monstey_Check(code.empty(),"Expected typed native error");}
 Monstey_Check(failed,"Invalid preview request accepted");
 if(!output.empty())Monstey_Check(!std::filesystem::exists(output),"Rejected preview left output");
}
}
int wmain(int argc,wchar_t** argv) {
 try {
  Monstey_Check(argc==2||argc==4,"Usage PreviewApiTests NEW-directory [actual.mesh actual.skel]");
  const std::filesystem::path directory=std::filesystem::path(argv[1]) / (L"run-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));Monstey_Check(!std::filesystem::exists(directory),"Test directory must be new");
  Monstey_RequireOfflineOutput(directory);std::filesystem::create_directories(directory);
  const auto mesh=directory/L"synthetic.mesh",skel=directory/L"synthetic.skel";
  const auto meshBytes=Monstey_MeshFixture(),skelBytes=Monstey_SkelFixture();
  Monstey_WriteNewFile(mesh,meshBytes);Monstey_WriteNewFile(skel,skelBytes);
  const auto meshHash=Monstey_HashSha256(mesh),skelHash=Monstey_HashSha256(skel);
  J tests=J::array();unsigned failures=0;
  const auto test=[&](const std::string& name,const std::function<void()>& action){
   try{action();Monstey_NoPartials(directory);tests.as_array().push_back(J::object({{"name",name},{"passed",true}}));}
   catch(const std::exception& e){++failures;tests.as_array().push_back(J::object({{"name",name},{"passed",false},{"error",e.what()}}));}
  };
  test("catalog_typed_flat_options_and_optional_guards",[&]{const auto c=Monstey_ModelPreviewCatalog();Monstey_Check(c.size()==1&&c[0u]["name"].as_string()=="model_preview_render"&&!c[0u]["read_only"].as_bool(),"Catalog operation differs");const auto& s=c[0u]["inputSchema"];Monstey_Check(s["required"]==J::array({"source","output"})&&!s["additionalProperties"].as_bool(),"Guard optionality differs");Monstey_Check(s["properties"]["width"]["type"].as_string()=="integer"&&s["properties"]["bones"]["type"].as_string()=="boolean"&&s["properties"]["zoom"]["type"].as_string()=="number","Viewport schema is untyped");});
  test("guardless_mesh_only_native_BMP",[&]{const auto out=directory/L"guardless.bmp";const auto result=Monstey_ModelPreviewCall("model_preview_render",Monstey_Args(mesh,out));const auto bmp=Monstey_ReadFile(out);Monstey_Check(Monstey_U16(bmp,0)==0x4d42&&Monstey_U32(bmp,18)==64&&Monstey_U32(bmp,22)==static_cast<std::uint32_t>(-64),"BMP layout differs");Monstey_Check(result["inputs"]["source"]["sha256"].as_string()==meshHash&&!result["inputs"]["source"]["expected_hash_guard"].as_bool()&&result["inputs"]["skeleton"].is_null(),"Automatic source identity missing");Monstey_Check(result["host_bind_geometry_only"].as_bool()&&!result["native_materials"].as_bool()&&!result["animation_playback"].as_bool(),"Scope flags differ");Monstey_Check(result["output_sha256"].as_string()==Monstey_HashSha256(out)&&result["output_bytes"].as_uint()==bmp.size(),"Published BMP hash differs");});
  test("guardless_skeleton_and_shared_core_byte_equivalence",[&]{const auto out=directory/L"with-skeleton.bmp";auto a=Monstey_Args(mesh,out);a["skeleton"]=Monstey_PathText(skel);const auto result=Monstey_ModelPreviewRenderFile(a);auto o=Monstey_ModelPreviewOptions{};o.width=o.height=64;const auto expected=Monstey_ModelPreviewBitmap(Monstey_ModelPreviewRender(Monstey_ModelReadScene(mesh,skel),o));Monstey_Check(Monstey_ReadFile(out)==expected,"Wrapper differs from shared native renderer");Monstey_Check(!result["inputs"]["skeleton"]["expected_hash_guard"].as_bool()&&result["inputs"]["skeleton"]["sha256"].as_string()==skelHash,"Optional skeleton guard became mandatory");});
  test("both_expected_guards_and_uppercase_hex",[&]{auto a=Monstey_Args(mesh,directory/L"guarded.bmp");a["skeleton"]=Monstey_PathText(skel);auto upper=meshHash;for(auto& c:upper)if(c>='a'&&c<='f')c=static_cast<char>(c-'a'+'A');a["expected_source_sha256"]=upper;a["expected_skeleton_sha256"]=skelHash;const auto result=Monstey_ModelPreviewCall("model_preview_render",a);Monstey_Check(result["inputs"]["source"]["expected_hash_guard"].as_bool()&&result["inputs"]["skeleton"]["expected_hash_guard"].as_bool(),"Supplied guards were ignored");});
  test("typed_viewport_is_forwarded",[&]{auto a=Monstey_Args(mesh,directory/L"options.bmp");a["skeleton"]=Monstey_PathText(skel);a["width"]=96;a["height"]=80;a["container"]=1;a["all_containers"]=true;a["bones"]=false;a["wireframe"]=true;a["vertex_colors"]=true;a["yaw_degrees"]=25.5;a["pitch_degrees"]=-8;a["zoom"]=1.5;a["pan_x"]=0.1;a["pan_y"]=-0.1;const auto r=Monstey_ModelPreviewRenderFile(a);Monstey_Check(r["viewport"]["width"].as_uint()==96&&r["viewport"]["height"].as_uint()==80&&r["viewport"]["wireframe"].as_bool()&&r["viewport"]["yaw_degrees"].as_number()==25.5&&r["preview"]["parts"].as_uint()==2&&r["preview"]["bones"].empty(),"Viewport option lost");});
  test("source_hash_mismatch_no_output",[&]{const auto out=directory/L"bad-source-hash.bmp";auto a=Monstey_Args(mesh,out);a["expected_source_sha256"]=std::string(64,'0');Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"source_hash_mismatch");});
  test("skeleton_hash_mismatch_no_output",[&]{const auto out=directory/L"bad-skel-hash.bmp";auto a=Monstey_Args(mesh,out);a["skeleton"]=Monstey_PathText(skel);a["expected_skeleton_sha256"]=std::string(64,'0');Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"source_hash_mismatch");});
  test("orphan_and_malformed_guards_rejected",[&]{for(const auto bad:J::Array{J::object({{"expected_skeleton_sha256",skelHash}}),J::object({{"expected_source_sha256",""}}),J::object({{"expected_source_sha256",std::string(64,'x')}}),J::object({{"expected_source_sha256",42}})}){const auto out=directory/L"malformed-guard.bmp";auto a=Monstey_Args(mesh,out);for(const auto& [key,v]:bad.as_object())a[key]=v;Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");}});
  test("unknown_operation_and_arguments",[&]{const auto out=directory/L"unknown.bmp";auto a=Monstey_Args(mesh,out);Monstey_Failure([&]{(void)Monstey_ModelPreviewCall("model_preview_guess",a);},out,"unknown_operation");a["animation"]=true;Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");});
  test("required_paths_and_object_shape",[&]{const auto out=directory/L"missing.bmp";for(const auto key:{"source","output"}){auto a=Monstey_Args(mesh,out);a.as_object().erase(key);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");}Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(J::array());},out,"model_preview");auto a=Monstey_Args(mesh,out);a["source"]=std::string("bad\0path",8);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");});
  test("viewport_type_and_limit_controls",[&]{const J cases=J::array({J::object({{"width",31}}),J::object({{"width",4097}}),J::object({{"width",64.5}}),J::object({{"width",true}}),J::object({{"height",0}}),J::object({{"width",4096},{"height",4096}}),J::object({{"container",-1}}),J::object({{"bones",1}}),J::object({{"mesh","false"}}),J::object({{"zoom",0}}),J::object({{"zoom",21}}),J::object({{"pan_x",9}}),J::object({{"yaw_degrees",false}})});for(const auto& c:cases.as_array()){const auto out=directory/L"invalid-options.bmp";auto a=Monstey_Args(mesh,out);for(const auto& [key,v]:c.as_object())a[key]=v;Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");}});
  test("missing_inputs_and_nonfile_source",[&]{const auto out=directory/L"missing-input.bmp";auto a=Monstey_Args(directory/L"absent.mesh",out);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out);a=Monstey_Args(directory,out);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");a=Monstey_Args(mesh,out);a["skeleton"]=Monstey_PathText(directory/L"absent.skel");Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out);});
  test("malformed_native_mesh_and_skeleton",[&]{const auto badMesh=directory/L"malformed.mesh",badSkel=directory/L"malformed.skel";Monstey_WriteNewFile(badMesh,Monstey_Bytes{1,2,3,4});Monstey_WriteNewFile(badSkel,Monstey_Bytes{1,2,3,4});const auto out=directory/L"malformed.bmp";Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(Monstey_Args(badMesh,out));},out);auto a=Monstey_Args(mesh,out);a["skeleton"]=Monstey_PathText(badSkel);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out);});
  test("nonfinite_native_position_no_output",[&]{auto raw=meshBytes;const auto data=20u+Monstey_U32(raw,8)+Monstey_U32(raw,12);Monstey_PutU16(raw,data+4,0x7e00);const auto bad=directory/L"nonfinite.mesh";Monstey_WriteNewFile(bad,raw);const auto out=directory/L"nonfinite.bmp";Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(Monstey_Args(bad,out));},out);});
  test("missing_container_no_output",[&]{const auto out=directory/L"no-container.bmp";auto a=Monstey_Args(mesh,out);a["container"]=99;Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},out,"model_preview");});
  test("source_and_skeleton_overwrite_rejected",[&]{auto a=Monstey_Args(mesh,mesh);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},{});a=Monstey_Args(mesh,skel);a["skeleton"]=Monstey_PathText(skel);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},{});Monstey_Check(Monstey_ReadFile(mesh)==meshBytes&&Monstey_ReadFile(skel)==skelBytes,"Overwrite guard altered an input");});
  test("existing_output_preserved_and_early_rejection",[&]{const auto out=directory/L"existing.bmp";const Monstey_Bytes marker{9,8,7,6};Monstey_WriteNewFile(out,marker);auto a=Monstey_Args(directory/L"absent.mesh",out);Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(a);},{},"output_exists");Monstey_Check(Monstey_ReadFile(out)==marker,"Existing output overwritten");});
  test("publication_parent_obstruction_no_partial",[&]{const auto block=directory/L"blocked-parent";Monstey_WriteNewFile(block,Monstey_Bytes{1});const auto out=block/L"new.bmp";Monstey_Failure([&]{(void)Monstey_ModelPreviewRenderFile(Monstey_Args(mesh,out));},out);Monstey_Check(Monstey_ReadFile(block)==Monstey_Bytes{1},"Publication failure altered obstruction");});
  test("same_output_parallel_calls_publish_only_once",[&]{const auto out=directory/L"parallel.bmp";std::promise<void> start;const auto signal=start.get_future().share();std::array<unsigned,2> successes{};const auto work=[&](std::size_t i){signal.wait();try{(void)Monstey_ModelPreviewRenderFile(Monstey_Args(mesh,out));successes[i]=1;}catch(const std::exception&) {}};std::thread first(work,0);try{std::thread second(work,1);start.set_value();first.join();second.join();}catch(...){try{start.set_value();}catch(...){}if(first.joinable())first.join();throw;}Monstey_Check(successes[0]+successes[1]==1,"Parallel output was published twice or not at all");Monstey_Check(Monstey_U16(Monstey_ReadFile(out),0)==0x4d42,"Parallel output is not complete BMP");});
  J actual=nullptr;
  if(argc==4)test("actual_asset_readonly_guarded_and_unguarded",[&]{const std::filesystem::path am=argv[2],as=argv[3];const auto mh=Monstey_HashSha256(am),sh=Monstey_HashSha256(as);auto a=Monstey_Args(am,directory/L"actual-guarded.bmp");a["skeleton"]=Monstey_PathText(as);a["expected_source_sha256"]=mh;a["expected_skeleton_sha256"]=sh;a["width"]=256;a["height"]=256;actual=Monstey_ModelPreviewRenderFile(a);a["output"]=Monstey_PathText(directory/L"actual-unguarded.bmp");a.as_object().erase("expected_source_sha256");a.as_object().erase("expected_skeleton_sha256");const auto unguarded=Monstey_ModelPreviewRenderFile(a);Monstey_Check(actual["output_sha256"]==unguarded["output_sha256"],"Optional guards changed rendered image");Monstey_Check(actual["preview"]["bone_edges"].as_uint()+1==actual["preview"]["bones"].size(),"Actual hierarchy edge count differs");Monstey_Check(Monstey_HashSha256(am)==mh&&Monstey_HashSha256(as)==sh,"Actual input changed");});
  Monstey_Check(Monstey_HashSha256(mesh)==meshHash&&Monstey_HashSha256(skel)==skelHash,"Synthetic source changed");
  auto report=J::object({{"schema","Monstey.ModelPreviewApiTests/1"},{"tests",tests},{"groups",static_cast<std::uint64_t>(tests.size())},{"failed",failures},{"actual",actual},{"source_hashes_unchanged",true},{"Python_runtime",false}});
  Monstey_WriteNewText(directory/L"report.json",report.dump(2)+"\n");std::cout<<"Native preview API tests: "<<tests.size()<<" groups; failed="<<failures<<"\n";return failures?1:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
