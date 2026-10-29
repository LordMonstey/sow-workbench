#include "Monstey_NavSeamAlternate.hpp"
#include "Monstey_Core.hpp"
#include <bit>
#include <immintrin.h>
namespace Monstey {
namespace {
using V=Monstey_NavSeamVector4;using M=Monstey_NavigationBoundaryMeshView;
__m128 Monstey_AltLoad(const V&v){return _mm_loadu_ps(v.data());}
V Monstey_AltStore(__m128 v){V out{};_mm_storeu_ps(out.data(),v);return out;}
float Monstey_AltLow(__m128 v){return _mm_cvtss_f32(v);}
__m128 Monstey_AltDot(__m128 a,__m128 b){const auto p=_mm_mul_ps(a,b);return _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p,p,0x55),_mm_shuffle_ps(p,p,0)),_mm_shuffle_ps(p,p,0xaa));}
__m128 Monstey_AltReverseCross(__m128 a,__m128 b){const auto value=_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(a,a,0xc9),b),_mm_mul_ps(_mm_shuffle_ps(b,b,0xc9),a));return _mm_shuffle_ps(value,value,0xc9);}
__m128 Monstey_AltNormal(__m128 v){const auto len=Monstey_AltDot(v,v),estimate=_mm_rsqrt_ps(len);auto factor=_mm_mul_ps(estimate,len);factor=_mm_mul_ps(factor,estimate);factor=_mm_sub_ps(_mm_set1_ps(3),factor);factor=_mm_mul_ps(factor,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));return _mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len,_mm_setzero_ps()),factor),v);}
__m128 Monstey_AltLength(__m128 v){const auto len=Monstey_AltDot(v,v),estimate=_mm_rsqrt_ps(len);auto value=_mm_mul_ps(estimate,len);value=_mm_mul_ps(value,estimate);value=_mm_sub_ps(_mm_set1_ps(3),value);value=_mm_mul_ps(value,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));value=_mm_mul_ps(value,len);return _mm_andnot_ps(_mm_cmple_ps(len,_mm_setzero_ps()),value);}
__m128 Monstey_AltAbs(__m128 v){return _mm_andnot_ps(_mm_set1_ps(-0.0f),v);}
__m128 Monstey_AltClamp(__m128 v){const auto one=_mm_set1_ps(1),mask=_mm_cmplt_ps(v,one);return _mm_max_ps(_mm_setzero_ps(),_mm_or_ps(_mm_and_ps(v,mask),_mm_andnot_ps(mask,one)));}
__m128 Monstey_AltInverse(__m128 v){const auto estimate=_mm_rcp_ps(v);return _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2),_mm_mul_ps(estimate,v)),estimate);}
__m128 Monstey_AltMidDistance(__m128 point,__m128 start,__m128 reverseDirection){const auto difference=_mm_sub_ps(start,point);const auto parameter=Monstey_AltClamp(_mm_mul_ps(Monstey_AltInverse(Monstey_AltDot(reverseDirection,reverseDirection)),Monstey_AltDot(difference,reverseDirection)));return Monstey_AltLength(_mm_sub_ps(difference,_mm_mul_ps(parameter,reverseDirection)));}
__m128 Monstey_AltPlane(__m128 normal,__m128 point){const auto distance=_mm_sub_ps(_mm_setzero_ps(),Monstey_AltDot(normal,point));return _mm_shuffle_ps(normal,_mm_unpackhi_ps(normal,distance),0xc4);}
__m128 Monstey_AltPlaneValue(__m128 point,__m128 plane){const auto p=_mm_mul_ps(point,plane);const auto xyzW=_mm_shuffle_ps(p,_mm_unpackhi_ps(p,plane),0xc4);const auto sum=_mm_add_ps(_mm_shuffle_ps(xyzW,xyzW,0x4e),xyzW);return _mm_add_ps(_mm_shuffle_ps(sum,sum,0xb1),sum);}
}
Monstey_NavSeamFaceNormalResult Monstey_NavSeamFaceNormal(const M&m,std::uint32_t face){
  const auto record=Monstey_NavigationBoundaryFace(m,face);
  const auto first=std::bit_cast<std::int32_t>(Monstey_U32(record,0));
  const auto count=std::bit_cast<std::int16_t>(Monstey_U16(record,8));
  Monstey_Require(first>=0,"Native face-normal first edge is negative");
  const auto initialEdge=Monstey_NavigationBoundaryEdge(m,static_cast<std::uint32_t>(first));
  const auto initialVertex=std::bit_cast<std::int32_t>(Monstey_U32(initialEdge,0));
  const auto origin=Monstey_AltLoad(Monstey_NavigationBoundaryWorldVertex(m,initialVertex));auto sum=_mm_setzero_ps();
  for(std::int32_t i=0;i<count;++i){Monstey_Require(static_cast<std::uint64_t>(first)+static_cast<std::uint32_t>(i)<=0x7fffffffu,"Native face-normal edge count overflows signed index");const auto edge=Monstey_NavigationBoundaryEndpoints(m,static_cast<std::uint32_t>(first+i));const auto a=_mm_sub_ps(Monstey_AltLoad(edge.first),origin),b=_mm_sub_ps(Monstey_AltLoad(edge.second),origin);sum=_mm_add_ps(sum,Monstey_AltReverseCross(b,a));}
  const auto len=Monstey_AltDot(sum,sum),estimate=_mm_rsqrt_ps(len),zero=_mm_setzero_ps();
  auto inverse=_mm_mul_ps(len,estimate);inverse=_mm_mul_ps(inverse,estimate);inverse=_mm_sub_ps(_mm_set1_ps(3),inverse);inverse=_mm_mul_ps(inverse,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));
  const auto positive=_mm_cmplt_ps(_mm_sub_ps(zero,len),zero);
  if(_mm_movemask_ps(positive)==0)return {V{0,0,1,0},V{0,0,0,0}};
  const auto unit=_mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len,zero),inverse),sum);
  const auto normal=_mm_or_ps(_mm_and_ps(unit,positive),_mm_andnot_ps(positive,sum));
  const auto estimateArea=_mm_rsqrt_ps(len);auto area=_mm_mul_ps(len,estimateArea);area=_mm_mul_ps(area,estimateArea);area=_mm_sub_ps(_mm_set1_ps(3),area);area=_mm_mul_ps(area,_mm_mul_ps(estimateArea,_mm_set1_ps(.5f)));area=_mm_mul_ps(area,len);area=_mm_andnot_ps(_mm_cmple_ps(len,zero),area);area=_mm_mul_ps(area,_mm_set1_ps(.5f));
  return {Monstey_AltStore(normal),Monstey_AltStore(area)};
}
Monstey_NavSeamGroundResult Monstey_NavSeamAlternateMatch(
    const M&a,const M&b,std::uint32_t faceA,std::uint32_t edgeA,
    std::uint32_t faceB,std::uint32_t edgeB,const Monstey_NavSeamAlternateSettings&s){
  Monstey_NavSeamGroundResult result{12,{}};
  const auto faceNormalA=Monstey_NavSeamFaceNormal(a,faceA),faceNormalB=Monstey_NavSeamFaceNormal(b,faceB);
  const auto fna=Monstey_AltLoad(faceNormalA.normal),fnb=Monstey_AltLoad(faceNormalB.normal);
  if(!(Monstey_AltLow(Monstey_AltDot(fna,fnb))>=s.face_cosine))return result;
  const auto endpointsA=Monstey_NavigationBoundaryEndpoints(a,edgeA),endpointsB=Monstey_NavigationBoundaryEndpoints(b,edgeB);
  const auto a0=Monstey_AltLoad(endpointsA.first),a1=Monstey_AltLoad(endpointsA.second),b0=Monstey_AltLoad(endpointsB.first),b1=Monstey_AltLoad(endpointsB.second);
  const auto deltaA=_mm_sub_ps(a1,a0),deltaB=_mm_sub_ps(b1,b0),normalA=Monstey_AltNormal(deltaA),normalB=Monstey_AltNormal(deltaB);
  // ABD350 removes the supplied projection vector before this angle test;
  // AB7ED0 supplies literal zero, leaving the full 3D directions unchanged.
  const auto projectedA=Monstey_AltNormal(_mm_add_ps(deltaA,_mm_mul_ps(_mm_sub_ps(_mm_setzero_ps(),Monstey_AltDot(_mm_setzero_ps(),deltaA)),_mm_setzero_ps())));
  const auto projectedB=Monstey_AltNormal(_mm_add_ps(deltaB,_mm_mul_ps(_mm_sub_ps(_mm_setzero_ps(),Monstey_AltDot(_mm_setzero_ps(),deltaB)),_mm_setzero_ps())));
  const float cosine=Monstey_AltLow(_mm_sub_ps(_mm_setzero_ps(),Monstey_AltDot(projectedA,projectedB)));
  if(!(cosine>=s.edge_cosine)){result.status=13;return result;}
  const auto overlap=Monstey_NavSeamProjectedOverlap({endpointsA.first,endpointsA.second,endpointsB.first,endpointsB.second},Monstey_AltStore(normalA),Monstey_AltStore(normalB),V{},s.minimum_overlap);
  if(overlap.status!=-1){result.status=overlap.status;return result;}
  const auto p0=Monstey_AltLoad(overlap.projections[0]),p1=Monstey_AltLoad(overlap.projections[1]),p2=Monstey_AltLoad(overlap.projections[2]),p3=Monstey_AltLoad(overlap.projections[3]),one=_mm_set1_ps(1),zero=_mm_setzero_ps();
  const auto inverseA=_mm_div_ps(one,_mm_sub_ps(p1,p0)),inverseB=_mm_div_ps(one,_mm_sub_ps(p3,p2));
  const float epsilon=std::bit_cast<float>(std::uint32_t{0x3a83126f});__m128 beginA=zero,beginB=zero,endA=one,endB=one;
  if(epsilon<=Monstey_AltLow(Monstey_AltAbs(_mm_sub_ps(p0,p2)))){if(Monstey_AltLow(p0)>Monstey_AltLow(p2))beginB=_mm_mul_ps(_mm_sub_ps(p0,p2),inverseB);else beginA=_mm_mul_ps(_mm_sub_ps(p2,p0),inverseA);}
  if(epsilon<=Monstey_AltLow(Monstey_AltAbs(_mm_sub_ps(p1,p3)))){if(Monstey_AltLow(p1)>Monstey_AltLow(p3))endA=_mm_mul_ps(_mm_sub_ps(p3,p0),inverseA);else endB=_mm_mul_ps(_mm_sub_ps(p1,p2),inverseB);}
  beginB=_mm_sub_ps(one,beginB);endB=_mm_sub_ps(one,endB);const auto swap=beginB;beginB=endB;endB=swap;
  if(Monstey_AltLow(beginA)>=Monstey_AltLow(endA)||Monstey_AltLow(beginB)>=Monstey_AltLow(endB)){result.status=5;return result;}
  const auto ca0=_mm_add_ps(_mm_mul_ps(deltaA,beginA),a0),ca1=_mm_add_ps(_mm_mul_ps(deltaA,endA),a0),cb0=_mm_add_ps(_mm_mul_ps(deltaB,beginB),b0),cb1=_mm_add_ps(_mm_mul_ps(deltaB,endB),b0);
  const auto closest=Monstey_NavSeamClosestSegments({Monstey_AltStore(ca0),Monstey_AltStore(ca1),Monstey_AltStore(cb0),Monstey_AltStore(cb1)});
  if(closest.squared_distance[0]>s.gap*s.gap){result.status=2;return result;}
  // AC26F0 is still executed with the caller's zero vector; no unit conversion.
  const auto project=[](__m128 v){return _mm_sub_ps(v,_mm_mul_ps(Monstey_AltDot(v,_mm_setzero_ps()),_mm_setzero_ps()));};
  const auto pa0=project(ca0),pa1=project(ca1),pb0=project(cb0),pb1=project(cb1);
  const auto midpointA=_mm_add_ps(_mm_mul_ps(_mm_sub_ps(pa1,pa0),_mm_set1_ps(.5f)),pa0),midpointB=_mm_add_ps(_mm_mul_ps(_mm_sub_ps(pb1,pb0),_mm_set1_ps(.5f)),pb0);
  const auto distanceA=Monstey_AltMidDistance(midpointA,pb0,_mm_sub_ps(pb0,pb1)),distanceB=Monstey_AltMidDistance(midpointB,pa0,_mm_sub_ps(pa0,pa1));
  const auto mean=_mm_mul_ps(_mm_add_ps(distanceB,distanceA),_mm_set1_ps(.5f));
  const auto planeA=Monstey_AltPlane(Monstey_AltNormal(Monstey_AltReverseCross(fna,normalA)),a0),planeB=Monstey_AltPlane(Monstey_AltNormal(Monstey_AltReverseCross(fnb,normalB)),b0);
  const float negativeSlack=Monstey_AltLow(_mm_sub_ps(zero,_mm_set1_ps(s.plane_slack)));
  if(negativeSlack>Monstey_AltLow(Monstey_AltPlaneValue(cb0,planeA))||negativeSlack>Monstey_AltLow(Monstey_AltPlaneValue(cb1,planeA))||negativeSlack>Monstey_AltLow(Monstey_AltPlaneValue(ca0,planeB))||negativeSlack>Monstey_AltLow(Monstey_AltPlaneValue(ca1,planeB))){result.status=7;return result;}
  result.status=-1;result.candidate={edgeA,edgeB,faceA,faceB,Monstey_AltLow(_mm_mul_ps(mean,mean)),cosine,overlap.overlap,true,{Monstey_AltLow(beginA),Monstey_AltLow(endA),Monstey_AltLow(beginB),Monstey_AltLow(endB)}};return result;
}
}