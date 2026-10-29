#include "Monstey_ParentDifferential.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <charconv>
#include <iostream>
#include <set>

namespace
{
using Monstey_J = Monstey::Monstey_Json;
using namespace Monstey_ParentDifferential;
Monstey_J Monstey_Words(const std::vector<Monstey_U64> &words)
{
    Monstey_J::Array values;
    for (const auto word : words)
    {
        char buffer[16];
        const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), word, 16);
        if (converted.ec != std::errc{}) throw std::logic_error("raw word formatting failed");
        values.push_back("0x" + std::string(buffer, converted.ptr));
    }
    return values;
}
Monstey_J Monstey_Describe(const Monstey_Outcome &o)
{
    Monstey_J::Array raw;
    for (const auto &r : o.results) raw.push_back(Monstey_Words({r[0], r[1], r[2], r[3]}));
    Monstey_J fault;
    if (o.fault)
        fault = Monstey_J::object({{"kind", static_cast<std::uint64_t>(o.fault->kind)},
            {"words", Monstey_Words(o.fault->words)}, {"label", o.fault->label},
            {"supplied_token_preserved", o.fault->supplied_token_preserved}});
    return Monstey_J::object({{"raw", raw}, {"fault", fault}, {"observer_failed", o.observer_failed},
        {"events", static_cast<std::uint64_t>(o.events.size())}});
}
Monstey_J Monstey_FirstImageDifference(const Monstey_Bytes &a, const Monstey_Bytes &b)
{
    const auto count = (std::min)(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i)
        if (a[i] != b[i])
            return Monstey_J::object({{"offset", static_cast<std::uint64_t>(i)},
                {"reference_byte", static_cast<std::uint64_t>(a[i])},
                {"product_byte", static_cast<std::uint64_t>(b[i])}});
    if (a.size() != b.size())
        return Monstey_J::object({{"reference_size", static_cast<std::uint64_t>(a.size())},
            {"product_size", static_cast<std::uint64_t>(b.size())}});
    return Monstey_J{};
}
Monstey_J Monstey_FirstEventDifference(const Monstey_Outcome &a, const Monstey_Outcome &b)
{
    const auto count = (std::min)(a.events.size(), b.events.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto &x = a.events[i]; const auto &y = b.events[i];
        if (x == y) continue;
        return Monstey_J::object({{"index", static_cast<std::uint64_t>(i)},
            {"reference_primitive", static_cast<std::uint64_t>(x.primitive)},
            {"product_primitive", static_cast<std::uint64_t>(y.primitive)},
            {"reference_phase", static_cast<std::uint64_t>(x.phase)},
            {"product_phase", static_cast<std::uint64_t>(y.phase)},
            {"reference_words", Monstey_Words(x.words)}, {"product_words", Monstey_Words(y.words)},
            {"entry_or_return_image_difference", Monstey_FirstImageDifference(x.image, y.image)}});
    }
    if (a.events.size() != b.events.size())
        return Monstey_J::object({{"first_missing_event", static_cast<std::uint64_t>(count)}});
    return Monstey_J{};
}
}

int wmain(int argc, wchar_t **argv)
{
    using namespace Monstey_ParentDifferential;
    using J = Monstey::Monstey_Json;
    try
    {
        if (argc != 2 && argc != 3)
            throw std::invalid_argument("expected fresh report path and optional case substring");
        const auto cases = Monstey_Cases();
        const auto filter = argc == 3 ? Monstey::Monstey_Utf8(argv[2]) : std::string{};
        std::size_t executed = 0;
        std::set<std::string> names;
        J::Array rows;
        std::size_t mismatches = 0, literal_failures = 0, faults = 0;
        for (const auto &c : cases)
        {
            if (!names.insert(c.id).second)
                throw std::logic_error("duplicate independent case ID");
            if (!filter.empty() && c.id.find(filter) == std::string::npos) continue;
            ++executed;
            const auto reference = Monstey_RunReference(c);
            const auto product = Monstey_RunProduct(c);
            const auto equal = Monstey_Equal(reference, product);
            const auto reference_literal = Monstey_MeetsLiteralExpectations(c, reference);
            const auto product_literal = Monstey_MeetsLiteralExpectations(c, product);
            mismatches += equal ? 0u : 1u;
            literal_failures += reference_literal && product_literal ? 0u : 1u;
            faults += product.fault ? 1u : 0u;
            J::Array criteria;
            for (const auto &id : c.criteria) criteria.push_back(id);
            auto row = J::object({{"id", c.id}, {"criteria", criteria}, {"equal", equal},
                {"reference_literal", reference_literal}, {"product_literal", product_literal},
                {"primitive_events", static_cast<std::uint64_t>(product.events.size())},
                {"full_image_bytes_compared", static_cast<std::uint64_t>(product.image.size())},
                {"fault", product.fault.has_value()}});
            if (!equal || !reference_literal || !product_literal)
            {
                row["reference"] = Monstey_Describe(reference);
                row["product"] = Monstey_Describe(product);
                row["first_image_difference"] = Monstey_FirstImageDifference(reference.image, product.image);
                row["first_service_event_difference"] = Monstey_FirstEventDifference(reference, product);
            }
            rows.push_back(std::move(row));
            if (!equal || !reference_literal || !product_literal) break;
        }
        if (!executed) throw std::invalid_argument("case filter selected no case");
        auto report = J::object({{"status", mismatches || literal_failures ? "FAIL" : "PASS"},
            {"cases", static_cast<std::uint64_t>(executed)}, {"mismatches", static_cast<std::uint64_t>(mismatches)},
            {"literal_failures", static_cast<std::uint64_t>(literal_failures)},
            {"fault_cases", static_cast<std::uint64_t>(faults)}, {"rows", rows},
            {"physical_CPU_OS_concurrency_qualified", false}, {"passive_product_memory_trace", false},
            {"native_original_entrypoint_invoked", false}, {"original_instruction_bytes_interpreted", false},
            {"full_higher239_qualified", false}, {"full_goal_complete", false}});
        Monstey::Monstey_WriteNewText(std::filesystem::path(argv[1]), report.dump(2) + "\n");
        std::cout << (mismatches || literal_failures ? "FAIL" : "PASS") << ' ' << executed << " cases "
                  << mismatches << " differences " << literal_failures << " literal failures\n";
        return mismatches || literal_failures ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "HARNESS_FAILURE " << e.what() << '\n';
        return 2;
    }
}
