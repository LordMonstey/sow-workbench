// Standard-only offline replica of selected CModelSkeletonMap and auxiliary gather contracts.
// No original executable or graphics device is loaded or executed.
#define main Monstey_FrozenBindingMain
#include "dependencies/Monstey_AnixModelBindingProbe.cpp"
#undef main
#include <bit>
#include <cmath>
#include <optional>

namespace {
using Monstey_Vec3=std::array<float,3>;
using Monstey_Vec4=std::array<float,4>;
using Monstey_Packed7=std::array<float,7>;
struct Monstey_Pose32 {
    std::array<std::uint32_t,8> words{};
    bool operator==(const Monstey_Pose32&)const=default;
};
constexpr Monstey_Pose32 Monstey_IdentityPose{{0,0,0,0,0,0,0,0x3f800000u}};
Monstey_Vec4 Monstey_Add4(const Monstey_Vec4&a,const Monstey_Vec4&b){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=a[i]+b[i];return r;}
Monstey_Vec4 Monstey_Sub4(const Monstey_Vec4&a,const Monstey_Vec4&b){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=a[i]-b[i];return r;}
Monstey_Vec4 Monstey_Mul4(const Monstey_Vec4&a,const Monstey_Vec4&b){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=a[i]*b[i];return r;}
Monstey_Vec4 Monstey_Shuffle4(const Monstey_Vec4&a,unsigned mask){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=a[(mask>>(2*i))&3u];return r;}
// Exact selected helper ordering: result is B * A in quaternion notation.
Monstey_Vec4 Monstey_MultiplyQ(const Monstey_Vec4&a,const Monstey_Vec4&b,bool zFirst=false){
    constexpr Monstey_Vec4 sign1{1,-1,1,-1},sign2{1,1,-1,-1},sign3{-1,1,1,-1};
    const auto reverse=Monstey_Shuffle4(a,0x1b), middle=Monstey_Shuffle4(reverse,0xb1);
    const auto x=Monstey_Mul4(sign1,Monstey_Mul4(Monstey_Shuffle4(b,0),reverse));
    const auto y=Monstey_Mul4(Monstey_Mul4(Monstey_Shuffle4(b,0x55),middle),sign2);
    const auto z=Monstey_Mul4(Monstey_Mul4(Monstey_Shuffle4(middle,0x1b),Monstey_Shuffle4(b,0xaa)),sign3);
    const auto w=Monstey_Mul4(Monstey_Shuffle4(b,0xff),a);
    return Monstey_Add4(zFirst?Monstey_Add4(z,y):Monstey_Add4(y,z),Monstey_Add4(w,x));
}
Monstey_Vec4 Monstey_Rotate4(Monstey_Vec4 value,const Monstey_Vec4&q){
    value[3]=0;constexpr Monstey_Vec4 conjugate{-1,-1,-1,1};
    return Monstey_MultiplyQ(Monstey_MultiplyQ(Monstey_Mul4(conjugate,q),value,true),q);
}
Monstey_Vec4 Monstey_Position(const Monstey_Pose32&p){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=std::bit_cast<float>(p.words[i]);return r;}
Monstey_Vec4 Monstey_Quaternion(const Monstey_Pose32&p){Monstey_Vec4 r{};for(unsigned i=0;i<4;++i)r[i]=std::bit_cast<float>(p.words[i+4]);return r;}
Monstey_Pose32 Monstey_MakePose(const Monstey_Vec4&p,const Monstey_Vec4&q){Monstey_Pose32 r;for(unsigned i=0;i<4;++i){r.words[i]=std::bit_cast<std::uint32_t>(p[i]);r.words[i+4]=std::bit_cast<std::uint32_t>(q[i]);}return r;}
Monstey_Pose32 Monstey_Translation(float x,float y=0,float z=0){return Monstey_MakePose({x,y,z,0},{0,0,0,1});}
Monstey_Vec3 Monstey_ScalarRotate(const Monstey_Vec4&q,const Monstey_Vec3&t){
    const float z=t[2],y=t[1], yx=q[0]*y, qy=q[1],qw=q[3],yy=y*qy;
    const float a=(float)((float)(t[0]*qw)+(float)(qy*z))-(float)(q[2]*y);
    const float b=(float)((float)(y*qw)-(float)(q[0]*z))+(float)(q[2]*t[0]);
    const float c=(float)(yx+(float)(qw*z))-(float)(t[0]*qy);
    const float ny=-qy,nx=-q[0],nz=-q[2];
    const float d=(float)((float)-(float)(q[0]*t[0])-yy)-(float)(q[2]*z);
    return {(float)((float)((float)(nx*d)+(float)(qw*a))+(float)(nz*b))-(float)(ny*c),
        (float)((float)((float)(ny*d)-(float)(nz*a))+(float)(qw*b))+(float)(nx*c),
        (float)((float)((float)(nz*d)+(float)(ny*a))-(float)(nx*b))+(float)(qw*c)};
}
Monstey_Packed7 Monstey_InvertRawBind(Monstey_Packed7 value){
    for(float f:value)Monstey_Require(std::isfinite(f),"SKEL adapter requires finite raw bind lanes");
    const auto input=value;value[3]=-value[3];value[4]=-value[4];value[5]=-value[5];
    const auto rotated=Monstey_ScalarRotate({value[3],value[4],value[5],value[6]},{input[0],input[1],input[2]});
    for(unsigned i=0;i<3;++i)value[i]=-rotated[i];
    return value;
}
Monstey_Vec4 Monstey_ScalarComposeQ(const Monstey_Vec4&p,const Monstey_Vec4&c){
    return {(float)((float)((float)(c[0]*p[3])+(float)(p[0]*c[3]))+(float)(p[1]*c[2]))-(float)(p[2]*c[1]),
      (float)((float)((float)(c[1]*p[3])-(float)(c[2]*p[0]))+(float)(p[1]*c[3]))+(float)(p[2]*c[0]),
      (float)((float)((float)(c[2]*p[3])+(float)(c[1]*p[0]))-(float)(p[1]*c[0]))+(float)(p[2]*c[3]),
      (float)((float)((float)(p[3]*c[3])-(float)(c[0]*p[0]))-(float)(c[1]*p[1]))-(float)(p[2]*c[2])};
}
struct Monstey_MapNode{
    std::uint32_t hash=0;
    std::uint8_t parent=255,self=0,childOffset=0,children=0,rawFlag=0;
    Monstey_Vec3 localPosition{};
    Monstey_Vec4 localQuaternion{0,0,0,1};
    Monstey_Pose32 inverseBind=Monstey_IdentityPose;
};
using Monstey_Rig=std::vector<Monstey_MapNode>;
void Monstey_ValidateRig(const Monstey_Rig&rig,bool source){
    Monstey_Require(rig.size()<=256&&(!source||!rig.empty()),"diagnostic requires 1..256 source nodes and 0..256 target nodes");
    if(rig.empty())return;
    std::vector<unsigned> incoming(rig.size());
    for(std::size_t i=0;i<rig.size();++i){
        const auto&n=rig[i];Monstey_Require(n.self==i,"native runtime self ordinal mismatch");
        Monstey_Require(i==0?n.parent==255:n.parent<i,"native parent ordering mismatch");
        if(!n.children)continue;
        const auto first=static_cast<std::uint8_t>(n.self+n.childOffset);
        Monstey_Require(first>i&&first<=rig.size()&&n.children<=rig.size()-first,"native child interval exceeds owner");
        for(std::size_t j=first;j<static_cast<std::size_t>(first)+n.children;++j){
            Monstey_Require(rig[j].parent==i,"child interval and parent byte disagree");++incoming[j];
        }
    }
    Monstey_Require(incoming[0]==0,"root has incoming edge");
    for(std::size_t i=1;i<incoming.size();++i)Monstey_Require(incoming[i]==1,"native diagnostic requires one complete rooted tree");
}
Monstey_Rig Monstey_RigFromSkel(Monstey_View bytes){
    const auto parsed=Monstey_ParseSkel(bytes);const auto count=parsed.source.size();
    std::vector<Monstey_Packed7> raw(count),inverse(count);
    for(std::size_t i=0;i<count;++i){
        for(unsigned j=0;j<7;++j)raw[i][j]=std::bit_cast<float>(Monstey_U32(bytes,parsed.boneStart+37*i+5+4*j));
        inverse[i]=Monstey_InvertRawBind(raw[i]);
    }
    Monstey_Rig result(count);std::vector<std::size_t> order(count);std::size_t next=1;
    const auto visit=[&](auto&&self,std::size_t s,std::size_t rt,std::uint8_t parent,std::optional<std::size_t> parentSource)->void{
        auto&n=result[rt];order[rt]=s;n.hash=parsed.source[s].hash;n.parent=parent;n.self=static_cast<std::uint8_t>(rt);
        n.rawFlag=parsed.source[s].rawFlag;const auto children=parsed.source[s].childSources.size();Monstey_Require(children<=255,"native child byte profile");
        n.children=static_cast<std::uint8_t>(children);n.childOffset=static_cast<std::uint8_t>(next-rt);
        const Monstey_Packed7 identity{0,0,0,0,0,0,1};const auto&pi=parentSource?inverse[*parentSource]:identity;
        const auto p=Monstey_ScalarRotate({pi[3],pi[4],pi[5],pi[6]},{raw[s][0],raw[s][1],raw[s][2]});
        for(unsigned j=0;j<3;++j)n.localPosition[j]=p[j]+pi[j];
        n.localQuaternion=Monstey_ScalarComposeQ({pi[3],pi[4],pi[5],pi[6]},{raw[s][3],raw[s][4],raw[s][5],raw[s][6]});
        n.inverseBind=Monstey_MakePose({inverse[s][0],inverse[s][1],inverse[s][2],1},{inverse[s][3],inverse[s][4],inverse[s][5],inverse[s][6]});
        const auto first=next;next+=children;Monstey_Require(next<=count,"native reservation exceeds count");
        for(std::size_t j=0;j<children;++j)self(self,parsed.source[s].childSources[j],first+j,static_cast<std::uint8_t>(rt),s);
    };
    visit(visit,0,0,255,std::nullopt);Monstey_Require(order==parsed.runtimeToSource&&next==count,"runtime permutation mismatch");
    Monstey_ValidateRig(result,true);return result;
}
Monstey_Rig Monstey_ParseRigProfile(Monstey_View bytes){
    Monstey_Require(Monstey_U32(bytes,0)==0x504d534du&&Monstey_U32(bytes,4)==1,"expected MSMP diagnostic profile version1");
    const auto count=static_cast<std::size_t>(Monstey_U32(bytes,8));
    Monstey_Require(count<=256&&bytes.size()==12+100*count,"MSMP extent/count mismatch");
    Monstey_Rig rig(count);
    for(std::size_t i=0;i<count;++i){
        const auto at=12+100*i;auto&n=rig[i];n.hash=Monstey_U32(bytes,at);
        n.parent=bytes[at+4];n.self=bytes[at+5];n.childOffset=bytes[at+6];n.children=bytes[at+7];n.rawFlag=bytes[at+8];
        for(unsigned j=0;j<3;++j)n.localPosition[j]=std::bit_cast<float>(Monstey_U32(bytes,at+4+36+4*j));
        for(unsigned j=0;j<4;++j)n.localQuaternion[j]=std::bit_cast<float>(Monstey_U32(bytes,at+4+48+4*j));
        for(unsigned j=0;j<8;++j)n.inverseBind.words[j]=Monstey_U32(bytes,at+68+4*j);
    }
    Monstey_ValidateRig(rig,false);return rig;
}
Monstey_Bytes Monstey_EncodeRigProfile(const Monstey_Rig&rig){
    Monstey_ValidateRig(rig,false);Monstey_Bytes bytes(12+100*rig.size());
    Monstey_Put32(bytes,0,0x504d534d);Monstey_Put32(bytes,4,1);Monstey_Put32(bytes,8,static_cast<std::uint32_t>(rig.size()));
    for(std::size_t i=0;i<rig.size();++i){const auto at=12+100*i;const auto&n=rig[i];Monstey_Put32(bytes,at,n.hash);
        bytes[at+4]=n.parent;bytes[at+5]=n.self;bytes[at+6]=n.childOffset;bytes[at+7]=n.children;bytes[at+8]=n.rawFlag;
        for(unsigned j=0;j<3;++j)Monstey_Put32(bytes,at+40+4*j,std::bit_cast<std::uint32_t>(n.localPosition[j]));
        for(unsigned j=0;j<4;++j)Monstey_Put32(bytes,at+52+4*j,std::bit_cast<std::uint32_t>(n.localQuaternion[j]));
        for(unsigned j=0;j<8;++j)Monstey_Put32(bytes,at+68+4*j,n.inverseBind.words[j]);
    }return bytes;
}
struct Monstey_MapTriple{
    std::uint16_t target=0,source=0,parent=0;
    bool operator==(const Monstey_MapTriple&)const=default;
};
struct Monstey_MapState{
    std::vector<std::uint16_t> remap,roots;
    std::vector<Monstey_MapTriple> triples;
    std::uint32_t auxiliaryCount=0;
    bool operator==(const Monstey_MapState&)const=default;
};
struct Monstey_SkeletonMapResult{
    Monstey_MapState state;
    std::vector<Monstey_Vec3> offsets;
    bool rootMatched=false,countWritten=false,ready=false;
    std::uint16_t targetRoot=0xffff;
    std::uint32_t maximumSource=0;
};
float Monstey_PositionDifferenceSquared(const Monstey_Vec3&source,const Monstey_Vec3&target){
    const float dx=target[0]-source[0],dy=target[1]-source[1],dz=target[2]-source[2];
    const float yy=dy*dy,xx=dx*dx,zz=dz*dz;
    const float yx=yy+xx;return yx+zz;
}
Monstey_SkeletonMapResult Monstey_BuildMapCore(const Monstey_Rig&source,const Monstey_Rig&target,std::uint8_t modeByte,std::uint8_t forceByte,const Monstey_MapState&initial={}){
    Monstey_ValidateRig(source,true);Monstey_ValidateRig(target,false);
    Monstey_Require(initial.remap.empty()||initial.remap.size()>=source.size(),"preexisting native map is too short");
    Monstey_SkeletonMapResult result;result.state=initial;
    for(std::size_t i=0;i<target.size();++i)if(target[i].hash==source[0].hash){result.rootMatched=true;result.targetRoot=static_cast<std::uint16_t>(i);break;}
    if(!result.rootMatched)return result;
    const bool plain=modeByte!=0,force=forceByte!=0;std::array<bool,256> parentSeen{};
    constexpr float threshold=std::bit_cast<float>(0x3c23d70au);
    const auto visit=[&](auto&&self,std::size_t s,std::size_t t,bool inherited)->std::uint32_t{
        std::uint32_t added=0;unsigned seed=0;
        const auto firstSource=static_cast<std::uint8_t>(source[s].self+source[s].childOffset);
        const auto firstTarget=static_cast<std::uint8_t>(target[t].self+target[t].childOffset);
        for(unsigned child=0;child<source[s].children;++child){
            const std::size_t si=static_cast<std::size_t>(firstSource)+child;std::optional<std::size_t> match;
            for(unsigned attempt=0;attempt<target[t].children;++attempt){
                const auto ti=static_cast<std::size_t>(firstTarget)+(seed+attempt)%target[t].children;
                if(target[ti].hash==source[si].hash){match=ti;seed=(attempt+1)%target[t].children;break;}
            }
            if(!match)continue;const auto ti=*match;
            const bool difference=Monstey_PositionDifferenceSquared(source[si].localPosition,target[ti].localPosition)>=threshold||force;
            const bool auxiliary=inherited||difference;
            if(!result.state.remap.empty()||si!=ti){
                if(result.state.remap.empty()){result.state.remap.resize(source.size());for(std::size_t i=0;i<source.size();++i)result.state.remap[i]=static_cast<std::uint16_t>(i);}
                const auto flag=static_cast<std::uint16_t>(!plain&&auxiliary?0x1000:0);
                result.state.remap[si]=static_cast<std::uint16_t>(ti|flag);
            }
            if(!inherited&&difference&&!parentSeen[s]){
                parentSeen[s]=true;
                result.state.roots.push_back(static_cast<std::uint16_t>(plain?s:target[ti].parent));
            }
            if(auxiliary){
                result.state.triples.push_back({static_cast<std::uint16_t>(ti),static_cast<std::uint16_t>(si),target[ti].parent});
                result.maximumSource=std::max(result.maximumSource,static_cast<std::uint32_t>(si));++added;
            }
            added+=self(self,si,ti,auxiliary);
        }return added;
    };
    result.state.auxiliaryCount=visit(visit,0,result.targetRoot,false);result.countWritten=true;return result;
}
Monstey_SkeletonMapResult Monstey_BuildClassMap(const Monstey_Rig&source,const Monstey_Rig&target,std::uint8_t modeByte,std::uint8_t forceByte,const Monstey_MapState&initial={}){
    auto result=Monstey_BuildMapCore(source,target,modeByte,forceByte,initial);
    Monstey_Require(result.state.auxiliaryCount==result.state.triples.size(),"native Size mismatch with AnimationMapDesc assertion");
    for(const auto&triple:result.state.triples){
        Monstey_Require(triple.source<source.size()&&triple.target<target.size(),"map triple outside skeleton owner");
        Monstey_Vec3 offset{};for(unsigned j=0;j<3;++j)offset[j]=target[triple.target].localPosition[j]-source[triple.source].localPosition[j];
        result.offsets.push_back(offset);
    }
    std::sort(result.state.roots.begin(),result.state.roots.end());result.ready=true;return result;
}

struct Monstey_AuxRange{
    std::uint16_t start=0xffff,count=0;
    std::vector<std::uint16_t> globalRoots;
    bool operator==(const Monstey_AuxRange&)const=default;
};
Monstey_AuxRange Monstey_RegisterAux(const Monstey_AuxRange&initial,const Monstey_SkeletonMapResult&map){
    auto range=initial;if(!map.state.auxiliaryCount)return range;
    Monstey_Require(!map.state.triples.empty(),"nonzero auxiliary count requires triples");
    std::uint16_t low=0xffff,high=0;
    for(const auto&p:map.state.triples){low=std::min(low,p.target);high=std::max(high,p.target);}
    if(range.count){
        const auto first=std::min(range.start,low);
        const auto end=std::max(static_cast<std::uint16_t>(range.start+range.count),static_cast<std::uint16_t>(low+high));
        range.start=first;range.count=static_cast<std::uint16_t>(end-first);
    }else{range.start=low;range.count=high;} // Native passes largest ordinal as the extent; no +1/min-span rewrite.
    range.globalRoots.insert(range.globalRoots.end(),map.state.roots.begin(),map.state.roots.end());
    std::sort(range.globalRoots.begin(),range.globalRoots.end());
    range.globalRoots.erase(std::unique(range.globalRoots.begin(),range.globalRoots.end()),range.globalRoots.end());
    return range;
}
struct Monstey_CopyRoot{
    std::uint16_t auxiliary=0,target=0;
    bool operator==(const Monstey_CopyRoot&)const=default;
};
struct Monstey_AuxDescriptor{
    std::uint16_t start=0,count=0;
    std::vector<Monstey_CopyRoot> roots;
    std::vector<Monstey_MapTriple> triples;
    std::vector<Monstey_Vec3> offsets;
    std::vector<Monstey_Pose32> inverseBind;
};
Monstey_AuxDescriptor Monstey_BuildAuxDescriptor(const Monstey_SkeletonMapResult&map,const Monstey_Rig&source,const Monstey_AuxRange&range){
    Monstey_Require(map.offsets.size()==map.state.triples.size(),"offset/triple count mismatch");
    Monstey_Require(std::is_sorted(map.state.roots.begin(),map.state.roots.end())&&std::is_sorted(range.globalRoots.begin(),range.globalRoots.end()),"native sorted-root owner precondition");
    Monstey_AuxDescriptor out;out.start=range.start;out.count=range.count;out.triples=map.state.triples;out.offsets=map.offsets;
    std::size_t cursor=0;
    for(const auto root:map.state.roots){
        for(std::size_t i=cursor;i<range.globalRoots.size();++i)if(range.globalRoots[i]==root){
            out.roots.push_back({static_cast<std::uint16_t>(i+range.count),root});cursor=i;break;
        }
    }
    for(const auto&triple:out.triples){
        Monstey_Require(triple.source<source.size(),"inverse-bind source outside owner");
        auto inverse=source[triple.source].inverseBind;inverse.words[3]=0;out.inverseBind.push_back(inverse);
    }return out;
}
std::vector<Monstey_Pose32> Monstey_CopyExtraRoots(const Monstey_AuxRange&range,std::span<const Monstey_Pose32>global,std::span<const Monstey_Pose32>initial){
    Monstey_Require(range.count<=initial.size()&&range.globalRoots.size()<=initial.size()-range.count,"auxiliary bank extent");
    for(const auto root:range.globalRoots)Monstey_Require(root<global.size(),"extra root outside evaluated owner");
    std::vector<Monstey_Pose32> out(initial.begin(),initial.end());
    for(std::size_t i=0;i<range.globalRoots.size();++i)out[range.count+i]=global[range.globalRoots[i]];
    return out;
}
struct Monstey_AuxOwner{
    std::optional<Monstey_AuxDescriptor> descriptor;
    std::vector<Monstey_Pose32> scratch;
};
Monstey_AuxOwner Monstey_InstallAux(const Monstey_AuxOwner&initial,const std::optional<Monstey_AuxDescriptor>&descriptor,std::span<const Monstey_Pose32>freshScratch={}){
    auto out=initial;out.descriptor=descriptor;
    if(!descriptor){out.scratch.clear();return out;}
    if(out.scratch.empty()){
        Monstey_Require(freshScratch.size()==256,"fresh native scratch bytes are unspecified; provide 256 snapshot records");
        out.scratch.assign(freshScratch.begin(),freshScratch.end());
    }
    Monstey_Require(out.scratch.size()==256,"native scratch owner extent");return out;
}
std::vector<Monstey_Pose32> Monstey_PrepareAux(const Monstey_AuxDescriptor&desc,std::span<const Monstey_Pose32>aux,std::span<const Monstey_Pose32>initial){
    Monstey_Require(initial.size()==256,"native scratch requires 256 records");
    Monstey_Require(desc.triples.size()==desc.offsets.size()&&desc.triples.size()==desc.inverseBind.size(),"descriptor parallel vector extent");
    for(const auto&root:desc.roots)Monstey_Require(root.target<256&&root.auxiliary<aux.size(),"root copy outside owning arrays");
    for(const auto&t:desc.triples)Monstey_Require(t.target<256&&t.parent<256&&t.target>=desc.start&&static_cast<std::size_t>(t.target-desc.start)<aux.size(),"auxiliary triple outside owning arrays");
    std::vector<Monstey_Pose32> scratch(initial.begin(),initial.end());
    for(const auto&root:desc.roots)scratch[root.target]=aux[root.auxiliary];
    // First pass reconstructs target-indexed records. Do not apply bind inverse until all parent records are available.
    for(std::size_t i=0;i<desc.triples.size();++i){
        const auto&t=desc.triples[i];const auto local=aux[t.target-desc.start],parent=scratch[t.parent];
        const auto q=Monstey_MultiplyQ(Monstey_Quaternion(local),Monstey_Quaternion(parent));
        const Monstey_Vec4 offset{desc.offsets[i][0],desc.offsets[i][1],desc.offsets[i][2],0};
        const auto p=Monstey_Add4(Monstey_Rotate4(Monstey_Sub4(Monstey_Position(local),offset),Monstey_Quaternion(parent)),Monstey_Position(parent));
        scratch[t.target]=Monstey_MakePose(p,q);
    }
    // Second pass deliberately retains repeated target ordinals and their sequential writes.
    for(std::size_t i=0;i<desc.triples.size();++i){
        const auto target=desc.triples[i].target;const auto current=scratch[target],inverse=desc.inverseBind[i];
        const auto q=Monstey_MultiplyQ(Monstey_Quaternion(inverse),Monstey_Quaternion(current));
        const auto p=Monstey_Add4(Monstey_Rotate4(Monstey_Position(inverse),Monstey_Quaternion(current)),Monstey_Position(current));
        scratch[target]=Monstey_MakePose(p,q);
    }return scratch;
}
struct Monstey_GatherInput{
    bool primaryPresent=true,remapPresent=false,descriptorPresent=false,auxiliaryPresent=false,previous=false;
    std::uint8_t incoming=0,instanceEnable=0,globalEnable=0;
    std::vector<std::uint8_t> palette;
    std::vector<Monstey_Pose32> primary,auxiliary,scratch,initialOutput;
    std::vector<std::uint16_t> remap;
    Monstey_AuxDescriptor descriptor;
};
struct Monstey_GatherOutput{
    std::vector<Monstey_Pose32> output,scratch;
    std::vector<bool>written;
    bool auxiliaryUsed=false;
};
Monstey_GatherOutput Monstey_DrawGather(const Monstey_GatherInput&in){
    Monstey_Require(in.palette.size()<=255,"draw count is U8");
    const auto count=std::min<std::size_t>(64,in.palette.size());
    Monstey_Require(in.initialOutput.size()==count,"initial output extent");
    Monstey_GatherOutput out{in.initialOutput,in.scratch,std::vector<bool>(count,false),false};
    if(!count)return out; // 253198 does not call the gather when its runtime count is zero.
    if(!in.primaryPresent){out.output.assign(count,Monstey_IdentityPose);std::fill(out.written.begin(),out.written.end(),true);return out;}
    if(in.remapPresent)Monstey_Require(!in.remap.empty(),"invalid non-null empty remap owner");
    out.auxiliaryUsed=in.remapPresent&&!in.previous&&in.incoming&&in.descriptorPresent&&in.instanceEnable&&in.globalEnable;
    if(out.auxiliaryUsed){
        Monstey_Require(in.auxiliaryPresent,"native auxiliary descriptor pointer precondition");
        out.scratch=Monstey_PrepareAux(in.descriptor,in.auxiliary,in.scratch);
    }
    for(std::size_t i=0;i<count;++i){
        if(in.remapPresent){
            const auto index=in.palette[i]<in.remap.size()?in.palette[i]:0;
            const auto word=in.remap[index];
            if(out.auxiliaryUsed){
                if(word&0x1000u){out.output[i]=out.scratch[static_cast<std::uint8_t>(word)];out.written[i]=true;}
                else if(word<in.primary.size()){out.output[i]=in.primary[word];out.written[i]=true;}
                // Native bad full-U16 primary indices leave the slot unwritten.
            }else{
                const auto source=static_cast<std::uint8_t>(word);
                out.output[i]=source<in.primary.size()?in.primary[source]:Monstey_IdentityPose;out.written[i]=true;
            }
        }else{
            const auto source=in.palette[i];out.output[i]=source<in.primary.size()?in.primary[source]:Monstey_IdentityPose;out.written[i]=true;
        }
    }return out;
}
struct Monstey_ProfileCursor{
    Monstey_View bytes;std::size_t at=0;
    Monstey_View Monstey_Take(std::size_t n){Monstey_Range(bytes,at,n);const auto view=bytes.subspan(at,n);at+=n;return view;}
    std::uint8_t Monstey_Byte(){return Monstey_Take(1)[0];}
    std::uint16_t Monstey_Word(){return Monstey_U16(Monstey_Take(2),0);}
    std::uint32_t Monstey_Dword(){return Monstey_U32(Monstey_Take(4),0);}
    std::size_t Monstey_Count(std::size_t stride){const auto n=Monstey_Dword();Monstey_Require(n<=4096&&n<=(bytes.size()-at)/stride,"bounded snapshot count/extent");return n;}
    Monstey_Pose32 Monstey_Pose(){Monstey_Pose32 p;for(auto&word:p.words)word=Monstey_Dword();return p;}
};
Monstey_GatherInput Monstey_ParseAuxProfile(Monstey_View bytes){
    Monstey_ProfileCursor c{bytes};Monstey_Require(c.Monstey_Dword()==0x5855414du&&c.Monstey_Dword()==1,"expected MAUX diagnostic snapshot version1");
    Monstey_GatherInput in;const auto p=c.Monstey_Byte(),m=c.Monstey_Byte(),d=c.Monstey_Byte(),a=c.Monstey_Byte();
    Monstey_Require(p<=1&&m<=1&&d<=1&&a<=1,"presence flags must be 0/1");in.primaryPresent=p!=0;in.remapPresent=m!=0;in.descriptorPresent=d!=0;in.auxiliaryPresent=a!=0;
    in.incoming=c.Monstey_Byte();in.instanceEnable=c.Monstey_Byte();in.globalEnable=c.Monstey_Byte();const auto previous=c.Monstey_Byte();Monstey_Require(previous<=1,"selection flag must be 0/1");in.previous=previous!=0;
    in.descriptor.start=c.Monstey_Word();in.descriptor.count=c.Monstey_Word();
    const auto np=c.Monstey_Count(1),nPrimary=c.Monstey_Count(32),nRemap=c.Monstey_Count(2),nAux=c.Monstey_Count(32),nRoots=c.Monstey_Count(4),nTriples=c.Monstey_Count(6);
    Monstey_Require(np<=255,"runtime palette byte count");
    const auto palette=c.Monstey_Take(np);in.palette.assign(palette.begin(),palette.end());
    for(std::size_t i=0;i<nPrimary;++i)in.primary.push_back(c.Monstey_Pose());
    for(std::size_t i=0;i<nRemap;++i)in.remap.push_back(c.Monstey_Word());
    for(std::size_t i=0;i<nAux;++i)in.auxiliary.push_back(c.Monstey_Pose());
    for(std::size_t i=0;i<nRoots;++i){const auto source=c.Monstey_Word(),target=c.Monstey_Word();in.descriptor.roots.push_back({source,target});}
    for(std::size_t i=0;i<nTriples;++i){const auto target=c.Monstey_Word(),source=c.Monstey_Word(),parent=c.Monstey_Word();in.descriptor.triples.push_back({target,source,parent});}
    for(std::size_t i=0;i<nTriples;++i){Monstey_Vec3 offset{};for(auto&v:offset)v=std::bit_cast<float>(c.Monstey_Dword());in.descriptor.offsets.push_back(offset);}
    for(std::size_t i=0;i<nTriples;++i)in.descriptor.inverseBind.push_back(c.Monstey_Pose());
    for(unsigned i=0;i<256;++i)in.scratch.push_back(c.Monstey_Pose());
    for(std::size_t i=0;i<std::min<std::size_t>(np,64);++i)in.initialOutput.push_back(c.Monstey_Pose());
    Monstey_Require(c.at==bytes.size(),"MAUX trailing bytes");return in;
}
void Monstey_Append16(Monstey_Bytes&bytes,std::uint16_t value){bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8));}
void Monstey_Append32(Monstey_Bytes&bytes,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void Monstey_AppendPose(Monstey_Bytes&bytes,const Monstey_Pose32&pose){for(const auto word:pose.words)Monstey_Append32(bytes,word);}
Monstey_Bytes Monstey_EncodeAuxProfile(const Monstey_GatherInput&in){
    Monstey_Require(in.scratch.size()==256&&in.initialOutput.size()==std::min<std::size_t>(in.palette.size(),64),"MAUX snapshot extents");
    Monstey_Require(in.descriptor.triples.size()==in.descriptor.offsets.size()&&in.descriptor.triples.size()==in.descriptor.inverseBind.size(),"MAUX parallel vectors");
    Monstey_Bytes bytes;Monstey_Append32(bytes,0x5855414d);Monstey_Append32(bytes,1);
    for(const bool flag:{in.primaryPresent,in.remapPresent,in.descriptorPresent,in.auxiliaryPresent})bytes.push_back(flag?1:0);
    bytes.push_back(in.incoming);bytes.push_back(in.instanceEnable);bytes.push_back(in.globalEnable);bytes.push_back(in.previous?1:0);
    Monstey_Append16(bytes,in.descriptor.start);Monstey_Append16(bytes,in.descriptor.count);
    for(const auto count:{in.palette.size(),in.primary.size(),in.remap.size(),in.auxiliary.size(),in.descriptor.roots.size(),in.descriptor.triples.size()}){Monstey_Require(count<=4096,"MAUX bounded count");Monstey_Append32(bytes,static_cast<std::uint32_t>(count));}
    bytes.insert(bytes.end(),in.palette.begin(),in.palette.end());
    for(const auto&p:in.primary)Monstey_AppendPose(bytes,p);
    for(const auto value:in.remap)Monstey_Append16(bytes,value);
    for(const auto&p:in.auxiliary)Monstey_AppendPose(bytes,p);
    for(const auto&p:in.descriptor.roots){Monstey_Append16(bytes,p.auxiliary);Monstey_Append16(bytes,p.target);}
    for(const auto&p:in.descriptor.triples){Monstey_Append16(bytes,p.target);Monstey_Append16(bytes,p.source);Monstey_Append16(bytes,p.parent);}
    for(const auto&p:in.descriptor.offsets)for(const auto v:p)Monstey_Append32(bytes,std::bit_cast<std::uint32_t>(v));
    for(const auto&p:in.descriptor.inverseBind)Monstey_AppendPose(bytes,p);
    for(const auto&p:in.scratch)Monstey_AppendPose(bytes,p);
    for(const auto&p:in.initialOutput)Monstey_AppendPose(bytes,p);
    return bytes;
}

Monstey_Rig Monstey_TestRig(std::initializer_list<std::uint32_t>hashes,std::initializer_list<unsigned>parents){
    Monstey_Require(hashes.size()==parents.size(),"test rig sizes");Monstey_Rig rig(hashes.size());auto h=hashes.begin();auto p=parents.begin();
    for(std::size_t i=0;i<rig.size();++i,++h,++p){rig[i].hash=*h;rig[i].parent=static_cast<std::uint8_t>(*p);rig[i].self=static_cast<std::uint8_t>(i);rig[i].inverseBind.words[3]=0x3f800000u;}
    for(std::size_t i=0;i<rig.size();++i){
        std::vector<std::size_t> children;for(std::size_t j=1;j<rig.size();++j)if(rig[j].parent==i)children.push_back(j);
        rig[i].children=static_cast<std::uint8_t>(children.size());rig[i].childOffset=children.empty()?1:static_cast<std::uint8_t>(children.front()-i);
    }Monstey_ValidateRig(rig,false);return rig;
}
void Monstey_TestTranslationBinds(Monstey_Rig&rig){
    std::vector<Monstey_Vec3> world(rig.size());
    for(std::size_t i=0;i<rig.size();++i){
        for(unsigned j=0;j<3;++j)world[i][j]=rig[i].localPosition[j]+(i?world[rig[i].parent][j]:0);
        rig[i].inverseBind=Monstey_MakePose({-world[i][0],-world[i][1],-world[i][2],1},{0,0,0,1});
    }
}
std::vector<Monstey_Pose32> Monstey_PoisonScratch(){
    std::vector<Monstey_Pose32> out(256);
    for(std::size_t i=0;i<256;++i)for(unsigned j=0;j<8;++j)out[i].words[j]=0x6d000000u+static_cast<std::uint32_t>(i*16+j);
    return out;
}
bool Monstey_PoseNear(const Monstey_Pose32&actual,const Monstey_Vec4&p,const Monstey_Vec4&q,float tolerance=0.00001f){
    const auto ap=Monstey_Position(actual),aq=Monstey_Quaternion(actual);
    for(unsigned i=0;i<4;++i)if(!std::isfinite(ap[i])||!std::isfinite(aq[i])||std::fabs(ap[i]-p[i])>tolerance||std::fabs(aq[i]-q[i])>tolerance)return false;
    return true;
}
Monstey_GatherInput Monstey_OneAuxFixture(){
    auto source=Monstey_TestRig({100,9},{255,0}),target=Monstey_TestRig({100,8,9},{255,0,0});
    source[1].localPosition={2,0,0};target[2].localPosition={3,0,0};Monstey_TestTranslationBinds(source);
    const auto map=Monstey_BuildClassMap(source,target,0,0);const auto range=Monstey_RegisterAux({},map);
    Monstey_GatherInput in;in.remapPresent=true;in.descriptorPresent=true;in.auxiliaryPresent=true;in.incoming=in.instanceEnable=in.globalEnable=1;
    in.descriptor=Monstey_BuildAuxDescriptor(map,source,range);in.remap=map.state.remap;in.palette={1};
    in.primary={Monstey_Translation(101),Monstey_Translation(102),Monstey_Translation(103)};
    in.auxiliary={Monstey_Translation(7),Monstey_Translation(999),Monstey_Translation(5)};
    in.scratch=Monstey_PoisonScratch();in.initialOutput={Monstey_Translation(-999)};return in;
}
void Monstey_MapTests(Monstey_TestSuite&tests){
    tests.Monstey_Test("identity_map_is_implicit_and_root_not_written",[]{
        const auto rig=Monstey_TestRig({100,1,2},{255,0,0});const auto map=Monstey_BuildClassMap(rig,rig,0,0);
        Monstey_Require(map.rootMatched&&map.targetRoot==0&&map.countWritten&&map.ready&&map.state.remap.empty()&&map.state.triples.empty()&&map.state.auxiliaryCount==0,"implicit identity");
    });
    tests.Monstey_Test("missing_root_and_empty_target_are_ready_without_new_map",[]{
        const auto source=Monstey_TestRig({100,1},{255,0}),target=Monstey_TestRig({200,1},{255,0});
        for(const auto&t:std::array<Monstey_Rig,2>{target,Monstey_Rig{}}){const auto map=Monstey_BuildClassMap(source,t,0,255);Monstey_Require(!map.rootMatched&&!map.countWritten&&map.ready&&map.state.remap.empty()&&map.state.auxiliaryCount==0,"root-miss contract");}
    });
    tests.Monstey_Test("root_scan_selects_first_equal_anywhere_but_keeps_root_zero",[]{
        const auto source=Monstey_TestRig({100,1},{255,0}),target=Monstey_TestRig({999,100,100,1,1},{255,0,0,1,2});
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.targetRoot==1&&map.state.remap==std::vector<std::uint16_t>{0,3},"root ordinal or collision policy");
    });
    tests.Monstey_Test("rotating_seed_uses_attempt_count_and_reuses_target_duplicates",[]{
        const auto source=Monstey_TestRig({100,7,7,7,7},{255,0,0,0,0}),target=Monstey_TestRig({100,7,7,7},{255,0,0,0});
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{0,1,2,2,2},"target-used mask or incorrect seed update");
    });
    tests.Monstey_Test("hash_search_is_hierarchical_not_flat",[]{
        const auto source=Monstey_TestRig({100,1,9},{255,0,1}),target=Monstey_TestRig({100,1,9},{255,0,0});
        Monstey_MapState initial;initial.remap={9,8,77};initial.auxiliaryCount=42;
        const auto map=Monstey_BuildMapCore(source,target,0,0,initial);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{9,1,77}&&map.state.auxiliaryCount==0,"globally matched wrong-parent descendant");
    });
    tests.Monstey_Test("missing_source_parent_skips_its_entire_subtree",[]{
        const auto source=Monstey_TestRig({100,1,9},{255,0,1}),target=Monstey_TestRig({100,2,9},{255,0,1});
        Monstey_MapState initial;initial.remap={9,8,7};
        Monstey_Require(Monstey_BuildMapCore(source,target,0,0,initial).state.remap==initial.remap,"unmatched subtree traversed");
    });
    tests.Monstey_Test("core_root_miss_preserves_existing_vectors_and_count",[]{
        const auto source=Monstey_TestRig({100,1},{255,0}),target=Monstey_TestRig({200,1},{255,0});
        Monstey_MapState initial;initial.remap={91,92,93};initial.roots={5};initial.triples={{1,1,0}};initial.auxiliaryCount=17;
        const auto map=Monstey_BuildMapCore(source,target,0,0,initial);
        Monstey_Require(!map.rootMatched&&!map.countWritten&&map.state==initial,"invented clearing on missing root");
    });
    tests.Monstey_Test("leaf_roots_ignore_force_and_do_not_generate_root_pair",[]{
        const auto rig=Monstey_TestRig({100},{255});const auto map=Monstey_BuildClassMap(rig,rig,255,255);
        Monstey_Require(map.rootMatched&&map.state.remap.empty()&&map.state.triples.empty(),"mapped root or forced nonexistent child");
    });
    tests.Monstey_Test("bind_quaternion_is_not_part_of_difference_classification",[]{
        auto source=Monstey_TestRig({100,9},{255,0}),target=Monstey_TestRig({100,8,9},{255,0,0});
        source[1].localQuaternion={0,0,0,1};target[2].localQuaternion={1,0,0,0};
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{0,2}&&map.state.triples.empty(),"invented quaternion comparison");
    });
    tests.Monstey_Test("raw_mode_suppresses_flag_and_uses_source_parent_root",[]{
        auto source=Monstey_TestRig({100,50,9},{255,0,1}),target=Monstey_TestRig({100,8,50,9},{255,0,0,2});target[3].localPosition={1,0,0};
        const auto plain=Monstey_BuildClassMap(source,target,1,0),normal=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(plain.state.remap==std::vector<std::uint16_t>{0,2,3}&&plain.state.roots==std::vector<std::uint16_t>{1},"plain mode");
        Monstey_Require(normal.state.remap==std::vector<std::uint16_t>{0,2,0x1003}&&normal.state.roots==std::vector<std::uint16_t>{2},"normal target-parent mode");
        Monstey_Require(plain.state.triples==std::vector<Monstey_MapTriple>{{3,2,2}}&&plain.state.triples==normal.state.triples,"mode removed auxiliary triples");
    });
    tests.Monstey_Test("all_nonzero_flag_bytes_have_boolean_semantics",[]{
        auto source=Monstey_TestRig({100,50,9},{255,0,1}),target=Monstey_TestRig({100,8,50,9},{255,0,0,2});target[3].localPosition={1,0,0};
        for(unsigned flag=1;flag<256;++flag){
            const auto plain=Monstey_BuildClassMap(source,target,static_cast<std::uint8_t>(flag),0);
            Monstey_Require(plain.state.remap==std::vector<std::uint16_t>{0,2,3}&&plain.state.roots==std::vector<std::uint16_t>{1},"mode byte normalized incorrectly");
            const auto force=Monstey_BuildClassMap(source,target,0,static_cast<std::uint8_t>(flag));
            Monstey_Require(force.state.remap==std::vector<std::uint16_t>{0,0x1002,0x1003}&&force.state.auxiliaryCount==2,"force byte normalized incorrectly");
        }
    });
    tests.Monstey_Test("auxiliary_propagates_to_matched_descendants",[]{
        auto source=Monstey_TestRig({100,50,9},{255,0,1}),target=Monstey_TestRig({100,8,50,9},{255,0,0,2});
        source[1].localPosition={2,0,0};target[2].localPosition={3,0,0};
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{0,0x1002,0x1003}&&map.state.roots==std::vector<std::uint16_t>{0},"inherited auxiliary flag");
        Monstey_Require(map.state.triples==std::vector<Monstey_MapTriple>{{2,1,0},{3,2,2}}&&map.maximumSource==2,"triple order/source maximum");
    });
    tests.Monstey_Test("threshold_boundary_uses_native_float_bits",[]{
        const auto source=Monstey_TestRig({100,9},{255,0});auto target=Monstey_TestRig({100,8,9},{255,0,0});
        target[2].localPosition={0.1f,0,0};const auto at=Monstey_BuildClassMap(source,target,0,0);
        target[2].localPosition[0]=std::nextafter(0.1f,0.0f);const auto below=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(at.state.remap==std::vector<std::uint16_t>{0,0x1002}&&below.state.remap==std::vector<std::uint16_t>{0,2},"threshold off-by-one ULP");
    });
    tests.Monstey_Test("native_yx_then_z_addition_order_changes_threshold_case",[]{
        const auto source=Monstey_TestRig({100,9},{255,0});auto target=Monstey_TestRig({100,8,9},{255,0,0});
        target[2].localPosition={std::bit_cast<float>(0x3d98bfa1u),std::bit_cast<float>(0x3d75310bu),std::bit_cast<float>(0x3cef5f4eu)};
        Monstey_Require(std::bit_cast<std::uint32_t>(Monstey_PositionDifferenceSquared({},target[2].localPosition))==0x3c23d709u,"native addition order");
        Monstey_Require(Monstey_BuildClassMap(source,target,0,0).state.remap==std::vector<std::uint16_t>{0,2},"reassociated threshold classification");
    });
    tests.Monstey_Test("unordered_nan_difference_is_false_unless_forced",[]{
        const auto source=Monstey_TestRig({100,9},{255,0});auto target=Monstey_TestRig({100,8,9},{255,0,0});
        target[2].localPosition[0]=std::bit_cast<float>(0x7fc12345u);
        const auto normal=Monstey_BuildClassMap(source,target,0,0),forced=Monstey_BuildClassMap(source,target,0,1);
        Monstey_Require(normal.state.remap==std::vector<std::uint16_t>{0,2}&&normal.state.triples.empty(),"NaN comparison");
        Monstey_Require(forced.state.remap==std::vector<std::uint16_t>{0,0x1002}&&forced.state.triples.size()==1,"force after unordered comparison");
    });
    tests.Monstey_Test("infinite_squared_difference_takes_auxiliary_branch",[]{
        const auto source=Monstey_TestRig({100,9},{255,0});auto target=Monstey_TestRig({100,8,9},{255,0,0});target[2].localPosition[0]=std::numeric_limits<float>::infinity();
        Monstey_Require(Monstey_BuildClassMap(source,target,0,0).state.remap==std::vector<std::uint16_t>{0,0x1002},"invented finite-only native classifier");
    });
    tests.Monstey_Test("lazy_allocation_does_not_backfill_earlier_identity_auxiliary_bit",[]{
        auto source=Monstey_TestRig({100,1,2},{255,0,0}),target=Monstey_TestRig({100,1,8,2},{255,0,0,0});target[1].localPosition={1,0,0};
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{0,1,3}&&map.state.triples==std::vector<Monstey_MapTriple>{{1,1,0}},"retroactive auxiliary backfill");
    });
    tests.Monstey_Test("earlier_ordinal_mismatch_makes_later_identity_auxiliary_bit_explicit",[]{
        auto source=Monstey_TestRig({100,2,1},{255,0,0}),target=Monstey_TestRig({100,1,8,2},{255,0,0,0});target[1].localPosition={1,0,0};
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.remap==std::vector<std::uint16_t>{0,3,0x1001}&&map.state.triples==std::vector<Monstey_MapTriple>{{1,2,0}},"existing map write branch");
    });
    tests.Monstey_Test("forced_identity_ordinals_can_have_triples_but_no_remap_owner",[]{
        const auto rig=Monstey_TestRig({100,1},{255,0});const auto map=Monstey_BuildClassMap(rig,rig,0,1);
        Monstey_Require(map.state.remap.empty()&&map.state.auxiliaryCount==1&&map.state.roots==std::vector<std::uint16_t>{0},"forced implicit identity was materialized");
    });
    tests.Monstey_Test("class_sorts_roots_but_does_not_deduplicate",[]{
        auto source=Monstey_TestRig({100,50,50,9,9},{255,0,0,1,2}),target=Monstey_TestRig({100,50,9},{255,0,1});target[2].localPosition={1,0,0};
        const auto map=Monstey_BuildClassMap(source,target,0,0);
        Monstey_Require(map.state.roots==std::vector<std::uint16_t>{1,1}&&map.state.triples==std::vector<Monstey_MapTriple>{{2,3,1},{2,4,1}},"root deduplication or triple reuse");
    });
    tests.Monstey_Test("maximum_256_node_map_context_and_root_scan_boundary",[]{
        Monstey_Rig rig(256);for(std::size_t i=0;i<256;++i){rig[i].hash=static_cast<std::uint32_t>(i);rig[i].self=static_cast<std::uint8_t>(i);rig[i].parent=i?0:255;rig[i].childOffset=1;}
        rig[0].children=255;const auto map=Monstey_BuildClassMap(rig,rig,0,1);
        Monstey_Require(map.state.remap.empty()&&map.state.triples.size()==255&&map.maximumSource==255,"context byte-array limit");
        auto target=rig;target[255].hash=0xffffffffu;const auto source=Monstey_TestRig({0xffffffffu},{255});
        Monstey_Require(Monstey_BuildClassMap(source,target,0,0).targetRoot==255,"last root hash candidate");
    });
    tests.Monstey_Test("raw_node_flags_do_not_filter_map_ordinals",[]{
        const auto original=Monstey_TestRig({100,9},{255,0});const auto other=Monstey_TestRig({100,8,9},{255,0,0});
        for(unsigned i=0;i<256;++i){auto source=original,target=other;source[1].rawFlag=static_cast<std::uint8_t>(i);target[2].rawFlag=static_cast<std::uint8_t>(255-i);
            Monstey_Require(Monstey_BuildClassMap(source,target,0,0).state.remap==std::vector<std::uint16_t>{0,2},"node flag removed map slot");}
    });
    tests.Monstey_Test("invalid_hierarchy_and_short_preexisting_map_reject_transactionally",[]{
        const auto source=Monstey_TestRig({100,9},{255,0}),target=Monstey_TestRig({100,8,9},{255,0,0});
        Monstey_MapState initial;initial.remap={77};const auto saved=initial;
        Monstey_Rejects([&]{(void)Monstey_BuildMapCore(source,target,0,0,initial);});Monstey_Require(initial==saved,"map changed on validation failure");
        for(unsigned field=0;field<3;++field){auto bad=source;if(field==0)bad[1].self=0;else if(field==1)bad[0].children=2;else bad[1].parent=1;
            Monstey_Rejects([&]{(void)Monstey_BuildClassMap(bad,target,0,0);});}
        Monstey_Rejects([&]{(void)Monstey_BuildClassMap({},target,0,0);});
    });
    tests.Monstey_Test("class_pair_count_assertion_preserves_input_core_state",[]{
        const auto source=Monstey_TestRig({100,1},{255,0}),target=Monstey_TestRig({100,1},{255,0});
        Monstey_MapState initial;initial.triples={{1,1,0}};initial.auxiliaryCount=1;const auto saved=initial;
        Monstey_Rejects([&]{(void)Monstey_BuildClassMap(source,target,0,0,initial);});Monstey_Require(initial==saved,"class failure mutated previous owner");
    });
    tests.Monstey_Test("MSMP_profile_bounds_and_semantic_roundtrip",[]{
        const auto source=Monstey_TestRig({100,1,2},{255,0,0});const auto bytes=Monstey_EncodeRigProfile(source);
        Monstey_Require(Monstey_ParseRigProfile(bytes).size()==3,"profile roundtrip");
        for(std::size_t n=0;n<bytes.size();++n)Monstey_Rejects([&]{(void)Monstey_ParseRigProfile(Monstey_View(bytes).first(n));});
        for(const auto at:{0u,4u,8u}){auto bad=bytes;Monstey_Put32(bad,at,0xffffffffu);Monstey_Rejects([&]{(void)Monstey_ParseRigProfile(bad);});}
        auto extra=bytes;extra.push_back(0xa7);Monstey_Rejects([&]{(void)Monstey_ParseRigProfile(extra);});
    });
}

