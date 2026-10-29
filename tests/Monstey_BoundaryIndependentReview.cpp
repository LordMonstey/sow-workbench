#include "Monstey_FearAudit.hpp"
#include "Monstey_NavigationBoundaryQuery.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <immintrin.h>
#include <iostream>
#include <limits>
#include <random>
using namespace Monstey;
namespace {
using V = Monstey_NavigationVector4;
using A = Monstey_NavigationAabb;
using T = Monstey_NavigationAffine4;
using Bytes = Monstey_Bytes;
using J = Monstey_Json;
unsigned groups = 0, treeCases = 0, slabCases = 0, localCases = 0, worldCases = 0,
         selectorCases = 0, orderedCases = 0, nativeChecks = 0;
void Check(bool ok, std::string_view name) {
    Monstey_Require(ok, name);
    ++groups;
    std::cout << "PASS " << name << '\n';
}
void Require(bool ok, std::string_view name) { Monstey_Require(ok, name); }
void Reject(const std::function<void()> &fn, std::string_view name) {
    try {
        fn();
    } catch (const std::exception &) {
        Check(true, name);
        return;
    }
    throw std::runtime_error("Expected rejection: " + std::string(name));
}
float Add(float x, float y) {
    volatile float n = x + y;
    return n;
}
float Sub(float x, float y) {
    volatile float n = x - y;
    return n;
}
float Mul(float x, float y) {
    volatile float n = x * y;
    return n;
}
float SignFlip(float x) {
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) ^ 0x80000000u);
}
float Abs(float x) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(x) & 0x7fffffffu); }
float Min(float x, float y) { return x < y ? x : y; }
float Max(float x, float y) { return x > y ? x : y; }
bool Equal(const V &a, const V &b) {
    for (unsigned i = 0; i < 4; ++i)
        if (std::bit_cast<std::uint32_t>(a[i]) != std::bit_cast<std::uint32_t>(b[i]))
            return false;
    return true;
}
bool Equal(const A &a, const A &b) {
    return Equal(a.minimum, b.minimum) && Equal(a.maximum, b.maximum);
}
T Identity() { return T{{V{1, 0, 0, 0}, V{0, 1, 0, 0}, V{0, 0, 1, 0}, V{0, 0, 0, 1}}}; }
struct FpScope {
    unsigned saved = _mm_getcsr();
    ~FpScope() { _mm_setcsr(saved); }
};
std::uint64_t Address(const J &x) {
    const auto &s = x.as_string();
    return std::stoull(s, nullptr, 16);
}
Bytes Hex(const std::string &s) {
    Bytes b;
    for (std::size_t i = 0; i < s.size(); i += 2)
        b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i, 2), nullptr, 16)));
    return b;
}
void Native(const J &proof) {
    unsigned totalBytes = 0;
    for (const auto &source : proof.at("inputs").as_array()) {
        const auto path = std::filesystem::path(Monstey_Wide(source.at("path").as_string()));
        const auto image = Monstey_ReadFile(path, 256u * 1024 * 1024);
        const auto sha = source.at("sha256").as_string();
        Require(Monstey_HashSha256(image) == sha, "Paired native image identity differs");
        unsigned bytes = 0;
        for (const auto &r : proof.at("ranges").as_array()) {
            const auto expected = Hex(r.at("hex").as_string());
            Require(expected.size() == r.at("size").as_uint(), "Native proof size differs");
            const auto got = Monstey_FearPeRange(image, Address(r.at("ea")), expected.size());
            Require(std::equal(got.begin(), got.end(), expected.begin()),
                    "Paired native bytes differ");
            Require(Monstey_HashSha256(got) == r.at("sha256").as_string(),
                    "Paired native range hash differs");
            bytes += static_cast<unsigned>(got.size());
            ++nativeChecks;
        }
        Require(bytes == 5288, "Selected native byte scope differs");
        Require(Monstey_U32(Monstey_FearPeRange(image, 0x1425d7ef0, 16), 0) == 0x3b90fdbc,
                "Native compact-tree scale literal differs");
        const auto mask = Monstey_FearPeRange(image, 0x1425d6f60, 16);
        Require(Monstey_U32(mask, 0) == 15 && Monstey_U32(mask, 4) == 15 &&
                    Monstey_U32(mask, 8) == 15 && Monstey_U32(mask, 12) == 0,
                "Native low-nibble mask differs");
        Require(Monstey_HashSha256(path) == sha, "Native source changed during review");
        totalBytes += bytes;
    }
    Check(nativeChecks == 70 && totalBytes == 10576,
          "Independent C++ verification: 70 paired native ranges and exact literals");
}
void Node(Bytes &n, std::size_t i, std::uint32_t payload, bool internal) {
    n[i * 6 + 3] = static_cast<std::uint8_t>((payload >> 16) | (internal ? 128 : 0));
    Monstey_PutU16(n, i * 6 + 4, static_cast<std::uint16_t>(payload));
}
std::uint32_t ReadPayload(std::span<const std::uint8_t> n, std::size_t i) {
    return (static_cast<std::uint32_t>(n[i * 6 + 3] % 128) * 65536) + Monstey_U16(n, i * 6 + 4);
}
bool Overlap(const A &a, const A &b) {
    for (unsigned k = 0; k < 3; ++k)
        if (!(a.minimum[k] <= b.maximum[k] && b.minimum[k] <= a.maximum[k]))
            return false;
    return true;
}
A Decode(const A &parent, const Bytes &n, std::size_t i) {
    A out = parent;
    for (unsigned k = 0; k < 3; ++k) {
        const auto q = n[i * 6 + k];
        const auto minimumUnits = static_cast<float>(q / 16),
                   maximumUnits = static_cast<float>(q % 16);
        const float unit =
            Mul(Sub(parent.maximum[k], parent.minimum[k]), std::bit_cast<float>(0x3b90fdbcu));
        out.minimum[k] = Add(parent.minimum[k], Mul(Mul(minimumUnits, minimumUnits), unit));
        out.maximum[k] = Sub(parent.maximum[k], Mul(Mul(maximumUnits, maximumUnits), unit));
    }
    return out;
}
struct Path {
    std::uint32_t value;
    std::vector<std::size_t> nodes;
};
void Paths(const Bytes &n, std::size_t index, std::vector<std::size_t> path,
           std::vector<Path> &out) {
    if ((n[index * 6 + 3] & 128) == 0) {
        out.push_back({ReadPayload(n, index), path});
        return;
    }
    const auto left = index + 1, right = index + 2 * ReadPayload(n, index);
    auto p = path;
    p.push_back(left);
    Paths(n, left, p, out);
    path.push_back(right);
    Paths(n, right, path, out);
}
std::vector<std::uint32_t> TreeReference(const Bytes &n, const A &root, const A &q) {
    std::vector<std::uint32_t> answer;
    if (n.empty() || !Overlap(root, q))
        return answer;
    std::vector<Path> paths;
    Paths(n, 0, {}, paths);
    for (const auto &p : paths) {
        A bound = root;
        bool valid = true;
        for (const auto index : p.nodes) {
            bound = Decode(bound, n, index);
            if (!Overlap(bound, q))
                valid = false;
        }
        if (valid)
            answer.push_back(p.value);
    }
    return answer;
}
std::size_t BuildTree(Bytes &n, unsigned depth, std::mt19937 &rng) {
    const auto i = n.size() / 6;
    n.resize(n.size() + 6);
    if (!depth) {
        Node(n, i, rng() & 0x7fffffu, false);
        return i;
    }
    const auto left = BuildTree(n, depth - 1, rng);
    const auto right = BuildTree(n, depth - 1, rng);
    Require(left == i + 1 && (right - i) % 2 == 0, "Review tree construction invariant");
    Node(n, i, static_cast<std::uint32_t>((right - i) / 2), true);
    for (std::size_t child : {left, right})
        for (unsigned k = 0; k < 3; ++k)
            n[child * 6 + k] = static_cast<std::uint8_t>(rng());
    return i;
}
void Trees() {
    std::mt19937 rng(0xcab48721);
    std::uniform_real_distribution<float> f(-12, 12);
    for (unsigned trial = 0; trial < 4096; ++trial) {
        Bytes n;
        BuildTree(n, 1 + trial % 5, rng);
        A root{{-10, -10, -10, 100}, {10, 10, 10, -100}}, q{};
        for (unsigned k = 0; k < 4; ++k) {
            q.minimum[k] = f(rng);
            q.maximum[k] = Add(q.minimum[k], std::abs(f(rng)));
        }
        Require(Monstey_NavigationCompactTreeQuery({n, root}, q) == TreeReference(n, root, q),
                "Independent root-to-leaf path oracle disagrees");
        ++treeCases;
    }
    Check(true, "4096 path-enumeration compact-tree cases, including quantized inverted children");
    Bytes n(42);
    Node(n, 0, 2, true);
    Node(n, 1, 1, true);
    Node(n, 2, 9, false);
    Node(n, 3, 1, false);
    Node(n, 4, 1, true);
    Node(n, 5, 9, false);
    Node(n, 6, 0x400003u, false);
    const A root{{-10, -10, -10, 77}, {10, 10, 10, -77}};
    Check(Monstey_NavigationCompactTreeQuery({n, root}, root) ==
              std::vector<std::uint32_t>{9, 1, 9, 0x400003u},
          "Native left-first duplicate/raw payload order survives opposing W bounds");
    Reject([&] { Monstey_NavigationCompactTreeQuery({std::span(n).first(41), root}, root); },
           "Owned six-byte stride violation rejected");
    Bytes bad(6);
    Node(bad, 0, 1, true);
    Reject([&] { Monstey_NavigationCompactTreeQuery({bad, root}, root); },
           "Owned child offset cannot escape tree");
}
V WorldReference(const T &t, const V &v) {
    V out;
    for (unsigned k = 0; k < 4; ++k)
        out[k] = Add(Add(Add(Mul(v[1], t[1][k]), Mul(v[0], t[0][k])), Mul(v[2], t[2][k])), t[3][k]);
    return out;
}
A TransformReference(const T &t, const A &q) {
    for (unsigned k = 0; k < 3; ++k)
        if (q.maximum[k] < q.minimum[k])
            return q;
    V center{}, extent{};
    for (unsigned k = 0; k < 4; ++k) {
        center[k] = Mul(Add(q.minimum[k], q.maximum[k]), 0.5f);
        extent[k] = Mul(Sub(q.maximum[k], q.minimum[k]), 0.5f);
    }
    A out;
    for (unsigned k = 0; k < 4; ++k) {
        const auto r = Add(Add(Abs(Mul(extent[0], t[0][k])), Abs(Mul(extent[1], t[1][k]))),
                           Add(Abs(Mul(extent[2], t[2][k])), 0));
        const auto c =
            Add(Add(Add(Mul(center[0], t[0][k]), Mul(center[1], t[1][k])), Mul(center[2], t[2][k])),
                t[3][k]);
        out.minimum[k] = Add(SignFlip(r), c);
        out.maximum[k] = Add(c, r);
    }
    return out;
}
A LocalReference(const T &t, const A &q) {
    const auto id = Identity();
    bool isIdentity = true;
    for (unsigned k = 0; k < 3; ++k) {
        float a = Abs(Sub(id[3][k], t[3][k]));
        float b = Max(Abs(Sub(id[2][k], t[2][k])), a);
        float c = Abs(Sub(id[1][k], t[1][k]));
        float d = Max(Abs(Sub(id[0][k], t[0][k])), c);
        d = Max(d, b);
        if (!(d <= 0))
            isIdentity = false;
    }
    if (isIdentity)
        return q;
    T inverse{{V{t[0][0], t[1][0], t[2][0], t[2][1]}, V{t[0][1], t[1][1], t[2][1], t[2][3]},
               V{t[0][2], t[1][2], t[2][2], t[2][3]}, V{}}};
    for (unsigned k = 0; k < 4; ++k)
        inverse[3][k] =
            Add(Add(Mul(SignFlip(t[3][1]), inverse[1][k]), Mul(SignFlip(t[3][0]), inverse[0][k])),
                Mul(SignFlip(t[3][2]), inverse[2][k]));
    return TransformReference(inverse, q);
}
void Transforms() {
    std::mt19937 rng(0x73da241);
    std::uniform_real_distribution<float> f(-5, 5);
    const A treeBounds{{-100, -100, -100, 0}, {100, 100, 100, 0}};
    for (unsigned i = 0; i < 2048; ++i) {
        T t{};
        for (auto &column : t)
            for (auto &lane : column)
                lane = f(rng);
        A q{};
        for (unsigned k = 0; k < 4; ++k) {
            q.minimum[k] = f(rng);
            q.maximum[k] = q.minimum[k] + std::abs(f(rng));
        }
        Monstey_NavigationBoundaryMeshView m{};
        m.transform = t;
        const auto got = Monstey_NavigationBoundaryQuery(m, {{}, treeBounds}, q);
        Require(Equal(got.local_query, LocalReference(t, q)),
                "Scalar transpose/negative-translation local query differs");
        ++localCases;
        V v{};
        for (auto &x : v)
            x = f(rng);
        Bytes bytes(16);
        std::memcpy(bytes.data(), v.data(), 16);
        m.vertices = bytes;
        Require(Equal(Monstey_NavigationBoundaryWorldVertex(m, 0), WorldReference(t, v)),
                "Scalar native-order four-lane endpoint transform differs");
        ++worldCases;
    }
    Check(true, "2048 scalar transposed local-query and four-lane world-vertex comparisons");
    T t = Identity();
    t[0][0] = 2;
    t[1][1] = 3;
    t[2][2] = 4;
    Monstey_NavigationBoundaryMeshView m{};
    m.transform = t;
    const A q{{1, 2, 3, 0}, {2, 3, 4, 0}};
    const auto got = Monstey_NavigationBoundaryQuery(m, {{}, treeBounds}, q);
    Check(
        got.local_query.minimum[0] == 2 && got.local_query.maximum[0] == 4 &&
            got.local_query.minimum[1] == 6 && got.local_query.maximum[1] == 9,
        "Transposition is preserved for nonorthonormal scaling, not replaced by a general inverse");
    t = Identity();
    for (unsigned k = 0; k < 4; ++k)
        t[k][3] = static_cast<float>(k * 7 + 2);
    m.transform = t;
    Check(Equal(Monstey_NavigationBoundaryQuery(m, {{}, treeBounds}, q).local_query, q),
          "Identity fast path ignores all W differences");
}
bool SlabReference(const V &a, const V &b, const A &q) {
    float low[3], high[3];
    for (unsigned k = 0; k < 3; ++k) {
        float d = Sub(b[k], a[k]);
        if (d == 0)
            d = 0;
        const float approximation = _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(d)));
        float reciprocal = Mul(Sub(2, Mul(approximation, d)), approximation);
        if (d == 0)
            reciprocal = std::bit_cast<float>(0x7f7fffeeu);
        const auto p = Mul(Sub(q.minimum[k], a[k]), reciprocal),
                   r = Mul(Sub(q.maximum[k], a[k]), reciprocal);
        low[k] = Min(p, r);
        high[k] = Max(p, r);
    }
    const auto enter = Max(Max(low[2], Max(low[1], low[0])), 0);
    const auto leave = Min(Min(high[2], Min(high[1], high[0])), 1);
    return leave >= enter;
}
void Slabs() {
    FpScope restore;
    std::mt19937 rng(0xd22e319);
    const std::array<std::uint32_t, 20> special{
        0,          0x80000000, 1,          0x80000001, 0x007fffff, 0x00800000, 0x3f800000,
        0xbf800000, 0x40000000, 0xc0000000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000,
        0x7fc00001, 0xffc12345, 0x3effffff, 0xbeffffff, 0x00002000, 0x80002000};
    for (unsigned mode = 0; mode < 8; ++mode) {
        _mm_setcsr(0x1f80u | ((mode % 4) << 13) | (mode >= 4 ? 0x8040u : 0));
        for (unsigned i = 0; i < 2048; ++i) {
            V a{}, b{};
            A q{};
            for (unsigned k = 0; k < 4; ++k) {
                a[k] = std::bit_cast<float>(special[rng() % special.size()]);
                b[k] = std::bit_cast<float>(special[rng() % special.size()]);
                q.minimum[k] = -1;
                q.maximum[k] = 1;
            }
            const auto got = Monstey_NavigationSegmentIntersectsAabb(a, b, q),
                       expected = SlabReference(a, b, q);
            Require(got == expected,
                    "Independent scalar RCP/slab reference differs under MXCSR mode");
            ++slabCases;
        }
    }
    Check(true, "16384 independent scalar slab cases over eight rounding/FTZ/DAZ modes and special "
                "float bits");
    _mm_setcsr(0x1f80);
    const float tiny = std::bit_cast<float>(0x00002000u);
    A gap{{tiny, -1, -1, 0}, {tiny * 2, 1, 1, 0}};
    Check(Monstey_NavigationSegmentIntersectsAabb(V{}, V{}, gap) && SlabReference(V{}, V{}, gap),
          "Native finite zero reciprocal admits the tiny parallel gap under gradual underflow");
}
std::uint32_t OrderedReference(float x) {
    const auto b = std::bit_cast<std::uint32_t>(x);
    const auto magnitude = b & 0x7fffffffu;
    return b >> 31 ? 0x3fffffffu - (magnitude / 2) : 0x40000000u + (magnitude / 2);
}
void Ordered() {
    std::mt19937 rng(0x51dc39);
    for (unsigned i = 0; i < 16384; ++i) {
        A a{};
        for (unsigned k = 0; k < 4; ++k) {
            a.minimum[k] = std::bit_cast<float>(rng());
            a.maximum[k] = std::bit_cast<float>(rng());
        }
        const auto index = rng();
        const auto got = Monstey_NavigationEncodeOrderedAabb(a, index);
        for (unsigned k = 0; k < 4; ++k) {
            Require(got.minimum[k] == (k == 3 ? index : OrderedReference(a.minimum[k])),
                    "Independent magnitude-based ordered minimum differs");
            Require(got.maximum[k] == OrderedReference(a.maximum[k]) + 1u,
                    "Independent magnitude-based ordered maximum differs");
        }
        ++orderedCases;
    }
    Check(true,
          "16384 full-bit ordered AABB cases including NaNs, signed zero and full-width hit IDs");
}
void Face(Bytes &x, unsigned i, std::int32_t first, std::int16_t count, std::uint32_t tag = 0) {
    Monstey_PutU32(x, 16 * i, std::bit_cast<std::uint32_t>(first));
    Monstey_PutU16(x, 16 * i + 8, std::bit_cast<std::uint16_t>(count));
    Monstey_PutU32(x, 16 * i + 12, tag);
}
void Edge(Bytes &x, unsigned i, std::int32_t a, std::int32_t b,
          std::uint32_t opposite = 0xffffffffu, std::uint32_t tag = 0) {
    Monstey_PutU32(x, 20 * i, std::bit_cast<std::uint32_t>(a));
    Monstey_PutU32(x, 20 * i + 4, std::bit_cast<std::uint32_t>(b));
    Monstey_PutU32(x, 20 * i + 8, opposite);
    Monstey_PutU32(x, 20 * i + 12, tag);
}
void Selectors() {
    Bytes faces(48), mappedFaces(48), addedFaces(32), edges(60), mappedEdges(60), addedEdges(40);
    for (unsigned i = 0; i < 3; ++i) {
        Face(faces, i, 0, 0, 10 + i);
        Face(mappedFaces, i, 0, 0, 20 + i);
        Edge(edges, i, 0, 0, 0xffffffffu, 30 + i);
        Edge(mappedEdges, i, 0, 0, 0xffffffffu, 40 + i);
    }
    for (unsigned i = 0; i < 2; ++i) {
        Face(addedFaces, i, 0, 0, 50 + i);
        Edge(addedEdges, i, 0, 0, 0xffffffffu, 60 + i);
    }
    std::array<std::int32_t, 3> map{-1, 2, 0};
    Monstey_NavigationBoundaryMeshView m{faces,      edges,       {},          map,
                                         map,        mappedFaces, mappedEdges, addedFaces,
                                         addedEdges, {},          Identity()};
    for (unsigned empty = 0; empty < 2; ++empty) {
        if (empty) {
            m.face_map = {};
            m.edge_map = {};
        }
        for (unsigned i = 0; i < 5; ++i) {
            const auto face = Monstey_NavigationBoundaryFace(m, i),
                       edge = Monstey_NavigationBoundaryEdge(m, i);
            const auto expectedFace = i >= 3         ? 50 + i - 3
                                      : empty        ? 20 + i
                                      : map[i] == -1 ? 10 + i
                                                     : 20 + map[i];
            const auto expectedEdge = i >= 3         ? 60 + i - 3
                                      : empty        ? 40 + i
                                      : map[i] == -1 ? 30 + i
                                                     : 40 + map[i];
            Require(Monstey_U32(face, 12) == expectedFace && Monstey_U32(edge, 12) == expectedEdge,
                    "Independent original/mapped/added selector table differs");
            ++selectorCases;
        }
    }
    Check(true, "All original/mapped/added and zero-map-count selector routes");
    std::array<std::int32_t, 3> bad{-2, -1, -1};
    m.face_map = bad;
    Reject([&] { Monstey_NavigationBoundaryFace(m, 0); },
           "Unsupported negative map index rejected on owned storage");
    Reject([&] { Monstey_NavigationBoundaryEdge(m, 5); },
           "Added edge selector cannot escape owned storage");
}
void QueryOrder() {
    Bytes faces(48), edges(80), vertices(64), nodes(42);
    Face(faces, 0, 0, 2);
    Face(faces, 1, 2, 1);
    Face(faces, 2, 3, 1);
    Edge(edges, 0, 0, 1);
    Edge(edges, 1, 0, 1, 0);
    Edge(edges, 2, 2, 3);
    Edge(edges, 3, 1, 0);
    const std::array<V, 4> v{V{-.5f, 0, 0, 1}, V{.5f, 0, 0, 2}, V{-3, .5f, 0, 3}, V{.5f, 3, 0, 4}};
    std::memcpy(vertices.data(), v.data(), 64);
    std::array<std::int32_t, 3> fm{-1, -1, -1};
    std::array<std::int32_t, 4> em{-1, -1, -1, -1};
    Node(nodes, 0, 2, true);
    Node(nodes, 1, 1, true);
    Node(nodes, 2, 2, false);
    Node(nodes, 3, 0, false);
    Node(nodes, 4, 1, true);
    Node(nodes, 5, 1, false);
    Node(nodes, 6, 2, false);
    Monstey_NavigationBoundaryMeshView m{faces, edges, vertices, fm, em,        {},
                                         {},    {},    {},       {}, Identity()};
    A root{{-4, -1, -1, 99}, {4, 4, 1, -99}}, query{{-1, -1, -1, 77}, {1, 1, 1, -77}};
    const auto got =
        Monstey_NavigationBoundaryQuery(m, {nodes, root}, query, V{.125f, .25f, .5f, 1});
    Require(got.masked_face_keys == std::vector<std::uint32_t>{2, 0, 1, 2},
            "Candidate tree order changed");
    Require(got.hits == std::vector<Monstey_NavigationBoundaryHit>{{2, 3}, {0, 0}, {0, 1}, {2, 3}},
            "Query hit order or segment rejection changed");
    Require(got.boundary_boxes.size() == 3 && got.boundary_boxes[0].minimum[3] == 0 &&
                got.boundary_boxes[1].minimum[3] == 1 && got.boundary_boxes[2].minimum[3] == 3,
            "Boundary hit holes compacted or reindexed");
    Check(true,
          "Ordered candidates, segment rejection, duplicate hits and non-boundary identity holes");
    const auto empty = Monstey_NavigationBoundaryQuery(m, {{}, root}, query);
    Check(empty.hits.empty() && empty.boundary_boxes.empty(),
          "Empty owned tree does not silently enumerate all edges");
    // Native hit generation retains raw edge index; the boundary stage masks to22bits.
    constexpr std::uint32_t packed = 0x400000u;
    Bytes bigEdges((static_cast<std::size_t>(packed) + 1) * 20), oneFace(16), leaf(6);
    std::vector<std::int32_t> bigMap(static_cast<std::size_t>(packed) + 1, -1);
    Face(oneFace, 0, static_cast<std::int32_t>(packed), 1);
    Edge(bigEdges, 0, 0, 1, 0);
    Edge(bigEdges, packed, 0, 1);
    Node(leaf, 0, 0, false);
    std::array<std::int32_t, 1> oneMap{-1};
    m.faces = oneFace;
    m.edges = bigEdges;
    m.face_map = oneMap;
    m.edge_map = bigMap;
    const auto wrapped = Monstey_NavigationBoundaryQuery(m, {leaf, root}, query);
    Check(wrapped.hits == std::vector<Monstey_NavigationBoundaryHit>{{0, packed}} &&
              wrapped.boundary_boxes.empty(),
          "Raw large edge hit survives while later boundary lookup uses the22-bit masked edge");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        Require(argc == 3, "Usage: BoundaryIndependentReview NATIVE_PROOF_OR_--synthetic NEW_REPORT");
        FpScope scope;
        _mm_setcsr(0x1f80);
        const bool native = std::wstring_view(argv[1]) != L"--synthetic";
        if (native)
            Native(J::Parse(Monstey_ReadText(argv[1])));
        Trees();
        Transforms();
        Slabs();
        Ordered();
        Selectors();
        QueryOrder();
        const auto report = J::object(
            {{"schema", "Monstey.NavigationBoundaryIndependentReview/1"},
             {"pass", true},
             {"groups", groups},
             {"paired_native_proof_supplied", native},
             {"native_range_checks", nativeChecks},
             {"independent_tree_path_cases", treeCases},
             {"independent_local_query_cases", localCases},
             {"independent_world_vertex_cases", worldCases},
             {"independent_scalar_slab_cases", slabCases},
             {"independent_ordered_full_bit_cases", orderedCases},
             {"selector_routes", selectorCases},
             {"actual_CPU_FP_modes", 8},
             {"owned_compact_tree_required", true},
             {"original_code_executed", false},
             {"game_process_accessed", false},
             {"scope", "Independent owned-input math/ordering references; optional supplied "
                       "paired native proof is separate. No compact-tree cooker, runtime "
                       "synchronization or actor traversal proof"}});
        Monstey_WriteNewText(argv[2], report.dump(2) + "\n");
        std::cout << "PASS " << groups << " independent boundary review groups\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}