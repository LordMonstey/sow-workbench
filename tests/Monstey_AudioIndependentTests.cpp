#include "Monstey_GadbAudioActionWord.hpp"
#include "Monstey_Json.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_AU64=std::uint64_t;using Monstey_AU32=std::uint32_t;
std::uint64_t Monstey_Checks=0,Monstey_Cases=0,Monstey_Groups=0,Monstey_FixtureStores=0;
void Monstey_Check(bool v,const char* text){++Monstey_Checks;if(!v)throw std::runtime_error(text);}
constexpr std::array Monstey_Paths{Monstey_GadbAudioActionPath::SwitchName,Monstey_GadbAudioActionPath::SwitchValue,Monstey_GadbAudioActionPath::RtpcName,Monstey_GadbAudioActionPath::Variable};
struct Monstey_AudioCase{
 Monstey_AU64 base=0x7000001200000000ull;std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x18000);
 Monstey_GadbAudioActionClasses ids;
 Monstey_AU64 holder,parent,child,fields[2],tables[2],values[2];
 explicit Monstey_AudioCase(Monstey_AU64 b=0x7000001200000000ull):base(b),ids{b+0x100,b+0x180,b+0x200,b+0x280,0xfedc12349876abcdull},holder(b+0x400),parent(b+0x500),child(b+0x600),fields{b+0x2000,b+0x4000},tables{b+0x6000,b+0x8000},values{b+0xa000,b+0xc000}{
  Put(ids.action,fields[0],8);Put(ids.set_switch,fields[1],8);Put(ids.set_rtpc,fields[1],8);Put(ids.variable_operation,fields[1],8);Put(holder,parent,8);
 }
 void Put(Monstey_AU64 a,Monstey_AU64 v,std::size_t n){const auto off=a-base;++Monstey_FixtureStores;if(!(a>=base&&off<=bytes.size()&&n<=bytes.size()-static_cast<std::size_t>(off)))throw std::runtime_error("test fixture store out of range");for(std::size_t i=0;i<n;++i)bytes[static_cast<std::size_t>(off)+i]=static_cast<std::uint8_t>(v>>(i*8));}
 Monstey_AU64 Field(Monstey_GadbAudioActionPath p,unsigned stage)const{const auto n=stage?(p==Monstey_GadbAudioActionPath::SwitchValue?2u:0u):(p==Monstey_GadbAudioActionPath::Variable?21u:(p==Monstey_GadbAudioActionPath::RtpcName?10u:9u));return fields[stage]+n*72;}
 Monstey_AU64 Fallback(Monstey_GadbAudioActionPath p)const{return p==Monstey_GadbAudioActionPath::Variable?0:ids.empty_string;}
 Monstey_AU64 Run(Monstey_GadbAudioActionPath p){return Monstey_GadbReadAudioActionWord({base,bytes},holder,p,ids);}
 void Cached(Monstey_GadbAudioActionPath p,unsigned s,Monstey_AU64 result,unsigned physical=0,unsigned word=0,Monstey_AU32 state=2){const auto f=Field(p,s),r=s?child:parent;Put(f+48,state,4);Put(f+64,physical,1);Put(r+8,tables[s],8);Put(r+16,values[s],8);Put(tables[s]+physical*16+10,0xff,1);Put(tables[s]+physical*16+12,1,2);Put(tables[s]+physical*16+14,word,2);Put(values[s]+word*8,result,8);}
 void Both(Monstey_GadbAudioActionPath p,Monstey_AU64 result=0x8123456789abcdefull){Cached(p,0,child);Cached(p,1,result);}
 void Fault(Monstey_GadbAudioActionPath p,Monstey_AU64 expected){const auto before=bytes;bool caught=false;try{(void)Run(p);}catch(const Monstey_GadbBinderFault&e){caught=e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable&&e.address==expected;}Monstey_Check(caught,"consumed fault order differs");Monstey_Check(before==bytes,"getter mutated borrowed bytes");++Monstey_Cases;}
};
Monstey_AU32 Monstey_IndependentSize(Monstey_AU32 t,Monstey_AU32 n,Monstey_AU32 f){if((f&0x280u)!=0)return 0;if(n!=1)return 16;switch(t){case 1:return 5;case 8:return 12;case 9:return 16;case 2:case 3:case 4:case 5:case 6:case 7:case 10:case 11:case 12:case 13:case 14:case 15:case 16:case 17:return 8;default:return 0;}}
void Monstey_Configure(Monstey_AudioCase&a,Monstey_GadbAudioActionPath p,unsigned s,unsigned mode,unsigned pick,Monstey_AU64 result,Monstey_AU32 prefixType=8,Monstey_AU32 prefixCount=1,Monstey_AU32 prefixFlags=0,bool dynamic=false){
 const auto field=a.Field(p,s),record=s?a.child:a.parent;const auto required=s&&p!=Monstey_GadbAudioActionPath::Variable?4u:10u;
 if(mode==0){a.Cached(p,s,result,pick%256,pick%7,static_cast<Monstey_AU32>(2+(pick%5)));return;}
 constexpr std::array<Monstey_AU32,9> keys{0,1,19,0x7ffffffeu,0x7fffffffu,0x80000000u,0x80000001u,0xfffffffeu,0xffffffffu};
 const auto target=pick%keys.size();a.Put(field+48,pick&1?0xffffffffu:1,4);a.Put(field+32,keys[target],4);a.Put(record,keys.size(),1);a.Put(record+1,(mode==2||mode==4?0u:1u)|(mode>=3?16u:0u),1);a.Put(record+8,a.tables[s],8);a.Put(record+16,a.values[s],8);
 for(unsigned i=0;i<keys.size();++i){const auto row=a.tables[s]+i*16;a.Put(row+(mode==2||mode==4?4u:0u),keys[i],4);a.Put(row+9,8-i,1);a.Put(row+10,required|0xc0u,1);a.Put(row+12,1,2);a.Put(row+14,i%7,2);}
 const auto physical=mode==2||mode==4?8-target:target;const auto row=a.tables[s]+physical*16;a.Put(a.values[s]+(physical%7)*8,result,8);
 if(mode<3)return;
 const auto owner=a.base+0xe000+s*0x1000,klass=owner+0x80,decls=owner+0x100;a.Put(record+24,owner,8);a.Put(owner+32,klass,8);a.Put(klass,decls,8);a.Put(klass+8,3,4);
 // Explicit linear declarations: first differs, second matches, third duplicates.
 a.Put(decls+32,keys[target]^0x12345678u,4);a.Put(decls+36,prefixType,4);a.Put(decls+40,prefixCount,4);a.Put(decls+44,prefixFlags,4);
 a.Put(decls+72+32,keys[target],4);a.Put(decls+72+36,0xeeeeeeeeu,4);a.Put(decls+72+40,dynamic?0xffffffffu:0,4);a.Put(decls+144+32,keys[target],4);a.Put(decls+144+40,1,4);
 const auto off=(Monstey_IndependentSize(prefixType,prefixCount,prefixFlags)+7u)&~7u;const auto at=a.values[s]+off;
 if(dynamic){a.Put(at,a.base+0x12000+s*0x1000,8);a.Put(at+8,0x80000000u|pick,4);a.Put(a.base+0x12000+s*0x1000,result,8);}else a.Put(at,result,8);
 (void)row;
}
void Monstey_Models(){std::mt19937_64 rng(0x9876543210ull);for(auto p:Monstey_Paths)for(unsigned pm=0;pm<5;++pm)for(unsigned cm=0;cm<5;++cm)for(unsigned n=0;n<129;++n){Monstey_AudioCase a;const auto value=n%17==0?0:rng();Monstey_Configure(a,p,0,pm,n,a.child,static_cast<Monstey_AU32>(rng()%21),n%3==0?0xffffffffu:n%3,static_cast<Monstey_AU32>((rng()&1)?0:0x280),n%2!=0);Monstey_Configure(a,p,1,cm,128-n,value,static_cast<Monstey_AU32>(rng()%21),n%3==0?0xffffffffu:n%3,static_cast<Monstey_AU32>((rng()&1)?0:0x200),n%3==0);const auto before=a.bytes;Monstey_Check(a.Run(p)==value,"declarative valid model differs");Monstey_Check(a.bytes==before,"read mutated model");++Monstey_Cases;}++Monstey_Groups;}
void Monstey_TypeAndState(){for(auto p:Monstey_Paths)for(unsigned s=0;s<2;++s)for(unsigned t=0;t<256;++t)for(auto state:std::array<Monstey_AU32,5>{0xffffffffu,1,2,0x7fffffffu,0x80000000u}){Monstey_AudioCase a;a.Both(p);const auto value=0x8123456789abcdefull;const bool cache=std::bit_cast<std::int32_t>(state)>=2;if(!cache)Monstey_Configure(a,p,s,1,4,s?value:a.child);a.Put(a.Field(p,s)+48,state,4);const auto physical=cache?0u:4u;const auto row=a.tables[s]+physical*16;a.Put(row+10,t,1);const auto expected=cache||((t&63u)==(s&&p!=Monstey_GadbAudioActionPath::Variable?4u:10u))?value:a.Fallback(p);Monstey_Check(a.Run(p)==expected,"typed/cache fallback differs");++Monstey_Cases;}++Monstey_Groups;}
void Monstey_CacheWidths(){for(auto p:Monstey_Paths)for(unsigned physical=0;physical<256;++physical){Monstey_AudioCase a;a.Cached(p,0,a.child,physical,3);a.Cached(p,1,0xa987654321012345ull,255-physical,4);Monstey_Check(a.Run(p)==0xa987654321012345ull,"cache byte narrowed");++Monstey_Cases;}for(auto p:Monstey_Paths)for(auto word:std::array<unsigned,3>{32767,32768,65535}){Monstey_AudioCase a;a.bytes.resize(0xb0000);a.Cached(p,0,a.child,255,word);a.Cached(p,1,0x1234000056780000ull,254,word);Monstey_Check(a.Run(p)==0x1234000056780000ull,"word offset narrowed");++Monstey_Cases;}++Monstey_Groups;}
void Monstey_Faults(){for(auto p:Monstey_Paths){const auto bad=0xfedcba9876543200ull;{Monstey_AudioCase a;a.Both(p);a.ids.action=bad;a.holder=bad+100;a.Fault(p,bad);}{Monstey_AudioCase a;a.Both(p);a.holder=bad;a.Put(a.ids.action,bad,8);a.Fault(p,bad);}{Monstey_AudioCase a;a.Both(p);a.Put(a.holder,0,8);a.Put(a.ids.action,bad,8);const auto ordinal=p==Monstey_GadbAudioActionPath::Variable?21u:p==Monstey_GadbAudioActionPath::RtpcName?10u:9u;a.Fault(p,bad+ordinal*72+48);}{Monstey_AudioCase a;a.Both(p);a.Put(a.holder,0,8);if(p==Monstey_GadbAudioActionPath::Variable)a.ids.variable_operation=bad;else if(p==Monstey_GadbAudioActionPath::RtpcName)a.ids.set_rtpc=bad;else a.ids.set_switch=bad;a.Fault(p,bad);}{Monstey_AudioCase a;a.Both(p);a.Put(a.holder,0,8);if(p==Monstey_GadbAudioActionPath::Variable)a.Put(a.ids.variable_operation,bad,8);else if(p==Monstey_GadbAudioActionPath::RtpcName)a.Put(a.ids.set_rtpc,bad,8);else a.Put(a.ids.set_switch,bad,8);a.Fault(p,bad+(p==Monstey_GadbAudioActionPath::SwitchValue?144u:0u)+48);}
 for(bool indirect:std::array{false,true}){Monstey_AudioCase a(0);a.Both(p);const auto ordinal=p==Monstey_GadbAudioActionPath::Variable?21u:p==Monstey_GadbAudioActionPath::RtpcName?10u:9u;const auto f=~Monstey_AU64(0)-47;a.Put(a.ids.action,f-ordinal*72,8);a.Put(0,1,4);a.Put(a.parent,1,1);a.Put(a.parent+1,indirect?0:1,1);a.Put(a.parent+8,bad,8);a.Fault(p,f+32);a.Put(a.parent,0,1);Monstey_Check(a.Run(p)==a.Fallback(p),"zero count consumed hash");++Monstey_Cases;}
 {Monstey_AudioCase a;a.Both(p);a.Put(a.Field(p,0)+48,1,4);const auto end=a.base+a.bytes.size();a.Put(a.holder,end-2,8);a.Put(end-2,0,1);a.Put(end-1,1,1);a.Fault(p,end+6);}
 for(bool cached:std::array{false,true}){Monstey_AudioCase a;a.Both(p);if(!cached){a.Put(a.Field(p,0)+48,1,4);a.Put(a.Field(p,0)+32,123,4);a.Put(a.parent,1,1);a.Put(a.parent+1,1,1);}const auto row=a.base+a.bytes.size()-14;a.Put(a.parent+8,row,8);a.Put(row,123,4);a.Put(row+10,10,1);a.Put(row+12,0,2);Monstey_Check(a.Run(p)==a.Fallback(p),"zero descriptor count consumed offset");++Monstey_Cases;}
 {Monstey_AudioCase a;a.Both(p);a.Put(a.Field(p,0)+48,1,4);a.Put(a.Field(p,0)+32,123,4);a.Put(a.parent,1,1);a.Put(a.parent+1,1,1);const auto row=a.base+a.bytes.size()-11;a.Put(a.parent+8,row,8);a.Put(row,123,4);a.Put(row+10,4,1);Monstey_Check(a.Run(p)==a.Fallback(p),"wrong type consumed descriptor count");++Monstey_Cases;}}
 ++Monstey_Groups;}
