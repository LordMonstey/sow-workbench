#include "Monstey_Gadb.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
using namespace Monstey;
namespace
{
using J = Monstey_Json;
J Monstey_LayoutProjection(const Monstey_Gadb &db, std::uint32_t category, std::size_t offset = 0,
                           std::size_t limit = 10000)
{
    const auto page = db.Records(category, {}, offset, limit);
    auto rows = J::array();
    for (const auto &row : page["rows"].as_array())
    {
        const auto metadata = db.Metadata(row["native_reference"].as_string());
        auto fields = J::array();
        for (const auto &f : metadata["fields"].as_array())
            fields.as_array().push_back(J::object({{"hash", f["hash"]},
                                                   {"native_ordinal", f["native_ordinal"]},
                                                   {"native_type", f["native_type"]},
                                                   {"count", f["count"]}}));
        rows.as_array().push_back(J::object({{"reference", metadata["reference"]},
                                             {"selector", metadata["selector"]},
                                             {"name", metadata["name"]},
                                             {"name_hash", metadata["name_hash"]},
                                             {"named", !metadata["anonymous"].as_bool()},
                                             {"anonymous", metadata["anonymous"]},
                                             {"clone", metadata["clone"]},
                                             {"pending_create", metadata["pending_create"]},
                                             {"pending_delete", metadata["pending_delete"]},
                                             {"fields", fields}}));
    }
    return rows;
}
std::uint32_t Monstey_LayoutCategory(const Monstey_Gadb &db, std::string_view name)
{
    const auto page = db.Categories();
    for (const auto &row : page["rows"].as_array())
        if (row["name"].as_string() == name)
            return static_cast<std::uint32_t>(row["index"].as_uint());
    throw Monstey_Error("test", "Category missing");
}
J Monstey_LayoutFind(const J &fields, std::string_view name)
{
    for (const auto &f : fields.as_array())
        if (f["hash"].as_string() == Monstey_Hex(Monstey_NativeHash(name)))
            return f;
    throw Monstey_Error("test", "Field missing");
}
Monstey_Bytes Monstey_LayoutBadStringPointer(Monstey_Bytes raw)
{
    const std::size_t pool = 46u + Monstey_U16(raw, 44);
    const auto descriptors = pool + Monstey_U32(raw, 8);
    std::size_t at = descriptors + static_cast<std::size_t>(Monstey_U32(raw, 28)) * 15 + 8;
    bool changed = false;
    for (std::uint32_t ci = 0; ci < Monstey_U32(raw, 12); ++ci)
    {
        const auto anon = Monstey_U32(raw, at + 12), named = Monstey_U32(raw, at + 16);
        at += 24;
        for (std::uint32_t ri = 0; ri < anon + named; ++ri)
        {
            if (ri >= anon)
                at += 4;
            const auto packed = Monstey_U32(raw, at), flags = Monstey_U32(raw, at + 4);
            at += 8;
            const auto start = ((flags & 65535u) << 8) | (packed >> 24), count = (packed >> 16) & 255u;
            for (std::uint32_t di = 0; di < count; ++di)
            {
                const auto d = descriptors + static_cast<std::size_t>(start + di) * 15;
                if (Monstey_U32(raw, d) == Monstey_NativeHash("String") && raw[d + 10] == 4)
                {
                    Monstey_PutU32(raw, at + static_cast<std::size_t>(Monstey_U16(raw, d + 13)) * 4,
                                   UINT32_MAX);
                    changed = true;
                }
            }
            at += static_cast<std::size_t>(packed & 65535u) * 4;
        }
    }
    Monstey_Require(changed, "String fixture not found");
    return raw;
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        static_assert(sizeof(Monstey_Gadb) == sizeof(void *), "The class gained an ABI data member");
        Monstey_Require(argc == 5, "Arguments: new_results synthetic.gamedb schema.json names.gamedb");
        const std::filesystem::path out(argv[1]), source(argv[2]), schema(argv[3]), names(argv[4]);
        Monstey_Require(!std::filesystem::exists(out), "Output exists");
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"}, {"engine_executed", false}, {"groups", J::array()}});
        const auto test = [&](const char *name, const std::function<void()> &action) {
            try
            {
                action();
            }
            catch (const std::exception &e)
            {
                throw Monstey_Error("test", std::string(name) + ": " + e.what());
            }
            report["groups"].as_array().push_back(name);
        };
        const auto reject = [](const std::function<void()> &action) {
            bool bad = false;
            try
            {
                action();
            }
            catch (const std::exception &)
            {
                bad = true;
            }
            Monstey_Require(bad, "Expected rejection");
        };
        const auto sourceHash = Monstey_HashSha256(source), namesHash = Monstey_HashSha256(names);
        test("all_native_types_match_legacy_descriptor_projection_and_preserve_metadata", [&] {
            Monstey_Gadb db(source, schema);
            const auto categories = db.Categories();
            std::set<unsigned> types;
            for (const auto &c : categories["rows"].as_array())
            {
                const auto ci = static_cast<std::uint32_t>(c["index"].as_uint());
                const auto before = Monstey_LayoutProjection(db, ci);
                const auto state = db.Changes();
                const auto page = db.DescriptorLayouts(ci, 0, 10000);
                Monstey_Require(page["rows"] == before && Monstey_LayoutProjection(db, ci) == before &&
                                    db.Changes() == state,
                                "Capture changed legacy fields or state");
                Monstey_Require(!page["values_decoded"].as_bool() &&
                                    page["published_order_provenance"].as_string() == "native_source" &&
                                    !page["publication_plan_computed"].as_bool(),
                                "Native source provenance wrong");
                for (const auto &row : page["rows"].as_array())
                    for (const auto &f : row["fields"].as_array())
                    {
                        types.insert(static_cast<unsigned>(f["native_type"].as_uint()));
                        Monstey_Require(f.size() == 4 && !f.contains("value") &&
                                            !f.contains("original_value"),
                                        "Value/schema materialization leaked");
                    }
            }
            Monstey_Require(
                types == std::set<unsigned>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 68, 99}),
                "Native type coverage incomplete");
        });
        test("pagination_zero_limit_extreme_offset_and_bad_limits", [&] {
            Monstey_Gadb db(source, schema);
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            const auto full = db.DescriptorLayouts(ci, 0, 10000);
            for (std::size_t offset = 0; offset < full["total"].as_uint() + 2; ++offset)
                Monstey_Require(db.DescriptorLayouts(ci, offset, 1)["rows"] ==
                                    Monstey_LayoutProjection(db, ci, offset, 1),
                                "Page differs");
            Monstey_Require(
                db.DescriptorLayouts(ci, 0, 0)["rows"].empty() &&
                    db.DescriptorLayouts(ci, static_cast<std::size_t>(INT64_MAX), 10000)["rows"].empty(),
                "Page size/offset overflow");
            reject([&] { (void)db.DescriptorLayouts(ci, SIZE_MAX, 10000); });
            reject([&] { (void)db.DescriptorLayouts(UINT32_MAX); });
            reject([&] { (void)db.DescriptorLayouts(ci, 0, 10001); });
        });
        test("raw_utf8_and_anonymous_hashes_match_existing_native_identity", [&] {
            Monstey_Gadb db(names);
            const auto page = db.DescriptorLayouts(0, 0, 10000);
            Monstey_Require(page["rows"] == Monstey_LayoutProjection(db, 0), "Raw identity differs");
            bool raw = false, anonymous = false;
            for (const auto &row : page["rows"].as_array())
            {
                if (row["anonymous"].as_bool())
                {
                    anonymous = true;
                    Monstey_Require(row["name_hash"].is_null(), "Anonymous name invented");
                }
                else if (row["name_hash"].as_string() !=
                         Monstey_Hex(Monstey_NativeHash(row["name"].as_string())))
                    raw = true;
            }
            Monstey_Require(raw && anonymous, "Raw/anonymous control missing");
        });
        test("malformed_value_atom_is_never_decoded_by_layout_capture", [&] {
            const auto bad = out / L"bad-string-pointer.gamedb";
            Monstey_WriteNewFile(bad, Monstey_LayoutBadStringPointer(Monstey_ReadFile(source)));
            Monstey_Gadb db(bad, schema), original(source, schema);
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            reject([&] { (void)db.Metadata("Mixed:Alpha"); });
            Monstey_Require(db.DescriptorLayouts(ci, 0, 10000)["rows"] ==
                                original.DescriptorLayouts(ci, 0, 10000)["rows"],
                            "Layout attempted value decode");
        });
        test("editor_metadata_only_preserves_native_order_provenance", [&] {
            Monstey_Gadb db(source, schema);
            db.SetProjectMetadata(J::object({{"view", "layout"}}));
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            const auto page = db.DescriptorLayouts(ci);
            Monstey_Require(!page["has_pending_native_changes"].as_bool() &&
                                page["published_order_provenance"].as_string() == "native_source",
                            "Metadata-only view changed native provenance");
        });
        test("pending_clones_renames_deletes_stay_project_snapshots_without_final_sort", [&] {
            Monstey_Gadb db(source, schema);
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            db.CloneRecord("Mixed:Alpha", "LayoutClone");
            db.RenameRecord("Mixed:Omega", "Monstey_Renamed");
            db.DeleteRecord("Mixed:Unused");
            db.MoveField("Monstey_LayoutClone", "Scalar", 0);
            const auto state = db.Changes();
            const auto page = db.DescriptorLayouts(ci, 0, 10000);
            Monstey_Require(page["rows"] == Monstey_LayoutProjection(db, ci) && db.Changes() == state,
                            "Pending snapshot differs or mutated");
            Monstey_Require(page["record_order"].as_string() == "project_reference_order" &&
                                page["published_order_provenance"].as_string() == "not_computed" &&
                                !page["publication_plan_computed"].as_bool(),
                            "Pending snapshot claims final writer plan");
            bool clone = false, deleted = false;
            for (const auto &row : page["rows"].as_array())
            {
                deleted |= row["pending_delete"].as_bool();
                if (row["clone"].as_bool())
                {
                    clone = true;
                    Monstey_Require(row["name_hash"].as_string() ==
                                        Monstey_Hex(Monstey_NativeHash("Monstey_LayoutClone")),
                                    "Friendly effective identity changed");
                }
            }
            Monstey_Require(clone && deleted, "Pending flags missing");
        });
        test("typed_array_resize_is_not_preemptively_replanned", [&] {
            Monstey_Gadb db(source, schema);
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            db.SetField("Mixed:Alpha", "Many", J::array({"Mixed:Alpha", nullptr, "Mixed:Alpha"}));
            const auto page = db.DescriptorLayouts(ci, 0, 10000);
            bool seen = false;
            for (const auto &row : page["rows"].as_array())
                if (row["selector"].as_string() == Monstey_Hex(db.Resolve("Mixed:Alpha")))
                {
                    seen = true;
                    Monstey_Require(Monstey_LayoutFind(row["fields"], "Many")["count"].as_uint() == 2,
                                    "Capture duplicated writer count planning");
                }
            Monstey_Require(seen && page["published_order_provenance"].as_string() == "not_computed",
                            "Resize provenance wrong");
            const auto built = out / L"resized.gamedb";
            db.Build(built);
            Monstey_Gadb parsed(built, schema);
            const auto native = parsed.DescriptorLayouts(Monstey_LayoutCategory(parsed, "Mixed"), 0, 10000);
            bool count = false;
            for (const auto &row : native["rows"].as_array())
                if (row["name"].as_string() == "Alpha")
                    count = Monstey_LayoutFind(row["fields"], "Many")["count"].as_uint() == 3;
            Monstey_Require(count && native["published_order_provenance"].as_string() == "native_source",
                            "Actual compiled layout not captured");
        });
        test("new_in_memory_records_are_not_claimed_as_published_source", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ci =
                static_cast<std::uint32_t>(db->CreateCategory("Mixed")["category"]["index"].as_uint());
            const auto ref = db->CreateRecord(ci, "Monstey_New")["record"]["selector"].as_string();
            db->AddField(ref, "Count", 3, 5);
            const auto page = db->DescriptorLayouts(ci);
            Monstey_Require(page["rows"] == Monstey_LayoutProjection(*db, ci) &&
                                page["published_order_provenance"].as_string() == "not_computed" &&
                                page["rows"][0u]["pending_create"].as_bool(),
                            "Fresh record provenance wrong");
        });
        test("undo_redo_and_build_do_not_gain_capture_mutations", [&] {
            Monstey_Gadb db(source, schema);
            const auto ci = Monstey_LayoutCategory(db, "Mixed");
            const auto clean = db.DescriptorLayouts(ci, 0, 10000);
            db.MoveField("Mixed:Alpha", "Scalar", 0);
            const auto changed = db.DescriptorLayouts(ci, 0, 10000);
            db.Undo();
            Monstey_Require(db.DescriptorLayouts(ci, 0, 10000) == clean, "Undo layout/provenance changed");
            db.Redo();
            Monstey_Require(db.DescriptorLayouts(ci, 0, 10000) == changed, "Redo layout/provenance changed");
            db.Revert();
            const auto file = out / L"identity.gamedb";
            db.Build(file);
            Monstey_Require(Monstey_ReadFile(file) == Monstey_ReadFile(source),
                            "Capture changed native identity build");
        });
        test("changed_source_is_rejected_before_raw_snapshot", [&] {
            const auto copy = out / L"changed-source.gamedb";
            const auto bytes = Monstey_ReadFile(source);
            Monstey_WriteNewFile(copy, bytes);
            Monstey_Gadb db(copy, schema);
            std::filesystem::resize_file(copy, bytes.size() + 1);
            reject([&] { (void)db.DescriptorLayouts(0); });
        });
        Monstey_Require(Monstey_HashSha256(source) == sourceHash && Monstey_HashSha256(names) == namesHash,
                        "Synthetic input changed");
        report["group_count"] = report["groups"].size();
        report["inputs_unchanged"] = true;
        Monstey_WriteNewText(out / L"report.json", report.dump(2) + "\n");
        std::cout << report.dump(2) << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
