#include "Monstey_TextureResource.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
void Check(bool ok, const char *m) {
  if (!ok)
    throw Monstey_Error("test", m);
}
template <class F> void Reject(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Expected rejection");
}
void Near(float a, float b) {
  if (!(std::abs(a - b) < 0.00001f))
    throw Monstey_Error(
        "test", "Decoded component differs: actual=" + std::to_string(a) +
                    " expected=" + std::to_string(b));
}
B Dds(std::uint32_t format, std::uint32_t width = 4, std::uint32_t height = 4,
      std::uint32_t depth = 1, std::uint32_t mips = 1,
      std::uint32_t dimension = 3, std::uint32_t layers = 1,
      std::uint32_t misc = 0) {
  B b(148);
  Monstey_PutU32(b, 0, 0x20534444);
  Monstey_PutU32(b, 4, 124);
  Monstey_PutU32(b, 8, 0x1007);
  Monstey_PutU32(b, 12, height);
  Monstey_PutU32(b, 16, width);
  Monstey_PutU32(b, 24, depth);
  Monstey_PutU32(b, 28, mips);
  Monstey_PutU32(b, 76, 32);
  Monstey_PutU32(b, 80, 4);
  Monstey_PutU32(b, 84, 0x30315844);
  Monstey_PutU32(b, 108, 0x1000);
  Monstey_PutU32(b, 128, format);
  Monstey_PutU32(b, 132, dimension);
  Monstey_PutU32(b, 136, misc);
  Monstey_PutU32(b, 140, layers);
  const auto blocks =
      (format >= 70 && format <= 84) || (format >= 94 && format <= 99);
  const auto unit =
      (format == 71 || format == 72 || format == 80 || format == 81) ? 8U : 16U;
  const auto pixelBytes = format == 61 ? 1U : format == 49 ? 2U : 4U;
  const auto faces = (misc & 4) ? 6U : 1U;
  for (std::uint32_t a = 0; a < layers * faces; ++a)
    for (std::uint32_t m = 0; m < mips; ++m) {
      const auto w = std::max(1U, width >> m), h = std::max(1U, height >> m),
                 z = dimension == 4 ? std::max(1U, depth >> m) : 1;
      const auto n = blocks ? ((w + 3U) / 4) * ((h + 3U) / 4) * unit * z
                            : w * h * pixelBytes * z;
      b.resize(b.size() + n, 0);
    }
  return b;
}
B Legacy(B b, std::uint32_t fourcc, bool bgra = false) {
  b.erase(b.begin() + 128, b.begin() + 148);
  Monstey_PutU32(b, 84, fourcc);
  if (bgra) {
    Monstey_PutU32(b, 80, 0x41);
    Monstey_PutU32(b, 88, 32);
    Monstey_PutU32(b, 92, 0xff0000);
    Monstey_PutU32(b, 96, 0xff00);
    Monstey_PutU32(b, 100, 0xff);
    Monstey_PutU32(b, 104, 0xff000000);
  }
  return b;
}
B Native(B dds, std::uint8_t kind = 3, std::uint16_t w = 4, std::uint16_t h = 4,
         std::uint16_t depthOrLayers = 1, std::uint8_t mips = 1,
         std::uint32_t format = 71) {
  B b(18);
  Monstey_PutU32(b, 0, kind == 4 ? 0x41584554 : 0x52584554);
  Monstey_PutU32(b, 4, kind == 4 ? 1 : 8);
  Monstey_PutU64(b, 8, 0xdeadbeeffeaa0102);
  const auto meta = b.size();
  b.resize(meta + (kind == 5 ? 22 : 23), 0xa7);
  b[meta] = kind;
  Monstey_PutU16(b, meta + 1, w);
  Monstey_PutU16(b, meta + 3, h);
  Monstey_PutU16(b, meta + 5, depthOrLayers);
  b[meta + 7] = mips;
  Monstey_PutU32(b, meta + (kind == 5 ? 8 : 9), format);
  if (kind == 4) {
    for (unsigned i = 0; i < depthOrLayers; ++i) {
      Monstey_AppendU16(b, 2);
      b.push_back(0xff);
      b.push_back(static_cast<std::uint8_t>(i));
    }
  }
  b.insert(b.end(), dds.begin(), dds.end());
  return b;
}
void Set(B &b, std::size_t at, const B &v) {
  std::copy(v.begin(), v.end(), b.begin() + static_cast<std::ptrdiff_t>(at));
}
B Red1() { return {0, 0xf8, 0, 0, 0, 0, 0, 0}; }
B Red7() {
  B b(16, 0);
  unsigned n = 0;
  const auto bits = [&](std::uint32_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i, ++n)
      if ((value >> i) & 1)
        b[n / 8] |= static_cast<std::uint8_t>(1U << (n % 8));
  };
  bits(64, 7);
  for (unsigned c = 0; c < 4; ++c) {
    bits(c == 0 || c == 3 ? 127 : 0, 7);
    bits(c == 0 || c == 3 ? 127 : 0, 7);
  }
  bits(1, 1);
  bits(1, 1);
  bits(0, 3);
  for (unsigned i = 1; i < 16; ++i)
    bits(0, 4);
  Check(n == 128, "BC7 fixture size");
  return b;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2, "Need fresh output root");
    const std::filesystem::path out = argv[1];
    Check(!std::filesystem::exists(out), "Output root already exists");
    std::filesystem::create_directories(out);
    auto results = J::array();
    const auto run = [&](const char *name, auto test) {
      test();
      results.as_array().push_back(name);
      std::cout << "PASS " << name << '\n';
    };
    run("BC1 red", [] {
      auto b = Dds(71);
      Set(b, 148, Red1());
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], 1);
      Near(q.rgba[15][1], 0);
      Near(q.rgba[15][3], 1);
    });
    run("BC2 alpha", [] {
      auto b = Dds(74);
      std::fill_n(b.begin() + 148, 8, std::uint8_t{0xff});
      Set(b, 156, Red1());
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[3][0], 1);
      Near(q.rgba[3][3], 1);
    });
    run("BC3 red", [] {
      auto b = Dds(77);
      b[148] = 255;
      b[149] = 255;
      Set(b, 156, Red1());
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[3][0], 1);
      Near(q.rgba[3][3], 1);
    });
    run("BC4 UNORM", [] {
      auto b = Dds(80);
      b[148] = 128;
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], 128 / 255.0f);
    });
    run("BC4 SNORM", [] {
      auto b = Dds(81);
      b[148] = 0x81;
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], -1);
    });
    run("BC5 UNORM", [] {
      auto b = Dds(83);
      b[148] = 128;
      b[156] = 64;
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], 128 / 255.0f);
      Near(q.rgba[0][1], 64 / 255.0f);
    });
    run("BC5 SNORM", [] {
      auto b = Dds(84);
      b[148] = 0x81;
      b[156] = 127;
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], -1);
      Near(q.rgba[0][1], 1);
    });
    run("BC6H unsigned", [] {
      auto q = Monstey_TextureResource(Dds(95)).Decode(0);
      for (auto p : q.rgba) {
        Near(p[0], 0);
        Near(p[1], 0);
        Near(p[2], 0);
        Near(p[3], 1);
      }
    });
    run("BC6H signed", [] {
      auto q = Monstey_TextureResource(Dds(96)).Decode(0);
      Near(q.rgba[0][0], 0);
      Near(q.rgba[0][3], 1);
    });
    run("BC7 valid mode 6 red", [] {
      auto b = Dds(98);
      Set(b, 148, Red7());
      auto q = Monstey_TextureResource(b).Decode(0);
      for (auto p : q.rgba) {
        Near(p[0], 1);
        Near(p[1], 1 / 255.0f);
        Near(p[2], 1 / 255.0f);
        Near(p[3], 1);
      }
    });
    run("BC7 invalid block deterministic", [] {
      auto q = Monstey_TextureResource(Dds(98)).Decode(0);
      for (auto p : q.rgba)
        for (auto v : p)
          Near(v, 0);
    });
    run("Block crop 5 by 3", [] {
      auto b = Dds(71, 5, 3);
      Set(b, 148, Red1());
      Set(b, 156, Red1());
      auto q = Monstey_TextureResource(b).Decode(0);
      Check(q.rgba.size() == 15, "Crop count");
      Near(q.rgba.back()[0], 1);
    });
    run("BGRA channel order", [] {
      auto b = Dds(87, 1, 1);
      Set(b, 148, {10, 20, 30, 40});
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], 30 / 255.0f);
      Near(q.rgba[0][2], 10 / 255.0f);
      Near(q.rgba[0][3], 40 / 255.0f);
    });
    run("RGBA channel order", [] {
      auto b = Dds(28, 1, 1);
      Set(b, 148, {10, 20, 30, 40});
      auto q = Monstey_TextureResource(b).Decode(0);
      Near(q.rgba[0][0], 10 / 255.0f);
      Near(q.rgba[0][2], 30 / 255.0f);
    });
    run("BGRX opaque alpha", [] {
      auto b = Dds(88, 1, 1);
      Set(b, 148, {10, 20, 30, 40});
      Near(Monstey_TextureResource(b).Decode(0).rgba[0][3], 1);
    });
    run("R8 R8G8 defaults", [] {
      auto b = Dds(61, 1, 1);
      b[148] = 128;
      auto p = Monstey_TextureResource(b).Decode(0).rgba[0];
      Near(p[0], 128 / 255.0f);
      Near(p[1], 0);
      Near(p[3], 1);
      b = Dds(49, 1, 1);
      b[148] = 128;
      b[149] = 64;
      p = Monstey_TextureResource(b).Decode(0).rgba[0];
      Near(p[1], 64 / 255.0f);
    });
    run("R32 NaN payload preserved", [] {
      auto b = Dds(41, 1, 1);
      Monstey_PutU32(b, 148, 0x7fc12345);
      Monstey_TextureResource r(b);
      Check(r.Build() == b, "NaN no-op");
      Check(std::bit_cast<std::uint32_t>(r.Decode(0).rgba[0][0]) == 0x7fc12345,
            "NaN payload");
    });
    run("sRGB declaration no automatic gamma", [] {
      auto b = Dds(29, 1, 1);
      b[148] = 128;
      Monstey_TextureResource r(b);
      Near(r.Decode(0).rgba[0][0], 128 / 255.0f);
      Check(r.Describe().at("dds_declared_color_space").as_string() == "sRGB",
            "sRGB tag");
    });
    run("Legacy DXT1 DXT5 BGRA", [] {
      for (auto format : {71U, 77U, 87U}) {
        auto b = Legacy(Dds(format),
                        format == 71   ? 0x31545844
                        : format == 77 ? 0x35545844
                                       : 0,
                        format == 87);
        Monstey_TextureResource r(b);
        Check(r.Build() == b &&
                  r.Describe().at("dds_format_dxgi").as_uint() == format,
              "Legacy profile");
      }
    });
    run("Native 2D raw metadata retained", [] {
      auto b = Native(Dds(71));
      b.push_back(0xbe);
      Monstey_TextureResource r(b);
      Check(r.Build() == b, "Native byte fidelity");
      Check(r.Describe().at("trailing_bytes").as_uint() == 1, "Opaque trailer");
    });
    run("Native partial mip chain", [] {
      auto b = Native(Dds(71, 4, 4, 1, 3), 3, 16, 16, 1, 5);
      Monstey_TextureResource r(b);
      Check(r.Describe().at("first_logical_mip").as_uint() == 2,
            "Partial mip mapping");
      Check(r.Subresources().size() == 3, "Mip ranges");
    });
    run("Native TEXA raw dimension 5", [] {
      auto b = Native(Dds(77, 4, 4, 1, 3, 5, 2), 4, 4, 4, 2, 3, 77);
      Monstey_TextureResource r(b);
      auto ss = r.Subresources();
      Check(ss.size() == 6 && ss[3].array_slice == 1 && ss[3].mip == 0,
            "Array major mip layout");
      Check(r.Describe().at("native_slice_source_names").size() == 2,
            "Raw names");
    });
    run("Native 22-byte volume", [] {
      auto b =
          Native(Legacy(Dds(87, 4, 4, 4, 3, 4), 0, true), 5, 4, 4, 4, 3, 87);
      Monstey_PutU32(b, 40 + 112, 0x200000);
      Monstey_TextureResource r(b);
      auto ss = r.Subresources();
      Check(ss.size() == 3 && ss[0].depth == 4 && ss[1].depth == 2 &&
                ss[2].depth == 1,
            "Volume mip depths");
      Check(r.Describe().at("native_metadata_bytes").as_uint() == 22,
            "Volume boundary");
      (void)r.Decode(0, 3);
      Reject([&] { (void)r.Decode(0, 4); });
    });
    run("DDS cube and 1D", [] {
      auto c =
          Monstey_TextureResource(Dds(71, 4, 4, 1, 1, 3, 1, 4)).Subresources();
      Check(c.size() == 6 && c[5].cube_face == 5, "Six cube faces");
      auto d = Monstey_TextureResource(Dds(71, 4, 1, 1, 3, 2)).Subresources();
      Check(d.size() == 3 && d.back().height == 1, "1D mip ranges");
    });
    run("CPUT raw no inferred channels", [] {
      B b(10);
      Monstey_PutU32(b, 0, 0x54555043);
      Monstey_PutU32(b, 4, 1);
      Monstey_AppendU32(b, 2);
      Monstey_AppendU32(b, 2);
      Monstey_AppendU32(b, 32);
      b.resize(38, 0x7a);
      Monstey_TextureResource r(b);
      Check(r.Build() == b && r.Subresources()[0].bytes == 16, "CPUT layout");
      Check(!r.Describe().at("decode_supported").as_bool(), "CPUT not RGBA");
      Reject([&] { (void)r.Decode(0); });
    });
    run("Unknown magic version format preserve", [] {
      for (auto b : {B{1, 2, 3, 4, 5}, Native(Dds(71)), Dds(12345)}) {
        if (b.size() > 40 && Monstey_U32(b, 0) == 0x52584554)
          Monstey_PutU32(b, 4, 99);
        Monstey_TextureResource r(b);
        Check(r.Build() == b && !r.Describe().at("layout_qualified").as_bool(),
              "Unknown profile preservation");
        Reject([&] { r.Replace(0, B{1}); });
      }
    });
    run("Truncated boundaries reject", [] {
      auto b = Native(Dds(71));
      for (std::size_t n : {0U, 3U, 17U, 19U, 40U, 100U, 196U}) {
        auto shortB = b;
        shortB.resize(n);
        Reject([&] { Monstey_TextureResource r(shortB); });
      }
    });
    run("Mip and resource range rejection", [] {
      auto b = Dds(71);
      Monstey_PutU32(b, 28, 32);
      Reject([&] { Monstey_TextureResource r(b); });
      b = Dds(71);
      Monstey_PutU32(b, 16, UINT32_MAX);
      Monstey_PutU32(b, 12, UINT32_MAX);
      Reject([&] { Monstey_TextureResource r(b); });
      b = Dds(71);
      Monstey_PutU32(b, 140, 0);
      Reject([&] { Monstey_TextureResource r(b); });
    });
    run("Decode pixel budget and index", [] {
      Monstey_TextureResource r(Dds(71));
      Reject([&] { (void)r.Decode(0, 0, 15); });
      Reject([&] { (void)r.Decode(1); });
      Reject([&] { (void)r.EncodedSubresource(1); });
    });
    run("Atomic failed batch", [] {
      auto b = Dds(71, 4, 4, 1, 2);
      Monstey_TextureResource r(b);
      std::vector<Monstey_TextureEdit> edits{{0, Red1()}, {1, B(7)}};
      Reject([&] { r.Edit(edits); });
      Check(r.Build() == b && !r.Describe().at("modified").as_bool(),
            "Failed batch changed state");
      edits[1] = {99, Red1()};
      Reject([&] { r.Edit(edits); });
      Check(r.Build() == b, "Invalid ordinal changed state");
    });
    run("Batch exact ranges and duplicate last wins", [] {
      auto b = Dds(71, 4, 4, 1, 2);
      b.push_back(0xbd);
      Monstey_TextureResource r(b);
      std::vector<Monstey_TextureEdit> edits{
          {0, B(8, 0x11)}, {1, B(8, 0x22)}, {0, B(8, 0x33)}};
      r.Edit(edits);
      auto built = r.Build();
      Check(std::equal(b.begin(), b.begin() + 148, built.begin()) &&
                built.back() == 0xbd,
            "Header/trailer mutated");
      Check(r.EncodedSubresource(0) == B(8, 0x33) &&
                r.EncodedSubresource(1) == B(8, 0x22),
            "Batch order");
    });
    run("Copy independent and caller buffer owned", [] {
      auto b = Dds(71);
      Monstey_TextureResource a(b);
      b[148] = 0xcc;
      auto c = a;
      c.Replace(0, Red1());
      Check(a.EncodedSubresource(0) == B(8) &&
                c.EncodedSubresource(0) == Red1(),
            "Copy aliases");
      a = c;
      auto encoded = c.EncodedSubresource(0);
      a.Replace(0, encoded);
      encoded[0] = 123;
      Check(a.EncodedSubresource(0) == Red1(),
            "Replace borrowed caller memory");
    });
    run("Moved state deterministic", [] {
      Monstey_TextureResource a(Dds(71));
      auto b = std::move(a);
      Check(b.Build().size() == 156, "Move target");
      Reject([&] { (void)a.Build(); });
      Reject([&] { Monstey_TextureResource c(a); });
      a = b;
      Check(a.Build() == b.Build(), "Moved reassignment");
    });
    run("Publication new file exact no overwrite", [&] {
      auto source = Dds(71);
      Monstey_TextureResource r(source);
      auto path = out / L"new-texture.tex";
      auto receipt = r.Build(path);
      Check(Monstey_ReadFile(path) == source &&
                receipt.at("no_op_byte_exact").as_bool(),
            "Publication fidelity");
      Reject([&] { r.Build(path); });
      Check(Monstey_ReadFile(path) == source, "Existing target changed");
    });
    run("Path hash source guards", [&] {
      auto bytes = Dds(71);
      const auto path = out / L"guard-source.tex";
      Monstey_WriteNewFile(path, bytes);
      Monstey_TextureResource r(path, Monstey_HashSha256(bytes));
      Reject([&] { r.Build(path); });
      Reject([&] { Monstey_TextureResource q(path, std::string(64, '0')); });
      bytes[148] = 0xaa;
      {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      }
      Reject([&] { (void)r.Build(); });
      Reject([&] { r.Replace(0, Red1()); });
      Reject([&] { r.Build(out / L"must-not-publish.tex"); });
      Check(!std::filesystem::exists(out / L"must-not-publish.tex"),
            "Changed source published");
    });
    run("Empty batch and replacement length", [] {
      Monstey_TextureResource r(Dds(71));
      Reject([&] { r.Edit({}); });
      Reject([&] { r.Replace(0, B(9)); });
      Check(!r.Describe().at("modified").as_bool(), "Rejected edit mutated");
    });

    run("Unobserved native kind remains opaque", [] {
      auto b = Native(Dds(71));
      b[18] = 4;
      Monstey_TextureResource r(b);
      Check(r.Build() == b && !r.Describe().at("layout_qualified").as_bool(),
            "TEXR array kind invented");
    });
    run("Contradictory native dimension remains opaque", [] {
      auto b = Native(Dds(71, 4, 4, 4, 1, 4));
      Monstey_TextureResource r(b);
      Check(r.Build() == b && !r.Describe().at("layout_qualified").as_bool(),
            "Native 2D accepted volume DDS");
    });
    run("Conflicting format tags remain opaque", [] {
      auto b = Native(Dds(71), 3, 4, 4, 1, 1, 77);
      Monstey_TextureResource r(b);
      Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
            "Conflicting format editable");
    });
    run("Unqualified padded pitch remains opaque", [] {
      auto b = Dds(61, 1, 1);
      Monstey_PutU32(b, 8, 0x100f);
      Monstey_PutU32(b, 20, 4);
      b.resize(152);
      Monstey_TextureResource r(b);
      Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
            "Padded pitch was silently ignored");
    });
    run("Source edit cannot mutate existing published output", [&] {
      auto b = Dds(71);
      const auto path = out / L"published-guard.tex";
      Monstey_TextureResource r(b);
      r.Build(path);
      r.Replace(0, Red1());
      Reject([&] { r.Build(path); });
      Check(Monstey_ReadFile(path) == b, "Existing published object altered");
    });
    run("Game installation output rejected", [&] {
      const auto root = out / L"fake-install";
      std::filesystem::create_directories(root);
      Monstey_WriteNewFile(root / L"ShadowOfWar.exe", B{0x4d, 0x5a});
      Monstey_TextureResource r(Dds(71));
      Reject([&] { r.Build(root / L"output.tex"); });
      Check(!std::filesystem::exists(root / L"output.tex"),
            "Game path written");
    });

    run("Unknown version minimal envelope preserved", [] {
      for (const auto magic : {0x52584554U, 0x41584554U, 0x54555043U}) {
        B b(8);
        Monstey_PutU32(b, 0, magic);
        Monstey_PutU32(b, 4, 999);
        Monstey_TextureResource r(b);
        Check(r.Build() == b && !r.Describe().at("layout_qualified").as_bool(),
              "Unknown version demanded known layout");
      }
    });

    run("RGB stale DXT1 uses 64-byte BGRA range", [&] {
      auto b = Legacy(Dds(87), 0x31545844, true);
      Monstey_PutU32(b, 8, 0x100f);
      Monstey_PutU32(b, 20, 16);
      Set(b, 128, {10, 20, 30, 40});
      Monstey_TextureResource r(b);
      auto d = r.Describe();
      auto ss = r.Subresources();
      Check(d.at("dds_format_dxgi").as_uint() == 87 && ss.size() == 1 &&
                ss[0].bytes == 64 && ss[0].offset == 128 &&
                d.at("trailing_bytes").as_uint() == 0,
            "Stale FourCC controlled RGB classification");
      Near(r.Decode(0).rgba[0][0], 30 / 255.0f);
      Near(r.Decode(0).rgba[0][3], 40 / 255.0f);
      Check(r.Build() == b, "RGB no-op");
      Reject([&] { r.Replace(0, B(8, 0xcc)); });
      r.Replace(0, B(64, 0x22));
      auto built = r.Build();
      Check(std::equal(b.begin(), b.begin() + 128, built.begin()),
            "RGB header edit");
      Check(std::all_of(built.begin() + 128, built.end(),
                        [](auto v) { return v == 0x22; }),
            "Full RGB range not edited");
      Monstey_WriteNewFile(out / L"stale-fourcc-rgb.dds", b);
    });
    run("RGB stale DX10 does not consume extension", [] {
      auto b = Legacy(Dds(87), 0x30315844, true);
      Monstey_TextureResource r(b);
      Check(r.Describe().at("dds_format_dxgi").as_uint() == 87 &&
                r.Subresources()[0].offset == 128 &&
                r.Subresources()[0].bytes == 64,
            "Stale DX10 extension consumed");
    });
    run("FourCC and DX10 need discriminator flag", [] {
      for (auto tag : {0x31545844U, 0x30315844U}) {
        auto b = Legacy(Dds(71), tag);
        Monstey_PutU32(b, 80, 0);
        Monstey_TextureResource r(b);
        Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
              "FourCC without flag decoded");
        Reject([&] { (void)r.Decode(0); });
      }
    });
    run("DX10 ignores unused RGB bitcount and masks", [] {
      auto b = Dds(71);
      Monstey_PutU32(b, 88, 13);
      Monstey_PutU32(b, 92, 0xdeadbeef);
      Monstey_PutU32(b, 104, 0xff000000);
      Monstey_TextureResource r(b);
      Check(r.Describe().at("dds_format_dxgi").as_uint() == 71 &&
                r.Subresources()[0].bytes == 8,
            "Unused RGB fields overrode DX10");
    });
    run("Conflicting discriminator flags remain opaque", [] {
      for (auto flags : {0x44U, 0x45U, 0x42U, 0x20040U, 0x204U}) {
        auto b = Legacy(Dds(87), 0x30315844, true);
        Monstey_PutU32(b, 80, flags);
        Monstey_TextureResource r(b);
        Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
              "Conflicting flags guessed a layout");
      }
    });
    run("RGB masks require RGB flag", [] {
      auto b = Legacy(Dds(87), 0, true);
      Monstey_PutU32(b, 80, 0);
      Monstey_TextureResource r(b);
      Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
            "Unflagged RGB masks decoded");
    });
    run("RGB without alpha ignores stale alpha mask", [] {
      auto b = Legacy(Dds(87), 0, true);
      Monstey_PutU32(b, 80, 0x40);
      b[131] = 0;
      Monstey_TextureResource r(b);
      Check(r.Describe().at("dds_format_dxgi").as_uint() == 88 &&
                r.Subresources()[0].bytes == 64,
            "Stale alpha mask overrode RGB flags");
      Near(r.Decode(0).rgba[0][3], 1);
    });
    run("Declared alpha needs qualified mask", [] {
      for (auto mask : {0U, 0xffU, 0x7f000000U}) {
        auto b = Legacy(Dds(87), 0, true);
        Monstey_PutU32(b, 104, mask);
        Monstey_TextureResource r(b);
        Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
              "Unqualified alpha mask decoded");
      }
    });
    run("Unqualified RGB masks remain opaque", [] {
      auto b = Legacy(Dds(87), 0, true);
      Monstey_PutU32(b, 92, 0xf800);
      Monstey_TextureResource r(b);
      Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
            "Unsupported masks guessed");
    });
    run("Qualified RGBA masks ignore stale FourCC", [] {
      auto b = Legacy(Dds(28), 0x31545844, true);
      Monstey_PutU32(b, 92, 0xff);
      Monstey_PutU32(b, 100, 0xff0000);
      Set(b, 128, {10, 20, 30, 40});
      Monstey_TextureResource r(b);
      Check(r.Describe().at("dds_format_dxgi").as_uint() == 28 &&
                r.Subresources()[0].bytes == 64,
            "RGBA stale FourCC");
      Near(r.Decode(0).rgba[0][0], 10 / 255.0f);
    });
    run("Luminance and alpha classes stay explicit opaque", [] {
      for (auto flags : {0x20000U, 0x20001U, 0x2U}) {
        auto b = Legacy(Dds(87), 0x31545844, true);
        Monstey_PutU32(b, 80, flags);
        Monstey_PutU32(b, 88, flags == 0x20001 ? 16 : 8);
        Monstey_PutU32(b, 92, flags == 2 ? 0 : 255);
        Monstey_PutU32(b, 96, 0);
        Monstey_PutU32(b, 100, 0);
        Monstey_PutU32(b, 104,
                       flags == 0x20001 ? 0xff00
                       : flags == 2     ? 255
                                        : 0);
        Monstey_TextureResource r(b);
        Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
              "Unsupported luminance/alpha guessed as RGB");
      }
    });
    run("Unknown YUV and optional flag profiles stay opaque", [] {
      for (auto flags : {0x200U, 0x80000004U, 0x5U, 0x1U}) {
        auto b = Legacy(Dds(87), 0x31545844, true);
        Monstey_PutU32(b, 80, flags);
        Monstey_TextureResource r(b);
        Check(!r.Describe().at("layout_qualified").as_bool() && r.Build() == b,
              "Unsupported flag profile guessed");
      }
    });
    run("Legacy FOURCC ignores stale RGB descriptors", [] {
      auto b = Legacy(Dds(71), 0x31545844);
      Monstey_PutU32(b, 88, 32);
      Monstey_PutU32(b, 92, 0xff0000);
      Monstey_PutU32(b, 96, 0xff00);
      Monstey_PutU32(b, 100, 0xff);
      Monstey_PutU32(b, 104, 0xff000000);
      Monstey_TextureResource r(b);
      Check(r.Subresources()[0].bytes == 8 &&
                r.Describe().at("dds_format_dxgi").as_uint() == 71,
            "Unused RGB descriptors overrode FOURCC");
    });
    const auto report =
        J::object({{"status", "PASS"},
                   {"groups", static_cast<std::uint64_t>(results.size())},
                   {"tests", results},
                   {"synthetic_only", true}});
    Monstey_WriteNewText(out / L"report.json", report.dump(2));
    std::cout << report.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