Monstey_AU64 Monstey_BadPointer();
void Monstey_GeneratedBoundaries(){for(auto p:Monstey_Paths)for(unsigned s=0;s<2;++s){for(auto flags:std::array<Monstey_AU32,7>{0,0x80,0x200,0x280,0x400,0xffffffffu,0x100})for(Monstey_AU32 type=0;type<=21;++type)for(auto count:std::array<Monstey_AU32,4>{0,1,2,0xffffffffu}){Monstey_AudioCase a;a.Both(p);Monstey_Configure(a,p,s,3,5,s?0x1122334455667788ull:a.child,type,count,flags,true);Monstey_Check(a.Run(p)==(s?0x1122334455667788ull:0x8123456789abcdefull),"generated prefix size/alignment differs");++Monstey_Cases;}
 {Monstey_AudioCase a;a.Both(p);Monstey_Configure(a,p,s,3,5,s?17:a.child,8,1,0,true);const auto at=a.values[s]+16;a.Put(at,Monstey_BadPointer(),8);a.Put(at+8,0,4);bool assertion=false;try{(void)a.Run(p);}catch(const Monstey_GadbScalarFault&e){assertion=e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion&&e.address==at+8;}Monstey_Check(assertion,"dynamic zero did not assert before pointer read");++Monstey_Cases;}
 }++Monstey_Groups;}
