#include "Monstey_NavigationBoundaryQuery.hpp"
#include "Monstey_Core.hpp"
#include <bit>
#include <cstring>
#include <immintrin.h>
#include <limits>
namespace Monstey {
namespace {
using V=Monstey_NavigationVector4;using B=Monstey_NavigationAabb;using T=Monstey_NavigationAffine4;
constexpr std::uint32_t IndexMask=0x3fffffu;
__m128 Load(const V& v) noexcept{return _mm_loadu_ps(v.data());}
V Store(__m128 v) noexcept{V out;_mm_storeu_ps(out.data(),v);return out;}
V Vector(std::span<const std::uint8_t> bytes,std::size_t index){Monstey_Require(index<bytes.size()/16,"Boundary vertex index outside owned bytes");V value;std::memcpy(value.data(),bytes.data()+index*16,16);return value;}
bool Overlap(const B&a,const B&b) noexcept{return (_mm_movemask_ps(_mm_and_ps(_mm_cmple_ps(Load(a.minimum),Load(b.maximum)),_mm_cmple_ps(Load(b.minimum),Load(a.maximum))))&7)==7;}
std::uint32_t Payload(std::span<const std::uint8_t> n){return (static_cast<std::uint32_t>(n[3]&0x7fu)<<16)|Monstey_U16(n,4);}
B Decode(const B&parent,std::span<const std::uint8_t> node){
    const auto scale=_mm_set1_ps(std::bit_cast<float>(std::uint32_t{0x3b90fdbc}));
    const auto extent=_mm_mul_ps(_mm_sub_ps(Load(parent.maximum),Load(parent.minimum)),scale);
    const auto upper=_mm_set_epi32(static_cast<int>(node[3]>>4),static_cast<int>(node[2]>>4),static_cast<int>(node[1]>>4),static_cast<int>(node[0]>>4));
    const auto lower=_mm_set_epi32(0,static_cast<int>(node[2]&15),static_cast<int>(node[1]&15),static_cast<int>(node[0]&15));
    const auto up=_mm_cvtepi32_ps(upper),down=_mm_cvtepi32_ps(lower);
    return {Store(_mm_add_ps(_mm_mul_ps(_mm_mul_ps(up,up),extent),Load(parent.minimum))),Store(_mm_sub_ps(Load(parent.maximum),_mm_mul_ps(_mm_mul_ps(down,down),extent)))};
}
void Stride(std::span<const std::uint8_t> b,std::size_t size,const char* error){Monstey_Require(b.size()%size==0,error);Monstey_Require(b.size()/size<=static_cast<std::size_t>(INT32_MAX),"Boundary array count exceeds signed native count");}
std::span<const std::uint8_t> SelectRecord(std::uint32_t index,std::span<const std::uint8_t> original,std::span<const std::int32_t> map,std::span<const std::uint8_t> mapped,std::span<const std::uint8_t> added,std::size_t stride){
    const auto originalCount=original.size()/stride;
    if(index>=originalCount){const auto offset=static_cast<std::size_t>(index)-originalCount;Monstey_Require(offset<added.size()/stride,"Boundary added record index outside owned bytes");return added.subspan(offset*stride,stride);}
    std::int32_t target=static_cast<std::int32_t>(index);
    if(!map.empty()){Monstey_Require(index<map.size(),"Boundary map index outside owned bytes");target=map[index];}
    if(target==-1)return original.subspan(static_cast<std::size_t>(index)*stride,stride);
    Monstey_Require(target>=0 && static_cast<std::size_t>(target)<mapped.size()/stride,"Boundary mapped record index outside owned bytes");
    return mapped.subspan(static_cast<std::size_t>(target)*stride,stride);
}
V SelectVertex(const Monstey_NavigationBoundaryMeshView&m,std::int32_t index){Monstey_Require(index>=0,"Boundary vertex is negative");const auto n=static_cast<std::size_t>(index);return n<m.vertices.size()/16?Vector(m.vertices,n):Vector(m.added_vertices,n-m.vertices.size()/16);}
V WorldVertex(const T&t,const V&v) noexcept{
    const auto value=Load(v);auto sum=_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(value,value,0x55),Load(t[1])),_mm_mul_ps(_mm_shuffle_ps(value,value,0x00),Load(t[0])));
    sum=_mm_add_ps(sum,_mm_mul_ps(_mm_shuffle_ps(value,value,0xaa),Load(t[2])));sum=_mm_add_ps(sum,Load(t[3]));return Store(sum);
}
std::pair<V,V> Endpoints(const Monstey_NavigationBoundaryMeshView&m,std::uint32_t index){const auto edge=SelectRecord(index,m.edges,m.edge_map,m.mapped_edges,m.added_edges,20);return {WorldVertex(m.transform,SelectVertex(m,std::bit_cast<std::int32_t>(Monstey_U32(edge,0)))),WorldVertex(m.transform,SelectVertex(m,std::bit_cast<std::int32_t>(Monstey_U32(edge,4))))};}
bool NativeIdentity(const T&t) noexcept{
    const T identity{{V{1,0,0,0},V{0,1,0,0},V{0,0,1,0},V{0,0,0,1}}};
    const auto sign=_mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u)));
    auto a=_mm_andnot_ps(sign,_mm_sub_ps(Load(identity[3]),Load(t[3])));
    auto b=_mm_andnot_ps(sign,_mm_sub_ps(Load(identity[2]),Load(t[2])));b=_mm_max_ps(b,a);
    auto c=_mm_andnot_ps(sign,_mm_sub_ps(Load(identity[1]),Load(t[1])));
    auto d=_mm_andnot_ps(sign,_mm_sub_ps(Load(identity[0]),Load(t[0])));d=_mm_max_ps(d,c);d=_mm_max_ps(d,b);
    return (_mm_movemask_ps(_mm_cmple_ps(d,_mm_setzero_ps()))&7)==7;
}
B LocalQuery(const T&t,const B&q) noexcept{
    if(NativeIdentity(t))return q;
    const auto c0=Load(t[0]),c1=Load(t[1]),c2=Load(t[2]);
    const auto lo=_mm_unpacklo_ps(c0,c1),hi=_mm_unpackhi_ps(c0,c1);
    const auto x=_mm_movelh_ps(lo,c2);
    const auto z=_mm_shuffle_ps(hi,c2,0xe4);
    const auto y=_mm_shuffle_ps(_mm_movehl_ps(x,lo),c2,0xd4);
    const auto sign=_mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u)));
    const auto translation=_mm_xor_ps(Load(t[3]),sign);
    auto inverseTranslation=_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(translation,translation,0x55),y),_mm_mul_ps(_mm_shuffle_ps(translation,translation,0x00),x));
    inverseTranslation=_mm_add_ps(inverseTranslation,_mm_mul_ps(_mm_shuffle_ps(translation,translation,0xaa),z));
    return Monstey_NavigationTransformedAabb(T{{Store(x),Store(y),Store(z),Store(inverseTranslation)}},q);
}
}
std::span<const std::uint8_t> Monstey_NavigationBoundaryFace(const Monstey_NavigationBoundaryMeshView&m,std::uint32_t index){for(const auto b:{m.faces,m.mapped_faces,m.added_faces})Stride(b,16,"Boundary face records must be 16 bytes");return SelectRecord(index,m.faces,m.face_map,m.mapped_faces,m.added_faces,16);}
std::span<const std::uint8_t> Monstey_NavigationBoundaryEdge(const Monstey_NavigationBoundaryMeshView&m,std::uint32_t index){for(const auto b:{m.edges,m.mapped_edges,m.added_edges})Stride(b,20,"Boundary edge records must be 20 bytes");return SelectRecord(index,m.edges,m.edge_map,m.mapped_edges,m.added_edges,20);}
V Monstey_NavigationBoundaryWorldVertex(const Monstey_NavigationBoundaryMeshView&m,std::int32_t index){Stride(m.vertices,16,"Boundary vertex records must be 16 bytes");Stride(m.added_vertices,16,"Boundary added vertex records must be 16 bytes");return WorldVertex(m.transform,SelectVertex(m,index));}
std::pair<V,V> Monstey_NavigationBoundaryEndpoints(const Monstey_NavigationBoundaryMeshView&m,std::uint32_t index){const auto edge=Monstey_NavigationBoundaryEdge(m,index);return {Monstey_NavigationBoundaryWorldVertex(m,std::bit_cast<std::int32_t>(Monstey_U32(edge,0))),Monstey_NavigationBoundaryWorldVertex(m,std::bit_cast<std::int32_t>(Monstey_U32(edge,4)))};}
std::vector<std::uint32_t> Monstey_NavigationCompactTreeQuery(const Monstey_NavigationCompactTreeView&tree,const B&query){
    Stride(tree.nodes,6,"Compact navigation tree nodes must be six-byte records");
    std::vector<std::uint32_t> out;if(tree.nodes.empty()||!Overlap(tree.bounds,query))return out;
    struct Pending{std::size_t index;B bounds;};std::vector<Pending> pending;pending.push_back({0,tree.bounds});
    while(!pending.empty()){
        const auto current=pending.back();pending.pop_back();Monstey_Require(current.index<tree.nodes.size()/6,"Compact navigation node outside owned bytes");
        const auto node=tree.nodes.subspan(current.index*6,6);const auto payload=Payload(node);
        if((node[3]&0x80u)==0){out.push_back(payload);continue;}
        const auto left=current.index+1;const auto right=current.index+static_cast<std::size_t>(payload)*2;
        Monstey_Require(left<tree.nodes.size()/6 && right>current.index && right<tree.nodes.size()/6,"Compact navigation child outside owned bytes");
        const B leftBounds=Decode(current.bounds,tree.nodes.subspan(left*6,6));
        const B rightBounds=Decode(current.bounds,tree.nodes.subspan(right*6,6));
        if(Overlap(rightBounds,query))pending.push_back({right,rightBounds});
        if(Overlap(leftBounds,query))pending.push_back({left,leftBounds});
    }
    return out;
}
bool Monstey_NavigationSegmentIntersectsAabb(const V&a,const V&b,const B&q) noexcept{
    auto direction=_mm_sub_ps(Load(b),Load(a));
    const auto ones=_mm_set1_ps(1.0f);const auto upper=_mm_unpackhi_ps(direction,ones);direction=_mm_shuffle_ps(direction,upper,0xc4);
    const auto zero=_mm_setzero_ps();direction=_mm_andnot_ps(_mm_cmpeq_ps(zero,direction),direction);
    const auto isZero=_mm_cmpeq_ps(zero,direction);const auto approximation=_mm_rcp_ps(direction);
    const auto refined=_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(2.0f),_mm_mul_ps(approximation,direction)),approximation);
    auto reciprocal=_mm_or_ps(_mm_andnot_ps(isZero,refined),_mm_and_ps(isZero,_mm_set1_ps(std::bit_cast<float>(std::uint32_t{0x7f7fffee}))));
    const auto signs=static_cast<std::uint32_t>(_mm_movemask_ps(_mm_cmple_ps(zero,direction))&7);
    auto reciprocalBits=_mm_castps_si128(reciprocal);reciprocalBits=_mm_insert_epi16(reciprocalBits,static_cast<int>(signs),6);reciprocalBits=_mm_insert_epi16(reciprocalBits,0x3f00,7);reciprocal=_mm_castsi128_ps(reciprocalBits);
    const auto nearT=_mm_mul_ps(_mm_sub_ps(Load(q.minimum),Load(a)),reciprocal);
    const auto farT=_mm_mul_ps(_mm_sub_ps(Load(q.maximum),Load(a)),reciprocal);
    const auto low=_mm_min_ps(nearT,farT),high=_mm_max_ps(nearT,farT);
    auto entry=_mm_max_ps(_mm_shuffle_ps(low,low,0x55),_mm_shuffle_ps(low,low,0x00));entry=_mm_max_ps(_mm_shuffle_ps(low,low,0xaa),entry);entry=_mm_max_ps(entry,zero);
    auto exit=_mm_min_ps(_mm_shuffle_ps(high,high,0x55),_mm_shuffle_ps(high,high,0x00));exit=_mm_min_ps(_mm_shuffle_ps(high,high,0xaa),exit);exit=_mm_min_ps(exit,ones);
    return _mm_cvtss_f32(exit)>=_mm_cvtss_f32(entry);
}
Monstey_NavigationOrderedAabb Monstey_NavigationEncodeOrderedAabb(const B&box,std::uint32_t hitIndex) noexcept{
    Monstey_NavigationOrderedAabb result{};
    const auto encode=[](float value){const auto bits=std::bit_cast<std::uint32_t>(value);const auto mask=(bits&0x80000000u)?0xffffffffu:0x80000000u;return (bits^mask)>>1;};
    for(unsigned lane=0;lane<4;++lane){result.minimum[lane]=encode(box.minimum[lane]);result.maximum[lane]=encode(box.maximum[lane])+1u;}
    result.minimum[3]=hitIndex;return result;
}
Monstey_NavigationBoundaryQueryResult Monstey_NavigationBoundaryQuery(const Monstey_NavigationBoundaryMeshView&m,const Monstey_NavigationCompactTreeView&tree,const B&worldQuery,const V&expansion){
    for(const auto b:{m.faces,m.mapped_faces,m.added_faces})Stride(b,16,"Boundary face records must be 16 bytes");
    for(const auto b:{m.edges,m.mapped_edges,m.added_edges})Stride(b,20,"Boundary edge records must be 20 bytes");
    Stride(m.vertices,16,"Boundary vertex records must be 16 bytes");Stride(m.added_vertices,16,"Boundary added vertex records must be 16 bytes");
    Monstey_NavigationBoundaryQueryResult result;result.local_query=LocalQuery(m.transform,worldQuery);result.raw_tree_leaves=Monstey_NavigationCompactTreeQuery(tree,result.local_query);
    for(const auto leaf:result.raw_tree_leaves){const auto faceKey=leaf&IndexMask;result.masked_face_keys.push_back(faceKey);const auto face=SelectRecord(faceKey,m.faces,m.face_map,m.mapped_faces,m.added_faces,16);const auto begin=std::bit_cast<std::int32_t>(Monstey_U32(face,0));const auto count=std::bit_cast<std::int16_t>(Monstey_U16(face,8));const auto end=static_cast<std::int64_t>(begin)+count;Monstey_Require(end>=INT32_MIN&&end<=INT32_MAX,"Boundary face edge range overflows signed native addition");
        for(std::int64_t i=begin;i<end;++i){Monstey_Require(i>=0,"Boundary face first edge is negative");const auto edge=static_cast<std::uint32_t>(i);const auto [a,b]=Endpoints(m,edge);if(Monstey_NavigationSegmentIntersectsAabb(a,b,worldQuery))result.hits.push_back({faceKey,edge});}
    }
    for(std::size_t i=0;i<result.hits.size();++i){const auto edgeKey=result.hits[i].edge_key&IndexMask;const auto edge=SelectRecord(edgeKey,m.edges,m.edge_map,m.mapped_edges,m.added_edges,20);if(Monstey_U32(edge,8)!=0xffffffffu)continue;const auto [a,b]=Endpoints(m,edgeKey);const auto low=_mm_sub_ps(_mm_min_ps(Load(a),Load(b)),Load(expansion));const auto high=_mm_add_ps(_mm_max_ps(Load(a),Load(b)),Load(expansion));Monstey_Require(i<=static_cast<std::size_t>(UINT32_MAX),"Boundary hit ordinal exceeds native width");result.boundary_boxes.push_back(Monstey_NavigationEncodeOrderedAabb(B{Store(low),Store(high)},static_cast<std::uint32_t>(i)));}
    return result;
}
}