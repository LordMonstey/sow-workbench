#include "Monstey_FxReflection.hpp"
#include "Monstey_Gadb.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
constexpr std::uint64_t base = 0x140000000ull, parent = base + 0x3000, child = base + 0x3040;
void Monstey_Put(Monstey_Bytes &bytes, std::size_t offset, std::uint64_t value, std::size_t count) {
    Monstey_CheckRange(bytes, offset, count);
    for (std::size_t i = 0; i < count; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::size_t Monstey_At(std::uint64_t address) {
    return static_cast<std::size_t>(address - base - 0x1000 + 0x400);
}
Monstey_Bytes Monstey_Fixture() {
    Monstey_Bytes b(0x5400);
    Monstey_Put(b, 0, 0x5a4d, 2);
    Monstey_Put(b, 0x3c, 0x80, 4);
    Monstey_Put(b, 0x80, 0x4550, 4);
    Monstey_Put(b, 0x84, 0x8664, 2);
    Monstey_Put(b, 0x86, 1, 2);
    Monstey_Put(b, 0x94, 0xf0, 2);
    Monstey_Put(b, 0x98, 0x20b, 2);
    Monstey_Put(b, 0xb0, base, 8);
    constexpr std::size_t section = 0x188;
    Monstey_Put(b, section + 8, 0x5000, 4);
    Monstey_Put(b, section + 12, 0x1000, 4);
    Monstey_Put(b, section + 16, 0x5000, 4);
    Monstey_Put(b, section + 20, 0x400, 4);
    std::uint64_t next = base + 0x1100;
    const auto text = [&](std::string_view value) mutable {
        const auto address = next;
        for (const auto c : value)
            b[Monstey_At(next++)] = static_cast<std::uint8_t>(c);
        b[Monstey_At(next++)] = 0;
        return address;
    };
    auto writeText = text;
    const auto falseToken = writeText("false"), pName = writeText("BaseFX"),
               cName = writeText("ClientFX.SyntheticCreate");
    const auto field = [&](std::uint64_t address, std::string_view name, std::uint32_t type,
                           std::uint64_t value, unsigned ordinal) {
        const auto at = Monstey_At(address);
        Monstey_Put(b, at, writeText(name), 8);
        Monstey_Put(b, at + 16, value, 8);
        Monstey_Put(b, at + 32, Monstey_NativeHash(name), 4);
        Monstey_Put(b, at + 36, type, 4);
        Monstey_Put(b, at + 40, 1, 4);
        Monstey_Put(b, at + 44, 0x42, 4);
        Monstey_Put(b, at + 64, ordinal, 1);
        const auto callback = base + 0x4000 + ordinal * 8;
        Monstey_Put(b, at + 56, callback, 8);
        if (ordinal == 0) {
            b[Monstey_At(callback)] = 0x32;
            b[Monstey_At(callback) + 1] = 0xc0;
        } else {
            b[Monstey_At(callback)] = 0xb0;
            b[Monstey_At(callback) + 1] = static_cast<std::uint8_t>(ordinal);
        }
        b[Monstey_At(callback) + 2] = 0xc3;
    };
    field(base + 0x2000, "StartTime", 2, std::bit_cast<std::uint32_t>(.5f), 0);
    field(base + 0x2048, "Enabled", 1, falseToken, 1);
    field(base + 0x2200, "StartTime", 2, std::bit_cast<std::uint32_t>(.5f), 0);
    field(base + 0x2248, "Enabled", 1, falseToken, 1);
    field(base + 0x2290, "NumToCreate", 3, 1, 2);
    field(base + 0x22d8, "Effect0", 4, 0, 3);
    const auto category = [&](std::uint64_t address, std::uint64_t table, unsigned count, std::uint64_t name,
                              std::uint64_t parentAddress, std::uint64_t flags) {
        const auto at = Monstey_At(address);
        Monstey_Put(b, at, table, 8);
        Monstey_Put(b, at + 8, count, 8);
        Monstey_Put(b, at + 16, name, 8);
        Monstey_Put(b, at + 32, name, 8);
        Monstey_Put(b, at + 40, parentAddress, 8);
        Monstey_Put(b, at + 56, flags, 8);
    };
    category(parent, base + 0x2000, 2, pName, 0, 0x100000000ull);
    category(child, base + 0x2200, 4, cName, parent, 0x180000000ull);
    return b;
}
J Monstey_Reflect(const Monstey_Bytes &bytes) {
    const std::array entries{child};
    return Monstey_FxReflectCategories(bytes, entries);
}
void Monstey_Write(const std::filesystem::path &path, const J &value) {
    Monstey_WriteNewText(path, value.dump(2) + "\n");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const auto out =
            (argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(L"fx-reflection-tests")) /
            (L"FxReflection_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"},
                                 {"python_product_runtime", false},
                                 {"originals_modified", false},
                                 {"cases", J::array()}});
        const auto test = [&](std::string_view name, const std::function<void()> &action) {
            try {
                action();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", std::string(name) + ": " + e.what());
            }
            report["cases"].as_array().push_back(std::string(name));
        };
        const auto reject = [&](const std::function<void()> &action) {
            bool bad = false;
            try {
                action();
            } catch (const std::exception &) {
                bad = true;
            }
            Monstey_Require(bad, "Expected bounded reflection failure");
        };
        const auto source = Monstey_Fixture();
        const auto fixturePath = out / L"synthetic-reflection.pe";
        Monstey_WriteNewFile(fixturePath, source);
        report["synthetic_fixture"] = J::object({{"file", "synthetic-reflection.pe"},
                                                 {"sha256", Monstey_HashSha256(source)},
                                                 {"category_entries", J::array({Monstey_Hex(child)})}});
        test("shared_pe_virtual_mapping_bounds", [&] {
            Monstey_Require(Monstey_FearPeRange(source, child, 64).size() == 64, "PE view wrong size");
            reject([&] { Monstey_FearPeRange(source, base - 1, 1); });
            reject([&] { Monstey_FearPeRange(source, base + 0x6000, 1); });
            reject([&] { Monstey_FearPeRange(source, base + 0x5ff0, 32); });
        });
        test("native_category_bit31_and_exact_parent_defaults", [&] {
            const auto r = Monstey_Reflect(source), c = r["categories"]["ClientFX.SyntheticCreate"];
            Monstey_Require(
                r["categories"].size() == 2 && c["parent"].as_string() == "BaseFX" &&
                    c["flags"].as_string() == "0x180000000" && c["fields"][0u]["default"].as_number() == .5 &&
                    !c["fields"][1u]["default"].as_bool() && c["fields"][2u]["default"].as_int() == 1 &&
                    c["fields"][3u]["default"].is_null(),
                "Category flags, inheritance or typed defaults changed");
        });
        test("immutable_callback_identity_is_separate_from_mutable_cache", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(base + 0x2290) + 64, 1, 1);
            const auto r = Monstey_Reflect(b);
            const auto field = r["categories"]["ClientFX.SyntheticCreate"]["fields"][2u];
            Monstey_Require(field["cached_ordinal"].as_uint() == 1 &&
                                field["native_ordinal"].as_uint() == 2 &&
                                field["native_ordinal_source"].as_string() == "native_index_callback",
                            "Mutable cache was reused as immutable field identity");
        });
        test("unsupported_and_inconsistent_index_callbacks_reject", [&] {
            auto b = source;
            b[Monstey_At(base + 0x4010)] = 0x55;
            reject([&] { Monstey_Reflect(b); });
            b = source;
            b[Monstey_At(base + 0x4010) + 1] = 7;
            reject([&] { Monstey_Reflect(b); });
        });
        test("snapshot_hash_is_verified_before_decoding", [&] {
            const std::array entries{child};
            reject([&] { Monstey_FxReflectCategories(source, entries, std::string(64, '0')); });
            Monstey_FxReflectCategories(source, entries, Monstey_HashSha256(source));
        });
        test("native_field_hash_mismatch_rejects", [&] {
            auto b = source;
            b[Monstey_At(base + 0x2200) + 32] ^= 1;
            reject([&] { Monstey_Reflect(b); });
        });
        test("native_parent_cycle_rejects", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(parent) + 40, child, 8);
            reject([&] { Monstey_Reflect(b); });
        });
        test("duplicate_full_names_within_parent_chain_reject", [&] {
            auto b = source;
            const auto parentName = Monstey_U64(b, Monstey_At(parent) + 32);
            Monstey_Put(b, Monstey_At(child) + 32, parentName, 8);
            reject([&] { Monstey_Reflect(b); });
        });
        test("native_field_type_count_and_table_extent_reject", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(base + 0x2200) + 36, 0, 4);
            reject([&] { Monstey_Reflect(b); });
            b = source;
            Monstey_Put(b, Monstey_At(base + 0x2200) + 40, 65536, 4);
            reject([&] { Monstey_Reflect(b); });
            b = source;
            Monstey_Put(b, Monstey_At(child), base + 0x5ff0, 8);
            reject([&] { Monstey_Reflect(b); });
        });
        test("duplicate_field_hashes_reject", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(base + 0x2248), Monstey_U64(b, Monstey_At(base + 0x2200)), 8);
            Monstey_Put(b, Monstey_At(base + 0x2248) + 32, Monstey_U32(b, Monstey_At(base + 0x2200) + 32), 4);
            reject([&] { Monstey_Reflect(b); });
            Monstey_Put(b, Monstey_At(base + 0x2200) + 44, 0x80, 4);
            reject([&] { Monstey_Reflect(b); });
            Monstey_Put(b, Monstey_At(base + 0x2200) + 44, 0x42, 4);
            Monstey_Put(b, Monstey_At(base + 0x2248) + 44, 0x80, 4);
            reject([&] { Monstey_Reflect(b); });
        });
        test("duplicate_editor_markers_keep_both_full_native_identities", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(base + 0x2248), Monstey_U64(b, Monstey_At(base + 0x2200)), 8);
            Monstey_Put(b, Monstey_At(base + 0x2248) + 32, Monstey_U32(b, Monstey_At(base + 0x2200) + 32), 4);
            Monstey_Put(b, Monstey_At(base + 0x2200) + 44, 0x40000080, 4);
            Monstey_Put(b, Monstey_At(base + 0x2248) + 44, 0x40000080, 4);
            const auto proof = Monstey_Reflect(b);
            const auto fields = proof["categories"]["ClientFX.SyntheticCreate"]["fields"];
            Monstey_Require(fields.size() == 4 && fields[0u]["hash"] == fields[1u]["hash"] &&
                                fields[0u]["native_ordinal"].as_uint() == 0 &&
                                fields[1u]["native_ordinal"].as_uint() == 1 &&
                                fields[0u]["group_marker"].as_bool() && fields[1u]["group_marker"].as_bool(),
                            "Duplicate markers were collapsed or reindexed");
        });
        test("nonfinite_native_default_keeps_raw_bits", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(base + 0x2200) + 16, 0x7fc00042, 4);
            const auto r = Monstey_Reflect(b);
            Monstey_Require(
                r["categories"]["ClientFX.SyntheticCreate"]["fields"][0u]["default"]["nonfinite_native_bits"]
                        .as_string() == "0x7fc00042",
                "NaN bits were normalized");
        });
        test("unterminated_text_and_nonzero_reserved_category_reject", [&] {
            auto b = source;
            Monstey_Put(b, Monstey_At(child) + 32, base + 0x4000, 8);
            std::fill(b.begin() + static_cast<std::ptrdiff_t>(Monstey_At(base + 0x4000)), b.end(),
                      static_cast<std::uint8_t>('A'));
            reject([&] { Monstey_Reflect(b); });
            b = source;
            Monstey_Put(b, Monstey_At(child) + 48, 1, 8);
            reject([&] { Monstey_Reflect(b); });
        });
        test("captured_binding_state_is_full_U32_at_48", [&] {
            auto b=source;Monstey_Put(b,Monstey_At(base+0x2290)+48,0x12345678u,4);
            const auto f=Monstey_Reflect(b)["categories"]["ClientFX.SyntheticCreate"]["fields"][2u];
            Monstey_Require(f["captured_binding_state"].as_uint()==0x12345678u && f["native_ordinal"].as_uint()==2,"Binding state was truncated or used as identity");
        });
        test("state_cache_and_callback_remain_independent", [&] {
            auto b=source;Monstey_Put(b,Monstey_At(base+0x2290)+48,1,4);Monstey_Put(b,Monstey_At(base+0x2290)+64,255,1);
            const auto f=Monstey_Reflect(b)["categories"]["ClientFX.SyntheticCreate"]["fields"][2u];
            Monstey_Require(f["captured_binding_state"].as_uint()==1&&f["cached_ordinal"].as_uint()==255&&f["native_ordinal"].as_uint()==2,"Mutable values were normalized or substituted");
        });
        test("captured_zero_state_is_retained", [&] {
            const auto f=Monstey_Reflect(source)["categories"]["ClientFX.SyntheticCreate"]["fields"][2u];
            Monstey_Require(f["captured_binding_state"].as_uint()==0,"Unbound state not captured");
        });
        test("cached_state_two_does_not_claim_runtime", [&] {
            auto b=source;Monstey_Put(b,Monstey_At(base+0x2200)+48,2,4);const auto r=Monstey_Reflect(b);
            Monstey_Require(r["categories"]["ClientFX.SyntheticCreate"]["fields"][0u]["captured_binding_state"].as_uint()==2&&!r["original_executed"].as_bool(),"File state confused with engine execution");
        });
        test("unknown_raw_state_is_not_coerced", [&] {
            auto b=source;Monstey_Put(b,Monstey_At(base+0x2200)+48,UINT32_MAX,4);
            Monstey_Require(Monstey_Reflect(b)["categories"]["ClientFX.SyntheticCreate"]["fields"][0u]["captured_binding_state"].as_uint()==UINT32_MAX,"Raw state narrowed to a guessed enum");
        });
        test("class_init_byte_remains_in_existing_flags", [&] {
            auto b=source;Monstey_Put(b,Monstey_At(parent)+60,0x7f,1);
            const auto r=Monstey_Reflect(b);
            Monstey_Require(r["categories"]["BaseFX"]["flags"].as_string()=="0x7f00000000","Existing raw class flags changed");
        });
        if (argc > 2)
            test("actual_native_create_reflection_and_candidate_schema", [&] {
                const auto image = Monstey_ReadFile(argv[2]);
                const std::array<std::uint64_t, 5> entries{0x1426406c0, 0x142640600, 0x142640580, 0x142640540,
                                                           0x142640080};
                const auto r = Monstey_FxReflectCategories(
                    image, entries, "6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455");
                const auto &cats = r["categories"];
                Monstey_Require(cats.size() == 9 &&
                                    cats["ClientFX.Create"]["parent"].as_string() == "BaseCreateFX" &&
                                    cats["BaseCreateFX"]["parent"].as_string() == "BaseFX" &&
                                    cats["ClientFX.Create"]["fields"].size() == 38,
                                "Actual native Create hierarchy differs");
                for (const auto name : {"ClientFX.DynaLight", "ClientFX.Create", "ClientFX.DebrisSystem",
                                        "ClientFX.Sprite", "ClientFX.FlareSprite"}) {
                    const auto &fields = cats[name]["fields"];
                    for (std::size_t i = 0; i < fields.size(); ++i)
                        Monstey_Require(fields[i]["native_ordinal"].as_uint() == i &&
                                            fields[i]["native_ordinal_source"].as_string() ==
                                                "native_index_callback",
                                        "Actual native immutable identity lacks its exact callback proof");
                }
                Monstey_Write(out / L"native-reflection.json", r);
                report["native_image_sha256"] = r["source_sha256"];
                if (argc > 3) {
                    auto candidate = J::Parse(Monstey_ReadText(argv[3]));
                    for (const auto name : {"BaseCreateFX", "ClientFX.Create", "ClientFX.DebrisSystem"})
                        candidate["categories"][name] = cats[name];
                    candidate["provenance"]["create_native_image_sha256"] = r["source_sha256"];
                    candidate["provenance"]["create_native_descriptor"] = "0x1426406c0";
                    candidate["provenance"]["create_parent_descriptor"] = "0x142640840";
                    candidate["provenance"]["create_scanner_omission"] =
                        "Earlier scanner excluded category low flags >=64; native Create has bit31 set.";
                    if (argc > 6) {
                        const auto sourceSchema = J::Parse(Monstey_ReadText(argv[6]));
                        for (const auto &[label, definition] : candidate["categories"].as_object()) {
                            (void)label;
                            for (const auto &field : definition["fields"].as_array())
                                if (field.contains("enum") &&
                                    !candidate["enums"].contains(field["enum"].as_string())) {
                                    const auto enumName = field["enum"].as_string();
                                    Monstey_Require(
                                        sourceSchema["enums"].contains(enumName),
                                        "Required native enum is absent from qualified source schema");
                                    candidate["enums"][enumName] = sourceSchema["enums"][enumName];
                                }
                        }
                    }
                    const auto schemaPath = out / L"fx-schema-with-create.candidate.json";
                    Monstey_Write(schemaPath, candidate);
                    report["candidate_schema_sha256"] = Monstey_HashSha256(schemaPath);
                    if (argc > 5) {
                        const auto corpus = J::Parse(Monstey_ReadText(argv[5]));
                        std::uint64_t files = 0, createFiles = 0, records = 0, debrisRecords = 0, fields = 0,
                                      links = 0;
                        for (const auto &file : corpus["files"].as_array()) {
                            bool hasCreate = false;
                            for (const auto &klass : file["key_classes"].as_array())
                                hasCreate |= klass.as_string() == "ClientFX.Create";
                            if (hasCreate)
                                ++createFiles;
                            const auto path =
                                std::filesystem::path(argv[4]) / Monstey_Wide(file["path"].as_string());
                            try {
                                auto db = Monstey_Gadb::WithSchema(path, candidate);
                                Monstey_Require(db->Describe()["source_sha256"] == file["sha256"],
                                                "Native corpus snapshot changed");
                                const auto categories = db->Categories();
                                for (const auto &category : categories["rows"].as_array()) {
                                    const auto klass = category["schema"].as_string();
                                    if (klass != "ClientFX.Create" && klass != "ClientFX.DebrisSystem" &&
                                        klass != "FxGlobalProps")
                                        continue;
                                    const auto rows =
                                        db->Records(static_cast<std::uint32_t>(category["index"].as_uint()),
                                                    {}, 0, 10000);
                                    for (const auto &row : rows["rows"].as_array()) {
                                        const auto selector = row["native_reference"].as_string();
                                        const auto meta = db->Metadata(selector);
                                        for (const auto &field : meta["fields"].as_array()) {
                                            if (klass == "ClientFX.Create" ||
                                                klass == "ClientFX.DebrisSystem") {
                                                const J *declaration = nullptr;
                                                for (const auto &native : cats[klass]["fields"].as_array())
                                                    if (native["hash"] == field["hash"])
                                                        declaration = &native;
                                                Monstey_Require(
                                                    declaration != nullptr &&
                                                        (field["native_type"] ==
                                                             (*declaration)["type_code"] ||
                                                         (field["native_type"].as_uint() == 68 &&
                                                          (*declaration)["type_code"].as_uint() == 4)),
                                                    "Native stored FX field does not match reflected "
                                                    "name/type");
                                                db->SetField(selector, field["hash"].as_string(),
                                                             field["value"]);
                                                ++fields;
                                            } else if (field["hash"].as_string() ==
                                                       Monstey_Hex(Monstey_NativeHash("KeyList"))) {
                                                db->SetField(selector, field["hash"].as_string(),
                                                             field["value"]);
                                                links += field["value"].size();
                                            }
                                        }
                                        if (klass == "ClientFX.Create")
                                            ++records;
                                        if (klass == "ClientFX.DebrisSystem")
                                            ++debrisRecords;
                                    }
                                }
                                Monstey_Require(!db->Describe()["native_dirty"].as_bool(),
                                                "Native field validation changed original typed values");
                                Monstey_Require(Monstey_HashSha256(path) == file["sha256"].as_string(),
                                                "Original FX corpus file changed");
                                ++files;
                            } catch (const std::exception &e) {
                                throw Monstey_Error("native_cohort",
                                                    file["path"].as_string() + ": " + e.what());
                            }
                        }
                        Monstey_Require(files == 436 && createFiles == 83 && records == 160 &&
                                            debrisRecords == 2 && links == 3327,
                                        "Actual Create cohort differs from pinned corpus");
                        report["native_create_cohort"] = J::object({{"files", files},
                                                                    {"create_files", createFiles},
                                                                    {"records", records},
                                                                    {"debris_records", debrisRecords},
                                                                    {"stored_fields_typed", fields},
                                                                    {"native_links_validated", links}});
                    }
                }
            });
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        Monstey_Write(out / L"fx-reflection-tests.json", report);
        std::cout << "Native FX reflection tests: " << report["passed"].as_uint() << " passed\n"
                  << Monstey_Utf8(out.wstring()) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
