#include "Monstey_NavSeamGeometry.hpp"
#include <bit>
#include <cstdint>
#include <immintrin.h>
namespace Monstey {
namespace {
__m128 Monstey_SeamDot3(__m128 a,__m128 b) noexcept {
  const __m128 p=_mm_mul_ps(a,b);
  return _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(p,p,0x55),_mm_shuffle_ps(p,p,0)),_mm_shuffle_ps(p,p,0xaa));
}
__m128 Monstey_SeamClamp01(__m128 v) noexcept {
  const __m128 one=_mm_set1_ps(1.0f),test=_mm_cmplt_ps(v,one);
  return _mm_max_ps(_mm_setzero_ps(),_mm_or_ps(_mm_and_ps(v,test),_mm_andnot_ps(test,one)));
}
__m128 Monstey_SeamNormal(__m128 v) noexcept {
  const __m128 length=Monstey_SeamDot3(v,v),estimate=_mm_rsqrt_ps(length);
  __m128 value=_mm_mul_ps(length,estimate);
  value=_mm_mul_ps(value,estimate);
  value=_mm_sub_ps(_mm_set1_ps(3.0f),value);
  value=_mm_mul_ps(value,_mm_mul_ps(estimate,_mm_set1_ps(.5f)));
  value=_mm_andnot_ps(_mm_cmple_ps(length,_mm_setzero_ps()),value);
  return _mm_mul_ps(v,value);
}
void Monstey_SeamStore(Monstey_NavSeamVector4& out,__m128 value) noexcept{_mm_storeu_ps(out.data(),value);}
float Monstey_SeamLow(__m128 value) noexcept{return _mm_cvtss_f32(value);}
__m128 Monstey_SeamAbs(__m128 value) noexcept{return _mm_andnot_ps(_mm_set1_ps(-0.0f),value);}
}
Monstey_NavSeamClosestResult Monstey_NavSeamClosestSegments(
    const Monstey_NavSeamSegments& segments) noexcept {
  const __m128 a0=_mm_loadu_ps(segments[0].data()),a1=_mm_loadu_ps(segments[1].data());
  const __m128 b0=_mm_loadu_ps(segments[2].data()),b1=_mm_loadu_ps(segments[3].data());
  const __m128 between=_mm_sub_ps(b0,a0),a=_mm_sub_ps(a1,a0),b=_mm_sub_ps(b1,b0);
  const __m128 ab=Monstey_SeamDot3(a,b),ad=Monstey_SeamDot3(a,between),bd=Monstey_SeamDot3(between,b);
  const __m128 aa=Monstey_SeamDot3(a,a),bb=Monstey_SeamDot3(b,b),two=_mm_set1_ps(2.0f);
  const __m128 estimateB=_mm_rcp_ps(bb);
  const __m128 denominator=_mm_sub_ps(_mm_mul_ps(bb,aa),_mm_mul_ps(ab,ab));
  const __m128 estimateD=_mm_rcp_ps(denominator);
  const __m128 inverseB=_mm_mul_ps(_mm_sub_ps(two,_mm_mul_ps(bb,estimateB)),estimateB);
  const __m128 estimateA=_mm_rcp_ps(aa);
  const __m128 inverseA=_mm_mul_ps(_mm_sub_ps(two,_mm_mul_ps(estimateA,aa)),estimateA);
  const __m128 inverseD=_mm_andnot_ps(_mm_cmpeq_ps(_mm_setzero_ps(),denominator),
      _mm_mul_ps(_mm_sub_ps(two,_mm_mul_ps(denominator,estimateD)),estimateD));
  const __m128 numerator=_mm_sub_ps(_mm_mul_ps(bb,ad),_mm_mul_ps(bd,ab));
  const __m128 first=Monstey_SeamClamp01(_mm_mul_ps(inverseD,numerator));
  const __m128 second=Monstey_SeamClamp01(_mm_sub_ps(
      _mm_mul_ps(first,_mm_mul_ps(inverseB,ab)),_mm_mul_ps(inverseB,bd)));
  const __m128 corrected=Monstey_SeamClamp01(_mm_add_ps(
      _mm_mul_ps(_mm_mul_ps(inverseA,ab),second),_mm_mul_ps(inverseA,ad)));
  const __m128 delta=_mm_sub_ps(_mm_add_ps(_mm_mul_ps(corrected,a),a0),
      _mm_add_ps(_mm_mul_ps(second,b),b0));
  Monstey_NavSeamClosestResult result{};
  Monstey_SeamStore(result.delta,delta);
  Monstey_SeamStore(result.squared_distance,Monstey_SeamDot3(delta,delta));
  return result;
}
Monstey_NavSeamOverlapResult Monstey_NavSeamProjectedOverlap(
    const Monstey_NavSeamSegments& clipped,
    const Monstey_NavSeamVector4& normal_a,
    const Monstey_NavSeamVector4& normal_b,
    const Monstey_NavSeamVector4& up,
    float minimum_overlap) noexcept {
  const __m128 zero=_mm_setzero_ps(),vertical=_mm_loadu_ps(up.data());
  __m128 axis=_mm_mul_ps(_mm_sub_ps(_mm_loadu_ps(normal_a.data()),_mm_loadu_ps(normal_b.data())),_mm_set1_ps(.5f));
  axis=_mm_add_ps(axis,_mm_mul_ps(_mm_sub_ps(zero,Monstey_SeamDot3(vertical,axis)),vertical));
  axis=Monstey_SeamNormal(axis);
  std::array<__m128,4> p{};
  for(unsigned i=0;i<4;++i)p[i]=Monstey_SeamDot3(_mm_loadu_ps(clipped[i].data()),axis);
  if(Monstey_SeamLow(p[2])>Monstey_SeamLow(p[3])){const __m128 temporary=p[2];p[2]=p[3];p[3]=temporary;}
  Monstey_NavSeamOverlapResult result{3,{},0,false};
  for(unsigned i=0;i<4;++i)Monstey_SeamStore(result.projections[i],p[i]);
  const float epsilon=std::bit_cast<float>(std::uint32_t{0x358637bd});
  const __m128 firstLength=_mm_sub_ps(p[1],p[0]),secondLength=_mm_sub_ps(p[3],p[2]);
  if(Monstey_SeamLow(p[2])>Monstey_SeamLow(p[1])||Monstey_SeamLow(p[0])>Monstey_SeamLow(p[3])||
      epsilon>Monstey_SeamLow(firstLength)||epsilon>Monstey_SeamLow(secondLength))return result;
  const __m128 overlap=_mm_sub_ps(_mm_min_ps(p[1],p[3]),_mm_max_ps(p[0],p[2]));
  result.overlap=Monstey_SeamLow(overlap);
  result.overlap_written=true;
  if(result.overlap>=minimum_overlap){result.status=-1;return result;}
  const float fullEpsilon=std::bit_cast<float>(std::uint32_t{0x3727c5ac});
  const float firstDifference=Monstey_SeamLow(Monstey_SeamAbs(_mm_sub_ps(overlap,firstLength)));
  const float secondDifference=Monstey_SeamLow(Monstey_SeamAbs(_mm_sub_ps(overlap,secondLength)));
  result.status=fullEpsilon>firstDifference||fullEpsilon>secondDifference?-1:4;
  return result;
}
}