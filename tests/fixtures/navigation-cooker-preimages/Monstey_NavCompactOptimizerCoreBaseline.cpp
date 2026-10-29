#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_NavCompactSortCentroids.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <immintrin.h>
#include <utility>
namespace Monstey {
namespace {
using B=Monstey_NavigationAabb;using V=Monstey_NavigationVector4;using H=Monstey_NavHierarchy;
constexpr std::uint32_t Monstey_PositiveSentinel=0x7f7fffee;
__m128 Monstey_Load(const V&v){return _mm_loadu_ps(v.data());}
V Monstey_Store(__m128 value){V out{};_mm_storeu_ps(out.data(),value);return out;}
B Monstey_EmptyBounds(){const float big=std::bit_cast<float>(Monstey_PositiveSentinel);return {{big,big,big,big},{-big,-big,-big,-big}};}
B Monstey_Union(const B&a,const B&b){return {Monstey_Store(_mm_min_ps(Monstey_Load(a.minimum),Monstey_Load(b.minimum))),Monstey_Store(_mm_max_ps(Monstey_Load(a.maximum),Monstey_Load(b.maximum)))};}
float Monstey_Area(const B&box){auto d=_mm_sub_ps(Monstey_Load(box.maximum),Monstey_Load(box.minimum));auto p=_mm_mul_ps(_mm_shuffle_ps(d,d,0xc9),d);return _mm_cvtss_f32(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p,p,0x55),_mm_shuffle_ps(p,p,0)),_mm_shuffle_ps(p,p,0xaa)));}
float Monstey_MulAdd(float area,std::uint32_t count,float old){const auto product=_mm_mul_ss(_mm_set_ss(area),_mm_cvtepi32_ps(_mm_cvtsi32_si128(static_cast<std::int32_t>(count))));return _mm_cvtss_f32(_mm_add_ss(product,_mm_set_ss(old)));}
struct Monstey_Work {std::vector<std::uint32_t> ids;std::uint32_t parent=0;std::int32_t slot=-1;};
struct Monstey_Bin {std::vector<std::uint32_t> ids;B bounds=Monstey_EmptyBounds();float cost=0;};
void Monstey_Append(std::vector<std::uint32_t>&target,const std::vector<std::uint32_t>&source){target.insert(target.end(),source.begin(),source.end());}
}
H Monstey_NavOptimizeHierarchyCoreBaseline(H tree){
  if(tree.root==0||tree.leaf_count<=2)return tree;
  Monstey_Require(tree.root<tree.nodes.size()&&tree.nodes[tree.root].child0!=0,"Native optimizer needs an internal owned root");
  Monstey_Require(tree.nodes[tree.root].parent==0,"Selected native constructor profile optimizes the entire root");
  std::vector<bool>seen(tree.nodes.size());std::vector<std::uint32_t>walk{tree.root},leaves,free;
  // A74D50..A750C5: left-first leaves, release internal nodes onto a LIFO free list.
  while(!walk.empty()){const auto at=walk.back();walk.pop_back();Monstey_Require(at>0&&at<tree.nodes.size()&&!seen[at],"Native optimizer source child invalid or cyclic");seen[at]=true;const auto&node=tree.nodes[at];for(unsigned axis=0;axis<3;++axis)Monstey_Require(std::isfinite(node.bounds.minimum[axis])&&std::isfinite(node.bounds.maximum[axis])&&node.bounds.minimum[axis]<=node.bounds.maximum[axis],"Native optimizer requires finite ordered XYZ bounds");if(node.child0){free.push_back(at);walk.push_back(node.child1_or_payload);walk.push_back(node.child0);}else leaves.push_back(at);}
  Monstey_Require(leaves.size()==tree.leaf_count&&free.size()+1==leaves.size(),"Native optimizer complete binary source count differs");
  auto allocate=[&](){Monstey_Require(!free.empty(),"Native successful optimizer exhausted released owned nodes");const auto id=free.back();free.pop_back();return id;};
  auto attach=[&](std::uint32_t parent,std::int32_t slot,std::uint32_t child){tree.nodes[child].parent=parent;if(slot<0)tree.root=child;else{Monstey_Require(parent>0&&parent<tree.nodes.size()&&(slot==0||slot==1),"Native optimizer attachment outside owned tree");if(slot==0)tree.nodes[parent].child0=child;else tree.nodes[parent].child1_or_payload=child;}};
  std::vector<Monstey_Work>pending;pending.push_back({std::move(leaves),0,-1});
  while(!pending.empty()){
    auto work=std::move(pending.back());pending.pop_back();Monstey_Require(!work.ids.empty(),"Native optimizer partition unexpectedly empty");
    // A757D6..A759F6: pair merge with strict ordered surface-area improvement.
    if(work.ids.size()<=16&&work.slot!=-1){
      while(work.ids.size()>1){float best=std::bit_cast<float>(Monstey_PositiveSentinel);std::size_t first=0,second=0;B joined{};bool selected=false;
        for(std::size_t i=0;i<work.ids.size();++i)for(std::size_t j=i+1;j<work.ids.size();++j){const auto candidate=Monstey_Union(tree.nodes[work.ids[j]].bounds,tree.nodes[work.ids[i]].bounds);const auto area=Monstey_Area(candidate);if(best>area){best=area;first=i;second=j;joined=candidate;selected=true;}}
        Monstey_Require(selected,"Native pair merge has no finite area candidate");const auto left=work.ids[first],right=work.ids[second],parent=allocate();tree.nodes[parent]={joined,0,left,right};tree.nodes[left].parent=parent;tree.nodes[right].parent=parent;if(second!=work.ids.size()-1)work.ids[second]=work.ids.back();work.ids[first]=parent;work.ids.pop_back();
      }
      attach(work.parent,work.slot,work.ids.front());continue;
    }
    // A759FB..A75ADA: union, native masked reciprocal, allocate split node.
    B bounds=Monstey_EmptyBounds();for(const auto id:work.ids)bounds=Monstey_Union(bounds,tree.nodes[id].bounds);
    const auto extent=_mm_sub_ps(Monstey_Load(bounds.maximum),Monstey_Load(bounds.minimum));const auto equalZero=_mm_cmpeq_ps(extent,_mm_setzero_ps());const auto reciprocal=_mm_and_ps(_mm_andnot_ps(equalZero,_mm_div_ps(_mm_set1_ps(1),extent)),_mm_castsi128_ps(_mm_set_epi32(0,-1,-1,-1)));const auto scale=_mm_mul_ps(reciprocal,_mm_set1_ps(32));
    const auto parent=allocate();tree.nodes[parent].bounds=bounds;attach(work.parent,work.slot,parent);
    std::array<std::array<Monstey_Bin,32>,3>bins{};auto low=_mm_set1_ps(32),high=_mm_setzero_ps();
    // A76000..A7623C: exact rounded/clamped centroid binning for XYZ.
    for(const auto id:work.ids){const auto&box=tree.nodes[id].bounds;const auto center=_mm_mul_ps(_mm_add_ps(Monstey_Load(box.maximum),Monstey_Load(box.minimum)),_mm_set1_ps(.5f));auto position=_mm_add_ps(_mm_mul_ps(_mm_sub_ps(center,Monstey_Load(bounds.minimum)),scale),_mm_set1_ps(.5f));position=_mm_min_ps(_mm_max_ps(position,_mm_setzero_ps()),_mm_set1_ps(31));high=_mm_max_ps(position,high);low=_mm_min_ps(position,low);std::array<std::int32_t,4>indices{};_mm_storeu_si128(reinterpret_cast<__m128i*>(indices.data()),_mm_cvttps_epi32(position));for(unsigned axis=0;axis<3;++axis){const auto index=indices[axis];Monstey_Require(index>=0&&index<32,"Native centroid bin not representable");auto&bin=bins[axis][static_cast<std::size_t>(index)];bin.bounds=Monstey_Union(bin.bounds,box);bin.ids.push_back(id);}}
    std::array<std::int32_t,4>minimum{},maximum{};_mm_storeu_si128(reinterpret_cast<__m128i*>(minimum.data()),_mm_cvttps_epi32(low));_mm_storeu_si128(reinterpret_cast<__m128i*>(maximum.data()),_mm_cvttps_epi32(high));
    float best=std::bit_cast<float>(Monstey_PositiveSentinel);unsigned chosenAxis=0;std::int32_t chosenBin=-1;
    // A76270..A763E2: accumulate prefix AND suffix area*count into each bin.
    for(unsigned axis=0;axis<3;++axis){const auto begin=minimum[axis],end=maximum[axis];B prefix=Monstey_EmptyBounds(),suffix=Monstey_EmptyBounds();std::uint32_t prefixCount=0,suffixCount=0;for(std::int32_t offset=0;offset<=end-begin;++offset){auto&left=bins[axis][static_cast<std::size_t>(begin+offset)];auto&right=bins[axis][static_cast<std::size_t>(end-offset)];prefix=Monstey_Union(prefix,left.bounds);prefixCount+=static_cast<std::uint32_t>(left.ids.size());suffix=Monstey_Union(suffix,right.bounds);suffixCount+=static_cast<std::uint32_t>(right.ids.size());left.cost=Monstey_MulAdd(Monstey_Area(prefix),prefixCount,left.cost);right.cost=Monstey_MulAdd(Monstey_Area(suffix),suffixCount,right.cost);}
      for(auto at=begin;at<=end;++at){const auto&bin=bins[axis][static_cast<std::size_t>(at)];if(!bin.ids.empty()&&!(bin.cost>=best)){best=bin.cost;chosenAxis=axis;chosenBin=at;}}
    }
    Monstey_Require(chosenBin>=0,"Native bin split has no selected finite profile candidate");Monstey_Work left{{},parent,0},right{{},parent,1};for(std::int32_t at=0;at<chosenBin;++at)Monstey_Append(left.ids,bins[chosenAxis][static_cast<std::size_t>(at)].ids);for(auto at=chosenBin+1;at<32;++at)Monstey_Append(right.ids,bins[chosenAxis][static_cast<std::size_t>(at)].ids);auto&middle=bins[chosenAxis][static_cast<std::size_t>(chosenBin)].ids;
    // A7657C..A7677F: empty-side special case; otherwise native median sort of ONE bin.
    if(left.ids.empty()!=right.ids.empty())Monstey_Append(left.ids.empty()?left.ids:right.ids,middle);else{Monstey_NavCompactSortCentroids(middle,static_cast<std::int32_t>(middle.size()),chosenAxis,tree.nodes);const auto midpoint=middle.size()/2;left.ids.insert(left.ids.end(),middle.begin(),middle.begin()+static_cast<std::ptrdiff_t>(midpoint));right.ids.insert(right.ids.end(),middle.begin()+static_cast<std::ptrdiff_t>(midpoint),middle.end());}
    Monstey_Require(!left.ids.empty()&&!right.ids.empty(),"Native successful split must populate both children");pending.push_back(std::move(left));pending.push_back(std::move(right));
  }
  Monstey_Require(free.empty(),"Native optimizer did not reuse all released internal nodes");return tree;
}
}

