#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
namespace Monstey {
enum class Monstey_GadbBankFaultKind {
    InvalidBackendObservation, Win32InitializationFailure,
    UnknownEnumerator, MissingGuardWord, InvalidRecipe
};
class Monstey_GadbBankFault final : public std::runtime_error {
public:
    Monstey_GadbBankFaultKind kind;
    std::uint64_t identity;
    std::uint32_t system_error;
    Monstey_GadbBankFault(Monstey_GadbBankFaultKind, std::uint64_t,
                          const char *, std::uint32_t system_error = 0);
};
// A real live aligned S32 object, not an atomic_ref over vector<byte> storage.
// All concurrent guard/global accesses must use these atomic operations; no
// ordinary/byte access to its backing object is permitted. Native identity is
// numeric metadata only. Initial bits are explicitly supplied, never reset.
class Monstey_GadbBankEpochWord final {
    alignas(std::atomic_ref<std::int32_t>::required_alignment)
        mutable std::int32_t bits_;
public:
    const std::uint64_t identity;
    Monstey_GadbBankEpochWord(std::uint64_t identity, std::int32_t initial);
    Monstey_GadbBankEpochWord(const Monstey_GadbBankEpochWord &) = delete;
    Monstey_GadbBankEpochWord &operator=(const Monstey_GadbBankEpochWord &) = delete;
    std::int32_t Load() const noexcept;
    void Store(std::int32_t) noexcept;
};
// Supplied observation of8C403C's completed backend selection. The original
// module/procedure resolution and encoded pointers are not invoked. A native
// nonzero event flag requires at least one unavailable CV API; a zero event
// flag requires all three present. No backend preference/default is inferred.
struct Monstey_GadbBankBackendObservation {
    bool kernel_module_available;
    bool initialize_condition_available;
    bool sleep_condition_available;
    bool wake_all_condition_available;
    std::uint64_t native_event_flag;
};
struct Monstey_GadbBankSynchronizationStats {
    std::uint64_t header_calls, wait_calls, wake_calls;
};
// Own Win32 resources only: CS(spin4000) and observed CV/manual-event branch.
// Lifetime must outlast/join every user. Guard/global words have disjoint live
// backing; threadEpoch is exclusive to the calling thread and supplied by it.
// These methods translate8C4214/8C41B4 with exact100ms retry and wake-after-unlock.
// Arbitrary sequential epochs preserve native signed behavior. Concurrent users
// must supply protocol-consistent epochs/publication; no history is synthesized.
class Monstey_GadbBankSynchronizer final {
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
public:
    explicit Monstey_GadbBankSynchronizer(Monstey_GadbBankBackendObservation);
    ~Monstey_GadbBankSynchronizer();
    Monstey_GadbBankSynchronizer(const Monstey_GadbBankSynchronizer &) = delete;
    Monstey_GadbBankSynchronizer &operator=(const Monstey_GadbBankSynchronizer &) = delete;
    void Header(Monstey_GadbBankEpochWord &guard,
                Monstey_GadbBankEpochWord &globalEpoch,
                std::int32_t &threadEpoch);
    void Footer(Monstey_GadbBankEpochWord &guard,
                Monstey_GadbBankEpochWord &globalEpoch,
                std::int32_t &threadEpoch);
    Monstey_GadbBankSynchronizationStats Stats() const noexcept;
};
} // namespace Monstey
