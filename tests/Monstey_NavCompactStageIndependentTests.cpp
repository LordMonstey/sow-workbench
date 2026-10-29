#include "Monstey_NavCompactCookerStages.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <xmmintrin.h>
namespace {
using namespace Monstey;using B=Monstey_NavigationAabb;using H=Monstey_NavHierarchy;
std::uint64_t cases=0;
void Check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
float Add(float a,float b){volatile float x=a+b;return x;}float Sub(float a,float b){volatile float x=a-b;return x;}float Mul(float a,float b){volatile float x=a*b;return x;}
B Join(const B&a,const B&b){B q{};for(unsigned k=0;k<4;++k){q.minimum[k]=a.minimum[k]<b.minimum[k]?a.minimum[k]:b.minimum[k];q.maximum[k]=a.maximum[k]>b.maximum[k]?a.maximum[k]:b.maximum[k];}return q;}
bool Same(const B&a,const B&b){for(unsigned k=0;k<4;++k)if(std::bit_cast<std::uint32_t>(a.minimum[k])!=std::bit_cast<std::uint32_t>(b.minimum[k])||std::bit_cast<std::uint32_t>(a.maximum[k])!=std::bit_cast<std::uint32_t>(b.maximum[k]))return false;return true;}
bool Same(const H&a,const H&b){if(a.root!=b.root||a.leaf_count!=b.leaf_count||a.nodes.size()!=b.nodes.size())return false;for(std::size_t n=0;n<a.nodes.size();++n){const auto&x=a.nodes[n];const auto&y=b.nodes[n];if(!Same(x.bounds,y.bounds)||x.parent!=y.parent||x.child0!=y.child0||x.child1_or_payload!=y.child1_or_payload)return false;}return true;}
float Cost(const B&a,const B&b){std::array<float,3>distance{},extent{};for(unsigned k=0;k<3;++k){const auto d=Sub(Add(a.maximum[k],a.minimum[k]),Add(b.maximum[k],b.minimum[k]));distance[k]=Mul(d,d);extent[k]=Add(Sub(a.maximum[k],a.minimum[k]),Sub(b.maximum[k],b.minimum[k]));}return Mul(Add(Add(distance[1],distance[0]),distance[2]),Add(Add(extent[1],extent[0]),extent[2]));}
H Insert(std::span<const B> bounds){H h;h.nodes.push_back({});h.nodes.reserve(bounds.size()*2+1);
 for(std::size_t n=0;n<bounds.size();++n){const auto leaf=static_cast<std::uint32_t>(h.nodes.size());h.nodes.push_back({bounds[n],0,0,static_cast<std::uint32_t>(n)});
  if(!h.root)h.root=leaf;else{const auto p=static_cast<std::uint32_t>(h.nodes.size());h.nodes.emplace_back();
   std::function<std::uint32_t(std::uint32_t)>descend=[&](auto at)->std::uint32_t{
    const auto old=h.nodes[at];if(!old.child0){h.nodes[p]={Join(old.bounds,bounds[n]),old.parent,at,leaf};h.nodes[at].parent=p;h.nodes[leaf].parent=p;return p;}
    const bool right=Cost(h.nodes[old.child0].bounds,bounds[n])>Cost(h.nodes[old.child1_or_payload].bounds,bounds[n]);
    const auto result=descend(right?old.child1_or_payload:old.child0);if(right)h.nodes[at].child1_or_payload=result;else h.nodes[at].child0=result;
    h.nodes[at].bounds=Join(old.bounds,bounds[n]);return at;
   };h.root=descend(h.root);
  }++h.leaf_count;
 }return h;
}
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
void Pipeline(const std::vector<B>&b){const auto expected=Insert(b),got=Monstey_NavInsertHierarchy(b);Check(Same(got,expected),"Insertion exact scalar topology differs");const auto pre=Monstey_NavPreorderHierarchy(got),wantPre=Preorder(expected);Check(Same(pre,wantPre),"Preorder exact recursive identity differs");const auto packed=Monstey_NavPackPreorder(pre),want=Pack(wantPre);Check(packed.nodes==want.nodes&&Same(packed.bounds,want.bounds),"Packed bytes/bounds independent oracle differs");++cases;}
B Box(float x,float y,float z,float e=1){return {{x,y,z,0},{Add(x,e),Add(y,e),Add(z,e),0}};}
void Error(const std::function<void()>&call){bool threw=false;try{call();}catch(const std::exception&){threw=true;}Check(threw,"Malformed owned stage input accepted");++cases;}
}
int wmain(int argc,wchar_t**argv){try{using namespace Monstey;Check(argc==2,"Need NEW report");unsigned groups=0;
 Pipeline({});auto empty=H{};empty.root=0xffffffffu;const auto zero=Monstey_NavPackPreorder(empty);Check(zero.nodes.empty()&&Same(zero.bounds,Pack(empty).bounds),"Empty packing reads/validates root or sentinel differs");++cases;++groups;
 for(unsigned n:std::array<unsigned,7>{1,2,3,4,16,31,64})Pipeline(std::vector<B>(n,Box(0,0,0)));auto tied=Monstey_NavInsertHierarchy(std::vector<B>(3,Box(0,0,0)));Check(tied.root==3&&tied.nodes[3].child0==5&&tied.nodes[3].child1_or_payload==2&&tied.nodes[5].child0==1&&tied.nodes[5].child1_or_payload==4,"Native insertion tie does not descend left");++cases;++groups;
 std::mt19937 rng(0xbfdc90u);for(unsigned trial=0;trial<384;++trial){const auto n=1+rng()%80;std::vector<B>b;for(unsigned k=0;k<n;++k)b.push_back(Box(static_cast<float>(static_cast<int>(rng()%129)-64)*.25f,static_cast<float>(rng()%33)*.125f,static_cast<float>(rng()%9)*.5f,static_cast<float>(rng()%8)*.125f));Pipeline(b);}++groups;
 for(unsigned n=0;n<4096;++n){const auto parent=Box(static_cast<float>(static_cast<int>(rng()%129)-64),static_cast<float>(rng()%17),static_cast<float>(rng()%5),static_cast<float>(rng()%129)*.25f);auto child=parent;for(unsigned a=0;a<3;++a){const auto e=Sub(parent.maximum[a],parent.minimum[a]);child.minimum[a]=Add(parent.minimum[a],Mul(e,static_cast<float>(rng()%150)*.01f));child.maximum[a]=Sub(parent.maximum[a],Mul(e,static_cast<float>(rng()%150)*.01f));}Check(Monstey_NavCompactQuantize(parent,child)==Quantize(parent,child),"Quantizer scalar exact bytes differ");++cases;}++groups;
 const B parent{{-7,-11,-13,10},{220,102,91,100}};for(unsigned tag=0;tag<256;++tag){const std::array<std::uint8_t,4>q{static_cast<std::uint8_t>(tag),static_cast<std::uint8_t>(255-tag),0x3d,static_cast<std::uint8_t>(tag)};Check(Same(Monstey_NavCompactDecode(parent,q),Decode(parent,q)),"Explicit decoder XYZ/W differs");++cases;}++groups;
 for(unsigned n=0;n<16;++n){auto h=Monstey_NavPreorderHierarchy(Insert(std::vector<B>(1,Box(0,0,0))));h.nodes[1].child1_or_payload=std::array<std::uint32_t,8>{0,1,0xffff,0x10000,0x7fffff,0x800000,0xffffffffu,0x12345678}[n%8];auto a=Monstey_NavPackPreorder(h),b=Pack(h);Check(a.nodes==b.nodes,"Leaf payload truncation/byte order differs");++cases;}++groups;
 for(unsigned trial=0;trial<16;++trial){std::vector<B>b;for(unsigned k=0;k<17;++k){auto q=Box(static_cast<float>(k),static_cast<float>(k%3),0,.5f);q.minimum[3]=k%3==0?std::bit_cast<float>(0x7fc01000u|k):-0.0f;q.maximum[3]=static_cast<float>(trial+k);b.push_back(q);}Pipeline(b);}++groups;
 const auto saved=_mm_getcsr();for(unsigned mode=0;mode<16;++mode){_mm_setcsr((saved&~0xe07fu)|((mode%4u)<<13)|((mode&4u)?0x8000u:0u)|((mode&8u)?0x40u:0u));std::vector<B>b;for(unsigned n=0;n<19;++n)b.push_back(Box(static_cast<float>(n)*.1f,static_cast<float>(n%4)*.3f,.1f,.15f));Pipeline(b);Check(Same(Monstey_NavCompactDecode(parent,{0xaf,0x3d,0x4b,0x8f}),Decode(parent,{0xaf,0x3d,0x4b,0x8f})),"Rounding decoder differs");++cases;}_mm_setcsr(saved);++groups;
 const float nan=std::bit_cast<float>(0x7fc00123u);auto weird=parent;weird.minimum[0]=nan;auto target=parent;target.maximum[1]=nan;Check(Monstey_NavCompactQuantize(weird,target)==Quantize(weird,target),"Unordered quantizer predicate differs");++cases;
 auto inverted=parent;std::swap(inverted.minimum[0],inverted.maximum[0]);Check(Monstey_NavCompactQuantize(inverted,target)==Quantize(inverted,target),"Noncontainment first-failure predicate differs");++cases;++groups;
 std::vector<std::uint8_t>faces(3*16),edges(9*20),vertices(9*16);std::vector<B>expectedBounds;
 auto put32=[](std::vector<std::uint8_t>&bytes,std::size_t at,std::uint32_t value){for(unsigned k=0;k<4;++k)bytes[at+k]=static_cast<std::uint8_t>(value>>(k*8));};
 for(unsigned f=0;f<3;++f){put32(faces,f*16,f*3);put32(faces,f*16+4,0xffffffffu);faces[f*16+8]=3;faces[f*16+10]=31;const float x=static_cast<float>(f*10);const std::array<std::array<float,4>,3>points{{{x,0,0,static_cast<float>(f)},{x+1,2,1,static_cast<float>(f+1)},{x-1,1,-1,static_cast<float>(f+2)}}};for(unsigned k=0;k<3;++k){put32(edges,(f*3+k)*20,f*3+k);std::memcpy(vertices.data()+(f*3+k)*16,points[k].data(),16);}expectedBounds.push_back({{x-1,0,-1,static_cast<float>(f)},{x+1,2,1,static_cast<float>(f+2)}});}
 Check(Same(Monstey_NavInsertFaceHierarchy(faces,edges,vertices),Insert(expectedBounds)),"Native face-to-insertion composition differs");++cases;Error([&]{(void)Monstey_NavInsertFaceHierarchy(std::span<const std::uint8_t>(faces.data(),faces.size()-1),edges,vertices);});++groups;
 auto h=Monstey_NavInsertHierarchy(std::vector<B>(3,Box(0,0,0)));auto bad=h;bad.nodes[bad.root].child0=bad.root;Error([&]{(void)Monstey_NavPreorderHierarchy(bad);});bad=h;bad.nodes[bad.root].child1_or_payload=9999;Error([&]{(void)Monstey_NavPreorderHierarchy(bad);});bad=h;++bad.leaf_count;Error([&]{(void)Monstey_NavPreorderHierarchy(bad);});auto pre=Monstey_NavPreorderHierarchy(h);bad=pre;bad.nodes[2].parent=2;Error([&]{(void)Monstey_NavPackPreorder(bad);});bad=pre;bad.nodes[1].child0=3;Error([&]{(void)Monstey_NavPackPreorder(bad);});++groups;
 Monstey_WriteNewText(argv[1],Monstey_Json::object({{"schema","Monstey.CompactStageIndependentTests/1"},{"pass",true},{"groups",groups},{"cases",cases},{"random_pipeline_cases",384},{"quantizer_cases",4096},{"decoder_tag_cases",256},{"original_functions_executed",false}}).dump(2)+"\n");std::cout<<"PASS "<<groups<<" groups "<<cases<<" stage cases\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
