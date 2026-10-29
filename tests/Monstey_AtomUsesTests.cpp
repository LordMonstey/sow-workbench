#include "Monstey_AtomFixtures.hpp"
#include <bit>
#include <iostream>
#include <limits>
#include <map>
using namespace Monstey;
namespace {
using J=Monstey_Json;using B=Monstey_Bytes;
std::size_t Monstey_Checks=0,Monstey_Cases=0;
void Monstey_Check(bool ok,std::string_view text){++Monstey_Checks;if(!ok)throw std::runtime_error(std::string(text));}
const J &Monstey_Row(const J&q,std::uint32_t hash,std::size_t element=0){
 for(const auto&r:q.at("rows").as_array())if(r.at("field_hash").as_string()==Monstey_Hex(hash)&&
    (r.at("element").is_null()||r.at("element").as_uint()==element))return r;
 throw std::runtime_error("Missing expected fixture row");
}
void Monstey_Unchanged(const Monstey_Gadb&db,const std::function<void()>&call){
 const auto before=J::array({db.Describe(),db.Changes(),db.Schema(),db.ProjectMetadata()});call();
 Monstey_Check(before==J::array({db.Describe(),db.Changes(),db.Schema(),db.ProjectMetadata()}),"query changed source/project/schema/history");
}
std::filesystem::path Monstey_Save(const std::filesystem::path&dir,std::string_view name,const B&bytes){
 const auto p=dir/(std::string(name)+".gamedb");Monstey_WriteNewFile(p,bytes);return p;
}
void Monstey_Synthetic(const std::filesystem::path&dir){
 std::filesystem::create_directories(dir);auto pool=Monstey_AtomFixturePool();
 const auto ff=Monstey_AtomFixtureAppend(pool,{0xff,0});
 const auto escaped=Monstey_AtomFixtureAppend(pool,{'\\','x','f','f',0});
 const auto alias=Monstey_AtomFixtureAppend(pool,{'a','b','c',0});
 const auto wide=Monstey_AtomFixtureAppend(pool,{0x3d,0xd8,0x00,0xde,0x00,0xd8,0x41,0x00,0x00,0xdc,0,0},2);
 while(pool.size()%4)pool.push_back(0);const auto vec=static_cast<std::uint32_t>(pool.size());
 for(auto n:{0x7fc12345u,0x80000000u,0x7fa00001u,0xff800000u})Monstey_AppendU32(pool,n);
 const auto curve=static_cast<std::uint32_t>(pool.size());Monstey_AppendU32(pool,2);
 for(auto n:{0x80000000u,0x7fc11111u,0x3f800000u,0x80000000u})Monstey_AppendU32(pool,n);
 std::vector<std::uint32_t> words{ff,escaped,alias,alias,alias+1,wide,vec,curve,
  0x7fc54321u,0x80000000u,0xffffffffu,0u,0x12345678u,0xffffffffu,ff,vec,vec,vec};
 std::vector<Monstey_AtomFixtureField> fields{
  {30,19,4,1,0},{10,7,4,1,1},{40,1,4,1,2},{20,9,5,1,3},{50,5,4,1,4},
  {60,21,6,1,5},{70,3,9,1,6},{80,2,13,1,7},{90,4,2,2,8},
  {100,11,10,2,10},{110,12,11,2,12},{120,13,68,1,14},
  {130,14,14,1,15},{140,15,17,1,16},{150,16,99,1,17},{160,17,1,1,35},
  {170,18,4,0,18}};
 const auto bytes=Monstey_AtomFixture(pool,fields,words,true);
 Monstey_Gadb db(Monstey_Save(dir,"mixed",bytes));J q;
 Monstey_Unchanged(db,[&]{q=db.AtomUses("0x00000000",0,10000);});++Monstey_Cases;
 Monstey_Check(q.at("total").as_uint()==20,"raw element total including empty/bool");
 const auto &a=Monstey_Row(q,30).at("atom"),&b=Monstey_Row(q,10).at("atom");
 Monstey_Check(a.at("length_bytes").as_uint()==2&&b.at("length_bytes").as_uint()==5,"FF vs literal lengths");
 Monstey_Check(a.at("span_sha256")!=b.at("span_sha256"),"FF vs escaped hash");
 Monstey_Check(a.at("span_sha256").as_string()==Monstey_HashSha256(std::span<const std::uint8_t>(pool).subspan(ff,2)),"literal independent span hash");
 Monstey_Check(Monstey_DisplayUtf8(std::string(1,static_cast<char>(0xff)))=="\\xff","display collision fixture premise");
 Monstey_Check(a.at("source_file_offset").as_uint()==8+46+ff,"wrapper atom file offset");
 Monstey_Check(Monstey_Row(q,30).at("source_word_file_offset").as_uint()==8+46+pool.size()+fields.size()*15+8+24+8,"wrapper original word offset");
 Monstey_Check(Monstey_Row(q,30).at("physical_index").as_uint()==0&&Monstey_Row(q,30).at("native_ordinal").as_uint()==19,"ordinal distinct from physical");
 Monstey_Check(q.at("span_relations").size()>=3,"sharing/suffix relations observed");
 bool shared=false,suffix=false;for(const auto&r:q.at("span_relations").as_array()){
  shared|=r.at("relation").as_string()=="shared_span";suffix|=r.at("relation").as_string()=="suffix_overlap";}
 Monstey_Check(shared&&suffix,"shared and suffix both retained");
 const auto &u=Monstey_Row(q,60).at("atom").at("utf16_units");
 Monstey_Check(u==J::array({0xd83d,0xde00,0xd800,0x41,0xdc00,0}),"UTF16 raw pair and lone units");
 Monstey_Check(Monstey_Row(q,70).at("atom").at("raw_f32_bits")==J::array({0x7fc12345u,0x80000000u,0x7fa00001u,0xff800000u}),"vector NaN/signed zero raw bits");
 Monstey_Check(Monstey_Row(q,80).at("atom").at("raw_f32_bits")==J::array({0x80000000u,0x7fc11111u,0x3f800000u,0x80000000u}),"curve raw bits");
 Monstey_Check(Monstey_Row(q,90).at("raw_u32").as_uint()==0x7fc54321u&&Monstey_Row(q,90,1).at("raw_u32").as_uint()==0x80000000u,"immediate bits");
 Monstey_Check(Monstey_Row(q,100).at("status").as_string()=="reference_null"&&Monstey_Row(q,110).at("kind").as_string()=="record_reference_list_element","10/11 reference words");
 Monstey_Check(Monstey_Row(q,120).at("atom").at("ownership").as_string()=="raw_0x40_flag_unqualified","68 ownership unknown");
 for(auto h:{130u,140u,150u})Monstey_Check(Monstey_Row(q,h).at("status").as_string()=="unknown_payload_layout"&&Monstey_Row(q,h).at("atom").is_null(),"unknown payload no span");
 Monstey_Check(Monstey_Row(q,160).at("bit_offset").as_uint()==35&&Monstey_Row(q,160).at("record_word_index").as_uint()==1,"bool bit offset");
 Monstey_Check(Monstey_Row(q,170).at("status").as_string()=="no_values"&&Monstey_Row(q,170).at("raw_u32").is_null(),"count0 lazy no word");
 for(std::size_t i=0;i<q.at("rows").size();++i){const auto page=db.AtomUses("0x00000000",i,1);Monstey_Check(page.at("rows").at(0)==q.at("rows").at(i),"page stability");++Monstey_Cases;}
 Monstey_Check(db.AtomUses("0x00000000",999,1).at("rows").empty(),"offset beyond total");
 for(auto bad:{std::size_t{0},std::size_t{10001}}){bool threw=false;try{(void)db.AtomUses("0x0",0,bad);}catch(const Monstey_Error&){threw=true;}Monstey_Check(threw,"invalid limit rejected");++Monstey_Cases;}
 {bool threw=false;try{(void)db.AtomUses("0x0",std::numeric_limits<std::size_t>::max(),1);}catch(const Monstey_Error&){threw=true;}Monstey_Check(threw,"INT64 bound");++Monstey_Cases;}
 auto checkCase=[&](std::string_view name,B p,std::uint8_t type,std::uint32_t pointer,std::string_view status){
  Monstey_Gadb item(Monstey_Save(dir,name,Monstey_AtomFixture(p,{{1,0,type,1,0}},{pointer})));
  Monstey_Unchanged(item,[&]{const auto r=item.AtomUses("0x0").at("rows").at(0);Monstey_Check(r.at("status").as_string()==status,name);});
  ++Monstey_Cases;};
 checkCase("outside",Monstey_AtomFixturePool(),4,0xffffffffu,"atom_offset_outside_pool");
 checkCase("zero_is_pool",Monstey_AtomFixturePool(),4,0,"atom_span_valid");
 checkCase("utf_odd",Monstey_AtomFixturePool(),6,1,"unaligned_utf16_offset");
 checkCase("vec_odd",Monstey_AtomFixturePool(),7,1,"unaligned_vector_offset");
 checkCase("curve_odd",Monstey_AtomFixturePool(),13,1,"unaligned_curve_offset");
 {auto p=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(p,{'A','B','C'});checkCase("narrow_missing",p,4,at,"unterminated_narrow_string");}
 {auto p=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(p,{0x41,1},2);checkCase("utf_missing",p,6,at,"unterminated_utf16_string");}
 {auto p=Monstey_AtomFixturePool();while(p.size()%4)p.push_back(0);const auto at=static_cast<std::uint32_t>(p.size());
  checkCase("vector_short",p,9,at,"vector_exceeds_pool");checkCase("curve_header_short",p,13,at,"curve_header_exceeds_pool");}
 {auto p=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(p,{0xff,0xff,0xff,0xff},4);checkCase("curve_huge",p,16,at,"curve_count_exceeds_pool");}
 {auto p=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(p,{0,0,0,0},4);checkCase("curve_zero",p,13,at,"atom_span_valid");}
 {auto p=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(p,{1,0,1,0},4);p.resize(p.size()+65537u*8u,0);
  checkCase("curve_policy",p,13,at,"atom_span_valid_adapter_curve_limit_exceeded");}
 {auto p=Monstey_AtomFixturePool();while(p.size()%2)p.push_back(0);const auto at=static_cast<std::uint32_t>(p.size());
  for(int i=0;i<70;++i){p.push_back(0x41);p.push_back(0);}p.push_back(0);p.push_back(0);
  Monstey_Gadb item(Monstey_Save(dir,"wide_preview",Monstey_AtomFixture(p,{{1,0,6,1,0}},{at})));
  const auto a1=item.AtomUses("0x0").at("rows").at(0).at("atom");Monstey_Check(a1.at("utf16_units").size()==64&&a1.at("utf16_units_truncated").as_bool(),"UTF preview bounded");++Monstey_Cases;}
 {Monstey_Gadb empty(Monstey_Save(dir,"lazy_zero",Monstey_AtomFixture(Monstey_AtomFixturePool(),{{1,0,4,0,0}},{})));
  Monstey_Check(empty.AtomUses("0x0").at("rows").at(0).at("raw_u32").is_null(),"zero record words lazy");++Monstey_Cases;}
 // Mutations are fixture setup only. Query never uses Build/SaveProject or a serializer.
 auto p=Monstey_AtomFixturePool();const auto ptr=Monstey_AtomFixtureAppend(p,{'o','l','d',0});
 const auto hash=Monstey_NativeHash("Value"),ih=Monstey_NativeHash("I");
 const auto schema=J::object({{"categories",J::object({{"Fixture",J::object({{"fields",J::array({
  J::object({{"name","Value"},{"type_code",4},{"array",1}}),J::object({{"name","I"},{"type_code",3},{"array",1}})})}})}})}});
 auto editable=Monstey_Gadb::WithSchema(Monstey_Save(dir,"editable",Monstey_AtomFixture(p,{{hash,3,4,1,0},{ih,8,3,1,1}},{ptr,7})),schema);
 const auto clone=editable->CloneRecord("0x0","AtomClone").at("clone").at("selector").as_string();
 auto rawClone=editable->AtomUses(clone);Monstey_Check(Monstey_Row(rawClone,hash).at("source_word_file_offset").is_integer(),"same-source clone retains source snapshot word coordinate");++Monstey_Cases;
 editable->SetField(clone,"Value","changed");
 Monstey_Unchanged(*editable,[&]{const auto r=Monstey_Row(editable->AtomUses(clone),hash);Monstey_Check(r.at("status").as_string()=="not_yet_serialized"&&r.at("raw_u32").is_null()&&r.at("source_word_file_offset").is_null(),"dirty clone no fabricated atom");});++Monstey_Cases;
 editable->MoveField("0x0","Value",1);
 {const auto r=Monstey_Row(editable->AtomUses("0x0"),hash);Monstey_Check(r.at("provenance").as_string()=="owned_layout_snapshot"&&r.at("source_word_file_offset").is_null()&&r.at("atom").at("span_valid").as_bool(),"owned layout raw pool remains usable");++Monstey_Cases;}
 editable->AddField("0x0","Introduced",4,J::array({"a","b","c"}),J::object({{"array",3}}));
 Monstey_Unchanged(*editable,[&]{const auto r=Monstey_Row(editable->AtomUses("0x0"),Monstey_NativeHash("Introduced"));Monstey_Check(r.at("introduced").as_bool()&&r.at("future_serialized_count").is_null()&&r.at("element").is_null()&&r.at("atom").is_null(),"introduced exactly one honest pending row");});++Monstey_Cases;
 auto fresh=Monstey_Gadb::New();fresh->CreateCategory("Fixture");fresh->CreateRecord(0,"Owned");
 fresh->AddField("0x0","OwnedValue",4,"value");
 Monstey_Unchanged(*fresh,[&]{const auto q1=fresh->AtomUses("0x0");Monstey_Check(q1.at("source_kind").as_string()=="logical_memory_source"&&q1.at("rows").size()==1&&q1.at("rows").at(0).at("source_word_file_offset").is_null(),"new project pending source provenance");});++Monstey_Cases;

 editable->SetField("0x0","I",123);
 Monstey_Unchanged(*editable,[&]{const auto r=Monstey_Row(editable->AtomUses("0x0"),ih);Monstey_Check(r.at("provenance").as_string()=="not_yet_serialized_typed_edit"&&r.at("raw_u32").is_null()&&r.at("future_serialized_count").is_null(),"dirty original typed edit remains pending");});++Monstey_Cases;
 {auto pp=Monstey_AtomFixturePool();const auto at=Monstey_AtomFixtureAppend(pp,{'x',0});
  Monstey_Gadb aliases(Monstey_Save(dir,"many_aliases",Monstey_AtomFixture(pp,{{1,0,4,20,0}},std::vector<std::uint32_t>(20,at))));
  const auto qa=aliases.AtomUses("0x0");Monstey_Check(qa.at("span_relations").size()==128&&qa.at("span_relations_truncated").as_bool(),"relation output cap explicit");
  Monstey_Check(!aliases.AtomUses("0x0",0,2).at("span_relations_truncated").as_bool(),"page relation scope bounded");++Monstey_Cases;}
 std::cout<<J::object({{"status","PASS"},{"cases",Monstey_Cases},{"checks",Monstey_Checks},{"Build_or_export_called",false},{"synthetic_only",true}}).dump()<<'\n';
}
void Monstey_Actual(const std::filesystem::path&path){
 constexpr std::uintmax_t size=78230096;
 const std::string expected="afe5fda2e80e44c227f53c07123569e0d4ed619718e826ad5a307445fe33b3ed";
 Monstey_Check(std::filesystem::file_size(path)==size,"actual size pin");
 const auto before=Monstey_HashSha256(path);Monstey_Check(before==expected,"actual SHA pin");
 const Monstey_Gadb db(path);const auto initial=db.Changes();const auto initialSchema=db.Schema();
 std::size_t records=0,uses=0;std::map<std::string,std::size_t> statuses;
 const auto categories=db.Categories();
 for(const auto&category:categories.at("rows").as_array()){
  if(!category.at("records").as_uint())continue;
  const auto ci=static_cast<std::uint32_t>(category.at("category_index").as_uint());
  const auto q=db.AtomUses(Monstey_Hex(static_cast<std::uint64_t>(ci)<<20),0,32);
  for(const auto&r:q.at("rows").as_array())++statuses[r.at("status").as_string()];
  uses+=q.at("rows").size();if(++records==3)break;
 }
 Monstey_Check(records>0,"actual sampled records");Monstey_Check(initial==db.Changes()&&initialSchema==db.Schema(),"actual query dirty/schema invariant");
 const auto after=Monstey_HashSha256(path);Monstey_Check(before==after&&std::filesystem::file_size(path)==size,"actual after pin");
 J st=J::object();for(const auto&[key,value]:statuses)st[key]=value;
 std::cout<<J::object({{"status","PASS"},{"source_sha256",before},{"source_bytes",size},{"records_sampled",records},{"uses_returned",uses},{"status_counts",st},{"one_Gadb_load",true},{"full_database_audit",false},{"source_hash_before_after_equal",true},{"project_schema_unchanged",true},{"Build_or_export_called",false},{"actual_names_or_value_rows_persisted",false}}).dump()<<'\n';
}
}
int wmain(int argc,wchar_t**argv){
 try{if(argc==3&&std::wstring_view(argv[1])==L"--actual")Monstey_Actual(argv[2]);
 else{Monstey_Require(argc==2,"Tests require an owned synthetic output directory");Monstey_Synthetic(argv[1]);}return 0;}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