Monstey_AU64 Monstey_BadPointer(){return 0xfedc123456780000ull;}
void Monstey_ZeroDeclaration(){for(auto p:Monstey_Paths){Monstey_AudioCase a(0);a.Both(p);Monstey_Configure(a,p,1,3,5,0x9988776655443322ull);const auto owner=a.base+0xf000,klass=owner+0x80;a.Put(klass,0,8);a.Put(klass+8,1,4);a.Put(32,0x80000000u,4);a.Put(36,0xffffffffu,4);a.Put(40,1,4);a.Put(a.values[1],0x9988776655443322ull,8);Monstey_Check(a.Run(p)==0x9988776655443322ull,"found declaration address zero treated absent");++Monstey_Cases;}++Monstey_Groups;}

void Monstey_GeneratedMissing(){for(auto p:Monstey_Paths)for(unsigned stage=0;stage<2;++stage)for(unsigned reason=0;reason<4;++reason){Monstey_AudioCase a;a.Both(p);Monstey_Configure(a,p,stage,3,5,stage?7:a.child);const auto rec=stage?a.child:a.parent,owner=a.base+0xe000+stage*0x1000;
 if(reason==0)a.Put(rec+24,0,8);else if(reason==1)a.Put(owner+32,0,8);else if(reason==2){a.Put(owner+32,a.base-8,8);a.Put(a.base,0,4);}else{const auto decls=owner+0x100;a.Put(decls+72+32,12,4);a.Put(decls+144+32,13,4);a.Put(rec+16,Monstey_BadPointer(),8);}
 Monstey_Check(a.Run(p)==a.Fallback(p),"generated absent owner/class/count/hash consumed storage");++Monstey_Cases;}
 for(auto p:Monstey_Paths)for(unsigned stage=0;stage<2;++stage){Monstey_AudioCase a;a.Both(p);Monstey_Configure(a,p,stage,3,5,stage?7:a.child,8,1,0,true);const auto at=a.values[stage]+16;a.Put(at,Monstey_BadPointer(),8);a.Put(at+8,1,4);a.Fault(p,Monstey_BadPointer());}++Monstey_Groups;}
