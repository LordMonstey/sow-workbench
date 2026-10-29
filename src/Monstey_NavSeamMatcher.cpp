#include "Monstey_NavSeamMatcher.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <bit>
#include <climits>
#include <immintrin.h>
#include <map>
#include <set>
namespace Monstey {
namespace {
constexpr std::uint32_t Mask=0x3fffffu;
using C=Monstey_NavSeamCandidate;using V=Monstey_NavSeamVector4;
float Abs(float v){return std::bit_cast<float>(std::bit_cast<std::uint32_t>(v)&0x7fffffffu);}
bool Less(const C&a,const C&b){const float epsilon=std::bit_cast<float>(std::uint32_t{0x3727c5ac});if(!(epsilon>=Abs(a.score-b.score)))return a.score>b.score;if(!(epsilon>=Abs(a.horizontal_cosine-b.horizontal_cosine)))return b.horizontal_cosine>a.horizontal_cosine;return !(a.overlap>=b.overlap);}
void Sort(std::vector<C>&v,std::int32_t begin,std::int32_t end){while(begin<end){std::int32_t left=begin,right=end;const C pivot=v[static_cast<std::size_t>((begin+end)/2)];do{while(Less(v[static_cast<std::size_t>(left)],pivot)){Monstey_Require(left<end,"Native candidate comparator exceeded owned sort range");++left;}while(Less(pivot,v[static_cast<std::size_t>(right)])){Monstey_Require(right>begin,"Native candidate comparator exceeded owned sort range");--right;}if(left<=right){if(left!=right)std::swap(v[static_cast<std::size_t>(left)],v[static_cast<std::size_t>(right)]);++left;--right;}}while(left<=right);if(begin<right)Sort(v,begin,right);begin=left;}}
float NativeMin(float a,float b){return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(a),_mm_set_ss(b)));}
float NativeMax(float a,float b){return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(a),_mm_set_ss(b)));}
float LengthSquared(const V&a,const V&b){const auto delta=_mm_sub_ps(_mm_loadu_ps(a.data()),_mm_loadu_ps(b.data()));const auto p=_mm_mul_ps(delta,delta);return _mm_cvtss_f32(_mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p,p,0x55),_mm_shuffle_ps(p,p,0)),_mm_shuffle_ps(p,p,0xaa)));}
bool Close(const V&a,const V&b,float tolerance){const auto difference=_mm_sub_ps(_mm_loadu_ps(a.data()),_mm_loadu_ps(b.data()));const auto distance=_mm_andnot_ps(_mm_set1_ps(-0.0f),difference);return (_mm_movemask_ps(_mm_cmple_ps(distance,_mm_set1_ps(tolerance)))&7)==7;}
}
std::vector<C> Monstey_NavSeamResolveCandidates(std::span<const C>input){Monstey_Require(input.size()<=static_cast<std::size_t>(INT_MAX/2),"Native candidate count exceeds signed pair-node grammar");std::vector<C>v(input.begin(),input.end());if(v.size()>1)Sort(v,0,static_cast<std::int32_t>(v.size()-1));struct Node{std::size_t candidate;std::int32_t next;};std::vector<Node>nodes;nodes.reserve(v.size()*2);std::map<std::uint32_t,std::int32_t>heads;for(std::size_t i=0;i<v.size();++i){for(const auto key:{v[i].edge_a,v[i].edge_b}){const auto found=heads.find(key);const auto previous=found==heads.end()?-1:found->second;nodes.push_back({i,previous});heads[key]=static_cast<std::int32_t>(nodes.size()-1);}}std::vector<std::pair<std::uint32_t,std::int32_t>>groups(heads.begin(),heads.end());std::sort(groups.begin(),groups.end(),[](const auto&a,const auto&b){return a.second>b.second;});const float trim=std::bit_cast<float>(std::uint32_t{0x3c23d70a});for(unsigned pass=0;pass<2;++pass){for(const auto&[key,head]:groups){for(auto at=head;at!=-1;at=nodes[static_cast<std::size_t>(at)].next){const auto&node=nodes[static_cast<std::size_t>(at)];auto&anchor=v[node.candidate];if(!anchor.enabled)continue;const auto side=anchor.edge_a==key?0u:2u;const float low=anchor.intervals[side],high=anchor.intervals[side+1];for(auto next=node.next;next!=-1;next=nodes[static_cast<std::size_t>(next)].next){auto&other=v[nodes[static_cast<std::size_t>(next)].candidate];if(!other.enabled)continue;const auto otherSide=other.edge_a==key?0u:2u;float&otherLow=other.intervals[otherSide];float&otherHigh=other.intervals[otherSide+1];const float overlap=NativeMin(high,otherHigh)-NativeMax(low,otherLow);if(pass==1){if(overlap>0)other.enabled=false;continue;}const bool lowInside=!(low>otherLow)&&high>=otherLow;const bool highInside=!(low>otherHigh)&&high>=otherHigh;const bool contained=lowInside&&highInside;if(!contained&&overlap>0&&overlap<trim){if(lowInside&&!highInside)otherLow=high;else if(!lowInside&&highInside)otherHigh=low;}const bool anchorLowInside=!(otherLow>low)&&!(low>otherHigh);const bool anchorHighInside=!(otherLow>high)&&!(high>otherHigh);if((anchorLowInside&&anchorHighInside)||contained)other.enabled=false;}}}}return v;}
Monstey_NavSeamGroundResult Monstey_NavSeamMatchEdgePair(
    const Monstey_NavigationBoundaryMeshView&a,const Monstey_NavigationBoundaryMeshView&b,
    std::uint32_t faceA,std::uint32_t edgeA,std::uint32_t faceB,std::uint32_t edgeB,
    std::uint8_t flagsA,std::uint8_t flagsB,const V&up,const Monstey_NavSeamSettings&s){
  const bool alternateA=(flagsA&1u)!=0,alternateB=(flagsB&1u)!=0;
  const Monstey_NavSeamAlternateSettings alternate{s.ground.horizontal_gap,s.ground.plane_slack,s.ground.minimum_overlap,s.face_cosine,s.alternate_edge_cosine};
  if(!alternateA&&!alternateB)return Monstey_NavSeamGroundMatch(a,b,faceA,edgeA,faceB,edgeB,up,s.ground);
  if(alternateA&&alternateB)return Monstey_NavSeamAlternateMatch(a,b,faceA,edgeA,faceB,edgeB,alternate);
  const auto ground=Monstey_NavSeamGroundMatch(a,b,faceA,edgeA,faceB,edgeB,up,s.ground);
  if(ground.status!=-1)return ground;
  auto combined=Monstey_NavSeamAlternateMatch(a,b,faceA,edgeA,faceB,edgeB,alternate);
  if(combined.status!=-1)return combined;
  combined.candidate.edge_a=ground.candidate.edge_a;combined.candidate.edge_b=ground.candidate.edge_b;
  combined.candidate.face_a=ground.candidate.face_a;combined.candidate.face_b=ground.candidate.face_b;
  combined.candidate.enabled=ground.candidate.enabled;
  combined.candidate.score=(combined.candidate.score+ground.candidate.score)*.5f;
  combined.candidate.horizontal_cosine=(combined.candidate.horizontal_cosine+ground.candidate.horizontal_cosine)*.5f;
  combined.candidate.overlap=(combined.candidate.overlap+ground.candidate.overlap)*.5f;
  for(unsigned i=0;i<4;i+=2){combined.candidate.intervals[i]=NativeMax(ground.candidate.intervals[i],combined.candidate.intervals[i]);combined.candidate.intervals[i+1]=NativeMin(ground.candidate.intervals[i+1],combined.candidate.intervals[i+1]);}
  return combined;
}
Monstey_NavSeamGroundMatchResult Monstey_NavSeamMatchGroundMeshes(
    const Monstey_NavigationBoundaryMeshView&a,const Monstey_NavigationBoundaryMeshView&b,
    const Monstey_NavigationAabb&boundsA,const Monstey_NavigationAabb&boundsB,
    const Monstey_NavigationCompactTreeView&treeA,const Monstey_NavigationCompactTreeView&treeB,
    const V&up,float tolerance,const Monstey_NavSeamGroundSettings&settings){
  // Both bit0 values are clear, so these alternate thresholds are never consumed.
  return Monstey_NavSeamMatchMeshes(a,b,boundsA,boundsB,treeA,treeB,0,0,up,tolerance,{settings,0,0});
}
Monstey_NavSeamGroundMatchResult Monstey_NavSeamMatchMeshes(
    const Monstey_NavigationBoundaryMeshView&a,const Monstey_NavigationBoundaryMeshView&b,
    const Monstey_NavigationAabb&boundsA,const Monstey_NavigationAabb&boundsB,
    const Monstey_NavigationCompactTreeView&treeA,const Monstey_NavigationCompactTreeView&treeB,
    std::uint8_t flagsA,std::uint8_t flagsB,const V&up,float tolerance,const Monstey_NavSeamSettings&settings){
  Monstey_NavSeamGroundMatchResult result{};
  const auto magnitude=_mm_andnot_ps(_mm_set1_ps(-0.0f),_mm_loadu_ps(up.data()));
  auto maximum=_mm_max_ps(_mm_shuffle_ps(magnitude,magnitude,0x55),_mm_shuffle_ps(magnitude,magnitude,0));
  maximum=_mm_max_ps(_mm_shuffle_ps(magnitude,magnitude,0xaa),maximum);
  const auto mask=static_cast<unsigned>(_mm_movemask_ps(_mm_cmple_ps(maximum,magnitude)))&7u;
  constexpr std::array<unsigned,8>axis{0,0,1,1,2,2,2,2};
  const auto expansion=[&](std::uint8_t flags){V value{settings.ground.horizontal_gap,settings.ground.horizontal_gap,settings.ground.horizontal_gap,settings.ground.horizontal_gap};if(flags&1u){const float largest=_mm_cvtss_f32(_mm_max_ss(_mm_set_ss(settings.ground.horizontal_gap),_mm_set_ss(settings.ground.step_height)));value={largest,largest,largest,largest};}else value[axis[mask]]=settings.ground.step_height;V resultValue{};_mm_storeu_ps(resultValue.data(),_mm_mul_ps(_mm_loadu_ps(value.data()),_mm_set1_ps(.5f)));return resultValue;};
  const V halfA=expansion(flagsA),halfB=expansion(flagsB);V edgePaddingA{},edgePaddingB{};
  _mm_storeu_ps(edgePaddingA.data(),_mm_add_ps(_mm_loadu_ps(halfA.data()),_mm_set1_ps(tolerance)));
  _mm_storeu_ps(edgePaddingB.data(),_mm_add_ps(_mm_loadu_ps(halfB.data()),_mm_set1_ps(tolerance)));
  auto worldA=Monstey_NavigationTransformedAabb(a.transform,boundsA),worldB=Monstey_NavigationTransformedAabb(b.transform,boundsB);
  _mm_storeu_ps(worldA.minimum.data(),_mm_sub_ps(_mm_loadu_ps(worldA.minimum.data()),_mm_loadu_ps(halfA.data())));
  _mm_storeu_ps(worldA.maximum.data(),_mm_add_ps(_mm_loadu_ps(worldA.maximum.data()),_mm_loadu_ps(halfA.data())));
  _mm_storeu_ps(worldB.minimum.data(),_mm_sub_ps(_mm_loadu_ps(worldB.minimum.data()),_mm_loadu_ps(halfB.data())));
  _mm_storeu_ps(worldB.maximum.data(),_mm_add_ps(_mm_loadu_ps(worldB.maximum.data()),_mm_loadu_ps(halfB.data())));
  result.boundary_a=Monstey_NavigationBoundaryQuery(a,treeA,worldB,edgePaddingA);
  result.boundary_b=Monstey_NavigationBoundaryQuery(b,treeB,worldA,edgePaddingB);
  std::vector<Monstey_NavSeamQuantizedAabb>boxesA,boxesB;for(const auto&q:result.boundary_a.boundary_boxes)boxesA.push_back({q.minimum,q.maximum});for(const auto&q:result.boundary_b.boundary_boxes)boxesB.push_back({q.minimum,q.maximum});
  result.broadphase=Monstey_NavSeamBroadphase(boxesA,boxesB);
  std::set<std::uint32_t>usedA,usedB;
  const float minimumLength=(tolerance+tolerance)*(tolerance+tolerance);
  auto hit=[&](const auto&query,std::uint32_t i){Monstey_Require(i<query.hits.size(),"Native seam hit ordinal outside owned hit array");auto value=query.hits[i];value.face_key&=Mask;value.edge_key&=Mask;return value;};
  for(const auto&p:result.broadphase){const auto ha=hit(result.boundary_a,p.first),hb=hit(result.boundary_b,p.second);if(usedA.contains(ha.edge_key)||usedB.contains(hb.edge_key))continue;const auto ea=Monstey_NavigationBoundaryEndpoints(a,ha.edge_key),eb=Monstey_NavigationBoundaryEndpoints(b,hb.edge_key);if(minimumLength>LengthSquared(ea.first,ea.second)||minimumLength>LengthSquared(eb.first,eb.second))continue;if(!Close(ea.first,eb.second,tolerance)||!Close(eb.first,ea.second,tolerance))continue;result.connections.push_back({ha.face_key,ha.edge_key,hb.face_key,hb.edge_key});usedA.insert(ha.edge_key);usedB.insert(hb.edge_key);}
  result.coincident_connections=static_cast<std::uint32_t>(result.connections.size());
  std::vector<C>candidates;for(const auto&p:result.broadphase){const auto ha=hit(result.boundary_a,p.first),hb=hit(result.boundary_b,p.second);if(usedA.contains(ha.edge_key)||usedB.contains(hb.edge_key))continue;auto matched=Monstey_NavSeamMatchEdgePair(a,b,ha.face_key,ha.edge_key,hb.face_key,hb.edge_key,flagsA,flagsB,up,settings);if(matched.status!=-1)continue;matched.candidate.edge_a&=Mask;matched.candidate.face_a&=Mask;matched.candidate.edge_b=(matched.candidate.edge_b&Mask)|0x400000u;matched.candidate.face_b=(matched.candidate.face_b&Mask)|0x400000u;candidates.push_back(matched.candidate);}
  result.candidates=Monstey_NavSeamResolveCandidates(candidates);for(const auto&c:result.candidates)if(c.enabled)result.connections.push_back({c.face_a&Mask,c.edge_a&Mask,c.face_b&Mask,c.edge_b&Mask});
  return result;
}
}