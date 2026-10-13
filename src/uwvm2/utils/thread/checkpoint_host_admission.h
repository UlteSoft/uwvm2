/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <utility>
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <mutex>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
// Global-module attachment for the future actual runtime implementation. No
// runtime producer is implemented by this component or its module import.
extern "C++" { namespace uwvm2::runtime::lib { class runtime_checkpoint_host_bridge; } }
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
    enum class checkpoint_host_admission_status : unsigned char
    { ok, busy, admission_closed, retired, untracked_host, quota, serial_exhausted, invalid_context };
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // A private, independent gate for KNOWN synchronous native host operations.
    // This does not own an execution-domain lease, pause ticket, code/source pin,
    // guest FD census or registry transaction. A closed gate is NEVER a world
    // stop or checkpoint publish/restore credential. No current runtime calls it.
    class checkpoint_host_admission final
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_host_bridge;
        static constexpr ::std::size_t operation_limit{65536u};
        struct state final
        {
            ::std::mutex mutex{};
            ::std::size_t active{};
            ::std::uint64_t next_serial{1u}, closed_serial{};
            bool retired{}, untracked{};
        };
        // Private control-block identity survives an operation/guard after owner
        // retirement, avoiding a dangling gate borrow. It does NOT pin VM code,
        // memory or any runtime resource: the producer needs its real lease too.
        ::std::shared_ptr<state> state_{::std::make_shared<state>()};
        checkpoint_host_admission() = default;
    public:
        struct observation
        {
            ::std::size_t active_operations{};
            bool admission_closed{}, retired{}, untracked_host{};
        }; // Diagnostic data only; no method accepts this as permission.
        class host_operation final
        {
            friend class checkpoint_host_admission;
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_host_bridge;
            ::std::shared_ptr<state> owner_{};
            void release() noexcept
            {
                if(!owner_) { return; }
                // [actual pinned gate state] complete until this local pin dies.
                // [safe                    ] move ownership before decrement;
                // no domain/native pointer or borrowed resource escapes.
                auto owner{::std::move(owner_)};
                ::std::lock_guard lock{owner->mutex};
                if(owner->active == 0u) { ::fast_io::fast_terminate(); }
                --owner->active;
            }
        public:
            host_operation() noexcept = default; // Empty cannot admit work.
            host_operation(host_operation const&) = delete;
            host_operation& operator=(host_operation const&) = delete;
            host_operation(host_operation&& other) noexcept : owner_{::std::move(other.owner_)} {}
            host_operation& operator=(host_operation&& other) noexcept
            { if(this != __builtin_addressof(other)) { release(); owner_ = ::std::move(other.owner_); } return *this; }
            ~host_operation() { release(); }
            explicit operator bool() const noexcept { return owner_ != nullptr; }
        };
        class closed_admission final
        {
            friend class checkpoint_host_admission;
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_host_bridge;
            ::std::shared_ptr<state> owner_{};
            ::std::uint64_t serial_{};
            void release() noexcept
            {
                if(!owner_) { return; }
                // [actual pinned gate state] guard-only pin, never runtime data.
                // [safe                    ] take ownership before resetting;
                // serial comparison prevents a stale guard reopening a later one.
                auto owner{::std::move(owner_)};
                auto const serial{::std::exchange(serial_, 0u)};
                ::std::lock_guard lock{owner->mutex};
                if(serial != 0u && owner->closed_serial == serial) { owner->closed_serial = 0u; }
                // Retired always rejects entries. Sticky untracked invalidates
                // checkpoint eligibility, not ordinary host execution semantics.
            }
        public:
            closed_admission() noexcept = default; // Empty grants nothing.
            closed_admission(closed_admission const&) = delete;
            closed_admission& operator=(closed_admission const&) = delete;
            closed_admission(closed_admission&& other) noexcept
                : owner_{::std::move(other.owner_)}, serial_{::std::exchange(other.serial_, 0u)} {}
            closed_admission& operator=(closed_admission&& other) noexcept
            {
                if(this != __builtin_addressof(other))
                { release(); owner_ = ::std::move(other.owner_); serial_ = ::std::exchange(other.serial_, 0u); }
                return *this;
            }
            ~closed_admission() { release(); }
            explicit operator bool() const noexcept
            {
                if(!owner_ || serial_ == 0u) { return false; }
                ::std::lock_guard lock{owner_->mutex};
                return !owner_->retired && !owner_->untracked && owner_->closed_serial == serial_;
            }
        };
        checkpoint_host_admission(checkpoint_host_admission const&) = delete;
        checkpoint_host_admission& operator=(checkpoint_host_admission const&) = delete;
        checkpoint_host_admission(checkpoint_host_admission&&) = delete;
        ~checkpoint_host_admission()
        {
            ::std::lock_guard lock{state_->mutex}; state_->retired = true;
            // No wait/cancel here. Existing state pins remain valid, but retiring
            // this gate supplies NO permission to free their separate VM owners.
        }
        [[nodiscard]] observation observe() const noexcept
        {
            ::std::lock_guard lock{state_->mutex};
            return {state_->active, state_->closed_serial != 0u, state_->retired, state_->untracked};
        }
    private:
        [[nodiscard]] checkpoint_host_admission_status try_enter(host_operation& out) noexcept
        {
            if(out.owner_) { return checkpoint_host_admission_status::invalid_context; }
            ::std::lock_guard lock{state_->mutex};
            if(state_->retired) { return checkpoint_host_admission_status::retired; }
            if(state_->closed_serial != 0u) { return checkpoint_host_admission_status::admission_closed; }
            if(state_->active == operation_limit) { return checkpoint_host_admission_status::quota; }
            ++state_->active; out.owner_ = state_;
            return checkpoint_host_admission_status::ok;
        }
        // Called only inside ONE actual stopped-domain callback. Nonwaiting:
        // no execution-domain drain, cancellation, condition wait, guest resume
        // or code publication occurs here. A suspended host continuation still
        // owns its operation and returns busy instead of deadlocking a manager.
        [[nodiscard]] checkpoint_host_admission_status try_close(closed_admission& out) noexcept
        {
            if(out.owner_) { return checkpoint_host_admission_status::invalid_context; }
            ::std::lock_guard lock{state_->mutex};
            if(state_->retired) { return checkpoint_host_admission_status::retired; }
            if(state_->untracked) { return checkpoint_host_admission_status::untracked_host; }
            if(state_->closed_serial != 0u) { return checkpoint_host_admission_status::admission_closed; }
            if(state_->active != 0u) { return checkpoint_host_admission_status::busy; }
            if(state_->next_serial == 0u) { return checkpoint_host_admission_status::serial_exhausted; }
            auto const serial{state_->next_serial};
            state_->next_serial = serial == (::std::numeric_limits<::std::uint64_t>::max)() ? 0u : serial + 1u;
            state_->closed_serial = serial; out.owner_ = state_; out.serial_ = serial;
            return checkpoint_host_admission_status::ok;
        }
        [[nodiscard]] bool current(closed_admission const& actual) const noexcept
        {
            // Compare actual owned state/control block BEFORE examining supplied
            // guard contents. No caller FD, numeric serial or boolean can mint it.
            if(state_.get() != actual.owner_.get() || state_.owner_before(actual.owner_) ||
               actual.owner_.owner_before(state_) || actual.serial_ == 0u) { return false; }
            ::std::lock_guard lock{state_->mutex};
            return !state_->retired && !state_->untracked && state_->active == 0u &&
                state_->closed_serial == actual.serial_;
        }
        void record_untracked_host() noexcept
        {
            ::std::lock_guard lock{state_->mutex}; state_->untracked = true;
            // No clearing API. A raw preload escape / unknown native mutator
            // keeps this instance ineligible until real drained-owner rebuild.
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
