#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_NavFinalizeHierarchy.hpp"
#include "Monstey_NavCompactCooker.hpp"
#include "Monstey_StaticNavFaceBounds.hpp"
#include <cstring>
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
H Preorder(const H&input){H out;out.nodes.push_back({});out.leaf_count=input.leaf_count;
 std::function<std::uint32_t(std::uint32_t,std::uint32_t)>walk=[&](auto id,auto parent){const auto index=static_cast<std::uint32_t>(out.nodes.size());const auto node=input.nodes[id];out.nodes.push_back({node.bounds,parent,0,node.child1_or_payload});if(node.child0){const auto a=walk(node.child0,index),b=walk(node.child1_or_payload,index);out.nodes[index].child0=a;out.nodes[index].child1_or_payload=b;}return index;};
 if(input.root)out.root=walk(input.root,0);return out;
}
B Decode(const B&parent,const std::array<std::uint8_t,4>&bytes){B out{};for(unsigned axis=0;axis<4;++axis){const auto scale=Mul(Sub(parent.maximum[axis],parent.minimum[axis]),std::bit_cast<float>(0x3b90fdbcu));const float hi=static_cast<float>(bytes[axis]>>4),lo=axis==3?0:static_cast<float>(bytes[axis]&15);out.minimum[axis]=Add(Mul(Mul(hi,hi),scale),parent.minimum[axis]);out.maximum[axis]=Sub(parent.maximum[axis],Mul(Mul(lo,lo),scale));}return out;}
std::array<std::uint8_t,3> Quantize(const B&parent,const B&child){std::array<std::uint8_t,3>out{};for(unsigned axis=0;axis<3;++axis){
 const auto scale=Mul(Sub(parent.maximum[axis],parent.minimum[axis]),std::bit_cast<float>(0x3b90fdbcu));unsigned high=0,low=0;
 // Independent scalar one-axis candidate evaluation, not the production full
 // four-lane decoder or its mutable byte array.
 for(unsigned n=1;n<=15;++n){const auto delta=Mul(Mul(static_cast<float>(n),static_cast<float>(n)),scale);if(!(child.minimum[axis]>=Add(delta,parent.minimum[axis])))break;high=n;}
 for(unsigned n=1;n<=15;++n){const auto delta=Mul(Mul(static_cast<float>(n),static_cast<float>(n)),scale);if(child.maximum[axis]>Sub(parent.maximum[axis],delta))break;low=n;}
 out[axis]=static_cast<std::uint8_t>((high<<4)|low);
 }return out;}
