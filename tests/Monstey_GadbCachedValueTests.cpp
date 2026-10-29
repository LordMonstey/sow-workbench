#include "Monstey_GadbCachedValue.hpp"
#include <array>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace {
std::uint64_t checks = 0, cases = 0;
void Check(bool p, const char* why) { ++checks; Monstey_Require(p, why); }
template<class F> void Reject(F&& f) { bool threw = false; try { f(); } catch (const std::exception&) { threw = true; } Check(threw, "Expected owned range rejection"); }
Monstey_Json Audit() {
    auto descriptor = Monstey_Json::object({{"hash", "0x5b57a121"}, {"native_type", 11},
                                           {"native_ordinal", 5}, {"count", 1}});
    auto actual = descriptor; actual["count"] = 0;
    auto row = Monstey_Json::object({{"category", "Combat.Buff"}, {"field", "ConditionalBuff"},
        {"record", "0xa8e00467"}, {"classification", "unknown_value_contract"},
        {"physical_index", 4}, {"native_ordinal", 5}, {"mismatches", Monstey_Json::array({"count"})},
        {"first_descriptor", descriptor}, {"actual_descriptor", actual}, {"declaration", descriptor}});
    return Monstey_Json::object({{"format", "Monstey_GadbClassCacheAudit"}, {"version", 1},
        {"status", "unknown"}, {"database_sha256", "synthetic"}, {"diagnostics_truncated", false},
        {"totals", Monstey_Json::object({{"unqualified_value_contracts", 1}, {"unsafe_categories", 0}})},
        {"diagnostics", Monstey_Json::array({row})}});
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        Monstey_Require(argc == 2, "Usage: CachedValueTests NEW-report.json");
        constexpr auto fallback = 0xfedcba9876543210ull;
        // Null record must not consume a descriptor or a value for any selector.
        for (unsigned p = 0; p != 256; ++p)
            for (const auto i : {0u, 1u, 65535u, 65536u, UINT32_MAX}) {
                ++cases; Check(Monstey_GadbReadCachedQword(nullptr, static_cast<std::uint8_t>(p), i, fallback) == fallback,
                               "Null-record raw fallback differs");
            }
        // Exact zero-read domain: storage ends immediately after the count.
        // Offset +14 is absent; all possible physical indices are exercised.
        for (unsigned p = 0; p != 256; ++p) {
            Monstey_Bytes bytes(static_cast<std::size_t>(p) * 16 + 14, 0xa5);
            Monstey_PutU16(bytes, static_cast<std::size_t>(p) * 16 + 12, 0);
            const Monstey_GadbCachedRecordView view{bytes, {}};
            for (const auto i : {0u, 1u, 65535u, 65536u, UINT32_MAX}) {
                ++cases; Check(Monstey_GadbReadCachedQword(&view, static_cast<std::uint8_t>(p), i, fallback) == fallback,
                               "Empty descriptor read an unused offset/value");
            }
        }
        // Independent direct table oracle, all U16 counts, low/high offsets,
        // and indices immediately around the actual exclusive count boundary.
        std::vector<std::uint64_t> values(131070);
        for (std::size_t k = 0; k < values.size(); ++k)
            values[k] = 0x8000000000000000ull ^ (0x0101010101010101ull * k);
        for (const std::uint16_t offset : {std::uint16_t{0}, std::uint16_t{17}, std::uint16_t{65535}})
            for (std::uint32_t count = 0; count != 65536; ++count) {
                Monstey_Bytes bytes(16, 0x5a); Monstey_PutU16(bytes, 12, static_cast<std::uint16_t>(count));
                Monstey_PutU16(bytes, 14, offset); const Monstey_GadbCachedRecordView view{bytes, values};
                const std::array indices{0u, count ? count - 1 : 0u, count, count + 1, UINT32_MAX};
                for (const auto index : indices) {
                    const auto expected = index < count ? values[static_cast<std::size_t>(offset) + index] : fallback;
                    ++cases; Check(Monstey_GadbReadCachedQword(&view, 0, index, fallback) == expected,
                                   "Independent U16 count/U32 index table oracle differs");
                }
            }
        // Owned-range errors happen only at consumed data. Do not invent the
        // native descriptor-index guard which is explicitly absent in evidence.
        {
            Monstey_Bytes bytes(13, 0); Monstey_GadbCachedRecordView view{bytes, {}};
            ++cases; Reject([&] { (void)Monstey_GadbReadCachedQword(&view, 0, 0, fallback); });
            bytes.resize(14); Monstey_PutU16(bytes, 12, 1); view.descriptors = bytes;
            ++cases; Reject([&] { (void)Monstey_GadbReadCachedQword(&view, 0, 0, fallback); });
            ++cases; Check(Monstey_GadbReadCachedQword(&view, 0, 1, fallback) == fallback,
                           "Out-of-count element prematurely read offset");
            bytes.resize(16); Monstey_PutU16(bytes, 14, 65535); view.descriptors = bytes;
            ++cases; Reject([&] { (void)Monstey_GadbReadCachedQword(&view, 0, 0, fallback); });
        }
        // Scope and old report preservation. Every declaration/slot/count
        // precondition is independently perturbed, not globally normalized.
        {
            const auto audit = Audit(); const auto before = audit.dump();
            const auto witness = Monstey_GadbConditionalBuffAbsence(audit); ++cases;
            Check(witness.at("qualified_selected_absences").as_uint() == 1, "Missing selected absence witness");
            Check(witness.at("original_checker_totals").dump() == audit.at("totals").dump(), "Original counters changed");
            Check(audit.dump() == before && witness.at("original_checker_status").as_string() == "unknown",
                   "Original audit mutated or reclassified");
            for (unsigned mutation = 0; mutation != 17; ++mutation) {
                auto altered = Audit(); auto& row = altered["diagnostics"].at(0);
                switch (mutation) {
                case 0: row["category"] = "Other"; break;
                case 1: row["field"] = "Other"; break;
                case 2: row["classification"] = "unsafe_slot"; break;
                case 3: row["physical_index"] = 5; break;
                case 4: row["native_ordinal"] = 4; break;
                case 5: row["mismatches"] = Monstey_Json::array({"type"}); break;
                case 6: row["mismatches"] = Monstey_Json::array({"count", "hash"}); break;
                case 7: row["first_descriptor"]["count"] = 0; break;
                case 8: row["actual_descriptor"]["count"] = 1; break;
                case 9: row["actual_descriptor"]["native_type"] = 10; break;
                case 10: row["first_descriptor"]["native_type"] = 10; break;
                case 11: row["declaration"]["native_type"] = 10; break;
                case 12: row["declaration"]["count"] = 0; break;
                case 13: row["first_descriptor"]["hash"] = "0x00000000"; break;
                case 14: row["actual_descriptor"]["hash"] = "0x00000000"; break;
                case 15: row["declaration"]["hash"] = "0x00000000"; break;
                default: row["actual_descriptor"]["native_ordinal"] = 6; break;
                }
                ++cases; Check(Monstey_GadbConditionalBuffAbsence(altered).at("qualified_selected_absences").as_uint() == 0,
                               "Unproved field contract was qualified");
            }
            auto partial = Audit(); partial["diagnostics_truncated"] = true;
            ++cases; Reject([&] { (void)Monstey_GadbConditionalBuffAbsence(partial); });
        }
        const auto report = Monstey_Json::object({{"status", "PASS"}, {"groups", 5}, {"cases", cases},
            {"assertions", checks}, {"game_assets", false}, {"original_programs_executed", false}});
        Monstey_WriteNewText(argv[1], report.dump(2) + "\n");
        std::cout << "PASS groups=5 cases=" << cases << " assertions=" << checks << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
