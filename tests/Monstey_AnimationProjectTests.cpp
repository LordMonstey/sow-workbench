#include "Monstey_AnimationProject.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <windows.h>
using namespace Monstey;
namespace {
using J = Monstey_Json;
void Monstey_Test(bool ok, const char *message) {
  Monstey_Require(ok, message);
}
std::uint16_t Monstey_ProjectTestChannel(const Monstey_Anix &library, std::uint32_t type, std::uint32_t bone) {
  const auto description = library.Describe();
  for (const auto &channel : description["channels"].as_array())
    if (channel["type_hash"].as_string() == Monstey_Hex(type) &&
        channel["bone_hash"].as_string() == Monstey_Hex(bone))
      return static_cast<std::uint16_t>(channel["index"].as_uint());
  throw std::runtime_error("Expected native project fixture identity is absent");
}
J Monstey_Scene() {
  auto track = [](unsigned channel, J a, J b) {
    return J::object(
        {{"channel_index", channel}, {"values", J::array({a, b})}});
  };
  auto clip = [&](const char *name, bool moving) {
    return J::object(
        {{"name", name},
         {"duration_ticks", 1000},
         {"ticks", J::array({0, 1000})},
         {"tracks",
          J::array(
              {track(0, J::array({0, 0, 0}), J::array({moving ? 20 : 0, 0, 0})),
               track(1, J::array({0, 0, 0, 1}), J::array({0, 0, 0, 1}))})},
         {"events", J::array()}});
  };
  return J::object(
      {{"format", "Monstey_ANIX_Semantic_v1"},
       {"library_path", "characters/animationcharacters/synthetic/native.anix"},
       {"channels", J::array({J::object({{"type_hash", "00ce66fc"},
                                         {"bone_hash", "88baa7e7"}}),
                              J::object({{"type_hash", "27436396"},
                                         {"bone_hash", "88baa7e7"}})})},
       {"clips", J::array({clip("bind", false), clip("walk", true)})}});
}

Monstey_Bytes Monstey_UnknownCaf4Fixture(bool misaligned) {
  auto scene = Monstey_Scene();
  scene["channels"].as_array().push_back(
      J::object({{"type_hash", "5f1d8eed"}, {"bone_hash", "11111111"}}));
  auto bytes = Monstey_Anix::Create(scene).Build();
  const auto metadata = Monstey_U32(bytes, 32);
  const auto descriptor = metadata + Monstey_U32(bytes, metadata + 28);
  const auto codecs = Monstey_U16(bytes, descriptor + 56);
  const auto tracks = Monstey_U16(bytes, descriptor + 58);
  std::size_t scalar_codec = codecs;
  for (std::size_t i = 0; i < codecs; ++i)
    if (std::string(reinterpret_cast<const char *>(bytes.data() + descriptor +
                                                   124 + 4 * i),
                    4) == "UATR") {
      scalar_codec = i;
      std::copy_n("CAF4", 4, bytes.begin() + descriptor + 124 + 4 * i);
    }
  Monstey_Test(scalar_codec < codecs,
               "Unknown CAF4 fixture has no source codec");
  const auto types = descriptor + 124 + 4 * codecs;
  const auto ids = (types + tracks + 3) / 4 * 4;
  for (std::size_t i = 0; i < tracks; ++i)
    if (Monstey_U16(bytes, ids + 2 * i) == 0)
      Monstey_PutU16(bytes, ids + 2 * i, 2);
  const auto table = (ids + 2 * tracks + 3) / 4 * 4;
  const auto segment = Monstey_U32(bytes, 24) + Monstey_U32(bytes, table);
  auto codec_offset = Monstey_U16(bytes, segment + 4 + 2 * scalar_codec);
  if (misaligned) {
    codec_offset += 4;
    Monstey_PutU16(bytes, segment + 4 + 2 * scalar_codec, codec_offset);
  }
  Monstey_PutU32(bytes, segment + codec_offset, misaligned ? 0 : 2);
  return bytes;
}

void Monstey_Overwrite(const std::filesystem::path &p,
                       const Monstey_Bytes &bytes) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char *>(bytes.data()),
          static_cast<std::streamsize>(bytes.size()));
  Monstey_Test(f.good(), "Cannot change owned test fixture");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto out =
        std::filesystem::path(argc > 1 ? argv[1] : L".") /
        (L"AnimationProject_" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(out);
    auto cases = J::array();
    auto test = [&](const char *name, auto f) {
      f();
      cases.as_array().push_back(name);
      std::cout << name << std::endl;
    };
    auto reject = [](auto f) {
      bool failed = false;
      try {
        f();
      } catch (const Monstey_Error &) {
        failed = true;
      }
      Monstey_Test(failed, "Expected animation project rejection");
    };
    const auto scenePath = out / L"scene.json", source = out / L"source.anix";
    const auto scene = Monstey_Scene();
    Monstey_WriteNewText(scenePath, scene.dump());
    const auto raw = Monstey_Anix::Create(scene).Build();
    Monstey_WriteNewFile(source, raw);
    const auto hash = Monstey_HashSha256(source);
    Monstey_AnimationProject project(source, hash);
    test("catalog_has_eighteen_native_animation_operations", [&] {
      Monstey_Test(Monstey_AnimationCatalog().size() == 18,
                   "Animation catalog incomplete");
    });
    test("source_qualified_compact_description_and_paging", [&] {
      const auto d = project.Describe();
      Monstey_Test(d["clip_count"].as_uint() == 2 && !d["dirty"].as_bool() &&
                       !d.contains("clips"),
                   "Description is not compact/native");
      auto p = project.Clips("WALK", 0, 1);
      Monstey_Test(p["total"].as_uint() == 1 &&
                       p["rows"][0]["name"].as_string() == "walk" &&
                       !p["rows"][0].contains("segments"),
                   "Clip paging/filter mismatch");
      Monstey_Test(project.Clips({}, 999, 1)["rows"].empty() &&
                       project.Clips({}, 0, 0)["total"].as_uint() == 2,
                   "Animation paging totals wrong");
    });
    test("source_hash_mismatch_rejects_before_session_open", [&] {
      reject([&] {
        Monstey_AnimationProject wrong(source, std::string(64, '0'));
      });
    });
    test("no_op_native_build_is_byte_exact_and_sources_are_protected", [&] {
      auto report = project.Build(out / L"noop.anix");
      Monstey_Test(report["no_op_byte_exact"].as_bool() &&
                       Monstey_ReadFile(out / L"noop.anix") == raw,
                   "Animation no-op changed bytes");
      reject([&] { project.Build(source); });
      reject([&] { project.Build(out / L"noop.anix"); });
    });
    test("native_sample_and_clip_metadata_are_read_only", [&] {
      const auto before = project.Changes();
      const auto pose = project.Sample("walk", J::array({0, 500, 1000}));
      Monstey_Test(pose["tracks"].size() == 2 &&
                       project.Clip("walk")["duration_ticks"].as_uint() ==
                           1000 &&
                       project.Changes() == before,
                   "Animation read changed history");
      reject([&] { project.Sample("walk", J::array()); });
      reject([&] { project.Clip("missing"); });
    });
    test("edit_batch_undo_redo_and_revert_share_one_history", [&] {
      project.Edit(J::array(
          {J::object({{"op", "duplicate"}, {"clip", "walk"}, {"name", "copy"}}),
           J::object(
               {{"op", "rename"}, {"clip", "copy"}, {"name", "renamed"}})}));
      Monstey_Test(project.Clips()["total"].as_uint() == 3 &&
                       project.Describe()["undo_steps"].as_uint() == 1,
                   "Batch history split");
      project.Undo();
      Monstey_Test(!project.Describe()["dirty"].as_bool(), "Undo failed");
      project.Redo();
      const auto changed = project.Describe()["effective_sha256"];
      project.Revert();
      Monstey_Test(!project.Describe()["dirty"].as_bool(), "Revert failed");
      project.Undo();
      Monstey_Test(project.Describe()["effective_sha256"] == changed,
                   "Revert is not undoable");
      project.Redo();
    });
    test("invalid_late_batch_is_atomic", [&] {
      const auto before = project.Changes();
      reject([&] {
        project.Edit(
            J::array({J::object({{"op", "duplicate"},
                                 {"clip", "walk"},
                                 {"name", "transient"}}),
                      J::object({{"op", "delete"}, {"clip", "bind"}})}));
      });
      Monstey_Test(project.Changes() == before,
                   "Failed batch committed partial edit");
    });
    test("new_edit_discards_redo_without_touching_baseline", [&] {
      project.Undo();
      project.Edit(J::array({J::object(
          {{"op", "rename"}, {"clip", "renamed"}, {"name", "final"}})}));
      Monstey_Test(!project.Describe()["can_redo"].as_bool() &&
                       Monstey_HashSha256(source) == hash,
                   "Redo branch or source changed");
    });
    test("source_qualified_project_replay_matches_native_bytes", [&] {
      const auto p = out / L"project.json", a = out / L"before.anix",
                 b = out / L"after.anix";
      project.SaveProject(p);
      project.Build(a);
      auto replay = Monstey_AnimationProject::LoadProject(p);
      replay->Build(b);
      Monstey_Test(Monstey_ReadFile(a) == Monstey_ReadFile(b) &&
                       replay->Changes()["steps"] == project.Changes()["steps"],
                   "Project/native replay differs");
    });
    test("semantic_new_and_utf8_bom_scene_are_supported", [&] {
      Monstey_AnimationProject fresh(scenePath, {}, true);
      Monstey_Test(fresh.Describe()["source_kind"].as_string() == "semantic",
                   "New semantic source not tracked");
      auto text = std::string("\xef\xbb\xbf") + scene.dump();
      const auto bom = out / L"bom.json";
      Monstey_WriteNewText(bom, text);
      Monstey_AnimationProject withBom(bom, {}, true);
      Monstey_Test(withBom.Describe()["effective_sha256"] ==
                       fresh.Describe()["effective_sha256"],
                   "BOM scene changed native result");
    });
    test("semantic_export_and_hash_remapped_clip_import", [&] {
      const auto exported = out / L"export.json";
      project.Export(exported, "walk");
      auto document = J::Parse(Monstey_ReadText(exported));
      std::swap(document["channels"][0], document["channels"][1]);
      for (auto &t : document["clips"][0]["tracks"].as_array())
        t["channel_index"] = 1 - t["channel_index"].as_uint();
      const auto remap = out / L"remap.json";
      Monstey_WriteNewText(remap, document.dump());
      project.ImportClip(remap, "walk", "imported", Monstey_HashSha256(remap));
      Monstey_Test(project.Clip("imported")["duration_ticks"].as_uint() == 1000,
                   "Remapped import missing");
      const auto a = project.Sample("walk", J::array({0, 1000}), false),
                 b = project.Sample("imported", J::array({0, 1000}), false);
      Monstey_Test(
          a["tracks"] == b["tracks"],
          "Imported source channel indices were not remapped by type/bone");
    });
    test("missing_or_ambiguous_import_channel_rejects_atomically", [&] {
      auto document = scene;
      document["channels"][0]["bone_hash"] = "12345678";
      const auto path = out / L"unmapped.json";
      Monstey_WriteNewText(path, document.dump());
      const auto before = project.Changes();
      reject([&] { project.ImportClip(path, "walk", "unmapped"); });
      Monstey_Test(project.Changes() == before,
                   "Unmapped clip partially imported");
    });
    test("same_size_restored_timestamp_source_mutation_rejects_read_edit_build",
         [&] {
           const auto file = out / L"stale.anix";
           Monstey_WriteNewFile(file, raw);
           Monstey_AnimationProject stale(file);
           const auto time = std::filesystem::last_write_time(file);
           auto data = raw;
           data.back() ^= 1;
           Monstey_Overwrite(file, data);
           std::filesystem::last_write_time(file, time);
           reject([&] { stale.Clips(); });
           reject([&] {
             stale.Edit(
                 J::array({J::object({{"op", "delete"}, {"clip", "walk"}})}));
           });
           reject([&] { stale.Build(out / L"stale-output.anix"); });
           Monstey_Test(!std::filesystem::exists(out / L"stale-output.anix"),
                        "Stale animation output created");
         });
    test("tampered_project_digest_and_missing_source_reject", [&] {
      auto doc = J::Parse(Monstey_ReadText(out / L"project.json"));
      doc["effective_sha256"] = std::string(64, '0');
      const auto bad = out / L"bad-project.json";
      Monstey_WriteNewText(bad, doc.dump());
      reject([&] { Monstey_AnimationProject::LoadProject(bad); });
    });

    test("publication_source_failure_rolls_back_owned_output", [&] {
      const auto file = out / L"rollback.anix";
      const std::array<std::filesystem::path, 1> sources{source};
      unsigned checks{};
      bool correct = false;
      try {
        Monstey_AnimationPublishOffline(file, raw, sources, [&] {
          if (++checks == 2)
            throw Monstey_Error("injected_source_change",
                                "Synthetic source changed");
        });
      } catch (const Monstey_Error &error) {
        correct = error.code == "injected_source_change" &&
                  std::string(error.what()) == "Synthetic source changed";
      }
      Monstey_Test(correct && checks == 2 && !std::filesystem::exists(file),
                   "Owned output rollback lost original error or left bytes");
    });
    test("publication_locked_output_reports_incomplete_rollback", [&] {
      const auto file = out / L"locked-rollback.anix";
      const std::array<std::filesystem::path, 1> sources{source};
      HANDLE locked = INVALID_HANDLE_VALUE;
      unsigned checks{};
      bool correct = false;
      try {
        Monstey_AnimationPublishOffline(file, raw, sources, [&] {
          if (++checks != 2)
            return;
          locked = CreateFileW(file.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
          Monstey_Test(locked != INVALID_HANDLE_VALUE,
                       "Could not lock owned output");
          throw Monstey_Error("injected_source_change",
                              "Synthetic source changed");
        });
      } catch (const Monstey_Error &error) {
        correct = error.code == "injected_source_change" &&
                  std::string(error.what()).find("rollback was incomplete") !=
                      std::string::npos;
      } catch (...) {
        if (locked != INVALID_HANDLE_VALUE)
          CloseHandle(locked);
        throw;
      }
      if (locked != INVALID_HANDLE_VALUE)
        CloseHandle(locked);
      Monstey_Test(correct && Monstey_ReadFile(file) == raw,
                   "Failed Windows deletion was not reported or bytes changed");
    });
    test("publication_preserves_foreign_replacement_and_reports_rollback", [&] {
      const auto file = out / L"foreign-rollback.anix";
      const std::array<std::filesystem::path, 1> sources{source};
      const Monstey_Bytes replacement{7, 6, 5, 4};
      unsigned checks{};
      bool correct = false;
      try {
        Monstey_AnimationPublishOffline(file, raw, sources, [&] {
          if (++checks == 2) {
            Monstey_Overwrite(file, replacement);
            throw Monstey_Error("injected_source_change",
                                "Synthetic source changed");
          }
        });
      } catch (const Monstey_Error &error) {
        correct = error.code == "injected_source_change" &&
                  std::string(error.what())
                          .find("externally changed files were preserved") !=
                      std::string::npos;
      }
      Monstey_Test(correct && Monstey_ReadFile(file) == replacement,
                   "Rollback deleted a replacement owned by another writer");
    });
    test("history_bound_roundtrips_and_rejected_step_is_atomic", [&] {
      Monstey_AnimationProject history(source);
      for (unsigned i = 0; i < 10000; ++i)
        history.Revert();
      const auto before = history.Describe();
      Monstey_Test(before["undo_steps"].as_uint() == 10000,
                   "Valid history boundary was rejected");
      reject([&] { history.Revert(); });
      Monstey_Test(history.Describe() == before,
                   "Rejected history step changed effective state");
      const auto file = out / L"history-limit.json";
      history.SaveProject(file);
      auto replay = Monstey_AnimationProject::LoadProject(file);
      Monstey_Test(replay->Describe()["undo_steps"].as_uint() == 10000 &&
                       replay->Describe()["effective_sha256"] ==
                           before["effective_sha256"],
                   "Saved boundary history cannot be loaded");
      history.Undo();
      history.Edit(J::array({J::object(
          {{"op", "duplicate"}, {"clip", "walk"}, {"name", "last"}})}));
      Monstey_Test(history.Describe()["undo_steps"].as_uint() == 10000 &&
                       !history.Describe()["can_redo"].as_bool(),
                   "Editing an undone boundary cannot replace the redo branch");
    });
    test("event_import_maps_exact_type_bone_identity_and_preserves_empty_"
         "binding",
         [&] {
           auto document = scene;
           document["channels"].as_array().push_back(J::object(
               {{"type_hash", "210c8f5a"}, {"bone_hash", "11111111"}}));
           document["channels"].as_array().push_back(J::object(
               {{"type_hash", "210c8f5a"}, {"bone_hash", "22222222"}}));
           document["clips"][1]["event_channel_index"] = 3;
           document["clips"][1]["events"] =
               J::array({J::object({{"tick", 500}, {"text", "SYNTH_EVENT"}})});
           const auto file = out / L"event-target.anix";
           Monstey_WriteNewFile(file, Monstey_Anix::Create(document).Build());
           Monstey_AnimationProject events(file);
           const auto info = events.Clip("walk");
           Monstey_Test(info["event_edit_supported"].as_bool() &&
                            info["event_channel_index"].as_uint() == 2 &&
                            info["events"] == document["clips"][1]["events"] &&
                            info["event_channels"].size() == 2,
                        "Selected clip metadata omitted event list or native "
                        "channel choices");
           std::swap(document["channels"][2], document["channels"][3]);
           document["clips"][1]["event_channel_index"] = 2;
           const auto remap = out / L"event-remap.json";
           Monstey_WriteNewText(remap, document.dump());
           events.ImportClip(remap, "walk", "event-copy");
           const auto exported = out / L"event-copy.json";
           events.Export(exported, "event-copy");
           const auto clip = J::Parse(Monstey_ReadText(exported))["clips"][0];
           Monstey_Test(
               clip["event_channel_index"].as_uint() == 2 &&
                   clip["events"] == document["clips"][1]["events"],
               "Imported Event used source ordinal or first global channel");
           document["clips"][1]["events"] = J::array();
           const auto empty = out / L"empty-event-remap.json";
           Monstey_WriteNewText(empty, document.dump());
           events.ImportClip(empty, "walk", "empty-event-copy");
           const auto emptyExport = out / L"empty-event-copy.json";
           events.Export(emptyExport, "empty-event-copy");
           const auto emptyClip =
               J::Parse(Monstey_ReadText(emptyExport))["clips"][0];
           Monstey_Test(emptyClip["events"].empty() &&
                            emptyClip["event_channel_index"].as_uint() == 2,
                        "Empty explicit Event identity was lost");
           const auto before = events.Changes();
           document["channels"][2]["bone_hash"] = "33333333";
           const auto unmatched = out / L"event-unmatched.json";
           Monstey_WriteNewText(unmatched, document.dump());
           reject([&] { events.ImportClip(unmatched, "walk", "bad-event"); });
           Monstey_Test(events.Changes() == before,
                        "Unmatched event import partially committed");
         });
    test("selected_export_ignores_an_unrelated_unqualified_event_binding", [&] {
      auto document = scene;
      document["channels"].as_array().push_back(
          J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "11111111"}}));
      document["channels"].as_array().push_back(
          J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "22222222"}}));
      document["clips"][1]["event_channel_index"] = 3;
      document["clips"][1]["events"] =
          J::array({J::object({{"tick", 500}, {"text", "SYNTH_EVENT"}})});
      document["channels"].as_array().push_back(
          J::object({{"type_hash", "27436396"}, {"bone_hash", "33333333"}}));
      for (auto &row : document["clips"].as_array())
        row["tracks"].as_array().push_back(J::object(
            {{"channel_index", 4},
             {"values",
              J::array({J::array({0, 0, 0, 1}), J::array({0, 0, 0, 1})})}}));
      const auto native = Monstey_Anix::Create(document);
      const auto addedRotation = Monstey_ProjectTestChannel(native, 0x27436396, 0x33333333);
      const auto otherEvent = Monstey_ProjectTestChannel(native, 0x210c8f5a, 0x11111111);
      Monstey_Test(otherEvent != native.ExportClip("walk")["event_channel_index"].as_uint(),
                   "Malformed fixture must select a second distinct Event identity");
      auto bytes = native.Build();
      const auto metadata = Monstey_U32(bytes, 32);
      const auto descriptor = metadata + Monstey_U32(bytes, metadata + 28);
      const auto codecs = Monstey_U16(bytes, descriptor + 56);
      const auto tracks = Monstey_U16(bytes, descriptor + 58);
      const auto types = descriptor + 124 + 4 * codecs;
      const auto ids = (types + tracks + 3) / 4 * 4;
      std::uint8_t stri{};
      for (std::size_t i = 0; i < codecs; ++i)
        if (std::string(reinterpret_cast<const char *>(
                            bytes.data() + descriptor + 124 + 4 * i),
                        4) == "STRI")
          stri = static_cast<std::uint8_t>(i);
      unsigned converted = 0;
      for (std::size_t i = 0; i < tracks; ++i)
        if (Monstey_U16(bytes, ids + 2 * i) == addedRotation) {
          bytes[types + i] = stri;
          Monstey_PutU16(bytes, ids + 2 * i, otherEvent);
          ++converted;
        }
      Monstey_Test(converted == 1, "Expected one added Rotation track to become the other STRI track");
      const auto file = out / L"mixed-qualified.anix";
      Monstey_WriteNewFile(file, bytes);
      Monstey_AnimationProject mixed(file);
      const auto info = mixed.Clip("walk");
      Monstey_Test(!info["event_edit_supported"].as_bool() &&
                       info["event_error"].as_string() ==
                           "unsupported_anix_event_binding",
                   "Unsupported event bindings became invisible to the editor");
      const auto selected = out / L"selected-bind.json";
      mixed.Export(selected, "bind");
      Monstey_Test(J::Parse(Monstey_ReadText(selected))["clips"].size() == 1,
                   "Selected export did not isolate the requested clip");
      reject([&] { mixed.Export(out / L"all-mixed.json"); });
      Monstey_Test(!std::filesystem::exists(out / L"all-mixed.json"),
                   "Unqualified full export published partial output");
    });

    auto unknownProfile = [&](bool misaligned) {
      const auto bytes = Monstey_UnknownCaf4Fixture(misaligned);
      const auto file = out / (misaligned ? L"opaque4.anix" : L"opaque16.anix");
      const auto output =
          out / (misaligned ? L"opaque4-noop.anix" : L"opaque16-noop.anix");
      Monstey_WriteNewFile(file, bytes);
      Monstey_AnimationProject opaque(file, Monstey_HashSha256(bytes));
      const auto metadata = opaque.Describe();
      Monstey_Test(
          metadata["opaque_codecs"] == J::array({"CAF4"}) &&
              metadata["opaque_codec_blocks"].as_uint() == 1 &&
              metadata["unqualified_variants"].size() == 1 &&
              opaque.Clips()["total"].as_uint() == 2 &&
              opaque.Clip("walk")["name"].as_string() == "walk",
          "Unknown CAF4 mode no longer opens as an inspectable opaque profile");
      const auto result = opaque.Build(output);
      Monstey_Test(result["no_op_byte_exact"].as_bool() &&
                       Monstey_ReadFile(output) == bytes &&
                       Monstey_ReadFile(file) == bytes,
                   "Unknown CAF4 mode no-op changed native bytes");
      bool unsupported = false;
      try {
        (void)opaque.Sample("walk", J::array({0}), false);
      } catch (const Monstey_Error &error) {
        unsupported = error.code == "unsupported_anix_codec_variant";
      }
      Monstey_Test(unsupported,
                   "Unknown CAF4 numeric sample was silently skipped");
    };
    test(
        "unknown_CAF4_mode_aligned_open_inspect_noop_and_explicit_sample_error",
        [&] { unknownProfile(false); });
    test("unknown_CAF4_mode_old_four_byte_profile_open_inspect_noop_and_"
         "explicit_sample_error",
         [&] { unknownProfile(true); });

    J actual = nullptr;
    test(
        "compact_paged_history_preserves_full_journal_and_project_replay", [&] {
          Monstey_AnimationProject history(source, hash);
          history.Edit(J::array({J::object({{"op", "duplicate"},
                                            {"clip", "walk"},
                                            {"name", "history_copy"}})}));
          history.Edit(J::array({J::object(
              {{"op", "rate"}, {"clip", "history_copy"}, {"factor", 2.0}})}));
          const auto full = history.Changes();
          const auto view = history.Changes(J::object(
              {{"include_operations", false}, {"offset", 0}, {"limit", 1}}));
          Monstey_Test(
              view["total_steps"].as_uint() == 2 &&
                  view["returned_steps"].as_uint() == 1 &&
                  view["has_more"].as_bool() &&
                  !view["operations_included"].as_bool() &&
                  !view["steps"][0].contains("operations") &&
                  view["steps"][0]["operation_count"].as_uint() == 1 &&
                  view["steps"][0]["operation_kinds"] ==
                      J::array({"duplicate"}),
              "Compact history page exposes payload or incorrect counts");
          const auto second = history.Changes(J::object(
              {{"include_operations", false}, {"offset", 1}, {"limit", 1}}));
          Monstey_Test(!second["has_more"].as_bool() &&
                           second["steps"][0]["first_clip"].as_string() ==
                               "history_copy" &&
                           second["steps"][0]["operation_kinds"] ==
                               J::array({"rate"}),
                       "Compact history second page mismatch");
          const auto dense = history.Changes(J::object(
              {{"include_operations", true}, {"offset", 0}, {"limit", 1}}));
          Monstey_Test(dense["steps"][0] == full["steps"][0] &&
                           history.Changes() == full &&
                           !full.contains("operations_included"),
                       "History view changed default journal or session state");
          const auto saved = out / L"compact-history-full.project.json";
          history.SaveProject(saved);
          const auto document = J::Parse(Monstey_ReadText(saved));
          Monstey_Test(
              document["steps"] == full["steps"],
              "Compact inspection changed the persisted operation payload");
          auto replay = Monstey_AnimationProject::LoadProject(saved);
          Monstey_Test(
              replay->Describe()["effective_sha256"] ==
                  history.Describe()["effective_sha256"],
              "Compact inspection changed deterministic project replay");
        });
    test("compact_history_summarizes_only_consumed_opcode_fields", [&] {
      Monstey_AnimationProject history(source, hash);
      history.Edit(J::array({J::object({{"op", "rate"},
                                        {"clip", "walk"},
                                        {"factor", 1.0},
                                        {"name", 123}})}));
      history.Edit(
          J::array({J::object({{"op", "held"},
                               {"source", "walk"},
                               {"name", "SummaryHeld"},
                               {"clip", std::string(2 * 1024 * 1024, 'x')}})}));
      const auto full = history.Changes();
      const auto compact = history.Changes(J::object(
          {{"include_operations", false}, {"offset", 0}, {"limit", 100}}));
      Monstey_Test(
          compact["steps"][0]["first_clip"] == J("walk") &&
              compact["steps"][1]["first_clip"] == J("SummaryHeld") &&
              compact["steps"][1]["affected_clip_count"].as_uint() == 1 &&
              compact.dump().size() < 64 * 1024,
          "Ignored opcode fields corrupted or expanded compact history");
      Monstey_Test(
          full["steps"][0]["operations"][0]["name"] == J(123) &&
              full["steps"][1]["operations"][0]["clip"].as_string().size() ==
                  2 * 1024 * 1024 &&
              history.Changes() == full,
          "Compact history changed accepted dense operation payloads");
      const auto saved = out / L"ignored-fields-full.project.json";
      history.SaveProject(saved);
      auto replay = Monstey_AnimationProject::LoadProject(saved);
      Monstey_Test(replay->Describe()["effective_sha256"] ==
                       history.Describe()["effective_sha256"],
                   "Ignored-field history failed deterministic project replay");
    });
    test("history_page_validation_and_empty_pages_are_read_only", [&] {
      Monstey_AnimationProject history(source, hash);
      const auto before = history.Changes();
      const auto Monstey_OwnedRejectionCases1 = J::array({J::object({{"offset", 1}}), J::object({{"offset", -1}}),
                     J::object({{"limit", 0}}), J::object({{"limit", 10001}}),
                     J::object({{"include_operations", 0}}),
                     J::object({{"unknown", true}})}); for (const auto &options : Monstey_OwnedRejectionCases1.as_array())
        reject([&] { (void)history.Changes(options); });
      const auto empty = history.Changes(J::object(
          {{"include_operations", false}, {"offset", 0}, {"limit", 1}}));
      Monstey_Test(
          empty["steps"].empty() && empty["total_steps"].as_uint() == 0 &&
              !empty["has_more"].as_bool() && history.Changes() == before,
          "Empty or rejected history view changed session state");
    });
    if (argc > 2) {
      const std::filesystem::path original(argv[2]);
      const auto originalHash = Monstey_HashSha256(original);
      Monstey_AnimationProject native(original, originalHash);
      test("actual_native_inspect_structural_edit_history_and_project_build",
           [&] {
             actual = native.Describe();
             native.Build(out / L"actual-noop.anix");
             Monstey_Test(Monstey_HashSha256(out / L"actual-noop.anix") ==
                              originalHash,
                          "Actual no-op differs");
             const auto clips = native.Clips();
             std::string selected;
             for (const auto &row : clips["rows"].as_array())
               if (row["name"].as_string() != "bind") {
                 selected = row["name"].as_string();
                 break;
               }
             Monstey_Test(!selected.empty(),
                          "Actual fixture has no editable clip");
             native.Edit(
                 J::array({J::object({{"op", "duplicate"},
                                      {"clip", selected},
                                      {"name", "Monstey_Project_Copy"}})}));
             const auto p = out / L"actual.project.json";
             native.SaveProject(p);
             auto replay = Monstey_AnimationProject::LoadProject(p);
             native.Build(out / L"actual-new.anix");
             replay->Build(out / L"actual-replay.anix");
             Monstey_Test(
                 Monstey_ReadFile(out / L"actual-new.anix") ==
                         Monstey_ReadFile(out / L"actual-replay.anix") &&
                     Monstey_HashSha256(original) == originalHash,
                 "Actual animation project/source changed");
           });
    }
    const auto report = J::object({{"status", "PASS"},
                                   {"groups", cases.size()},
                                   {"cases", cases},
                                   {"actual", actual},
                                   {"python_product_runtime", false},
                                   {"game_written", false}});
    Monstey_WriteNewText(out / L"animation-project-tests.json", report.dump(2));
    std::cout << report.dump() << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
