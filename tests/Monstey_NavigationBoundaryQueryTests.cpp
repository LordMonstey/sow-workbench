#include "Monstey_NavigationBoundaryQuery.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <immintrin.h>
#include <iostream>
#include <limits>
#include <random>
using namespace Monstey;
namespace {
using V=Monstey_NavigationVector4;using B=Monstey_NavigationAabb;using T=Monstey_NavigationAffine4;
unsigned groups=0,treeCases=0,slabCases=0,quantCases=0,negativeCases=0;
void Check(bool yes,const char*text){Monstey_Require(yes,text);}
void Reject(const std::function<void()>&call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}Check(rejected,"Malformed owned input accepted");++negativeCases;}
T Identity(){return T{{V{1,0,0,0},V{0,1,0,0},V{0,0,1,0},V{0,0,0,1}}};}
void Vertex(Monstey_Bytes&v,unsigned i,const V&value){std::memcpy(v.data()+16*i,value.data(),16);}
void Face(Monstey_Bytes&v,unsigned i,std::int32_t first,std::int16_t count){Monstey_PutU32(v,16*i,std::bit_cast<std::uint32_t>(first));Monstey_PutU16(v,16*i+8,std::bit_cast<std::uint16_t>(count));}
void Edge(Monstey_Bytes&v,unsigned i,std::int32_t a,std::int32_t b,std::uint32_t opposite=0xffffffffu){Monstey_PutU32(v,20*i,std::bit_cast<std::uint32_t>(a));Monstey_PutU32(v,20*i+4,std::bit_cast<std::uint32_t>(b));Monstey_PutU32(v,20*i+8,opposite);}
void Node(Monstey_Bytes&v,unsigned i,unsigned payload,bool internal){v[6*i+3]=static_cast<std::uint8_t>((payload>>16)|(internal?0x80u:0u));Monstey_PutU16(v,6*i+4,static_cast<std::uint16_t>(payload));}
struct Fixture{
 Monstey_Bytes faces=Monstey_Bytes(16),edges=Monstey_Bytes(60),vertices=Monstey_Bytes(48),mappedFaces,mappedEdges,addedFaces,addedEdges,addedVertices,nodes=Monstey_Bytes(6);
 std::vector<std::int32_t>faceMap{-1},edgeMap{-1,-1,-1};T transform=Identity();B bounds{V{-2,0,0,7},V{2,2,0,9}};
 Fixture(){Face(faces,0,0,3);Edge(edges,0,0,1);Edge(edges,1,1,2,1);Edge(edges,2,2,0);Vertex(vertices,0,V{-2,0,0,7});Vertex(vertices,1,V{2,0,0,8});Vertex(vertices,2,V{0,2,0,9});Node(nodes,0,0,false);}
 Monstey_NavigationBoundaryMeshView View()const{return {faces,edges,vertices,faceMap,edgeMap,mappedFaces,mappedEdges,addedFaces,addedEdges,addedVertices,transform};}
 Monstey_NavigationCompactTreeView Tree()const{return {nodes,bounds};}
};
float Add(float a,float b){volatile float x=a+b;return x;}float Sub(float a,float b){volatile float x=a-b;return x;}float Mul(float a,float b){volatile float x=a*b;return x;}
std::vector<std::uint32_t> RefTree(std::span<const std::uint8_t>nodes,const B&bounds,const B&q){
 const auto overlap=[](const B&a,const B&b){for(unsigned k=0;k<3;++k)if(!(a.minimum[k]<=b.maximum[k]&&b.minimum[k]<=a.maximum[k]))return false;return true;};std::vector<std::uint32_t>out;if(nodes.empty()||!overlap(bounds,q))return out;
 const auto decode=[](const B&p,std::span<const std::uint8_t>n){B r=p;for(unsigned k=0;k<3;++k){const auto high=static_cast<float>(n[k]>>4),low=static_cast<float>(n[k]&15);const float scale=Mul(Sub(p.maximum[k],p.minimum[k]),std::bit_cast<float>(std::uint32_t{0x3b90fdbc}));r.minimum[k]=Add(Mul(Mul(high,high),scale),p.minimum[k]);r.maximum[k]=Sub(p.maximum[k],Mul(Mul(low,low),scale));}return r;};
 std::function<void(unsigned,const B&)>walk=[&](unsigned index,const B&box){const auto n=nodes.subspan(index*6,6);const unsigned payload=(static_cast<unsigned>(n[3]&0x7f)<<16)|Monstey_U16(n,4);if(!(n[3]&0x80)){out.push_back(payload);return;}const unsigned left=index+1,right=index+2*payload;const B a=decode(box,nodes.subspan(left*6,6)),b=decode(box,nodes.subspan(right*6,6));if(overlap(a,q))walk(left,a);if(overlap(b,q))walk(right,b);};walk(0,bounds);return out;
}
std::pair<bool,double> RefSlab(const V&a,const V&b,const B&q){double entry=0,exit=1;for(unsigned k=0;k<3;++k){const double d=static_cast<double>(b[k])-a[k];if(d==0){if(a[k]<q.minimum[k]||a[k]>q.maximum[k])return {false,1};continue;}double p=(q.minimum[k]-static_cast<double>(a[k]))/d,r=(q.maximum[k]-static_cast<double>(a[k]))/d;if(p>r)std::swap(p,r);entry=std::max(entry,p);exit=std::min(exit,r);}return {entry<=exit,std::abs(entry-exit)};}
}
int wmain(int argc,wchar_t**argv){try{
Check(argc==2,"Need new output report");const std::filesystem::path report=argv[1];Monstey_RequireOfflineOutput(report);
Fixture f;const B all{V{-3,-1,-1,0},V{3,3,1,0}};auto result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);
Check(result.hits==std::vector<Monstey_NavigationBoundaryHit>{{0,0},{0,1},{0,2}},"Native hit order/keys");Check(result.boundary_boxes.size()==2&&result.boundary_boxes[0].minimum[3]==0&&result.boundary_boxes[1].minimum[3]==2,"Non-boundary hit ordinal hole lost");++groups;
// Tree traversal is left-first, preserves duplicate leaves, and does not sort IDs.
Monstey_Bytes tree(42);Node(tree,0,2,true);Node(tree,1,1,true);Node(tree,2,4,false);Node(tree,3,2,false);Node(tree,4,1,true);Node(tree,5,9,false);Node(tree,6,1,false);const B root{V{-10,-10,-10,0},V{10,10,10,0}};
Check(Monstey_NavigationCompactTreeQuery({tree,root},root)==std::vector<std::uint32_t>{4,2,9,1},"Native left-first DFS order");++treeCases;++groups;
Monstey_Bytes two(18);Node(two,0,1,true);Node(two,1,0,false);Node(two,2,0,false);f.nodes=two;result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.hits.size()==6&&result.boundary_boxes.size()==4,"Repeated native face leaves deduplicated");Check(result.boundary_boxes[3].minimum[3]==5,"Duplicate hit ordinal identity");++groups;
Node(f.nodes,0,1,true);Node(f.nodes,1,0,false);Node(f.nodes,2,0,false);f.nodes[6]=0x08;f.nodes[12]=0x80;const B right{V{5,-1,-1,0},V{6,1,1,0}};Check(Monstey_NavigationCompactTreeQuery({f.nodes,root},right)==std::vector<std::uint32_t>{0},"Nibble bounds right-only query");++treeCases;++groups;
// Independent scalar decoding across random packed child bounds and query boxes.
std::mt19937 rng(0xbfc7d0u);std::uniform_real_distribution<float>value(-12,12);for(unsigned n=0;n<2048;++n){Monstey_Bytes t=tree;for(unsigned i=1;i<7;++i)for(unsigned k=0;k<3;++k)t[i*6+k]=static_cast<std::uint8_t>(rng());B q{};for(unsigned k=0;k<4;++k){q.minimum[k]=value(rng);q.maximum[k]=q.minimum[k]+std::abs(value(rng))/3;}Check(Monstey_NavigationCompactTreeQuery({t,root},q)==RefTree(t,root,q),"Packed tree/scalar decoder differs");++treeCases;}++groups;
Check(Monstey_NavigationCompactTreeQuery({{},root},root).empty(),"Empty tree must be empty");Check(Monstey_NavigationCompactTreeQuery({tree,root},B{V{20,20,20,0},V{21,21,21,0}}).empty(),"Root culling ignored");++groups;
Reject([&]{Monstey_NavigationCompactTreeQuery({std::span(tree).first(41),root},root);});Monstey_Bytes bad(6);Node(bad,0,0,true);Reject([&]{Monstey_NavigationCompactTreeQuery({bad,root},root);});Node(bad,0,3,true);Reject([&]{Monstey_NavigationCompactTreeQuery({bad,root},root);});++groups;
// Full transform route uses native transpose conversion, not a general inverse.
f=Fixture{};f.transform=T{{V{0,1,0,0},V{-1,0,0,0},V{0,0,1,0},V{10,20,30,1}}};result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),B{V{7,17,29,77},V{11,23,31,99}});Check(result.hits.size()==3,"Rotated translated query lost edges");Check(result.local_query.minimum[0]==-3&&result.local_query.maximum[0]==3&&result.local_query.minimum[1]==-1&&result.local_query.maximum[1]==3,"Native transpose local query");++groups;
f=Fixture{};f.transform[0][0]=2;result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),B{V{3,-1,-1,0},V{5,3,1,0}});Check(result.local_query.minimum[0]==6&&result.local_query.maximum[0]==10&&result.hits.empty(),"Unrequested general inverse substituted");++groups;
f=Fixture{};f.transform[0][3]=5;f.transform[1][3]=7;f.transform[2][3]=11;f.transform[3][3]=13;result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.local_query.minimum==all.minimum&&result.local_query.maximum==all.maximum,"W affected XYZ identity fast path");const auto endpoints=Monstey_NavigationBoundaryEndpoints(f.View(),0);Check(endpoints.first[3]==3&&endpoints.second[3]==23,"Endpoint W normalized");++groups;
// Empty mapping arrays select mapped storage at the original index, not original storage.
f=Fixture{};f.faceMap.clear();Reject([&]{Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);});f.mappedFaces=f.faces;f.edgeMap.clear();Reject([&]{Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);});f.mappedEdges=f.edges;Check(Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all).hits.size()==3,"Count-zero mapped storage route");++groups;
f=Fixture{};f.mappedFaces=f.faces;Face(f.mappedFaces,0,1,1);f.faceMap[0]=0;result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.hits==std::vector<Monstey_NavigationBoundaryHit>{{0,1}}&&result.boundary_boxes.empty(),"Face map ignored");++groups;
f=Fixture{};f.mappedEdges=Monstey_Bytes(20);f.addedVertices=Monstey_Bytes(16);Vertex(f.addedVertices,0,V{-1,-1,0,88});Edge(f.mappedEdges,0,3,1);f.edgeMap[0]=0;result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.hits.size()==3&&result.boundary_boxes.size()==2,"Mapped edge/added vertex route");Check(Monstey_NavigationBoundaryEndpoints(f.View(),0).first[0]==-1,"Added vertex selector");++groups;
f=Fixture{};f.addedFaces=Monstey_Bytes(16);f.addedEdges=Monstey_Bytes(20);Face(f.addedFaces,0,3,1);Edge(f.addedEdges,0,0,1);Node(f.nodes,0,1,false);result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.hits==std::vector<Monstey_NavigationBoundaryHit>{{1,3}}&&result.boundary_boxes.size()==1,"Added face/edge route");++groups;
f=Fixture{};f.faces.resize(32);Face(f.faces,1,0,1);f.faceMap.push_back(-1);Node(f.nodes,0,0x400001u,false);result=Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);Check(result.raw_tree_leaves[0]==0x400001u&&result.masked_face_keys[0]==1&&result.hits[0].face_key==1,"22-bit face key mask omitted");++groups;
f=Fixture{};Face(f.faces,0,0,-1);Check(Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all).hits.empty(),"Negative signed edge count iterated");Face(f.faces,0,INT32_MAX,1);Reject([&]{Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);});f=Fixture{};f.edgeMap[0]=-2;Reject([&]{Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);});f=Fixture{};Edge(f.edges,0,-1,1);Reject([&]{Monstey_NavigationBoundaryQuery(f.View(),f.Tree(),all);});++groups;
// Slab predicate is segment-box intersection, not edge-AABB overlap.
const B unit{V{-1,-1,-1,-999},V{1,1,1,999}};Check(!Monstey_NavigationSegmentIntersectsAabb(V{-3,0.5f,0,4},V{0.5f,3,0,8},unit),"Segment falsely accepted from AABB alone");Check(Monstey_NavigationSegmentIntersectsAabb(V{-3,0,0,77},V{3,0,0,-77},unit),"Crossing segment rejected");Check(Monstey_NavigationSegmentIntersectsAabb(V{1,0,0,0},V{1,0,0,0},unit),"Closed-boundary point rejected");Check(!Monstey_NavigationSegmentIntersectsAabb(V{2,0,0,0},V{2,0,0,0},unit),"Outside parallel point accepted");++groups;
for(unsigned n=0;n<8192;++n){V a{},b{};for(unsigned k=0;k<4;++k){a[k]=value(rng);b[k]=value(rng);}const auto expected=RefSlab(a,b,unit);if(expected.second<1e-5)continue;Check(Monstey_NavigationSegmentIntersectsAabb(a,b,unit)==expected.first,"Native slab/independent double clipping disagreement");++slabCases;}++groups;
// Finite zero-direction reciprocal intentionally preserves the native tiny-gap behavior.
{const unsigned old=_mm_getcsr();_mm_setcsr(old&~0x8040u);const float tiny=std::bit_cast<float>(std::uint32_t{0x000116c2u});const B gap{V{tiny,-1,-1,0},V{tiny*2,1,1,0}};const bool hit=Monstey_NavigationSegmentIntersectsAabb(V{},V{},gap);_mm_setcsr(old);Check(hit,"Native finite parallel reciprocal replaced with infinity policy");}++groups;
const B codes{V{-1,-0.0f,0,7},V{1,0,0,9}};const auto encoded=Monstey_NavigationEncodeOrderedAabb(codes,0x12345678u);Check(encoded.minimum==std::array<std::uint32_t,4>{0x203fffff,0x3fffffff,0x40000000,0x12345678}&&encoded.maximum[0]==0x5fc00001&&encoded.maximum[1]==0x40000001,"Ordered float golden bits/hit ordinal");++groups;
for(unsigned n=0;n<8192;++n){float lo=value(rng),hi=value(rng);if(lo>hi)std::swap(lo,hi);const auto q=Monstey_NavigationEncodeOrderedAabb(B{V{lo,lo,lo,lo},V{hi,hi,hi,hi}},n);Check(q.minimum[0]<q.maximum[0]&&q.minimum[3]==n,"Ordered float interval/hit identity");const auto words=[](float x){const std::uint32_t b=std::bit_cast<std::uint32_t>(x);return b&0x80000000u?(~b)>>1:(b^0x80000000u)>>1;};Check(q.minimum[0]==words(lo)&&q.maximum[0]==words(hi)+1,"Ordered float bit mapping");++quantCases;}++groups;
Monstey_WriteNewText(report,Monstey_Json::object({{"schema","Monstey.NavigationBoundaryQueryTests/1"},{"pass",true},{"groups",groups},{"independent_tree_cases",treeCases},{"independent_slab_cases",slabCases},{"independent_quantization_cases",quantCases},{"malformed_input_negatives",negativeCases},{"original_code_executed",false},{"game_process_accessed",false},{"scope","Owned native compact tree query, native slab predicate, exact original/mapped/added selectors and boundary box encoding; no original runtime tree cooker or game traversal test"}}).dump(2)+"\n");std::cout<<"PASS "<<groups<<" groups, tree="<<treeCases<<", slab="<<slabCases<<", quant="<<quantCases<<", negatives="<<negativeCases<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}