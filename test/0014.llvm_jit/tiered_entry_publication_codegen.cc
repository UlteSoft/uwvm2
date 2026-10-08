#include <uwvm2/runtime/lib/uwvm_runtime_tiered_publication.h>
#include <thread>

// Expose the real cold guard's optimized instruction sequence. This is a
// publication probe, not a generated Wasm call or memory-access benchmark.
extern "C" void uwvm_tiered_cold_publish(std::uint_least8_t& lock, std::uint_least8_t& ready,
                                        std::uintptr_t& slot, std::uintptr_t lazy, std::uintptr_t full) noexcept
{
    uwvm2::runtime::lib::details::tiered_entry_publication_scope transaction{
        lock, []() noexcept { std::this_thread::yield(); }};
    auto const value{std::atomic_ref<std::uint_least8_t>{ready}.load(std::memory_order_acquire) ? full : lazy};
    std::atomic_ref<std::uintptr_t>{slot}.store(value, std::memory_order_release);
}