void Monstey_AuxTests(Monstey_TestSuite&tests){
    tests.Monstey_Test("range_uses_native_largest_ordinal_then_u16_union",[]{
        Monstey_SkeletonMapResult map;map.state.auxiliaryCount=2;map.state.triples={{5,1,0},{7,2,0}};map.state.roots={5,1,5};
        const auto first=Monstey_RegisterAux({},map);Monstey_Require(first.start==5&&first.count==7&&first.globalRoots==std::vector<std::uint16_t>{1,5},"first range rewritten to minimal span");
        map.state.triples={{2,1,0},{4,2,0}};map.state.roots={3,1};const auto second=Monstey_RegisterAux(first,map);
        Monstey_Require(second.start==2&&second.count==10&&second.globalRoots==std::vector<std::uint16_t>{1,3,5},"range union/owner root uniqueness");
        map.state.auxiliaryCount=0;Monstey_Require(Monstey_RegisterAux(second,map)==second,"zero-count registration changed owner");
    });
    tests.Monstey_Test("descriptor_keeps_duplicate_roots_and_source_inverse_indices",[]{
        auto source=Monstey_TestRig({100,1,2},{255,0,0});source[1].inverseBind=Monstey_Translation(-13);source[1].inverseBind.words[3]=0x7fc00011u;
        source[2].inverseBind=Monstey_Translation(-27);
        Monstey_SkeletonMapResult map;map.state.roots={1,5,5,9};map.state.triples={{7,1,0},{5,2,0}};map.offsets={{1,2,3},{4,5,6}};
        Monstey_AuxRange range{5,7,{0,1,3,5}};const auto desc=Monstey_BuildAuxDescriptor(map,source,range);
        Monstey_Require(desc.roots==std::vector<Monstey_CopyRoot>{{8,1},{10,5},{10,5}},"descriptor root cursor/duplicates/missing policy");
        Monstey_Require(desc.inverseBind[0].words[0]==std::bit_cast<std::uint32_t>(-13.0f)&&desc.inverseBind[0].words[3]==0&&desc.inverseBind[1].words[0]==std::bit_cast<std::uint32_t>(-27.0f),"inverse bind selected by target rather than source or W not cleared");
        Monstey_Require(desc.offsets==map.offsets&&desc.triples==map.state.triples,"descriptor reordered pairs");
        const auto initial=Monstey_PoisonScratch();std::vector<Monstey_Pose32> evaluated(10,Monstey_Translation(55));evaluated[3]=Monstey_Translation(33);
        const auto copy=Monstey_CopyExtraRoots(range,evaluated,initial);
        Monstey_Require(copy[9]==evaluated[3]&&copy[0]==initial[0],"extra evaluated roots not appended at range extent");
        auto bad=range;bad.globalRoots={10};Monstey_Rejects([&]{(void)Monstey_CopyExtraRoots(bad,evaluated,initial);});
    });
    tests.Monstey_Test("descriptor_installation_deep_copy_explicit_fresh_bytes_reuse_and_null",[]{
        const auto in=Monstey_OneAuxFixture();Monstey_AuxOwner owner;
        Monstey_Rejects([&]{(void)Monstey_InstallAux(owner,in.descriptor);});Monstey_Require(owner.scratch.empty()&&!owner.descriptor,"failed first installation modified owner");
        auto desc=in.descriptor;const auto scratch=Monstey_PoisonScratch();owner=Monstey_InstallAux(owner,desc,scratch);desc.roots.clear();
        Monstey_Require(owner.descriptor->roots.size()==1&&owner.scratch==scratch,"descriptor shallow copy or assumed initialization");
        owner.scratch[13]=Monstey_Translation(42);const auto again=Monstey_InstallAux(owner,in.descriptor);
        Monstey_Require(again.scratch[13]==owner.scratch[13],"nonnull replacement reset scratch");
        const auto empty=Monstey_InstallAux(again,std::nullopt);Monstey_Require(!empty.descriptor&&empty.scratch.empty(),"null descriptor failed to release scratch");
    });
    tests.Monstey_Test("auxiliary_target_index_and_source_inverse_are_distinct",[]{
        const auto in=Monstey_OneAuxFixture();const auto out=Monstey_DrawGather(in);
        Monstey_Require(out.auxiliaryUsed&&out.written==std::vector<bool>{true}&&Monstey_PoseNear(out.output[0],{9,0,0,0},{0,0,0,1}),"single auxiliary literal result");
        Monstey_Require(out.scratch[0]==in.auxiliary[2]&&out.scratch[1]==in.scratch[1]&&out.scratch[2]==out.output[0],"auxiliary stored at source ordinal instead of target");
    });
    tests.Monstey_Test("all_reconstruction_precedes_all_inverse_bind_passes",[]{
        auto source=Monstey_TestRig({100,9,10},{255,0,1}),target=Monstey_TestRig({100,8,9,10},{255,0,0,2});
        source[1].localPosition={2,0,0};source[2].localPosition={1,0,0};target[2].localPosition={3,0,0};target[3].localPosition={1,0,0};Monstey_TestTranslationBinds(source);
        const auto map=Monstey_BuildClassMap(source,target,0,0);const auto range=Monstey_RegisterAux({},map);
        auto in=Monstey_OneAuxFixture();in.remap=map.state.remap;in.descriptor=Monstey_BuildAuxDescriptor(map,source,range);in.palette={1,2};
        in.auxiliary={Monstey_Translation(7),Monstey_Translation(4),Monstey_Translation(999),Monstey_Translation(5)};in.initialOutput={Monstey_Translation(-1),Monstey_Translation(-2)};
        const auto out=Monstey_DrawGather(in);
        Monstey_Require(Monstey_PoseNear(out.output[0],{9,0,0,0},{0,0,0,1})&&Monstey_PoseNear(out.output[1],{12,0,0,0},{0,0,0,1}),"per-node inverse pass contaminated child reconstruction");
    });
    tests.Monstey_Test("noncommuting_quaternion_operands_have_independent_literal_result",[]{
        const float s=std::sqrt(0.5f);const auto product=Monstey_MultiplyQ({0,s,0,s},{s,0,0,s});
        for(unsigned i=0;i<4;++i)Monstey_Require(std::fabs(product[i]-0.5f)<0.000001f,"native helper must return second times first");
        const auto reversed=Monstey_MultiplyQ({s,0,0,s},{0,s,0,s});
        Monstey_Require(reversed[2]<-0.499999f,"noncommuting literal did not distinguish operand reversal");
        const auto rotated=Monstey_Rotate4({0,1,0,0},{s,0,0,s});
        Monstey_Require(std::fabs(rotated[0])<0.000001f&&std::fabs(rotated[1])<0.000001f&&std::fabs(rotated[2]-1)<0.000001f,"native X rotation literal");
    });
    tests.Monstey_Test("quaternion_rotation_and_inverse_order_literal",[]{
        auto in=Monstey_OneAuxFixture();const float s=std::sqrt(0.5f);
        in.auxiliary[0]=Monstey_MakePose({7,0,0,0},{0,0,s,s});in.auxiliary[2]=Monstey_MakePose({10,0,0,0},{0,0,s,s});
        const auto out=Monstey_DrawGather(in);
        Monstey_Require(Monstey_PoseNear(out.output[0],{12,6,0,0},{0,0,1,0}),"Hamilton/rotation operand order");
        const auto scaled=Monstey_Rotate4({2,0,0,0},{0,0,0,2});Monstey_Require(scaled==Monstey_Vec4{8,0,0,0},"rotation silently normalized a nonunit quaternion");
    });
    tests.Monstey_Test("duplicate_target_pairs_sequentially_reapply_inverse",[]{
        auto in=Monstey_OneAuxFixture();in.descriptor.start=1;in.descriptor.count=1;in.descriptor.roots={{1,0}};
        in.descriptor.triples={{1,1,0},{1,2,0}};in.descriptor.offsets={{0,0,0},{0,0,0}};in.descriptor.inverseBind={Monstey_Translation(-1),Monstey_Translation(-2)};
        in.auxiliary={Monstey_Translation(7),Monstey_Translation(5)};in.remap={0,0x1001};const auto out=Monstey_DrawGather(in);
        Monstey_Require(Monstey_PoseNear(out.output[0],{9,0,0,0},{0,0,0,1}),"duplicate target deduplicated or inverse interleaved");
    });
    tests.Monstey_Test("active_auxiliary_full_u16_invalid_primary_leaves_output_unwritten",[]{
        auto in=Monstey_OneAuxFixture();in.remap[1]=0x0201;const auto original=in.initialOutput[0];const auto active=Monstey_DrawGather(in);
        Monstey_Require(active.auxiliaryUsed&&!active.written[0]&&active.output[0]==original,"invalid full-U16 index replaced output");
        in.incoming=0;const auto ordinary=Monstey_DrawGather(in);Monstey_Require(ordinary.output[0]==in.primary[1]&&ordinary.written[0],"ordinary low-byte projection");
    });
    tests.Monstey_Test("flagged_ffff_uses_snapshot_255_while_ordinary_uses_primary_255",[]{
        auto in=Monstey_OneAuxFixture();in.descriptor={};in.remap[1]=0xffff;const auto active=Monstey_DrawGather(in);
        Monstey_Require(active.output[0]==in.scratch[255],"flagged remap low-byte scratch selection");
        in.previous=true;Monstey_Require(Monstey_DrawGather(in).output[0]==Monstey_IdentityPose,"ordinary out-of-range identity");
        in.primary.resize(256,Monstey_Translation(0));in.primary[255]=Monstey_Translation(255);Monstey_Require(Monstey_DrawGather(in).output[0]==in.primary[255],"previous low byte must remain primary");
    });
    tests.Monstey_Test("each_auxiliary_guard_and_previous_selection_disables_preparation",[]{
        const auto fixture=Monstey_OneAuxFixture();
        for(unsigned guard=0;guard<5;++guard){auto in=fixture;if(guard==0)in.incoming=0;if(guard==1)in.descriptorPresent=false;if(guard==2)in.instanceEnable=0;if(guard==3)in.globalEnable=0;if(guard==4)in.previous=true;
            const auto out=Monstey_DrawGather(in);Monstey_Require(!out.auxiliaryUsed&&out.output[0]==in.primary[2]&&out.scratch==in.scratch,"guard or previous selection entered auxiliary path");}
        auto in=fixture;in.incoming=in.instanceEnable=in.globalEnable=255;Monstey_Require(Monstey_DrawGather(in).auxiliaryUsed,"nonzero guard treated as exact one");
    });
    tests.Monstey_Test("null_primary_null_remap_and_missing_auxiliary_contracts",[]{
        auto in=Monstey_OneAuxFixture();in.primaryPresent=false;in.scratch.clear();in.descriptor.triples.clear();
        const auto absent=Monstey_DrawGather(in);Monstey_Require(absent.output[0]==Monstey_IdentityPose&&!absent.auxiliaryUsed,"null primary did not short circuit");
        in=Monstey_OneAuxFixture();in.remapPresent=false;in.descriptor.triples.clear();const auto direct=Monstey_DrawGather(in);
        Monstey_Require(direct.output[0]==in.primary[1]&&!direct.auxiliaryUsed,"null remap used auxiliary");
        in=Monstey_OneAuxFixture();in.auxiliaryPresent=false;Monstey_Rejects([&]{(void)Monstey_DrawGather(in);});in.incoming=0;Monstey_Require(!Monstey_DrawGather(in).auxiliaryUsed,"inactive auxiliary pointer was dereferenced");
        in.remap.clear();Monstey_Rejects([&]{(void)Monstey_DrawGather(in);});
    });
    tests.Monstey_Test("zero_draw_skips_invalid_auxiliary_and_65_draw_clamps_64",[]{
        auto in=Monstey_OneAuxFixture();in.palette.clear();in.initialOutput.clear();in.scratch.clear();in.remap.clear();const auto empty=Monstey_DrawGather(in);
        Monstey_Require(empty.output.empty()&&!empty.auxiliaryUsed,"empty draw prepared auxiliary scratch");
        in=Monstey_OneAuxFixture();in.palette.assign(65,1);in.initialOutput.assign(64,Monstey_Translation(-1));const auto clipped=Monstey_DrawGather(in);
        Monstey_Require(clipped.output.size()==64&&std::all_of(clipped.written.begin(),clipped.written.end(),[](bool value){return value;}),"draw limit changed");
        in.palette.assign(256,1);Monstey_Rejects([&]{(void)Monstey_DrawGather(in);});
    });
    tests.Monstey_Test("descriptor_bounds_are_transactional_and_only_used_when_active",[]{
        const auto original=Monstey_OneAuxFixture();
        for(unsigned fault=0;fault<6;++fault){auto in=original;if(fault==0)in.descriptor.offsets.clear();if(fault==1)in.descriptor.inverseBind.clear();if(fault==2)in.descriptor.roots[0].target=256;if(fault==3)in.descriptor.roots[0].auxiliary=99;if(fault==4)in.descriptor.triples[0].target=1;if(fault==5)in.descriptor.triples[0].parent=256;
            const auto saved=in.scratch;Monstey_Rejects([&]{(void)Monstey_DrawGather(in);});Monstey_Require(in.scratch==saved,"invalid descriptor changed snapshot");
            in.incoming=0;Monstey_Require(!Monstey_DrawGather(in).auxiliaryUsed,"inactive malformed descriptor was traversed");}
    });
    tests.Monstey_Test("raw_root_copies_and_missing_roots_do_not_invent_identity",[]{
        auto in=Monstey_OneAuxFixture();in.descriptor={};in.descriptor.roots={{0,255}};in.remap[1]=0x10ff;
        in.auxiliary[0].words={0x7fa12345u,0x80000000u,0xffc00001u,0,0,0,0,0x3f800000u};const auto out=Monstey_DrawGather(in);
        Monstey_Require(out.output[0]==in.auxiliary[0],"copy-only path changed raw words");
        in.descriptor.roots.clear();Monstey_Require(Monstey_DrawGather(in).output[0]==in.scratch[255],"unwritten scratch invented identity");
    });
    tests.Monstey_Test("forced_identity_ordinals_can_have_auxiliary_pairs_without_a_remap",[]{
        auto source=Monstey_TestRig({100,9},{255,0});source[1].localPosition={2,0,0};Monstey_TestTranslationBinds(source);
        const auto map=Monstey_BuildClassMap(source,source,0,1);Monstey_Require(map.state.remap.empty()&&map.state.auxiliaryCount==1,"native identity/lazy-map distinction");
        auto in=Monstey_OneAuxFixture();in.remap=map.state.remap;in.remapPresent=!in.remap.empty();in.descriptor=Monstey_BuildAuxDescriptor(map,source,Monstey_RegisterAux({},map));
        Monstey_Require(!Monstey_DrawGather(in).auxiliaryUsed,"empty implicit map incorrectly prepared auxiliary pairs");
    });
    tests.Monstey_Test("MAUX_snapshot_roundtrip_bounds_and_negative_controls",[]{
        const auto original=Monstey_OneAuxFixture();const auto bytes=Monstey_EncodeAuxProfile(original);const auto parsed=Monstey_ParseAuxProfile(bytes);
        Monstey_Require(Monstey_EncodeAuxProfile(parsed)==bytes&&Monstey_DrawGather(parsed).output==Monstey_DrawGather(original).output,"snapshot roundtrip changed contract");
        const std::array<std::size_t,16> cuts{0,1,4,7,8,11,15,19,20,23,43,44,45,100,500,bytes.size()-1};
        for(const auto cut:cuts)Monstey_Rejects([&]{(void)Monstey_ParseAuxProfile(Monstey_View(bytes).first(cut));});
        for(const auto at:{0u,4u,20u,24u,28u,32u,36u,40u}){auto bad=bytes;Monstey_Put32(bad,at,0xffffffffu);Monstey_Rejects([&]{(void)Monstey_ParseAuxProfile(bad);});}
        for(const auto at:{8u,9u,10u,11u,15u}){auto bad=bytes;bad[at]=2;Monstey_Rejects([&]{(void)Monstey_ParseAuxProfile(bad);});}
        auto extra=bytes;extra.push_back(0xa7);Monstey_Rejects([&]{(void)Monstey_ParseAuxProfile(extra);});
    });
}

