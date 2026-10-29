#include "Monstey_AssetInspect.hpp"
#include "Monstey_Catalog.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldComposition.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Check(bool v, const char *text) {
  if (!v)
    throw std::runtime_error(text);
}
template <class F> void Reject(F &&call) {
  bool rejected = false;
  try {
    call();
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Expected format rejection");
}
void F(B &b, float v) { Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(v)); }
B S(std::string value) {
  B b(value.begin(), value.end());
  b.push_back(0);
  return b;
}
B Material() {
  struct Field {
    std::string name;
    std::uint32_t kind;
    B data;
  };
  B v3;
  for (auto f : {1.25f, -2.f, 3.f})
    F(v3, f);
  B v4;
  for (auto f : {1.f, 2.f, 3.f, 4.f})
    F(v4, f);
  B u;
  Monstey_AppendU32(u, 0xff224466);
  B scalar;
  Monstey_AppendU32(scalar, 0x7fc12345);
  B v2;
  F(v2, .5f);
  F(v2, 1.f);
  const std::vector<Field> fields{
      {"Vector", 2, v3},    {"Vector4", 3, v4},
      {"Color", 4, u},      {"Scalar", 5, scalar},
      {"Enabled", 6, B{1,0,0,0}}, {"Texture", 8, S("textures/test.tex")},
      {"UV", 11, v2},       {"External", 65537, S("fx/test")},
      {"Opaque", 999, B{5}}};
  const auto shader = S("fulldeferred_rigid");
  B bytes(30);
  Monstey_PutU32(bytes, 0, 0x494d544c);
  Monstey_PutU32(bytes, 4, 7);
  Monstey_PutU32(bytes, 8, 0x12345678);
  Monstey_PutU16(bytes, 16, 5);
  Monstey_PutU16(bytes, 18, 4);
  Monstey_PutU16(bytes, 28, static_cast<std::uint16_t>(shader.size()));
  bytes.insert(bytes.end(), shader.begin(), shader.end());
  B values;
  std::vector<std::uint32_t> offsets;
  for (const auto &field : fields) {
    offsets.push_back(static_cast<std::uint32_t>(values.size()));
    values.insert(values.end(), field.data.begin(), field.data.end());
  }
  Monstey_PutU32(bytes, 12, static_cast<std::uint32_t>(values.size()));
  bytes.insert(bytes.end(), values.begin(), values.end());
  Monstey_AppendU32(bytes, 3);
  bytes.insert(bytes.end(), {0x91,0x92,0x93});
  for (std::size_t i = 0; i < fields.size(); ++i) {
    Monstey_AppendU32(bytes, fields[i].kind);
    const auto name = S(fields[i].name);
    Monstey_AppendU16(bytes, static_cast<std::uint16_t>(name.size()));
    bytes.insert(bytes.end(), name.begin(), name.end());
    Monstey_AppendU32(bytes, offsets[i]);
    Monstey_AppendU16(bytes, 0x1234);
  }
  return bytes;
}
B Texture() {
  const auto peer = S("texture_peer.tex");
  const std::size_t meta = 18 + peer.size(), dds = meta + 23;
  std::size_t payload = 0;
  for (unsigned m = 0; m < 8; ++m)
    payload += ((std::max(1U,512U >> m)+3)/4) *
               ((std::max(1U,256U >> m)+3)/4) * 16;
  B bytes(dds + 128 + payload);
  Monstey_PutU32(bytes,0,0x52584554);
  Monstey_PutU32(bytes,4,8);
  Monstey_PutU16(bytes,16,static_cast<std::uint16_t>(peer.size()));
  std::copy(peer.begin(),peer.end(),bytes.begin()+18);
  bytes[meta]=3;
  Monstey_PutU16(bytes,meta+1,1024);
  Monstey_PutU16(bytes,meta+3,512);
  Monstey_PutU16(bytes,meta+5,1);
  bytes[meta+7]=9; // DDS starts at logical mip 1 and stores 8 levels.
  bytes[meta+8]=1;
  Monstey_PutU32(bytes,meta+9,77);
  Monstey_PutU32(bytes,dds,0x20534444);
  Monstey_PutU32(bytes,dds+4,124);
  Monstey_PutU32(bytes,dds+8,0x21007);
  Monstey_PutU32(bytes,dds+12,256);
  Monstey_PutU32(bytes,dds+16,512);
  Monstey_PutU32(bytes,dds+28,8);
  Monstey_PutU32(bytes,dds+76,32);
  Monstey_PutU32(bytes,dds+80,4);
  Monstey_PutU32(bytes,dds+84,0x35545844);
  Monstey_PutU32(bytes,dds+108,0x401008);
  for(std::size_t i=dds+128;i<bytes.size();++i)
    bytes[i]=static_cast<std::uint8_t>(i);
  return bytes;
}

} // namespace
int main(int argc, char **argv) {
  try {
    const auto dir =
        (argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                  : std::filesystem::temp_directory_path()) /
        ("inspect-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    unsigned passed = 0;
    const auto test = [&](const char *name, auto &&f) {
      f();
      ++passed;
      std::cout << "PASS " << name << '\n';
    };
    const auto material = Material(), texture = Texture();
    test("original LTMI fixture decodes every known value kind without losing "
         "opaque fields",
         [&] {
           const auto r = Monstey_MaterialInspect(material);
           Check(r.at("version").as_uint() == 7 &&
                     r.at("shader_id").as_uint() == 0x12345678 &&
                     r.at("shader").as_string() == "fulldeferred_rigid",
                 "Material header differs");
           const auto p = r.at("properties");
           Check(p.at("Vector") == J::array({1.25, -2.0, 3.0}) &&
                     p.at("Vector4") == J::array({1.0, 2.0, 3.0, 4.0}),
                 "Material vectors differ");
           Check(p.at("Color").as_uint() == 0xff224466 &&
                     p.at("Enabled").as_bool(),
                 "Material color/bool differ");
           Check(p.at("Scalar").at("float32_bits").as_uint() == 0x7fc12345,
                 "NaN payload lost");
           Check(p.at("Texture").as_string() == "textures/test.tex" &&
                     p.at("UV") == J::array({.5, 1.0}),
                 "Material string/UV differ");
           Check(p.at("External").as_string() == "fx/test" &&
                     p.at("Opaque").at("unsupported_type").as_uint() == 999,
                 "Opaque/native reference lost");
           Check(r.at("entries").size() == 9 &&
                     r.at("trailing_bytes").as_uint() == 0,
                 "Material fields dropped");
         });
    test("LTMI bounds and unterminated typed values reject", [&] {
      auto bad = material;
      bad.resize(29);
      Reject([&] { Monstey_MaterialInspect(bad); });
      bad = material;
      Monstey_PutU32(bad, 12, 0xffffffff);
      Reject([&] { Monstey_MaterialInspect(bad); });
      bad = material;
      Monstey_PutU16(bad, 28, 65535);
      Reject([&] { Monstey_MaterialInspect(bad); });
      bad = material;
      const auto entries = Monstey_MaterialInspect(bad).at("entries");
      const auto valueEnd = 30 + Monstey_U16(bad,28) + Monstey_U32(bad,12);
      std::size_t row = valueEnd + 4 + 3;
      for (unsigned i=0;i<7;++i) row += 12 + Monstey_U16(bad,row+4);
      (void)entries;
      Monstey_PutU32(bad,row + 6 + Monstey_U16(bad,row+4),0xffff);
      Reject([&] { Monstey_MaterialInspect(bad); });
    });
    test("TEXR envelope retains logical and DDS dimensions and payload hash",
         [&] {
           const auto r = Monstey_TextureInspect(texture);
           Check(r.at("logical_dimensions") == J::array({1024, 512}) &&
                     r.at("dds_dimensions") == J::array({512, 256}),
                 "Texture dimensions differ");
           Check(r.at("dds_mips").as_uint() == 8 &&
                     r.at("fourcc").as_string() == "DXT5",
                 "Texture mip/compression metadata differs");
           const auto at = r.at("dds_offset").as_uint();
           Check(r.at("dds_sha256").as_string() ==
                     Monstey_HashSha256(std::span(texture).subspan(
                         static_cast<std::size_t>(at))),
                 "DDS payload hash differs");
         });
    test("TEXR envelope offsets and truncation reject", [&] {
      auto bad = texture;
      Monstey_PutU16(bad, 16, 65535);
      bad.resize(64); // The declared peer must actually exceed the available bytes.
      Reject([&] { Monstey_TextureInspect(bad); });
      bad = texture;
      bad.resize(40);
      Reject([&] { Monstey_TextureInspect(bad); });
      bad = texture;
      Monstey_PutU32(
          bad,
          static_cast<std::size_t>(
              Monstey_TextureInspect(texture).at("dds_offset").as_uint()),
          0);
      Reject([&] { Monstey_TextureInspect(bad); });
    });
    test("asset API inspects LTMI and TEXR without changing sources", [&] {
      const auto m = dir / L"material.mat", t = dir / L"texture.tex";
      Monstey_WriteNewFile(m, material);
      Monstey_WriteNewFile(t, texture);
      const auto mr = Monstey_AssetInspect(m), tr = Monstey_AssetInspect(t);
      Check(mr.at("format").as_string() == "LTMI" &&
                tr.at("format").as_string() == "TEXR",
            "Asset format dispatch differs");
      Check(Monstey_ReadFile(m) == material && Monstey_ReadFile(t) == texture,
            "Asset inspection modified source");
      Check(
          Monstey_WorldCall("sow_asset_inspect",
                            J::object({{"source", Monstey_Utf8(m.wstring())}}))
                  .at("material").at("shader_id") == mr.at("material").at("shader_id"),
          "Public world asset inspect route differs");
    });
    test("BNDL and EMBB inspection shares exact native readers", [&] {
      const auto b = Monstey_BuildBundle({"T", {{"a.mesh", 7}, {"b.mat", 11}}});
      const auto e =
          Monstey_BuildEmbedded({{"a.mesh", B{1, 2, 3}},
                                 {"large.bin", B(2 * 1024 * 1024 + 7, 0x42)}});
      const auto bp = dir / L"bundle.bndl", ep = dir / L"bundle.embb";
      Monstey_WriteNewFile(bp, b);
      Monstey_WriteNewFile(ep, e);
      const auto br = Monstey_AssetInspect(bp), er = Monstey_AssetInspect(ep);
      Check(br.at("resources").size() == 2 &&
                br.at("resources")[1].at("size_hint").as_uint() == 11,
            "Bundle dependencies differ");
      Check(er.at("resources").size() == 2 &&
                !er.at("parent_materialized").as_bool(),
            "Embedded parent materialized");
      Check(er.at("resources")[0].at("sha256").as_string() ==
                    Monstey_HashSha256(B{1, 2, 3}) &&
                er.at("sha256").as_string() == Monstey_HashSha256(e),
            "Embedded hash differs");
    });
    test("unsupported full archive and truncated input reject", [&] {
      const auto p = dir / L"invalid.bin";
      Monstey_WriteNewFile(p, B{'L', 'T', 'A', 'R'});
      Reject([&] { Monstey_AssetInspect(p); });
      const auto q = dir / L"short.bin";
      Monstey_WriteNewFile(q, B{1, 2, 3});
      Reject([&] { Monstey_AssetInspect(q); });
    });
    if (argc > 2) {
      const auto modelDir = std::filesystem::path(Monstey_Wide(argv[2]));
      test("native model fixture meshes skeleton and GLB cover remaining "
           "formats",
           [&] {
             const auto m = Monstey_AssetInspect(modelDir / L"synthetic.mesh");
             const auto s = Monstey_AssetInspect(modelDir / L"synthetic.skel");
             const auto g = Monstey_AssetInspect(modelDir / L"export.glb");
             Check(m.at("format").as_string() == "MMSH17" &&
                       m.at("native_allocations_valid").as_bool() &&
                       m.at("parts")[0].contains("bounds_cm"),
                   "Mesh inspector contract incomplete");
             Check(s.at("format").as_string() == "SKEL17" &&
                       s.at("bone_names").size() > 0,
                   "Standalone skeleton names missing");
             Check(g.at("format").as_string() == "GLB2" &&
                       g.contains("primitive_attributes") &&
                       g.contains("materials"),
                   "GLB inspection fields missing");
           });
    }
    if (argc > 3) {
      const auto nativeRoot = std::filesystem::path(Monstey_Wide(argv[3]));
      const auto armor =
          nativeRoot / L"extracted/resources/characters/modelcharacters/player/"
                       L"armor/blacknum_armor";
      test("actual native material and texture are decoded read-only", [&] {
        const auto m = armor / L"player_blacknumt1_armor.mat",
                   t = armor / L"player_blacknumt1_armor_d.tex";
        const auto mh = Monstey_HashSha256(m), th = Monstey_HashSha256(t);
        const auto mr = Monstey_AssetInspect(m), tr = Monstey_AssetInspect(t);
        Check(!mr.at("material").at("properties").empty() &&
                  tr.at("dds_dimensions")[0].as_uint() > 0,
              "Native material/texture empty");
        Check(Monstey_HashSha256(m) == mh && Monstey_HashSha256(t) == th,
              "Native material/texture changed");
        Monstey_WriteNewText(dir / L"native-material.json", mr.dump(2));
        Monstey_WriteNewText(dir / L"native-texture.json", tr.dump(2));
      });
    }
    std::cout << passed << " native asset inspection groups passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}