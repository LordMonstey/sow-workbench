#include "Monstey_StaticNavFaceBounds.hpp"
#include "Monstey_Core.hpp"
#include <bit>
#include <cstring>
#include <immintrin.h>
namespace Monstey {
Monstey_StaticNavFaceAabb Monstey_StaticNavFaceBoundsBaseline(
    std::span<const std::uint8_t> faces,
    std::span<const std::uint8_t> edges,
    std::span<const std::uint8_t> vertices,
    std::uint32_t face_index,
    const std::array<float,4>& padding) {
    Monstey_Require(faces.size()%16==0,"Static nav face bytes must be a multiple of 16");
    Monstey_Require(edges.size()%20==0,"Static nav edge bytes must be a multiple of 20");
    Monstey_Require(vertices.size()%16==0,"Static nav vertex bytes must be a multiple of 16");
    Monstey_Require(face_index<faces.size()/16,"Static nav face index outside owned bytes");
    const auto offset=static_cast<std::size_t>(face_index)*16;
    const auto first=std::bit_cast<std::int32_t>(Monstey_U32(faces,offset));
    const auto count=std::bit_cast<std::int16_t>(Monstey_U16(faces,offset+8));
    auto minimum=_mm_set1_ps(std::bit_cast<float>(std::uint32_t{0x7f7fffee}));
    auto maximum=_mm_xor_ps(minimum,_mm_set1_ps(-0.0f));
    std::uint32_t iterations=0;
    if(count>0) {
        Monstey_Require(first>=0,"Static nav first edge is negative");
        const auto begin=static_cast<std::size_t>(first);
        const auto length=static_cast<std::size_t>(count);
        Monstey_Require(begin<=edges.size()/20 && length<=edges.size()/20-begin,
            "Static nav edge range outside owned bytes");
        for(std::size_t i=0;i<length;++i) {
            const auto vertex=std::bit_cast<std::int32_t>(Monstey_U32(edges,(begin+i)*20));
            Monstey_Require(vertex>=0 && static_cast<std::size_t>(vertex)<vertices.size()/16,
                "Static nav start vertex outside owned bytes");
            std::array<float,4> lanes{};
            std::memcpy(lanes.data(),vertices.data()+static_cast<std::size_t>(vertex)*16,16);
            const auto value=_mm_loadu_ps(lanes.data());
            minimum=_mm_min_ps(minimum,value);
            maximum=_mm_max_ps(maximum,value);
            ++iterations;
        }
    }
    const auto expansion=_mm_loadu_ps(padding.data());
    minimum=_mm_sub_ps(minimum,expansion);
    maximum=_mm_add_ps(maximum,expansion);
    Monstey_StaticNavFaceAabb result{{},{},first,count,iterations};
    _mm_storeu_ps(result.minimum.data(),minimum);
    _mm_storeu_ps(result.maximum.data(),maximum);
    return result;
}
}
