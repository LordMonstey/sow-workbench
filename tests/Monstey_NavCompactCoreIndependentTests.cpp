#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <xmmintrin.h>
namespace {
using namespace Monstey;using H=Monstey_NavHierarchy;using B=Monstey_NavigationAabb;
std::uint64_t cases=0;
void Check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
float Add(float a,float b){volatile float r=a+b;return r;}
float Sub(float a,float b){volatile float r=a-b;return r;}
float Mul(float a,float b){volatile float r=a*b;return r;}
float Div(float a,float b){volatile float r=a/b;return r;}
float Min(float a,float b){return a<b?a:b;}
float Max(float a,float b){return a>b?a:b;}
B Empty(){const auto q=std::bit_cast<float>(0x7f7fffeeu);return {{q,q,q,q},{-q,-q,-q,-q}};}
B Join(const B&a,const B&b){B c{};for(unsigned n=0;n<4;++n){c.minimum[n]=Min(a.minimum[n],b.minimum[n]);c.maximum[n]=Max(a.maximum[n],b.maximum[n]);}return c;}
float Area(const B&a){std::array<float,3>d{};for(unsigned n=0;n<3;++n)d[n]=Sub(a.maximum[n],a.minimum[n]);return Add(Add(Mul(d[1],d[2]),Mul(d[0],d[1])),Mul(d[2],d[0]));}
float Center(const B&a,unsigned axis){return Mul(Add(a.maximum[axis],a.minimum[axis]),.5f);}
void SortIds(std::vector<std::uint32_t>&ids,std::ptrdiff_t lo,std::ptrdiff_t hi,unsigned axis,const H&h){
 if(lo>=hi)return;const float key=Center(h.nodes[ids[static_cast<std::size_t>((lo+hi)/2)]].bounds,axis);auto i=lo,j=hi;
 while(i<=j){while(Center(h.nodes[ids[static_cast<std::size_t>(i)]].bounds,axis)<key)++i;while(Center(h.nodes[ids[static_cast<std::size_t>(j)]].bounds,axis)>key)--j;if(i<=j){std::swap(ids[static_cast<std::size_t>(i)],ids[static_cast<std::size_t>(j)]);++i;--j;}}
 if(lo<j)SortIds(ids,lo,j,axis,h);if(i<hi)SortIds(ids,i,hi,axis,h);
}
H Make(std::span<const B> boxes,bool skew=false){
 H h;h.nodes.emplace_back();for(std::size_t n=0;n<boxes.size();++n)h.nodes.push_back({boxes[n],0,0,static_cast<std::uint32_t>(n)});h.leaf_count=static_cast<std::uint32_t>(boxes.size());
 if(boxes.empty())return h;
 std::function<std::uint32_t(std::size_t,std::size_t)> build=[&](std::size_t lo,std::size_t hi)->std::uint32_t{
  if(hi-lo==1)return static_cast<std::uint32_t>(lo+1);
  const auto middle=skew?lo+1:(lo+(hi-lo)/2);const auto a=build(lo,middle),b=build(middle,hi);const auto id=static_cast<std::uint32_t>(h.nodes.size());
  h.nodes.push_back({Join(h.nodes[a].bounds,h.nodes[b].bounds),0,a,b});h.nodes[a].parent=id;h.nodes[b].parent=id;return id;
 };h.root=build(0,boxes.size());return h;
}
struct Oracle {
 H h;std::vector<std::uint32_t>free;
 std::uint32_t Alloc(){Check(!free.empty(),"Oracle free-list empty");auto id=free.back();free.pop_back();return id;}
 std::uint32_t Rebuild(std::vector<std::uint32_t>ids,bool initial){
  if(ids.size()<=16&&!initial){
   while(ids.size()>1){float best=std::bit_cast<float>(0x7f7fffeeu);std::size_t ai=0,bi=0;B bounds{};bool found=false;
    for(std::size_t a=0;a<ids.size();++a)for(std::size_t b=a+1;b<ids.size();++b){const auto u=Join(h.nodes[ids[b]].bounds,h.nodes[ids[a]].bounds);const auto score=Area(u);if(score<best){found=true;best=score;ai=a;bi=b;bounds=u;}}
    Check(found,"Oracle no merge");const auto a=ids[ai],b=ids[bi],p=Alloc();h.nodes[p]={bounds,0,a,b};h.nodes[a].parent=p;h.nodes[b].parent=p;
    ids[bi]=ids.back();ids.pop_back();ids[ai]=p;
   }return ids[0];
  }
  B box=Empty();for(auto id:ids)box=Join(box,h.nodes[id].bounds);
  const auto p=Alloc();h.nodes[p].bounds=box;
  std::array<std::array<std::vector<std::uint32_t>,32>,3> groups;
  std::array<std::array<B,32>,3>bounds;
  for(auto&axis:bounds)axis.fill(Empty());
  for(auto id:ids)for(unsigned axis=0;axis<3;++axis){
   const auto extent=Sub(box.maximum[axis],box.minimum[axis]);const auto reciprocal=extent==0?0:Div(1,extent);const auto scale=Mul(reciprocal,32);
   auto value=Add(Mul(Sub(Center(h.nodes[id].bounds,axis),box.minimum[axis]),scale),.5f);value=Min(Max(value,0),31);
   const auto at=static_cast<std::size_t>(value);groups[axis][at].push_back(id);bounds[axis][at]=Join(bounds[axis][at],h.nodes[id].bounds);
  }
  // Independent O(3*32^2) closed prefix/suffix expression, rather than sharing
  // the production two-ended update loop or its aliased cost accumulators.
  float best=std::bit_cast<float>(0x7f7fffeeu);unsigned chosenAxis=0,chosenBin=0;bool found=false;
  for(unsigned axis=0;axis<3;++axis)for(unsigned at=0;at<32;++at){
   if(groups[axis][at].empty())continue;B left=Empty(),right=Empty();std::uint32_t lc=0,rc=0;
   for(unsigned q=0;q<=at;++q){left=Join(left,bounds[axis][q]);lc+=static_cast<std::uint32_t>(groups[axis][q].size());}
   for(unsigned q=32;q-->at;){right=Join(right,bounds[axis][q]);rc+=static_cast<std::uint32_t>(groups[axis][q].size());}
   const auto score=Add(Mul(Area(left),static_cast<float>(lc)),Mul(Area(right),static_cast<float>(rc)));
   if(!(score>=best)){found=true;best=score;chosenAxis=axis;chosenBin=at;}
  }
  Check(found,"Oracle no bin");std::vector<std::uint32_t>left,right;
  for(unsigned q=0;q<32;++q)if(q<chosenBin)left.insert(left.end(),groups[chosenAxis][q].begin(),groups[chosenAxis][q].end());else if(q>chosenBin)right.insert(right.end(),groups[chosenAxis][q].begin(),groups[chosenAxis][q].end());
  auto middle=groups[chosenAxis][chosenBin];
  if(left.empty()&&!right.empty())left.insert(left.end(),middle.begin(),middle.end());
  else if(right.empty()&&!left.empty())right.insert(right.end(),middle.begin(),middle.end());
  else{SortIds(middle,0,static_cast<std::ptrdiff_t>(middle.size())-1,chosenAxis,h);const auto split=middle.begin()+static_cast<std::ptrdiff_t>(middle.size()/2);left.insert(left.end(),middle.begin(),split);right.insert(right.end(),split,middle.end());}
  Check(!left.empty()&&!right.empty(),"Oracle empty partition");
  const auto r=Rebuild(std::move(right),false);const auto l=Rebuild(std::move(left),false);
  h.nodes[p].child0=l;h.nodes[p].child1_or_payload=r;h.nodes[l].parent=p;h.nodes[r].parent=p;return p;
 }
 H Run(){if(h.root==0||h.leaf_count<=2)return h;std::vector<std::uint32_t>leaves;
  std::function<void(std::uint32_t)>visit=[&](auto id){if(!h.nodes[id].child0){leaves.push_back(id);return;}free.push_back(id);visit(h.nodes[id].child0);visit(h.nodes[id].child1_or_payload);};visit(h.root);
  h.root=Rebuild(std::move(leaves),true);h.nodes[h.root].parent=0;Check(free.empty(),"Oracle unused free ID");return h;}
};
bool Equal(const H&a,const H&b){if(a.root!=b.root||a.leaf_count!=b.leaf_count||a.nodes.size()!=b.nodes.size())return false;
 for(std::size_t n=0;n<a.nodes.size();++n){const auto&x=a.nodes[n];const auto&y=b.nodes[n];if(x.parent!=y.parent||x.child0!=y.child0||x.child1_or_payload!=y.child1_or_payload)return false;for(unsigned q=0;q<4;++q)if(std::bit_cast<std::uint32_t>(x.bounds.minimum[q])!=std::bit_cast<std::uint32_t>(y.bounds.minimum[q])||std::bit_cast<std::uint32_t>(x.bounds.maximum[q])!=std::bit_cast<std::uint32_t>(y.bounds.maximum[q]))return false;}return true;
}
void Compare(const H&input){const auto want=Oracle{input,{}}.Run();const auto got=Monstey_NavOptimizeHierarchyCore(input);Check(Equal(got,want),"Independent core topology/bounds/ID oracle differs");++cases;}
B Box(float x,float y,float z,float extent){return {{x,y,z,0},{Add(x,extent),Add(y,extent),Add(z,extent),0}};}
}
int wmain(int argc,wchar_t**argv){try{using namespace Monstey;Check(argc==2,"Need NEW report");unsigned groups=0;
 Compare(Make({}));for(unsigned n=1;n<3;++n){std::vector<B>b(n,Box(0,0,0,1));Compare(Make(b));}++groups;
 for(unsigned n:std::array<unsigned,8>{3,4,5,16,17,31,32,65})for(bool skew:{false,true}){std::vector<B>b(n,Box(0,0,0,1));Compare(Make(b,skew));}++groups;
 for(unsigned axis=0;axis<3;++axis)for(unsigned n:std::array<unsigned,5>{3,16,17,32,64}){std::vector<B>b;for(unsigned k=0;k<n;++k){auto q=Box(0,0,0,.5f);q.minimum[axis]=static_cast<float>(k);q.maximum[axis]=Add(q.minimum[axis],.5f);b.push_back(q);}Compare(Make(b));std::reverse(b.begin(),b.end());Compare(Make(b,true));}++groups;
 for(unsigned n:std::array<unsigned,4>{3,4,17,33}){std::vector<B>b;for(unsigned k=0;k<n;++k)b.push_back(Box(k%3==0?31.0f:0.0f,k%2?15.49f:15.51f,static_cast<float>(k%5),1));Compare(Make(b));}++groups;
 for(float e:std::array<float,5>{0,.125f,1,16,31}){std::vector<B>b;for(unsigned k=0;k<35;++k)b.push_back(Box(static_cast<float>(k%7),static_cast<float>(k/7),0,e));Compare(Make(b));}++groups;
 std::mt19937 rng(0xa74ab0u);
 for(unsigned trial=0;trial<768;++trial){const unsigned n=3+rng()%94;std::vector<B>b;for(unsigned k=0;k<n;++k){const float x=static_cast<float>(static_cast<int>(rng()%2049)-1024)*.125f,y=static_cast<float>(static_cast<int>(rng()%1025)-512)*.25f,z=static_cast<float>(rng()%17)*.125f,e=static_cast<float>(1+rng()%16)*.125f;b.push_back(Box(x,y,z,e));}Compare(Make(b,trial%2!=0));}++groups;
 for(unsigned trial=0;trial<16;++trial){std::vector<B>b;for(unsigned k=0;k<33;++k){auto q=Box(static_cast<float>(k%7),static_cast<float>(k/7),0,.5f);q.minimum[3]=k%3==0?std::bit_cast<float>(0x7fc00000u|k):((k%2)?-0.0f:static_cast<float>(trial));q.maximum[3]=k%5==0?std::bit_cast<float>(0x7fc01000u|k):static_cast<float>(k);b.push_back(q);}Compare(Make(b,trial%2!=0));}++groups;
 const auto originalCsr=_mm_getcsr();for(unsigned mode=0;mode<16;++mode){_mm_setcsr((originalCsr&~0xe07fu)|((mode%4u)<<13)|((mode&4u)?0x8000u:0u)|((mode&8u)?0x40u:0u));std::vector<B>b;for(unsigned k=0;k<37;++k)b.push_back(Box(static_cast<float>(k%11)*.1f,static_cast<float>(k/11)*.3f,static_cast<float>(k%3)*.7f,.15f));Compare(Make(b));}_mm_setcsr(originalCsr);++groups;
 auto malformed=Make(std::vector<B>(5,Box(0,0,0,1)));auto expect=[&](H bad){bool failed=false;try{(void)Monstey_NavOptimizeHierarchyCore(std::move(bad));}catch(const std::exception&){failed=true;}Check(failed,"Owned malformed hierarchy not rejected");++cases;};
 auto bad=malformed;bad.nodes[bad.root].parent=2;expect(bad);bad=malformed;bad.nodes[bad.root].child0=bad.root;expect(bad);bad=malformed;bad.nodes[bad.root].child1_or_payload=9999;expect(bad);bad=malformed;++bad.leaf_count;expect(bad);bad=malformed;bad.nodes[1].bounds.minimum[0]=100;expect(bad);++groups;
 Monstey_WriteNewText(argv[1],Monstey_Json::object({{"schema","Monstey.CompactCoreIndependentTests/1"},{"pass",true},{"groups",groups},{"cases",cases},{"random_cases",768},{"original_functions_executed",false}}).dump(2)+"\n");std::cout<<"PASS "<<groups<<" groups "<<cases<<" exact core cases\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