void Monstey_PrintSkeletonMap(const Monstey_Rig&source,const Monstey_Rig&target,const Monstey_SkeletonMapResult&map,std::uint8_t mode,std::uint8_t force){
    std::cout<<"{\"schema\":\"Monstey.SkeletonMapReplica/1\",\"source_nodes\":"<<source.size()<<",\"target_nodes\":"<<target.size()
        <<",\"mode_byte\":"<<static_cast<unsigned>(mode)<<",\"force_byte\":"<<static_cast<unsigned>(force)
        <<",\"root_matched\":"<<(map.rootMatched?"true":"false")<<",\"target_root\":"<<map.targetRoot<<",\"count_written\":"<<(map.countWritten?"true":"false")
        <<",\"ready\":"<<(map.ready?"true":"false")<<",\"maximum_auxiliary_source\":"<<map.maximumSource<<",\"remap_u16\":[";
    for(std::size_t i=0;i<map.state.remap.size();++i){if(i)std::cout<<',';std::cout<<map.state.remap[i];}
    std::cout<<"],\"root_ordinals\":[";
    for(std::size_t i=0;i<map.state.roots.size();++i){if(i)std::cout<<',';std::cout<<map.state.roots[i];}
    std::cout<<"],\"auxiliary_count\":"<<map.state.auxiliaryCount<<",\"triples\":[";
    for(std::size_t i=0;i<map.state.triples.size();++i){if(i)std::cout<<',';const auto&t=map.state.triples[i];std::cout<<'['<<t.target<<','<<t.source<<','<<t.parent<<']';}
    std::cout<<"],\"offset_xyz_bits\":[";
    for(std::size_t i=0;i<map.offsets.size();++i){if(i)std::cout<<',';std::cout<<'[';for(unsigned j=0;j<3;++j){if(j)std::cout<<',';std::cout<<std::bit_cast<std::uint32_t>(map.offsets[i][j]);}std::cout<<']';}
    std::cout<<"],\"native_rig_profile\":true,\"offline_only\":true,\"original_executable_executed\":false}\n";
}
void Monstey_PrintGather(const Monstey_GatherOutput&out){
    std::cout<<"{\"schema\":\"Monstey.SkeletonMapAuxiliaryGather/1\",\"auxiliary_used\":"<<(out.auxiliaryUsed?"true":"false")<<",\"output_u32\":[";
    for(std::size_t i=0;i<out.output.size();++i){if(i)std::cout<<',';std::cout<<'[';for(unsigned j=0;j<8;++j){if(j)std::cout<<',';std::cout<<out.output[i].words[j];}std::cout<<']';}
    std::cout<<"],\"written\":[";
    for(std::size_t i=0;i<out.written.size();++i){if(i)std::cout<<',';std::cout<<(out.written[i]?"true":"false");}
    std::cout<<"],\"scratch_u32\":[";
    for(std::size_t i=0;i<out.scratch.size();++i){if(i)std::cout<<',';std::cout<<'[';for(unsigned j=0;j<8;++j){if(j)std::cout<<',';std::cout<<out.scratch[i].words[j];}std::cout<<']';}
    std::cout<<"],\"offline_only\":true,\"original_executable_executed\":false}\n";
}
std::uint8_t Monstey_ParseByteArgument(std::string_view text){
    Monstey_Require(!text.empty()&&text.front()!='-',"expected unsigned byte");std::size_t used=0;const auto value=std::stoul(std::string(text),&used,0);
    Monstey_Require(used==text.size()&&value<=255,"argument exceeds byte");return static_cast<std::uint8_t>(value);
}
int Monstey_RunSkeletonMapTests(){
    Monstey_TestSuite tests;Monstey_MapTests(tests);Monstey_AuxTests(tests);
    std::cout<<"{\"schema\":\"Monstey.SkeletonMapTests/1\",\"groups\":"<<tests.total<<",\"failed\":"<<tests.failures<<",\"status\":\""<<(tests.failures?"FAIL":"PASS")<<"\",\"original_executable_executed\":false}\n";
    return tests.failures?1:0;
}
} // namespace
int Monstey_FrozenSkeletonMapEntry(int argc,char**argv){
    try{
        if(argc==1||(argc==2&&std::string_view(argv[1])=="--test"))return Monstey_RunSkeletonMapTests();
        if(argc==5&&std::string_view(argv[1])=="--verify-native"){
            const auto rows=Monstey_VerifyNative(argv[2],argv[3],argv[4]);
            std::cout<<"{\"schema\":\"Monstey.SkeletonMapNativeBytes/1\",\"manifest_rows\":"<<rows<<",\"images\":2,\"differences\":0,\"original_executable_executed\":false}\n";return 0;
        }
        if((argc==4||argc==6)&&(std::string_view(argv[1])=="--map"||std::string_view(argv[1])=="--map-skel")){
            const auto sourceBytes=Monstey_Read(argv[2]),targetBytes=Monstey_Read(argv[3]);const bool skel=std::string_view(argv[1])=="--map-skel";
            const auto source=skel?Monstey_RigFromSkel(sourceBytes):Monstey_ParseRigProfile(sourceBytes),target=skel?Monstey_RigFromSkel(targetBytes):Monstey_ParseRigProfile(targetBytes);
            const auto mode=argc==6?Monstey_ParseByteArgument(argv[4]):std::uint8_t{0},force=argc==6?Monstey_ParseByteArgument(argv[5]):std::uint8_t{0};
            Monstey_PrintSkeletonMap(source,target,Monstey_BuildClassMap(source,target,mode,force),mode,force);return 0;
        }
        if(argc==3&&std::string_view(argv[1])=="--aux"){
            const auto bytes=Monstey_Read(argv[2]);Monstey_PrintGather(Monstey_DrawGather(Monstey_ParseAuxProfile(bytes)));return 0;
        }
        std::cerr<<"Usage: Monstey_SkeletonMapProbe [--test | --map source.msmp target.msmp [modeByte forceByte] | --map-skel source.skel target.skel [modeByte forceByte] | --aux snapshot.maux | --verify-native manifest.txt pinned.exe installed.exe]\n";return 2;
    }catch(const std::exception&error){std::cerr<<"Monstey_SkeletonMapProbe: "<<error.what()<<'\n';return 1;}
}
