#include "Monstey_GadbBankSynchronization.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bit>
#if !defined(_MSC_VER) || !defined(_M_X64)
#error This native compatibility profile requires qualified MSVC x64 code generation.
#endif
namespace Monstey {
static_assert(std::atomic_ref<std::int32_t>::is_always_lock_free);
// Kept as distinct owned compiler boundaries so the exact linked leaf MOV
// instructions can be verified. Acquire/release supplies defined C++ publication
// over real S32 objects; x64 hardware must remain plain MOV, no LOCK/fence.
__declspec(noinline) std::int32_t Monstey_GadbBankEpochAcquire(
    std::int32_t &word) noexcept {
    return std::atomic_ref<std::int32_t>(word).load(std::memory_order_acquire);
}
__declspec(noinline) void Monstey_GadbBankEpochRelease(
    std::int32_t &word, std::int32_t value) noexcept {
    std::atomic_ref<std::int32_t>(word).store(value, std::memory_order_release);
}
Monstey_GadbBankFault::Monstey_GadbBankFault(
    Monstey_GadbBankFaultKind k, std::uint64_t where, const char *message,
    std::uint32_t error)
    : std::runtime_error(message), kind(k), identity(where), system_error(error) {}
Monstey_GadbBankEpochWord::Monstey_GadbBankEpochWord(
    std::uint64_t where, std::int32_t initial) : bits_(initial), identity(where) {}
std::int32_t Monstey_GadbBankEpochWord::Load() const noexcept {
    return Monstey_GadbBankEpochAcquire(bits_);
}
void Monstey_GadbBankEpochWord::Store(std::int32_t value) noexcept {
    Monstey_GadbBankEpochRelease(bits_, value);
}
struct Monstey_GadbBankSynchronizer::Monstey_Impl {
    CRITICAL_SECTION critical;
    CONDITION_VARIABLE condition;
    HANDLE event = nullptr;
    bool event_branch;
    std::atomic<std::uint64_t> headers{0}, waits{0}, wakes{0};
    explicit Monstey_Impl(Monstey_GadbBankBackendObservation observed)
        : event_branch(observed.native_event_flag != 0) {
        const bool all_condition = observed.initialize_condition_available &&
            observed.sleep_condition_available && observed.wake_all_condition_available;
        if (!observed.kernel_module_available || event_branch == all_condition)
            throw Monstey_GadbBankFault(
                Monstey_GadbBankFaultKind::InvalidBackendObservation,
                0x1408c403c, "Supplied backend observation is not a completed native branch");
        if (!InitializeCriticalSectionEx(&critical, 4000, 0))
            throw Monstey_GadbBankFault(
                Monstey_GadbBankFaultKind::Win32InitializationFailure,
                0x1426f8d00, "Owned critical-section initialization failed", GetLastError());
        if (event_branch) {
            event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!event) {
                const auto error = GetLastError();
                DeleteCriticalSection(&critical);
                throw Monstey_GadbBankFault(
                    Monstey_GadbBankFaultKind::Win32InitializationFailure,
                    0x1426f8d30, "Owned manual-reset event initialization failed", error);
            }
        } else {
            InitializeConditionVariable(&condition);
        }
    }
    ~Monstey_Impl() {
        //8C415C deletes its CS before closing its optional event. All users must
        // have joined; concurrent destruction is outside the native contract.
        DeleteCriticalSection(&critical);
        if (event) CloseHandle(event);
    }
    void Wait100() noexcept {
        waits.fetch_add(1, std::memory_order_relaxed);
        if (event_branch) {
            LeaveCriticalSection(&critical);
            (void)WaitForSingleObjectEx(event, 100, FALSE);
            EnterCriticalSection(&critical);
        } else {
            (void)SleepConditionVariableCS(&condition, &critical, 100);
        }
        // The native header ignores the wait result and rechecks its guard.
    }
    void Wake() noexcept {
        wakes.fetch_add(1, std::memory_order_relaxed);
        if (event_branch) {
            (void)SetEvent(event);
            (void)ResetEvent(event);
        } else {
            WakeAllConditionVariable(&condition);
        }
    }
};
Monstey_GadbBankSynchronizer::Monstey_GadbBankSynchronizer(
    Monstey_GadbBankBackendObservation observed)
    : impl_(std::make_unique<Monstey_Impl>(observed)) {}
Monstey_GadbBankSynchronizer::~Monstey_GadbBankSynchronizer() = default;
void Monstey_GadbBankSynchronizer::Header(
    Monstey_GadbBankEpochWord &guard, Monstey_GadbBankEpochWord &globalEpoch,
    std::int32_t &threadEpoch) {
    if (&guard == &globalEpoch || guard.identity == globalEpoch.identity)
        throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
            guard.identity, "Guard and global epoch require distinct live identities");
    impl_->headers.fetch_add(1, std::memory_order_relaxed);
    EnterCriticalSection(&impl_->critical);
    for (;;) {
        if (guard.Load() == 0) {
            guard.Store(-1);
            break;
        }
        if (guard.Load() != -1) {
            threadEpoch = globalEpoch.Load();
            break;
        }
        impl_->Wait100();
    }
    LeaveCriticalSection(&impl_->critical);
}
void Monstey_GadbBankSynchronizer::Footer(
    Monstey_GadbBankEpochWord &guard, Monstey_GadbBankEpochWord &globalEpoch,
    std::int32_t &threadEpoch) {
    if (&guard == &globalEpoch || guard.identity == globalEpoch.identity)
        throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
            guard.identity, "Guard and global epoch require distinct live identities");
    EnterCriticalSection(&impl_->critical);
    const auto incremented = std::bit_cast<std::int32_t>(
        std::bit_cast<std::uint32_t>(globalEpoch.Load()) + 1u);
    globalEpoch.Store(incremented);
    guard.Store(incremented);
    threadEpoch = globalEpoch.Load();
    LeaveCriticalSection(&impl_->critical);
    impl_->Wake();
}
Monstey_GadbBankSynchronizationStats Monstey_GadbBankSynchronizer::Stats() const noexcept {
    return {impl_->headers.load(std::memory_order_relaxed),
            impl_->waits.load(std::memory_order_relaxed),
            impl_->wakes.load(std::memory_order_relaxed)};
}
} // namespace Monstey
