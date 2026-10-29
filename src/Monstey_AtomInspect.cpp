#include "Monstey_Gadb.hpp"
#include <charconv>
#include <iostream>
#include <limits>
namespace {
std::size_t Monstey_AtomPage(const wchar_t *arg){
 const auto s=Monstey::Monstey_Utf8(arg);std::uint64_t n=0;
 const auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),n);
 Monstey::Monstey_Require(e==std::errc()&&p==s.data()+s.size()&&n<=std::numeric_limits<std::size_t>::max(),"Invalid unsigned page argument");
 return static_cast<std::size_t>(n);
}
}
int wmain(int argc,wchar_t **argv){
 using namespace Monstey;
 try{
  if(argc<3||argc>5){std::cerr<<"Usage: Monstey_AtomInspect SOURCE SELECTOR [OFFSET [LIMIT]]\n";return 64;}
  const Monstey_Gadb db{std::filesystem::path(argv[1])};
  std::cout<<db.AtomUses(Monstey_Utf8(argv[2]),argc>=4?Monstey_AtomPage(argv[3]):0,
                              argc>=5?Monstey_AtomPage(argv[4]):250).dump(2)<<'\n';return 0;
 }catch(const Monstey_Error&e){std::cerr<<Monstey_Json::object({{"status","ERROR"},{"code",e.code},{"message",e.what()}}).dump()<<'\n';return 2;}
 catch(const std::exception&e){std::cerr<<Monstey_Json::object({{"status","ERROR"},{"message",e.what()}}).dump()<<'\n';return 2;}
}
