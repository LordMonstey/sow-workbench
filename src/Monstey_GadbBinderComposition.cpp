#include "Monstey_GadbBinderComposition.hpp"
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
Monstey_GadbBinderFault::Monstey_GadbBinderFault(
    Monstey_GadbBinderFaultKind k, std::uint64_t a, const char *message)
    : std::runtime_error(message), kind(k), address(a) {}
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
using Kind = Monstey_GadbBinderEventKind;
class Monstey_BinderRun {
  Monstey_GadbBinderMemory memory;
  Monstey_GadbBinderTrace &trace;
  U64 remaining;
  void Consume(U64 address) {
    if (remaining == 0)
      throw Monstey_GadbBinderFault(
          Monstey_GadbBinderFaultKind::OwnedStepLimit, address,
          "Owned binder diagnostic step budget exhausted");
    --remaining;
    ++trace.consumed_operations;
  }
  std::size_t Offset(U64 address, std::size_t size) {
    Consume(address);
    if (address < memory.base || address - memory.base > memory.bytes.size() ||
        size > memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
      throw Monstey_GadbBinderFault(
          Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
          "Consumed native binder range is not present in the owned image");
    return static_cast<std::size_t>(address - memory.base);
  }
  U64 Read(U64 a, std::size_t width) {
    const auto p = Offset(a, width);
    U64 v = 0;
    for (std::size_t i = 0; i < width; ++i)
      v |= static_cast<U64>(memory.bytes[p + i]) << (8 * i);
    return v;
  }
  void Write(U64 a, U64 value, std::size_t width) {
    const auto p = Offset(a, width);
    for (std::size_t i = 0; i < width; ++i)
      memory.bytes[p + i] = static_cast<std::uint8_t>(value >> (8 * i));
  }
  U32 HashCString(U64 p) {
    U32 h = 0;
    for (;;) {
      const auto c = static_cast<char>(Read(p, 1));
      if (c == 0)
        return h;
      h = Monstey_NativeHash(std::string_view(&c, 1), h);
      ++p;
    }
  }
  std::uint8_t ConstantIndex(U64 p) {
    const auto first = Read(p, 1);
    if (first == 0xb0) {
      const auto result = static_cast<std::uint8_t>(Read(p + 1, 1));
      if (Read(p + 2, 1) == 0xc3)
        return result;
    } else if (first == 0x32 || first == 0x30 || first == 0x33) {
      if (Read(p + 1, 1) == 0xc0 && Read(p + 2, 1) == 0xc3)
        return 0;
    } else if (first == 0xb8) {
      const auto result = static_cast<std::uint8_t>(Read(p + 1, 4));
      if (Read(p + 5, 1) == 0xc3)
        return result;
    }
    throw Monstey_GadbBinderFault(
        Monstey_GadbBinderFaultKind::UnsupportedIndexCallback, p,
        "Consumed index callback is outside the qualified constant literal forms");
  }
  void Note(Kind kind, U64 subject, U64 other = 0, U32 value = 0, U32 aux = 0) {
    trace.events.push_back({kind, subject, other, value, aux});
  }
public:
  Monstey_BinderRun(Monstey_GadbBinderMemory m, Monstey_GadbBinderTrace &t, U64 n)
      : memory(m), trace(t), remaining(n) {}
  void Initialize(U64 selected) {
    // Explicit frames preserve recursive parent-first order without borrowing
    // C++ references into a potentially growing vector.
    struct Frame { U64 cl; bool after_parent; };
    std::vector<Frame> frames{{selected, false}};
    while (!frames.empty()) {
      const auto frame = frames.back();
      if (!frame.after_parent) {
        Note(Kind::InitializeEnter, frame.cl);
        if (Read(frame.cl + 0x3c, 1) != 0) {
          frames.pop_back();
          continue;
        }
        const auto parent = Read(frame.cl + 0x28, 8);
        frames.back().after_parent = true;
        if (parent != 0) {
          frames.push_back({parent, false});
          continue;
        }
      }
      if ((Read(frame.cl + 0x38, 1) & 4) == 0) {
        U32 ordinal = 0, compact = 0;
        while (ordinal < Read(frame.cl + 8, 4)) {
          const auto row = Read(frame.cl, 8) + static_cast<U64>(ordinal) * 72;
          if (row != 0 && (Read(row + 0x2c, 4) & 0x280) == 0) {
            Write(row + 0x40, compact, 1);
            Note(Kind::CacheWrite, row, frame.cl, compact & 255, ordinal);
            ++compact;
          }
          ++ordinal;
        }
      }
      Write(frame.cl + 0x3c, 1, 1);
      Note(Kind::InitializeComplete, frame.cl);
      frames.pop_back();
    }
  }
  void Propagate(U64 selected, U64 field, U32 index, bool success) {
    Note(success ? Kind::SuccessCall : Kind::FallbackCall, field, selected, index);
    for (;;) {
      const auto state = static_cast<U32>(Read(field + 0x30, 4));
      if (success ? state != 0 : state == 1)
        return;
      const U32 next = success ? 2u : 1u;
      Write(field + 0x30, next, 4);
      Note(Kind::StateWrite, field, selected, next, state);
      selected = Read(selected + 0x28, 8);
      if (selected == 0 || index >= Read(selected + 8, 4))
        return;
      const auto parent_field = Read(selected, 8) + static_cast<U64>(index) * 72;
      for (const U64 member : {0x20ull, 0x24ull, 0x2cull, 0x28ull, 0x40ull}) {
        const std::size_t width = member == 0x40 ? 1 : 4;
        const auto source_value = Read(field + member, width);
        if (Read(parent_field + member, width) != source_value)
          throw Monstey_GadbBinderFault(
              Monstey_GadbBinderFaultKind::NativeParentCorrespondenceAssertion,
              parent_field + member,
              "Native same-index parent correspondence assertion path");
      }
      Note(Kind::ParentPropagation, field, parent_field, index);
      field = parent_field;
    }
  }
  bool HasRecords(U64 category) {
    return Read(category + 0x28, 4) != 0 || Read(category + 0x40, 4) != 0;
  }
  void Bind(U64 selected, U64 category) {
    // Entry after class-name selection. Do not repeat its earlier count gate.
    if (selected == 0)
      return;
    Initialize(selected);
    Write(category + 0x20, selected, 8);
    Note(Kind::CategoryBound, category, selected);
    U64 record = 0;
    if (Read(category + 0x28, 4) != 0)
      record = Read(category + 0x38, 8);
    else {
      if (Read(category + 0x40, 4) == 0)
        return;
      record = Read(category + 0x48, 8);
    }
    if (record == 0)
      return;
    const auto field_count = static_cast<U32>(Read(selected + 8, 4));
    const auto class_mask = static_cast<U32>(Read(selected + 0x38, 4)) & 4u;
    const auto descriptor_count = static_cast<std::uint8_t>(Read(record, 1));
    Note(Kind::LoopSamples, selected, record, field_count,
         descriptor_count | (class_mask << 8));
    for (U32 index = 0; index < field_count; ++index) {
      const auto field = Read(selected, 8) + static_cast<U64>(index) * 72;
      if (Read(field + 0x30, 4) == 1) {
        Note(Kind::StateSkipped, field, selected, index);
        continue;
      }
      if ((Read(field + 0x2c, 4) & 0x280) != 0) {
        Note(Kind::MarkerSkipped, field, selected, index);
        continue;
      }
      const auto cache = static_cast<std::uint8_t>(Read(field + 0x40, 1));
      if (cache >= descriptor_count) {
        Propagate(selected, field, index, false);
        continue;
      }
      const auto descriptor = Read(record + 8, 8) + static_cast<U64>(cache) * 16;
      const auto identity = static_cast<std::uint8_t>(Read(descriptor + 8, 1));
      const auto hash = HashCString(Read(field, 8));
      const auto stored_type = Read(descriptor + 0x0a, 1) & 0x3f;
      const auto declared_type = Read(field + 0x24, 4);
      const bool type_ok = declared_type == stored_type ||
                           (declared_type == 11 && stored_type == 10);
      const auto stored_count = Read(descriptor + 0x0c, 2);
      const auto declared_count = Read(field + 0x28, 4);
      const bool count_ok = declared_count == stored_count ||
                            declared_count == UINT32_MAX;
      const auto callback = Read(field + 0x38, 8);
      const auto returned = ConstantIndex(callback);
      Note(Kind::IndexCallback, field, callback, returned, identity);
      const bool success = returned == identity && Read(descriptor, 4) == hash &&
                           type_ok && count_ok && class_mask == 0;
      Propagate(selected, field, index, success);
    }
  }
};
} // namespace
void Monstey_GadbInitializeSelectedClass(Monstey_GadbBinderMemory memory,
    std::uint64_t selected, Monstey_GadbBinderTrace &trace, std::uint64_t budget) {
  Monstey_BinderRun(memory, trace, budget).Initialize(selected);
}
void Monstey_GadbPropagateSelectedField(Monstey_GadbBinderMemory memory,
    std::uint64_t selected, std::uint64_t field, std::uint32_t index, bool success,
    Monstey_GadbBinderTrace &trace, std::uint64_t budget) {
  Monstey_BinderRun(memory, trace, budget).Propagate(selected, field, index, success);
}
bool Monstey_GadbCategoryHasBindingRecords(Monstey_GadbBinderMemory memory,
    std::uint64_t category, Monstey_GadbBinderTrace &trace, std::uint64_t budget) {
  return Monstey_BinderRun(memory, trace, budget).HasRecords(category);
}
void Monstey_GadbBindSelectedCategory(Monstey_GadbBinderMemory memory,
    std::uint64_t selected, std::uint64_t category, Monstey_GadbBinderTrace &trace,
    std::uint64_t budget) {
  Monstey_BinderRun(memory, trace, budget).Bind(selected, category);
}
} // namespace Monstey
