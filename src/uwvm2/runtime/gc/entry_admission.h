/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE) && defined(UWVM_MODULE)
# error "Experimental managed numeric page is NONMODULE-only until separately qualified"
#endif
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <exception>
# include <limits>
# include <span>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    // This is a cold execution/administrative boundary, never a guest memory
    // guard. EVERY outer full/lazy/raw/debug-prepare entry owns one shared lease.
    // Nested guest entry reuses that lease. Initializer/root-graph writers own
    // another shared lease. A collector may acquire exclusive only from one:
    // its own live outer execution, with no omitted peer or graph writer.
    // Peers wait only for that short transaction and then continue normally.
    class managed_entry_admission
    {
        static constexpr auto exclusive_bit{::std::size_t{1u} <<
            (::std::numeric_limits<::std::size_t>::digits - 1u)};
        static constexpr auto count_mask{exclusive_bit - 1uz};
        ::std::atomic_size_t state_{};
        void leave_shared() noexcept
        {
            auto const previous{state_.fetch_sub(1uz, ::std::memory_order_acq_rel)};
            if((previous & exclusive_bit) != 0uz || (previous & count_mask) == 0uz)
            { ::std::terminate(); }
            state_.notify_all();
        }
        void leave_exclusive(::std::size_t expected_count) noexcept
        {
            auto expected{exclusive_bit | expected_count};
            if(!state_.compare_exchange_strong(expected, expected_count,
                ::std::memory_order_release, ::std::memory_order_relaxed))
            { ::std::terminate(); }
            state_.notify_all();
        }
    public:
        class shared_lease
        {
            managed_entry_admission* owner_{};
            friend class managed_entry_admission;
            explicit shared_lease(managed_entry_admission& owner) noexcept : owner_{&owner} {}
        public:
            shared_lease() noexcept = default;
            shared_lease(shared_lease const&) = delete;
            shared_lease& operator=(shared_lease const&) = delete;
            shared_lease(shared_lease&& other) noexcept : owner_{::std::exchange(other.owner_, nullptr)} {}
            shared_lease& operator=(shared_lease&& other) noexcept
            { if(this != &other) { reset(); owner_ = ::std::exchange(other.owner_, nullptr); } return *this; }
            ~shared_lease() { reset(); }
            void reset() noexcept { if(auto* owner{::std::exchange(owner_, nullptr)}) { owner->leave_shared(); } }
            explicit operator bool() const noexcept { return owner_ != nullptr; }
        };
        class exclusive_lease
        {
            managed_entry_admission* owner_{};
            ::std::size_t count_{};
            friend class managed_entry_admission;
            exclusive_lease(managed_entry_admission& owner, ::std::size_t count) noexcept : owner_{&owner}, count_{count} {}
        public:
            exclusive_lease() noexcept = default;
            exclusive_lease(exclusive_lease const&) = delete;
            exclusive_lease& operator=(exclusive_lease const&) = delete;
            exclusive_lease(exclusive_lease&& other) noexcept
                : owner_{::std::exchange(other.owner_, nullptr)}, count_{other.count_} {}
            exclusive_lease& operator=(exclusive_lease&&) = delete;
            ~exclusive_lease() { reset(); }
            void reset() noexcept
            { if(auto* owner{::std::exchange(owner_, nullptr)}) { owner->leave_exclusive(count_); } }
            explicit operator bool() const noexcept { return owner_ != nullptr; }
        };
        [[nodiscard]] shared_lease enter() noexcept
        {
            auto state{state_.load(::std::memory_order_acquire)};
            for(;;)
            {
                if((state & exclusive_bit) != 0uz)
                {
                    state_.wait(state, ::std::memory_order_acquire);
                    state = state_.load(::std::memory_order_acquire);
                    continue;
                }
                if(state == count_mask) { ::std::terminate(); }
                if(state_.compare_exchange_weak(state, state + 1uz,
                    ::std::memory_order_acq_rel, ::std::memory_order_acquire))
                { return shared_lease{*this}; }
            }
        }

        // Cold, nonwaiting native reader admission. A debugger that cannot
        // enter during a sweep returns unavailable; it never waits while holding
        // a pause/publication lock. This adds no guest memory-access operation.
        [[nodiscard]] shared_lease try_enter() noexcept
        {
            auto state{state_.load(::std::memory_order_acquire)};
            for(;;)
            {
                if((state & exclusive_bit) != 0uz || state == count_mask) { return {}; }
                if(state_.compare_exchange_weak(state, state + 1uz,
                    ::std::memory_order_acq_rel, ::std::memory_order_acquire))
                { return shared_lease{*this}; }
            }
        }
        // Trusted-native utility, NOT a world-stop/root/restore credential.
        // The caller must retain these actual owned lease objects unchanged
        // through exclusion. Runtime uses only privately minted before-park
        // borrows authenticated in ONE current complete cooperative cohort.
        // A matching numerical count, caller bool or copied epoch is insufficient.
        [[nodiscard]] exclusive_lease try_exclusive_owned(
            ::std::span<shared_lease const* const> actual_leases) noexcept
        {
            if(actual_leases.empty() || actual_leases.size() > count_mask ||
               actual_leases.size() > static_cast<::std::size_t>(
                   (::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(shared_lease const*))
            { return {}; }
            for(::std::size_t i{}; i != actual_leases.size(); ++i)
            {
                // [trusted complete stable native lease-pointer array] end
                // [safe] i<N before indexing; runtime canonicalizes the capture
                // and current episode BEFORE borrowing/dereferencing this object.
                auto const* actual{actual_leases[i]};
                if(actual == nullptr || actual->owner_ != this) { return {}; }
                for(::std::size_t j{}; j != i; ++j)
                { if(actual_leases[j] == actual) { return {}; } }
            }
            // No wait: any omitted execution, host reader, initializer or root
            // writer contributes another real lease and makes this CAS fail.
            auto expected{actual_leases.size()};
            if(!state_.compare_exchange_strong(expected, exclusive_bit | actual_leases.size(),
                ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return {}; }
            return exclusive_lease{*this, actual_leases.size()};
        }

        [[nodiscard]] exclusive_lease try_exclusive(::std::size_t required_count) noexcept
        {
            if(required_count > 1uz) { return {}; }
            auto expected{required_count};
            if(!state_.compare_exchange_strong(expected, exclusive_bit | required_count,
                ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return {}; }
            return exclusive_lease{*this, required_count};
        }
        // Genuine leases issued by THIS admission word. Not active_count==1.
        [[nodiscard]] bool protects_shared(exclusive_lease const& exclusive, shared_lease const& shared) const noexcept
        {
            return exclusive.owner_ == this && exclusive.count_ == 1uz && shared.owner_ == this &&
                state_.load(::std::memory_order_acquire) == (exclusive_bit | 1uz);
        }
        [[nodiscard]] ::std::size_t active_count() const noexcept
        { return state_.load(::std::memory_order_acquire) & count_mask; }
    };
    inline managed_entry_admission runtime_gc_entry_admission{};
    inline thread_local ::std::size_t cli_gc_execution_depth{};
    // Native launch token constructed at the actual CLI graph dispatcher. It
    // carries no guest-supplied bool/address/handle and confers no authority on
    // externally constructed module storage. Privileged native code is still
    // required to respect the documented loader/native-reader contract.
    class scoped_cli_gc_execution
    {
    public:
        scoped_cli_gc_execution() noexcept
        {
            if(cli_gc_execution_depth == (::std::numeric_limits<::std::size_t>::max)()) { ::std::terminate(); }
            ++cli_gc_execution_depth;
        }
        scoped_cli_gc_execution(scoped_cli_gc_execution const&) = delete;
        scoped_cli_gc_execution& operator=(scoped_cli_gc_execution const&) = delete;
        ~scoped_cli_gc_execution() { if(cli_gc_execution_depth == 0uz) { ::std::terminate(); } --cli_gc_execution_depth; }
    };
    [[nodiscard]] inline bool is_cli_gc_execution() noexcept { return cli_gc_execution_depth != 0uz; }

    // Administrative storage/segment mutation is a counted native reader and
    // writer. It waits for an in-progress sweep, then preserves existing guest
    // threading behavior: active>1 only prevents GC, not ordinary execution.
    // The caller STILL obeys the loader's external storage/reset synchronization
    // contract. This guard is not permission to destroy a live compiled module.
    class scoped_gc_root_graph_administration
    {
        managed_entry_admission::shared_lease lease_;
    public:
        scoped_gc_root_graph_administration() noexcept : lease_{runtime_gc_entry_admission.enter()} {}
        scoped_gc_root_graph_administration(scoped_gc_root_graph_administration const&) = delete;
        scoped_gc_root_graph_administration& operator=(scoped_gc_root_graph_administration const&) = delete;
    };
}
