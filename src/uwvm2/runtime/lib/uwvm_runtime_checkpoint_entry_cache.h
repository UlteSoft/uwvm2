// Calling-thread metadata memoization, never an activation or execution right.
// The runtime supplies keys from its actual publication under an execution
// lease, and inserts only after full canonical source/generation validation.
// Lifetime is ONE admitted execution scope; cold capture never uses this cache.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <uwvm2/runtime/checkpoint/materialization.h>
#endif
#include <uwvm2/utils/macro/push_macros.h>
namespace uwvm2::runtime::lib::details
{
    class checkpoint_entry_plan_cache
    {
    public:
        // Fixed metadata-only storage: 20 KiB on the supported 64-bit host.
        // A 256-target round-robin must not evict four plans per cache row.
        inline static constexpr ::std::size_t entry_count{256u};
        static_assert((entry_count & (entry_count - 1u)) == 0u);
        struct key
        {
            void const* record{};
            void const* publication{};
            ::std::uint64_t epoch{}, module{}, function{}, generation{};
            ::std::uintptr_t raw_entry{}, typed_entry{};
            UWVM_ALWAYS_INLINE friend bool operator==(key const&, key const&) = default;
        };
    private:
        struct row { key binding{}; ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{}; };
        ::std::array<row,entry_count> rows_{};
        [[nodiscard]] static constexpr ::std::size_t index(key const& actual) noexcept
        {
            // Keep the original function-only projection: 256 rows refine
            // the old 64-row collision partition without an extra warm XOR.
            // Module IDs remain in the complete binding comparison. A module
            // collision misses normally and can never reuse another plan.
            return static_cast<::std::size_t>(actual.function) & (entry_count - 1u);
        }
    public:
        [[nodiscard]] UWVM_ALWAYS_INLINE ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const*
        find(key const& actual, ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const& live_plan) const noexcept
        {
            auto const& selected{rows_[index(actual)]};
            if(!live_plan || selected.binding != actual || selected.plan.get() != live_plan.get() ||
               selected.plan.owner_before(live_plan) || live_plan.owner_before(selected.plan)) { return nullptr; }
            return ::std::addressof(selected.plan);
        }
        void remember_validated(key actual, ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const& plan) noexcept
        {
            if(!plan || actual.record == nullptr || actual.publication == nullptr || actual.epoch == 0u ||
               actual.generation == 0u || actual.raw_entry == 0u || actual.typed_entry == 0u) { return; }
            rows_[index(actual)] = {actual,plan};
        }
    };
}
#include <uwvm2/utils/macro/pop_macros.h>