Monstey_NavCompactTreeOwned Pack(const H&h){Monstey_NavCompactTreeOwned out;
 if(!h.leaf_count){const auto big=std::bit_cast<float>(0x7f7fffeeu);out.bounds={{big,big,big,big},{-big,-big,-big,-big}};return out;}
 out.bounds=h.nodes[h.root].bounds;std::vector<B>decoded(h.nodes.size());decoded[0]=out.bounds;
 for(std::size_t n=1;n<h.nodes.size();++n){const auto&node=h.nodes[n];const auto q=Quantize(decoded[node.parent],node.bounds);auto payload=node.child1_or_payload&0x7fffffu;if(node.child0)payload=0x800000u|(((node.child1_or_payload-static_cast<std::uint32_t>(n))/2)&0x7fffffu);
  out.nodes.insert(out.nodes.end(),{q[0],q[1],q[2],static_cast<std::uint8_t>(payload>>16),static_cast<std::uint8_t>(payload),static_cast<std::uint8_t>(payload>>8)});
  // Native scratch byte3 is unwritten at its BFDDA0 call. Use a deliberately
  // different private byte here: only XYZ can influence future output bytes.
  decoded[n]=Decode(decoded[node.parent],{q[0],q[1],q[2],0xd3});
 }return out;
}

}
namespace Monstey {
Monstey_NavHierarchy Monstey_NavOptimizeHierarchyCoreBaseline(Monstey_NavHierarchy);
Monstey_NavHierarchy Monstey_NavFinalizeHierarchyBaseline(Monstey_NavHierarchy,bool);
Monstey_StaticNavFaceAabb Monstey_StaticNavFaceBoundsBaseline(std::span<const std::uint8_t>,std::span<const std::uint8_t>,std::span<const std::uint8_t>,std::uint32_t,const std::array<float,4>&);
}
namespace {
void Put32(std::vector<std::uint8_t>&b,std::size_t p,std::uint32_t v){for(unsigned n=0;n<4;++n)b[p+n]=static_cast<std::uint8_t>(v>>(8*n));}
void Put16(std::vector<std::uint8_t>&b,std::size_t p,std::uint16_t v){b[p]=static_cast<std::uint8_t>(v);b[p+1]=static_cast<std::uint8_t>(v>>8);}
bool IsSentinel(const B&b){for(unsigned n=0;n<4;++n)if(std::bit_cast<std::uint32_t>(b.minimum[n])!=0x7f7fffeeu||std::bit_cast<std::uint32_t>(b.maximum[n])!=0xff7fffeeu)return false;return true;}
bool LoopEnters(std::int32_t first,std::int16_t count){auto sum=static_cast<std::int64_t>(first)+count;if(sum>INT32_MAX)sum-=4294967296ll;if(sum<INT32_MIN)sum+=4294967296ll;return static_cast<std::int64_t>(first)<sum;}
bool Throws(const std::function<void()>&fn){try{fn();}catch(const std::exception&){return true;}return false;}
H SmallNormalize(H h){for(std::size_t n=1;n<h.nodes.size();++n){auto&node=h.nodes[n];if(n==h.root||!node.child0)continue;Check(h.nodes[node.child0].child0==0&&h.nodes[node.child1_or_payload].child0==0,"Small independent finalizer fixture unexpectedly needs rotation");if(Area(h.nodes[node.child1_or_payload].bounds)>Area(h.nodes[node.child0].bounds))std::swap(node.child0,node.child1_or_payload);}return h;}
void Payloads(const Monstey_NavCompactTreeOwned&tree,std::size_t count){Check(tree.nodes.size()==(count?2*count-1:0)*6,"Native unused faces were dropped from compact nodes");std::vector<std::uint32_t>ids;for(std::size_t n=0;n<tree.nodes.size();n+=6)if((tree.nodes[n+3]&0x80)==0)ids.push_back((static_cast<std::uint32_t>(tree.nodes[n+3]&0x7f)<<16)|tree.nodes[n+4]|(static_cast<std::uint32_t>(tree.nodes[n+5])<<8));std::sort(ids.begin(),ids.end());Check(ids.size()==count,"Unused face payload count changed");for(std::size_t n=0;n<count;++n)Check(ids[n]==n,"Unused face ordinal/payload was renumbered");}
void FullSmall(std::span<const B>bounds){const auto input=Monstey_NavInsertHierarchy(bounds);H expected=input;if(input.leaf_count>2){expected=Oracle{input,{}}.Run();expected=SmallNormalize(std::move(expected));}const auto want=Pack(Preorder(expected));const auto got=Monstey_NavCookCompactBounds(bounds);Check(got.nodes==want.nodes,"Full unused-face compact bytes differ from independent small-route oracle");for(unsigned n=0;n<4;++n)Check(std::bit_cast<std::uint32_t>(got.bounds.minimum[n])==std::bit_cast<std::uint32_t>(want.bounds.minimum[n])&&std::bit_cast<std::uint32_t>(got.bounds.maximum[n])==std::bit_cast<std::uint32_t>(want.bounds.maximum[n]),"Full unused-face root bounds differ");Payloads(got,bounds.size());++cases;}
}
int wmain(int argc,wchar_t**argv){try{using namespace Monstey;Check(argc==2,"Need NEW report");unsigned groups=0;const auto empty=Empty();
 // Exact native signed-end branch, independent mathematical wrap oracle.
 std::mt19937 rng(0x3cd554u);std::vector<std::pair<std::int32_t,std::int32_t>>loopCases{{INT32_MIN,0},{INT32_MAX,0},{0,-1},{INT32_MIN,-1},{INT32_MAX,1},{INT32_MAX-1,2},{INT32_MIN,1},{INT32_MIN+32768,-32768},{INT32_MIN,-32768},{INT32_MAX,32767},{-100,-32768},{-1,0},{0,0}};
 for(unsigned n=0;n<4096;++n)loopCases.push_back({std::bit_cast<std::int32_t>(rng()),std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(rng()))});
 for(const auto[first,countValue]:loopCases){const auto count=static_cast<std::int16_t>(countValue);std::vector<std::uint8_t>faces(16);Put32(faces,0,std::bit_cast<std::uint32_t>(first));Put16(faces,8,std::bit_cast<std::uint16_t>(count));const auto entered=LoopEnters(first,count);if(entered){Check(Throws([&]{(void)Monstey_StaticNavFaceBounds(faces,{},{},0);}),"Entered native-invalid range was fabricated as empty");}else{const auto face=Monstey_StaticNavFaceBounds(faces,{},{},0);Check(face.vertex_iterations==0&&face.first_edge==first&&face.edge_count==count&&IsSentinel({face.minimum,face.maximum}),"Native skipped wrapped range did not return zero-read sentinel");}++cases;}++groups;
 // Old helper is deliberately wrong at both signed-overflow boundaries.
 std::vector<std::uint8_t>single(16);Put32(single,0,0x7fffffffu);Put16(single,8,1);Check(Throws([&]{(void)Monstey_StaticNavFaceBoundsBaseline(single,{},{},0,{});}),"Preimage no longer exposes positive-overflow regression");Put32(single,0,0x80000000u);Put16(single,8,0xffffu);Check(Monstey_StaticNavFaceBoundsBaseline(single,{},{},0,{}).vertex_iterations==0,"Preimage no longer exposes negative-underflow regression");++cases;++groups;
 // Every nonoverflowing ordinary/unused case remains bitwise identical.
 for(std::int16_t count:std::array<std::int16_t,5>{0,-1,-2,-32768,3}){std::vector<std::uint8_t>faces(16),edges(60),vertices(48);Put32(faces,0,0);Put16(faces,8,std::bit_cast<std::uint16_t>(count));for(unsigned n=0;n<3;++n){Put32(edges,n*20,n);const std::array<float,4>p{static_cast<float>(n),0,static_cast<float>(n%2),static_cast<float>(n)};std::memcpy(vertices.data()+n*16,p.data(),16);}const auto a=Monstey_StaticNavFaceBounds(faces,edges,vertices,0),b=Monstey_StaticNavFaceBoundsBaseline(faces,edges,vertices,0,{});Check(a.minimum==b.minimum&&a.maximum==b.maximum&&a.vertex_iterations==b.vertex_iterations,"Nonoverflowing face helper regressed");++cases;}++groups;
 const B positive{{0,0,0,0},{1,0,1,0}},symmetric{{-1,-1,-1,0},{1,1,1,0}},negative{{-11,0,-11,0},{-10,0,-10,0}};
 for(const auto&valid:std::array{positive,symmetric,negative}){const std::vector<B>b{empty,valid,valid};const auto input=Monstey_NavInsertHierarchy(b);Compare(input);Check(Throws([&]{(void)Monstey_NavOptimizeHierarchyCoreBaseline(input);}),"Core preimage no longer rejects native-safe mixed case");const auto core=Monstey_NavOptimizeHierarchyCore(input);Check(Throws([&]{(void)Monstey_NavFinalizeHierarchyBaseline(core,true);}),"Finalizer preimage no longer rejects native-safe mixed case");FullSmall(b);}++groups;
 FullSmall(std::array{empty,empty,symmetric,symmetric});++groups;
 for(std::size_t n=0;n<=2;++n)FullSmall(std::vector<B>(n,empty));++groups;
 // Exhaust mixed insertion profiles. Unsafe oracle cases remain exact no-bin/
 // no-pair diagnostics, without a made-up output or a global empty-count ban.
 unsigned safe=0,unsafe=0;for(unsigned n=3;n<=8;++n)for(unsigned mask=0;mask<(1u<<n);++mask){std::vector<B>b;for(unsigned k=0;k<n;++k)b.push_back((mask&(1u<<k))?empty:symmetric);const auto input=Monstey_NavInsertHierarchy(b);std::optional<H>want;std::string referenceError;try{want=Oracle{input,{}}.Run();}catch(const std::exception&e){referenceError=e.what();Check(referenceError=="Oracle no merge"||referenceError=="Oracle no bin","Unexpected independent oracle error");}if(want){const auto got=Monstey_NavOptimizeHierarchyCore(input);Check(Equal(got,*want),"Mixed sentinel core differs from independent native-cost oracle");++safe;}else{bool nativeBoundary=false;try{(void)Monstey_NavOptimizeHierarchyCore(input);}catch(const std::exception&e){const std::string text=e.what();nativeBoundary=text.find("pair merge")!=std::string::npos||text.find("bin split")!=std::string::npos;}Check(nativeBoundary,"Unsafe native partition was masked by broad validation or fabricated output");++unsafe;}++cases;}Check(safe>0&&unsafe>0,"Mixed grammar failed to exercise both native branches");++groups;
 for(unsigned n:std::array<unsigned,3>{3,4,8}){Check(Throws([&]{(void)Monstey_NavCookCompactBounds(std::vector<B>(n,empty));}),"All-unused native no-candidate branch fabricated output");++cases;}Check(Throws([&]{(void)Monstey_NavCookCompactBounds(std::array{empty,empty,positive,positive});}),"Isolated unused pair fabricated a native merge");++cases;++groups;
 // Raw signed face records preserve every payload and ignore invalid first-edge
 // ONLY when the proven signed-end branch skips.
 std::vector<std::uint8_t>faces(48),edges(60),vertices(48);for(unsigned n=0;n<3;++n){Put32(edges,n*20,n);const std::array<std::array<float,4>,3>p{{{0,0,0,0},{1,0,0,0},{0,0,1,0}}};std::memcpy(vertices.data()+n*16,p[n].data(),16);}Put16(faces,24,3);Put16(faces,40,3);
 const auto reference=Monstey_NavCookCompactBounds(std::array{empty,positive,positive});for(const auto[first,countValue]:std::array<std::pair<std::int32_t,std::int32_t>,7>{{{INT32_MIN,0},{INT32_MAX,0},{-1,0},{0,-1},{INT32_MAX,-32768},{INT32_MIN+32768,-32768},{INT32_MAX,1}}}){const auto count=static_cast<std::int16_t>(countValue);Put32(faces,0,std::bit_cast<std::uint32_t>(first));Put16(faces,8,std::bit_cast<std::uint16_t>(count));const auto cooked=Monstey_NavCookCompactMesh(faces,edges,vertices);Check(cooked.nodes==reference.nodes,"Raw unused face first/count changed compact bytes");Payloads(cooked,3);++cases;}Put32(faces,0,0x80000000u);Put16(faces,8,0xffffu);Check(Throws([&]{(void)Monstey_NavCookCompactMesh(faces,edges,vertices);}),"Negative-underflow first/count was silently treated unused");++cases;++groups;
 // Ordinary native behavior and public ABI remain unchanged.
 for(unsigned trial=0;trial<96;++trial){std::vector<B>b;for(unsigned n=0;n<3+rng()%35;++n)b.push_back(Box(static_cast<float>(rng()%31)*.125f,static_cast<float>(rng()%17)*.25f,static_cast<float>(rng()%7)*.5f,.125f));const auto input=Monstey_NavInsertHierarchy(b);const auto core=Monstey_NavOptimizeHierarchyCore(input),old=Monstey_NavOptimizeHierarchyCoreBaseline(input);Check(Equal(core,old),"Ordinary core output changed");Check(Equal(Monstey_NavFinalizeHierarchy(core,true),Monstey_NavFinalizeHierarchyBaseline(old,true)),"Ordinary finalizer output changed");++cases;}++groups;
 for(unsigned mode=0;mode<3;++mode){auto bad=empty;if(mode==0)bad.minimum[0]=std::nextafter(bad.minimum[0],0.0f);if(mode==1)bad.minimum[3]=bad.maximum[3]=0;if(mode==2)bad={{2,0,2,0},{1,0,1,0}};const auto input=Monstey_NavInsertHierarchy(std::array{bad,positive,positive});Check(Throws([&]{(void)Monstey_NavOptimizeHierarchyCore(input);}),"Nonexact/inverted input broadened beyond selected native grammar");++cases;}++groups;
 Monstey_WriteNewText(argv[1],Monstey_Json::object({{"schema","Monstey.UnusedFaceFidelityTests/1"},{"pass",true},{"groups",groups},{"cases",cases},{"wrapped_loop_cases",loopCases.size()},{"profiles_safe",safe},{"profiles_unsafe",unsafe},{"ordinary_profiles",6},{"mixed_safe",safe-6},{"original_functions_executed",false}}).dump(2)+"\n");std::cout<<"PASS "<<groups<<" groups "<<cases<<" unused-face cases; mixed safe="<<safe<<" unsafe="<<unsafe<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}