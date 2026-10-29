#include "Monstey_SkelDefaultPose.hpp"
#include "Monstey_Model.hpp"
#include <bit>
#include <cmath>
namespace Monstey {
namespace {
using Monstey_Vec3 = std::array<float, 3>;
using Monstey_Vec4 = std::array<float, 4>;
using Monstey_Packed7 = std::array<float, 7>;
void Monstey_PoseRequire(bool value, const char* message) {
    if (!value) throw Monstey_Error("skel_default_pose", message);
}
// The scalar operation ordering is retained from the frozen native SKEL probe.
Monstey_Vec3 Monstey_ScalarRotate(const Monstey_Vec4& q, const Monstey_Vec3& t) {
    const float z=t[2], y=t[1], yx=q[0]*y, qy=q[1], qw=q[3], yy=y*qy;
    const float a=(float)((float)(t[0]*qw)+(float)(qy*z))-(float)(q[2]*y);
    const float b=(float)((float)(y*qw)-(float)(q[0]*z))+(float)(q[2]*t[0]);
    const float c=(float)(yx+(float)(qw*z))-(float)(t[0]*qy);
    const float ny=-qy, nx=-q[0], nz=-q[2];
    const float d=(float)((float)-(float)(q[0]*t[0])-yy)-(float)(q[2]*z);
    return {(float)((float)((float)(nx*d)+(float)(qw*a))+(float)(nz*b))-(float)(ny*c),
        (float)((float)((float)(ny*d)-(float)(nz*a))+(float)(qw*b))+(float)(nx*c),
        (float)((float)((float)(nz*d)+(float)(ny*a))-(float)(nx*b))+(float)(qw*c)};
}
Monstey_Packed7 Monstey_InvertRawBind(Monstey_Packed7 value) {
    const auto input=value;
    value[3]=-value[3]; value[4]=-value[4]; value[5]=-value[5];
    const auto p=Monstey_ScalarRotate({value[3],value[4],value[5],value[6]}, {input[0],input[1],input[2]});
    for (unsigned i=0;i<3;++i) value[i]=-p[i];
    return value;
}
Monstey_Vec4 Monstey_ScalarComposeQ(const Monstey_Vec4& p,const Monstey_Vec4& c) {
    return {(float)((float)((float)(c[0]*p[3])+(float)(p[0]*c[3]))+(float)(p[1]*c[2]))-(float)(p[2]*c[1]),
        (float)((float)((float)(c[1]*p[3])-(float)(c[2]*p[0]))+(float)(p[1]*c[3]))+(float)(p[2]*c[0]),
        (float)((float)((float)(c[2]*p[3])+(float)(c[1]*p[0]))-(float)(p[1]*c[0]))+(float)(p[2]*c[3]),
        (float)((float)((float)(p[3]*c[3])-(float)(c[0]*p[0]))-(float)(c[1]*p[1]))-(float)(p[2]*c[2])};
}
std::array<std::uint32_t,7> Monstey_Pack(const Monstey_Packed7& value) {
    std::array<std::uint32_t,7> words{};
    for (unsigned i=0;i<7;++i) {
        Monstey_PoseRequire(std::isfinite(value[i]), "Raw local-pose arithmetic is nonfinite");
        words[i]=std::bit_cast<std::uint32_t>(value[i]);
    }
    return words;
}
}
Monstey_SkelDefaultPose Monstey_SkelDefaultPoseRead(std::span<const std::uint8_t> bytes) {
    Monstey_PoseRequire(bytes.size()>=96 && Monstey_U32(bytes,0)==0x4c454b53u && Monstey_U32(bytes,4)==17,
                        "Expected SKEL17");
    const auto count=static_cast<std::size_t>(Monstey_U32(bytes,40));
    Monstey_PoseRequire(count>0 && count<=256, "Selected native default-pose profile needs 1..256 nodes");
    const auto metadata=Monstey_ModelInspectSkeleton(bytes);
    const auto& entries=metadata.at("entries").as_array();
    Monstey_PoseRequire(entries.size()==count && metadata.at("root_count").as_uint()==1,
                        "Selected native default-pose profile needs one complete root");
    const auto first=static_cast<std::size_t>(metadata.at("bone_data_offset").as_uint());
    Monstey_CheckRange(bytes, first, count*37);
    std::vector<std::vector<std::size_t>> children(count);
    std::vector<int> parents(count,-1);
    std::vector<Monstey_Packed7> raw(count), inverse(count);
    for (std::size_t i=0;i<count;++i) {
        const auto parent=entries[i].at("parent_source").as_int();
        Monstey_PoseRequire(i==0 ? parent==-1 : parent>=0 && static_cast<std::size_t>(parent)<i,
                            "SKEL root or preorder parent mismatch");
        parents[i]=static_cast<int>(parent);
        if (parent>=0) children[static_cast<std::size_t>(parent)].push_back(i);
        for (unsigned j=0;j<7;++j) {
            raw[i][j]=std::bit_cast<float>(Monstey_U32(bytes,first+37*i+5+4*j));
            Monstey_PoseRequire(std::isfinite(raw[i][j]), "SKEL raw global pose contains nonfinite lanes");
        }
        inverse[i]=Monstey_InvertRawBind(raw[i]);
    }
    Monstey_SkelDefaultPose result;
    result.source_sha256=metadata.at("source_sha256").as_string();
    result.nodes.resize(count); result.hk_to_source.resize(count); result.source_to_hk.resize(count);
    std::size_t next=1;
    const auto visit=[&](auto&& self,std::size_t source,std::size_t hk,int parentHk)->void {
        auto& node=result.nodes[hk]; node.hk_index=hk; node.source_index=source; node.parent_hk=parentHk;
        node.name=entries[source].at("name").as_string(); node.name_hash=Monstey_NativeHash(node.name);
        node.raw_flag_byte=bytes[first+37*source+4];
        result.hk_to_source[hk]=source; result.source_to_hk[source]=hk;
        node.global_words=Monstey_Pack(raw[source]); node.inverse_words=Monstey_Pack(inverse[source]);
        const Monstey_Packed7 identity{0,0,0,0,0,0,1};
        const auto& pi=parents[source]<0 ? identity : inverse[static_cast<std::size_t>(parents[source])];
        const auto p=Monstey_ScalarRotate({pi[3],pi[4],pi[5],pi[6]}, {raw[source][0],raw[source][1],raw[source][2]});
        const auto q=Monstey_ScalarComposeQ({pi[3],pi[4],pi[5],pi[6]}, {raw[source][3],raw[source][4],raw[source][5],raw[source][6]});
        node.local_words=Monstey_Pack({p[0]+pi[0],p[1]+pi[1],p[2]+pi[2],q[0],q[1],q[2],q[3]});
        for (unsigned j=0;j<3;++j) {
            node.default_pose_words[j]=node.local_words[j]; node.inverse_pose_words[j]=node.inverse_words[j];
        }
        node.default_pose_words[3]=0; node.inverse_pose_words[3]=0x3f800000u;
        for (unsigned j=0;j<4;++j) {
            node.default_pose_words[j+4]=node.local_words[j+3]; node.inverse_pose_words[j+4]=node.inverse_words[j+3];
        }
        const auto childFirst=next; next+=children[source].size();
        Monstey_PoseRequire(next<=count, "Runtime child reservations exceed source nodes");
        for (std::size_t j=0;j<children[source].size();++j)
            self(self,children[source][j],childFirst+j,static_cast<int>(hk));
    };
    visit(visit,0,0,-1);
    Monstey_PoseRequire(next==count, "Runtime default-pose permutation is incomplete");
    return result;
}
}