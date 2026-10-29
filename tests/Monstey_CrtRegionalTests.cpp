#include "Monstey_CrtRegionalFixtures.hpp"
#include <iostream>
#include <string_view>
int main(int argc,char** argv) {
 using namespace Monstey_Independent;
 const bool record=argc==2&&std::string_view(argv[1])=="--expect-v1-gap";
 std::uint32_t cases=0,gaps=0,checks=0;
 try {
  for(const auto& c:Monstey_Cases) {
   Monstey_Fixture full(c);const auto all=full.Monstey_Regions(c,0);
   if(full.Monstey_Run(c,all)!=c.expected)throw std::runtime_error("contiguous mismatch "+c.name);
   ++checks;
   for(Monstey_U32 split=1;split<c.width;++split) {
    Monstey_Fixture fragmented(c);const auto regions=fragmented.Monstey_Regions(c,split);
    bool fault=false;Monstey_U32 at=0,width=0;Monstey_I32 result=0;
    try {result=fragmented.Monstey_Run(c,regions);}
    catch(const Monstey::Monstey_CrtFault& f){fault=true;at=f.address;width=f.width;}
    ++cases;
    if(record) {
     if(!fault||at!=c.address||width!=c.width)throw std::runtime_error("expected exact regional failure absent "+c.name);
     ++gaps;++checks;
     std::cout<<"{\"case\":\""<<c.name<<"\",\"split\":"<<split<<",\"expected\":"<<c.expected
       <<",\"observed\":\"fault\",\"address\":"<<at<<",\"width\":"<<width<<"}\n";
    } else {
     if(fault||result!=c.expected||fragmented.events!=full.events)
      throw std::runtime_error("adjacent-region equivalence failed "+c.name+" split="+std::to_string(split));
     ++checks;
    }
   }
  }
  std::cout<<"{\"status\":\""<<(record?"CONFIRMED_DISCREPANCY":"PASS")<<"\",\"cases\":"<<cases
   <<",\"checks\":"<<checks<<",\"confirmed_gaps\":"<<gaps<<"}\n";
  return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
