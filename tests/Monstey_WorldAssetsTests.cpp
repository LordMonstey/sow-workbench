#include "Monstey_WorldAssets.hpp"
#include "Monstey_WorldComposition.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
void Check(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
}
template <class F> void Reject(F &&f) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  Check(bad, "Expected rejection");
}
std::string Text(const std::filesystem::path &p) {
 return Monstey_Utf8(std::filesystem::weakly_canonical(std::filesystem::absolute(p)).wstring());
}
B Bytes(const std::string &s) { return B(s.begin(), s.end()); }
void Rewrite(const std::filesystem::path &p, const B &b) {
  std::ofstream stream(Monstey_WindowsFilePath(p),
                       std::ios::binary | std::ios::trunc);
  stream.write(reinterpret_cast<const char *>(b.data()),
               static_cast<std::streamsize>(b.size()));
  Check(stream.good(), "Fixture rewrite failed");
}
void Archive(const std::filesystem::path &out,
             const std::filesystem::path &temp,
             const std::vector<std::pair<std::string, B>> &files) {
  Monstey_ArchiveProject p;
  unsigned i = 0;
  for (const auto &[path, bytes] : files) {
    const auto source = temp / (std::to_wstring(i++) + L".bin");
    Monstey_WriteNewFile(source, bytes);
    p.AddFile(path, source);
  }
  p.Build(out, false);
}
J Extract(const std::filesystem::path &game, const std::filesystem::path &out,
          J requests, std::uint64_t budget = 268435456) {
  return Monstey_WorldAssetsCall("sow_world_asset_extract",
                                 J::object({{"game_root", Text(game)},
                                            {"output", Text(out)},
                                            {"requests", std::move(requests)},
                                            {"max_total_bytes", budget}}));
}
J Request(const std::string &path, const std::string &parent = "") {
  auto r = J::object({{"resource_path", path}});
  if (!parent.empty())
    r["bundle"] = parent;
  return r;
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto dir =
        (argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                  : std::filesystem::temp_directory_path()) /
        ("assets-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto game = dir / L"game", config = game / L"x64/default.archcfg";
    std::filesystem::create_directories(game / L"x64");
    const auto parent = Monstey_BuildEmbedded(
        {{"only.bin", Bytes("explicit child")},
         {"shared.bin", Bytes("embedded old")},
         {"large-unselected.bin", B(2 * 1024 * 1024, 0x55)}});
    Archive(game / L"base.arch06", dir / L"base-input",
            {{"shared.bin", Bytes("base")},
             {"parent.embb", parent},
             {"nested/small.objres", Bytes("fixture")}});
    Archive(game / L"patch.arch06", dir / L"patch-input",
            {{"shared.bin", Bytes("patch")}});
    Monstey_WriteNewText(game / L"x64/Live/shared.bin", "loose winner");
    Monstey_WriteNewFile(
        game / L"x64/Live/loose.embb",
        Monstey_BuildEmbedded({{"loose-child.bin", Bytes("loose child")}}));
    Monstey_WriteNewText(
        config,
        "\"..\\base.arch06\" ; base\n;comment\n\n..\\patch.arch06\nLive\n");
    const auto configHash = Monstey_HashSha256(config);
    unsigned passed = 0;
    const auto test = [&](const char *s, auto &&f) {
      f();
      ++passed;
      std::cout << "PASS " << s << '\n';
    };
    test(
        "current native mount order chooses latest loose direct resource", [&] {
          const auto out = dir / L"direct";
          const auto r = Extract(
              game, out, J::array({Request("SHARED.BIN", "unavailable.embb")}));
          Check(Monstey_ReadText(out / L"assets/shared.bin") == "loose winner",
                "Mount order ignored");
          Check(r.at("resources")[0].at("provenance").at("kind").as_string() ==
                    "ordered_loose_resource",
                "Direct override provenance wrong");
          Check(r.at("resources")[0]
                        .at("provenance")
                        .at("priority_line")
                        .as_uint() == 5,
                "Physical config priority wrong");
          Check(J::Parse(Monstey_ReadText(out / L"resource-manifest.json")) ==
                    r,
                "Manifest differs");
        });
    test("explicit parent selects one embedded child without copying parent",
         [&] {
           const auto out = dir / L"embedded";
           const auto r = Extract(
               game, out, J::array({Request("only.bin", "parent.embb")}));
           Check(Monstey_ReadText(out / L"assets/only.bin") == "explicit child",
                 "Child contents differ");
           const auto p = r.at("resources")[0].at("provenance");
           Check(p.at("kind").as_string() == "explicit_embedded_child" &&
                     !p.at("extraction").at("parent_copied").as_bool(),
                 "Parent copied or source missing");
           Check(r.at("total_bytes").as_uint() == 14,
                 "Only selected bytes count wrong");
           Check(!std::filesystem::exists(out / L"assets/parent.embb"),
                 "Parent payload copied");
         });
    test("loose native EMBB parent uses the same bounded decoder", [&] {
      const auto out = dir / L"loose-parent";
      Extract(game, out, J::array({Request("loose-child.bin", "loose.embb")}));
      Check(Monstey_ReadText(out / L"assets/loose-child.bin") == "loose child",
            "Loose parent child differs");
    });
    test("later archive overrides earlier direct resource", [&] {
      Rewrite(config, Bytes("..\\base.arch06\n..\\patch.arch06\n"));
      const auto out = dir / L"patch-direct";
      Extract(game, out, J::array({Request("shared.bin")}));
      Check(Monstey_ReadText(out / L"assets/shared.bin") == "patch",
            "Later archive lost");
      Rewrite(config, Bytes("\"..\\base.arch06\" ; "
                            "base\n;comment\n\n..\\patch.arch06\nLive\n"));
    });
    test("budget and missing parent fail before output directory claim", [&] {
      const auto out = dir / L"budget";
      Reject([&] {
        Extract(game, out,
                J::array({Request("shared.bin"),
                          Request("only.bin", "parent.embb")}),
                12);
      });
      Check(!std::filesystem::exists(out),
            "Budget error created output directory");
      Reject([&] { Extract(game, out, J::array({Request("only.bin")})); });
      Check(!std::filesystem::exists(out),
            "Missing parent created output directory");
    });
    test("duplicate unsafe paths and game destination reject", [&] {
      Reject([&] {
        Extract(game, dir / L"duplicate",
                J::array({Request("Shared.bin"), Request("shared.BIN")}));
      });
      Reject([&] {
        Extract(game, dir / L"unsafe", J::array({Request("../secret")}));
      });
      Reject([&] {
        Extract(game, game / L"do-not-write",
                J::array({Request("shared.bin")}));
      });
      const auto existing = dir / L"existing";
      std::filesystem::create_directories(existing);
      Monstey_WriteNewText(existing / L"keep", "keep");
      Reject(
          [&] { Extract(game, existing, J::array({Request("shared.bin")})); });
      Check(Monstey_ReadText(existing / L"keep") == "keep",
            "Existing destination modified");
    });
    test("deep Unicode destinations work for direct and embedded extraction",
         [&] {
           const auto out = dir / (L"long-\u03a9-" + std::wstring(70, L'a')) /
                            std::wstring(70, L'b') / std::wstring(70, L'c');
           const auto r =
               Extract(game, out,
                       J::array({Request("nested/small.objres"),
                                 Request("only.bin", "parent.embb")}));
           Check(Monstey_ReadText(out / L"assets/nested/small.objres") ==
                         "fixture" &&
                     Monstey_ReadText(out / L"assets/only.bin") ==
                         "explicit child",
                 "Deep extraction differs");
           Check(r.at("resources").size() == 2,
                 "Deep extraction manifest incomplete");
         });
    test("JSONL and component manifest filters preserve all variants", [&] {
      const auto catalog = dir / L"catalog.jsonl",
                 manifest = dir / L"custom.json";
      auto first = J::object({{"path", "Worlds/Test.OBJRES"},
                              {"bundle", "Native.EMBB"},
                              {"variant", 1}});
      auto second = J::object({{"resource_path", "worlds/test.objres"},
                               {"bundle", "native.embb"},
                               {"variant", 2}});
      Monstey_WriteNewText(catalog, std::string("\xef\xbb\xbf") + first.dump() +
                                        "\n\n" + second.dump() + "\n");
      Monstey_WriteNewText(
          manifest,
          J::object({{"resources", J::array({J::object(
                                       {{"resource_path", "custom/test.objres"},
                                        {"bundle", J()}})})}})
              .dump());
      const auto all = Monstey_WorldAssetsCall(
          "sow_world_asset_search",
          J::object({{"catalog", Text(catalog)},
                     {"suffix", ".OBJRES"},
                     {"manifests", J::array({Text(manifest)})},
                     {"offset", 1},
                     {"limit", 1}}));
      Check(all.at("total").as_uint() == 3 &&
                all.at("items")[0].at("variant").as_uint() == 2 &&
                all.at("next_offset").as_uint() == 2,
            "Variant pagination differs");
      const auto filtered = Monstey_WorldAssetsCall(
          "sow_world_asset_search", J::object({{"catalog", Text(catalog)},
                                               {"bundle_contains", "NATIVE"},
                                               {"path_contains", "TEST"},
                                               {"suffix", ".objres"}}));
      Check(filtered.at("total").as_uint() == 2 &&
                filtered.at("items")[0].at("catalog_origin").as_string() ==
                    Text(catalog),
            "Native filters or provenance differs");
      const auto custom = Monstey_WorldAssetsCall(
          "sow_world_asset_search",
          J::object({{"catalog", Text(dir / L"absent.jsonl")},
                     {"manifests", J::array({Text(manifest)})}}));
      Check(custom.at("total").as_uint() == 1, "Manifest-only search failed");
    });
    test("new native catalog output is searchable without schema loss", [&] {
      const auto catalog = dir / L"native-catalog.jsonl";
      Monstey_ArchiveCatalog(game, catalog,
                             J::object({{"include_embedded", true}}));
      const auto r =
          Monstey_WorldAssetsCall("sow_world_asset_search",
                                  J::object({{"catalog", Text(catalog)},
                                             {"path_contains", "shared.bin"}}));
      Check(r.at("total").as_uint() == 4,
            "Native direct/embedded variants missing");
      for (const auto &row : r.at("items").as_array())
        Check(row.contains("kind") && row.contains("priority"),
              "Native metadata dropped");
    });
    test("corrupt later payload rolls back earlier published resources", [&] {
      const auto badGame = dir / L"bad-game";
      std::filesystem::create_directories(badGame / L"x64");
      Archive(badGame / L"bad.arch06", dir / L"bad-input",
              {{"good.bin", Bytes("good")}, {"bad.bin", Bytes("bad")}});
      const auto path = badGame / L"bad.arch06";
      Monstey_Archive archive(path);
      auto bytes = Monstey_ReadFile(path);
      Monstey_PutU32(
          bytes, static_cast<std::size_t>(archive.Find("bad.bin")->offset), 0);
      Rewrite(path, bytes);
      Monstey_WriteNewText(badGame / L"x64/default.archcfg",
                           "..\\bad.arch06\n");
      const auto out = dir / L"rollback";
      Reject([&] {
        Extract(badGame, out,
                J::array({Request("good.bin"), Request("bad.bin")}));
      });
      Check(!std::filesystem::exists(out),
            "Failed group left an earlier output or directories");
    });
    test("search input validation does not mutate catalogs", [&] {
      const auto absent = dir / L"missing.jsonl";
      Reject([&] {
        Monstey_WorldAssetsCall("sow_world_asset_search",
                                J::object({{"catalog", Text(absent)}}));
      });
      Reject([&] {
        Monstey_WorldAssetsCall(
            "sow_world_asset_search",
            J::object({{"catalog", Text(absent)}, {"limit", 0}}));
      });
      Reject([&] {
        Monstey_WorldAssetsCall(
            "sow_world_asset_search",
            J::object({{"catalog", Text(absent)}, {"unknown", 1}}));
      });
      Check(Monstey_HashSha256(config) == configHash,
            "Native mount configuration changed");
    });
    if (argc > 2) {
      const auto nativeGame = std::filesystem::path(Monstey_Wide(argv[2]));
      test("actual mounted game extracts one small resource read-only", [&] {
        const auto mounts = Monstey_ArchiveMounts(nativeGame);
        std::string selected;
        std::uint64_t size = 0;
        for (auto it = mounts.at("mounts").as_array().rbegin();
             it != mounts.at("mounts").as_array().rend() && selected.empty();
             ++it) {
          if (it->at("kind").as_string() != "archive")
            continue;
          Monstey_Archive archive(std::filesystem::path(
              Monstey_Wide(it->at("source").as_string())));
          for (const auto &entry : archive.Entries())
            if (entry.size > 0 && entry.size <= 65536) {
              selected = entry.path;
              size = entry.size;
              break;
            }
        }
        Check(!selected.empty(), "No small native direct fixture found");
        auto r = Extract(nativeGame, dir / L"native-direct",
                         J::array({Request(selected)}));
        Check(r.at("resources").size() == 1 &&
                  r.at("total_bytes").as_uint() <= 268435456,
              "Native resource output wrong");
        (void)size;
        Monstey_WriteNewText(dir / L"native-proof.json", r.dump(2));
      });
    }
    std::cout << passed << " world assets groups passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}