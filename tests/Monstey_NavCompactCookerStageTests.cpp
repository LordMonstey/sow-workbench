#include "Monstey_NavCompactCookerStages.hpp"
#include "Monstey_NavigationBoundaryQuery.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
using namespace Monstey;
namespace {
using B=Monstey_NavigationAabb;
void Check(bool ok,const char* message){Monstey_Require(ok,message);}
float ScalarMinimum(float lo,float hi,unsigned q){volatile float extent=(hi-lo)*std::bit_cast<float>(std::uint32_t{0x3b90fdbc});volatile float square=static_cast<float>(q*q);volatile float scaled=square*extent;return lo+scaled;}
float ScalarMaximum(float lo,float hi,unsigned q){volatile float extent=(hi-lo)*std::bit_cast<float>(std::uint32_t{0x3b90fdbc});volatile float square=static_cast<float>(q*q);volatile float scaled=square*extent;return hi-scaled;}
std::array<std::uint8_t,3> Reference(const B&p,const B&c){std::array<std::uint8_t,3>out{};for(unsigned a=0;a<3;++a){unsigned upper=0,lower=0;for(unsigned q=1;q<=15;++q)if(c.minimum[a]>=ScalarMinimum(p.minimum[a],p.maximum[a],q))upper=q;else break;for(unsigned q=1;q<=15;++q)if(!(c.maximum[a]>ScalarMaximum(p.minimum[a],p.maximum[a],q)))lower=q;else break;out[a]=static_cast<std::uint8_t>(upper*16+lower);}return out;}
std::vector<std::uint32_t> Query(const Monstey_NavCompactTreeOwned&t,const B&q){return Monstey_NavigationCompactTreeQuery({t.nodes,t.bounds},q);}
}
int main(int argc,char**argv){try{Check(argc==2,"One output path required");unsigned groups=0,cases=0;
const B root{{-1,-2,-3,5},{4,2,3,11}};
const B a{{-1,-2,-3,5},{1,2,3,7}},b{{2,-2,-3,9},{4,2,3,11}};
Check(Monstey_NavCompactQuantize(root,a)==std::array<std::uint8_t,3>{11,0,0},"Known left quantizer bytes");Check(Monstey_NavCompactQuantize(root,b)==std::array<std::uint8_t,3>{176,0,0},"Known right quantizer bytes");++groups;cases+=2;
B flat{{3,3,3,3},{3,3,3,3}};Check(Monstey_NavCompactQuantize(flat,flat)==std::array<std::uint8_t,3>{255,255,255},"Zero extent must advance to FF");++groups;++cases;
std::mt19937 rng(0x47414230);std::uniform_real_distribution<float>value(-10000,10000),unit(0,1);
for(unsigned n=0;n<4096;++n){B p{},c{};for(unsigned axis=0;axis<4;++axis){p.minimum[axis]=value(rng);p.maximum[axis]=p.minimum[axis]+std::abs(value(rng))+1;c.minimum[axis]=p.minimum[axis]+unit(rng)*(p.maximum[axis]-p.minimum[axis]);c.maximum[axis]=c.minimum[axis]+unit(rng)*(p.maximum[axis]-c.minimum[axis]);}const auto encoded=Monstey_NavCompactQuantize(p,c);Check(encoded==Reference(p,c),"Independent scalar quantizer differs");const auto decoded=Monstey_NavCompactDecode(p,{encoded[0],encoded[1],encoded[2],128});for(unsigned axis=0;axis<3;++axis)Check(decoded.minimum[axis]<=c.minimum[axis]&&decoded.maximum[axis]>=c.maximum[axis],"Encoded child must be conservatively bounded");++cases;}++groups;
const auto w0=Monstey_NavCompactDecode(root,{0,0,0,0}),w8=Monstey_NavCompactDecode(root,{0,0,0,143});Check(w0.minimum[3]==5&&w8.minimum[3]>5&&w8.maximum[3]==root.maximum[3],"Explicit fourth encoded byte affects W");for(unsigned axis=0;axis<3;++axis)Check(w0.minimum[axis]==w8.minimum[axis]&&w0.maximum[axis]==w8.maximum[axis],"Encoded W cannot affect XYZ");++groups;++cases;
const std::array boxes{a,b};const auto inserted=Monstey_NavInsertHierarchy(boxes);Check(inserted.root==3&&inserted.leaf_count==2&&inserted.nodes.size()==4,"Native insertion identities");Check(inserted.nodes[3].child0==1&&inserted.nodes[3].child1_or_payload==2&&inserted.nodes[1].parent==3&&inserted.nodes[2].parent==3,"Native insertion links");Check(inserted.nodes[3].bounds.minimum==root.minimum&&inserted.nodes[3].bounds.maximum==root.maximum,"Native all-four-lane insertion bounds");++groups;cases+=3;
const auto preorder=Monstey_NavPreorderHierarchy(inserted);Check(preorder.root==1&&preorder.nodes[1].child0==2&&preorder.nodes[1].child1_or_payload==3&&preorder.nodes[2].child1_or_payload==0&&preorder.nodes[3].child1_or_payload==1,"Left-first preorder payload preservation");++groups;++cases;
const auto packed=Monstey_NavPackPreorder(preorder);const std::vector<std::uint8_t>expected{0,0,0,128,1,0,11,0,0,0,0,0,176,0,0,0,1,0};Check(packed.nodes==expected,"Known native two-leaf compact bytes");Check(packed.bounds.minimum==root.minimum&&packed.bounds.maximum==root.maximum,"Stored root all-four bounds");++groups;cases+=2;
Check(Query(packed,{{-0.5f,-1,-1,0},{0.5f,1,1,0}})==std::vector<std::uint32_t>{0},"Compact consumer left leaf");Check(Query(packed,{{3,-1,-1,0},{3.5f,1,1,0}})==std::vector<std::uint32_t>{1},"Compact consumer right leaf");Check(Query(packed,root)==std::vector<std::uint32_t>({0,1}),"Compact consumer left-first full query");++groups;cases+=3;
const std::array<B,3>equal{flat,flat,flat};const auto ties=Monstey_NavInsertHierarchy(equal);Check(ties.root==3&&ties.nodes[3].child0==5&&ties.nodes[5].child0==1&&ties.nodes[5].child1_or_payload==4,"Native insertion equal metric chooses child0");++groups;++cases;
const auto deep=Monstey_NavPreorderHierarchy(ties);Check(deep.nodes[1].child0==2&&deep.nodes[1].child1_or_payload==5&&deep.nodes[2].child0==3&&deep.nodes[2].child1_or_payload==4&&deep.nodes[3].child1_or_payload==0&&deep.nodes[4].child1_or_payload==2&&deep.nodes[5].child1_or_payload==1,"Native nested left-first preorder must finish subtree before root sibling");++groups;++cases;
const auto empty=Monstey_NavInsertHierarchy({});Check(empty.root==0&&empty.leaf_count==0&&empty.nodes.size()==1,"Empty insertion stage");const auto emptyPacked=Monstey_NavPackPreorder(empty);Check(emptyPacked.nodes.empty(),"Empty native packing has no nodes");for(unsigned axis=0;axis<4;++axis)Check(std::bit_cast<std::uint32_t>(emptyPacked.bounds.minimum[axis])==0x7f7fffee&&std::bit_cast<std::uint32_t>(emptyPacked.bounds.maximum[axis])==0xff7fffee,"Empty native packing sentinel bits differ");Check(Query(emptyPacked,root).empty(),"Empty compact consumer must return no faces");++groups;cases+=3;bool rejected=false;
auto cyclic=inserted;cyclic.nodes[3].child0=3;rejected=false;try{(void)Monstey_NavPreorderHierarchy(cyclic);}catch(const std::exception&){rejected=true;}Check(rejected,"Owned invalid hierarchy cycle rejected");++groups;++cases;
Monstey_WriteNewText(argv[1],"{\"status\":\"PASS\",\"scope\":\"isolated native cooker stages; full cooker tested separately\",\"groups\":"+std::to_string(groups)+",\"cases\":"+std::to_string(cases)+",\"original_program_executed\":false}\n");std::cout<<"PASS "<<groups<<" stage groups "<<cases<<" cases\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
