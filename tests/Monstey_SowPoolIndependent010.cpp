#include "Monstey_SowPoolDifferentialDriver008.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("Usage: independent NEW-result.json");
  if(std::filesystem::exists(argv[1]))throw std::runtime_error("Result already exists");
  const auto result=Monstey_SowPoolDifferential::Monstey_DifferentialCampaign();
  std::ostringstream json;json<<"{\"status\":\"SOW_INDEPENDENT_DIFFERENTIAL_PASS\",\"cases\":"<<result.cases<<",\"checks\":"<<result.checks
    <<",\"consumed_faults\":"<<result.faults<<",\"whole_arena_bytes_compared\":"<<result.comparedBytes<<",\"primitive_read_events\":"<<result.primitiveEvents
    <<",\"raw_predicate_cases\":"<<result.predicates<<",\"stable_view_profile\":true,\"reference_frozen_before_product_read\":true,\"original_executed\":false}";
  std::ofstream output(argv[1],std::ios::binary);output<<json.str()<<'\n';if(!output)throw std::runtime_error("Failed writing result");std::cout<<json.str()<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
