#include "Monstey_FxReflection.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using S = std::span<const std::uint8_t>;
std::string Monstey_RFHex(S bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string value;
    value.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        value.push_back(digits[byte >> 4]);
        value.push_back(digits[byte & 15]);
    }
    return value;
}
struct Monstey_RFReader {
    S bytes;
    S At(std::uint64_t address, std::size_t count) const {
        return Monstey_FearPeRange(bytes, address, count);
    }
    std::string Text(std::uint64_t address) const {
        if (!address)
            return {};
        std::string result;
        for (std::size_t i = 0; i < 4096; ++i) {
            Monstey_Require(address <= UINT64_MAX - i, "Reflection text address overflow");
            const auto byte = At(address + i, 1)[0];
            if (!byte) {
                Monstey_Wide(result);
                return result;
            }
            result.push_back(static_cast<char>(byte));
        }
        throw Monstey_Error("fx_reflection_text", "Unterminated or oversized native reflection string");
    }
    J Float(S data, std::size_t offset) const {
        const auto bits = Monstey_U32(data, offset);
        const auto value = std::bit_cast<float>(bits);
        return std::isfinite(value) ? J(static_cast<double>(value))
                                    : J::object({{"nonfinite_native_bits", Monstey_Hex(bits)}});
    }
    J Default(std::uint32_t type, S raw) const {
        const auto pointer = Monstey_U64(raw, 0);
        if (type == 1) {
            if (!pointer)
                return nullptr;
            const auto value = Text(pointer);
            Monstey_Require(value == "true" || value == "false", "Unknown native Boolean default token");
            return value == "true";
        }
        if (type == 2)
            return Float(raw, 0);
        if (type == 3)
            return static_cast<std::int64_t>(std::bit_cast<std::int32_t>(Monstey_U32(raw, 0)));
        if (type == 4 || type == 5 || type == 6 || type == 68)
            return pointer ? J(Text(pointer)) : J(nullptr);
        if (type >= 7 && type <= 9) {
            auto value = J::array();
            for (std::uint32_t i = 0; i < type - 5; ++i)
                value.as_array().push_back(Float(raw, static_cast<std::size_t>(i) * 4));
            return value;
        }
        // The captured sixteen bytes do not qualify an allocated curve/reference default.
        return nullptr;
    }
    J Field(std::uint64_t address, std::uint64_t reflectionIndex) const {
        const auto raw = At(address, 72);
        const auto name = Text(Monstey_U64(raw, 0));
        const auto hash = Monstey_U32(raw, 32), type = Monstey_U32(raw, 36), count = Monstey_U32(raw, 40),
                   flags = Monstey_U32(raw, 44);
        Monstey_Require(!name.empty() && Monstey_NativeHash(name) == hash,
                        "Native reflection field name/hash mismatch");
        Monstey_Require(type > 0 && type <= 255 && (count == UINT32_MAX || count <= 65535),
                        "Native reflection field type/count exceeds bounded descriptor grammar");
        auto field = J::object({{"name", name},
                                {"hash", Monstey_Hex(hash)},
                                {"type_code", type},
                                {"ordinal", raw[64]},
                                {"array", count == UINT32_MAX ? J(-1) : J(count)},
                                {"flags", flags},
                                {"default", Default(type, raw.subspan(16, 16))},
                                {"native_descriptor_ea", Monstey_Hex(address)},
                                {"native_descriptor_sha256", Monstey_HashSha256(raw)},
                                {"native_default_hex", Monstey_RFHex(raw.subspan(16, 16))}});
        Monstey_Require(reflectionIndex <= 255, "Native reflection index exceeds UInt8 field identity");
        field["cached_ordinal"] = raw[64];
        field["captured_binding_state"] = Monstey_U32(raw, 48);
        field["native_ordinal"] = reflectionIndex;
        field["native_ordinal_source"] = "complete_reflection_table_index";
        const auto callback = Monstey_U64(raw, 56);
        if (callback) {
            const auto code = At(callback, 8);
            std::uint32_t identity = UINT32_MAX;
            if (code[0] == 0xb0 && code[2] == 0xc3)
                identity = code[1];
            else if ((code[0] == 0x32 || code[0] == 0x30 || code[0] == 0x33) && code[1] == 0xc0 &&
                     code[2] == 0xc3)
                identity = 0;
            else if (code[0] == 0xb8 && code[5] == 0xc3)
                identity = Monstey_U32(code, 1);
            Monstey_Require(identity <= 255 && identity == reflectionIndex,
                            "Native field index callback is unsupported or differs from reflection order");
            field["native_ordinal_source"] = "native_index_callback";
            field["native_index_callback_ea"] = Monstey_Hex(callback);
            field["native_index_callback_hex"] = Monstey_RFHex(code);
        }
        if (flags & 0x280u)
            field["group_marker"] = true;
        const auto constraint = Text(Monstey_U64(raw, 8));
        if (!constraint.empty()) {
            field["constraint"] = constraint;
            std::size_t start = 0;
            while (start < constraint.size()) {
                const auto end = constraint.find(';', start);
                auto item =
                    constraint.substr(start, end == std::string::npos ? std::string::npos : end - start);
                const auto first = item.find_first_not_of(" \t"), last = item.find_last_not_of(" \t");
                item = first == std::string::npos ? std::string{} : item.substr(first, last - first + 1);
                const auto equals = item.find('=');
                if (equals != std::string::npos) {
                    const auto key = item.substr(0, equals);
                    auto value = item.substr(equals + 1);
                    if ((key == "min" || key == "max") && value.ends_with('f'))
                        value.pop_back();
                    if (key == "min" || key == "max")
                        field[key] = value;
                    else if (key == "type")
                        field["enum"] = value;
                } else if (!item.empty()) {
                    const bool publicTarget = item.starts_with("public ");
                    field[type == 10 || type == 11 ? "target" : "tag"] = publicTarget ? item.substr(7) : item;
                    if (publicTarget)
                        field["public"] = true;
                }
                if (end == std::string::npos)
                    break;
                start = end + 1;
            }
        }
        return field;
    }
};
} // namespace
Monstey_Json Monstey_FxReflectCategories(S image, std::span<const std::uint64_t> entries,
                                         std::string_view expectedSha256) {
    Monstey_Require(!entries.empty() && entries.size() <= 4096,
                    "Reflection requires 1..4096 explicit entries");
    const auto sourceSha = Monstey_HashSha256(image);
    if (!expectedSha256.empty() && sourceSha != expectedSha256)
        throw Monstey_Error("source_hash_mismatch", "Native reflection image differs from expected snapshot");
    const Monstey_RFReader reader{image};
    auto categories = J::object(), proof = J::array();
    std::set<std::uint64_t> active;
    std::map<std::uint64_t, std::string> finished;
    std::function<std::string(std::uint64_t)> visit = [&](std::uint64_t address) {
        if (finished.contains(address))
            return finished.at(address);
        Monstey_Require(active.size() < 64 && finished.size() < 4096 && active.insert(address).second,
                        "Native reflection category parent cycle/depth/size limit");
        const auto raw = reader.At(address, 64);
        const auto fields = Monstey_U64(raw, 0), count = Monstey_U64(raw, 8), parent = Monstey_U64(raw, 40),
                   flags = Monstey_U64(raw, 56);
        const auto name = reader.Text(Monstey_U64(raw, 32));
        Monstey_Require(!name.empty() && count <= 4096 && Monstey_U64(raw, 48) == 0,
                        "Invalid native reflection category descriptor");
        Monstey_Require(!categories.contains(name), "Duplicate native reflection category names");
        Monstey_Require(!count || fields <= UINT64_MAX - (count - 1) * 72,
                        "Native reflection field table address overflow");
        auto category = J::object({{"name", name},
                                   {"description", reader.Text(Monstey_U64(raw, 24))},
                                   {"struct", (flags & 2) != 0},
                                   {"flags", Monstey_Hex(flags)},
                                   {"field_count", count},
                                   {"unreadable_fields", 0},
                                   {"fields", J::array()}});
        std::map<std::uint32_t, bool> hashes;
        for (std::uint64_t i = 0; i < count; ++i) {
            auto field = reader.Field(fields + i * 72, i);
            const auto hash = Monstey_NativeHash(field["name"].as_string());
            const bool marker = (field["flags"].as_uint() & 0x280u) != 0;
            const auto [previous, inserted] = hashes.emplace(hash, marker);
            Monstey_Require(inserted || (marker && previous->second),
                            "Duplicate consumed native field hash in category descriptor");
            category["fields"].as_array().push_back(std::move(field));
        }
        if (parent) {
            const auto parentName = visit(parent);
            category["parent"] = parentName;
            auto ancestors = J::array({parentName});
            if (categories[parentName].contains("ancestors"))
                for (const auto &ancestor : categories[parentName]["ancestors"].as_array())
                    ancestors.as_array().push_back(ancestor);
            category["ancestors"] = ancestors;
        }
        Monstey_Require(!categories.contains(name),
                        "Duplicate native category full name within parent hierarchy");
        categories[name] = std::move(category);
        proof.as_array().push_back(J::object({{"entry_ea", Monstey_Hex(address)},
                                              {"name", name},
                                              {"parent_ea", Monstey_Hex(parent)},
                                              {"fields_ea", Monstey_Hex(fields)},
                                              {"count", count},
                                              {"flags", Monstey_Hex(flags)},
                                              {"entry_sha256", Monstey_HashSha256(raw)}}));
        active.erase(address);
        finished[address] = name;
        return name;
    };
    for (const auto entry : entries)
        visit(entry);
    return J::object({{"format", "Monstey_NativeFxReflection"},
                      {"version", 1},
                      {"source_sha256", sourceSha},
                      {"original_executed", false},
                      {"original_modified", false},
                      {"entries", proof},
                      {"categories", categories}});
}
} // namespace Monstey

