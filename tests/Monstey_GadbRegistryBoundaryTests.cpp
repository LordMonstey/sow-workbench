#include "Monstey_RegistryBoundaryFixtures.hpp"
int wmain(int argc,wchar_t** argv){try{
 Check(argc==2,"Need new result file");J::Array tests;
 auto test=[&](const char* name,const std::function<void()>& f){f();tests.push_back(name);};
 for(const unsigned count:{64u,65u,80u})test(("parent_order_independent_"+std::to_string(count)).c_str(),[&]{
   const auto a=Read(Chain(count,true)),b=Read(Chain(count,false));
   const auto accepted=std::min(count,64u);Check(a.at("categories").size()==accepted&&b.at("categories").size()==accepted,"Parent qualification depends on visit order");
   Check(Depth(a)==63&&Depth(b)==63,"Ancestor limit bypassed");Check(a.at("unknown").size()==count-accepted&&b.at("unknown").size()==count-accepted,"Valid ancestor poisoned by descendant budget");
 });
 auto reject=[&](const B& bytes){bool threw=false;try{(void)Read(bytes);}catch(const Monstey_Error&){threw=true;}Check(threw,"Malformed PE mapping was accepted");};
 test("overlapping_exec_alias_rejected",[&]{auto b=FieldPE();U(b,392+36,0xc0000040);Monstey_PutU16(b,134,2);const std::size_t h=432;std::memcpy(b.data()+h,".alias",6);for(const auto& [off,v]:std::vector<std::pair<std::size_t,std::uint32_t>>{{8,32},{12,0x1800},{16,32},{20,512+0x800},{36,0x20000020}})U(b,h+off,v);reject(b);});
 test("RVA_extent_wrap_rejected",[&]{auto b=PE();U(b,392+12,0xfffff000u);const auto high=base+0xfffff000ull;T(b,0x100,"High");Q(b,0x1810,high+0x100);Q(b,0x1820,high+0x100);Q(b,0x1838,8);reject(b);});
 test("ordinary_executable_callback_control_preserved",[]{const auto r=Read(FieldPE());Check(r.at("categories").size()==1&&r.at("categories").at("Test.Parent").at("fields")[0].at("native_index_callback_executable").as_bool(),"Ordinary callback control lost");});
 const auto result=J::object({{"status","PASS"},{"groups",tests.size()},{"tests",tests},{"scoped_review_findings_closed",2},{"original_executed",false}});
 Monstey_WriteNewText(argv[1],result.dump(2)+"\n");std::cout<<result.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
