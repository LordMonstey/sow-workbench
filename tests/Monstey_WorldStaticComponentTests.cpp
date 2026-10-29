#include "Monstey_World.hpp"
#include "Monstey_WorldEdit.hpp"
#include "Monstey_WorldComposition.hpp"
#include "Monstey_WorldLighting.hpp"
#include "Monstey_LtmiMaterial.hpp"
#include "Monstey_StaticFixtures.hpp"
#include "Monstey_NavFixture.hpp"
#include "Monstey_LightFixture.hpp"
#include "Monstey_PhysicsFixture.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using J=Monstey_Json;using B=Monstey_Bytes;
unsigned passed{};J::Array cases;
void Check(bool x,std::string_view s){Monstey_Require(x,s);}
void Test(const char *name,const std::function<void()> &f){f();++passed;cases.push_back(name);std::cout<<"PASS "<<name<<'\n';}
void Reject(const std::function<void()> &f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
std::string P(const std::filesystem::path&p){return Monstey_Utf8(p.wstring());}
J Desc(const std::filesystem::path&p,const B&b){return J::object({{"path",P(p)},{"bytes",std::uint64_t(b.size())},{"sha256",Monstey_HashSha256(b)}});}
B Text(const J&j){const auto s=j.dump(2)+"\n";return B(s.begin(),s.end());}
}
int wmain(int argc,wchar_t **argv){try{
 Check(argc==2||argc==4,"NEW-output-directory [actual-MESH actual-WDSC]");
 const std::filesystem::path out=argv[1];Check(!std::filesystem::exists(out),"Test output exists");
 std::filesystem::create_directories(out);
 auto put=[&](const std::string&name,const B&b){const auto p=out/Monstey_Wide(name);Monstey_WriteNewFile(p,b);return Desc(p,b);};
 const auto mesh=Monstey_StaticFixture::MeshFixture(),render=Monstey_StaticFixture::RenderFixture();
 const auto md=put("source.wldmsh",mesh),rd=put("source.wldclnt",render);
 const auto spec=J::object({{"topology",J::object({{"groups",J::array({J::object({{"group_index",0},
  {"draws",J::array({J::object({{"source_draw",0}}),J::object({{"source_draw",0},{"material","materials/new.mat"}})})}})})}})}});
 const auto authored=Monstey_WorldMeshApplyBytes(mesh,spec);
 const auto built=Monstey_WorldStaticRenderBuild(authored.first,render,authored.second,
  "worlds/source.wldmsh","worlds/private/test.wldmsh",false);
 const auto cm=put("candidate.wldmsh",authored.first),cr=put("candidate.wldclnt",built.first),pd=put("topology.json",Text(authored.second));
 const auto validate=[&](const B&m,const B&r,const J&proof){return Monstey_WorldStaticPairValidate(mesh,render,m,r,proof,
  "worlds/source.wldmsh","worlds/private/test.wldmsh",false);};
 Test("new topology exact pair replay and indexed scene",[&]{const auto r=validate(authored.first,built.first,authored.second);
  Check(r.at("scene_format_validated").as_bool()&&!r.at("runtime_verified").as_bool(),"Pair or runtime claim differs");});
 Test("forged source topology hash rejected",[&]{auto p=authored.second;p["source_sha256"]=std::string(64,'0');Reject([&]{validate(authored.first,built.first,p);});});
 Test("forged encoded material slot rejected",[&]{auto p=authored.second;p["groups"][0]["draws"][0]["material_slot"]=6;Reject([&]{validate(authored.first,built.first,p);});});
 Test("forged original material slot rejected",[&]{auto p=authored.second;p["groups"][0]["draws"][0]["source_material_slot"]=6;Reject([&]{validate(authored.first,built.first,p);});});
 Test("altered indexed bounds rejected",[&]{auto r=built.first;const auto at=44ull+Monstey_U32(r,40)+12+4ull*9+12;Monstey_PutU32(r,at,std::bit_cast<std::uint32_t>(99.f));Reject([&]{validate(authored.first,r,authored.second);});});
 Test("opaque cull flag mutation rejected",[&]{auto r=built.first;const auto at=44ull+Monstey_U32(r,40)+12+4ull*9+12;r[at+24]^=1;Reject([&]{validate(authored.first,r,authored.second);});});
 Test("opaque draw parameter mutation rejected",[&]{auto m=authored.first;Monstey_PutU32(m,m.size()-8,0x12345678u);auto proof=authored.second;proof["output_sha256"]=Monstey_HashSha256(m);const auto r=Monstey_WorldStaticRenderBuild(m,render,proof,"worlds/source.wldmsh","worlds/private/test.wldmsh",false);Reject([&]{validate(m,r.first,proof);});});
 Test("opaque geometry dictionary mutation rejected",[&]{auto m=authored.first;m[32]^=1;auto proof=authored.second;proof["output_sha256"]=Monstey_HashSha256(m);const auto r=Monstey_WorldStaticRenderBuild(m,render,proof,"worlds/source.wldmsh","worlds/private/test.wldmsh",false);Reject([&]{validate(m,r.first,proof);});});
 Test("unselected context cannot change without render association",[&]{auto original=mesh;original.insert(original.end(),mesh.begin()+35,mesh.end());for(auto word:{2u,3u,4u,5u,6u})Monstey_PutU32(original,4*word,Monstey_U32(mesh,4*word)*2);const auto t=Monstey_WorldMeshApplyBytes(original,J::object({{"topology",J::object()}}));auto m=Monstey_WorldMeshApplyBytes(t.first,J::object({{"updates",J::array({J::object({{"group_index",1},{"draw_index",0},{"translation",J::array({1,0,0})}})})}})).first;auto proof=t.second;proof["output_sha256"]=Monstey_HashSha256(m);const auto r=Monstey_WorldStaticRenderBuild(m,render,proof,"worlds/source.wldmsh","worlds/private/test.wldmsh",false);Reject([&]{Monstey_WorldStaticPairValidate(original,render,m,r.first,proof,"worlds/source.wldmsh","worlds/private/test.wldmsh",false);});});
 Test("source geometry contexts cannot be silently dropped",[&]{auto original=mesh;original.insert(original.end(),mesh.begin()+35,mesh.end());for(auto word:{2u,3u,4u,5u,6u})Monstey_PutU32(original,4*word,Monstey_U32(mesh,4*word)*2);auto proof=authored.second;proof["source_sha256"]=Monstey_HashSha256(original);Reject([&]{Monstey_WorldStaticPairValidate(original,render,authored.first,built.first,proof,"worlds/source.wldmsh","worlds/private/test.wldmsh",false);});});
 Test("candidate byte truncation bounded",[&]{auto r=built.first;r.pop_back();Reject([&]{validate(authored.first,r,authored.second);});});
 const auto pair=J::object({{"mesh_source",md},{"render_source",rd},{"mesh_candidate",cm},{"render_candidate",cr},{"topology",pd},
  {"source_resource","worlds/source.wldmsh"},{"mesh_resource_path","worlds/private/test.wldmsh"},
  {"render_resource_path","worlds/private/test.wldclnt"},{"render_mode","private"},
  {"dependencies",J::array({"materials/m7.mat","materials/new.mat"})}});
 Monstey_LtmiMaterial material;material.shader_name=B{'s','y','n','t','h',0};const auto mat=Monstey_LtmiBuild(material);
 const auto matd=put("synthetic.mat",mat);
 auto dependency=[&](const char*path){return J::object({{"backend","DEPENDENCY"},{"resource_path",path},{"candidate",matd},{"dependency_kind","MATERIAL"}});};
 const auto obj=Monstey_StaticFixture::ObjectFixture();
 auto physics=obj;const auto physical=Monstey_PhysicsFixture::Fixture({Monstey_PhysicsFixture::Box()});
 Monstey_PutU32(physics,12,Monstey_U32(physics,12)+1);Monstey_AppendU32(physics,0xb9e81cf3u);Monstey_AppendU32(physics,static_cast<std::uint32_t>(physical.size()));physics.insert(physics.end(),physical.begin(),physical.end());
 const auto physd=put("collision.objres",physics),navd=put("nav.hkai",Monstey_NavFixture::Fixture()),ownerd=put("nav.objres",obj);
 const auto fxsrc=put("donor.fxp",Monstey_LightFixture::Fixture());
 const auto fxbuild=Monstey_WorldLightingCall("sow_world_light_build",J::object({{"source",fxsrc.at("path")},
  {"output",P(out/"light-recipe")},{"spec",J::object({{"source_sha256",fxsrc.at("sha256")},{"resource_path","fx/monstey/static.fxp"},
  {"radius_cm",400},{"color_rgb",J::array({1,0,0})}})}}));
 const auto fxd=fxbuild.at("resources")[0];
 const auto lightowners=Monstey_ObjresApply(Monstey_StaticFixture::ObjectFixture("SpecialFX"),J::object({{"objects",J::array({J::object({{"class_name","SpecialFX"},{"name","StaticLight"},
  {"position",J::array({0,0,0})},{"quaternion",J::array({0,0,0,1})},
  {"properties",J::object({{"FXName","fx\\monstey\\static.fxp"},{"Loop",true},{"StartOn",true}})},
  {"property_types",J::object({{"FXName",4},{"Loop",1},{"StartOn",1}})}})})}})).first;
 const auto lod=put("lights.objres",lightowners);
 J::Array generated{dependency("materials/m7.mat"),dependency("materials/new.mat"),
  J::object({{"backend","PHYSICS"},{"resource_path","worlds/collision.objres"},{"candidate",physd}}),
  J::object({{"backend","NAVIGATION"},{"resource_path","worlds/nav.hkai"},{"candidate",navd},{"owner_resource_path","worlds/nav.objres"},{"owner_candidate",ownerd}}),
  J::object({{"backend","LIGHT"},{"resource_path","fx/monstey/static.fxp"},{"candidate",fxd},{"owner_resource_path","worlds/lights.objres"},{"owner_candidate",lod},{"owner_name","StaticLight"}})};
 J::Array resources{"worlds/private/test.wldmsh","worlds/private/test.wldclnt","materials/m7.mat","materials/new.mat","worlds/collision.objres","worlds/nav.hkai","worlds/nav.objres","fx/monstey/static.fxp","worlds/lights.objres"};
 const B opaque{7,8,9,0,0xfe};
 const auto bd=put("source.bndl",Monstey_BuildBundle({"Bundles/Types:WorldQuest",{{"worlds/opaque.bin",static_cast<std::uint32_t>(opaque.size())}}}));
 const auto ed=put("source.embb",Monstey_BuildEmbedded({{"worlds/opaque.bin",opaque}}));
 const auto bundle=J::object({{"resource_path","worlds/private/test.bndl"},{"source",bd},{"resources",resources},{"additions",resources},
  {"embb_resource_path","worlds/private/test.embb"},{"embb_source",ed}});
 const auto plan=J::object({{"schema","Monstey_WorldEdit_1"},{"static_pairs",J::array({pair})},{"generated_resources",generated},{"bundles",J::array({bundle})}});
 auto run=[&](const J&p){return Monstey_WorldEditCall("sow_worldedit_compile",J::object({{"plan",p}}));};
 Test("physics nav light materials and static pair staged atomically",[&]{const auto r=run(plan);Check(r.at("resources").size()==11&&r.at("component_evidence").size()==8,"Resource or evidence extent differs");Check(!r.at("runtime_verified").as_bool(),"Runtime incorrectly inferred");Monstey_WriteNewText(out/"synthetic-validation.json",r.dump(2));});
 Test("per-component evidence never infers spatial compatibility",[&]{const auto r=run(plan);for(const auto &e:r.at("component_evidence").as_array()){Check(e.contains("format_valid")&&e.contains("owner_binding_valid")&&e.at("package_residency_valid").as_bool(),"Required evidence split absent");Check(e.at("geometry_alignment").at("status").as_string()=="UNVERIFIED"&&e.at("geometry_alignment").at("proof").is_null()&&!e.at("runtime_verified").as_bool(),"Spatial/runtime qualification inferred");if(e.at("component").as_string()=="PHYSICS")Check(e.at("owner_binding_valid").is_null(),"Unverified Physics mapping promoted to owner proof");}});
 Test("native texture dependency uses qualified resource codec",[&]{B dds(136);for(const auto &[at,v]:std::vector<std::pair<std::size_t,std::uint32_t>>{{0,0x20534444u},{4,124},{8,0x100fu},{12,1},{16,2},{20,8},{76,32},{80,0x41},{88,32},{92,0x00ff0000},{96,0x0000ff00},{100,0x000000ff},{104,0xff000000},{108,0x1000}})Monstey_PutU32(dds,at,v);auto p=plan;p["generated_resources"].as_array().push_back(J::object({{"backend","DEPENDENCY"},{"dependency_kind","TEXTURE"},{"resource_path","textures/test.dds"},{"candidate",put("test.dds",dds)}}));p["generated_resources"][0]["requires"]=J::array({"textures/test.dds"});p["bundles"][0]["resources"].as_array().push_back("textures/test.dds");p["bundles"][0]["additions"].as_array().push_back("textures/test.dds");const auto r=run(p);bool found=false;for(const auto &e:r.at("component_evidence").as_array())if(e.contains("resource_path")&&e.at("resource_path").as_string()=="textures/test.dds"){found=true;Check(e.at("offline_result").at("layout_qualified").as_bool()&&e.at("format_valid").as_bool(),"Texture ranges not qualified");}Check(found,"Texture evidence absent");});
 Test("missing active navigation THSH rejected",[&]{const auto bytes=Monstey_NavFixture::Fixture(true);Monstey_Havok h(std::span<const std::uint8_t>(bytes).subspan(16,bytes.size()-20));Check(h.TypeHashCoverage().at("status").as_string()=="FAIL","Missing hash fixture is not active");auto p=plan;p["generated_resources"][3]["candidate"]=put("missing-thsh.hkai",bytes);Reject([&]{run(p);});});
 Test("component completion manifest and paired residency readback",[&]{const auto r=Monstey_WorldEditCall("sow_worldedit_compile",J::object({{"plan",plan},{"output",P(out/"component")}}));
  Check(r.at("ready_for_component_integration").as_bool(),"Offline integration unexpectedly pending");
  const auto eb=Monstey_ReadEmbedded(Monstey_ReadFile(out/"component/assets/worlds/private/test.embb"));
  Check(eb.size()==10,"Embedded children lost");const auto it=std::find_if(eb.begin(),eb.end(),[](const auto&x){return x.resource=="worlds/opaque.bin";});Check(it!=eb.end()&&it->bytes==opaque,"Opaque unrelated child changed");});
 Test("existing topology build report accepted without hand rewriting",[&]{auto p=plan;p["static_pairs"][0]["topology"]=put("wrapped-topology.json",Text(J::object({{"topology",authored.second},{"render",built.second}})));Check(run(p).at("resources").size()==11,"Wrapped topology report failed");});
 Test("static stage with absent gameplay candidates reports each omission",[&]{auto p=plan;p["generated_resources"]=J::array({generated[0],generated[1]});p["bundles"][0]["resources"]=J::array({resources[0],resources[1],resources[2],resources[3]});p["bundles"][0]["additions"]=p["bundles"][0]["resources"];const auto r=run(p);unsigned absent=0;for(const auto &e:r.at("component_evidence").as_array())if(e.at("native_format").as_string()=="NOT_PROVIDED")++absent;Check(absent==3,"Missing physics/nav/light were not explicit");});
 Test("missing explicit material dependency rejected before publication",[&]{auto p=plan;p["static_pairs"][0]["dependencies"]=J::array({"materials/m7.mat"});Reject([&]{run(p);});});
 Test("material cannot be opaque bytes",[&]{auto p=plan;p["generated_resources"][0]["dependency_kind"]="OPAQUE";Reject([&]{run(p);});});
 Test("unresolved declared dependency rejected",[&]{auto p=plan;p["generated_resources"][0]["requires"]=J::array({"textures/missing.tex"});Reject([&]{run(p);});});
 Test("BNDL without paired EMBB rejected for static stage",[&]{auto p=plan;p["bundles"][0].as_object().erase("embb_source");p["bundles"][0].as_object().erase("embb_resource_path");Reject([&]{run(p);});});
 Test("unstaged residency child rejected",[&]{auto p=plan;p["bundles"][0]["resources"].as_array().push_back("worlds/missing.bin");Reject([&]{run(p);});});
 Test("generated resource checksum mismatch rejects entire transaction",[&]{auto p=plan;p["generated_resources"][2]["candidate"]["sha256"]=std::string(64,'0');Reject([&]{run(p);});});
 Test("legacy normalized plans cannot silently discard new components",[&]{auto p=plan;p["schema"]="Monstey_WorldBlenderEdits_1";Reject([&]{run(p);});});
 Test("irrelevant owner fields rejected rather than ignored",[&]{auto p=plan;p["generated_resources"][2]["owner_candidate"]=ownerd;p["generated_resources"][2]["owner_resource_path"]="worlds/nav.objres";Reject([&]{run(p);});});
 Test("declared owner source checksum is enforced",[&]{auto p=plan;p["generated_resources"][3]["owner_source"]=ownerd;p["generated_resources"][3]["owner_source"]["sha256"]=std::string(64,'0');Reject([&]{run(p);});});
 Test("same-stem navigation owner enforced",[&]{auto p=plan;p["generated_resources"][3]["owner_resource_path"]="worlds/other.objres";Reject([&]{run(p);});});
 Test("light owner start flag enforced",[&]{const auto bad=Monstey_ObjresApply(lightowners,J::object({{"updates",J::array({J::object({{"name","StaticLight"},{"properties",J::object({{"StartOn",false}})}})})}})).first;auto p=plan;p["generated_resources"][4]["owner_candidate"]=put("bad-light-owner.objres",bad);Reject([&]{run(p);});});
 Test("conflicting shared owner bytes rejected",[&]{auto p=plan;p["generated_resources"][4]["owner_resource_path"]="worlds/nav.objres";Reject([&]{run(p);});});
 Test("invalid generated collision rolls back before any component publication",[&]{auto bad=physics;bad.pop_back();auto p=plan;p["generated_resources"][2]["candidate"]=put("bad-collision.objres",bad);const auto target=out/"failed-component";Reject([&]{Monstey_WorldEditCall("sow_worldedit_compile",J::object({{"plan",p},{"output",P(target)}}));});Check(!std::filesystem::exists(target),"Failed validation published output");});
 Test("existing completed component cannot be overwritten",[&]{const auto hash=Monstey_HashSha256(out/"component/resource-manifest.json");Reject([&]{Monstey_WorldEditCall("sow_worldedit_compile",J::object({{"plan",plan},{"output",P(out/"component")}}));});Check(Monstey_HashSha256(out/"component/resource-manifest.json")==hash,"Existing component changed");});
 Test("original source and opaque bytes remain unchanged",[&]{Check(Monstey_ReadFile(Monstey_Wide(md.at("path").as_string()))==mesh&&Monstey_ReadFile(Monstey_Wide(rd.at("path").as_string()))==render,"Original source changed");Check(Monstey_ObjresReadChunk(physics,77)==B({1,2,3,4,5}),"Unknown OBJRES chunk changed");});
 if(argc==4){const auto am=Monstey_ReadFile(argv[2]),ar=Monstey_ReadFile(argv[3]);
  const auto identity=Monstey_WorldMeshApplyBytes(am,J::object({{"topology",J::object()}}));
  const auto rebuilt=Monstey_WorldStaticRenderBuild(identity.first,ar,identity.second,"worlds/retail/snowmtn/snowmtn.wldmsh","worlds/retail/snowmtn/snowmtn.wldmsh",true);
  Test("effective mounted DLC4 pair preserve-section replay",[&]{const auto r=Monstey_WorldStaticPairValidate(am,ar,identity.first,rebuilt.first,identity.second,"worlds/retail/snowmtn/snowmtn.wldmsh","worlds/retail/snowmtn/snowmtn.wldmsh",true);Check(r.at("native_scene_preserved_exact").as_bool(),"Actual native scene changed");Monstey_WriteNewText(out/"actual-pair-validation.json",r.dump(2));});
  Test("effective mounted arbitrary private topology replay",[&]{auto spec2=J::object({{"topology",J::object({{"groups",J::array({J::object({{"group_index",0},{"draws",J::array({J::object({{"source_draw",0}}),J::object({{"source_draw",0}})})}})})}})}});const auto t=Monstey_WorldMeshApplyBytes(am,spec2);const auto r=Monstey_WorldStaticRenderBuild(t.first,ar,t.second,"worlds/retail/snowmtn/snowmtn.wldmsh","worlds/private/mounted-test.wldmsh",false);const auto v=Monstey_WorldStaticPairValidate(am,ar,t.first,r.first,t.second,"worlds/retail/snowmtn/snowmtn.wldmsh","worlds/private/mounted-test.wldmsh",false);Monstey_WriteNewFile(out/"actual-private.wldmsh",t.first);Monstey_WriteNewFile(out/"actual-private.wldclnt",r.first);Monstey_WriteNewText(out/"actual-private-validation.json",v.dump(2));});
  Test("effective mounted source hashes unchanged",[&]{Check(Monstey_HashSha256(std::filesystem::path(argv[2]))==Monstey_HashSha256(am)&&Monstey_HashSha256(std::filesystem::path(argv[3]))==Monstey_HashSha256(ar),"Actual sources changed");});
 }
 const auto result=J::object({{"status","PASS"},{"groups",passed},{"cases",cases},{"runtime_verified",false},{"game_written",false}});Monstey_WriteNewText(out/"result.json",result.dump(2));std::cout<<result.dump(2)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
