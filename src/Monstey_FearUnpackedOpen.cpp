#include "Monstey_FearUnpackedOpen.hpp"
#include <string>
namespace Monstey {
Monstey_FearUnpackedOpenBoundaryFault::Monstey_FearUnpackedOpenBoundaryFault(const char *name,
                                                                             std::uint32_t site,
                                                                             std::uint32_t value)
    : std::runtime_error(std::string("Consumed unpacked Open boundary: ") + name),
      native_site(site), target(value) {}
namespace {
using U = std::uint32_t;
using Mem = Monstey_FearDbMutableMemory;
using Services = Monstey_FearUnpackedOpenServices;
U R(const Mem &m, U at, U width = 4) {
    if (at < m.base || std::uint64_t(at) + width > 0x100000000ull ||
        std::uint64_t(at) - m.base > m.bytes.size() ||
        width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
        throw Monstey_FearDbFault(at, width, false);
    U value = 0;
    for (U n = 0; n < width; ++n)
        value |= U(m.bytes[at - m.base + n]) << (n * 8);
    return value;
}
void W(Mem &m, U at, U value, U width = 4) {
    if (at < m.base || std::uint64_t(at) + width > 0x100000000ull ||
        std::uint64_t(at) - m.base > m.bytes.size() ||
        width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
        throw Monstey_FearDbFault(at, width, true);
    for (U n = 0; n < width; ++n)
        m.bytes[at - m.base + n] = static_cast<std::uint8_t>(value >> (n * 8));
}
void Push(Mem &m, U &sp, U value) {
    sp -= 4u;
    W(m, sp, value);
}
U Sentinel(Mem &m, Services &s, bool parsed, U map, U site) {
    if (!s.new_sentinel)
        throw Monstey_FearUnpackedOpenBoundaryFault("new_sentinel", site);
    return s.new_sentinel(m, parsed, map, site);
}
U NewArray(Mem &m, Services &s, U site, U bytes) {
    if (!s.new_array)
        throw Monstey_FearUnpackedOpenBoundaryFault("new_array", site);
    return s.new_array(m, site, bytes);
}
void DeleteArray(Mem &m, Services &s, U site, U address) {
    if (!s.delete_array)
        throw Monstey_FearUnpackedOpenBoundaryFault("delete_array", site);
    s.delete_array(m, {site, address});
}
void DeleteScalar(Mem &m, Services &s, U site, U address) {
    if (!s.cleanup.delete_scalar)
        throw Monstey_FearUnpackedOpenBoundaryFault("delete_scalar", site);
    s.cleanup.delete_scalar(m, {site, address});
}
void Erase(Mem &m, Services &s, U site, U map, U sp, bool parsed) {
    const U hidden = R(m, sp), first = R(m, sp + 4u), last = R(m, sp + 8u);
    if (!s.cleanup.erase_range)
        throw Monstey_FearUnpackedOpenBoundaryFault("erase_range", site);
    (void)s.cleanup.erase_range(m, {parsed, site, map, hidden, first, last});
}
void FinishMaps(Mem &m, Services &s, U f) {
    Monstey_FearUnpackedCleanup(m, f + 0x1Cu, f - 0x0Cu, true, s.cleanup);
    Monstey_FearUnpackedCleanup(m, f + 0x28u, f - 0x0Cu, false, s.cleanup);
}
} // namespace
std::uint32_t Monstey_FearOpenUnpackedBody(Mem &m, U f, Services &s) {
    const U a = f + 8u;
    U sp = a;
    const U constraintHead = Sentinel(m, s, false, a + 0x20u, 0x10006209u);
    const U constraintStream = R(m, a + 0x4Cu);
    W(m, a + 0x24u, constraintHead);
    W(m, constraintHead + 0x35u, 1, 1);
    U head = R(m, a + 0x24u);
    W(m, head + 4u, head);
    head = R(m, a + 0x24u);
    W(m, head, head);
    head = R(m, a + 0x24u);
    W(m, head + 8u, head);
    Push(m, sp, a + 0x20u);
    Push(m, sp, constraintStream);
    W(m, a + 0x28u, 0);
    const bool constraintsOk = Monstey_FearReadConstraints(m, a - 0x50u, s.constraints);
    sp += 8u;
    if (!constraintsOk) {
        head = R(m, a + 0x24u);
        const U first = R(m, head);
        Push(m, sp, head);
        Push(m, sp, first);
        Push(m, sp, a + 8u);
        Erase(m, s, 0x10006258u, a + 0x20u, sp, false);
        sp += 12u;
        head = R(m, a + 0x24u);
        Push(m, sp, head);
        DeleteScalar(m, s, 0x10006262u, head);
        return 0;
    }
    const U textStream = R(m, a + 0x48u);
    const U lengthVtable = R(m, textStream);
    sp = f;
    const U lengthTarget = R(m, lengthVtable + 0x1Cu);
    if (!s.get_length)
        throw Monstey_FearUnpackedOpenBoundaryFault("get_length", 0x1000627Cu, lengthTarget);
    const U length = static_cast<U>(s.get_length(m, {lengthTarget, textStream}));
    const U parsedHead = Sentinel(m, s, true, f + 0x1Cu, 0x10006285u);
    W(m, f + 0x20u, parsedHead);
    W(m, parsedHead + 0x4Du, 1, 1);
    head = R(m, f + 0x20u);
    W(m, head + 4u, head);
    head = R(m, f + 0x20u);
    W(m, head, head);
    head = R(m, f + 0x20u);
    W(m, head + 8u, head);
    const U constraintMap = f + 0x28u, parsedMap = f + 0x1Cu;
    Push(m, sp, length);
    W(m, f + 0x24u, 0);
    W(m, f + 0x14u, constraintMap);
    W(m, f + 0x18u, parsedMap);
    const U text = NewArray(m, s, 0x100062BAu, length);
    const U readVtable = R(m, textStream);
    sp += 4u;
    Push(m, sp, length);
    Push(m, sp, text);
    const U readTarget = R(m, readVtable + 4u);
    if (!s.read)
        throw Monstey_FearUnpackedOpenBoundaryFault("read", 0x100062CAu, readTarget);
    const U readResult = s.read(m, {readTarget, textStream, text, length});
    sp += 8u;
    if (readResult != 0) {
        head = R(m, f + 0x20u);
        const U first = R(m, head);
        Push(m, sp, head);
        Push(m, sp, first);
        Push(m, sp, f + 0x10u);
        Erase(m, s, 0x100062E2u, f + 0x1Cu, sp, true);
        sp += 12u;
        head = R(m, f + 0x20u);
        Push(m, sp, head);
        DeleteScalar(m, s, 0x100062ECu, head);
        const U cachedConstraintHead = R(m, f + 0x2Cu);
        sp += 4u;
        W(m, f + 0x20u, 0);
        W(m, f + 0x24u, 0);
        const U constraintFirst = R(m, cachedConstraintHead);
        Push(m, sp, cachedConstraintHead);
        Push(m, sp, constraintFirst);
        Push(m, sp, f + 0x10u);
        Erase(m, s, 0x1000630Du, f + 0x28u, sp, false);
        sp += 12u;
        head = R(m, f + 0x2Cu);
        Push(m, sp, head);
        DeleteScalar(m, s, 0x10006317u, head);
        return 0;
    }
    Push(m, sp, f + 0x14u);
    Push(m, sp, 0x10005EC0u);
    Push(m, sp, length);
    Push(m, sp, text);
    const U b = ((f - 0x18u) & 0xFFFFFFF8u) - 0x410u;
    Monstey_FearUnpackedLineDispatch dispatch = [&](Mem &view, U callback, U line, U context) -> U {
        U callsp = b;
        Push(view, callsp, context);
        Push(view, callsp, line);
        if (callback != 0x10005EC0u)
            throw Monstey_FearUnpackedOpenBoundaryFault("callback_identity", 0x10009E0Au, callback);
        return Monstey_FearUnpackedLineCallback(view, b - 0x9Cu, b - 8u, b - 4u, s.callback);
    };
    const U lineResult = Monstey_FearUnpackedFrameLines(m, sp, b + 0x10u, dispatch);
    sp += 16u;
    Push(m, sp, text);
    if ((lineResult & 255u) == 0) {
        DeleteArray(m, s, 0x1000640Bu, text);
        sp += 4u;
        FinishMaps(m, s, f);
        return 0;
    }
    DeleteArray(m, s, 0x10006346u, text);
    const U sizingMap = f + 0x1Cu;
    Push(m, sp, sizingMap);
    const U context = sp + 0x3Cu;
    for (U n = 0; n < 6u; ++n)
        W(m, sp + 0x3Cu + n * 4u, 0);
    Monstey_FearUnpackedSize(m, context, f - 0xC4u, s.materializer);
    const U count = R(m, sp + 0x3Cu);
    sp += 8u;
    if (count == 0) {
        Push(m, sp, 20);
        if (!s.new_scalar)
            throw Monstey_FearUnpackedOpenBoundaryFault("new_scalar", 0x1000637Eu);
        const U root = s.new_scalar(m, 0x1000637Eu, 20);
        sp += 4u;
        W(m, root, 0);
        W(m, root + 4u, 0);
        W(m, root + 8u, 0);
        W(m, root + 12u, 0);
        W(m, root + 16u, 0);
        FinishMaps(m, s, f);
        return root;
    }
    U sum = R(m, f + 0x38u);
    sum = R(m, f + 0x3Cu) + sum;
    sum += count;
    const U valueCount = R(m, f + 0x40u);
    U units = valueCount + sum * 4u;
    const U pool = R(m, f + 0x44u);
    const U bytes = pool + units * 4u + 20u;
    Push(m, sp, bytes);
    const U root = NewArray(m, s, 0x100063D2u, bytes);
    sp += 4u;
    if (root == 0) {
        FinishMaps(m, s, f);
        return 0;
    }
    const U end = root + bytes;
    Push(m, sp, root);
    W(m, f + 0x48u, end);
    Push(m, sp, f + 0x1Cu);
    Push(m, sp, f + 0x34u);
    W(m, f + 0x10u, root + 20u);
    Push(m, sp, root);
    const auto materialized =
        Monstey_FearUnpackedMaterialize(m, f + 0x10u, f - 0x140u, s.materializer);
    sp += 16u;
    if ((materialized & 255u) != 0) {
        FinishMaps(m, s, f);
        return root;
    }
    Push(m, sp, root);
    DeleteArray(m, s, 0x1000640Bu, root);
    sp += 4u;
    FinishMaps(m, s, f);
    return 0;
}
std::uint32_t Monstey_FearOpenUnpacked(Mem &m, U w, Services &s) {
    const U constraints = R(m, w + 8u);
    const U text = R(m, w + 4u);
    U sp = w;
    Push(m, sp, constraints);
    Push(m, sp, text);
    const U result = Monstey_FearOpenUnpackedBody(m, w - 0x58u, s);
    if (result != 0)
        W(m, result + 0x10u, R(m, result + 0x10u) + 1u);
    return result;
}
} // namespace Monstey
