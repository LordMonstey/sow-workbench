#include "Monstey_Core.hpp"
#include "Monstey_NavigationStreaming.hpp"
#include <bit>
#include <iostream>
#include <random>
using namespace Monstey;
namespace {
unsigned Monstey_Groups = 0;
void Monstey_Check(bool value, const char *text) {
  Monstey_Require(value, text);
}
template <class F> void Monstey_Reject(F action) {
  bool failed = false;
  try {
    action();
  } catch (const Monstey_Error &) {
    failed = true;
  }
  Monstey_Check(failed, "Expected owned-range rejection");
  ++Monstey_Groups;
}
Monstey_NavigationCutterSection Monstey_Section(std::uint32_t uid) {
  Monstey_NavigationCutterSection section;
  section.mesh_uid = uid;
  section.cutter_present = true;
  section.face_flags = {1, 2, 8, 16};
  return section;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2, "Usage: NEW_REPORT");
    const std::filesystem::path output = argv[1];
    Monstey_RequireOfflineOutput(output);
    for (const auto [raw, next] :
         std::array<std::pair<std::uint32_t, std::uint32_t>, 8>{
             {{0, 1},
              {1, 2},
              {0x7ffffffe, 0x7fffffff},
              {0x7fffffff, 1},
              {0x80000000, 1},
              {0xfffffffe, 1},
              {0xffffffff, 0},
              {123, 124}}}) {
      const auto result = Monstey_NavigationAdvanceUid(raw);
      Monstey_Check(result.assigned == raw && result.next == next,
                    "Raw UID assignment/wrap changed");
    }
    ++Monstey_Groups;
    std::vector<std::optional<std::uint32_t>> uids = {std::nullopt, 7, 9, 7,
                                                      0xffffffff};
    Monstey_Check(Monstey_NavigationFindUid(uids, 5, 7) == 1 &&
                      Monstey_NavigationFindUid(uids, 5, 9) == 2 &&
                      Monstey_NavigationFindUid(uids, 5, 0xffffffff) == 4 &&
                      Monstey_NavigationFindUid(uids, 5, 17) == -1,
                  "UID first nonnull/raw equality lookup");
    ++Monstey_Groups;
    Monstey_Check(Monstey_NavigationFindUid({}, 0, 7) == -1 &&
                      Monstey_NavigationFindUid({}, -1, 7) == -1 &&
                      Monstey_NavigationFindUid({}, INT32_MIN, 7) == -1 &&
                      Monstey_NavigationFindUid(uids, 1, 7) == -1,
                  "Signed counts/prefix handling");
    ++Monstey_Groups;
    Monstey_Reject([&] { (void)Monstey_NavigationFindUid(uids, 6, 7); });
    std::vector<Monstey_NavigationCutterSection> sections = {
        Monstey_Section(7), Monstey_Section(9)};
    sections[0].face_ids = {99};
    sections[0].changed_faces = {99};
    sections[0].bounds.push_back({99, 99, 99, 99, 99, 99, 99, 99});
    sections[1].face_ids = {3};
    sections[1].changed_faces = {3};
    Monstey_NavigationStreamSet set;
    set.uid_a = 7;
    set.uid_b = 9;
    set.connections = {{1, 101, 2, 202}, {1, 303, 3, 404}};
    set.bounds_a.push_back({-1, -2, -3, -4, 1, 2, 3, 4});
    set.bounds_b.push_back({-5, -6, -7, -8, 5, 6, 7, 8});
    const auto forward =
        Monstey_NavigationStageStreamSets(sections, 0, std::span(&set, 1));
    Monstey_Check(
        forward.sections[0].face_ids == std::vector<std::uint32_t>{1, 1} &&
            forward.sections[1].face_ids == std::vector<std::uint32_t>{3, 2, 3},
        "Face lists append, keep duplicates, reset current");
    ++Monstey_Groups;
    Monstey_Check(
        forward.sections[0].changed_faces == std::vector<std::uint32_t>{1} &&
            forward.sections[1].changed_faces == std::vector<std::uint32_t>{3},
        "Changed-face set is current only and unique");
    ++Monstey_Groups;
    Monstey_Check(forward.sections[0].face_flags ==
                          std::vector<std::uint8_t>{1, 6, 8, 16} &&
                      forward.sections[1].face_flags ==
                          std::vector<std::uint8_t>{1, 2, 12, 20},
                  "Face flag OR4 preserves unrelated bits");
    ++Monstey_Groups;
    Monstey_Check(forward.sections[0].bounds == set.bounds_a &&
                      forward.sections[1].bounds == set.bounds_b &&
                      forward.applied.size() == 1 &&
                      forward.applied[0].slot_a == 0 &&
                      forward.applied[0].slot_b == 1,
                  "Side-specific full-lane bounds and slots");
    ++Monstey_Groups;
    Monstey_Check(sections[0].face_ids == std::vector<std::uint32_t>{99} &&
                      sections[0].face_flags ==
                          std::vector<std::uint8_t>{1, 2, 8, 16},
                  "Input mutated");
    ++Monstey_Groups;
    const auto reverse =
        Monstey_NavigationStageStreamSets(sections, 1, std::span(&set, 1));
    Monstey_Check(
        reverse.sections[0].face_ids == std::vector<std::uint32_t>{99, 1, 1} &&
            reverse.sections[1].face_ids == std::vector<std::uint32_t>{2, 3} &&
            reverse.sections[1].changed_faces ==
                std::vector<std::uint32_t>{2, 3} &&
            reverse.sections[0].changed_faces == std::vector<std::uint32_t>{99},
        "Reverse UID orientation keeps A/B semantics");
    ++Monstey_Groups;
    auto later = set;
    later.connections = {{0, 0, 1, 0}};
    std::array two{set, later};
    const auto order = Monstey_NavigationStageStreamSets(sections, 0, two);
    Monstey_Check(order.sections[0].face_ids ==
                          std::vector<std::uint32_t>{0, 1, 1} &&
                      order.sections[0].changed_faces ==
                          std::vector<std::uint32_t>{0, 1} &&
                      order.applied[0].source_set == 1 &&
                      order.applied[1].source_set == 0,
                  "Native reverse descriptor visit and first insertion order");
    ++Monstey_Groups;
    auto unrelated = set;
    unrelated.uid_a = 20;
    unrelated.uid_b = 30;
    unrelated.connections = {{0xffffffff, 0, 0xffffffff, 0}};
    auto missing = unrelated;
    missing.uid_a = 7;
    const std::array skipped{unrelated, missing};
    const auto skip = Monstey_NavigationStageStreamSets(sections, 0, skipped);
    Monstey_Check(skip.applied.empty() && skip.unrelated == 1 &&
                      skip.unresolved == 1 &&
                      skip.sections[0].face_ids.empty() &&
                      skip.sections[1].face_ids == sections[1].face_ids,
                  "Missing/unrelated UID skips descriptor data but current "
                  "cutter is fresh");
    ++Monstey_Groups;
    auto duplicate = sections;
    duplicate.insert(duplicate.begin(), Monstey_Section(9));
    const auto first =
        Monstey_NavigationStageStreamSets(duplicate, 1, std::span(&set, 1));
    Monstey_Check(first.applied[0].slot_b == 0 &&
                      first.sections[2].face_ids == sections[1].face_ids,
                  "Duplicate UID resolves first matching occupied slot");
    ++Monstey_Groups;
    auto self = set;
    self.uid_b = 7;
    self.connections = {{0, 0, 1, 0}};
    const auto alias =
        Monstey_NavigationStageStreamSets(sections, 0, std::span(&self, 1));
    Monstey_Check(
        alias.sections[0].face_ids == std::vector<std::uint32_t>{0, 1} &&
            alias.sections[0].changed_faces == std::vector<std::uint32_t>{0} &&
            alias.sections[0].bounds.size() == 2,
        "Aliased section slots preserve sequential effects");
    ++Monstey_Groups;
    const float qnan = std::bit_cast<float>(0x7fc12345u);
    auto bits = set;
    bits.connections.clear();
    bits.bounds_a[0][3] = qnan;
    bits.bounds_a[0][4] = -0.0f;
    const auto opaque =
        Monstey_NavigationStageStreamSets(sections, 0, std::span(&bits, 1));
    Monstey_Check(std::bit_cast<std::uint32_t>(
                      opaque.sections[0].bounds[0][3]) == 0x7fc12345u &&
                      std::bit_cast<std::uint32_t>(
                          opaque.sections[0].bounds[0][4]) == 0x80000000u,
                  "Bounds lanes are copied without arithmetic/normalization");
    ++Monstey_Groups;
    Monstey_Reject(
        [&] { (void)Monstey_NavigationStageStreamSets(sections, 2, {}); });
    auto bad = sections;
    bad[0].mesh_uid.reset();
    Monstey_Reject(
        [&] { (void)Monstey_NavigationStageStreamSets(bad, 0, {}); });
    bad = sections;
    bad[1].cutter_present = false;
    Monstey_Reject([&] {
      (void)Monstey_NavigationStageStreamSets(bad, 0, std::span(&set, 1));
    });
    auto invalid = set;
    invalid.connections[0].face_a = 0xffffffff;
    Monstey_Reject([&] {
      (void)Monstey_NavigationStageStreamSets(sections, 0,
                                              std::span(&invalid, 1));
    });
    std::mt19937 random(0x59eb7c);
    std::uint32_t cases = 0;
    for (unsigned i = 0; i < 2048; ++i) {
      const std::uint32_t raw = random();
      const auto got = Monstey_NavigationAdvanceUid(raw);
      const auto sum =
          static_cast<std::uint32_t>(static_cast<std::uint64_t>(raw) + 1);
      const auto expected = std::bit_cast<std::int32_t>(sum) < 0 ? 1u : sum;
      Monstey_Check(got.assigned == raw && got.next == expected,
                    "Independent raw32 UID arithmetic");
      ++cases;
    }
    ++Monstey_Groups;
    Monstey_WriteNewText(
        output,
        Monstey_Json::object(
            {{"status", "PASS"},
             {"groups", Monstey_Groups},
             {"uid_random_cases", cases},
             {"input_owned", true},
             {"engine_executed", false},
             {"runtime_verified", false},
             {"scope",
              "Raw UID algebra, first-match lookup and successful-cutter owned "
              "staging; no runtime adjacency application"}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << Monstey_Groups << " groups\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}