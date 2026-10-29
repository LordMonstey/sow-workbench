#include "Monstey_Core.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct Monstey_Row{std::string key;std::uint8_t type;std::uint32_t discarded=0xDEADC0DE,low=0x80000000,high=0x7FFFFFFF;};
struct Monstey_Attribute{std::string name;unsigned type=0,count=0,parent_record=0;std::vector<std::uint32_t>words;std::vector<std::string>strings;std::vector<std::vector<std::uint32_t>>vectors;};
struct Monstey_Record{std::string name;std::vector<Monstey_Attribute>attributes;};
struct Monstey_Category{std::string name;std::vector<Monstey_Record>records;};
struct Monstey_Fixture{std::string name,text,expect="loaded";std::vector<Monstey_Row>rows;std::vector<Monstey_Category>categories;std::uint32_t arena=64u*1024u*1024u;};
void Monstey_U32(std::vector<std::uint8_t>&out,std::uint32_t n){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::vector<std::uint8_t>Monstey_ConstraintBytes(const std::vector<Monstey_Row>&rows){std::vector<std::uint8_t>out;for(const auto&r:rows){Monstey_U32(out,static_cast<std::uint32_t>(r.key.size()));out.insert(out.end(),r.key.begin(),r.key.end());out.push_back(r.type);Monstey_U32(out,r.discarded);Monstey_U32(out,r.low);Monstey_U32(out,r.high);}return out;}
std::string Monstey_Q(const std::string&s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c>=32)o<<static_cast<char>(c);else o<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;}o<<'"';return o.str();}
void Monstey_Write(const std::filesystem::path&p,const std::vector<std::uint8_t>&b){std::ofstream o(p,std::ios::binary);if(!o)throw std::runtime_error("fixture output");o.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!o)throw std::runtime_error("fixture write");}
std::string Monstey_Graph(const std::vector<Monstey_Category>&cs){std::ostringstream o;o<<'[';bool fc=true;for(const auto&c:cs){if(!fc)o<<',';fc=false;o<<"{\"name\":"<<Monstey_Q(c.name)<<",\"records\":[";bool fr=true;for(const auto&r:c.records){if(!fr)o<<',';fr=false;o<<"{\"name\":"<<Monstey_Q(r.name)<<",\"attributes\":[";bool fa=true;for(const auto&a:r.attributes){if(!fa)o<<',';fa=false;o<<"{\"name\":"<<Monstey_Q(a.name)<<",\"type\":"<<a.type<<",\"count\":"<<a.count<<",\"usage\":0,\"parent_record_ordinal\":"<<a.parent_record<<",\"raw_words\":[";for(std::size_t i=0;i<a.words.size();++i){if(i)o<<',';o<<a.words[i];}o<<"],\"strings\":[";for(std::size_t i=0;i<a.strings.size();++i){if(i)o<<',';o<<Monstey_Q(a.strings[i]);}o<<"],\"vector_bits\":[";for(std::size_t i=0;i<a.vectors.size();++i){if(i)o<<',';o<<'[';for(std::size_t j=0;j<a.vectors[i].size();++j){if(j)o<<',';o<<a.vectors[i][j];}o<<']';}o<<"]}";}o<<"]}";}o<<"]}";}o<<']';return o.str();}
Monstey_Fixture Monstey_One(std::string name,std::uint8_t type,std::string value,Monstey_Attribute a){a.name="A";a.type=type;return {name,"C/R/A="+value+"\n","loaded",{{"C/R/A",type}},{{"C",{{"R",{a}}}}}};}
std::vector<Monstey_Fixture>Monstey_Fixtures(){std::vector<Monstey_Fixture>f;
 f.push_back({"empty","","loaded",{}, {}});
 f.push_back({"missing-constraint","C/R/A=1\n","loaded",{}, {}});
 f.push_back(Monstey_One("bool-negative",1,"-7",{"",0,1,0,{1},{},{}}));
 f.push_back(Monstey_One("bool-zero",1,"0",{"",0,1,0,{0},{},{}}));
 f.push_back(Monstey_One("float-exact",2,"1.5",{"",0,1,0,{0x3FC00000},{},{}}));f.back().rows[0].low=0xFF800000;f.back().rows[0].high=0x7F800000;
 f.push_back(Monstey_One("int-negative",3,"-7",{"",0,1,0,{0xFFFFFFF9},{},{}}));
 f.push_back(Monstey_One("int-base-disagreement-ignored-validator",3,"0x10",{"",0,1,0,{0},{},{}}));
 f.push_back(Monstey_One("string-pool-after-graph",4,"hello",{"",0,1,0,{}, {"hello"},{}}));
 f.push_back(Monstey_One("skip-type5",5,"hello;again",{"",0,0,0,{},{},{}}));
 f.push_back(Monstey_One("skip-type255",255,"hello",{"",0,0,0,{},{},{}}));
 f.push_back(Monstey_One("vector2",6,"1.25,-2.5",{"",0,1,0,{},{},{{0x3FA00000,0xC0200000}}}));
 f.push_back(Monstey_One("vector3",7,"1,2,3",{"",0,1,0,{},{},{{0x3F800000,0x40000000,0x40400000}}}));
 f.push_back(Monstey_One("vector4",8,"1,2,3,4",{"",0,1,0,{},{},{{0x3F800000,0x40000000,0x40400000,0x40800000}}}));
 f.push_back(Monstey_One("vector-partial",7,"1.25,no",{"",0,1,0,{},{},{{0x3FA00000,0xCDCDCDCD,0xCDCDCDCD}}}));
 f.push_back(Monstey_One("vector-no-component",6,"no,2",{"",0,1,0,{},{},{{0xCDCDCDCD,0xCDCDCDCD}}}));
 f.push_back({"multi-value-overwrite","C/R/A=10;20\nC/R/B=30\n","loaded",{{"C/R/A",3},{"C/R/B",3}},{{"C",{{"R",{{"A",3,2,0,{10,30},{},{}},{"B",3,1,0,{30},{},{}}}}}}}});
 f.push_back({"string-multi-value-overwrite","C/R/A=one;two\nC/R/B=three\n","loaded",{{"C/R/A",4},{"C/R/B",4}},{{"C",{{"R",{{"A",4,2,0,{}, {"one","three"},{}},{"B",4,1,0,{}, {"three"},{}}}}}}}});
 f.push_back({"two-record-first-parent","C/R1/A=11\nC/R2/B=22\n","loaded",{{"C/R1/A",3},{"C/R2/B",3}},{{"C",{{"R1",{{"A",3,1,0,{11},{},{}}}},{"R2",{{"B",3,1,0,{22},{},{}}}}}}}});
 f.push_back({"two-categories","B/R/A=22\nA/R/A=11\n","loaded",{{"B/R/A",3},{"A/R/A",3}},{{"A",{{"R",{{"A",3,1,0,{11},{},{}}}}}},{"B",{{"R",{{"A",3,1,0,{22},{},{}}}}}}}});
 f.push_back(Monstey_One("duplicate-source",3,"1\nC/R/A=2",{"",0,1,0,{2},{},{}}));
 f.push_back(Monstey_One("duplicate-constraint",3,"hello",{"",0,1,0,{}, {"hello"},{}}));f.back().rows.push_back({"C/R/A",4});f.back().categories[0].records[0].attributes[0].type=4;
 f.push_back({"no-equals-matching-key","C/R/A\n","loaded",{{"C/R/A",4}},{{"C",{{"R",{{"A",4,1,0,{}, {"C/R/A"},{}}}}}}}});
 f.push_back({"no-slash-key","Bare=42\n","loaded",{{"Bare",3}},{{"Bare",{{"Bare",{{"Bare",3,1,0,{42},{},{}}}}}}}});
 f.push_back(Monstey_One("comment-and-crlf",3,"7\r\n;C/R/A=99",{"",0,1,0,{7},{},{}}));
 f.push_back(Monstey_One("nul-lines",3,std::string("1\0C/R/A=9",10),{"",0,1,0,{9},{},{}}));
 f.push_back(Monstey_One("standalone-cr-data",3,"7\rC/R/X=8",{"",0,1,0,{7},{},{}}));
 f.push_back(Monstey_One("line1023",4,std::string(1017,'x'),{"",0,1,0,{}, {std::string(1017,'x')},{}}));
 f.push_back(Monstey_One("line1024",4,std::string(1018,'x'),{}));f.back().expect="line_overflow";f.back().categories.clear();
 Monstey_Fixture budget{"budget-exhaustion", "", "arena_budget_exhaustion", {}, {},256u*1024u};for(unsigned i=0;i<4000;++i){const auto k="C/R/K"+std::to_string(10000+i);budget.rows.push_back({k,3});budget.text+=k+"=1\n";}f.push_back(std::move(budget));
 return f;
}
}
int wmain(int argc,wchar_t**argv){try{if(argc!=2)throw std::runtime_error("output directory required");const auto root=std::filesystem::path(argv[1]);std::filesystem::create_directories(root);std::ofstream report(root/"expected-fixtures-001.json");report<<"{\"schema\":\"Monstey.IndependentUnpackedFixtureOracle/1\",\"fixtures\":[";bool first=true;unsigned count=0;for(const auto&f:Monstey_Fixtures()){const auto dir=root/f.name;std::filesystem::create_directory(dir);const auto text=std::vector<std::uint8_t>(f.text.begin(),f.text.end());auto bytes=Monstey_ConstraintBytes(f.rows);Monstey_Write(dir/"source.txt",text);Monstey_Write(dir/"constraints.dat",bytes);if(!first)report<<',';first=false;report<<"{\"name\":"<<Monstey_Q(f.name)<<",\"text\":"<<Monstey_Q(Monstey::Monstey_Utf8((dir/"source.txt").wstring()))<<",\"constraints\":"<<Monstey_Q(Monstey::Monstey_Utf8((dir/"constraints.dat").wstring()))<<",\"expected_condition\":"<<Monstey_Q(f.expect)<<",\"arena_bytes\":"<<f.arena<<",\"graph\":"<<Monstey_Graph(f.categories)<<"}";++count;}report<<"]}\n";std::cout<<count<<" independent fixtures generated\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
