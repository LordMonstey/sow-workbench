#include "Monstey_AssetInspect.hpp"
#include "Monstey_Runtime.hpp"
#include "Monstey_TextureApi.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
namespace {
using namespace Monstey;
using J = Monstey_Json;
using B = Monstey_Bytes;
void Check(bool v) {
  if (!v)
    throw std::runtime_error("Texture API assertion failed");
}
template <class F> void Reject(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  Check(failed);
}
B Fixture(bool volume = false) {
  B b(148 + (volume ? 32 : 16));
  Monstey_PutU32(b, 0, 0x20534444);
  Monstey_PutU32(b, 4, 124);
  Monstey_PutU32(b, 8, 0x1007);
  Monstey_PutU32(b, 12, 2);
  Monstey_PutU32(b, 16, 2);
  Monstey_PutU32(b, 24, volume ? 2 : 1);
  Monstey_PutU32(b, 28, 1);
  Monstey_PutU32(b, 76, 32);
  Monstey_PutU32(b, 80, 4);
  Monstey_PutU32(b, 84, 0x30315844);
  Monstey_PutU32(b, 108, 0x1000);
  Monstey_PutU32(b, 128, 87);
  Monstey_PutU32(b, 132, volume ? 4 : 3);
  Monstey_PutU32(b, 140, 1);
  for (std::size_t i = 148; i < b.size(); ++i)
    b[i] = static_cast<std::uint8_t>(i * 13);
  return b;
}
std::string Path(const std::filesystem::path &p) {
  return Monstey_Utf8(p.wstring());
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto parent = argc > 1 ? std::filesystem::path(argv[1])
                                 : std::filesystem::temp_directory_path();
    const auto dir =
        parent /
        (L"texture-api-" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    auto names = J::array();
    const auto run = [&](const char *name, const auto &f) {
      f();
      names.as_array().emplace_back(name);
    };
    const auto source = dir / L"texture_\u00e9_\u6c34.dds",
               volume = dir / L"volume.dds";
    const auto data = Fixture(), vol = Fixture(true);
    Monstey_WriteNewFile(source, data);
    Monstey_WriteNewFile(volume, vol);
    const auto hash = Monstey_HashSha256(source);
    auto args =
        J::object({{"source", Path(source)}, {"expected_source_sha256", hash}});
    run("Four canonical texture descriptors", [&] {
      const auto c = Monstey_TextureCatalog();
      Check(c.size() == 4);
      for (const auto &r : c.as_array())
        Check(r.at("inputSchema").at("additionalProperties") == J(false));
    });
    run("Source-qualified complete layout", [&] {
      const auto r = Monstey_TextureCall("texture_inspect", args);
      Check(r.at("source_sha256") == J(hash) &&
            r.at("decode_supported") == J(true) &&
            r.at("subresources").size() == 1 &&
            r.at("subresources")[0].at("bytes") == J(16));
    });
    run("Generic asset inspection uses same layout", [&] {
      const auto a = Monstey_AssetInspect(source);
      Check(a.at("format") == J("DDS") &&
            a.at("native_layout").at("source_sha256") == J(hash) &&
            a.at("native_layout").at("decode_supported") == J(true));
    });
    run("Encoded extraction is exact", [&] {
      auto a = args;
      a["output"] = Path(dir / L"raw.bin");
      const auto r = Monstey_TextureCall("texture_extract", a);
      Check(Monstey_ReadFile(dir / L"raw.bin") ==
                B(data.begin() + 148, data.end()) &&
            r.at("output_bytes") == J(16));
    });
    run("Native roundtrip preserves every byte", [&] {
      auto a = args;
      a["output"] = Path(dir / L"copy.dds");
      const auto r = Monstey_TextureCall("texture_roundtrip", a);
      Check(r.at("no_op_byte_exact") == J(true) &&
            Monstey_ReadFile(dir / L"copy.dds") == data);
    });
    run("Unicode PNG output has WIC pixel-verified encoding", [&] {
      auto a = args;
      a["output"] = Path(dir / L"preview_\u00e9_\u6c34.png");
      const auto r = Monstey_TextureCall("texture_preview_render", a);
      const auto bytes = Monstey_ReadFile(dir / L"preview_\u00e9_\u6c34.png");
      Check(bytes.size() > 20 && Monstey_U32(bytes, 0) == 0x474e5089 &&
            r.at("width") == J(2) && r.at("height") == J(2) &&
            r.at("output_sha256") == J(Monstey_HashSha256(bytes)) &&
            r.at("native_rendering_verified") == J(false));
    });
    run("Volume depth selection changes decoded output", [&] {
      auto a = J::object(
          {{"source", Path(volume)}, {"output", Path(dir / L"depth0.png")}});
      const auto x = Monstey_TextureCall("texture_preview_render", a);
      a["output"] = Path(dir / L"depth1.png");
      a["depth_slice"] = 1;
      const auto y = Monstey_TextureCall("texture_preview_render", a);
      Check(x.at("output_sha256") != y.at("output_sha256") &&
            y.at("depth_slice") == J(1));
    });
    run("Wrong hash and empty explicit hash reject", [&] {
      for (const auto &v : {std::string(64, '0'), std::string()}) {
        auto a = args;
        a["expected_source_sha256"] = v;
        Reject([&] { (void)Monstey_TextureCall("texture_inspect", a); });
      }
    });
    run("Unknown arguments and operation reject", [&] {
      auto a = args;
      a["fake"] = true;
      Reject([&] { (void)Monstey_TextureCall("texture_inspect", a); });
      Reject([&] { (void)Monstey_TextureCall("texture_fake", args); });
    });
    run("Source and existing outputs cannot be replaced", [&] {
      auto a = args;
      a["output"] = Path(source);
      Reject([&] { (void)Monstey_TextureCall("texture_roundtrip", a); });
      a["output"] = Path(dir / L"raw.bin");
      Reject([&] { (void)Monstey_TextureCall("texture_extract", a); });
      Check(Monstey_ReadFile(source) == data);
    });
    run("Bounds and wrong integer types reject without output", [&] {
      for (const auto &[key, value] : std::array<std::pair<const char *, J>, 7>{
               {{"subresource", -1},
                {"subresource", 65536},
                {"subresource", true},
                {"depth_slice", 2},
                {"maximum_pixels", 0},
                {"maximum_pixels", 3},
                {"maximum_pixels", 16777217}}}) {
        auto a = args;
        a["output"] = Path(dir / L"invalid.png");
        a[key] = value;
        Reject([&] { (void)Monstey_TextureCall("texture_preview_render", a); });
        Check(!std::filesystem::exists(dir / L"invalid.png"));
      }
    });
    run("Opaque source remains honest and raw roundtrips", [&] {
      const auto p = dir / L"opaque.tex";
      Monstey_WriteNewFile(p, B{1, 2, 3, 4});
      const auto r = Monstey_TextureCall("texture_inspect",
                                         J::object({{"source", Path(p)}}));
      Check(r.at("layout_qualified") == J(false) &&
            r.at("decode_supported") == J(false));
    });
    run("Runtime shared transport routes and revision semantics", [&] {
      Monstey_Runtime runtime({}, dir / L"settings.json");
      const auto before = runtime.Call(
          J::object({{"operation", "texture_inspect"}, {"arguments", args}}));
      Check(before.at("ok") == J(true) && before.at("revision") == J(0));
      auto a = args;
      a["output"] = Path(dir / L"runtime.png");
      const auto after = runtime.Call(J::object(
          {{"operation", "texture_preview_render"}, {"arguments", a}}));
      Check(after.at("ok") == J(true) && after.at("revision") == J(1));
      a["output"] = Path(dir / L"stale.png");
      a["expected_revision"] = 0;
      const auto stale = runtime.Call(J::object(
          {{"operation", "texture_preview_render"}, {"arguments", a}}));
      Check(stale.at("ok") == J(false) &&
            stale.at("error").at("code") == J("stale_revision") &&
            !std::filesystem::exists(dir / L"stale.png"));
    });
    run("All original source bytes remain unchanged", [&] {
      Check(Monstey_ReadFile(source) == data &&
            Monstey_ReadFile(volume) == vol &&
            Monstey_HashSha256(source) == hash);
    });
    const auto r = J::object({{"status", "PASS"},
                              {"groups", names.size()},
                              {"tests", names},
                              {"synthetic_only", true}});
    Monstey_WriteNewText(dir / L"report.json", r.dump(2));
    std::cout << r.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
