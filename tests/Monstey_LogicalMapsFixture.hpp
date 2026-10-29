#pragma once
#include "Monstey_FearUnpackedCallback.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Monstey_Memory=Monstey::Monstey_FearDbMutableMemory;
using Monstey_Maps=Monstey::Monstey_FearUnpackedCallbackLogicalMaps;
std::uint64_t Monstey_checks=0,Monstey_cases=0;
void Monstey_Check(bool v,const char* m){++Monstey_checks;if(!v)throw std::runtime_error(m);}
struct Monstey_Less {bool operator()(const std::string&a,const std::string&b)const{
 return std::lexicographical_compare(a.begin(),a.end(),b.begin(),b.end(),[](char x,char y){return static_cast<unsigned char>(x)<static_cast<unsigned char>(y);});}};
struct Monstey_Value {std::uint32_t type=0,w4=0,w8=0;std::string text;};
using Monstey_Oracle=std::map<std::string,Monstey_Value,Monstey_Less>;
struct Monstey_Fixture {
 static constexpr std::uint32_t base=0x21000000,map=base+0x100,hidden=base+0x200,pair=base+0x1000;
 std::vector<std::uint8_t> image=std::vector<std::uint8_t>(4u*1024u*1024u,0xA5);
 Monstey_Memory mem{base,image};Monstey_Maps maps;std::vector<std::pair<std::uint32_t,std::uint32_t>> allocations;
 Monstey_Fixture(){maps.allocate=[this,cursor=base+0x4000u](Monstey_Memory,std::uint32_t n)mutable{
  const auto p=cursor;cursor=(cursor+n+15u)&~15u;Monstey_Check(cursor<base+0x280000,"arena exhausted");allocations.emplace_back(p,n);return p;};maps.strings.allocate=maps.allocate;}
 std::uint32_t R(std::uint32_t a,unsigned n=4)const{Monstey_Check(a>=base&&static_cast<std::uint64_t>(a-base)+n<=image.size(),"reference read range");std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=static_cast<std::uint32_t>(image[a-base+i])<<(8*i);return v;}
 void W(std::uint32_t a,std::uint32_t v,unsigned n=4){Monstey_Check(a>=base&&static_cast<std::uint64_t>(a-base)+n<=image.size(),"reference write range");for(unsigned i=0;i<n;++i)image[a-base+i]=static_cast<std::uint8_t>(v>>(8*i));}
 void S(std::uint32_t p,const std::string&s,std::uint32_t backing){for(unsigned i=0;i<28;++i)W(p+i,0,1);const bool large=s.size()>15;W(p+20,static_cast<std::uint32_t>(s.size()));W(p+24,large?static_cast<std::uint32_t>(s.size()):15u);const auto b=large?backing:p+4;if(large)W(p+4,b);for(std::size_t i=0;i<s.size();++i)W(b+static_cast<std::uint32_t>(i),static_cast<unsigned char>(s[i]),1);W(b+static_cast<std::uint32_t>(s.size()),0,1);}
 std::string S(std::uint32_t p)const{const auto n=R(p+20),cap=R(p+24),b=cap<=15?p+4:R(p+4);Monstey_Check(n<=cap,"string size/capacity");std::string s;for(std::uint32_t i=0;i<n;++i)s.push_back(static_cast<char>(R(b+i,1)));Monstey_Check(R(b+n,1)==0,"string terminator");return s;}
 std::uint32_t Init(bool parsed){W(map,0xFEED1234);maps.Initialize(mem,map,parsed);Monstey_Check(R(map)==0xFEED1234,"map unrelated prefix");const auto head=R(map+4);Monstey_Check(R(map+8)==0,"empty count");for(unsigned i=0;i<3;++i)Monstey_Check(R(head+4*i)==head,"empty sentinel links");Monstey_Check(R(head+(parsed?0x4D:0x35),1)==1,"sentinel nil");Monstey_Check(allocations.size()==1&&allocations[0].second==(parsed?80u:56u),"head allocation width");return head;}
 void Input(const std::string&key,const Monstey_Value&v){S(pair,key,base+0x300000);W(pair+28,v.type);S(pair+32,v.text,base+0x310000);W(pair+60,v.w8);}
 void Verify(bool parsed,const Monstey_Oracle&o)const{
  const auto head=R(map+4),root=R(head+4);std::set<std::uint32_t>seen;std::vector<std::string>ordered;
  std::function<void(std::uint32_t,std::uint32_t)>visit=[&](std::uint32_t n,std::uint32_t parent){if(n==head)return;Monstey_Check(seen.insert(n).second,"tree repeated/cyclic node");Monstey_Check(R(n+(parsed?0x4D:0x35),1)==0,"node nil");Monstey_Check(R(n+4)==parent,"node parent");visit(R(n),n);const auto key=S(n+12);ordered.push_back(key);const auto it=o.find(key);Monstey_Check(it!=o.end(),"unexpected key");Monstey_Check(R(n+40)==it->second.type,"full type word");if(parsed){Monstey_Check(S(n+44)==it->second.text,"parsed value string");Monstey_Check(R(n+72)==it->second.w8,"parsed opaque word");}else{Monstey_Check(R(n+44)==it->second.w4,"constraint minimum word");Monstey_Check(R(n+48)==it->second.w8,"constraint maximum word");}visit(R(n+8),n);};
  Monstey_Check(R(head+(parsed?0x4D:0x35),1)==1,"head nil preserved");visit(root,head);Monstey_Check(seen.size()==o.size()&&R(map+8)==o.size(),"map size/count");std::vector<std::string>expected;for(const auto&[k,v]:o){(void)v;expected.push_back(k);}Monstey_Check(ordered==expected,"unsigned counted key order");if(o.empty()){Monstey_Check(R(head)==head&&R(head+8)==head,"empty extremes");}else{Monstey_Check(S(R(head)+12)==o.begin()->first,"first node");Monstey_Check(S(R(head+8)+12)==o.rbegin()->first,"last node");}
 }
};
std::uint32_t Monstey_Random(std::uint32_t&s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
std::vector<std::string>Monstey_Keys(){return {"","a","A","z","aa",std::string(15,'q'),std::string(16,'q'),std::string(63,'x'),std::string("a\0b",3),std::string("a\0a",3),std::string(1,static_cast<char>(0x80)),std::string(1,static_cast<char>(0xFF)),"prefix","prefixmore","b","n","m","c"};}
}
