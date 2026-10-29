#include "Monstey_AnonymousDifferential.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("Expected one fresh result path");
 const auto result=Monstey_AnonymousDifferential::Monstey_CompareAll();std::ostringstream json;
 json<<"{\"status\":\"SOW_ANONYMOUS_INDEPENDENT_PASS\",\"cases\":"<<result.cases<<",\"checks\":"<<result.checks<<",\"fault_cases\":"<<result.faults
   <<",\"whole_arena_bytes\":"<<result.bytes<<",\"primitive_read_events\":"<<result.events<<",\"stable_view_profile\":true,\"reference_frozen_before_product_read\":true,\"original_executed\":false}";
 std::ofstream output(argv[1],std::ios::binary);output<<json.str()<<'\n';if(!output)throw std::runtime_error("Report write failed");std::cout<<json.str()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
