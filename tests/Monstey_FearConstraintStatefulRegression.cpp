#define main Monstey_PreparedIndependentMain
#include "Monstey_FearConstraintIndependent.cpp"
#undef main
namespace {
void Monstey_AssertStateful(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Monstey_StatefulPosition() {
 const auto input=Monstey_Serialize({{{},3,0xDEADBEEF,0x12345678,0xFEDCBA98}});
 Monstey_Fixture expected({},input),actual({},input);auto es=expected.Services(),as=actual.Services();
 const auto install=[](Monstey_Fixture& fixture,Monstey_FearConstraintServices& services){
  services.get_position=[epoch=0u,length=fixture.input.size(),&fixture](Monstey_FearConstraintCall call) mutable -> std::uint64_t {
   const auto value=epoch++==0?std::uint64_t{0}:static_cast<std::uint64_t>(length);
   fixture.events.push_back({"stateful-position",call.target,call.object,value});return value;
  };
 };
 install(expected,es);install(actual,as);
 const auto want=Monstey_ReferenceConstraints(expected.view,Monstey_Frame,es),got=Monstey_FearReadConstraints(actual.view,Monstey_Frame,as);
 Monstey_AssertStateful(want&&expected.reads==6&&expected.position==17,"independent position fixture malformed");
 Monstey_AssertStateful(got==want&&actual.reads==expected.reads&&actual.events==expected.events&&actual.images==expected.images,"callback mutable GetPos state reset by copy");
}
void Monstey_StatefulMap() {
 const auto input=Monstey_Serialize(Monstey_Rows());
 Monstey_Fixture expected({},input),actual({},input);auto es=expected.Services(),as=actual.Services();
 const auto install=[](Monstey_Fixture& fixture,Monstey_FearConstraintServices& services){
  services.map_value=[ordinal=0u,&fixture](std::uint32_t map,std::uint32_t name) mutable {
   const auto address=Monstey_Value+12u*ordinal++;++fixture.maps;
   fixture.events.push_back({"stateful-map",map,name,address,0,fixture.current_key});fixture.keys.push_back(fixture.current_key);return address;
  };
 };
 install(expected,es);install(actual,as);
 const auto want=Monstey_ReferenceConstraints(expected.view,Monstey_Frame,es),got=Monstey_FearReadConstraints(actual.view,Monstey_Frame,as);
 Monstey_AssertStateful(want&&expected.maps==2,"independent map fixture malformed");
 Monstey_AssertStateful(got==want&&actual.events==expected.events&&actual.images==expected.images,"callback mutable map state reset by copy");
}
}
int main(int argc,char**argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report required");
  Monstey_StatefulPosition();Monstey_StatefulMap();
  std::ofstream out(argv[1]);out<<"{\"status\":\"PASS\",\"groups\":2,\"cases\":2,\"checks\":4}\n";
  std::cout<<"PASS two stateful supplied provider regressions\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}