#include "Monstey_GadbRegistry.hpp"
namespace Monstey {
namespace {
struct Monstey_RegistryCandidate {
    std::uint64_t address{},parent{};
    J category,entry;
    std::string failure;
};
bool Monstey_RegistryName(std::string_view name) {
    if(name.empty()||name.size()>512)return false;
    for(const unsigned char c:name)
        if(c<32||c>=127)return false;
    return true;
}
std::string Monstey_RegistryFlat(std::string name) {
    std::erase(name,'.');
    for(auto& c:name)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return name;
}
J Monstey_GadbRegistryScan(S image,std::string_view expected) {
    Monstey_Require(expected.size()==64,"Native declaration discovery requires exact SHA256");
    const auto sha=Monstey_HashSha256(image);
    Monstey_Require(sha==expected,"Native declaration image hash differs");
    Monstey_Require(image.size()>=64&&Monstey_U16(image,0)==0x5a4d,"Invalid PE DOS header");
    const auto nt=static_cast<std::size_t>(Monstey_U32(image,60));
    Monstey_Require(nt<=image.size()&&image.size()-nt>=24,"PE NT header exceeds file");
    Monstey_Require(Monstey_U32(image,nt)==0x4550&&Monstey_U16(image,nt+4)==0x8664,
        "Expected x64 PE");
    const auto sectionCount=Monstey_U16(image,nt+6),optionalSize=Monstey_U16(image,nt+20);
    const auto optional=nt+24,table=optional+optionalSize;
    Monstey_Require(optionalSize>=136&&table<=image.size()&&Monstey_U16(image,optional)==0x20b,
        "Invalid PE32+ optional header");
    Monstey_Require(sectionCount>0&&sectionCount<=96&&sectionCount<=(image.size()-table)/40,
        "PE section table exceeds file");
    const auto imageBase=Monstey_U64(image,optional+24);
    // RVA mapping and executable provenance must describe one unique file-backed view.
    std::vector<std::pair<std::uint64_t,std::uint64_t>> virtualRanges;
    for(std::size_t si=0;si<sectionCount;++si) {
        const auto at=table+si*40;
        const auto rva=static_cast<std::uint64_t>(Monstey_U32(image,at+12));
        const auto rawSize=Monstey_U32(image,at+16),rawOffset=Monstey_U32(image,at+20);
        Monstey_Require(rawOffset<=image.size()&&rawSize<=image.size()-rawOffset,"PE raw section exceeds file");
        const auto extent=std::max<std::uint64_t>(Monstey_U32(image,at+8),rawSize);
        Monstey_Require(extent<=0x100000000ull-rva,"PE section RVA extent exceeds UInt32 address space");
        if(!extent)continue;
        const auto end=rva+extent;
        for(const auto& range:virtualRanges)
            Monstey_Require(end<=range.first||range.second<=rva,"Overlapping PE RVA sections make native provenance ambiguous");
        Monstey_Require(imageBase<=UINT64_MAX-end,"PE image virtual-address extent overflow");
        virtualRanges.emplace_back(rva,end);
    }
    const Monstey_RFReader reader{image};
    std::vector<std::pair<std::uint64_t,std::uint64_t>> executable;
    for(std::size_t si=0;si<sectionCount;++si) {
        const auto at=table+si*40;
        if(!(Monstey_U32(image,at+36)&0x20000000u))continue;
        const auto rva=Monstey_U32(image,at+12),length=Monstey_U32(image,at+16);
        Monstey_Require(imageBase<=UINT64_MAX-rva&&imageBase+rva<=UINT64_MAX-length,"Executable section overflow");
        executable.emplace_back(imageBase+rva,imageBase+rva+length);
    }
    const auto callbackExecutable=[&](std::uint64_t address) {
        return std::any_of(executable.begin(),executable.end(),[&](const auto& range) {
            return address>=range.first&&address<=range.second&&range.second-address>=8;
        });
    };
    auto diagnostics=J::array();
    std::map<std::string,std::vector<Monstey_RegistryCandidate>> candidates;
    std::uint64_t examined=0,shape=0;
    for(std::size_t si=0;si<sectionCount;++si) {
        const auto at=table+si*40;
        const auto rva=Monstey_U32(image,at+12),length=Monstey_U32(image,at+16),
            offset=Monstey_U32(image,at+20),flags=Monstey_U32(image,at+36);
        Monstey_Require(offset<=image.size()&&length<=image.size()-offset,"PE raw section exceeds file");
        if(!(flags&0x40000000u)||length<64)continue;
        for(std::size_t local=0;local<=length-64;local+=8) {
            ++examined;
            const auto raw=image.subspan(static_cast<std::size_t>(offset)+local,64);
            const auto count=Monstey_U64(raw,8),namePointer=Monstey_U64(raw,16),
                fullPointer=Monstey_U64(raw,32),flags64=Monstey_U64(raw,56);
            if(count>=2048||!namePointer||!fullPointer||Monstey_U64(raw,48)!=0||
                (flags64>>32)>=4||(flags64&0xffffffffull)>=64)continue;
            std::string shortName,fullName;
            try {shortName=reader.Text(namePointer);fullName=reader.Text(fullPointer);}
            catch(const Monstey_Error&){continue;}
            if(!Monstey_RegistryName(shortName)||!Monstey_RegistryName(fullName))continue;
            if(!count) {
                const auto flat=Monstey_RegistryFlat(fullName),shortFlat=Monstey_RegistryFlat(shortName);
                const auto dot=fullName.rfind('.');
                const auto tail=Monstey_RegistryFlat(dot==fullName.npos?fullName:fullName.substr(dot+1));
                if(flat.find(shortFlat)==flat.npos&&shortFlat.find(tail)==shortFlat.npos)continue;
            }
            const auto fields=Monstey_U64(raw,0),parent=Monstey_U64(raw,40);
            if(count) {
                try {const auto first=reader.At(fields,72);
                    const auto name=reader.Text(Monstey_U64(first,0));
                    if(name.empty()||Monstey_NativeHash(name)!=Monstey_U32(first,32)||
                        Monstey_U32(first,36)>255)continue;
                }catch(const Monstey_Error&){continue;}
            }
            ++shape;
            Monstey_RegistryCandidate c;
            Monstey_Require(imageBase<=UINT64_MAX-rva&&imageBase+rva<=UINT64_MAX-local,
                "PE native entry address overflow");
            c.address=imageBase+rva+local;c.parent=parent;
            c.entry=J::object({{"entry_ea",Monstey_Hex(c.address)},{"name",fullName},
                {"parent_ea",Monstey_Hex(parent)},{"fields_ea",Monstey_Hex(fields)},
                {"count",count},{"flags",Monstey_Hex(flags64)},
                {"entry_sha256",Monstey_HashSha256(raw)}});
            c.category=J::object({{"name",fullName},{"short_name",shortName},
                {"struct",(flags64&2)!=0},{"flags",Monstey_Hex(flags64)},
                {"field_count",count},{"unreadable_fields",0},{"fields",J::array()}});
            try {
                c.category["description"]=reader.Text(Monstey_U64(raw,24));
                Monstey_Require(count<=256,"Native table exceeds UInt8 identity envelope");
                Monstey_Require(!count||fields<=UINT64_MAX-(count-1)*72,"Native field table overflow");
                std::map<std::uint32_t,bool> hashes;
                bool callbacks=true;
                for(std::uint64_t i=0;i<count;++i) {
                    auto field=reader.Field(fields+i*72,i);
                    const auto rawField=reader.At(fields+i*72,72);
                    const auto callback=Monstey_U64(rawField,56);
                    Monstey_Require(!callback||callbackExecutable(callback),"Native index callback is not file-backed executable code");
                    field["native_index_callback_executable"]=callback!=0;
                    const auto hash=Monstey_NativeHash(field.at("name").as_string());
                    const bool marker=(field.at("flags").as_uint()&0x280)!=0;
                    const auto [previous,inserted]=hashes.emplace(hash,marker);
                    Monstey_Require(inserted||(marker&&previous->second),"Duplicate consumed field hash");
                    if(!marker&&field.at("native_ordinal_source").as_string()!="native_index_callback")callbacks=false;
                    c.category["fields"].as_array().push_back(std::move(field));
                }
                c.category["native_index_callbacks_qualified"]=callbacks;
                c.category["complete_declaration_table"]=true;
            }catch(const Monstey_Error& e){c.failure=e.what();}
            candidates[fullName].push_back(std::move(c));
        }
    }
    auto categories=J::object(),entries=J::array(),unknown=J::array(),ambiguous=J::array();
    std::map<std::uint64_t,std::string> names;
    std::map<std::string,const Monstey_RegistryCandidate*> selected;
    for(const auto& [name,rows]:candidates) {
        if(rows.size()!=1) {
            auto eas=J::array();for(const auto& row:rows)eas.as_array().push_back(Monstey_Hex(row.address));
            ambiguous.as_array().push_back(J::object({{"name",name},{"entries",eas}}));continue;
        }
        const auto& row=rows.front();
        if(!row.failure.empty()) {
            unknown.as_array().push_back(J::object({{"name",name},{"entry",row.entry},{"reason",row.failure},
                {"complete_fields_read",static_cast<std::uint64_t>(row.category.at("fields").size())}}));continue;
        }
        names.emplace(row.address,name);selected.emplace(name,&row);
    }
    std::map<std::string,std::vector<std::string>> ancestry;
    std::set<std::string> unqualified;
    for(const auto& [name,row]:selected) {
        std::vector<std::string> chain;
        std::set<std::uint64_t> seen{row->address};
        auto parent=row->parent;
        std::string reason;
        while(parent) {
            if(chain.size()>=63) {reason="Parent declaration exceeds the reader's 63-ancestor qualified limit";break;}
            const auto found=names.find(parent);
            if(found==names.end()||!seen.insert(parent).second) {
                reason="Parent declaration is absent, ambiguous, unsupported or cyclic";break;
            }
            chain.push_back(found->second);
            parent=selected.at(found->second)->parent;
        }
        if(!reason.empty()) {
            unqualified.insert(name);
            unknown.as_array().push_back(J::object({{"name",name},{"entry",row->entry},{"reason",reason}}));
        } else ancestry.emplace(name,std::move(chain));
    }
    // Eligibility is now independent of lexical visitation/cache order. Preserve
    // the previous parent-before-child proof ordering for qualified native inputs.
    std::function<void(const std::string&)> emit=[&](const std::string& name) {
        if(categories.contains(name))return;
        const auto& row=*selected.at(name);const auto& chain=ancestry.at(name);
        auto category=row.category;
        if(!chain.empty()) {
            emit(chain.front());category["parent"]=chain.front();
            auto ancestors=J::array();for(const auto& parent:chain)ancestors.as_array().push_back(parent);
            category["ancestors"]=std::move(ancestors);
        }
        categories[name]=std::move(category);entries.as_array().push_back(row.entry);
    };
    for(const auto& [name,row]:selected){(void)row;if(!unqualified.contains(name))emit(name);}
    return J::object({{"format","Monstey_NativeGadbDeclarationRegistry"},{"version",1},
        {"source_sha256",sha},{"image_base",Monstey_Hex(imageBase)},
        {"original_executed",false},{"original_modified",false},
        {"discovery_scope","File-backed aligned 64-byte native declaration shapes; not runtime registration membership"},
        {"qwords_examined",examined},{"candidate_entries",shape},
        {"categories",categories},{"entries",entries},{"unknown",unknown},{"ambiguous",ambiguous},
        {"diagnostics",diagnostics}});
}
} // namespace
Monstey_Json Monstey_GadbDiscoverNativeDeclarations(S image,std::string_view expected) {
    return Monstey_GadbRegistryScan(image,expected);
}
} // namespace Monstey
