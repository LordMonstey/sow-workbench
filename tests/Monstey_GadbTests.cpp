#include "Monstey_Gadb.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace Monstey
{
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
struct Field
{
    std::string name;
    std::uint8_t type;
    std::uint16_t count, offset;
};
struct Row
{
    std::string name;
    bool named;
    std::vector<std::uint32_t> words;
    std::vector<Field> fields;
};
struct Cat
{
    std::string name;
    std::vector<Row> rows;
};
B Monstey_TestFixture()
{
    B pool(4);
    auto bytes = [&](B raw) {
        const auto at = static_cast<std::uint32_t>(pool.size());
        pool.insert(pool.end(), raw.begin(), raw.end());
        while (pool.size() % 4)
            pool.push_back(0);
        return at;
    };
    auto text = [&](std::string value) {
        B raw(value.begin(), value.end());
        raw.push_back(0);
        return bytes(raw);
    };
    const auto string = text("fx\\test\\original.mat"), token = text("DemoToken");
    B wide;
    Monstey_AppendU16(wide, 'A');
    Monstey_AppendU16(wide, 0x03a9);
    Monstey_AppendU16(wide, 0);
    const auto wideAt = bytes(wide);
    auto vector = [&](std::initializer_list<float> values) {
        B raw;
        for (auto v : values)
            Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(v));
        return bytes(raw);
    };
    const auto v2 = vector({1.25f, -2.5f}), v3 = vector({3, 4, 5}), v4 = vector({6, 7, 8, 9});
    auto curve = [&](std::uint8_t type) {
        B raw;
        Monstey_AppendU32(raw, 2);
        for (int i = 0; i < 2; ++i)
        {
            Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(static_cast<float>(i)));
            if (type == 12)
                Monstey_AppendU32(raw, i ? 0xff040506u : 0xff010203u);
            else if (type == 13)
                Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(static_cast<float>(i + 1)));
            else if (type == 15)
                Monstey_AppendU32(raw, static_cast<std::uint32_t>(i ? 2 : -1));
            else
                for (int k = 1; k <= 3; ++k)
                    Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(static_cast<float>(i * 3 + k)));
        }
        return bytes(raw);
    };
    const auto c12 = curve(12), c13 = curve(13), c15 = curve(15), c16 = curve(16);
    std::vector<Field> fields = {
        {"FlagLow", 1, 1, 0},    {"FlagHigh", 1, 1, 35},    {"Signed", 3, 1, 2},       {"Scalar", 2, 1, 3},
        {"String", 4, 1, 4},     {"Enum", 5, 1, 5},         {"WideText", 6, 1, 6},     {"Vector2", 7, 1, 7},
        {"Vector3", 8, 1, 8},    {"Vector4", 9, 1, 9},      {"Anonymous", 10, 1, 10},  {"Null", 10, 1, 11},
        {"Named", 11, 1, 12},    {"Many", 10, 2, 13},       {"Unknown", 99, 1, 15},    {"FlaggedString", 68, 1, 16},
        {"Nonfinite", 2, 1, 17}, {"Empty", 10, 0, 0},       {"ColorCurve", 12, 1, 18}, {"FloatCurve", 13, 1, 19},
        {"IntCurve", 15, 1, 20}, {"VectorCurve", 16, 1, 21}};
    std::vector<Cat> cats = {
        {"Mixed",
         {{"", false, {7}, {{"Count", 3, 1, 0}}},
          {"Alpha",
           true,
           {1,           8, 0xfffffffeu, 0x3fc00000,  string,     token,  wideAt,     v2,  v3,  v4,  0,
            0xffffffffu, 0, 0,           0xffffffffu, 0x12345678, string, 0x7f800000, c12, c13, c15, c16},
           fields},
          {"Omega", true, {}, {}},
          {"Unused", true, {}, {}}}},
        {"Other", {{"OtherRecord", true, {}, {}}}}};
    std::sort(cats.begin(), cats.end(),
              [](const auto &a, const auto &b) { return Monstey_NativeHash(a.name) < Monstey_NativeHash(b.name); });
    std::map<std::string, std::uint32_t> refs;
    for (std::uint32_t ci = 0; ci < cats.size(); ++ci)
    {
        auto &rows = cats[ci].rows;
        std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
            if (a.named != b.named)
                return !a.named;
            return !a.named ? false : Monstey_NativeHash(a.name) < Monstey_NativeHash(b.name);
        });
        for (std::uint32_t ri = 0; ri < rows.size(); ++ri)
            refs[cats[ci].name + ":" + rows[ri].name] = ci << 20 | ri;
    }
    for (auto &c : cats)
        if (c.name == "Mixed")
            for (auto &r : c.rows)
                if (r.name == "Alpha")
                {
                    r.words[10] = refs["Mixed:"];
                    r.words[12] = refs["Mixed:Alpha"];
                    r.words[13] = refs["Mixed:"];
                }
    B descriptors, registry;
    std::uint32_t totalNamed = 0, totalAnonymous = 0, totalWords = 0;
    for (const auto &c : cats)
    {
        const auto ac = static_cast<std::uint32_t>(
            std::count_if(c.rows.begin(), c.rows.end(), [](const auto &r) { return !r.named; }));
        for (auto word : {text(c.name), text(c.name), text(""), ac, static_cast<std::uint32_t>(c.rows.size()) - ac,
                          Monstey_NativeHash(c.name)})
            Monstey_AppendU32(registry, word);
        totalAnonymous += ac;
        totalNamed += static_cast<std::uint32_t>(c.rows.size()) - ac;
        for (const auto &r : c.rows)
        {
            const auto start = static_cast<std::uint32_t>(descriptors.size() / 15);
            std::vector<std::size_t> order;
            for (std::size_t i = 0; i < r.fields.size(); ++i)
                order.push_back(i);
            std::sort(order.begin(), order.end(), [&](auto a, auto b) {
                return Monstey_NativeHash(r.fields[a].name) < Monstey_NativeHash(r.fields[b].name);
            });
            for (std::size_t i = 0; i < r.fields.size(); ++i)
            {
                const auto &f = r.fields[i];
                Monstey_AppendU32(descriptors, Monstey_NativeHash(f.name));
                Monstey_AppendU32(descriptors, Monstey_NativeHash(r.fields[order[i]].name));
                descriptors.push_back(static_cast<std::uint8_t>(i));
                descriptors.push_back(static_cast<std::uint8_t>(order[i]));
                descriptors.push_back(f.type);
                Monstey_AppendU16(descriptors, f.count);
                Monstey_AppendU16(descriptors, f.offset);
            }
            if (r.named)
                Monstey_AppendU32(registry, text(r.name));
            Monstey_AppendU32(registry, ((start & 255u) << 24) | (static_cast<std::uint32_t>(r.fields.size()) << 16) |
                                            static_cast<std::uint32_t>(r.words.size()));
            Monstey_AppendU32(registry, (r.named ? 0x08000000u : 0u) | (start >> 8));
            for (auto word : r.words)
                Monstey_AppendU32(registry, word);
            totalWords += static_cast<std::uint32_t>(r.words.size());
        }
    }
    const std::string path = "synthetic.gamedb";
    B out = {'G', 'A', 'D', 'B'};
    for (auto word :
         {17u, static_cast<std::uint32_t>(pool.size()), static_cast<std::uint32_t>(cats.size()), totalNamed,
          totalAnonymous, totalWords, static_cast<std::uint32_t>(descriptors.size() / 15), 0x12345678u, 0u, 44u})
        Monstey_AppendU32(out, word);
    Monstey_AppendU16(out, static_cast<std::uint16_t>(path.size()));
    out.insert(out.end(), path.begin(), path.end());
    out.insert(out.end(), pool.begin(), pool.end());
    out.insert(out.end(), descriptors.begin(), descriptors.end());
    Monstey_AppendU32(out, 0);
    Monstey_AppendU32(out, static_cast<std::uint32_t>(cats.size()));
    out.insert(out.end(), registry.begin(), registry.end());
    Monstey_AppendU32(out, 0);
    return out;
}
J Monstey_TestSchema()
{
    J fields = J::array();
    auto field = [&](const char *name, int type, J extra = J::object()) {
        extra["name"] = name;
        extra["type_code"] = type;
        fields.as_array().push_back(extra);
    };
    field("FlagLow", 1);
    field("FlagHigh", 1);
    field("Signed", 3, J::object({{"min", "-10"}, {"max", "100"}}));
    field("Scalar", 2, J::object({{"min", "0"}, {"max", "20"}}));
    field("String", 4);
    field("Enum", 5, J::object({{"enum", "EDemo"}, {"enum_values", J::array({"DemoToken", "OtherToken"})}}));
    field("WideText", 6);
    field("Vector2", 7);
    field("Vector3", 8);
    field("Vector4", 9);
    field("Anonymous", 10, J::object({{"target", "Mixed"}}));
    field("Null", 10, J::object({{"target", "Mixed"}}));
    field("Named", 11, J::object({{"target", "Mixed"}, {"array", -1}}));
    field("Many", 10, J::object({{"target", "Mixed"}, {"array", -1}}));
    field("Empty", 10, J::object({{"target", "Mixed"}, {"array", -1}}));
    field("FlaggedString", 68);
    field("Nonfinite", 2);
    field("Count", 3);
    field("ColorCurve", 12);
    field("FloatCurve", 13);
    field("IntCurve", 15);
    field("VectorCurve", 16);
    return J::object(
        {{"categories",
          J::object({{"Mixed", J::object({{"fields", fields}})}, {"Other", J::object({{"fields", J::array()}})}})},
         {"enums", J::object({{"EDemo", J::array({J::object({{"name", "DemoToken"}, {"index", 0}}),
                                                  J::object({{"name", "OtherToken"}, {"index", 1}})})}})}});
}
J Monstey_Field(const J &metadata, std::string_view field)
{
    for (const auto &f : metadata.at("fields").as_array())
        if (f.at("name").as_string() == field)
            return f;
    throw std::runtime_error("Test field missing");
}
} // namespace
int Monstey_RunGadbTests(const std::filesystem::path &directory, const std::filesystem::path &nativeDatabase,
                         const std::filesystem::path &nativeSchema, const std::filesystem::path &nativeFx)
{
    std::filesystem::create_directories(directory);
    const auto source = directory / "synthetic.gamedb", schema = directory / "schema.json";
    Monstey_WriteNewFile(source, Monstey_TestFixture());
    Monstey_WriteNewText(schema, Monstey_TestSchema().dump(2));
    const auto sourceHash = Monstey_HashSha256(source);
    std::size_t passed = 0;
    auto check = [&](bool condition, const char *name) {
        Monstey_Require(condition, name);
        ++passed;
        std::cout << "PASS " << name << "\n";
    };
    auto rejected = [&](auto function, const char *name) {
        bool fail = false;
        try
        {
            function();
        }
        catch (const std::exception &)
        {
            fail = true;
        }
        check(fail, name);
    };
    auto inlineSchema = Monstey_TestSchema();
    auto inlineDb = Monstey_Gadb::WithSchema(source, inlineSchema);
    check(inlineDb->Describe().at("schema_inline").as_bool() &&
              inlineDb->Describe().at("schema_path").as_string().empty(),
          "qualified inline schema has no file dependency");
    inlineSchema["categories"].as_object().clear();
    check(inlineDb->Metadata("Mixed:Alpha") == Monstey_Gadb(source, schema).Metadata("Mixed:Alpha"),
          "inline schema owns its field metadata independently of caller lifetime");
    inlineDb->SetField("Mixed:Alpha", "Signed", 5);
    inlineDb->SaveProject(directory / "inline-schema.project.json");
    auto inlineLoaded = Monstey_Gadb::LoadProject(directory / "inline-schema.project.json");
    check(inlineLoaded->Changes() == inlineDb->Changes() &&
              inlineLoaded->Describe().at("schema_sha256") == inlineDb->Describe().at("schema_sha256"),
          "inline project persists qualified schema and canonical checksum");
    inlineLoaded->Build(directory / "inline-schema.gamedb");
    Monstey_Gadb inlineBuilt(directory / "inline-schema.gamedb", schema);
    check(Monstey_Field(inlineBuilt.Metadata("Mixed:Alpha"), "Signed").at("value").as_int() == 5,
          "inline schema uses the unchanged native typed writer");
    rejected([&] { Monstey_Gadb::WithSchema(source, J::object()); }, "invalid inline schema is rejected");
    Monstey_Gadb db(source, schema);
    const auto alpha = db.Metadata("Mixed:Alpha");
    check(db.Describe().at("category_count").as_int() == 2, "bounded native category/record parse");
    check(Monstey_Field(alpha, "WideText").at("value").as_string() == "A\xce\xa9", "UTF16 atom decode");
    check(Monstey_Field(alpha, "FlagHigh").at("value").as_bool(), "packed boolean bit offsets");
    check(!Monstey_Field(alpha, Monstey_Hex(Monstey_NativeHash("Unknown"))).at("editable").as_bool(),
          "unknown native field preserved read-only");
    check(db.Metadata("Mixed:Omega").at("fields").empty(), "empty named record");
    rejected([&] { db.Resolve("Mixed:missing"); }, "missing native binary lookup rejected");
    auto identity = db.Build(directory / "identity.gamedb");
    check(Monstey_ReadFile(source) == Monstey_ReadFile(directory / "identity.gamedb"), "byte exact identity build");
    rejected([&] { db.SetField("Mixed:Alpha", "Signed", 101); }, "native integer maximum");
    rejected([&] { db.SetField("Mixed:Alpha", "FlagLow", 1); }, "strict boolean type");
    rejected([&] { db.SetField("Mixed:Alpha", "Enum", "Missing"); }, "native enum constraints");
    rejected([&] { db.SetField("Mixed:Alpha", "Anonymous", "Other:OtherRecord"); }, "reference category constraints");
    rejected([&] { db.SetField("Mixed:Alpha", "Vector2", J::array({1})); }, "vector dimensions");
    check(!db.Describe().at("dirty").as_bool(), "rejected edits leave project unchanged");
    const J edits = J::object({{"FlagLow", false},
                               {"FlagHigh", false},
                               {"Signed", 17},
                               {"Scalar", 2.5},
                               {"String", "New source string"},
                               {"Enum", "OtherToken"},
                               {"WideText", "Monstey \xc3\xa9\xce\xa9\xf0\x9f\x94\xa5"},
                               {"Vector2", J::array({2.25, 3.5})},
                               {"Vector3", J::array({1, 2, 3})},
                               {"Vector4", J::array({4, 3, 2, 1})},
                               {"Anonymous", "Mixed:Omega"},
                               {"Null", nullptr},
                               {"Named", J::array({"Mixed:Alpha", "Mixed:Omega"})},
                               {"Many", J::array({"Mixed:Omega"})},
                               {"FlaggedString", "new resource path"},
                               {"Nonfinite", 1.5},
                               {"Empty", J::array({"Mixed:Omega"})}});
    for (const auto &[field, value] : edits.as_object())
        db.SetField("Mixed:Alpha", field, value);
    check(db.Describe().at("dirty").as_bool(), "typed edit state");
    db.Undo();
    check(db.Describe().at("can_redo").as_bool(), "undo state");
    db.Redo();
    const auto typed = db.Build(directory / "typed.gamedb");
    check(typed.at("typed_field_readbacks").as_uint() == edits.size() - 1,
          "all scalar/vector/UTF16/reference/list typed readbacks");
    check(typed.at("original_descriptor_prefix_byte_exact").as_bool(), "descriptor prefix retained during list resize");
    check(typed.at("original_atoms_byte_exact").as_bool(), "shared atom copy on write");
    const auto clone = db.CloneRecord("Mixed:Alpha", "NativeClone").at("clone").at("id").as_string();
    db.SetField(clone, "Anonymous", clone);
    db.SetField("Mixed:Alpha", "Named", J::array({clone, "Mixed:Omega"}));
    const auto cloneReport = db.Build(directory / "clone.gamedb");
    check(cloneReport.at("old_record_ordinals_unchanged").as_bool(), "clone preserves all original ordinals");
    const auto map = J::Parse(Monstey_ReadText(directory / "clone.gamedb.namemap.json"));
    check(map.at("records").at(0).at("name").as_string().starts_with("Monstey_NativeClone_"),
          "native 397 ordered suffix assigned");
    Monstey_Gadb cloneDb(directory / "clone.gamedb", schema);
    const auto &mappedName = map.at("records").at(0).at("name").as_string();
    check(cloneDb.Metadata("Mixed:" + mappedName).at("fields").size() == alpha.at("fields").size(),
          "compiled clone available to native binary search");
    db.SaveProject(directory / "project.json");
    auto loaded = Monstey_Gadb::LoadProject(directory / "project.json");
    check(loaded->Changes() == db.Changes(), "persistent project roundtrip");
    loaded->Build(directory / "reopened.gamedb");
    check(Monstey_ReadFile(directory / "reopened.gamedb") == Monstey_ReadFile(directory / "clone.gamedb"),
          "saved project reproducible native build");
    Monstey_Gadb transformed(source, schema);
    transformed.RenameRecord("Mixed:Omega", "RenamedOmega");
    transformed.DeleteRecord("Mixed:Unused");
    const auto transforms = transformed.Build(directory / "transformed.gamedb");
    check(!transforms.at("old_record_ordinals_unchanged").as_bool(),
          "record transforms explicitly report ordinal remap");
    Monstey_Gadb transformedDb(directory / "transformed.gamedb", schema);
    check(transformedDb.Metadata("Mixed:RenamedOmega").at("name").as_string() == "RenamedOmega",
          "renamed records sorted for native lookup");
    rejected([&] { transformedDb.Resolve("Mixed:Unused"); }, "deleted record absent");
    Monstey_Gadb deletion(source, schema);
    const auto anonymousRef = Monstey_Field(alpha, "Anonymous").at("value").as_string();
    deletion.DeleteRecord(anonymousRef);
    rejected([&] { deletion.Build(directory / "bad-delete.gamedb"); }, "incoming references require deletion policy");
    check(!std::filesystem::exists(directory / "bad-delete.gamedb"), "failed build publishes no partial output");
    deletion.Undo();
    deletion.DeleteRecord(anonymousRef, "0xffffffff");
    deletion.Build(directory / "nullified.gamedb");
    check(true, "explicit nullification deletion");
    rejected([&] { db.Build(source); }, "source overwrite rejected");
    B bad = Monstey_TestFixture();
    bad.resize(45);
    Monstey_WriteNewFile(directory / "truncated.gamedb", bad);
    rejected([&] { Monstey_Gadb malformed(directory / "truncated.gamedb"); }, "truncated header rejected");
    bad = Monstey_TestFixture();
    Monstey_PutU32(bad, 4, 3);
    Monstey_WriteNewFile(directory / "legacy.gamedb", bad);
    rejected([&] { Monstey_Gadb legacy(directory / "legacy.gamedb"); }, "FEAR GADB3 never misparsed as GADB17");
    check(Monstey_HashSha256(source) == sourceHash, "all tests preserve immutable source");
    const auto fxSource = directory / "synthetic.fxp";
    B fxBytes = {'L', 'T', 'F', 'X', 8, 0, 0, 0};
    const auto body = Monstey_TestFixture();
    fxBytes.insert(fxBytes.end(), body.begin(), body.end());
    Monstey_WriteNewFile(fxSource, fxBytes);
    auto inlineFx = Monstey_Gadb::WithSchema(fxSource, Monstey_TestSchema());
    inlineFx->SetField("Mixed:Alpha", "FloatCurve", J::object({{"constant", 321}}));
    inlineFx->Build(directory / "fx-inline-schema.fxp");
    Monstey_Gadb inlineFxBuilt(directory / "fx-inline-schema.fxp", schema);
    check(Monstey_Field(inlineFxBuilt.Metadata("Mixed:Alpha"), "FloatCurve")
                  .at("value")
                  .at("knots")[1]
                  .at("value")
                  .as_number() == 321,
          "inline LTFX schema preserves native curve authoring");
    Monstey_Gadb fx(fxSource, schema);
    const auto fxHash = Monstey_HashSha256(fxSource);
    check(fx.Describe().at("format").as_string() == "LTFX8/GADB17", "native LTFX8 wrapper detection");
    fx.Build(directory / "fx-identity.fxp");
    check(Monstey_ReadFile(directory / "fx-identity.fxp") == fxBytes, "LTFX byte exact identity export");
    check(fx.FxRecords("Mixed").at("total").as_uint() >= 4, "flattened FX record pages");
    check(!fx.Dependencies().at("resources").empty(), "native FX resource dependency fields");
    const auto fxMeta = fx.Metadata("Mixed:Alpha");
    check(Monstey_Field(fxMeta, "ColorCurve").at("value").at("value_type").as_string() == "color",
          "native color curve decoding");
    const J constants = J::object({{"ColorCurve", J(static_cast<std::uint32_t>(0xff110022))},
                                   {"FloatCurve", 3.5},
                                   {"IntCurve", -123},
                                   {"VectorCurve", J::array({7, 8, 9})}});
    for (const auto &[field, value] : constants.as_object())
        fx.SetField("Mixed:Alpha", field, J::object({{"constant", value}}));
    fx.SetField("Mixed:Alpha", "FlagLow", false);
    fx.SetField("Mixed:Alpha", "FlagHigh", false);
    const auto fxReport = fx.Build(directory / "fx-curves.fxp");
    check(fxReport.at("typed_field_readbacks").as_uint() == 6,
          "native color/float/int/vec3 curve readbacks and packed booleans");
    Monstey_Gadb fxReopened(directory / "fx-curves.fxp", schema);
    for (const auto &[field, value] : constants.as_object())
        check(Monstey_Field(fxReopened.Metadata("Mixed:Alpha"), field).at("value").at("knots").at(0).at("value") ==
                  (field == "FloatCurve"    ? J(3.5)
                   : field == "VectorCurve" ? J::array({7.0, 8.0, 9.0})
                                            : value),
              "compiled native curve value");
    rejected([&] { fx.SetField("Mixed:Alpha", "FloatCurve", J::object({{"knots", J::array()}})); },
             "empty FX curve edit rejected");
    rejected([&] { fx.SetField("Mixed:Alpha", "ColorCurve", J::object({{"constant", -1}})); },
             "negative ARGB curve rejected");
    rejected([&] { fx.SetField("Mixed:Alpha", "VectorCurve", J::object({{"constant", J::array({1, 2})}})); },
             "wrong FX vector curve dimensions rejected");
    rejected(
        [&] {
            fx.SetField("Mixed:Alpha", "FloatCurve",
                        J::array({J::object({{"time", 1}, {"value", 1}}), J::object({{"time", 0}, {"value", 2}})}));
        },
        "unsorted FX curve times rejected");
    fx.SaveProject(directory / "fx-project.json");
    auto savedFx = Monstey_Gadb::LoadProject(directory / "fx-project.json");
    savedFx->Build(directory / "fx-project.fxp");
    check(Monstey_ReadFile(directory / "fx-project.fxp") == Monstey_ReadFile(directory / "fx-curves.fxp"),
          "native FX project reproducible export");
    check(Monstey_HashSha256(fxSource) == fxHash, "native FX source untouched");
    // Independent authored structures: no source record or template is involved.
    auto fresh = Monstey_Gadb::New();
    check(fresh->Describe().at("category_count").as_int() == 0 && fresh->Describe().at("source").as_string().empty(),
          "logical empty database has no backing file");
    fresh->Build(directory / "empty.gamedb");
    const auto emptyBytes = Monstey_ReadFile(directory / "empty.gamedb");
    check(emptyBytes.size() == 58 && Monstey_U32(emptyBytes, 12) == 0 && Monstey_U32(emptyBytes, 40) == 44 &&
              Monstey_U32(emptyBytes, 50) == 0,
          "independent empty native header and registry layout");
    Monstey_Gadb emptyAgain(directory / "empty.gamedb");
    emptyAgain.Build(directory / "empty-again.gamedb");
    check(Monstey_ReadFile(directory / "empty-again.gamedb") == emptyBytes, "empty native source identity export");
    std::string firstCategory = "AuthoredA", secondCategory = "AuthoredB";
    if (Monstey_NativeHash(firstCategory) < Monstey_NativeHash(secondCategory))
        std::swap(firstCategory, secondCategory);
    const auto cat0 = fresh->CreateCategory(firstCategory, "First", "test/first").at("category");
    const auto cat1 = fresh->CreateCategory(secondCategory, "Second", "test/second").at("category");
    check(cat0.at("index").as_int() == 0 && cat1.at("index").as_int() == 1 && cat0.at("pending_create").as_bool(),
          "project category indices stay stable before native hash sorting");
    const auto a = fresh->CreateRecord(0, "Authored").at("record").at("selector").as_string();
    const auto target = fresh->CreateRecord(1, "Target").at("record").at("selector").as_string();
    const auto anonymous = fresh->CreateRecord(0, "", true).at("record").at("selector").as_string();
    check(a == "0x00000000" && anonymous == "0x00000001" && fresh->Metadata(anonymous).at("anonymous").as_bool(),
          "independent named and anonymous records allocate real stable references");
    fresh->AddField(a, "Integer", 3, 42);
    fresh->AddField(a, "Text", 4, "Independent string");
    fresh->AddField(a, "Wide", 6, "Unicode \xce\xa9\xf0\x9f\x94\xa5");
    fresh->AddField(a, "Vector", 8, J::array({1.25, 2.5, -3}));
    fresh->AddField(a, "Target", 10, target);
    fresh->AddField(a, "Targets", 11, J::array({target, anonymous, nullptr}));
    fresh->AddField(a, "Numbers", 3, J::array({1, 2, 3}), J::object({{"array", -1}}));
    fresh->AddField(a, "Fixed", 2, J::array({1.5, 2.5}), J::object({{"array", 2}}));
    fresh->AddField(a, "FirstFlag", 1, true);
    fresh->AddField(a, "SecondFlag", 1, false);
    fresh->AddField(a, "Curve", 13, J::object({{"constant", 1.25}}));
    fresh->AddField(a, "Discard", 4, "unused");
    fresh->AddField(anonymous, "Back", 10, a);
    fresh->AddField(target, "Back", 10, a);
    check(Monstey_Field(fresh->Metadata(a), "Text").at("original_value").is_null() &&
              Monstey_Field(fresh->Metadata(a), "Text").at("value").as_string() == "Independent string",
          "authored atom fields have typed values before any atom exists");
    const auto invalidBefore = fresh->Changes();
    rejected([&] { fresh->AddField(a, "Integer", 3, 1); }, "duplicate field hashes rejected transactionally");
    rejected([&] { fresh->AddField(a, "Bad", 14, 0); }, "unsupported native descriptor type rejected");
    rejected([&] { fresh->AddField(a, "BadBool", 1, J::array({true}), J::object({{"array", 1}})); },
             "unqualified packed bool arrays rejected");
    rejected([&] { fresh->AddField(a, "BadVector", 8, J::array({1, 2})); },
             "authored vector dimensions validated before transaction");
    rejected([&] { fresh->CreateCategory(firstCategory); }, "duplicate native category hash rejected");
    rejected([&] { fresh->CreateRecord(0, "Authored"); }, "duplicate native named hash rejected");
    check(fresh->Changes() == invalidBefore, "failed structural commands preserve complete project state");
    std::map<std::uint32_t, std::uint8_t> originalFieldIdentities;
    const auto beforeStructuralFields = fresh->Metadata(a);
    for (const auto &field : beforeStructuralFields.at("fields").as_array())
        originalFieldIdentities.emplace(Monstey_NativeHash(field.at("name").as_string()),
            static_cast<std::uint8_t>(field.at("native_ordinal").as_uint()));
    fresh->DeleteField(a, "Discard");
    fresh->MoveField(a, "SecondFlag", 0);
    check(fresh->Metadata(a).at("fields").at(0).at("name").as_string() == "SecondFlag" &&
              fresh->Metadata(a).at("fields").size() == 11,
          "descriptor delete and reorder retain intended authored order");
    fresh->Undo();
    check(fresh->Metadata(a).at("fields").at(0).at("name").as_string() == "Integer",
          "descriptor move participates in undo history");
    fresh->Redo();
    fresh->SetField(a, "Numbers", J::array({7, 8, 9, 10}));
    const auto copied = fresh->CloneRecord(a, "AuthoredSnapshot").at("clone").at("id").as_string();
    fresh->DeleteField(a, "Text");
    check(Monstey_Field(fresh->Metadata(copied), "Text").at("value").as_string() == "Independent string",
          "clones snapshot authored layouts independently of subsequent source deletion");
    fresh->SaveProject(directory / "structural-project.json");
    auto loadedStructure = Monstey_Gadb::LoadProject(directory / "structural-project.json");
    if (loadedStructure->Changes() != fresh->Changes())
        Monstey_WriteNewText(directory / "structural-loaded-changes.json", loadedStructure->Changes().dump(2));
    check(loadedStructure->Changes() == fresh->Changes(),
          "version2 project preserves categories independent records layouts and clone snapshots");
    const auto structural = fresh->Build(directory / "structural.gamedb");
    loadedStructure->Build(directory / "structural-reopened.gamedb");
    check(Monstey_ReadFile(directory / "structural.gamedb") ==
              Monstey_ReadFile(directory / "structural-reopened.gamedb"),
          "structural project reproducibly builds the same native bytes");
    const auto &refmap = structural.at("reference_map");
    check(refmap.at(a).as_string() == "0x00100001" && refmap.at(anonymous).as_string() == "0x00100000" &&
              refmap.at(target).as_string() == "0x00000000",
          "category hash sorting and anonymous prefix remap project references exactly");
    Monstey_Gadb authored(directory / "structural.gamedb");
    auto authoredField = [&](std::string_view selector, std::string_view name) {
        return Monstey_Field(authored.Metadata(selector), Monstey_Hex(Monstey_NativeHash(name)));
    };
    check(authoredField("First:Authored", "Target").at("value") == refmap.at(target),
          "type10 references remap across reordered categories");
    check(authoredField("First:Authored", "Targets").at("value") ==
              J::array({refmap.at(target), refmap.at(anonymous), nullptr}),
          "type11 reference lists remap named anonymous and null targets");
    check(authoredField(refmap.at(anonymous).as_string(), "Back").at("value") == refmap.at(a),
          "anonymous authored record references remap to sorted named ordinal");
    check(authoredField("Second:Target", "Back").at("value") == refmap.at(a),
          "reverse cross-category references remain coherent");
    check(authoredField("First:Authored", "FirstFlag").at("value").as_bool() &&
              !authoredField("First:Authored", "SecondFlag").at("value").as_bool(),
          "packed booleans survive independent insertions and variable resizing");
    check(authoredField("First:Authored", "Wide").at("value").as_string() == "Unicode \xce\xa9\xf0\x9f\x94\xa5",
          "authored UTF16 surrogate pairs read back exactly");
    check(authoredField("First:Authored", "Numbers").at("value") == J::array({7, 8, 9, 10}),
          "authored variable scalar arrays resize with valid native offsets");
    const auto authoredRaw = Monstey_ReadFile(directory / "structural.gamedb");
    const std::uint32_t atomBase = 46u + Monstey_U16(authoredRaw, 44),
                        descriptorBase = atomBase + Monstey_U32(authoredRaw, 8);
    const auto registry = descriptorBase + static_cast<std::size_t>(Monstey_U32(authoredRaw, 28)) * 15 + 8;
    std::size_t rawAt = registry;
    std::uint32_t priorCategory = 0;
    bool rawLookup = true;
    for (std::uint32_t ci = 0; ci < Monstey_U32(authoredRaw, 12); ++ci)
    {
        const auto hash = Monstey_U32(authoredRaw, rawAt + 20), anonCount = Monstey_U32(authoredRaw, rawAt + 12),
                   namedCount = Monstey_U32(authoredRaw, rawAt + 16);
        rawLookup = rawLookup && (ci == 0 || hash > priorCategory);
        priorCategory = hash;
        rawAt += 24;
        for (std::uint32_t ri = 0; ri < anonCount + namedCount; ++ri)
        {
            if (ri >= anonCount)
                rawAt += 4;
            const auto packed = Monstey_U32(authoredRaw, rawAt), flags = Monstey_U32(authoredRaw, rawAt + 4);
            rawAt += 8;
            const auto dc = (packed >> 16) & 255u, start = ((flags & 65535u) << 8) | (packed >> 24);
            std::uint32_t prior = 0;
            std::set<std::uint8_t> identities;
            for (std::uint32_t di = 0; di < dc; ++di)
            {
                const auto at = descriptorBase + static_cast<std::size_t>(start + di) * 15;
                const auto h = Monstey_U32(authoredRaw, at + 4);
                const auto index = authoredRaw[at + 9];
                rawLookup =
                    rawLookup && identities.insert(authoredRaw[at + 8]).second && index < dc && (di == 0 || h > prior) &&
                    Monstey_U32(authoredRaw, descriptorBase + static_cast<std::size_t>(start + index) * 15) == h;
                const auto publishedRecord = Monstey_Hex(ci << 20 | ri);
                if (publishedRecord == refmap.at(a).as_string() || publishedRecord == refmap.at(Monstey_Hex(fresh->Resolve(copied))).as_string())
                    rawLookup = rawLookup && originalFieldIdentities.at(Monstey_U32(authoredRaw, at)) ==
                        authoredRaw[at + 8];
                prior = h;
            }
            rawAt += static_cast<std::size_t>(packed & 65535u) * 4;
        }
    }
    check(rawLookup && rawAt + 4 == authoredRaw.size(),
          "independent raw parser verifies stable unique field identities sorted-hash index and value boundaries");
    auto legacyDoc = J::Parse(Monstey_ReadText(directory / "inline-schema.project.json"));
    legacyDoc["version"] = 1;
    legacyDoc["changes"].as_object().erase("categories");
    legacyDoc["changes"].as_object().erase("records");
    legacyDoc["changes"].as_object().erase("layouts");
    Monstey_WriteNewText(directory / "legacy-v1.project.json", legacyDoc.dump());
    auto legacy = Monstey_Gadb::LoadProject(directory / "legacy-v1.project.json");
    check(Monstey_Field(legacy->Metadata("Mixed:Alpha"), "Signed").at("value").as_int() == 5,
          "legacy version1 projects remain readable");
    auto malformed = J::Parse(Monstey_ReadText(directory / "structural-project.json"));
    malformed["changes"]["layouts"][0]["payload"]["descriptors"][0][6] = 65535;
    Monstey_WriteNewText(directory / "invalid-structural.project.json", malformed.dump());
    rejected([&] { Monstey_Gadb::LoadProject(directory / "invalid-structural.project.json"); },
             "malformed project field offsets rejected before exposure");
    const auto undoFresh = fresh->Changes();
    fresh->Revert();
    check(fresh->Categories().at("rows").empty(), "revert clears authored categories and records");
    fresh->Undo();
    check(fresh->Changes() == undoFresh, "undo restores a reverted complete authored graph");
    Monstey_Gadb originalStructure(source, schema);
    originalStructure.AddField("Mixed:Alpha", "ExtraFlag", 1, true);
    originalStructure.DeleteField("Mixed:Alpha", "Signed");
    originalStructure.MoveField("Mixed:Alpha", "WideText", 0);
    originalStructure.SetField("Mixed:Alpha", "Many", J::array({"Mixed:Omega", "Mixed:Alpha", "Mixed:Omega"}));
    originalStructure.Build(directory / "original-structure.gamedb");
    Monstey_Gadb originalStructureRead(directory / "original-structure.gamedb", schema);
    check(Monstey_Field(originalStructureRead.Metadata("Mixed:Alpha"), Monstey_Hex(Monstey_NativeHash("ExtraFlag")))
              .at("value")
              .as_bool(),
          "existing source accepts new packed bool without corrupting retained native fields");
    check(Monstey_Field(originalStructureRead.Metadata("Mixed:Alpha"), Monstey_Hex(Monstey_NativeHash("Unknown")))
                  .at("value") == Monstey_Field(alpha, Monstey_Hex(Monstey_NativeHash("Unknown"))).at("value"),
          "structural edits preserve unknown native word payloads");
    check(Monstey_HashSha256(source) == sourceHash,
          "all structural source transformations leave original bytes unchanged");
    auto transaction = Monstey_Gadb::New();
    const auto txBefore = transaction->Changes();
    rejected(
        [&] {
            transaction->Transaction([&](Monstey_Gadb &d) {
                d.CreateCategory("Transaction");
                d.CreateRecord(0, "One");
                d.AddField("Transaction:One", "Value", 3, 2);
                throw std::runtime_error("rollback");
            });
        },
        "multi-record transaction propagates original failure");
    check(transaction->Changes() == txBefore && !transaction->Describe().at("can_undo").as_bool(),
          "failed transaction restores authored state and history");
    transaction->Transaction([&](Monstey_Gadb &d) {
        d.CreateCategory("Transaction");
        d.CreateRecord(0, "One");
        d.AddField("Transaction:One", "Value", 3, 2);
        d.CreateRecord(0, "Two");
        d.AddField("Transaction:Two", "Link", 10, "Transaction:One");
    });
    const auto txAfter = transaction->Changes();
    transaction->Undo();
    check(transaction->Changes() == txBefore && transaction->Describe().at("can_redo").as_bool(),
          "successful graph transaction is exactly one undo step");
    transaction->Redo();
    check(transaction->Changes() == txAfter, "transaction redo restores complete native graph");
    transaction->Undo();
    const auto undoState = transaction->Changes();
    rejected(
        [&] {
            transaction->Transaction([&](Monstey_Gadb &d) {
                d.CreateCategory("Failure");
                throw std::runtime_error("redo-preservation");
            });
        },
        "failed transaction with existing redo is rejected");
    check(transaction->Changes() == undoState && transaction->Describe().at("can_redo").as_bool(),
          "failed transaction preserves existing redo history");
    auto limits = Monstey_Gadb::New();
    limits->CreateCategory("Limits");
    const auto limitRef = limits->CreateRecord(0, "Record").at("record").at("selector").as_string();
    for (std::size_t i = 0; i < 255; ++i)
        limits->AddField(limitRef, "Field" + std::to_string(i), 3, static_cast<int>(i));
    const auto maximumFields = limits->Changes();
    rejected([&] { limits->AddField(limitRef, "Overflow", 3, 1); }, "descriptor count enforces native 8-bit maximum");
    check(limits->Changes() == maximumFields && limits->Metadata(limitRef).at("fields").size() == 255,
          "descriptor overflow preserves maximum valid record");
    limits->MoveField(limitRef, "Field254", 0);
    limits->Build(directory / "descriptor-maximum.gamedb");
    check(true, "255-field record builds with valid sorted lookup");
    auto words = Monstey_Gadb::New();
    words->CreateCategory("Words");
    const auto wordsRef = words->CreateRecord(0, "Record").at("record").at("selector").as_string();
    J manyWords = J::array();
    manyWords.as_array().resize(65535, J(0));
    words->AddField(wordsRef, "Full", 3, manyWords, J::object({{"array", -1}}));
    const auto maximumWords = words->Changes();
    rejected([&] { words->AddField(wordsRef, "Overflow", 3, 1); }, "value words enforce native 16-bit maximum");
    rejected([&] { words->AddField(wordsRef, "OverflowBool", 1, true); },
             "packed bool insertion enforces word-count maximum");
    check(words->Changes() == maximumWords, "word-limit failures preserve authored payload");
    words->Build(directory / "word-maximum.gamedb");
    check(true, "65535-word variable array builds with exact counts");
    auto emptyArray = Monstey_Gadb::New();
    emptyArray->CreateCategory("Array");
    emptyArray->CreateRecord(0, "Record");
    emptyArray->AddField("Array:Record", "Empty", 11, J::array());
    emptyArray->Build(directory / "empty-array.gamedb");
    Monstey_Gadb emptyArrayRead(directory / "empty-array.gamedb");
    check(Monstey_Field(emptyArrayRead.Metadata("Array:Record"), Monstey_Hex(Monstey_NativeHash("Empty")))
              .at("value")
              .is_array(),
          "zero-count reference list has valid zero-word descriptor");
    auto newFx = Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8));
    newFx->CreateCategory("FxGlobalProps");
    newFx->CreateRecord(0, "", true);
    newFx->AddField("0x00000000", "Duration", 2, 2.0);
    newFx->SaveProject(directory / "empty-fx.project.json");
    auto restoredFx = Monstey_Gadb::LoadProject(directory / "empty-fx.project.json");
    check(newFx->Describe().at("format").as_string() == "LTFX8/GADB17" &&
              restoredFx->Describe().at("format") == newFx->Describe().at("format"),
          "independent empty FX project preserves qualified envelope");
    newFx->Build(directory / "new-independent.fxp");
    restoredFx->Build(directory / "new-independent-reopened.fxp");
    check(Monstey_ReadFile(directory / "new-independent.fxp") ==
              Monstey_ReadFile(directory / "new-independent-reopened.fxp"),
          "independent FX graph persists exact native bytes");
    rejected([&] { Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 7)); },
             "truncated FX source prefix rejected");
    auto scalarSchema = Monstey_Gadb::New(schema);
    scalarSchema->CreateCategory("Mixed");
    scalarSchema->CreateRecord(0, "New");
    scalarSchema->AddField("Mixed:New", "FlagLow", 1, true, J::object({{"array", 1}}));
    check(Monstey_Field(scalarSchema->Metadata("Mixed:New"), "FlagLow").at("value").as_bool(),
          "schema array-one remains scalar for packed bool");
    rejected([&] { scalarSchema->AddField("Mixed:New", "Signed", 2, 1.0); },
             "authored type cannot conflict with qualified native schema");
    auto schemaCopy = scalarSchema->Schema();
    schemaCopy["categories"].as_object().clear();
    check(scalarSchema->Schema() == Monstey_TestSchema(), "qualified schema accessor owns a safe independent copy");
    Monstey_Gadb metadataOnly(source, schema);
    const auto tracks =
        J::object({{"fx_tracks", J::array({J::object({{"name", "Primary"}, {"keys", J::array({"0x00000001"})}})})}});
    metadataOnly.SetProjectMetadata(tracks);
    check(metadataOnly.Describe().at("dirty").as_bool() && !metadataOnly.Describe().at("native_dirty").as_bool() &&
              metadataOnly.Describe().at("editor_metadata_dirty").as_bool(),
          "editor metadata dirty state remains distinct from native mutations");
    metadataOnly.Build(directory / "metadata-only.gamedb");
    check(Monstey_ReadFile(directory / "metadata-only.gamedb") == Monstey_ReadFile(source),
          "track metadata alone preserves native database bytes exactly");
    metadataOnly.Undo();
    check(metadataOnly.ProjectMetadata().empty(), "editor metadata participates in undo");
    metadataOnly.Redo();
    check(metadataOnly.ProjectMetadata() == tracks, "editor metadata participates in redo");
    rejected(
        [&] {
            metadataOnly.Transaction([&](Monstey_Gadb &d) {
                d.SetProjectMetadata(J::object({{"temporary", true}}));
                throw std::runtime_error("rollback metadata");
            });
        },
        "metadata transaction failure propagates");
    check(metadataOnly.ProjectMetadata() == tracks, "transaction rollback restores editor metadata");
    metadataOnly.SaveProject(directory / "track-metadata.project.json");
    auto metadataReplay = Monstey_Gadb::LoadProject(directory / "track-metadata.project.json");
    check(metadataReplay->ProjectMetadata() == tracks && metadataReplay->Changes() == metadataOnly.Changes(),
          "editor tracks persist inside the same version2 project");
    rejected([&] { metadataOnly.SetProjectMetadata(J::array()); }, "non-object editor metadata rejected");
    auto authoredNull = Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8));
    authoredNull->CreateCategory("NullGraph");
    authoredNull->CreateRecord(0, "Source");
    authoredNull->AddField("NullGraph:Source", "NullTarget", 10, nullptr);
    authoredNull->AddField("NullGraph:Source", "False", 1, false);
    authoredNull->AddField("NullGraph:Source", "Zero", 3, 0);
    const auto nullClone = authoredNull->CloneRecord("NullGraph:Source", "NullCopy").at("clone").at("id").as_string();
    authoredNull->DeleteRecord("NullGraph:Source", nullClone);
    authoredNull->SaveProject(directory / "authored-null.project.json");
    auto nullReplay = Monstey_Gadb::LoadProject(directory / "authored-null.project.json");
    check(nullReplay->Changes() == authoredNull->Changes(),
          "project replay retains explicit null false and zero authored values including clone snapshots");
    authoredNull->Build(directory / "authored-null.fxp");
    nullReplay->Build(directory / "authored-null-replayed.fxp");
    check(Monstey_ReadFile(directory / "authored-null.fxp") ==
              Monstey_ReadFile(directory / "authored-null-replayed.fxp"),
          "authored null graph deletion replacement persists exact native bytes");
    auto aliasSchema = J::object(
        {{"categories", J::object({{"FxGlobalProps", J::object({{"fields", J::array()}})},
                                   {"BaseFX", J::object({{"parent", ""}, {"fields", J::array()}})},
                                   {"ClientFX.Model", J::object({{"parent", "BaseFX"}, {"fields", J::array()}})},
                                   {"Unrelated", J::object({{"fields", J::array()}})}})},
         {"reference_aliases", J::object({{"FxKey", "BaseFX"}, {"SecondAlias", "FxKey"}})}});
    auto aliasDb = Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8), aliasSchema);
    aliasDb->CreateCategory("FxGlobalProps");
    aliasDb->CreateCategory("ClientFX.Model");
    aliasDb->CreateCategory("Unrelated");
    aliasDb->CreateRecord(0, "", true);
    aliasDb->CreateRecord(1, "", true);
    aliasDb->CreateRecord(2, "Unrelated");
    aliasDb->AddField("0x00000000", "KeyList", 10, J::array({"0x00100000"}),
                      J::object({{"array", -1}, {"target", "FxKey"}}));
    aliasDb->AddField("0x00000000", "AliasLink", 10, "0x00100000", J::object({{"target", "SecondAlias"}}));
    check(Monstey_Field(aliasDb->Metadata("0x00000000"), "KeyList").at("value") == J::array({"0x00100000"}),
          "qualified reference alias retains BaseFX subclass compatibility");
    rejected([&] { aliasDb->SetField("0x00000000", "KeyList", J::array({"Unrelated:Unrelated"})); },
             "qualified alias rejects unrelated native target category");
    aliasDb->SaveProject(directory / "aliased-fx.project.json");
    auto aliasReplay = Monstey_Gadb::LoadProject(directory / "aliased-fx.project.json");
    check(aliasReplay->Schema() == aliasSchema &&
              aliasReplay->Describe().at("schema_sha256") == aliasDb->Describe().at("schema_sha256"),
          "reference aliases persist with exact qualified schema provenance");
    aliasDb->Build(directory / "aliased-fx.fxp");
    aliasReplay->Build(directory / "aliased-fx-replayed.fxp");
    check(Monstey_ReadFile(directory / "aliased-fx.fxp") == Monstey_ReadFile(directory / "aliased-fx-replayed.fxp"),
          "aliased native FX typed graph replays exact bytes");
    auto badAlias = aliasSchema;
    badAlias["reference_aliases"]["FxKey"] = "SecondAlias";
    rejected([&] { Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8), badAlias); },
             "cyclic reference aliases rejected before source exposure");
    badAlias = aliasSchema;
    badAlias["reference_aliases"]["FxKey"] = "Absent";
    rejected([&] { Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8), badAlias); },
             "reference alias cannot invent an absent native category");
    badAlias = aliasSchema;
    badAlias["reference_aliases"]["BaseFX"] = "Unrelated";
    rejected([&] { Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8), badAlias); },
             "reference alias cannot shadow existing native category");
    badAlias = aliasSchema;
    badAlias["reference_aliases"]["FxKey"] = J::array({"BaseFX"});
    rejected([&] { Monstey_Gadb::NewFx(std::span<const std::uint8_t>(fxBytes.data(), 8), badAlias); },
             "malformed alias target type rejected");
    if (!nativeDatabase.empty())
    {
        Monstey_Gadb native(nativeDatabase, nativeSchema);
        const auto original = Monstey_HashSha256(nativeDatabase);
        auto report = native.Build(directory / "native-identity.gamedb");
        check(report.at("sha256").as_string() == original, "real database exact native identity build");
        const auto meta = native.Metadata("Combat.VariableFloat:Float_CurrentHealth");
        check(!meta.at("fields").empty(), "real database named fields and schema");
        if (!nativeSchema.empty())
        {
            native.SetField("Combat.VariableFloat:Float_CurrentHealth", "InitialValue", 123.0);
            native.Build(directory / "native-edited.gamedb");
            check(true, "real database typed candidate compilation");
        }
        native.CreateCategory("Monstey.NativeStructuralProof", "Monstey.NativeStructuralProof",
                              "native/structural-proof");
        const auto nativeCat = static_cast<std::uint32_t>(native.Describe().at("category_count").as_uint() - 1);
        const auto own =
            native.CreateRecord(nativeCat, "Monstey_IndependentRecord").at("record").at("selector").as_string();
        native.AddField(own, "HealthRecord", 10, "Combat.VariableFloat:Float_CurrentHealth");
        native.AddField(own, "Label", 6, "Native \xce\xa9");
        native.AddField("Combat.VariableFloat:Float_CurrentHealth", "Monstey_ProofFlag", 1, true);
        const auto nativeStructural = native.Build(directory / "native-structural.gamedb");
        Monstey_Gadb nativeRead(directory / "native-structural.gamedb", nativeSchema);
        check(nativeRead.Metadata("Monstey.NativeStructuralProof:Monstey_IndependentRecord").at("fields").size() == 2,
              "real source independent category record and fields compile");
        check(nativeStructural.at("typed_reference_words_checked").as_uint() > 0 &&
                  nativeStructural.at("original_descriptor_prefix_byte_exact").as_bool(),
              "real source all typed references remap with unchanged descriptor prefix");
        native.SaveProject(directory / "native-structural.project.json");
        auto nativeReopen = Monstey_Gadb::LoadProject(directory / "native-structural.project.json");
        nativeReopen->Build(directory / "native-structural-reopened.gamedb");
        check(Monstey_ReadFile(directory / "native-structural.gamedb") ==
                  Monstey_ReadFile(directory / "native-structural-reopened.gamedb"),
              "real source structural project reproduces exact candidate bytes");
        check(Monstey_HashSha256(nativeDatabase) == original, "real database source unchanged");
    }
    if (!nativeFx.empty())
    {
        Monstey_Gadb native(nativeFx, nativeSchema);
        const auto original = Monstey_HashSha256(nativeFx);
        const auto fxIdentity = native.Build(directory / "native-fx-identity.fxp");
        check(fxIdentity.at("sha256").as_string() == original, "real LTFX exact identity export");
        const auto rows = native.FxRecords("", 0, 10000).at("rows");
        bool scalar = false, curve = false;
        for (const auto &record : rows.as_array())
        {
            const auto selector = record.at("reference").as_string();
            const auto meta = native.Metadata(selector);
            for (const auto &field : meta.at("fields").as_array())
            {
                if (!field.at("editable").as_bool() || field.at("array").as_bool())
                    continue;
                if (!scalar && field.at("native_type").as_int() == 2)
                {
                    native.SetField(selector, field.at("hash").as_string(),
                                    field.at("value").is_number() && field.at("value").as_number() == 0.5 ? 0.75 : 0.5);
                    scalar = true;
                }
                if (!curve && field.at("native_type").as_int() == 13)
                {
                    native.SetField(selector, field.at("hash").as_string(), J::object({{"constant", 1.25}}));
                    curve = true;
                }
            }
            if (scalar && curve)
                break;
        }
        check(scalar && curve, "real LTFX scalar and curve schema fields");
        const auto candidate = native.Build(directory / "native-fx-edited.fxp");
        check(candidate.at("typed_field_readbacks").as_int() == 2, "real LTFX typed scalar and curve compilation");
        check(Monstey_HashSha256(nativeFx) == original, "real LTFX source unchanged");
    }
    std::cout << "GADB native C++ tests passed: " << passed << "\n";
    return 0;
}
} // namespace Monstey
int wmain(int argc, wchar_t **argv)
{
    try
    {
        const auto base = argc > 1 ? std::filesystem::path(argv[1])
                                   : std::filesystem::temp_directory_path() / L"Monstey-NativeGadbTests";
        const auto output =
            base / (L"run-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        return Monstey::Monstey_RunGadbTests(output,
                                             argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path{},
                                             argc > 3 ? std::filesystem::path(argv[3]) : std::filesystem::path{},
                                             argc > 4 ? std::filesystem::path(argv[4]) : std::filesystem::path{});
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << "\n";
        return 1;
    }
}
