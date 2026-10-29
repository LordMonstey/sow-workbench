#include "Monstey_Blend.hpp"
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
void Monstey_Pad(Monstey_Bytes &b) {
    while (b.size() % 4)
        b.push_back(0);
}
void Monstey_Int(Monstey_Bytes &b, unsigned n, std::uint64_t v, bool little) {
    for (unsigned i = 0; i < n; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> ((little ? i : n - 1 - i) * 8)));
}
void Monstey_Tag(Monstey_Bytes &b, std::string_view text) { b.insert(b.end(), text.begin(), text.end()); }
Monstey_Bytes Monstey_BlendFixture(unsigned format = 1, unsigned pointer = 8, bool little = true, bool scoped=false) {
    Monstey_Bytes dna;
    const auto integer = [&](unsigned n, std::uint64_t v) { Monstey_Int(dna, n, v, little); };
    Monstey_Tag(dna, "SDNANAME");
    integer(4, 4);
    for (const auto *s : {"name[8]", "value", "point[3]", "*next"}) {
        Monstey_Tag(dna, s);
        dna.push_back(0);
    }
    Monstey_Pad(dna);
    Monstey_Tag(dna, "TYPE");
    integer(4, 4);
    for (const auto *s : {"char", "int", "float", "Thing"}) {
        Monstey_Tag(dna, s);
        dna.push_back(0);
    }
    Monstey_Pad(dna);
    Monstey_Tag(dna, "TLEN");
    for (const auto n : {1u, 4u, 4u, 24u + pointer})
        integer(2, n);
    Monstey_Pad(dna);
    Monstey_Tag(dna, "STRC");
    integer(4, 1);
    integer(2, 3);
    integer(2, 4);
    for (unsigned i = 0; i < 4; ++i) {
        integer(2, i);
        integer(2, i);
    }
    Monstey_Bytes data;
    Monstey_Tag(data, std::string_view("source", 6));
    data.resize(8);
    Monstey_Int(data, 4, 7, little);
    for (const float f : {1.0f, 2.0f, 3.0f})
        Monstey_Int(data, 4, std::bit_cast<std::uint32_t>(f), little);
    Monstey_Int(data, pointer, 0, little);
    Monstey_Bytes file;
    if (format)
        Monstey_Tag(file, "BLENDER17-01v0502");
    else {
        Monstey_Tag(file, "BLENDER");
        file.push_back(pointer == 8 ? '-' : '_');
        file.push_back(little ? 'v' : 'V');
        Monstey_Tag(file, "405");
    }
    const auto block = [&](std::string_view code, std::uint64_t address, std::uint64_t count,
                           const Monstey_Bytes &bytes) {
        for (unsigned i = 0; i < 4; ++i)
            file.push_back(i < code.size() ? static_cast<std::uint8_t>(code[i]) : 0);
        if (format) {
            Monstey_Int(file, 4, 0, true);
            Monstey_Int(file, 8, address, true);
            Monstey_Int(file, 8, bytes.size(), true);
            Monstey_Int(file, 8, count, true);
        } else {
            Monstey_Int(file, 4, bytes.size(), little);
            Monstey_Int(file, pointer, address, little);
            Monstey_Int(file, 4, 0, little);
            Monstey_Int(file, 4, count, little);
        }
        file.insert(file.end(), bytes.begin(), bytes.end());
    };
    if(scoped){
      block("OB",0x20000,1,data);block("DATA",0x30000,1,data);
      Monstey_PutU32(data,8,9);block("OB",0x40000,1,data);block("DATA",0x30000,1,data);
    }else block("DATA", 0x10000, 1, data);
    block("DNA1", 0, 1, dna);
    block("ENDB", 0, 0, {});
    return file;
}
Monstey_Bytes Monstey_RawFrame(const Monstey_Bytes &data) {
    Monstey_Bytes b;
    Monstey_AppendU32(b, 0xfd2fb528);
    if (data.size() < 256) {
        b.push_back(0x20);
        b.push_back(static_cast<std::uint8_t>(data.size()));
    } else {
        Monstey_Require(data.size() < 65792, "Raw test frame size");
        b.push_back(0x60);
        Monstey_AppendU16(b, static_cast<std::uint16_t>(data.size() - 256));
    }
    const auto header = static_cast<std::uint32_t>(data.size()) * 8 + 1;
    Monstey_Int(b, 3, header, true);
    b.insert(b.end(), data.begin(), data.end());
    return b;
}
Monstey_Bytes Monstey_GzipFixture(const Monstey_Bytes &data) {
    Monstey_Require(data.size() <= 65535, "Stored test gzip extent");
    Monstey_Bytes out = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 255, 1};
    Monstey_AppendU16(out, static_cast<std::uint16_t>(data.size()));
    Monstey_AppendU16(out, static_cast<std::uint16_t>(~static_cast<std::uint16_t>(data.size())));
    out.insert(out.end(), data.begin(), data.end());
    std::uint32_t crc = 0xffffffffu;
    for (const auto b : data) {
        crc ^= b;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    Monstey_AppendU32(out, crc ^ 0xffffffffu);
    Monstey_AppendU32(out, static_cast<std::uint32_t>(data.size()));
    return out;
}
void Monstey_TestWrite(const std::filesystem::path &path, const Monstey_Bytes &data) {
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    Monstey_WriteNewFile(path, data);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const std::filesystem::path dir = argc > 1 ? argv[1] : L"blend-test-output";
        std::filesystem::create_directories(dir);
        unsigned passed = 0;
        auto report = Monstey_Json::object();
        report["cases"] = Monstey_Json::array();
        std::set<std::string> uniqueCases;
        const auto test = [&](const char *name, const std::function<void()> &fn) {
            Monstey_Require(uniqueCases.insert(name).second, "Duplicate test case name");
            try {
                fn();
            } catch (const std::exception &e) {
                throw std::runtime_error(std::string(name) + ": " + e.what());
            }
            ++passed;
            report["cases"].as_array().push_back(name);
        };
        const auto reject = [&](const std::function<void()> &fn) {
            bool failed = false;
            try {
                fn();
            } catch (const std::exception &) {
                failed = true;
            }
            Monstey_Require(failed, "Expected Blend input rejection");
        };
        const auto fixture = Monstey_BlendFixture();
        const auto path = dir / L"synthetic.blend";
        Monstey_TestWrite(path, fixture);
        test("modern_header_sdna_exact_offsets", [&] {
            Monstey_Blend b(path);
            const auto d = b.Describe();
            Monstey_Require(d["container_version"].as_uint() == 1 && d["blocks"].as_uint() == 3 &&
                                d["exact_struct_layouts"].as_uint() == 1,
                            "Wrong modern header/schema");
            const auto fields = b.Fields("Thing");
            Monstey_Require(fields[2u]["offset"].as_uint() == 12 && fields[3u]["offset"].as_uint() == 24,
                            "Wrong field offsets");
            Monstey_Require(b.ReadField(0, "name").as_string() == "source" &&
                                b.ReadField(0, "value").as_int() == 7 &&
                                b.ReadField(0, "point[1]").as_number() == 2,
                            "Wrong typed values");
        });
        test("legacy_64_and_big_endian_32", [&] {
            for (const auto pointer : {4u, 8u})
                for (const bool little : {false, true}) {
                    const auto file =
                        dir / (std::to_wstring(pointer) + (little ? L"-little.blend" : L"-big.blend"));
                    Monstey_TestWrite(file, Monstey_BlendFixture(0, pointer, little));
                    Monstey_Blend b(file);
                    Monstey_Require(b.ReadField(0, "value").as_int() == 7 &&
                                        b.ReadField(0, "point[2]").as_number() == 3,
                                    "Legacy endian/layout mismatch");
                    const auto out = dir / (file.filename().wstring() + L"-out.blend");
                    if (std::filesystem::exists(out))
                        std::filesystem::remove(out);
                    Monstey_Require(b.Write(out)["byte_exact_decoded"].as_bool(),
                                    "Legacy no-op bytes changed");
                }
        });
        test("zstd_concatenated_frames_native_decoder", [&] {
            const auto middle = fixture.size() / 2;
            Monstey_Bytes a(fixture.begin(), fixture.begin() + static_cast<std::ptrdiff_t>(middle)),
                c(fixture.begin() + static_cast<std::ptrdiff_t>(middle), fixture.end());
            auto bytes = Monstey_RawFrame(a);
            const auto tail = Monstey_RawFrame(c);
            bytes.insert(bytes.end(), tail.begin(), tail.end());
            const auto file = dir / L"compressed.blend";
            Monstey_TestWrite(file, bytes);
            Monstey_Blend b(file);
            Monstey_Require(b.Describe()["zstd_compressed"].as_bool() &&
                                b.ReadField(0, "name").as_string() == "source",
                            "Concatenated Zstd decoding failed");
        });
        test("typed_edits_and_pointer_block_creation", [&] {
            Monstey_Blend b(path);
            b.SetField(0, "name", "edited");
            b.SetField(0, "value", -17);
            b.SetField(0, "point", Monstey_Json::array({4.0, 5.0, 6.0}));
            const auto added = b.NewBlock("DATA", "Thing");
            const auto index = static_cast<std::size_t>(added["block"].as_uint());
            b.SetField(index, "name", "child");
            b.SetField(0, "next", added["address"]);
            const auto out = dir / L"edited.blend";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            b.Write(out);
            Monstey_Blend check(out);
            Monstey_Require(check.ReadField(0, "value").as_int() == -17 &&
                                check.ReadField(index, "name").as_string() == "child",
                            "Typed edits lost");
            Monstey_Require(check.BytesAtAddress(added["address"].as_uint(), 5) ==
                                Monstey_Bytes({'c', 'h', 'i', 'l', 'd'}),
                            "File pointer resolution failed");
        });
        test("failed_array_edit_is_transactional", [&] {
            Monstey_Blend b(path);
            const auto before = b.ReadField(0, "point");
            reject([&] { b.SetField(0, "point", Monstey_Json::array({9.0, "invalid", 8.0})); });
            Monstey_Require(before == b.ReadField(0, "point"), "Failed field edit partially committed");
        });
        test("invalid_header_bounds_and_source_conflict", [&] {
            for (const auto n : {0u, 7u, 11u, 16u}) {
                const auto file = dir / (L"truncated-" + std::to_wstring(n) + L".blend");
                Monstey_TestWrite(file, Monstey_Bytes(fixture.begin(), fixture.begin() + n));
                reject([&] { Monstey_Blend b(file); });
            }
            Monstey_Blend b(path);
            reject([&] { b.Write(path); });
            reject([&] { b.ReadField(0, "point[3]"); });
            reject([&] { b.ReadField(0, "missing"); });
        });
        test("truncated_zstd_rejected", [&] {
            auto data = Monstey_RawFrame(fixture);
            data.pop_back();
            const auto file = dir / L"bad-zstd.blend";
            Monstey_TestWrite(file, data);
            reject([&] { Monstey_Blend b(file); });
        });
        test("short_legacy_terminator_remains_byte_exact", [&] {
            auto bytes = Monstey_BlendFixture(0, 8, true);
            bytes.resize(bytes.size() - 16);
            const auto input = dir / L"short-end.blend", output = dir / L"short-end-out.blend";
            Monstey_TestWrite(input, bytes);
            Monstey_Blend b(input);
            if (std::filesystem::exists(output))
                std::filesystem::remove(output);
            Monstey_Require(b.Write(output)["byte_exact_decoded"].as_bool() &&
                                Monstey_ReadFile(output) == bytes,
                            "Short legacy ENDB changed");
        });
        test("raw_attribute_buffers_and_cloned_typed_blocks", [&] {
            Monstey_Blend b(path);
            const auto clone = b.CloneBlock(0);
            const auto copied = static_cast<std::size_t>(clone["block"].as_uint());
            b.SetField(copied, "name", "copy");
            b.SetField(copied, "value", 99);
            const auto raw = b.NewData(Monstey_Bytes({1, 2, 3}));
            const auto data = static_cast<std::size_t>(raw["block"].as_uint());
            b.ReplaceData(data, Monstey_Bytes({9, 8, 7, 6}));
            b.SetField(copied, "next", raw["address"]);
            const auto output = dir / L"raw-blocks.blend";
            if (std::filesystem::exists(output))
                std::filesystem::remove(output);
            b.Write(output);
            Monstey_Blend check(output);
            Monstey_Require(check.ReadField(0, "value").as_int() == 7 &&
                                check.ReadField(copied, "value").as_int() == 99,
                            "Cloned typed record aliases original");
            Monstey_Require(check.BlockAtAddress(raw["address"].as_uint()) == data &&
                                check.BlockData(data) == Monstey_Bytes({9, 8, 7, 6}),
                            "Raw attribute buffer lookup changed");
        });
        test("signed_field_and_pointer_ranges_rejected_atomically", [&] {
            Monstey_Blend b(path);
            reject([&] { b.SetField(0, "value", 2147483648ull); });
            Monstey_Require(b.ReadField(0, "value").as_int() == 7, "Signed overflow changed field");
            reject([&] { b.SetField(0, "next", -1); });
            Monstey_Require(b.ReadField(0, "next")["pointer"].as_uint() == 0,
                            "Negative pointer changed field");
        });
        test("legacy_gzip_body_and_crc_checked", [&] {
            const auto file = dir / L"gzip.blend";
            Monstey_TestWrite(file, Monstey_GzipFixture(fixture));
            Monstey_Blend b(file);
            const auto description = b.Describe();
            Monstey_Require(description["gzip_compressed"].as_bool() &&
                                !description["zstd_compressed"].as_bool() &&
                                b.ReadField(0, "name").as_string() == "source",
                            "Native gzip blend decoding failed");
            auto bad = Monstey_GzipFixture(fixture);
            bad[bad.size() - 8] ^= 1;
            const auto corrupt = dir / L"bad-gzip.blend";
            Monstey_TestWrite(corrupt, bad);
            reject([&] { Monstey_Blend broken(corrupt); });
        });
        test("legacy_gzip_multiple_members_explicitly_rejected", [&] {
            auto data = Monstey_GzipFixture(fixture);
            const auto second = data;
            data.insert(data.end(), second.begin(), second.end());
            const auto file = dir / L"concatenated-gzip.blend";
            Monstey_TestWrite(file, data);
            reject([&] { Monstey_Blend b(file); });
        });
        test("data_pointer_namespaces_are_owned_by_id", [&]{
          const auto file=dir/L"scoped.blend";Monstey_TestWrite(file,Monstey_BlendFixture(1,8,true,true));Monstey_Blend b(file);
          Monstey_Require(b.ReadField(b.BlockAtAddress(0x30000,0),"value").as_int()==7&&b.ReadField(b.BlockAtAddress(0x30000,2),"value").as_int()==9,"Per-ID pointer namespace resolution differs");
          reject([&]{b.BlockAtAddress(0x30000);});
        });
        test("duplicate_data_pointer_inside_one_id_is_rejected", [&]{
          auto bytes=Monstey_BlendFixture(1,8,true,true);const auto secondId=17+2*(32+32);for(unsigned k=0;k<4;++k)bytes[secondId+k]=static_cast<std::uint8_t>("DATA"[k]);const auto file=dir/L"bad-scoped.blend";Monstey_TestWrite(file,bytes);reject([&]{Monstey_Blend b(file);});
        });
        test("authored_data_is_serialized_under_reachable_id", [&]{
          const auto file=dir/L"owner-source.blend";Monstey_TestWrite(file,Monstey_BlendFixture(1,8,true,true));Monstey_Blend b(file);auto data=b.NewBlock("DATA","Thing");const auto i=static_cast<std::size_t>(data["block"].as_uint());b.SetField(i,"value",41);b.SetDataOwner(i,0);const auto out=dir/L"owner-written.blend";if(std::filesystem::exists(out))std::filesystem::remove(out);b.Write(out);Monstey_Blend check(out);Monstey_Require(check.ReadField(check.BlockAtAddress(data["address"].as_uint(),0),"value").as_int()==41,"Authored DATA lost its ID scope");
        });
        test("typed_bulk_write_checks_extent_and_preserves_fields", [&]{
          Monstey_Blend b(path);auto block=b.NewBlock("DATA","Thing",2);const auto i=static_cast<std::size_t>(block["block"].as_uint());auto bytes=b.BlockData(i);Monstey_PutU32(bytes,8,123);b.ReplaceTypedData(i,bytes);Monstey_Require(b.ReadField(i,"value").as_int()==123,"Typed bulk write differs");bytes.push_back(0);reject([&]{b.ReplaceTypedData(i,bytes);});
        });
        if (argc > 3)
            test("independent_dotnet_gzip_fixture", [&] {
                Monstey_Blend b(argv[3]);
                Monstey_Require(b.Describe()["gzip_compressed"].as_bool() &&
                                    b.Describe()["decoded_sha256"].as_string() == Monstey_HashSha256(fixture),
                                "Independent .NET gzip fixture differs");
                Monstey_Require(b.ReadField(0, "point[1]").as_number() == 2,
                                "Independent gzip typed data changed");
            });
        if (argc > 2) {
            const std::filesystem::path real = argv[2];
            test("stock_blender_schema_and_complete_block_walk", [&] {
                Monstey_Blend b(real);
                report["stock"] = b.Describe();
                report["mesh_schema"] = b.Fields("Mesh");
                report["object_schema"] = b.Fields("Object");
                report["armature_schema"] = b.Fields("bArmature");
                report["bone_schema"] = b.Fields("Bone");
                report["stock_objects"] = b.Blocks("OB");
                report["stock_meshes"] = b.Blocks("ME");
                report["additional_schemas"] = Monstey_Json::object();
                for (const char *name :
                     {"AttributeStorage", "Attribute", "AttributeData", "CustomData", "CustomDataLayer",
                      "MDeformVert", "MDeformWeight", "bDeformGroup", "ArmatureModifierData", "bPose",
                      "bPoseChannel", "Collection", "CollectionObject", "LayerCollection", "Scene",
                      "ViewLayer", "FileGlobal", "ListBase"}) {
                    try {
                        report["additional_schemas"][name] = b.Fields(name);
                    } catch (const std::exception &e) {
                        report["additional_schemas"][name] =
                            Monstey_Json::object({{"unavailable", e.what()}});
                    }
                }
                report["stock_mesh_values"] = Monstey_Json::object();
                if (!report["stock_meshes"]["rows"].empty()) {
                    const auto index =
                        static_cast<std::size_t>(report["stock_meshes"]["rows"][0u]["index"].as_uint());
                    for (const char *field :
                         {"id.name", "verts_num", "totvert", "edges_num", "faces_num", "corners_num",
                          "face_offset_indices", "attribute_storage.attributes_num",
                          "attribute_storage.attributes_data", "vdata.totlayer", "vdata.layers",
                          "ldata.totlayer", "ldata.layers"}) {
                        try {
                            report["stock_mesh_values"][field] = b.ReadField(index, field);
                        } catch (const std::exception &e) {
                            report["stock_mesh_values"][field] =
                                Monstey_Json::object({{"unavailable", e.what()}});
                        }
                    }
                }
                Monstey_Require(report["stock"]["structs"].as_uint() > 500, "Incomplete stock SDNA");
            });
            test("stock_blender_plain_noop_candidate", [&] {
                Monstey_Blend b(real);
                const auto out = dir / L"stock-native-roundtrip.blend";
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                report["stock_write"] = b.Write(out);
                Monstey_Require(report["stock_write"]["byte_exact_decoded"].as_bool(),
                                "Stock decoded blocks changed");
            });
        }
        if (argc > 2) {
            const std::filesystem::path real = argv[2];
            test("stock_blender_typed_scene_edit_candidate", [&] {
                Monstey_Blend b(real);
                const auto objects = b.Blocks("OB");
                bool changed = false;
                for (const auto &row : objects["rows"].as_array()) {
                    const auto index = static_cast<std::size_t>(row["index"].as_uint());
                    if (b.ReadField(index, "type").as_int() == 1) {
                        report["edited_object_before"] = b.ReadField(index, "id.name");
                        b.SetField(index, "loc", Monstey_Json::array({1.0, 0.0, 0.0}));
                        b.SetField(index, "id.name", "OBMonstey_Native_Probe");
                        report["edited_object_block"] = static_cast<std::uint64_t>(index);
                        changed = true;
                        break;
                    }
                }
                Monstey_Require(changed, "Stock fixture has no mesh object");
                const auto output = dir / L"stock-native-edited.blend";
                if (std::filesystem::exists(output))
                    std::filesystem::remove(output);
                report["stock_edit"] = b.Write(output);
                Monstey_Blend check(output);
                const auto index = static_cast<std::size_t>(report["edited_object_block"].as_uint());
                Monstey_Require(check.ReadField(index, "id.name").as_string() == "OBMonstey_Native_Probe" &&
                                    check.ReadField(index, "loc")[0u].as_number() == 1,
                                "Stock typed edit changed in readback");
            });
        }
        report["status"] = "PASS";
        report["passed"] = passed;
        const auto out = dir / L"blend-tests.json";
        if (std::filesystem::exists(out))
            std::filesystem::remove(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native Blend tests: " << passed << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Native Blend failure: " << e.what() << "\n";
        return 1;
    }
}
