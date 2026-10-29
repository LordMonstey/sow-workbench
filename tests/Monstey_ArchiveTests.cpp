#include "Monstey_Archive.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <random>

using namespace Monstey;
namespace {
unsigned count{};
void Check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class F> void Case(const char *name, F &&f) {
    f();
    ++count;
    std::cout << "PASS " << name << '\n';
}
template <class F> void Reject(F &&f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "expected rejection");
}
void A32(Monstey_Bytes &b, std::uint32_t n, bool be = false) {
    for (unsigned j = 0; j < 4; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * (be ? 3 - j : j))));
}
void A64(Monstey_Bytes &b, std::uint64_t n, bool be = false) {
    for (unsigned j = 0; j < 8; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * (be ? 7 - j : j))));
}
Monstey_Bytes Hex(const std::string &s) {
    Monstey_Bytes b;
    for (std::size_t j = 0; j < s.size(); j += 2)
        b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(j, 2), nullptr, 16)));
    return b;
}
Monstey_Bytes Fixture(unsigned version, const Monstey_Bytes &compressed, std::uint32_t decoded,
                      bool be = false) {
    // Independent one-root, one-file table. No writer/project API is used.
    Monstey_Bytes names{0, 'a', '.', 'b', 'i', 'n', 0};
    auto start = 48ull + names.size() + (version == 3 ? 32 : 30) + 16;
    Monstey_Bytes payload;
    if (decoded) {
        A32(payload, static_cast<std::uint32_t>(compressed.size()), be);
        A32(payload, decoded, be);
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        while (payload.size() % 4)
            payload.push_back(0x58);
    }
    Monstey_Bytes b;
    if (be)
        b = {'R', 'A', 'T', 'L'};
    else
        b = {'L', 'T', 'A', 'R'};
    A32(b, version, be);
    A32(b, static_cast<std::uint32_t>(names.size()), be);
    A32(b, 1, be);
    A32(b, 1, be);
    A32(b, 1, be);
    b.resize(48);
    b.insert(b.end(), names.begin(), names.end());
    A32(b, 1, be);
    A64(b, start, be);
    A64(b, payload.size(), be);
    A64(b, decoded, be);
    if (version == 3)
        A32(b, 9, be);
    else {
        b.push_back(1);
        b.push_back(9);
    }
    A32(b, 0, be);
    A32(b, 0xffffffff, be);
    A32(b, 0xffffffff, be);
    A32(b, 1, be);
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}
} // namespace
int main(int argc, char **argv) {
    try {
        const bool first_archive = argc > 1 && (std::filesystem::path(argv[1]).extension() == ".arch06" ||
                                                std::filesystem::path(argv[1]).extension() == ".arch00" ||
                                                std::filesystem::path(argv[1]).extension() == ".arch05");
        auto base = argc > 1 && !first_archive ? std::filesystem::path(Monstey_Wide(argv[1]))
                                               : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(base);
        auto temp = base / ("Monstey_CppArchiveTests_" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(temp);
        auto put = [&](const std::string &name, const Monstey_Bytes &bytes) {
            auto p = temp / name;
            Monstey_WriteNewFile(p, bytes);
            return p;
        };
        Monstey_Bytes hello;
        const std::string phrase = "hello world";
        for (unsigned j = 0; j < 64; ++j)
            hello.insert(hello.end(), phrase.begin(), phrase.end());
        auto z = Hex("78DACB48CDC9C95728CF2FCA49C918658E32871A13003AC01710");
        auto fixture = put("golden-v3.arch00", Fixture(3, z, 704));
        Case("independent LTAR3 zlib fixed-Huffman fixture", [&] {
            Monstey_Archive a(fixture);
            Check(a.Version() == 3 && a.Entries().size() == 1 && a.Read("A.BIN") == hello,
                  "LTAR3 read mismatch");
        });
        Case("bounded raw RFC1951 stream for native ZIP/NPZ", [&] {
            auto raw = std::span(z).subspan(2, z.size() - 6);
            Check(Monstey_InflateRaw(raw, hello.size()) == hello, "raw DEFLATE mismatch");
            Reject([&] { (void)Monstey_InflateRaw(raw, hello.size() - 1); });
            Reject([&] { (void)Monstey_InflateRaw({}, 1); });
        });
        auto gzip = Hex("1F8B080000000000020ACB48CDC9C95728CF2FCA49C918658E32871A1300236F5152C0020000");
        Case("independent .NET gzip fixture and standard CRC32", [&] {
            Check(Monstey_Crc32(Monstey_Bytes{'1', '2', '3', '4', '5', '6', '7', '8', '9'}) == 0xcbf43926,
                  "CRC32 known vector mismatch");
            Check(Monstey_InflateGzip(gzip) == hello,
                  "gzip native decode differs from independent .NET fixture");
        });
        Case("gzip optional fields and FHCRC are validated", [&] {
            Monstey_Bytes header(gzip.begin(), gzip.begin() + 10);
            header[3] = 30;
            header.insert(header.end(), {3, 0, 1, 2, 3});
            const std::string file = "B.blend", comment = "native";
            header.insert(header.end(), file.begin(), file.end());
            header.push_back(0);
            header.insert(header.end(), comment.begin(), comment.end());
            header.push_back(0);
            auto crc = Monstey_Crc32(header);
            header.push_back(static_cast<std::uint8_t>(crc));
            header.push_back(static_cast<std::uint8_t>(crc >> 8));
            header.insert(header.end(), gzip.begin() + 10, gzip.end());
            Check(Monstey_InflateGzip(header) == hello, "gzip optional header decode failed");
            header[header.size() - gzip.size() + 8] ^= 1;
            Reject([&] { (void)Monstey_InflateGzip(header); });
        });
        Case("gzip CRC ISIZE limits and truncation rejected", [&] {
            auto bad = gzip;
            bad[bad.size() - 8] ^= 1;
            Reject([&] { (void)Monstey_InflateGzip(bad); });
            bad = gzip;
            bad.back() ^= 1;
            Reject([&] { (void)Monstey_InflateGzip(bad); });
            Reject([&] { (void)Monstey_InflateGzip(gzip, hello.size() - 1); });
            bad = gzip;
            bad.pop_back();
            Reject([&] { (void)Monstey_InflateGzip(bad); });
            bad = gzip;
            bad[3] = 0x80;
            Reject([&] { (void)Monstey_InflateGzip(bad); });
        });
        Case("gzip concatenated members explicitly rejected", [&] {
            auto joined = gzip;
            joined.insert(joined.end(), gzip.begin(), gzip.end());
            bool clear = false;
            try {
                (void)Monstey_InflateGzip(joined);
            } catch (const std::exception &e) {
                clear = std::string(e.what()).find("concatenated") != std::string::npos;
            }
            Check(clear, "gzip did not reject unsupported concatenated members explicitly");
        });
        Case("big endian RATL3 fixed-Huffman fixture", [&] {
            Monstey_Archive a(put("big.arch00", Fixture(3, z, 704, true)));
            Check(a.Read("a.bin") == hello, "RATL read mismatch");
        });
        Case("independent dynamic-Huffman .NET zlib fixture", [&] {
            auto compressed = Hex("78DAEDD20701803010C0406DCFDE9496E1DF090E8800721A2EE25B056AD0801674A0070318"
                                  "C10466B080156C6007091C2083024E70811B3C20FCED6F7FFBDBDFFEF6B7BFFDED6F7FFBDB"
                                  "DFFEF6B7BFFDED6F7FFBDBDFFEF6B7BFFDED6F7FFBFB27BF5F10B17CC8");
            Monstey_Archive a(put("dynamic.arch00", Fixture(3, compressed, 8192)));
            auto raw = a.Read("a.bin");
            Check(raw.size() == 8192, "dynamic size");
            for (std::size_t j = 0; j < raw.size(); ++j)
                Check(raw[j] == (j / 32) % 23 + 65, "dynamic data mismatch");
        });
        Case("zlib stored-block fixture", [&] {
            auto tail = Hex("78DA73741C05A360148C8251300A46C148030085EF081F");
            Monstey_Bytes compressed{0x78, 0xda, 0x00, 0xc0, 0x02, 0x3f, 0xfd};
            compressed.insert(compressed.end(), hello.begin(), hello.end());
            compressed.insert(compressed.end(), tail.begin() + 2, tail.end() - 4);
            auto expected = hello;
            expected.insert(expected.end(), 2048, 0x41);
            std::uint32_t aa = 1, bb = 0;
            for (auto c : expected) {
                aa = (aa + c) % 65521;
                bb = (bb + aa) % 65521;
            }
            A32(compressed, (bb << 16) | aa, true);
            Monstey_Archive a(
                put("stored.arch00", Fixture(3, compressed, static_cast<std::uint32_t>(expected.size()))));
            Check(a.Read("a.bin") == expected, "stored mismatch");
        });
        Case("independent LTAR4 raw fixture", [&] {
            Monstey_Archive a(put("raw.arch06", Fixture(4, hello, 704)));
            Check(a.Read("a.bin") == hello, "LTAR4 mismatch");
        });
        Case("independent RATL4 raw fixture", [&] {
            Monstey_Archive a(put("rawbe.arch06", Fixture(4, hello, 704, true)));
            Check(a.Read("a.bin") == hello, "RATL4 mismatch");
        });
        Case("empty native resource", [&] {
            Monstey_Archive a(put("empty.arch06", Fixture(4, {}, 0)));
            Check(a.Read("a.bin").empty(), "empty mismatch");
        });
        Case("bounded resource read", [&] {
            Monstey_Archive a(fixture);
            Reject([&] { (void)a.Read("a.bin", {}, 10); });
        });
        Case("bad zlib Adler32 rejected", [&] {
            auto bad = z;
            bad.back() ^= 1;
            Monstey_Archive a(put("adler.arch00", Fixture(3, bad, 704)));
            Reject([&] { (void)a.Read("a.bin"); });
        });
        Case("zlib trailing byte rejected", [&] {
            auto bad = z;
            bad.push_back(0);
            Monstey_Archive a(put("tail.arch00", Fixture(3, bad, 704)));
            Reject([&] { (void)a.Read("a.bin"); });
        });
        Case("DEFLATE output overflow rejected", [&] {
            Monstey_Archive a(put("overflow.arch00", Fixture(3, z, 703)));
            Reject([&] { (void)a.Read("a.bin"); });
        });
        Case("truncated index rejected", [&] {
            auto bad = Fixture(4, hello, 704);
            bad.resize(60);
            auto p = put("truncated.arch06", bad);
            Reject([&] { Monstey_Archive a(p); });
        });
        Case("out of extent payload rejected", [&] {
            auto bad = Fixture(4, hello, 704);
            Monstey_PutU64(bad, 48 + 7 + 4, ~0ull);
            auto p = put("extent.arch06", bad);
            Reject([&] { Monstey_Archive a(p); });
        });
        Case("folder cycle rejected", [&] {
            auto bad = Fixture(4, hello, 704);
            Monstey_PutU32(bad, 48 + 7 + 30 + 4, 0);
            auto p = put("cycle.arch06", bad);
            Reject([&] { Monstey_Archive a(p); });
        });
        Case("wrong LTAR4 marker rejected", [&] {
            auto bad = Fixture(4, hello, 704);
            bad[48 + 7 + 28] = 0;
            auto p = put("marker.arch06", bad);
            Reject([&] { Monstey_Archive a(p); });
        });
        Case("unsafe virtual paths rejected", [&] {
            for (auto p : {"../a", "/a", "a//b", "a/./b", "a/../b", "a:", "a/"})
                Reject([&] { (void)Monstey_ArchiveVirtual(p); });
        });
        Case("foreign archive entry rejected", [&] {
            Monstey_Archive a(fixture);
            auto e = a.Entries()[0];
            ++e.offset;
            Reject([&] { a.Stream(e, [](auto) {}); });
        });
        Case("new file extraction and overwrite rejection", [&] {
            Monstey_Archive a(fixture);
            auto p = temp / "extracted.bin";
            a.Extract("a.bin", p);
            Check(Monstey_ReadFile(p) == hello, "extract mismatch");
            Reject([&] { a.Extract("a.bin", p); });
            Check(Monstey_ReadFile(p) == hello, "overwritten output");
        });
        Case("partial extraction removed on failure", [&] {
            auto bad = z;
            bad.back() ^= 1;
            Monstey_Archive a(put("badextract.arch00", Fixture(3, bad, 704)));
            auto p = temp / "partial.bin";
            Reject([&] { a.Extract("a.bin", p); });
            Check(!std::filesystem::exists(p), "partial retained");
        });
        auto local = put("input.bin", hello);
        Monstey_ArchiveProject project(fixture);
        Case("lazy archive project and clone-free undo", [&] {
            Check(!project.Dirty() && project.Source() == std::filesystem::absolute(fixture),
                  "project source");
            project.AddFile("folder/file.bin", local);
            Check(project.Dirty() && project.CanUndo() && project.Entries().size() == 2, "add failed");
            Check(project.Undo() && project.Entries().size() == 1 && project.Redo() &&
                      project.Entries().size() == 2,
                  "history failed");
        });
        Case("effective project replacement and renamed original extraction", [&] {
            Monstey_ArchiveProject overlay(fixture);
            Monstey_Bytes replacement{'r', 'e', 'p', 'l', 'a', 'c', 'e', 'd'};
            auto local_path = put("overlay-source.bin", replacement);
            overlay.Replace("a.bin", local_path);
            overlay.Rename("a.bin", "custom/renamed.bin");
            auto target = temp / "overlay-extract.bin";
            overlay.Extract("custom/renamed.bin", target);
            Check(Monstey_ReadFile(target) == replacement,
                  "overlay extracted original instead of replacement");
            Reject([&] { overlay.Extract("a.bin", temp / "missing-overlay.bin"); });
            Reject([&] { overlay.Extract("custom/renamed.bin", target); });
            Check(Monstey_ReadFile(target) == replacement, "overlay overwrote existing target");
            overlay.Undo();
            overlay.Undo();
            overlay.Rename("a.bin", "original-renamed.bin");
            overlay.Extract("original-renamed.bin", temp / "original-renamed-extract.bin");
            Check(Monstey_ReadFile(temp / "original-renamed-extract.bin") == hello,
                  "renamed original lost archive provenance");
        });
        Case("effective project extraction rejects changed local source", [&] {
            Monstey_ArchiveProject overlay;
            auto local_path = put("stale-overlay-source.bin", hello);
            overlay.AddFile("local.bin", local_path);
            std::ofstream change(local_path, std::ios::binary | std::ios::app);
            change.put('!');
            change.close();
            auto target = temp / "stale-overlay-extract.bin";
            Reject([&] { overlay.Extract("local.bin", target); });
            Check(!std::filesystem::exists(target), "stale overlay retained partial output");
        });
        Case("case insensitive collision rejected",
             [&] { Reject([&] { project.AddFile("FOLDER/FILE.BIN", local); }); });
        Case("folder rename and remove undo", [&] {
            project.CreateFolder("empty/deep");
            project.Rename("folder", "renamed");
            auto entries = project.Entries();
            Check(std::any_of(entries.begin(), entries.end(),
                              [](auto &e) { return e.path == "renamed/file.bin"; }),
                  "rename failed");
            project.Remove("renamed");
            Check(project.Entries().size() == 1 && project.Undo() && project.Entries().size() == 2,
                  "remove/undo failed");
        });
        Case("persistent project source validation", [&] {
            auto p = temp / "source.msap";
            project.Save(p);
            auto restored = Monstey_ArchiveProject::Load(p);
            Check(restored.Dirty() && restored.Entries().size() == 2 &&
                      restored.Folders() == project.Folders(),
                  "project reload failed");
        });
        Case("native LTAR4 build full payload readback", [&] {
            auto p = temp / "compiled.arch06";
            auto report = project.Build(p);
            Check(report.readback_verified && report.files == 3 && !report.sha256.empty(),
                  "build proof absent");
            Monstey_Archive a(p);
            Check(a.Read("a.bin") == hello && a.Read("renamed/file.bin") == hello, "compiled bytes differ");
            auto dirs = a.Folders();
            Check(std::find(dirs.begin(), dirs.end(), "empty/deep") != dirs.end(), "empty folders lost");
            auto patch = Monstey_ReadArchivePatchList(a.Read("archivepatchloosefiles.txt"));
            Check(patch.size() == 2, "patch list count");
        });
        Case("empty archive with empty folders", [&] {
            Monstey_ArchiveProject empty;
            empty.CreateFolder("kept/empty");
            auto p = temp / "onlyfolders.arch06";
            auto report = empty.Build(p, false);
            Monstey_Archive a(p);
            Check(report.files == 0 && a.Entries().empty() && a.Folders().size() == 3,
                  "empty archive failed");
        });
        Case("chunk boundaries 65536 and 65537", [&] {
            Monstey_Bytes bytes(65537);
            for (std::size_t j = 0; j < bytes.size(); ++j)
                bytes[j] = static_cast<std::uint8_t>(j * 37);
            Monstey_ArchiveProject boundary;
            boundary.AddFile("boundary.bin", put("boundary-source.bin", bytes));
            auto p = temp / "boundary.arch06";
            (void)boundary.Build(p, false);
            Monstey_Archive a(p);
            Check(a.Read("boundary.bin") == bytes, "chunk boundary data mismatch");
        });
        Case("build output existing preserved", [&] {
            auto p = put("exists.arch06", hello);
            Reject([&] { (void)project.Build(p); });
            Check(Monstey_ReadFile(p) == hello, "existing archive overwritten");
        });
        Case("atomic directory addition and undo", [&] {
            auto source = temp / "tree";
            std::filesystem::create_directories(source / "empty");
            Monstey_WriteNewFile(source / "file", hello);
            Monstey_ArchiveProject directory;
            directory.AddDirectory("import", source);
            Check(directory.Entries().size() == 1 && directory.Undo() && directory.Entries().empty() &&
                      !directory.CanUndo(),
                  "directory addition not one transaction");
        });
        Case("changed local source rejected and output cleaned", [&] {
            auto changing = put("changing.bin", hello);
            Monstey_ArchiveProject stale;
            stale.AddFile("a.bin", changing);
            std::ofstream append(changing, std::ios::binary | std::ios::app);
            append.put('!');
            append.close();
            auto p = temp / "must-not-build.arch06";
            Reject([&] { (void)stale.Build(p); });
            Check(!std::filesystem::exists(p), "failed build retained partial");
        });
        Case("patch-list native hash sorted and verified", [&] {
            auto b = Monstey_BuildArchivePatchList({"characters/model/a.mesh", "weapons/b.skel"});
            auto names = Monstey_ReadArchivePatchList(b);
            Check(names.size() == 2, "patch mismatch");
            auto ns = Monstey_U32(b, 4);
            b[12 + ns] ^= 1;
            Reject([&] { (void)Monstey_ReadArchivePatchList(b); });
        });
        Case("project revert remains undoable", [&] {
            project.Revert();
            Check(!project.Dirty() && project.Entries().size() == 1 && project.Undo() && project.Dirty(),
                  "revert undo failed");
        });
        Monstey_Bytes range_source(6 * 65536 + 17);
        for (std::size_t j = 0; j < range_source.size(); ++j)
            range_source[j] = static_cast<std::uint8_t>(j * 37 + j / 65536);
        Monstey_ArchiveProject range_project;
        range_project.AddFile("large.embb", put("range-source.bin", range_source));
        auto range_path = temp / "range.arch06";
        (void)range_project.Build(range_path, false);
        auto range_archive = std::make_shared<const Monstey_Archive>(range_path);
        Case("archive metadata index getters and lazy range construction", [&] {
            Check(range_archive->IndexLength() < range_archive->Length() && !range_archive->BigEndian(),
                  "index metadata getters wrong");
            Monstey_ArchiveRange view(range_archive, *range_archive->Find("large.embb"));
            auto stats = view.Stats();
            Check(view.Size() == range_source.size() && stats.chunks == 7 && stats.decoded_chunk_loads == 0 &&
                      stats.cached_chunks == 0,
                  "range constructor decoded parent payload");
            Check(stats.metadata_window_reads == 1 && stats.metadata_window_peak <= 4ull * 1024 * 1024 &&
                      stats.metadata_bytes_read == range_archive->Find("large.embb")->stored_size,
                  "range metadata still uses per-chunk seeks or unbounded windows");
        });
        Case("bounded range reads cross chunks without decoding whole parent", [&] {
            Monstey_ArchiveRange view(range_archive, *range_archive->Find("large.embb"));
            auto selected = view.Read(65530, 32);
            Check(std::equal(selected.begin(), selected.end(), range_source.begin() + 65530),
                  "cross-chunk range differs");
            auto stats = view.Stats();
            Check(stats.decoded_chunk_loads == 2 && stats.cached_chunks == 2 &&
                      stats.decoded_chunk_loads < stats.chunks,
                  "range materialized whole parent");
            auto tail = view.Read(range_source.size() - 17, 17);
            Check(std::equal(tail.begin(), tail.end(), range_source.end() - 17),
                  "range EOF boundary differs");
            Reject([&] { (void)view.Read(range_source.size(), 1); });
            Reject([&] { (void)view.Read(0, 64ull * 1024 * 1024 + 1); });
        });
        Case("four-chunk LRU reuses and evicts exact decoded chunks", [&] {
            Monstey_ArchiveRange view(range_archive, *range_archive->Find("large.embb"));
            for (std::uint64_t j = 0; j < 5; ++j)
                (void)view.Read(j * 65536, 1);
            auto first = view.Stats();
            Check(first.decoded_chunk_loads == 5 && first.cached_chunks == 4,
                  "range cache not limited to four chunks");
            (void)view.Read(65536, 1);
            auto hit = view.Stats();
            Check(hit.decoded_chunk_loads == first.decoded_chunk_loads &&
                      hit.cache_hits == first.cache_hits + 1,
                  "LRU did not reuse cached chunk");
            (void)view.Read(0, 1);
            Check(view.Stats().decoded_chunk_loads == 6 && view.Stats().cached_chunks == 4,
                  "LRU did not evict least recently used chunk");
        });
        Case("metadata prefetch crosses stored windows with bounded memory", [&] {
            Monstey_Bytes bytes(70 * 65536 + 17);
            for (std::size_t j = 0; j < bytes.size(); ++j)
                bytes[j] = static_cast<std::uint8_t>(j * 19 + j / 65536);
            Monstey_ArchiveProject project;
            project.AddFile("windows.embb", put("window-source.bin", bytes));
            auto target = temp / "windows.arch06";
            (void)project.Build(target, false);
            auto archive = std::make_shared<const Monstey_Archive>(target);
            Monstey_ArchiveRange view(archive, *archive->Find("windows.embb"));
            auto stats = view.Stats();
            Check(stats.chunks == 71 && stats.metadata_window_reads == 2 &&
                      stats.metadata_window_peak == 4ull * 1024 * 1024 && stats.decoded_chunk_loads == 0,
                  "metadata prefetch did not remain bounded across windows");
            auto selected = view.Read(64 * 65536 - 7, 31);
            Check(std::equal(selected.begin(), selected.end(), bytes.begin() + 64 * 65536 - 7),
                  "stored prefetch window boundary lost a chunk header");
        });
        Case("range shared ownership and concurrent reads preserve bytes", [&] {
            Monstey_ArchiveRange view(range_archive, *range_archive->Find("large.embb"));
            std::vector<std::future<void>> jobs;
            for (std::uint64_t j = 0; j < 4; ++j)
                jobs.push_back(std::async(std::launch::async, [&, j] {
                    auto start = j * 65536 + 17;
                    auto bytes = view.Read(start, 100);
                    Check(std::equal(bytes.begin(), bytes.end(),
                                     range_source.begin() + static_cast<std::ptrdiff_t>(start)),
                          "concurrent range mismatch");
                }));
            for (auto &job : jobs)
                job.get();
            Check(view.Stats().cached_chunks <= 4, "concurrent cache exceeded bound");
        });
        Case("range reuses RFC1950 and big-endian LTAR codecs", [&] {
            auto archive = std::make_shared<const Monstey_Archive>(fixture);
            Monstey_ArchiveRange view(archive, *archive->Find("a.bin"));
            auto bytes = view.Read(9, 20);
            Check(std::equal(bytes.begin(), bytes.end(), hello.begin() + 9), "zlib range differs");
            auto be =
                std::make_shared<const Monstey_Archive>(put("range-big.arch00", Fixture(3, z, 704, true)));
            Check(be->BigEndian() && be->IndexLength() == 103, "big endian index getters wrong");
            Monstey_ArchiveRange big(be, *be->Find("a.bin"));
            Check(big.Read(9, 20) == bytes, "big endian range differs");
        });
        Case("range validates chunk metadata before payload decode", [&] {
            auto raw = Fixture(4, hello, 704);
            auto start = 48 + 7 + 30 + 16;
            Monstey_PutU32(raw, start, 65537);
            auto archive = std::make_shared<const Monstey_Archive>(put("bad-range.arch06", raw));
            Reject([&] { Monstey_ArchiveRange view(archive, *archive->Find("a.bin")); });
        });
        Case("empty archive resource range and changed source rejection", [&] {
            auto archive =
                std::make_shared<const Monstey_Archive>(put("range-empty.arch06", Fixture(4, {}, 0)));
            Monstey_ArchiveRange empty(archive, *archive->Find("a.bin"));
            Check(empty.Size() == 0 && empty.Read(0, 0).empty() && empty.Stats().chunks == 0,
                  "empty range failed");
            auto source = put("range-changing.arch06", Fixture(4, hello, 704));
            auto changing = std::make_shared<const Monstey_Archive>(source);
            Monstey_ArchiveRange view(changing, *changing->Find("a.bin"));
            std::ofstream mutate(source, std::ios::binary | std::ios::app);
            mutate.put('!');
            mutate.close();
            Reject([&] { (void)view.Read(0, 1); });
        });
        Case("game output prohibited", [&] {
            const auto Monstey_FakeGame = temp / "fake-game";
            std::filesystem::create_directory(Monstey_FakeGame);
            const auto Monstey_GameMarker = Monstey_FakeGame / "ShadowOfWar.exe";
            const Monstey_Bytes Monstey_MarkerBytes{'o', 'w', 'n', 'e', 'd'};
            Monstey_WriteNewFile(Monstey_GameMarker, Monstey_MarkerBytes);
            const auto Monstey_ProtectedOutput = Monstey_FakeGame / "Monstey_Test_MustNotExist.arch06";
            bool Monstey_Rejected = false;
            try {
                (void)project.Build(Monstey_ProtectedOutput);
            } catch (const Monstey_Error &e) {
                Monstey_Rejected = std::string(e.what()) ==
                                   "Write an offline output outside the game installation";
            }
            Check(Monstey_Rejected, "game output protection did not reject the synthetic installation");
            Check(!std::filesystem::exists(Monstey_ProtectedOutput), "game output created");
            Check(Monstey_ReadFile(Monstey_GameMarker) == Monstey_MarkerBytes,
                  "synthetic game marker changed");
        });
        const int native_arg = first_archive ? 1 : (argc > 2 ? 2 : 0);
        if (native_arg) {
            auto archive = std::filesystem::path(Monstey_Wide(argv[native_arg]));
            auto dll = argc > native_arg + 1 ? std::filesystem::path(Monstey_Wide(argv[native_arg + 1]))
                                             : std::filesystem::path{};
            Case("native shipped archive read-only index and chunks", [&] {
                auto before = Monstey_HashSha256(archive);
                Monstey_Archive native(archive);
                std::size_t n{};
                for (auto &e : native.Entries()) {
                    if (e.size > 1024 * 1024)
                        continue;
                    auto raw = native.Read(e.path, dll);
                    auto range_owner = std::make_shared<const Monstey_Archive>(archive);
                    Monstey_ArchiveRange view(range_owner, *range_owner->Find(e.path), dll);
                    Check(view.Stats().decoded_chunk_loads == 0, "native range eagerly decoded payload");
                    auto range_count = std::min<std::uint64_t>(23, e.size);
                    auto selected = view.Read(e.size - range_count, range_count);
                    Check(std::equal(selected.begin(), selected.end(),
                                     raw.end() - static_cast<std::ptrdiff_t>(range_count)),
                          "native Oodle range differs from streamed payload");
                    Check(raw.size() == e.size, "native size");
                    std::cout << "NATIVE " << e.path << " " << Monstey_HashSha256(raw) << '\n';
                    if (++n == 4)
                        break;
                }
                Check(n > 0, "native no bounded samples");
                Check(before == Monstey_HashSha256(archive), "native source changed");
                std::cout << "NATIVE_INDEX " << native.Entries().size() << " " << native.Folders().size()
                          << '\n';
            });
        }
        std::cout << "RESULT " << count << " tests passed; synthetic outputs " << temp.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
