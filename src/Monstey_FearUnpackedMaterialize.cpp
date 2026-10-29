#include "Monstey_FearUnpackedMaterialize.hpp"
#include <algorithm>
#include <bit>
#include <utility>
namespace Monstey {
Monstey_FearUnpackedBoundaryFault::Monstey_FearUnpackedBoundaryFault(std::string name)
    : std::runtime_error("Unqualified consumed FEAR boundary: " + name), boundary(std::move(name)) {
}
namespace {
using U = std::uint32_t;
using Op = Monstey_FearUnpackedStringOp;
using Mem = Monstey_FearDbMutableMemory;
using Imports = Monstey_FearUnpackedImports;
U Read(Monstey_FearDbMemory m, U at, U width = 4) {
    if (at < m.base || std::uint64_t(at) + width > 0x100000000ull ||
        std::uint64_t(at) - m.base > m.bytes.size() ||
        width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
        throw Monstey_FearDbFault(at, width, false);
    U v = 0;
    for (U i = 0; i < width; ++i)
        v |= U(m.bytes[at - m.base + i]) << (i * 8);
    return v;
}
U R(Mem m, U at, U width = 4) { return Read({m.base, m.bytes}, at, width); }
void W(Mem m, U at, U value, U width = 4) {
    if (at < m.base || std::uint64_t(at) + width > 0x100000000ull ||
        std::uint64_t(at) - m.base > m.bytes.size() ||
        width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
        throw Monstey_FearDbFault(at, width, true);
    for (U i = 0; i < width; ++i)
        m.bytes[at - m.base + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
U Str(Mem m, const Imports &i, Op op, U self, U a = 0, U b = 0, U c = 0) {
    if (!i.string)
        throw Monstey_FearUnpackedBoundaryFault("string");
    return i.string(m, op, self, a, b, c);
}
U Npos(Mem m, const Imports &i) { return R(m, R(m, i.npos_pointer_cell)); }
U Length(Mem m, U source) {
    if (source == 0)
        return 0;
    const U first = source;
    while (R(m, source, 1) != 0)
        ++source;
    return source - first;
}
void Push(Mem m, U &sp, U value) {
    sp -= 4u;
    W(m, sp, value);
}
void CopyBytes(Mem m, const Imports &i, U dst, U src, U count) {
    if (!i.strncpy)
        throw Monstey_FearUnpackedBoundaryFault("strncpy");
    i.strncpy(m, dst, src, count);
}
// Complete3A70. Its actual output words are reloaded after each imported substr.
void Key(Mem m, U entry, U input, const Imports &i) {
    const U k = entry - 0x24u, temp = k + 8u;
    U last = Str(m, i, Op::rfind, input, 47, Npos(m, i));
    const U n = Npos(m, i);
    U returned = Str(m, i, Op::substr, input, temp, last + 1u, n);
    Str(m, i, Op::assign, R(m, k + 0x30u), returned);
    Str(m, i, Op::destroy, temp);
    const U previous = Str(m, i, Op::rfind, input, 47, last - 1u);
    const U count = last - previous - 1u;
    returned = Str(m, i, Op::substr, input, temp, previous + 1u, count);
    Str(m, i, Op::assign, R(m, k + 0x2Cu), returned);
    Str(m, i, Op::destroy, temp);
    returned = Str(m, i, Op::substr, input, temp, 0, previous);
    Str(m, i, Op::assign, R(m, k + 0x28u), returned);
    Str(m, i, Op::destroy, temp);
}
void CallKey(Mem m, U frame, U input, U category, U record, U attribute, const Imports &i) {
    U sp = frame;
    Push(m, sp, attribute);
    Push(m, sp, record);
    Push(m, sp, category);
    Key(m, sp - 4u, input, i);
}
// Complete3FE0, including the non-nil right subtree and parent climb order.
void Next(Mem m, U iterator) {
    U node = R(m, iterator);
    if (R(m, node + 0x4Du, 1) != 0)
        return;
    U next = R(m, node + 8u);
    if (R(m, next + 0x4Du, 1) == 0) {
        U left = R(m, next);
        while (R(m, left + 0x4Du, 1) == 0) {
            next = left;
            left = R(m, next);
        }
        W(m, iterator, next);
        return;
    }
    node = R(m, node + 4u);
    while (R(m, node + 0x4Du, 1) == 0) {
        next = R(m, iterator);
        if (next != R(m, node + 8u))
            break;
        W(m, iterator, node);
        node = R(m, node + 4u);
    }
    W(m, iterator, node);
}
// Exact6EA0 argument reads interleave the field stores. The receiver stays cached.
void Node(Mem m, U entry, U out) {
    const U name = R(m, entry + 4u);
    const U count = R(m, entry + 12u);
    W(m, out, name);
    const U children = R(m, entry + 16u);
    W(m, out + 8u, count);
    const U parent = R(m, entry + 8u);
    W(m, out + 12u, children);
    W(m, out + 4u, parent);
}
// Exact6EF0: narrowing occurs at each consumed byte/word read, not on PUSH.
void Attribute(Mem m, U entry, U out) {
    const U name = R(m, entry + 4u);
    const U type = R(m, entry + 12u, 1);
    W(m, out, name);
    const U usage = R(m, entry + 16u, 1);
    W(m, out + 10u, type, 1);
    const U count = R(m, entry + 20u, 2);
    W(m, out + 11u, usage, 1);
    const U values = R(m, entry + 24u);
    W(m, out + 8u, count, 2);
    const U parent = R(m, entry + 8u);
    W(m, out + 12u, values);
    W(m, out + 4u, parent);
}
// Exact6EC0 root initializer, with argument cells still live between stores.
void Database(Mem m, U entry, U out) {
    const U name = R(m, entry + 4u);
    const U count = R(m, entry + 8u);
    W(m, out + 4u, name);
    const U children = R(m, entry + 12u);
    W(m, out + 8u, count);
    const U allocation = R(m, entry + 16u);
    W(m, out + 12u, children);
    W(m, out, allocation);
    W(m, out + 16u, 0);
}
// Complete3E80/3DD0 after caller's real outgoing argument writes and copy.
void NamedNode(Mem m, U entry, U context, U pool, const Imports &i) {
    const U offset = R(m, entry + 0x30u);
    const U end = R(m, context + 0x14u);
    const U remaining = end - R(m, offset) - pool;
    U source = Str(m, i, Op::c_str, entry + 4u);
    const U destination = R(m, offset) + pool;
    if (destination != 0 && remaining != 0) {
        if (source == 0)
            source = i.empty_cstring;
        CopyBytes(m, i, destination, source, remaining - 1u);
        W(m, destination + remaining - 1u, 0, 1);
    }
    const U children = R(m, entry + 0x2Cu);
    const U count = R(m, entry + 0x28u);
    const U parent = R(m, entry + 0x24u);
    U sp = entry - 12u;
    Push(m, sp, children);
    const U nameOffset = R(m, offset);
    Push(m, sp, count);
    Push(m, sp, parent);
    const U out = R(m, entry + 0x20u);
    Push(m, sp, nameOffset + pool);
    Node(m, sp - 4u, out);
    source = Str(m, i, Op::c_str, entry + 4u);
    const U n = Length(m, source);
    W(m, offset, R(m, offset) + n + 1u);
    Str(m, i, Op::destroy, entry + 4u);
}
void NamedAttribute(Mem m, U entry, U context, U pool, const Imports &i) {
    const U offset = R(m, entry + 0x38u);
    const U end = R(m, context + 0x14u);
    const U remaining = end - R(m, offset) - pool;
    U source = Str(m, i, Op::c_str, entry + 4u);
    const U destination = R(m, offset) + pool;
    if (destination != 0 && remaining != 0) {
        if (source == 0)
            source = i.empty_cstring;
        CopyBytes(m, i, destination, source, remaining - 1u);
        W(m, destination + remaining - 1u, 0, 1);
    }
    const U values = R(m, entry + 0x34u);
    const U count = R(m, entry + 0x30u);
    const U usage = R(m, entry + 0x2Cu);
    U sp = entry - 12u;
    Push(m, sp, values);
    const U type = R(m, entry + 0x28u);
    Push(m, sp, count);
    const U parent = R(m, entry + 0x24u);
    Push(m, sp, usage);
    const U nameOffset = R(m, offset);
    Push(m, sp, type);
    Push(m, sp, parent);
    Push(m, sp, nameOffset + pool);
    const U out = R(m, entry + 0x20u);
    Attribute(m, sp - 4u, out);
    source = Str(m, i, Op::c_str, entry + 4u);
    const U n = Length(m, source);
    W(m, offset, R(m, offset) + n + 1u);
    Str(m, i, Op::destroy, entry + 4u);
}
} // namespace
void Monstey_FearUnpackedSize(Mem m, U context, U f, const Imports &i) {
    Str(m, i, Op::construct, f + 0x64u);
    Str(m, i, Op::construct, f + 0x48u);
    U map = R(m, f + 0xBCu), header = R(m, map + 4u), node = R(m, header);
    W(m, f + 0x0Cu, node);
    if (node != header) {
        for (;;) {
            node = R(m, f + 0x0Cu);
            Str(m, i, Op::construct, f + 0x2Cu);
            Str(m, i, Op::construct, f + 0x10u);
            Str(m, i, Op::construct, f + 0x80u);
            CallKey(m, f, node + 0x0Cu, f + 0x2Cu, f + 0x10u, f + 0x80u, i);
            if ((Str(m, i, Op::unequal, f + 0x2Cu, f + 0x64u) & 255u) != 0) {
                W(m, context, R(m, context) + 1u);
                const U source = Str(m, i, Op::c_str, f + 0x2Cu);
                const U n = Length(m, source);
                const U p = R(m, context + 0x10u) + n + 1u;
                W(m, context + 0x10u, p);
                Str(m, i, Op::assign, f + 0x64u, f + 0x2Cu);
                Str(m, i, Op::clear, f + 0x48u);
            }
            if ((Str(m, i, Op::unequal, f + 0x10u, f + 0x48u) & 255u) != 0) {
                W(m, context + 4u, R(m, context + 4u) + 1u);
                const U source = Str(m, i, Op::c_str, f + 0x10u);
                const U n = Length(m, source);
                const U p = R(m, context + 0x10u) + n + 1u;
                W(m, context + 0x10u, p);
                Str(m, i, Op::assign, f + 0x48u, f + 0x10u);
            }
            W(m, context + 8u, R(m, context + 8u) + 1u);
            const U source = Str(m, i, Op::c_str, f + 0x80u);
            const U nameLength = Length(m, source);
            U budget = R(m, context + 0x10u) + nameLength + 1u;
            const U value = node + 0x2Cu;
            W(m, context + 0x10u, budget);
            const U last = R(m, value + 0x14u) - 1u;
            const U charPointer = Str(m, i, Op::subscript, value, last);
            if (R(m, charPointer, 1) != 59)
                Str(m, i, Op::append, value, 59);
            U start = 0, delimiter = Str(m, i, Op::find, value, 59, start);
            if (delimiter != Npos(m, i)) {
                do {
                    Str(m, i, Op::substr, value, f + 0x9Cu, start, delimiter - start);
                    U tokenCount = R(m, context + 0x0Cu);
                    const U liveNode = R(m, f + 0x0Cu);
                    ++tokenCount;
                    W(m, context + 0x0Cu, tokenCount);
                    const U type = R(m, liveNode + 0x28u);
                    if (type == 4) {
                        const U length = R(m, f + 0xB0u);
                        budget = R(m, context + 0x10u) + length + 1u;
                    } else {
                        budget = R(m, context + 0x10u) + (type == 6   ? 8u
                                                          : type == 7 ? 12u
                                                          : type == 8 ? 16u
                                                                      : 4u);
                    }
                    start = delimiter + 1u;
                    W(m, context + 0x10u, budget);
                    delimiter = Str(m, i, Op::find, value, 59, start);
                    Str(m, i, Op::destroy, f + 0x9Cu);
                } while (delimiter != Npos(m, i));
            }
            Str(m, i, Op::destroy, f + 0x80u);
            Str(m, i, Op::destroy, f + 0x10u);
            Str(m, i, Op::destroy, f + 0x2Cu);
            Next(m, f + 0x0Cu);
            map = R(m, f + 0xBCu);
            node = R(m, f + 0x0Cu);
            if (node == R(m, map + 4u))
                break;
        }
    }
    Str(m, i, Op::destroy, f + 0x48u);
    Str(m, i, Op::destroy, f + 0x64u);
}
std::uint32_t Monstey_FearUnpackedAllocationBytes(Monstey_FearDbMemory m, U context) {
    const U c = Read(m, context), r = Read(m, context + 4u), a = Read(m, context + 8u),
            v = Read(m, context + 12u), p = Read(m, context + 16u);
    return 20u + 16u * (c + r + a) + 4u * v + p;
}
void Monstey_FearUnpackedFixLinks(Mem m, U root) {
    for (U c = 0; c < R(m, root + 8u); ++c) {
        const U category = R(m, root + 12u) + c * 16u;
        for (U r = 0; r < R(m, category + 8u); ++r) {
            const U record = R(m, category + 12u) + r * 16u;
            for (U a = 0; a < R(m, record + 8u); ++a) {
                const U attribute = R(m, record + 12u) + a * 16u;
                if (R(m, attribute + 10u, 1) != 9)
                    continue;
                const U parent = R(m, attribute + 4u);
                const U owner = R(m, parent + 4u);
                const U db = R(m, owner + 4u);
                for (U n = 0; n < R(m, attribute + 8u, 2); ++n) {
                    const U at = R(m, attribute + 12u) + n * 4u;
                    const U packed = R(m, at), ci = packed >> 16, ri = packed & 65535u;
                    U resolved = 0;
                    if (ci < R(m, db + 8u)) {
                        const U cat = R(m, db + 12u) + ci * 16u;
                        if (ri < R(m, cat + 8u))
                            resolved = R(m, cat + 12u) + ri * 16u;
                    }
                    W(m, at, resolved);
                }
            }
        }
    }
}
std::uint8_t Monstey_FearUnpackedMaterialize(Mem m, U cursor, U f, const Imports &i) {
    U current = R(m, cursor);
    U context = R(m, f + 0x134u);
    U size = R(m, context) * 16u;
    W(m, f + 0x3Cu, current);
    current += size;
    W(m, cursor, current);
    size = R(m, context + 4u) * 16u;
    W(m, f + 0x24u, current);
    current += size;
    W(m, cursor, current);
    size = R(m, context + 8u) * 16u;
    U pool = current + size;
    W(m, cursor, pool);
    W(m, f + 0x40u, current);
    size = R(m, context + 12u);
    W(m, f + 0x28u, pool);
    pool += size * 4u;
    W(m, cursor, pool);
    size = R(m, context + 16u);
    U offset = 0;
    const U end = pool + size;
    W(m, cursor, end);
    W(m, f + 0x38u, 0);
    W(m, f + 0x18u, 0);
    W(m, f + 0x20u, 0);
    W(m, f + 0x10u, 0);
    for (U o : std::array<U, 6>{0x60, 0xBC, 0x110, 0xA0, 0xD8, 0xF4})
        Str(m, i, Op::construct, f + o);
    U initialSp = f;
    Push(m, initialSp, f + 0x110u);
    U map = R(m, f + 0x138u);
    Push(m, initialSp, f + 0xBCu);
    U node = R(m, R(m, map + 4u));
    Push(m, initialSp, f + 0x60u);
    W(m, f + 0x2Cu, 0);
    W(m, f + 0x1Cu, 0);
    Key(m, initialSp - 4u, node + 12u, i);
    map = R(m, f + 0x138u);
    U header = R(m, map + 4u);
    node = R(m, header);
    W(m, f + 0x34u, node);
    if (node != header) {
        U values = R(m, f + 0x28u);
        U cat = R(m, f + 0x3Cu);
        W(m, f + 0x14u, values);
        W(m, f + 0x28u, cat);
        bool first = true;
        for (;;) {
            if (!first)
                offset = R(m, f + 0x10u);
            first = false;
            node = R(m, f + 0x34u);
            CallKey(m, f, node + 12u, f + 0xA0u, f + 0xD8u, f + 0xF4u, i);
            bool flush = (Str(m, i, Op::unequal, f + 0xD8u, f + 0xBCu) & 255u) != 0;
            if (!flush)
                flush = (Str(m, i, Op::unequal, f + 0xA0u, f + 0x60u) & 255u) != 0;
            U records = 0;
            if (flush) {
                U start = R(m, f + 0x20u);
                U attrs = R(m, f + 0x2Cu);
                cat = R(m, f + 0x28u);
                records = R(m, f + 0x1Cu);
                U sp = f;
                Push(m, sp, f + 0x10u);
                const U attrBase = R(m, f + 0x40u);
                const U child = attrBase + start * 16u;
                const U recordStart = R(m, f + 0x18u);
                Push(m, sp, child);
                Push(m, sp, attrs);
                Push(m, sp, cat);
                const U recordBase = R(m, f + 0x24u);
                const U record = recordBase + (records + recordStart) * 16u;
                Push(m, sp, record);
                sp -= 28u;
                Str(m, i, Op::copy, sp, f + 0xBCu);
                NamedNode(m, sp - 4u, context, pool, i);
                start = R(m, f + 0x20u) + attrs;
                W(m, f + 0x20u, start);
                ++records;
                W(m, f + 0x2Cu, 0);
                W(m, f + 0x1Cu, records);
                Str(m, i, Op::assign, f + 0xBCu, f + 0xD8u);
                offset = R(m, f + 0x10u);
            } else
                records = R(m, f + 0x1Cu);
            if ((Str(m, i, Op::unequal, f + 0xA0u, f + 0x60u) & 255u) != 0) {
                const U recordStart = R(m, f + 0x18u);
                const U recordBase = R(m, f + 0x24u);
                const U root = R(m, f + 0x130u);
                U sp = f;
                Push(m, sp, f + 0x10u);
                const U child = recordBase + recordStart * 16u;
                cat = R(m, f + 0x28u);
                Push(m, sp, child);
                Push(m, sp, records);
                Push(m, sp, root);
                Push(m, sp, cat);
                sp -= 28u;
                Str(m, i, Op::copy, sp, f + 0x60u);
                NamedNode(m, sp - 4u, context, pool, i);
                const U nextStart = R(m, f + 0x18u) + records;
                U categoryIndex = R(m, f + 0x38u);
                W(m, f + 0x18u, nextStart);
                ++categoryIndex;
                cat += 16u;
                W(m, f + 0x38u, categoryIndex);
                W(m, f + 0x28u, cat);
                W(m, f + 0x1Cu, 0);
                Str(m, i, Op::assign, f + 0x60u, f + 0xA0u);
                offset = R(m, f + 0x10u);
            }
            node = R(m, f + 0x34u);
            const U rawType = R(m, node + 0x28u);
            W(m, f + 0x7Cu, rawType);
            U count = 0;
            Str(m, i, Op::copy, f + 0x80u, node + 0x2Cu);
            const U opaque = R(m, node + 0x48u);
            U start = 0;
            W(m, f + 0x9Cu, opaque);
            U delimiter = Str(m, i, Op::find, f + 0x80u, 59, 0);
            const U firstNposPointer = R(m, i.npos_pointer_cell);
            const bool hasFirst = delimiter != R(m, firstNposPointer);
            W(m, f + 0x30u, delimiter);
            if (hasFirst) {
                bool again = false;
                do {
                    delimiter = R(m, f + 0x30u);
                    Str(m, i, Op::substr, f + 0x80u, f + 0x44u, start, delimiter - start);
                    const U ctx = R(m, f + 0x134u);
                    U tokens = R(m, ctx + 12u);
                    const U type = R(m, f + 0x7Cu);
                    ++tokens;
                    W(m, ctx + 12u, tokens);
                    if (type >= 1 && type <= 8 && type != 5) {
                        if (type <= 3) {
                            const U source = Str(m, i, Op::c_str, f + 0x44u);
                            U bits = 0;
                            if (type == 2) {
                                if (!i.atof_fstp32_bits)
                                    throw Monstey_FearUnpackedBoundaryFault("atof_fstp32_bits");
                                bits = i.atof_fstp32_bits(m, source);
                            } else {
                                if (!i.atoi_bits)
                                    throw Monstey_FearUnpackedBoundaryFault("atoi_bits");
                                bits = i.atoi_bits(m, source);
                                if (type == 1)
                                    bits = bits ? 1u : 0u;
                            }
                            const U at = R(m, f + 0x14u) + count * 4u;
                            W(m, at, bits);
                            ++count;
                        } else if (type == 4) {
                            const U remaining = R(m, ctx + 20u) - pool - offset;
                            U source = Str(m, i, Op::c_str, f + 0x44u);
                            const U dest = pool + offset;
                            if (dest && remaining) {
                                if (source == 0)
                                    source = i.empty_cstring;
                                CopyBytes(m, i, dest, source, remaining - 1u);
                                W(m, dest + remaining - 1u, 0, 1);
                            }
                            W(m, R(m, f + 0x14u) + count * 4u, pool + offset);
                            ++count;
                            source = Str(m, i, Op::c_str, f + 0x44u);
                            offset += Length(m, source) + 1u;
                        } else {
                            const U dest = pool + offset;
                            const U dimensions = type - 4u;
                            offset += dimensions * 4u;
                            const U source = Str(m, i, Op::c_str, f + 0x44u);
                            if (!i.sscanf)
                                throw Monstey_FearUnpackedBoundaryFault("sscanf");
                            (void)i.sscanf(m, source, dimensions,
                                           {dest, dest + 4u, dest + 8u, dest + 12u});
                            W(m, R(m, f + 0x14u) + count * 4u, dest);
                            ++count;
                        }
                    }
                    start = R(m, f + 0x30u) + 1u;
                    delimiter = Str(m, i, Op::find, f + 0x80u, 59, start);
                    W(m, f + 0x30u, delimiter);
                    Str(m, i, Op::destroy, f + 0x44u);
                    const U loopNposPointer = R(m, i.npos_pointer_cell);
                    delimiter = R(m, f + 0x30u);
                    again = delimiter != R(m, loopNposPointer);
                } while (again);
                W(m, f + 0x10u, offset);
            }
            values = R(m, f + 0x14u);
            const U type = R(m, f + 0x7Cu);
            U recordStart = R(m, f + 0x18u);
            U sp = f;
            Push(m, sp, f + 0x10u);
            const U attrStart = R(m, f + 0x20u);
            Push(m, sp, values);
            Push(m, sp, count);
            U attrs = R(m, f + 0x2Cu);
            Push(m, sp, 0);
            Push(m, sp, type);
            const U recordBase = R(m, f + 0x24u);
            recordStart *= 16u;
            recordStart += recordBase;
            U attrIndex = attrs + attrStart;
            const U attrBase = R(m, f + 0x40u);
            Push(m, sp, recordStart);
            attrIndex *= 16u;
            Push(m, sp, attrIndex + attrBase);
            sp -= 28u;
            Str(m, i, Op::copy, sp, f + 0xF4u);
            const U helperContext = R(m, f + 0x134u);
            NamedAttribute(m, sp - 4u, helperContext, pool, i);
            values += 4u;
            ++attrs;
            W(m, f + 0x14u, values);
            W(m, f + 0x2Cu, attrs);
            Str(m, i, Op::destroy, f + 0x80u);
            Next(m, f + 0x34u);
            map = R(m, f + 0x138u);
            node = R(m, f + 0x34u);
            const U finishedHeader = R(m, map + 4u);
            context = R(m, f + 0x134u);
            if (node == finishedHeader)
                break;
        }
    }
    // Final record, then final category. These are unconditionally executed.
    U start = R(m, f + 0x18u);
    U records = R(m, f + 0x1Cu);
    U recordBase = R(m, f + 0x24u);
    U categoryBase = R(m, f + 0x3Cu);
    const U record = recordBase + (records + start) * 16u;
    U categoryIndex = R(m, f + 0x38u);
    U category = categoryBase + categoryIndex * 16u;
    U attributeStart = R(m, f + 0x20u);
    U sp = f;
    Push(m, sp, f + 0x10u);
    const U attributeBase = R(m, f + 0x40u);
    Push(m, sp, attributeBase + attributeStart * 16u);
    const U attributes = R(m, f + 0x2Cu);
    Push(m, sp, attributes);
    Push(m, sp, category);
    Push(m, sp, record);
    sp -= 28u;
    Str(m, i, Op::copy, sp, f + 0xBCu);
    NamedNode(m, sp - 4u, context, pool, i);
    recordBase = R(m, f + 0x24u);
    records = R(m, f + 0x1Cu);
    sp = f;
    Push(m, sp, f + 0x10u);
    Push(m, sp, recordBase + start * 16u);
    U root = R(m, f + 0x130u);
    ++records;
    Push(m, sp, records);
    Push(m, sp, root);
    Push(m, sp, category);
    sp -= 28u;
    Str(m, i, Op::copy, sp, f + 0x60u);
    NamedNode(m, sp - 4u, context, pool, i);
    const U allocation = R(m, f + 0x13Cu);
    categoryBase = R(m, f + 0x3Cu);
    const U categories = R(m, context);
    sp = f;
    Push(m, sp, allocation);
    const U name = R(m, i.root_name_cell);
    Push(m, sp, categoryBase);
    Push(m, sp, categories);
    Push(m, sp, name);
    Database(m, sp - 4u, root);
    Monstey_FearUnpackedFixLinks(m, root);
    for (U o : std::array<U, 6>{0xF4, 0xD8, 0xA0, 0x110, 0xBC, 0x60})
        Str(m, i, Op::destroy, f + o);
    return 1;
}
void Monstey_FearUnpackedForwardStrncpy(Mem m, U dst, U src, U count) {
    bool ended = false;
    for (U k = 0; k < count; ++k) {
        U byte = 0;
        if (!ended) {
            byte = R(m, src + k, 1);
            ended = byte == 0;
        }
        W(m, dst + k, byte, 1);
    }
}
namespace {
U Data(Mem m, U object) { return R(m, object + 0x18u) > 15 ? R(m, object + 4u) : object + 4u; }
std::string Text(Mem m, U object) {
    const U size = R(m, object + 0x14u);
    const U data = Data(m, object);
    std::string out;
    for (U n = 0; n < size; ++n)
        out.push_back(static_cast<char>(R(m, data + n, 1)));
    return out;
}
} // namespace
std::uint32_t Monstey_FearUnpackedLogicalStrings::Invoke(Mem m, Op op, U self, U a, U b,
                                                         U c) const {
    auto set = [&](U object, const std::string &text) {
        if (text.size() > UINT32_MAX)
            throw std::length_error("Logical FEAR string size");
        const U length = static_cast<U>(text.size());
        U data = object + 4u, capacity = 15;
        if (length > 15) {
            if (!allocate)
                throw Monstey_FearUnpackedBoundaryFault("logical_string_allocate");
            data = allocate(m, length + 1u);
            W(m, object + 4u, data);
            capacity = length;
        }
        for (U n = 0; n < length; ++n)
            W(m, data + n, static_cast<std::uint8_t>(text[n]), 1);
        W(m, data + length, 0, 1);
        W(m, object + 0x14u, length);
        W(m, object + 0x18u, capacity);
        return object;
    };
    switch (op) {
    case Op::construct_cstring: {
        std::string text;
        U at = a;
        for (;;) {
            const U byte = R(m, at++, 1);
            if (byte == 0)
                break;
            text.push_back(static_cast<char>(byte));
        }
        return set(self, text);
    }
    case Op::less: {
        const auto left = Text(m, self);
        const auto right = Text(m, a);
        return left < right ? 1u : 0u;
    }
    case Op::construct:
        W(m, self + 0x18u, 15);
        W(m, self + 0x14u, 0);
        W(m, self + 4u, 0, 1);
        return self;
    case Op::destroy:
        return self; // Borrowed arena profile: caller retains all raw allocations.
    case Op::copy:
    case Op::assign:
        return set(self, Text(m, a));
    case Op::clear:
        return set(self, {});
    case Op::c_str:
        return Data(m, self);
    case Op::subscript: {
        const U size = R(m, self + 0x14u);
        if (a > size)
            throw std::out_of_range("Logical FEAR string subscript");
        return Data(m, self) + a;
    }
    case Op::append: {
        auto text = Text(m, self);
        text.push_back(static_cast<char>(a));
        return set(self, text);
    }
    case Op::find:
    case Op::rfind: {
        const auto text = Text(m, self);
        const auto pos = op == Op::find ? text.find(static_cast<char>(a), b)
                                        : text.rfind(static_cast<char>(a), b);
        return pos == std::string::npos ? UINT32_MAX : static_cast<U>(pos);
    }
    case Op::substr: {
        const auto text = Text(m, self);
        return set(a, text.substr(b, c));
    }
    case Op::unequal: {
        const auto left = Text(m, self);
        const auto right = Text(m, a);
        return left != right ? 1u : 0u;
    }
    }
    throw std::invalid_argument("Unknown logical FEAR string primitive");
}
} // namespace Monstey
