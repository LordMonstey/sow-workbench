#include "Monstey_NavCompactSortCentroids.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <xmmintrin.h>
namespace {
using namespace Monstey;
std::uint64_t cases = 0;
void Check(bool condition,const char* what) { if(!condition)throw std::runtime_error(what); }
Monstey_NavHierarchyNode Node(float x,float y,float z,float w) {
 return {{{x,y,z,w},{x,y,z,w}},0x11223344u,0x55667788u,0x99aabbccu};
}
float Key(const Monstey_NavHierarchyNode& node,std::uint32_t axis) {
 // Independent scalar axis projection. No production intrinsics, stack or masks.
 volatile float sum = node.bounds.maximum[axis]+node.bounds.minimum[axis];
 volatile float center = sum*0.5f;
 return center;
}
void Reference(std::vector<std::uint32_t>& a,std::ptrdiff_t lo,std::ptrdiff_t hi,
               std::uint32_t axis,const std::vector<Monstey_NavHierarchyNode>& nodes) {
 if(lo>=hi)return;
 const auto pivot=Key(nodes[a[static_cast<std::size_t>((lo+hi)/2)]],axis);
 auto i=lo,j=hi;
 while(i<=j) {
  while(Key(nodes[a[static_cast<std::size_t>(i)]],axis)<pivot)++i;
  while(Key(nodes[a[static_cast<std::size_t>(j)]],axis)>pivot)--j;
  if(i<=j){std::swap(a[static_cast<std::size_t>(i)],a[static_cast<std::size_t>(j)]);++i;--j;}
 }
 // Deliberately left-first recursive scheduling, independent of the production
 // smaller-first stack. Disjoint partitions commute; tie permutations agree.
 if(lo<j)Reference(a,lo,j,axis,nodes);
 if(i<hi)Reference(a,i,hi,axis,nodes);
}
void Compare(std::vector<std::uint32_t> a,std::int32_t count,std::uint32_t axis,
             const std::vector<Monstey_NavHierarchyNode>& nodes) {
 auto expected=a;const auto before=nodes;
 if(count>1)Reference(expected,0,static_cast<std::ptrdiff_t>(count)-1,axis,nodes);
 Monstey_NavCompactSortCentroids(a,count,axis,nodes);
 Check(a==expected,"Exact scalar-reference permutation differs");
 Check(nodes.size()==before.size() && std::equal(nodes.begin(),nodes.end(),before.begin(),[](const auto& x,const auto& y){
  return x.parent==y.parent&&x.child0==y.child0&&x.child1_or_payload==y.child1_or_payload&&
    std::equal(x.bounds.minimum.begin(),x.bounds.minimum.end(),y.bounds.minimum.begin(),[](float u,float v){return std::bit_cast<std::uint32_t>(u)==std::bit_cast<std::uint32_t>(v);})&&
    std::equal(x.bounds.maximum.begin(),x.bounds.maximum.end(),y.bounds.maximum.begin(),[](float u,float v){return std::bit_cast<std::uint32_t>(u)==std::bit_cast<std::uint32_t>(v);});}),"Hierarchy modified");
 ++cases;
}
void Expected(std::vector<std::uint32_t> a,const std::vector<std::uint32_t>& expected,
              const std::vector<Monstey_NavHierarchyNode>& nodes,std::uint32_t axis=0) {
 Monstey_NavCompactSortCentroids(a,static_cast<std::int32_t>(a.size()),axis,nodes);
 Check(a==expected,"Analytic expected permutation differs");++cases;
}
void ExpectError(std::vector<std::uint32_t> a,std::int32_t count,std::uint32_t axis,
                 const std::vector<Monstey_NavHierarchyNode>& nodes) {
 const auto before=a; bool caught=false;
 try{Monstey_NavCompactSortCentroids(a,count,axis,nodes);}catch(const std::exception&){caught=true;}
 Check(caught&&a==before,"Owned-range error missing or partial mutation");++cases;
}
}
int wmain(int argc,wchar_t**argv){try{using namespace Monstey;
 Check(argc==2,"Need NEW report path");
 std::uint32_t groups=0;
 // No-reads/no-op preserves native signed early-return semantics, including an
 // empty owned prefix with count1 and invalid axis/indices.
 for(auto count:std::array<std::int32_t,5>{INT32_MIN,-1,0,1,INT32_MIN+1}) {
  std::vector<std::uint32_t>a{0xffffffffu};Monstey_NavCompactSortCentroids(a,count,99,{});
  Check(a[0]==0xffffffffu,"No-op altered ID");Monstey_NavCompactSortCentroids({},count,99,{});++cases;
 }++groups;
 std::vector<Monstey_NavHierarchyNode> distinct{Node(3,0,2,1),Node(0,3,1,2),Node(2,1,0,3),Node(1,2,3,0)};
 Expected({0,1,2,3},{1,3,2,0},distinct,0);Expected({0,1,2,3},{0,2,3,1},distinct,1);
 Expected({0,1,2,3},{2,1,0,3},distinct,2);Expected({0,1,2,3},{3,0,1,2},distinct,3);++groups;
 const std::vector<Monstey_NavHierarchyNode> equal(5,Node(2,2,2,2));
 Expected({0,1,2,3},{2,3,0,1},equal);Expected({0,1,2,3,4},{3,4,2,0,1},equal);++groups;
 Compare({3,2,1,0,0xffffffffu},4,0,distinct);Compare({0,3,1,2,0xffffffffu},2,1,distinct);++groups;
 // Explicit duplicate IDs and equal values: sorting is neither stable nor unique.
 Compare({3,1,3,2,1,0,1,3,2,0},10,2,distinct);++groups;
 for(std::uint32_t axis=0;axis<4;++axis){
  for(std::size_t n:std::array<std::size_t,8>{2,3,4,5,16,31,64,1024}) {
   std::vector<Monstey_NavHierarchyNode> nodes;std::vector<std::uint32_t>a(n);
   for(std::size_t k=0;k<n;++k)nodes.push_back(Node(static_cast<float>(k),static_cast<float>(k),static_cast<float>(k),static_cast<float>(k)));
   std::iota(a.begin(),a.end(),0u);Compare(a,static_cast<std::int32_t>(n),axis,nodes);
   std::reverse(a.begin(),a.end());Compare(a,static_cast<std::int32_t>(n),axis,nodes);
  }
 }++groups;
 std::mt19937 random(0x72ec0u);
 for(std::uint32_t n=0;n<4096;++n){
  const auto size=2u+random()%127u;std::vector<Monstey_NavHierarchyNode> nodes;std::vector<std::uint32_t>a(size);
  for(std::uint32_t k=0;k<size;++k)nodes.push_back(Node(static_cast<float>(static_cast<int>(random()%31u)-15),static_cast<float>(static_cast<int>(random()%31u)-15),static_cast<float>(static_cast<int>(random()%31u)-15),static_cast<float>(static_cast<int>(random()%31u)-15)));
  for(auto& id:a)id=random()%size;Compare(a,static_cast<std::int32_t>(size),n%4,nodes);
 }++groups;
 std::vector<Monstey_NavHierarchyNode> huge{Node(0,0,0,0),Node(0,0,0,0)};
 huge[0].bounds.minimum[0]=1.7e38f;huge[0].bounds.maximum[0]=1.8e38f;
 huge[1].bounds.minimum[0]=2e38f;huge[1].bounds.maximum[0]=3e38f;
 Expected({0,1},{1,0},huge);++groups;
 const float qnan=std::bit_cast<float>(0x7fc00001u),inf=std::numeric_limits<float>::infinity();
 std::vector<Monstey_NavHierarchyNode> special{Node(qnan,0,0,0),Node(1,0,0,0),Node(inf,0,0,0),Node(-inf,0,0,0),Node(-0.0f,0,0,0),Node(0,0,0,0)};
 for(auto a:std::array<std::vector<std::uint32_t>,3>{{{0,1,2,3,4,5},{5,4,3,2,1,0},{1,0,1,0,0,4}}})Compare(a,6,0,special);++groups;
 const auto original_csr=_mm_getcsr();
 for(std::uint32_t mode=0;mode<16;++mode){
  const auto csr=(original_csr & ~0xe07fu)|((mode%4u)<<13)|((mode&4u)?0x8000u:0u)|((mode&8u)?0x40u:0u);
  _mm_setcsr(csr);
  std::vector<Monstey_NavHierarchyNode> rounding{Node(1,0,0,0),Node(1,0,0,0),Node(std::bit_cast<float>(1u),0,0,0),Node(-std::bit_cast<float>(1u),0,0,0)};
  rounding[0].bounds.maximum[0]=std::nextafter(1.0f,2.0f);
  Compare({0,1,2,3},4,0,rounding);
 }
 _mm_setcsr(original_csr);++groups;
 ExpectError({0,1},3,0,distinct);ExpectError({1,0,999},3,0,distinct);ExpectError({0,1},2,4,distinct);ExpectError({1,0},2,0,{});++groups;
 Monstey_WriteNewText(argv[1],Monstey_Json::object({{"schema","Monstey.CentroidSortTests/1"},{"passed",true},{"groups",groups},{"cases",cases},{"original_code_executed",false},{"random_seed","0x72ec0"}}).dump(2)+"\n");
 std::cout<<"PASS "<<groups<<" groups "<<cases<<" cases\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
