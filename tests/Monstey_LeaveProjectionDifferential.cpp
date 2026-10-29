#include "Monstey_LeaveProjectionReference.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_Memory = Monstey::Monstey_GadbBinderMemory;
using Monstey_Raw = Monstey::Monstey_GadbBufferedResult;
using Monstey_Result = Monstey::Monstey_GadbManagerLeaveProjectionResult;
using Monstey_Call = Monstey::Monstey_GadbManagerSyncCall;
using Monstey_Bindings = Monstey::Monstey_GadbManagerPrimitiveBindings;
using Monstey_Services = Monstey::Monstey_GadbManagerLeaveProjectionServices;
constexpr Monstey_Raw Monstey_In{0xF00123456789ABCDULL, 0x8A10203040506070ULL};
constexpr Monstey_U64 Monstey_Lock = 0xA123456789ABCDEFULL;
constexpr Monstey_U64 Monstey_Target = 0xF9876543210ABCDEULL;
struct Monstey_TestFailure : std::runtime_error { using std::runtime_error::runtime_error; };
struct Monstey_Case { std::string name, detail; std::size_t checks; bool pass; };
std::vector<Monstey_Case> Monstey_Cases;
std::size_t Monstey_Checks = 0, Monstey_Providers = 0, Monstey_Old = 0, Monstey_ImageBytes = 0;
struct Monstey_Observation {
  std::string kind;
  std::vector<Monstey_U64> words;
  std::vector<std::uint8_t> image;
  bool operator==(const Monstey_Observation&) const = default;
};
struct Monstey_LegTotals {
  std::size_t checks = 0, providers = 0, old_calls = 0, image_bytes = 0, events = 0;
};
bool Monstey_Actual = false, Monstey_Stop = false;
std::vector<Monstey_Observation>* Monstey_ActiveTrace = nullptr;
std::vector<Monstey_Memory> Monstey_ActiveImages;
Monstey_LegTotals Monstey_ReferenceTotals, Monstey_ActualTotals;
std::vector<Monstey_Observation> Monstey_FailureReference, Monstey_FailureActual;
std::size_t Monstey_DiffChecks = 0, Monstey_DiffImageBytes = 0;
void Monstey_Record(std::string kind, std::vector<Monstey_U64> words) {
  if (!Monstey_ActiveTrace) return;
  Monstey_Observation event{std::move(kind), std::move(words), {}};
  if (!Monstey_ActiveImages.empty()) {
    const auto memory = Monstey_ActiveImages.back();
    event.image.assign(memory.bytes.begin(), memory.bytes.end());
  }
  Monstey_ActiveTrace->push_back(std::move(event));
}
struct Monstey_ActiveImageScope {
  explicit Monstey_ActiveImageScope(Monstey_Memory memory) { Monstey_ActiveImages.push_back(memory); }
  ~Monstey_ActiveImageScope() { Monstey_ActiveImages.pop_back(); }
};
void Monstey_RecordException();
void Monstey_Check(bool ok, const char* detail) {
  ++Monstey_Checks; if (!ok) throw Monstey_TestFailure(detail);
}
template<class Function> void Monstey_Run(const std::string& name, Function&& function) {
  if (Monstey_Stop) return;
  const auto start = Monstey_Checks;
  Monstey_Case item{name, "", 0, false};
  std::vector<Monstey_Observation> reference, actual;
  Monstey_LegTotals ref_case, actual_case;
  auto leg = [&](bool is_actual, std::vector<Monstey_Observation>& trace,
                 Monstey_LegTotals& totals, Monstey_LegTotals& one) {
    Monstey_Actual = is_actual; Monstey_ActiveTrace = &trace;
    const auto checks = Monstey_Checks, providers = Monstey_Providers;
    const auto old_calls = Monstey_Old, bytes = Monstey_ImageBytes;
    function();
    one = {Monstey_Checks - checks, Monstey_Providers - providers,
           Monstey_Old - old_calls, Monstey_ImageBytes - bytes, trace.size()};
    totals.checks += one.checks; totals.providers += one.providers;
    totals.old_calls += one.old_calls; totals.image_bytes += one.image_bytes;
    totals.events += one.events;
  };
  try {
    leg(false, reference, Monstey_ReferenceTotals, ref_case);
    leg(true, actual, Monstey_ActualTotals, actual_case);
    Monstey_ActiveTrace = nullptr;
    auto compare = [&](bool equal, const char* detail) {
      ++Monstey_DiffChecks; Monstey_Check(equal, detail);
    };
    compare(ref_case.checks == actual_case.checks, "paired fixture check count");
    compare(ref_case.providers == actual_case.providers, "paired primitive call count");
    compare(ref_case.old_calls == actual_case.old_calls, "paired old regression count");
    compare(ref_case.image_bytes == actual_case.image_bytes, "paired golden image bytes");
    compare(reference.size() == actual.size(), "paired observation count");
    for (std::size_t i = 0; i < reference.size(); ++i) {
      compare(reference[i].kind == actual[i].kind, "paired event kind/order");
      compare(reference[i].words == actual[i].words, "paired Raw4/DTO/fault words");
      Monstey_DiffImageBytes += reference[i].image.size();
      compare(reference[i].image == actual[i].image, "paired full image/prefix");
    }
    item.pass = true;
  } catch (const std::exception& e) {
    item.detail = (Monstey_Actual ? "actual: " : "reference: ") + std::string(e.what());
  } catch (...) {
    item.detail = Monstey_Actual ? "actual: nonstandard exception" : "reference: nonstandard exception";
  }
  Monstey_ActiveTrace = nullptr;
  if (!item.pass) {
    Monstey_Stop = true; Monstey_FailureReference = reference; Monstey_FailureActual = actual;
  }
  item.checks = Monstey_Checks - start; Monstey_Cases.push_back(std::move(item));
}
void Monstey_ImageEqual(const std::vector<std::uint8_t>& a,
    const std::vector<std::uint8_t>& b) {
  Monstey_ImageBytes += a.size(); Monstey_Check(a == b, "full image differs");
}
Monstey_Result Monstey_Reply(std::size_t variant = 0) {
  const auto v = static_cast<Monstey_U64>(variant);
  return {{0xF123456789ABCDEFULL ^ v, 0x8FEDCBA987654321ULL ^ (v << 4)},
          0x90123456AABBCCDDULL ^ (v << 8), 0xFFEEDDCC10203040ULL ^ (v << 12)};
}
void Monstey_Equal(const Monstey_Result& a, const Monstey_Result& b) {
  Monstey_Check(a.raw.rax == b.raw.rax, "RAX");
  Monstey_Check(a.raw.rdx == b.raw.rdx, "RDX");
  Monstey_Check(a.post_leave_r8 == b.post_leave_r8, "R8");
  Monstey_Check(a.post_leave_r9 == b.post_leave_r9, "R9");
}
struct Monstey_Image {
  Monstey_U64 base; std::vector<std::uint8_t> data;
  explicit Monstey_Image(Monstey_U64 first = 0x1000, std::size_t size = 128)
      : base(first), data(size, 0xA5) {}
  Monstey_U64 At(std::size_t offset) const { return base + static_cast<Monstey_U64>(offset); }
  Monstey_Memory View() { return {base, data}; }
  void Put(std::size_t offset, Monstey_U64 value) {
    for (std::size_t i = 0; i < 8; ++i)
      data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
  }
  Monstey_U64 Get(std::size_t offset) const {
    Monstey_U64 value = 0;
    for (std::size_t i = 0; i < 8; ++i)
      value |= static_cast<Monstey_U64>(data.at(offset + i)) << (i * 8);
    return value;
  }
};
Monstey_Result Monstey_Invoke(bool destroy, Monstey_Image& image, Monstey_U64 guard, Monstey_U64 lock,
    Monstey_Raw incoming, const Monstey_Bindings& bindings, const Monstey_Services& services) {
  Monstey_ActiveImageScope scope(image.View());
  Monstey_Record("entry", {static_cast<Monstey_U64>(destroy), guard, lock,
      incoming.rax, incoming.rdx, bindings.leave_cell});
  try {
    Monstey_Result result;
    if (Monstey_Actual) {
      result = destroy ? Monstey::Monstey_GadbDestroyManagerGuardProjected(
          image.View(), guard, incoming, bindings, services) :
          Monstey::Monstey_GadbLeaveManagerCriticalSectionProjected(
          image.View(), lock, incoming, bindings, services);
    } else {
      result = destroy ? Monstey::Monstey_ReferenceDestroyManagerGuardProjected(
          image.View(), guard, incoming, bindings, services) :
          Monstey::Monstey_ReferenceLeaveManagerCriticalSectionProjected(
          image.View(), lock, incoming, bindings, services);
    }
    Monstey_Record("normal", {result.raw.rax, result.raw.rdx,
        result.post_leave_r8, result.post_leave_r9});
    return result;
  } catch (...) {
    Monstey_RecordException(); throw;
  }
}
void Monstey_Dto(const Monstey_Call& call, Monstey_U64 lock, Monstey_U64 cell, Monstey_U64 target, Monstey_Raw incoming = Monstey_In) {
  Monstey_Record(Monstey_ActiveImages.empty() ? "DTO-old-Raw2" : "DTO-Leave",
      {static_cast<Monstey_U64>(call.operation), call.native_site, call.slot_identity,
       call.target_identity, call.critical_section, call.incoming.rax, call.incoming.rdx});
  Monstey_Check(call.operation == Monstey::Monstey_GadbManagerSyncOperation::leave, "operation");
  Monstey_Check(call.native_site == 0x14114BA88ULL, "site");
  Monstey_Check(call.slot_identity == cell, "slot");
  Monstey_Check(call.target_identity == target, "target");
  Monstey_Check(call.critical_section == lock, "live lock");
  Monstey_Check(call.incoming.rax == incoming.rax, "incoming RAX changed");
  Monstey_Check(call.incoming.rdx == incoming.rdx, "incoming RDX changed");
}
template<class Function> void Monstey_MemoryFault(Function&& function, Monstey_U64 address) {
  bool observed = false;
  try { function(); }
  catch (const Monstey::Monstey_GadbBufferedMemoryFault& fault) {
    observed = true;
    Monstey_Check(fault.address == address, "first fault address");
    Monstey_Check(fault.width == 8, "first fault width");
    Monstey_Check(!fault.writing, "first fault writing");
  }
  Monstey_Check(observed, "memory fault absent");
}
struct Monstey_Token { Monstey_U64 value; };
struct Monstey_ProviderFailure final : std::runtime_error {
  std::shared_ptr<Monstey_Token> token;
  explicit Monstey_ProviderFailure(std::shared_ptr<Monstey_Token> identity)
      : std::runtime_error("declared provider partial failure"), token(std::move(identity)) {}
};
struct Monstey_Lifetime {};
void Monstey_RecordException() {
  try { throw; }
  catch (const Monstey::Monstey_GadbBufferedMemoryFault& fault) {
    Monstey_Record("memory-fault", {fault.address, static_cast<Monstey_U64>(fault.width),
        static_cast<Monstey_U64>(fault.writing)});
  } catch (const Monstey::Monstey_GadbManagerPrimitiveBoundaryFault& fault) {
    Monstey_Record("boundary-fault", {static_cast<Monstey_U64>(fault.kind), fault.native_site,
        fault.address, static_cast<Monstey_U64>(fault.width), fault.slot_identity,
        fault.target_identity, fault.argument});
  } catch (const Monstey_ProviderFailure& fault) {
    Monstey_Record(std::string("provider-typed:") + typeid(fault).name(), {fault.token->value});
  } catch (const std::exception& fault) {
    Monstey_Record(std::string("other:") + typeid(fault).name() + ":" + fault.what(), {});
  } catch (...) {
    Monstey_Record("other-nonstandard", {});
  }
}
void Monstey_Suite() {
  for (const bool destroy : {false, true}) {
    const std::string mode = destroy ? "destroy" : "leave";
    for (std::size_t v = 0; v < 8; ++v) {
      Monstey_Run("LP01/" + mode + "/raw64-" + std::to_string(v), [=] {
        Monstey_Image image;
        const Monstey_U64 lock = v == 0 ? 0 : Monstey_Lock ^ v;
        const Monstey_U64 target = v == 1 ? 0 : Monstey_Target ^ v;
        image.Put(8, lock); image.Put(40, target);
        Monstey_Bindings bindings; bindings.leave_cell = image.At(40);
        const auto before = image.data; const auto reply = Monstey_Reply(v);
        Monstey_Services services; std::size_t calls = 0;
        services.leave = [&](Monstey_Memory memory, const Monstey_Call& call) {
          ++calls; ++Monstey_Providers; Monstey_Dto(call, lock, bindings.leave_cell, target);
          Monstey_Check(memory.base == image.base && memory.bytes.data() == image.data.data() &&
              memory.bytes.size() == image.data.size(), "borrowed view changed");
          return reply;
        };
        Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), lock, Monstey_In,
            bindings, services), reply);
        Monstey_Check(calls == 1, "primitive count"); Monstey_ImageEqual(image.data, before);
      });
    }
    for (std::size_t v = 0; v < 3; ++v) {
      Monstey_Run("LP03/" + mode + "/cell-fault-" + std::to_string(v), [=] {
        Monstey_Image image; image.Put(8, Monstey_Lock);
        Monstey_Bindings bindings; const std::array<Monstey_U64, 3> bad{image.base - 1, image.At(121), 0};
        bindings.leave_cell = bad[v]; const auto before = image.data; Monstey_Services services;
        if (v != 0) services.leave = [&](Monstey_Memory, const Monstey_Call&) -> Monstey_Result {
          ++Monstey_Providers; throw Monstey_TestFailure("unreached primitive");
        };
        Monstey_MemoryFault([&] { (void)Monstey_Invoke(destroy, image, image.At(8),
            Monstey_Lock, Monstey_In, bindings, services); }, bindings.leave_cell);
        Monstey_ImageEqual(image.data, before);
      });
      Monstey_Run("LP03/" + mode + "/missing-service-" + std::to_string(v), [=] {
        Monstey_Image image;
        const Monstey_U64 lock = v == 0 ? 0 : Monstey_Lock;
        const Monstey_U64 target = v == 1 ? 0 : Monstey_Target;
        image.Put(8, lock); image.Put(40 + v, target);
        Monstey_Bindings bindings; bindings.leave_cell = image.At(40 + v);
        const auto before = image.data; Monstey_Services services; bool observed = false;
        try { (void)Monstey_Invoke(destroy, image, image.At(8), lock, Monstey_In, bindings, services); }
        catch (const Monstey::Monstey_GadbManagerPrimitiveBoundaryFault& fault) {
          observed = true;
          Monstey_Check(fault.kind == Monstey::Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
              "boundary kind");
          Monstey_Check(fault.native_site == 0x14114BA88ULL, "boundary site");
          Monstey_Check(fault.address == bindings.leave_cell, "boundary address");
          Monstey_Check(fault.width == 8, "boundary width");
          Monstey_Check(fault.slot_identity == bindings.leave_cell, "boundary slot");
          Monstey_Check(fault.target_identity == target, "boundary target");
          Monstey_Check(fault.argument == lock, "boundary lock");
        }
        Monstey_Check(observed, "missing-service absent"); Monstey_ImageEqual(image.data, before);
      });
    }
    for (const std::size_t offset : {40U, 41U, 39U, 47U}) {
      Monstey_Run("LP02/" + mode + "/alias-" + std::to_string(offset), [=] {
        Monstey_Image image; image.Put(offset, Monstey_Lock); image.Put(40, Monstey_Target);
        const auto lock = image.Get(offset), target = image.Get(40); const auto before = image.data;
        Monstey_Bindings bindings; bindings.leave_cell = image.At(40); Monstey_Services services;
        services.leave = [&](Monstey_Memory, const Monstey_Call& call) {
          ++Monstey_Providers; Monstey_Dto(call, lock, bindings.leave_cell, target); return Monstey_Reply();
        };
        Monstey_Equal(Monstey_Invoke(destroy, image, image.At(offset), lock, Monstey_In,
            bindings, services), Monstey_Reply()); Monstey_ImageEqual(image.data, before);
      });
    }
    for (std::size_t v = 0; v < 3; ++v) {
      Monstey_Run("LP02/" + mode + "/mapped-zero-" + std::to_string(v), [=] {
        Monstey_Image image(0); const std::size_t g = v == 1 ? 16 : 0, cell = v == 0 ? 16 : 0;
        image.Put(g, Monstey_Lock); image.Put(cell, Monstey_Target);
        const auto lock = image.Get(g); const auto before = image.data;
        Monstey_Bindings bindings; bindings.leave_cell = image.At(cell); Monstey_Services services;
        services.leave = [&](Monstey_Memory, const Monstey_Call& call) {
          ++Monstey_Providers; Monstey_Dto(call, lock, bindings.leave_cell, image.Get(cell)); return Monstey_Reply();
        };
        Monstey_Equal(Monstey_Invoke(destroy, image, image.At(g), lock, Monstey_In,
            bindings, services), Monstey_Reply()); Monstey_ImageEqual(image.data, before);
      });
    }
    Monstey_Run("LP02/" + mode + "/unaligned", [=] {
      Monstey_Image image; image.Put(1, Monstey_Lock); image.Put(35, Monstey_Target);
      Monstey_Bindings bindings; bindings.leave_cell = image.At(35);
      const auto before = image.data; Monstey_Services services;
      services.leave = [&](Monstey_Memory, const Monstey_Call& call) {
        ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target); return Monstey_Reply();
      };
      Monstey_Equal(Monstey_Invoke(destroy, image, image.At(1), Monstey_Lock, Monstey_In,
          bindings, services), Monstey_Reply()); Monstey_ImageEqual(image.data, before);
    });
    for (std::size_t v = 0; v < 3; ++v) {
      Monstey_Run("LP04/" + mode + "/partial-throw-" + std::to_string(v), [=] {
        Monstey_Image image; image.Put(8, Monstey_Lock); image.Put(40, Monstey_Target);
        Monstey_Bindings bindings; bindings.leave_cell = image.At(40); auto expected = image.data;
        auto token = std::make_shared<Monstey_Token>(Monstey_Token{0xFEDCBA9876543210ULL});
        Monstey_Services services;
        services.leave = [&](Monstey_Memory memory, const Monstey_Call& call) -> Monstey_Result {
          ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
          const std::array<std::size_t, 3> offsets{8, 40, 90};
          for (std::size_t i = 0; i <= v; ++i) {
            const auto value = static_cast<std::uint8_t>(0x30U + i);
            memory.bytes[offsets[i]] = value; expected[offsets[i]] = value;
          }
          throw Monstey_ProviderFailure(token);
        };
        bool observed = false;
        try { (void)Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services); }
        catch (const Monstey_ProviderFailure& fault) {
          observed = true;
          Monstey_Check(typeid(fault) == typeid(Monstey_ProviderFailure), "exception dynamic type");
          Monstey_Check(fault.token == token && fault.token->value == 0xFEDCBA9876543210ULL, "exception token");
        }
        Monstey_Check(observed, "provider exception absent"); Monstey_ImageEqual(image.data, expected);
      });
    }
    Monstey_Run("LP05/" + mode + "/mutable-current", [=] {
      Monstey_Image image; image.Put(8, Monstey_Lock); image.Put(40, Monstey_Target);
      Monstey_Bindings bindings; bindings.leave_cell = image.At(40); Monstey_Services services;
      const auto before = image.data;
      auto expected = before;
      for (std::size_t i = 40; i < 48; ++i) expected[i] = 0;
      services.leave = [&, count = std::size_t{0}](Monstey_Memory, const Monstey_Call& call) mutable {
        ++count; ++Monstey_Providers;
        Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, image.Get(40)); return Monstey_Reply(count);
      };
      Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services), Monstey_Reply(1));
      Monstey_ImageEqual(image.data, before);
      image.Put(40, 0);
      Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services), Monstey_Reply(2));
      Monstey_ImageEqual(image.data, expected);
    });
    Monstey_Run("LP05/" + mode + "/self-clear", [=] {
      Monstey_Image image; image.Put(8, Monstey_Lock); image.Put(40, Monstey_Target);
      Monstey_Bindings bindings; bindings.leave_cell = image.At(40); const auto before = image.data;
      auto life = std::make_shared<Monstey_Lifetime>(); std::weak_ptr<Monstey_Lifetime> weak = life;
      Monstey_Services services;
      services.leave = [&, life](Monstey_Memory, const Monstey_Call& call) {
        ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
        services.leave = {};
        Monstey_Check(!weak.expired() && life.use_count() >= 1, "active target retired"); return Monstey_Reply();
      };
      life.reset();
      Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services), Monstey_Reply());
      Monstey_Check(weak.expired(), "cleared target retained after return");
      Monstey_Check(!services.leave, "self-clear unpublished"); Monstey_ImageEqual(image.data, before);
    });
    Monstey_Run("LP05/" + mode + "/self-clear-throw", [=] {
      Monstey_Image image; image.Put(8, Monstey_Lock); image.Put(40, Monstey_Target);
      Monstey_Bindings bindings; bindings.leave_cell = image.At(40); auto expected = image.data;
      auto life = std::make_shared<Monstey_Lifetime>(); std::weak_ptr<Monstey_Lifetime> weak = life;
      auto token = std::make_shared<Monstey_Token>(Monstey_Token{0xFFEEDDCCBBAA9988ULL}); Monstey_Services services;
      services.leave = [&, life](Monstey_Memory memory, const Monstey_Call& call) -> Monstey_Result {
        ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
        services.leave = {};
        Monstey_Check(!weak.expired() && life.use_count() >= 1, "active throw target retired");
        memory.bytes[8] = 0x33; expected[8] = 0x33; throw Monstey_ProviderFailure(token);
      };
      life.reset(); bool observed = false;
      try { (void)Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services); }
      catch (const Monstey_ProviderFailure& fault) {
        observed = true;
        Monstey_Check(typeid(fault) == typeid(Monstey_ProviderFailure) && fault.token == token, "self-clear exception type/token");
      }
      Monstey_Check(observed, "self-clear exception absent");
      Monstey_Check(weak.expired() && !services.leave, "throw target retained after catch");
      Monstey_ImageEqual(image.data, expected);
    });
    Monstey_Run("LP05/" + mode + "/replace-reenter", [=] {
      Monstey_Image image; image.Put(8, Monstey_Lock); image.Put(40, Monstey_Target);
      Monstey_Bindings bindings; bindings.leave_cell = image.At(40);
      auto old_life = std::make_shared<Monstey_Lifetime>(), new_life = std::make_shared<Monstey_Lifetime>();
      std::weak_ptr<Monstey_Lifetime> old_weak = old_life, new_weak = new_life;
      Monstey_Services services; std::size_t old_calls = 0, new_calls = 0;
      constexpr Monstey_U64 nested_lock = 0x8123456789ABCDEFULL;
      auto expected_image = image; expected_image.Put(8, nested_lock); expected_image.Put(40, 0);
      const auto expected = expected_image.data;
      services.leave = [&, old_life, new_life](Monstey_Memory, const Monstey_Call& call) {
        ++old_calls; ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
        image.Put(8, nested_lock); image.Put(40, 0);
        services.leave = [&, new_life](Monstey_Memory, const Monstey_Call& nested) {
          ++new_calls; ++Monstey_Providers; Monstey_Dto(nested, nested_lock, bindings.leave_cell, 0);
          Monstey_Check(new_life.use_count() >= 1, "replacement unavailable"); return Monstey_Reply(7);
        };
        Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), nested_lock, Monstey_In, bindings, services), Monstey_Reply(7));
        Monstey_Check(!old_weak.expired() && old_life.use_count() >= 1, "active old target retired");
        return Monstey_Reply(2);
      };
      old_life.reset(); new_life.reset();
      Monstey_Equal(Monstey_Invoke(destroy, image, image.At(8), Monstey_Lock, Monstey_In, bindings, services), Monstey_Reply(2));
      Monstey_Check(old_calls == 1 && new_calls == 1, "reentry dispatch count");
      Monstey_Check(old_weak.expired() && !new_weak.expired(), "replacement lifetime");
      services.leave = {}; Monstey_Check(new_weak.expired(), "replacement retained after clear");
      Monstey_ImageEqual(image.data, expected);
    });
    for (std::size_t v = 0; v < 4; ++v) {
      Monstey_Run("LP06/" + mode + "/old-Raw2-" + std::to_string(v), [=] {
        Monstey_Image old_image, new_image;
        old_image.Put(8, Monstey_Lock); old_image.Put(40, Monstey_Target); new_image.data = old_image.data;
        Monstey_Bindings bindings; bindings.leave_cell = old_image.At(40); const auto declared = Monstey_Reply(v);
        Monstey::Monstey_GadbManagerPrimitiveServices old_services;
        old_services.sync = [&](Monstey_Memory memory, const Monstey_Call& call) {
          ++Monstey_Old; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
          memory.bytes[90] = static_cast<std::uint8_t>(v); return Monstey_Raw{declared.raw.rax, declared.raw.rdx};
        };
        Monstey_Services services;
        services.leave = [&](Monstey_Memory memory, const Monstey_Call& call) {
          ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target);
          memory.bytes[90] = static_cast<std::uint8_t>(v); return declared;
        };
        const auto old_raw = destroy ?
            Monstey::Monstey_GadbDestroyManagerGuard(old_image.View(), old_image.At(8), Monstey_In, bindings, old_services) :
            Monstey::Monstey_GadbLeaveManagerCriticalSection(old_image.View(), Monstey_Lock, Monstey_In, bindings, old_services);
        const auto result = Monstey_Invoke(destroy, new_image, new_image.At(8), Monstey_Lock, Monstey_In, bindings, services);
        Monstey_Check(old_raw.rax == result.raw.rax && old_raw.rdx == result.raw.rdx, "true old Raw2 projection differs");
        Monstey_Equal(result, declared); Monstey_ImageEqual(new_image.data, old_image.data);
      });
    }
  }
  for (std::size_t v = 0; v < 3; ++v) {
    Monstey_Run("LP03/destroy/guard-first-" + std::to_string(v), [=] {
      Monstey_Image image; const std::array<Monstey_U64, 3> bad{image.base - 1, image.At(121), 0};
      Monstey_Bindings bindings; bindings.leave_cell = image.At(120); const auto before = image.data; Monstey_Services services;
      Monstey_MemoryFault([&] { (void)Monstey_Invoke(true, image, bad[v], Monstey_Lock, Monstey_In, bindings, services); }, bad[v]);
      Monstey_ImageEqual(image.data, before);
    });
  }
  Monstey_Run("LP03/leave/no-guard-read", [] {
    Monstey_Image image; image.Put(40, Monstey_Target); Monstey_Bindings bindings; bindings.leave_cell = image.At(40);
    const auto before = image.data; Monstey_Services services;
    services.leave = [&](Monstey_Memory, const Monstey_Call& call) {
      ++Monstey_Providers; Monstey_Dto(call, Monstey_Lock, bindings.leave_cell, Monstey_Target); return Monstey_Reply();
    };
    Monstey_Equal(Monstey_Invoke(false, image, 0, Monstey_Lock, Monstey_In, bindings, services), Monstey_Reply());
    Monstey_ImageEqual(image.data, before);
  });
  for (const bool destroy : {false, true}) {
    Monstey_Run(destroy ? "LP03/destroy/wrapedge" : "LP03/leave/wrapedge", [=] {
      Monstey_Image image(std::numeric_limits<Monstey_U64>::max() - 3, 8);
      Monstey_Bindings bindings; bindings.leave_cell = image.base; const auto before = image.data; Monstey_Services services;
      Monstey_MemoryFault([&] { (void)Monstey_Invoke(destroy, image, image.base, Monstey_Lock, Monstey_In, bindings, services); }, image.base);
      Monstey_ImageEqual(image.data, before);
    });
  }
}
std::string Monstey_Hex(Monstey_U64 value) {
  std::ostringstream out; out << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(16) << value;
  return out.str();
}
}
std::string Monstey_JsonEscape(const std::string& value) {
  std::string result;
  for (const char c : value) {
    if (c == '\\' || c == '"') { result.push_back('\\'); result.push_back(c); }
    else if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else result.push_back(c);
  }
  return result;
}
void Monstey_WriteEvents(std::ostream& out, const std::vector<Monstey_Observation>& events) {
  out << '[';
  for (std::size_t i = 0; i < events.size(); ++i) {
    const auto& event = events[i];
    if (i) out << ',';
    out << "{\"kind\":\"" << Monstey_JsonEscape(event.kind) << "\",\"words\":[";
    for (std::size_t j = 0; j < event.words.size(); ++j) {
      if (j) out << ',';
      out << '"' << Monstey_Hex(event.words[j]) << '"';
    }
    out << "],\"image_hex\":\"";
    constexpr char digits[] = "0123456789ABCDEF";
    for (const auto byte : event.image) out << digits[byte >> 4] << digits[byte & 15];
    out << "\"}";
  }
  out << ']';
}
void Monstey_WriteTotals(std::ostream& out, const Monstey_LegTotals& value) {
  out << "{\"checks\":" << value.checks << ",\"provider_calls\":" << value.providers
      << ",\"old_Raw2_calls\":" << value.old_calls << ",\"gold_image_bytes\":" << value.image_bytes
      << ",\"events\":" << value.events << '}';
}
int main(int argc, char** argv) {
  if (argc != 2) { std::cerr << "report path required\n"; return 2; }
  Monstey_Suite(); std::size_t failures = 0;
  for (const auto& item : Monstey_Cases) if (!item.pass) ++failures;
  const bool complete = Monstey_Cases.size() == 72;
  const bool pass = failures == 0 && complete;
  std::ofstream out(argv[1], std::ios::binary);
  if (!out) { std::cerr << "report open failed\n"; return 2; }
  const auto sample = Monstey_Reply();
  out << "{\n\"schema\":\"Monstey.LeaveProjectionActualDifferential/6\",\n\"status\":\""
      << (pass ? "PASS" : "FAIL") << "\",\n\"paired_cases\":" << Monstey_Cases.size()
      << ",\"complete_72pairs\":" << (complete ? "true" : "false") << ",\"failed_cases\":" << failures
      << ",\"checks\":" << Monstey_Checks << ",\"differential_checks\":" << Monstey_DiffChecks
      << ",\"differential_image_bytes\":" << Monstey_DiffImageBytes << ",\n\"reference\":";
  Monstey_WriteTotals(out, Monstey_ReferenceTotals); out << ",\n\"actual\":";
  Monstey_WriteTotals(out, Monstey_ActualTotals);
  out << ",\n\"full64_sample\":{\"rax\":\"" << Monstey_Hex(sample.raw.rax)
      << "\",\"rdx\":\"" << Monstey_Hex(sample.raw.rdx) << "\",\"r8\":\"" << Monstey_Hex(sample.post_leave_r8)
      << "\",\"r9\":\"" << Monstey_Hex(sample.post_leave_r9)
      << "\"},\n\"parent_or_physical_qualified\":false,\n\"results\":[\n";
  for (std::size_t i = 0; i < Monstey_Cases.size(); ++i) {
    const auto& item = Monstey_Cases[i];
    out << "{\"id\":\"" << Monstey_JsonEscape(item.name) << "\",\"pass\":" << (item.pass ? "true" : "false")
        << ",\"checks\":" << item.checks << ",\"detail\":\"" << Monstey_JsonEscape(item.detail) << "\"}"
        << (i + 1 == Monstey_Cases.size() ? "\n" : ",\n");
  }
  out << "],\n\"first_failure_reference_events\":";
  Monstey_WriteEvents(out, Monstey_FailureReference);
  out << ",\n\"first_failure_actual_events\":";
  Monstey_WriteEvents(out, Monstey_FailureActual);
  out << "\n}\n"; out.close();
  std::cout << "pairs=" << Monstey_Cases.size() << " failed=" << failures
      << " checks=" << Monstey_Checks << " diff=" << Monstey_DiffChecks << '\n';
  return pass ? 0 : 1;
}