void Monstey_DefaultAndInvalid(){const Monstey_GadbAudioActionClasses c;Monstey_Check(c.action==0x14264c660ull&&c.set_switch==0x14264c560ull&&c.set_rtpc==0x14264c520ull&&c.variable_operation==0x14264c5a0ull&&c.empty_string==0x141db10e0ull,"native default identities changed");++Monstey_Cases;Monstey_AudioCase a;bool caught=false;try{(void)a.Run(static_cast<Monstey_GadbAudioActionPath>(123));}catch(const std::invalid_argument&){caught=true;}Monstey_Check(caught,"invalid owned enum accepted");++Monstey_Cases;++Monstey_Groups;}

void Monstey_Alias(){for(auto p:Monstey_Paths){Monstey_AudioCase a;a.child=a.parent;a.tables[1]=a.tables[0];a.values[1]=a.values[0];a.ids.set_switch=a.ids.action;a.ids.set_rtpc=a.ids.action;a.ids.variable_operation=a.ids.action;a.fields[1]=a.fields[0];a.Cached(p,0,a.child,9,7);a.Cached(p,1,0xffffaaaa00001111ull,10,8);const auto before=a.bytes;Monstey_Check(a.Run(p)==0xffffaaaa00001111ull,"valid same-record/class alias differs");Monstey_Check(before==a.bytes,"alias was mutated");++Monstey_Cases;}++Monstey_Groups;}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("NEW report required");Monstey_Models();Monstey_TypeAndState();Monstey_CacheWidths();Monstey_Faults();Monstey_GeneratedBoundaries();Monstey_ZeroDeclaration();Monstey_GeneratedMissing();Monstey_DefaultAndInvalid();Monstey_Alias();std::ofstream(argv[1],std::ios::binary)<<Monstey_Json::object({{"schema","Monstey.AudioIndependentTests/1"},{"passed",true},{"groups",Monstey_Groups},{"cases",Monstey_Cases},{"checks",Monstey_Checks},{"fixture_store_range_checks_separate",Monstey_FixtureStores},{"synthetic_only",true},{"original_code_executed",false}}).dump(2)<<'\n';std::cout<<"PASS "<<Monstey_Groups<<" groups "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
