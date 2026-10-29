#pragma once
#include "Monstey_Gadb.hpp"
#include <algorithm>
#include <numeric>
namespace Monstey {
struct Monstey_AtomFixtureField { std::uint32_t hash; std::uint8_t ordinal, type; std::uint16_t count, offset; };
inline Monstey_Bytes Monstey_AtomFixturePool()
{
    return {'F','i','x','t','u','r','e',0,0};
}
inline std::uint32_t Monstey_AtomFixtureAppend(Monstey_Bytes &pool, std::initializer_list<std::uint8_t> bytes, unsigned alignment=1)
{
    while (pool.size()%alignment) pool.push_back(0);
    const auto at=static_cast<std::uint32_t>(pool.size()); pool.insert(pool.end(),bytes);return at;
}
inline Monstey_Bytes Monstey_AtomFixture(const Monstey_Bytes &pool,
 const std::vector<Monstey_AtomFixtureField>&fields,const std::vector<std::uint32_t>&words,bool wrapped=false)
{
    Monstey_Bytes b(46,0);b[0]='G';b[1]='A';b[2]='D';b[3]='B';
    Monstey_PutU32(b,4,17);Monstey_PutU32(b,8,static_cast<std::uint32_t>(pool.size()));
    Monstey_PutU32(b,12,1);Monstey_PutU32(b,20,1);
    Monstey_PutU32(b,24,static_cast<std::uint32_t>(words.size()));
    Monstey_PutU32(b,28,static_cast<std::uint32_t>(fields.size()));Monstey_PutU32(b,40,44);
    b.insert(b.end(),pool.begin(),pool.end());
    std::vector<std::size_t> order(fields.size());std::iota(order.begin(),order.end(),std::size_t{0});
    std::sort(order.begin(),order.end(),[&](auto a,auto c){return fields[a].hash<fields[c].hash;});
    for(std::size_t i=0;i<fields.size();++i){
        const auto &f=fields[i];Monstey_AppendU32(b,f.hash);Monstey_AppendU32(b,fields[order[i]].hash);
        b.push_back(f.ordinal);b.push_back(static_cast<std::uint8_t>(order[i]));b.push_back(f.type);
        Monstey_AppendU16(b,f.count);Monstey_AppendU16(b,f.offset);
    }
    Monstey_AppendU32(b,0);Monstey_AppendU32(b,1);
    Monstey_AppendU32(b,0);Monstey_AppendU32(b,0);Monstey_AppendU32(b,8);
    Monstey_AppendU32(b,1);Monstey_AppendU32(b,0);Monstey_AppendU32(b,Monstey_NativeHash("Fixture"));
    Monstey_AppendU32(b,static_cast<std::uint32_t>(words.size())|(static_cast<std::uint32_t>(fields.size())<<16));
    Monstey_AppendU32(b,0);for(auto word:words)Monstey_AppendU32(b,word);Monstey_AppendU32(b,0);
    if(wrapped){Monstey_Bytes out{'L','T','F','X',8,0,0,0};out.insert(out.end(),b.begin(),b.end());return out;}
    return b;
}
}
