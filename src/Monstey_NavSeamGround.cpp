#include "Monstey_NavSeamGround.hpp"
#include "Monstey_Core.hpp"
#include <bit>
#include <cstring>
#include <immintrin.h>
namespace Monstey {
namespace {
using V=Monstey_NavSeamVector4;using M=Monstey_NavigationBoundaryMeshView;
__m128 Load(const V&v){return _mm_loadu_ps(v.data());}
V Store(__m128 v){V out{};_mm_storeu_ps(out.data(),v);return out;}
__m128 Dot(__m128 a,__m128 b){const auto p=_mm_mul_ps(a,b);return _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p,p,0x55),_mm_shuffle_ps(p,p,0)),_mm_shuffle_ps(p,p,0xaa));}
float Low(__m128 v){return _mm_cvtss_f32(v);}
__m128 Neg(__m128 v){return _mm_sub_ps(_mm_setzero_ps(),v);}
__m128 Abs(__m128 v){return _mm_andnot_ps(_mm_set1_ps(-0.0f),v);}
__m128 Normal(__m128 v){const auto len=Dot(v,v),estimate=_mm_rsqrt_ps(len);auto value=_mm_mul_ps(estimate,len);value=_mm_mul_ps(value,estimate);value=_mm_sub_ps(_mm_set1_ps(3),value);value=_mm_mul_ps(value,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));return _mm_mul_ps(_mm_andnot_ps(_mm_cmple_ps(len,_mm_setzero_ps()),value),v);}
__m128 Length(__m128 v){const auto len=Dot(v,v),estimate=_mm_rsqrt_ps(len);auto value=_mm_mul_ps(estimate,len);value=_mm_mul_ps(value,estimate);value=_mm_sub_ps(_mm_set1_ps(3),value);value=_mm_mul_ps(value,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));value=_mm_mul_ps(value,len);return _mm_andnot_ps(_mm_cmple_ps(len,_mm_setzero_ps()),value);}
__m128 Project(__m128 v,__m128 up){return _mm_sub_ps(v,_mm_mul_ps(Dot(v,up),up));}
__m128 Cross(__m128 a,__m128 b){return _mm_shuffle_ps(_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(a,a,0xc9),b),_mm_mul_ps(_mm_shuffle_ps(b,b,0xc9),a)),_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(a,a,0xc9),b),_mm_mul_ps(_mm_shuffle_ps(b,b,0xc9),a)),0xc9);}
__m128 Clamp(__m128 v){const auto one=_mm_set1_ps(1),mask=_mm_cmplt_ps(v,one);return _mm_max_ps(_mm_setzero_ps(),_mm_or_ps(_mm_and_ps(v,mask),_mm_andnot_ps(mask,one)));}
__m128 Inverse(__m128 v){const auto e=_mm_rcp_ps(v);return _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2),_mm_mul_ps(e,v)),e);}
__m128 Plane(__m128 normal,__m128 point){const auto distance=Neg(Dot(normal,point));const auto mixed=_mm_unpackhi_ps(normal,distance);return _mm_shuffle_ps(normal,mixed,0xc4);}
__m128 PlaneValue(__m128 point,__m128 plane){const auto product=_mm_mul_ps(point,plane);const auto mixed=_mm_unpackhi_ps(product,plane);const auto xyzW=_mm_shuffle_ps(product,mixed,0xc4);const auto sum=_mm_add_ps(_mm_shuffle_ps(xyzW,xyzW,0x4e),xyzW);return _mm_add_ps(_mm_shuffle_ps(sum,sum,0xb1),sum);}
__m128 WorldXFirst(const M&m,std::int32_t index){Monstey_Require(index>=0,"Seam support vertex index is negative");const auto original=m.vertices.size()/16;auto bytes=m.vertices;std::size_t selected=static_cast<std::size_t>(index);if(selected>=original){selected-=original;bytes=m.added_vertices;}Monstey_Require(selected<bytes.size()/16,"Seam support vertex outside owned bytes");V v{};std::memcpy(v.data(),bytes.data()+selected*16,16);const auto p=Load(v);auto result=_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(p,p,0),Load(m.transform[0])),_mm_mul_ps(_mm_shuffle_ps(p,p,0x55),Load(m.transform[1])));result=_mm_add_ps(result,_mm_mul_ps(_mm_shuffle_ps(p,p,0xaa),Load(m.transform[2])));return _mm_add_ps(result,Load(m.transform[3]));}
__m128 FaceSupport(const M&m,std::uint32_t face,__m128 plane,bool selectedComparison){const auto f=Monstey_NavigationBoundaryFace(m,face);const auto start=std::bit_cast<std::int32_t>(Monstey_U32(f,0));const auto count=std::bit_cast<std::int16_t>(Monstey_U16(f,8));auto maximum=Neg(_mm_set1_ps(std::bit_cast<float>(std::uint32_t{0x7f7fffee})));for(std::int32_t i=0;i<count;++i){Monstey_Require(start>=0&&static_cast<std::uint64_t>(start)+static_cast<std::uint32_t>(i)<=0x7fffffffu,"Seam support face edge outside signed range");const auto e=Monstey_NavigationBoundaryEdge(m,static_cast<std::uint32_t>(start+i));const auto value=PlaneValue(WorldXFirst(m,std::bit_cast<std::int32_t>(Monstey_U32(e,0))),plane);if(selectedComparison){const auto replace=_mm_cmplt_ps(maximum,value);maximum=_mm_or_ps(_mm_and_ps(replace,value),_mm_andnot_ps(replace,maximum));}else maximum=_mm_max_ps(maximum,value);}return maximum;}
__m128 MidpointDistance(__m128 point,__m128 start,__m128 reverseDirection){const auto difference=_mm_sub_ps(start,point);const auto t=Clamp(_mm_mul_ps(Inverse(Dot(reverseDirection,reverseDirection)),Dot(difference,reverseDirection)));return Length(_mm_sub_ps(difference,_mm_mul_ps(t,reverseDirection)));}
}
Monstey_NavSeamGroundResult Monstey_NavSeamGroundMatch(
    const M&a,const M&b,std::uint32_t faceA,std::uint32_t edgeA,
    std::uint32_t faceB,std::uint32_t edgeB,const V&upValue,
    const Monstey_NavSeamGroundSettings&s){
  Monstey_NavSeamGroundResult result{0,{}};
  const auto endpointsA=Monstey_NavigationBoundaryEndpoints(a,edgeA);
  const auto endpointsB=Monstey_NavigationBoundaryEndpoints(b,edgeB);
  const __m128 a0=Load(endpointsA.first),a1=Load(endpointsA.second),b0=Load(endpointsB.first),b1=Load(endpointsB.second),up=Load(upValue),one=_mm_set1_ps(1),zero=_mm_setzero_ps();
  const auto deltaA=_mm_sub_ps(a1,a0),deltaB=_mm_sub_ps(b1,b0);
  const auto normalA=Normal(deltaA),normalB=Normal(deltaB);
  const auto horizontalA=Normal(_mm_add_ps(_mm_mul_ps(Neg(Dot(deltaA,up)),up),deltaA));
  const auto horizontalB=Normal(_mm_add_ps(_mm_mul_ps(Neg(Dot(deltaB,up)),up),deltaB));
  const float cosine=Low(Neg(Dot(horizontalA,horizontalB)));
  if(!(cosine>=s.horizontal_cosine))return result;
  const auto cross=Normal(Cross(up,_mm_sub_ps(normalA,normalB)));
  const auto directionA=Normal(_mm_add_ps(_mm_mul_ps(Neg(Dot(cross,normalA)),cross),normalA));
  const auto directionB=Normal(_mm_add_ps(_mm_mul_ps(Neg(Dot(cross,normalB)),cross),normalB));
  if(!(Low(_mm_xor_ps(Dot(directionA,directionB),_mm_set1_ps(-0.0f)))>=s.direction_cosine)){result.status=1;return result;}
  const Monstey_NavSeamSegments original{endpointsA.first,endpointsA.second,endpointsB.first,endpointsB.second};
  const auto overlap=Monstey_NavSeamProjectedOverlap(original,Store(normalA),Store(normalB),upValue,s.minimum_overlap);
  if(overlap.status!=-1){result.status=overlap.status;return result;}
  const auto p0=Load(overlap.projections[0]),p1=Load(overlap.projections[1]),p2=Load(overlap.projections[2]),p3=Load(overlap.projections[3]);
  const auto inverseA=_mm_div_ps(one,_mm_sub_ps(p1,p0)),inverseB=_mm_div_ps(one,_mm_sub_ps(p3,p2));
  const float epsilon=std::bit_cast<float>(std::uint32_t{0x3a83126f});
  __m128 beginA=zero,beginB=zero,endA=one,endB=one;
  if(epsilon<=Low(Abs(_mm_sub_ps(p0,p2)))){if(Low(p0)>Low(p2))beginB=_mm_mul_ps(inverseB,_mm_sub_ps(p0,p2));else beginA=_mm_mul_ps(_mm_sub_ps(p2,p0),inverseA);}
  if(epsilon<=Low(Abs(_mm_sub_ps(p1,p3)))){if(Low(p1)>Low(p3))endA=_mm_mul_ps(_mm_sub_ps(p3,p0),inverseA);else endB=_mm_mul_ps(_mm_sub_ps(p1,p2),inverseB);}
  beginB=_mm_sub_ps(one,beginB);endB=_mm_sub_ps(one,endB);const auto swap=beginB;beginB=endB;endB=swap;
  if(Low(beginA)>=Low(endA)||Low(beginB)>=Low(endB)){result.status=5;return result;}
  const auto ca0=_mm_add_ps(_mm_mul_ps(deltaA,beginA),a0),ca1=_mm_add_ps(_mm_mul_ps(deltaA,endA),a0);
  const auto cb0=_mm_add_ps(_mm_mul_ps(deltaB,beginB),b0),cb1=_mm_add_ps(_mm_mul_ps(deltaB,endB),b0);
  const Monstey_NavSeamSegments clipped{Store(ca0),Store(ca1),Store(cb0),Store(cb1)};
  const auto closest=Monstey_NavSeamClosestSegments(clipped);
  if(Low(Abs(Dot(Load(closest.delta),up)))>s.step_height){result.status=6;return result;}
  const auto pa0=Project(ca0,up),pa1=Project(ca1,up),pb0=Project(cb0,up),pb1=Project(cb1,up);
  const auto horizontal=Monstey_NavSeamClosestSegments({Store(pa0),Store(pa1),Store(pb0),Store(pb1)});
  if(horizontal.squared_distance[0]>s.horizontal_gap*s.horizontal_gap){result.status=2;return result;}
  const auto midpointA=_mm_add_ps(_mm_mul_ps(_mm_sub_ps(pa1,pa0),_mm_set1_ps(.5f)),pa0);
  const auto midpointB=_mm_add_ps(_mm_mul_ps(_mm_sub_ps(pb1,pb0),_mm_set1_ps(.5f)),pb0);
  const auto firstDistance=MidpointDistance(midpointA,pb0,_mm_sub_ps(pb0,pb1));
  const auto secondDistance=MidpointDistance(midpointB,pa0,_mm_sub_ps(pa0,pa1));
  const auto mean=_mm_mul_ps(_mm_add_ps(secondDistance,firstDistance),_mm_set1_ps(.5f));
  const auto planeA=Plane(Normal(Cross(up,normalA)),a0),planeB=Plane(Normal(Cross(up,normalB)),b0);
  const float negativeSlack=Low(Neg(_mm_set1_ps(s.plane_slack)));
  if(negativeSlack>Low(PlaneValue(cb0,planeA))||negativeSlack>Low(PlaneValue(cb1,planeA))||negativeSlack>Low(PlaneValue(ca0,planeB))||negativeSlack>Low(PlaneValue(ca1,planeB))){result.status=7;return result;}
  if(s.face_support>Low(FaceSupport(b,faceB,planeA,false))||s.face_support>Low(FaceSupport(a,faceA,planeB,false))){result.status=8;return result;}
  if(s.face_support>0){
    const auto oppositeA=_mm_xor_ps(planeA,_mm_set1_ps(-0.0f)),oppositeB=_mm_xor_ps(planeB,_mm_set1_ps(-0.0f));
    if(!(Low(FaceSupport(a,faceA,oppositeA,true))>0)||!(Low(FaceSupport(b,faceB,oppositeB,true))>0)){result.status=9;return result;}
  }
  result.status=-1;
  result.candidate={edgeA,edgeB,faceA,faceB,Low(_mm_mul_ps(mean,mean)),cosine,overlap.overlap,true,{Low(beginA),Low(endA),Low(beginB),Low(endB)}};
  return result;
}
}